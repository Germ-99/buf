/*
    buf - Command line tool for flashing ISO images onto USB drives.
    Copyright (C) 2026  Bryson Kelly

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
    
*/


// https://stackoverflow.com/questions/10368305/how-to-test-posix-compatibility
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L 
#endif

#include "../include/buf.h"
#include <sys/sendfile.h>

#define BLOCK_SIZE (32 * 1024 * 1024) // 32MB block size

static unsigned long long total_copied = 0; // Bytes copied so far
static unsigned long long total_size = 0;   // Total size to copy
static time_t last_update = 0;              // Last time progress was displayed
static char current_file[MAX_PATH] = "";    // Currently copying file (for display)

void print_progress(int verbose) {
    time_t now = time(NULL);
    int percent;
    unsigned long long copied_mb;
    unsigned long long total_mb;
    
    // Throttle updates to once per-second when not using --verbose flag
    if (!verbose && (now - last_update) < 1) {
        return;
    }
    
    last_update = now;
    
    if (total_size > 0) {
        // Calculate and display progress %
        percent = (int)((total_copied * 100) / total_size);
        copied_mb = total_copied / (1024 * 1024);
        total_mb = total_size / (1024 * 1024);
        
        printf("\rCopying: %llu MB / %llu MB (%d%%) - %s", 
               copied_mb, total_mb, percent, 
               current_file[0] ? current_file : "");
        fflush(stdout);
    }
}

// Try to copy using sendfile() - zero-copy kernel transfer.
static int copy_file_sendfile(const char *source, const char *target) {
    int src_fd, dst_fd;
    struct stat st;
    off_t offset = 0;
    ssize_t bytes_sent;
    
    src_fd = open(source, O_RDONLY);
    if (src_fd < 0) {
        return -1;
    }
    
    if (fstat(src_fd, &st) != 0) {
        close(src_fd);
        return -1;
    }
    
    // Don't use sendfile for small files (overhead not worth it)
    if (st.st_size < 1024 * 1024) {
        close(src_fd);
        return -1;
    }
    
    dst_fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        close(src_fd);
        return -1;
    }
    
    // Advise kernel about access pattern
    posix_fadvise(src_fd, 0, st.st_size, POSIX_FADV_SEQUENTIAL);
    
    while (offset < st.st_size) {
        bytes_sent = sendfile(dst_fd, src_fd, &offset, st.st_size - offset);
        if (bytes_sent <= 0) {
            if (errno == EINTR) {
                continue;  // Interrupted, retry
            }
            if (errno == EINVAL || errno == ENOSYS) {
                // sendfile not supported for this file type
                close(src_fd);
                close(dst_fd);
                return -1;
            }
            // Real error
            log_write(g_log_ctx, LOG_ERROR, "sendfile failed: %s", strerror(errno));
            close(src_fd);
            close(dst_fd);
            return -1;
        }
        
        total_copied += bytes_sent;
        print_progress(0);
    }
    
    // Sync to disk
    if (fsync(dst_fd) != 0) {
        log_write(g_log_ctx, LOG_WARNING, "fsync failed for: %s", target);
    }
    
    close(src_fd);
    close(dst_fd);
    
    struct timespec times[2];
    times[0].tv_sec = st.st_atime;
    times[0].tv_nsec = 0;
    times[1].tv_sec = st.st_mtime;
    times[1].tv_nsec = 0;
    utimensat(AT_FDCWD, target, times, 0);
    
    chmod(target, st.st_mode);
    
    return 0;
}

// Copy using aligned buffers
// This is a fallback for when sendfile doesn't work
static int copy_file_buffered(const char *source, const char *target) {
    int src_fd, dst_fd;
    char *buffer = NULL;
    ssize_t bytes_read, bytes_written, total_written;
    struct stat st;
    int result = 0;
    
    src_fd = open(source, O_RDONLY);
    if (src_fd < 0) {
        log_write(g_log_ctx, LOG_ERROR, "Failed to open source: %s (%s)", 
                  source, strerror(errno));
        return -1;
    }
    
    if (fstat(src_fd, &st) != 0) {
        log_write(g_log_ctx, LOG_ERROR, "Failed to stat source: %s", source);
        close(src_fd);
        return -1;
    }
    
    dst_fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        log_write(g_log_ctx, LOG_ERROR, "Failed to open target: %s (%s)", 
                  target, strerror(errno));
        close(src_fd);
        return -1;
    }
    
    if (posix_memalign((void **)&buffer, 4096, BLOCK_SIZE) != 0) {
        log_write(g_log_ctx, LOG_ERROR, "Memory allocation failed");
        close(src_fd);
        close(dst_fd);
        return -1;
    }
    
    // Advise kernel about our access patterns
    posix_fadvise(src_fd, 0, st.st_size, POSIX_FADV_SEQUENTIAL);
    posix_fadvise(dst_fd, 0, 0, POSIX_FADV_DONTNEED);  // Don't cache writes
    
    // Copy data in large blocks
    while ((bytes_read = read(src_fd, buffer, BLOCK_SIZE)) > 0) {
        total_written = 0;
        
        // Handle partial writes
        while (total_written < bytes_read) {
            bytes_written = write(dst_fd, buffer + total_written, 
                                 bytes_read - total_written);
            
            if (bytes_written < 0) {
                if (errno == EINTR) {
                    continue;  // Interrupted, retry
                }
                log_write(g_log_ctx, LOG_ERROR, "Write failed: %s (%s)", 
                          target, strerror(errno));
                result = -1;
                goto cleanup;
            }
            
            total_written += bytes_written;
        }
        
        total_copied += bytes_read;
        print_progress(0);
    }
    
    if (bytes_read < 0) {
        log_write(g_log_ctx, LOG_ERROR, "Read failed: %s (%s)", 
                  source, strerror(errno));
        result = -1;
        goto cleanup;
    }
    
    // Force write to disk
    if (fsync(dst_fd) != 0) {
        log_write(g_log_ctx, LOG_WARNING, "fsync failed: %s", target);
    }
    
