#include "../include/buf.h"

int create_mountpoints(MountPoints *mounts) {
    time_t now = time(NULL);
    pid_t pid = getpid();
    
    snprintf(mounts->source_mountpoint, sizeof(mounts->source_mountpoint), 
             "/tmp/buf_source_%ld_%d", (long)now, pid);
    
    snprintf(mounts->target_mountpoint, sizeof(mounts->target_mountpoint), 
             "/tmp/buf_target_%ld_%d", (long)now, pid);
    
    snprintf(mounts->temp_directory, sizeof(mounts->temp_directory), 
             "/tmp/buf_temp_%ld_%d", (long)now, pid);
    
    if (make_directory(mounts->source_mountpoint) != 0) {
        fprintf(stderr, "Error: Failed to create source mountpoint\n");
        return -1;
    }
    
    if (make_directory(mounts->target_mountpoint) != 0) {
        fprintf(stderr, "Error: Failed to create target mountpoint\n");
        return -1;
    }
    
    if (make_directory(mounts->temp_directory) != 0) {
        fprintf(stderr, "Error: Failed to create temp directory\n");
        return -1;
    }
    
    return 0;
}

int mount_source(const char *source, const char *mountpoint) {
    char command[MAX_PATH];
    struct stat st;
    
    print_colored("Mounting source media...", "green");
    
    if (stat(source, &st) == 0 && S_ISREG(st.st_mode)) {
        snprintf(command, sizeof(command), 
                "mount -o loop,ro -t udf,iso9660 '%s' '%s' 2>/dev/null", 
                source, mountpoint);
    } else {
        snprintf(command, sizeof(command), 
                "mount -o ro '%s' '%s' 2>/dev/null", 
                source, mountpoint);
    }
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: Failed to mount source media\n");
        return -1;
    }
    
    return 0;
}

int mount_target(const char *target, const char *mountpoint) {
    char command[MAX_PATH];
    
    print_colored("Mounting target partition...", "green");
    
    snprintf(command, sizeof(command), "mount '%s' '%s' 2>/dev/null", target, mountpoint);
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: Failed to mount target partition\n");
        return -1;
    }
    
    return 0;
}

int cleanup_mountpoint(const char *mountpoint) {
    char command[MAX_PATH];
    struct stat st;
    
    if (stat(mountpoint, &st) != 0) {
        return 0;
    }
    
    snprintf(command, sizeof(command), "mountpoint -q '%s'", mountpoint);
    if (run_command(command) == 0) {
        print_colored("Unmounting filesystem...", "");
        
        snprintf(command, sizeof(command), "umount '%s' 2>/dev/null", mountpoint);
        if (run_command(command) != 0) {
            fprintf(stderr, "Warning: Failed to unmount %s\n", mountpoint);
            return -1;
        }
    }
    
    if (rmdir(mountpoint) != 0) {
        fprintf(stderr, "Warning: Failed to remove mountpoint %s\n", mountpoint);
        return -1;
    }
    
    return 0;
}

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
        snprintf(command, sizeof(command), "rm -rf '%s' 2>/dev/null", mounts->temp_directory);
        run_command(command);
    }
    
    if (unsafe) {
        print_colored("Warning: Target filesystem may not be fully unmounted", "yellow");
        print_colored("Please unmount manually before removing device", "yellow");
    } else {
        if (is_device_busy(target_media)) {
            print_colored("Warning: Target device is still busy", "yellow");
        }
    }
}