#include "../include/buf.h"

int check_dependencies(void) {
    const char *required_commands[] = {
        "mount", "umount", "wipefs", "lsblk", "blockdev", 
        "df", "parted", "7z", NULL
    };
    int i;
    char command[256];
    int missing = 0;
    
    for (i = 0; required_commands[i] != NULL; i++) {
        snprintf(command, sizeof(command), "which %s >/dev/null 2>&1", required_commands[i]);
        if (run_command(command) != 0) {
            fprintf(stderr, "Error: Required command '%s' not found\n", required_commands[i]);
            log_write(g_log_ctx, LOG_ERROR, "Required command not found: %s", required_commands[i]);
            missing = 1;
        }
    }
    
    snprintf(command, sizeof(command), "which mkdosfs >/dev/null 2>&1 || which mkfs.vfat >/dev/null 2>&1 || which mkfs.fat >/dev/null 2>&1");
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: FAT filesystem tools not found (install dosfstools)\n");
        log_write(g_log_ctx, LOG_ERROR, "FAT filesystem tools not found (dosfstools required)");
        missing = 1;
    }
    
    snprintf(command, sizeof(command), "which mkntfs >/dev/null 2>&1");
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: NTFS filesystem tools not found (install ntfs-3g)\n");
        log_write(g_log_ctx, LOG_ERROR, "NTFS filesystem tools not found (ntfs-3g required)");
        missing = 1;
    }
    
    snprintf(command, sizeof(command), "which grub-install >/dev/null 2>&1 || which grub2-install >/dev/null 2>&1");
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: GRUB not found (install grub2 or grub-pc)\n");
        log_write(g_log_ctx, LOG_ERROR, "GRUB not found (grub2 or grub-pc required)");
        missing = 1;
    }
    
    return missing ? -1 : 0;
}

int check_source_media(const char *source) {
    struct stat st;
    
    if (!file_exists(source)) {
        fprintf(stderr, "Error: Source media '%s' not found\n", source);
        log_write(g_log_ctx, LOG_ERROR, "Source media not found: %s", source);
        return -1;
    }
    
    if (!is_block_device(source)) {
        if (stat(source, &st) != 0) {
            fprintf(stderr, "Error: Cannot access source media '%s'\n", source);
            log_write(g_log_ctx, LOG_ERROR, "Cannot access source media: %s", source);
            return -1;
        }
        
        if (!S_ISREG(st.st_mode)) {
            fprintf(stderr, "Error: Source must be a regular file or block device\n");
            log_write(g_log_ctx, LOG_ERROR, "Source is not a regular file or block device: %s", source);
            return -1;
        }
    }
    
    return 0;
}

int check_target_media(const char *target, InstallMode mode) {
    if (!is_block_device(target)) {
        fprintf(stderr, "Error: Target '%s' is not a block device\n", target);
        log_write(g_log_ctx, LOG_ERROR, "Target is not a block device: %s", target);
        return -1;
    }
    
    if (mode == MODE_WIPE) {
        if (isdigit(target[strlen(target) - 1])) {
            fprintf(stderr, "Error: Target must be a device (e.g., /dev/sdb), not a partition\n");
            log_write(g_log_ctx, LOG_ERROR, "Wipe mode requires a device, not partition: %s", target);
            return -1;
        }
    } else if (mode == MODE_PARTITION) {
        if (!isdigit(target[strlen(target) - 1])) {
            fprintf(stderr, "Error: Target must be a partition (e.g., /dev/sdb1), not a device\n");
            log_write(g_log_ctx, LOG_ERROR, "Partition mode requires a partition, not device: %s", target);
            return -1;
        }
    }
    
    return 0;
}

int determine_target_parameters(Config *config) {
    size_t len;
    
    if (config->mode == MODE_PARTITION) {
        strncpy(config->target_partition, config->target, sizeof(config->target_partition) - 1);
        
        strncpy(config->target_device, config->target, sizeof(config->target_device) - 1);
        len = strlen(config->target_device);
        while (len > 0 && isdigit(config->target_device[len - 1])) {
            config->target_device[len - 1] = '\0';
            len--;
        }
    } else {
        strncpy(config->target_device, config->target, sizeof(config->target_device) - 1);
        snprintf(config->target_partition, sizeof(config->target_partition), "%s1", config->target_device);
    }
    
    if (config->verbose) {
        printf("Target device: %s\n", config->target_device);
        printf("Target partition: %s\n", config->target_partition);
    }
    
    return 0;
}