cleanup:
    free(buffer);
    close(src_fd);
    close(dst_fd);
    
    if (result == 0) {
        struct timespec times[2];
        times[0].tv_sec = st.st_atime;
        times[0].tv_nsec = 0;
        times[1].tv_sec = st.st_mtime;
        times[1].tv_nsec = 0;
        utimensat(AT_FDCWD, target, times, 0);
        
        chmod(target, st.st_mode);
    } else {        // Remove incomplete file on error
        unlink(target);
    }
    
    return result;
}

int copy_file(const char *source, const char *target) {
    const char *display_name;
    
    // Set current file for progress display
    display_name = source;
    if (strlen(source) > 50) {
        display_name = source + strlen(source) - 50;
    }
    strncpy(current_file, display_name, sizeof(current_file) - 1);
    current_file[sizeof(current_file) - 1] = '\0';
    
    // Try sendfile first
    if (copy_file_sendfile(source, target) == 0) {
        return 0;
    }
    
    // Fall back to buffered copy
    return copy_file_buffered(source, target);
}

// This is for subdirectories
int copy_directory_recursive(const char *source, const char *target, int verbose) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char source_path[MAX_PATH];
    char target_path[MAX_PATH];
    const char *display_name;
    
    dir = opendir(source);
    if (dir == NULL) {
        fprintf(stderr, "\nError: Failed to open directory: %s - %s\n", source, strerror(errno));
        log_write(g_log_ctx, LOG_ERROR, "Failed to open directory: %s", source);
        return -1;
    }
    
    // Create target directory if it doesn't already exist
    if (!is_directory(target)) {
        if (make_directory(target) != 0) {
            fprintf(stderr, "\nError: Failed to create directory: %s - %s\n", target, strerror(errno));
            log_write(g_log_ctx, LOG_ERROR, "Failed to create directory: %s", target);
            closedir(dir);
            return -1;
        }
    }
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip all . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Build full paths
        snprintf(source_path, sizeof(source_path), "%s/%s", source, entry->d_name);
        snprintf(target_path, sizeof(target_path), "%s/%s", target, entry->d_name);
        
        // Get entry metadata
        if (lstat(source_path, &st) != 0) {
            fprintf(stderr, "\nWarning: Cannot stat: %s - %s\n", source_path, strerror(errno));
            log_write(g_log_ctx, LOG_WARNING, "Cannot stat: %s", source_path);
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            if (copy_directory_recursive(source_path, target_path, verbose) != 0) {
                closedir(dir);
                return -1;
            }
        } else if (S_ISREG(st.st_mode)) {
            // Truncate long file paths for display
            display_name = source_path;
            if (strlen(source_path) > 50) {
                display_name = source_path + strlen(source_path) - 50;
            }
            strncpy(current_file, display_name, sizeof(current_file) - 1);
            current_file[sizeof(current_file) - 1] = '\0';
            
            if (verbose) {
                printf("\nCopying: %s", source_path);
                fflush(stdout);
            }
            
            if (copy_file(source_path, target_path) != 0) {
                fprintf(stderr, "\nFailed to copy: %s\n", source_path);
                closedir(dir);
                return -1;
            }
            
            print_progress(verbose);
        }
        // Skip symlinks, device files, etc.
    }
    
    closedir(dir);
    return 0;
}

int copy_filesystem_files(const char *source, const char *target, int verbose) {
    // Reset progress tracking
    total_copied = 0;
    total_size = get_directory_size(source);
    last_update = 0;
    
    if (total_size == 0) {
        fprintf(stderr, "Error: Source directory appears to be empty\n");
        log_write(g_log_ctx, LOG_ERROR, "Source directory appears to be empty");
        return -1;
    }
    
    printf("Total size to copy: %llu MB\n", total_size / (1024 * 1024));
    log_write(g_log_ctx, LOG_INFO, "Total size to copy: %llu MB", total_size / (1024 * 1024));
    
    if (copy_directory_recursive(source, target, verbose) != 0) {
        fprintf(stderr, "\nError: File copy failed\n");
        log_write(g_log_ctx, LOG_ERROR, "File copy operation failed");
        return -1;
    }
    
    printf("\n");
    print_colored("File copy complete", "green");
    log_write(g_log_ctx, LOG_SUCCESS, "File copy completed - %llu MB copied", total_copied / (1024 * 1024));
    
    return 0;
}