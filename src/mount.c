#include "../include/buf.h"

// Create temp mount points
int create_mountpoints(MountPoints *mounts) {
    time_t now = time(NULL);
    pid_t pid = getpid();
    
    // Generate mount point names using timestamp and process ID
    snprintf(mounts->source_mountpoint, sizeof(mounts->source_mountpoint), 
             "/tmp/buf_source_%ld_%d", (long)now, pid);
    
    snprintf(mounts->target_mountpoint, sizeof(mounts->target_mountpoint), 
             "/tmp/buf_target_%ld_%d", (long)now, pid);
    
    snprintf(mounts->temp_directory, sizeof(mounts->temp_directory), 
             "/tmp/buf_temp_%ld_%d", (long)now, pid);
    
    if (make_directory(mounts->source_mountpoint) != 0) {
        fprintf(stderr, "Error: Failed to create source mountpoint\n");
        log_write(g_log_ctx, LOG_ERROR, "Failed to create source mountpoint: %s", mounts->source_mountpoint);
        return -1;
    }
    
    if (make_directory(mounts->target_mountpoint) != 0) {
        fprintf(stderr, "Error: Failed to create target mountpoint\n");
        log_write(g_log_ctx, LOG_ERROR, "Failed to create target mountpoint: %s", mounts->target_mountpoint);
        return -1;
    }
    
    if (make_directory(mounts->temp_directory) != 0) {
        fprintf(stderr, "Error: Failed to create temp directory\n");
        log_write(g_log_ctx, LOG_ERROR, "Failed to create temp directory: %s", mounts->temp_directory);
        return -1;
    }
    
    return 0;
}

int mount_source(const char *source, const char *mountpoint) {
    char command[MAX_PATH];
    struct stat st;
    
    print_colored("Mounting source media...", "green");
    log_write(g_log_ctx, LOG_STEP, "Mounting source media: %s -> %s", source, mountpoint);
    
    // Check if source is a regular ISO or block device
    if (stat(source, &st) == 0 && S_ISREG(st.st_mode)) {
        // It's a regular ISO, mount as loop device with UDF or ISO9660 filesystem
        snprintf(command, sizeof(command), 
                "mount -o loop,ro -t udf,iso9660 '%s' '%s' 2>/dev/null", 
                source, mountpoint);
        log_write(g_log_ctx, LOG_INFO, "Source is a file, mounting as loop device");
    } else {
        // It's a block device, mount directly as read-only
        snprintf(command, sizeof(command), 
                "mount -o ro '%s' '%s' 2>/dev/null", 
                source, mountpoint);
        log_write(g_log_ctx, LOG_INFO, "Source is a block device");
    }
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: Failed to mount source media\n");
        log_write(g_log_ctx, LOG_ERROR, "Mount command failed for source media");
        return -1;
    }
    
    log_write(g_log_ctx, LOG_SUCCESS, "Source media mounted successfully");
    return 0;
}

int mount_target(const char *target, const char *mountpoint) {
    char command[MAX_PATH];
    
    print_colored("Mounting target partition...", "green");
    log_write(g_log_ctx, LOG_STEP, "Mounting target partition: %s -> %s", target, mountpoint);
    
    snprintf(command, sizeof(command), "mount '%s' '%s' 2>/dev/null", target, mountpoint);
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: Failed to mount target partition\n");
        log_write(g_log_ctx, LOG_ERROR, "Mount command failed for target partition");
        return -1;
    }
    
    log_write(g_log_ctx, LOG_SUCCESS, "Target partition mounted successfully");
    return 0;
}

int cleanup_mountpoint(const char *mountpoint) {
    char command[MAX_PATH];
    struct stat st;
    
    // Check if mount point directory exists
    if (stat(mountpoint, &st) != 0) {
        return 0; // Directory doesn't exist, nothing for us to cleanup
    }
    
    snprintf(command, sizeof(command), "mountpoint -q '%s'", mountpoint);
    if (run_command(command) == 0) {
        // Mount point is mounted, unmount it
        print_colored("Unmounting filesystem...", "");
        log_write(g_log_ctx, LOG_INFO, "Unmounting: %s", mountpoint);
        
        snprintf(command, sizeof(command), "umount '%s' 2>/dev/null", mountpoint);
        if (run_command(command) != 0) {
            fprintf(stderr, "Warning: Failed to unmount %s\n", mountpoint);
            log_write(g_log_ctx, LOG_WARNING, "Failed to unmount: %s", mountpoint);
            return -1;
        }
        
        log_write(g_log_ctx, LOG_SUCCESS, "Unmounted: %s", mountpoint);
    }
    
    // Remove mount point directory
    if (rmdir(mountpoint) != 0) {
        fprintf(stderr, "Warning: Failed to remove mountpoint %s\n", mountpoint);
        log_write(g_log_ctx, LOG_WARNING, "Failed to remove mountpoint: %s", mountpoint);
        return -1;
    }
    
    return 0;
}

// Cleanup all the mount points and temp directories
void cleanup(MountPoints *mounts, const char *target_media) {
    int unsafe = 0;
    char command[MAX_PATH];
    
    if (cleanup_mountpoint(mounts->source_mountpoint) != 0) {
        print_colored("Warning: Source mountpoint not fully cleaned", "yellow");
    }
    
    if (cleanup_mountpoint(mounts->target_mountpoint) != 0) {
        print_colored("Warning: Target mountpoint not fully cleaned", "yellow");
        unsafe = 1;
    }
    
    if (mounts->temp_directory[0] != '\0') {
        log_write(g_log_ctx, LOG_INFO, "Removing temp directory: %s", mounts->temp_directory);
        snprintf(command, sizeof(command), "rm -rf '%s' 2>/dev/null", mounts->temp_directory);
        run_command(command);
    }
    
    if (unsafe) {
        print_colored("Warning: Target filesystem may not be fully unmounted", "yellow");
        print_colored("Please unmount manually before removing device", "yellow");
        log_write(g_log_ctx, LOG_WARNING, "Target filesystem may not be fully unmounted");
    } else {
        if (is_device_busy(target_media)) {
            print_colored("Warning: Target device is still busy", "yellow");
            log_write(g_log_ctx, LOG_WARNING, "Target device is still busy");
        }
    }
}