#include "../include/buf.h"

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

int copy_file(const char *source, const char *target) {
    FILE *src_file;
    FILE *dst_file;
    unsigned char *buffer;
    size_t bytes_read;
    size_t bytes_written;
    struct stat st;
    int result = 0;
    unsigned long long file_copied = 0;
    struct timespec times[2];
    
    // Get source metadata
    if (stat(source, &st) != 0) {
        fprintf(stderr, "\nError: Cannot stat source file: %s\n", source);
        log_write(g_log_ctx, LOG_ERROR, "Cannot stat source file: %s", source);
        return -1;
    }
    
    src_file = fopen(source, "rb");
    if (src_file == NULL) {
        fprintf(stderr, "\nError: Failed to open source file: %s (%s)\n", source, strerror(errno));
        log_write(g_log_ctx, LOG_ERROR, "Failed to open source file: %s (%s)", source, strerror(errno));
        return -1;
    }
    
    dst_file = fopen(target, "wb");
    if (dst_file == NULL) {
        fprintf(stderr, "\nError: Failed to create target file: %s (%s)\n", target, strerror(errno));
        log_write(g_log_ctx, LOG_ERROR, "Failed to create target file: %s (%s)", target, strerror(errno));
        fclose(src_file);
        return -1;
    }
    
    // Allocate the buffer
    buffer = (unsigned char *)malloc(BUFFER_SIZE);
    if (buffer == NULL) {
        fprintf(stderr, "\nError: Memory allocation failed\n");
        log_write(g_log_ctx, LOG_ERROR, "Memory allocation failed for copy buffer");
        fclose(src_file);
        fclose(dst_file);
        return -1;
    }
    
    // Copy files in chunks using said buffer
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, src_file)) > 0) {
        bytes_written = fwrite(buffer, 1, bytes_read, dst_file);
        if (bytes_written != bytes_read) {
            fprintf(stderr, "\nError: Write failed for: %s (wrote %zu of %zu bytes) - %s\n", 
                    target, bytes_written, bytes_read, strerror(errno));
            log_write(g_log_ctx, LOG_ERROR, "Write failed for: %s (wrote %zu of %zu bytes)", 
                      target, bytes_written, bytes_read);
            result = -1;
            break;
        }
        
        total_copied += bytes_read;
        file_copied += bytes_read;
        
        // Update progress display over periods (every 10MB or at file completion)
        if (file_copied % (10 * 1024 * 1024) == 0 || file_copied == (unsigned long long)st.st_size) {
            print_progress(0);
        }
    }
    
    if (ferror(src_file)) {
        fprintf(stderr, "\nError: Read error for: %s - %s\n", source, strerror(errno));
        log_write(g_log_ctx, LOG_ERROR, "Read error for: %s", source);
        result = -1;
    }
    
    free(buffer);
    
    if (fflush(dst_file) != 0) {
        fprintf(stderr, "\nError: Failed to flush file: %s - %s\n", target, strerror(errno));
        log_write(g_log_ctx, LOG_ERROR, "Failed to flush file: %s", target);
        result = -1;
    }
    
    // Force write to USB
    if (fsync(fileno(dst_file)) != 0) {
        fprintf(stderr, "\nWarning: Failed to sync file: %s - %s\n", target, strerror(errno));
        log_write(g_log_ctx, LOG_WARNING, "Failed to sync file: %s", target);
    }
    
    fclose(src_file);
    fclose(dst_file);
    
    if (result == 0) {
        times[0].tv_sec = st.st_atime;
        times[0].tv_nsec = 0;
        times[1].tv_sec = st.st_mtime;
        times[1].tv_nsec = 0;
        utimensat(AT_FDCWD, target, times, 0);
        chmod(target, st.st_mode);
    } else {
        // Remove incomplete file if we error out
        unlink(target);
    }
    
    return result;
}

// This is for subdirectories
// (could we not just make copy_file do this?)
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