ISOType detect_iso_type(const char *source_mountpoint) {
    char bootmgr_path[MAX_PATH];
    char sources_path[MAX_PATH];
    char isolinux_path[MAX_PATH];
    char syslinux_path[MAX_PATH];
    
    snprintf(bootmgr_path, sizeof(bootmgr_path), "%s/bootmgr", source_mountpoint);
    snprintf(sources_path, sizeof(sources_path), "%s/sources", source_mountpoint);
    
    if (file_exists(bootmgr_path) && is_directory(sources_path)) {
        return ISO_WINDOWS;
    }
    
    snprintf(isolinux_path, sizeof(isolinux_path), "%s/isolinux", source_mountpoint);
    snprintf(syslinux_path, sizeof(syslinux_path), "%s/syslinux", source_mountpoint);
    
    if (is_directory(isolinux_path) || is_directory(syslinux_path)) {
        return ISO_LINUX;
    }
    
    return ISO_OTHER;
}

int is_device_busy(const char *device) {
    char command[MAX_PATH];
    
    snprintf(command, sizeof(command), "mount | grep -q '%s'", device);
    if (run_command(command) == 0) {
        return 1;
    }
    
    return 0;
}

int unmount_device(const char *device) {
    char command[MAX_PATH];
    FILE *pipe;
    char line[512];
    int unmounted = 0;
    
    snprintf(command, sizeof(command), "mount | grep '^%s'", device);
    pipe = popen(command, "r");
    
    if (pipe == NULL) {
        log_write(g_log_ctx, LOG_ERROR, "Failed to check mount status for: %s", device);
        return -1;
    }
    
    while (fgets(line, sizeof(line), pipe) != NULL) {
        char device_name[MAX_PATH];
        char mount_point[MAX_PATH];
        char umount_cmd[MAX_PATH];
        char *space_pos;
        char *type_pos;
        size_t len;
        
        if (sscanf(line, "%s on %s", device_name, mount_point) == 2) {
            space_pos = strstr(line, " on ");
            if (space_pos != NULL) {
                space_pos += 4;
                type_pos = strstr(space_pos, " type ");
                if (type_pos != NULL) {
                    len = type_pos - space_pos;
                    if (len < sizeof(mount_point)) {
                        strncpy(mount_point, space_pos, len);
                        mount_point[len] = '\0';
                    }
                }
            }
            
            printf("Unmounting %s from %s...\n", device_name, mount_point);
            log_write(g_log_ctx, LOG_INFO, "Unmounting %s from %s", device_name, mount_point);
            
            snprintf(umount_cmd, sizeof(umount_cmd), "umount '%s' 2>/dev/null", mount_point);
            if (run_command(umount_cmd) != 0) {
                snprintf(umount_cmd, sizeof(umount_cmd), "umount -l '%s' 2>/dev/null", mount_point);
                if (run_command(umount_cmd) != 0) {
                    fprintf(stderr, "Warning: Failed to unmount %s\n", mount_point);
                    log_write(g_log_ctx, LOG_ERROR, "Failed to unmount: %s", mount_point);
                    pclose(pipe);
                    return -1;
                }
            }
            unmounted = 1;
        }
    }
    
    pclose(pipe);
    
    if (unmounted) {
        sleep(1);
    }
    
    return 0;
}

int check_fat32_limitation(const char *source_mountpoint, FilesystemType *fs_type) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char full_path[MAX_PATH];
    
    dir = opendir(source_mountpoint);
    if (dir == NULL) {
        return 0;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", source_mountpoint, entry->d_name);
        
        if (lstat(full_path, &st) != 0) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            if (check_fat32_limitation(full_path, fs_type) != 0) {
                closedir(dir);
                return 1;
            }
        } else if (S_ISREG(st.st_mode)) {
            if (st.st_size > FAT32_MAX_FILESIZE) {
                log_write(g_log_ctx, LOG_WARNING, "Large file detected (>4GB): %s (%llu bytes)", 
                          full_path, (unsigned long long)st.st_size);
                closedir(dir);
                *fs_type = FS_NTFS;
                return 1;
            }
        }
    }
    
    closedir(dir);
    return 0;
}

int check_free_space(const char *source_mountpoint, const char *target_mountpoint, 
                    const char *target_partition) {
    unsigned long long needed_space;
    unsigned long long free_space;
    unsigned long long additional_space = 10 * 1024 * 1024;
    
    needed_space = get_directory_size(source_mountpoint) + additional_space;
    free_space = get_free_space(target_mountpoint);
    
    if (needed_space > free_space) {
        fprintf(stderr, "Error: Not enough space on target partition\n");
        fprintf(stderr, "       Required: %llu bytes\n", needed_space);
        fprintf(stderr, "       Available: %llu bytes\n", free_space);
        log_write(g_log_ctx, LOG_ERROR, "Insufficient space on target partition");
        log_write(g_log_ctx, LOG_ERROR, "Required: %llu MB, Available: %llu MB", 
                  needed_space / (1024 * 1024), free_space / (1024 * 1024));
        return -1;
    }
    
    return 0;
}