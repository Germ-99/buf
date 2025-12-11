#include "../include/buf.h"

int main(int argc, char *argv[]) {
    Config config = {0};
    MountPoints mounts = {0};
    char uefi_partition[MAX_PATH];
    LogContext log_ctx = {0};
    int operation_success = 0;

    // Error out if not running with root privileges
    if (!check_root_privileges()) {
        fprintf(stderr, "Error: buf must be run as sudo\n");
        return 1;
    }
    
    // Config with default values
    config.filesystem = FS_FAT;
    config.verbose = 0;
    config.no_log = 0;
    config.iso_type = ISO_UNKNOWN;
    strncpy(config.label, DEFAULT_FS_LABEL, sizeof(config.label) - 1);

    if (parse_arguments(argc, argv, &config) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    // Ignore logging if --no-log flag is passed
    if (!config.no_log) {
        if (log_init(&log_ctx, NULL) == 0) {
            g_log_ctx = &log_ctx;
            log_system_info(&log_ctx);
	    log_command_invocation(&log_ctx, argc, argv);
        }
    }

    print_colored("buf v" VERSION, "");
    print_colored("================================", "");
    
    log_write(&log_ctx, LOG_STEP, "Starting buf v%s", VERSION);

    // Dependency check
    if (check_dependencies() != 0) {
        fprintf(stderr, "Error: Required dependencies not found\n");
        log_write(&log_ctx, LOG_ERROR, "Required dependencies check failed");
        log_close(&log_ctx, 0);
        return 1;
    }
    
    log_write(&log_ctx, LOG_SUCCESS, "All dependencies verified");

    // Make sure source media exists
    if (check_source_media(config.source) != 0) {
        log_write(&log_ctx, LOG_ERROR, "Source media validation failed: %s", config.source);
        log_close(&log_ctx, 0);
        return 1;
    }
    
    log_write(&log_ctx, LOG_SUCCESS, "Source media validated: %s", config.source);

    // Check target device/partition is correct for selected mode
    if (check_target_media(config.target, config.mode) != 0) {
        log_write(&log_ctx, LOG_ERROR, "Target media validation failed: %s", config.target);
        log_close(&log_ctx, 0);
        return 1;
    }
    
    log_write(&log_ctx, LOG_SUCCESS, "Target media validated: %s", config.target);

    // Calculate target device and partition paths based on mode
    if (determine_target_parameters(&config) != 0) {
        log_write(&log_ctx, LOG_ERROR, "Failed to determine target parameters");
        log_close(&log_ctx, 0);
        return 1;
    }
    
    log_write(&log_ctx, LOG_INFO, "Target device: %s", config.target_device);
    log_write(&log_ctx, LOG_INFO, "Target partition: %s", config.target_partition);

    // Check if device is currently mounted
    if (is_device_busy(config.source)) {
        fprintf(stderr, "Error: Source media is currently in use\n");
        log_write(&log_ctx, LOG_ERROR, "Source media is currently in use");
        log_close(&log_ctx, 0);
        return 1;
    }

    // Partition mode handling
    if (config.mode == MODE_PARTITION) {
        if (is_device_busy(config.target_partition)) {
            print_colored("Target partition is mounted, unmounting...", "yellow");
            log_write(&log_ctx, LOG_WARNING, "Target partition is mounted, attempting to unmount");
            
            if (unmount_device(config.target_partition) != 0) {
                fprintf(stderr, "Error: Failed to unmount target partition\n");
                log_write(&log_ctx, LOG_ERROR, "Failed to unmount target partition: %s", config.target_partition);
                log_close(&log_ctx, 0);
                return 1;
            }
            
            log_write(&log_ctx, LOG_SUCCESS, "Target partition unmounted successfully");
        }
    } else {
        // Wipe mode handling
        if (is_device_busy(config.target_device)) {
            print_colored("Target device is mounted, unmounting...", "yellow");
            log_write(&log_ctx, LOG_WARNING, "Target device is mounted, attempting to unmount");
            
            if (unmount_device(config.target_device) != 0) {
                fprintf(stderr, "Error: Failed to unmount target device\n");
                log_write(&log_ctx, LOG_ERROR, "Failed to unmount target device: %s", config.target_device);
                log_close(&log_ctx, 0);
                return 1;
            }
            
            log_write(&log_ctx, LOG_SUCCESS, "Target device unmounted successfully");
        }
    }

    // Create temporary mount points
    if (create_mountpoints(&mounts) != 0) {
        fprintf(stderr, "Error: Failed to create mountpoints\n");
        log_write(&log_ctx, LOG_ERROR, "Failed to create temporary mountpoints");
        log_close(&log_ctx, 0);
        return 1;
    }
    
    log_write(&log_ctx, LOG_SUCCESS, "Created temporary mountpoints");
    log_write(&log_ctx, LOG_INFO, "Source mountpoint: %s", mounts.source_mountpoint);
    log_write(&log_ctx, LOG_INFO, "Target mountpoint: %s", mounts.target_mountpoint);

    if (mount_source(config.source, mounts.source_mountpoint) != 0) {
        fprintf(stderr, "Error: Failed to mount source media\n");
        log_write(&log_ctx, LOG_ERROR, "Failed to mount source media: %s", config.source);
        cleanup(&mounts, config.target);
        log_close(&log_ctx, 0);
        return 1;
    }
    
    log_write(&log_ctx, LOG_SUCCESS, "Source media mounted successfully");

    // What ISO is this?
    config.iso_type = detect_iso_type(mounts.source_mountpoint);
    log_write(&log_ctx, LOG_INFO, "Detected ISO type: %s", 
              config.iso_type == ISO_WINDOWS ? "Windows" : 
              config.iso_type == ISO_LINUX ? "Linux" : "Other");

    // Check if files exceed FAT32 limits. If so, switch to NTFS.
    if (config.iso_type == ISO_WINDOWS) {
        if (check_fat32_limitation(mounts.source_mountpoint, &config.filesystem) != 0) {
            print_colored("Notice: Large files detected, switching to NTFS", "yellow");
            log_write(&log_ctx, LOG_WARNING, "Large files detected (>4GB), switching to NTFS filesystem");
            config.filesystem = FS_NTFS;
        } else {
            log_write(&log_ctx, LOG_INFO, "No large files detected, using FAT32 filesystem");
        }
    }
    
    log_config(&log_ctx, &config);

    // Wipe mode execution
    if (config.mode == MODE_WIPE) {
        log_section(&log_ctx, "DEVICE PREPARATION");
        
        print_colored("Preparing target device...", "green");
        log_write(&log_ctx, LOG_STEP, "Starting device preparation (wipe mode)");
        
        // Wipe existing FS signatures
        if (wipe_device(config.target_device) != 0) {
            fprintf(stderr, "Error: Failed to wipe device\n");
            log_write(&log_ctx, LOG_ERROR, "Failed to wipe device: %s", config.target_device);
            cleanup(&mounts, config.target);
            log_close(&log_ctx, 0);
            return 1;
        }
        
        log_write(&log_ctx, LOG_SUCCESS, "Device wiped successfully");

        // Create MSDOS partition table
        if (create_partition_table(config.target_device) != 0) {
            fprintf(stderr, "Error: Failed to create partition table\n");
            log_write(&log_ctx, LOG_ERROR, "Failed to create partition table on: %s", config.target_device);
            cleanup(&mounts, config.target);
            log_close(&log_ctx, 0);
            return 1;
        }
        
        log_write(&log_ctx, LOG_SUCCESS, "Partition table created (MSDOS/MBR)");

        // Create and format partition
        if (create_partition(config.target_device, config.target_partition, 
                           config.filesystem, config.label) != 0) {
            fprintf(stderr, "Error: Failed to create partition\n");
            log_write(&log_ctx, LOG_ERROR, "Failed to create partition: %s", config.target_partition);
            cleanup(&mounts, config.target);
            log_close(&log_ctx, 0);
            return 1;
        }
        
        log_write(&log_ctx, LOG_SUCCESS, "Partition created and formatted: %s (%s)", 
                  config.target_partition, config.filesystem == FS_NTFS ? "NTFS" : "FAT32");

        // Create UEFI:NTFS helper partition for windows NTFS installs
        if (config.iso_type == ISO_WINDOWS && config.filesystem == FS_NTFS) {
            log_write(&log_ctx, LOG_INFO, "Creating UEFI:NTFS support partition for Windows NTFS installation");
            
            if (create_uefi_ntfs_partition(config.target_device) != 0) {
                print_colored("Warning: Failed to create UEFI:NTFS partition", "yellow");
                log_write(&log_ctx, LOG_WARNING, "Failed to create UEFI:NTFS partition");
            } else {
                snprintf(uefi_partition, sizeof(uefi_partition), "%s2", config.target_device);
                log_write(&log_ctx, LOG_SUCCESS, "UEFI:NTFS partition created: %s", uefi_partition);
                
                // Install UEFI:NTFS bootloader to aforementioned helper partition
                if (install_uefi_ntfs(uefi_partition, mounts.temp_directory) != 0) {
                    print_colored("Warning: Failed to install UEFI:NTFS support", "yellow");
                    log_write(&log_ctx, LOG_WARNING, "Failed to install UEFI:NTFS support");
                } else {
                    log_write(&log_ctx, LOG_SUCCESS, "UEFI:NTFS support installed successfully");
                }
            }
        }
    } else {
        // We're in partition mode, just use the existing partition
        log_section(&log_ctx, "PARTITION MODE");
        log_write(&log_ctx, LOG_INFO, "Using existing partition: %s", config.target_partition);
    }

    // Mount partition for writing
    if (mount_target(config.target_partition, mounts.target_mountpoint) != 0) {
        fprintf(stderr, "Error: Failed to mount target partition\n");
        log_write(&log_ctx, LOG_ERROR, "Failed to mount target partition: %s", config.target_partition);
        cleanup(&mounts, config.target);
        log_close(&log_ctx, 0);
        return 1;
    }
    
    log_write(&log_ctx, LOG_SUCCESS, "Target partition mounted successfully");

    // Check if we have free space on target. If not, stop the bastard
    if (check_free_space(mounts.source_mountpoint, mounts.target_mountpoint, 
                        config.target_partition) != 0) {
        log_write(&log_ctx, LOG_ERROR, "Insufficient space on target partition");
        cleanup(&mounts, config.target);
        log_close(&log_ctx, 0);
        return 1;
    }
    
    // Calculate and log space info
    unsigned long long source_size = get_directory_size(mounts.source_mountpoint);
    unsigned long long target_free = get_free_space(mounts.target_mountpoint);
    
    log_write(&log_ctx, LOG_SUCCESS, "Space check passed");
    log_write(&log_ctx, LOG_INFO, "Source size: %llu MB", source_size / (1024 * 1024));
    log_write(&log_ctx, LOG_INFO, "Target free space: %llu MB", target_free / (1024 * 1024));

    log_section(&log_ctx, "FILE COPY OPERATION");
    
    print_colored("Copying installation files...", "green");
    log_write(&log_ctx, LOG_STEP, "Starting file copy operation");
    log_write(&log_ctx, LOG_INFO, "Copying from: %s", mounts.source_mountpoint);
    log_write(&log_ctx, LOG_INFO, "Copying to: %s", mounts.target_mountpoint);
    
    // Copy all files from source to target
    if (copy_filesystem_files(mounts.source_mountpoint, mounts.target_mountpoint, 
                             config.verbose) != 0) {
        fprintf(stderr, "Error: Failed to copy files\n");
        log_write(&log_ctx, LOG_ERROR, "File copy operation failed");
        cleanup(&mounts, config.target);
        log_close(&log_ctx, 0);
        return 1;
    }
    
    log_write(&log_ctx, LOG_SUCCESS, "All files copied successfully");

    // Some windows-specific crap
    if (config.iso_type == ISO_WINDOWS) {
        log_section(&log_ctx, "BOOTLOADER INSTALLATION");
        
        log_write(&log_ctx, LOG_STEP, "Applying Windows-specific configurations");
        
        if (workaround_win7_uefi(mounts.source_mountpoint, mounts.target_mountpoint) != 0) {
            print_colored("Notice: Windows 7 UEFI workaround applied", "");
            log_write(&log_ctx, LOG_INFO, "Windows 7 UEFI workaround was necessary and applied");
        } else {
            log_write(&log_ctx, LOG_INFO, "Windows 7 UEFI workaround check completed");
        }

        // Install GRUB for BIOS boot support
        print_colored("Installing GRUB bootloader...", "green");
        log_write(&log_ctx, LOG_STEP, "Installing GRUB bootloader for Windows");
        
        if (install_grub(mounts.target_mountpoint, config.target_device) != 0) {
            fprintf(stderr, "Error: Failed to install GRUB\n");
            log_write(&log_ctx, LOG_ERROR, "Failed to install GRUB bootloader");
            cleanup(&mounts, config.target);
            log_close(&log_ctx, 0);
            return 1;
        }
        
        log_write(&log_ctx, LOG_SUCCESS, "GRUB bootloader installed successfully");

        // Create GRUB config for windows boot
        if (install_grub_config(mounts.target_mountpoint) != 0) {
            fprintf(stderr, "Error: Failed to install GRUB configuration\n");
            log_write(&log_ctx, LOG_ERROR, "Failed to install GRUB configuration");
            cleanup(&mounts, config.target);
            log_close(&log_ctx, 0);
            return 1;
        }
        
        log_write(&log_ctx, LOG_SUCCESS, "GRUB configuration installed successfully");
    }

    operation_success = 1;
    
    log_section(&log_ctx, "CLEANUP");
    log_write(&log_ctx, LOG_STEP, "Starting cleanup operations");
    
    print_colored("Installation complete!", "green");
    log_write(&log_ctx, LOG_SUCCESS, "USB installation completed successfully!");
    
    // Unmount everything and remove temp directories
    cleanup(&mounts, config.target);
    
    log_write(&log_ctx, LOG_SUCCESS, "Cleanup completed");

    print_colored("You may now safely remove the USB device", "green");

    // Write to log
    if (!config.no_log) {
        log_close(&log_ctx, operation_success);
    }

    return 0;
}
