#include "../include/buf.h"

int main(int argc, char *argv[]) {
    Config config = {0};
    MountPoints mounts = {0};
    char uefi_partition[MAX_PATH];

    if (!check_root_privileges()) {
        fprintf(stderr, "Error: buf must be run as sudo\n");
        return 1;
    }

    config.filesystem = FS_FAT;
    config.verbose = 0;
    config.iso_type = ISO_UNKNOWN;
    strncpy(config.label, DEFAULT_FS_LABEL, sizeof(config.label) - 1);

    if (parse_arguments(argc, argv, &config) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    print_colored("buf v" VERSION, "");
    print_colored("================================", "");

    if (check_dependencies() != 0) {
        fprintf(stderr, "Error: Required dependencies not found\n");
        return 1;
    }

    if (check_source_media(config.source) != 0) {
        return 1;
    }

    if (check_target_media(config.target, config.mode) != 0) {
        return 1;
    }

    if (determine_target_parameters(&config) != 0) {
        return 1;
    }

    if (is_device_busy(config.source)) {
        fprintf(stderr, "Error: Source media is currently in use\n");
        return 1;
    }

    if (config.mode == MODE_PARTITION) {
        if (is_device_busy(config.target_partition)) {
            print_colored("Target partition is mounted, unmounting...", "yellow");
            if (unmount_device(config.target_partition) != 0) {
                fprintf(stderr, "Error: Failed to unmount target partition\n");
                return 1;
            }
        }
    } else {
        if (is_device_busy(config.target_device)) {
            print_colored("Target device is mounted, unmounting...", "yellow");
            if (unmount_device(config.target_device) != 0) {
                fprintf(stderr, "Error: Failed to unmount target device\n");
                return 1;
            }
        }
    }

    if (create_mountpoints(&mounts) != 0) {
        fprintf(stderr, "Error: Failed to create mountpoints\n");
        return 1;
    }

    if (mount_source(config.source, mounts.source_mountpoint) != 0) {
        fprintf(stderr, "Error: Failed to mount source media\n");
        cleanup(&mounts, config.target);
        return 1;
    }

    config.iso_type = detect_iso_type(mounts.source_mountpoint);

    if (config.iso_type == ISO_WINDOWS) {
        if (check_fat32_limitation(mounts.source_mountpoint, &config.filesystem) != 0) {
            print_colored("Notice: Large files detected, switching to NTFS", "yellow");
            config.filesystem = FS_NTFS;
        }
    }

    if (config.mode == MODE_WIPE) {
        print_colored("Preparing target device...", "green");
        
        if (wipe_device(config.target_device) != 0) {
            fprintf(stderr, "Error: Failed to wipe device\n");
            cleanup(&mounts, config.target);
            return 1;
        }

        if (create_partition_table(config.target_device) != 0) {
            fprintf(stderr, "Error: Failed to create partition table\n");
            cleanup(&mounts, config.target);
            return 1;
        }

        if (create_partition(config.target_device, config.target_partition, 
                           config.filesystem, config.label) != 0) {
            fprintf(stderr, "Error: Failed to create partition\n");
            cleanup(&mounts, config.target);
            return 1;
        }

        if (config.iso_type == ISO_WINDOWS && config.filesystem == FS_NTFS) {
            if (create_uefi_ntfs_partition(config.target_device) != 0) {
                print_colored("Warning: Failed to create UEFI:NTFS partition", "yellow");
            } else {
                snprintf(uefi_partition, sizeof(uefi_partition), "%s2", config.target_device);
                if (install_uefi_ntfs(uefi_partition, mounts.temp_directory) != 0) {
                    print_colored("Warning: Failed to install UEFI:NTFS support", "yellow");
                }
            }
        }
    }

    if (mount_target(config.target_partition, mounts.target_mountpoint) != 0) {
        fprintf(stderr, "Error: Failed to mount target partition\n");
        cleanup(&mounts, config.target);
        return 1;
    }

    if (check_free_space(mounts.source_mountpoint, mounts.target_mountpoint, 
                        config.target_partition) != 0) {
        cleanup(&mounts, config.target);
        return 1;
    }

    print_colored("Copying installation files...", "green");
    
    if (copy_filesystem_files(mounts.source_mountpoint, mounts.target_mountpoint, 
                             config.verbose) != 0) {
        fprintf(stderr, "Error: Failed to copy files\n");
        cleanup(&mounts, config.target);
        return 1;
    }

    if (config.iso_type == ISO_WINDOWS) {
        if (workaround_win7_uefi(mounts.source_mountpoint, mounts.target_mountpoint) != 0) {
            print_colored("Notice: Windows 7 UEFI workaround applied", "");
        }

        print_colored("Installing GRUB bootloader...", "green");
        
        if (install_grub(mounts.target_mountpoint, config.target_device) != 0) {
            fprintf(stderr, "Error: Failed to install GRUB\n");
            cleanup(&mounts, config.target);
            return 1;
        }

        if (install_grub_config(mounts.target_mountpoint) != 0) {
            fprintf(stderr, "Error: Failed to install GRUB configuration\n");
            cleanup(&mounts, config.target);
            return 1;
        }
    }

    print_colored("Installation complete!", "green");
    
    cleanup(&mounts, config.target);

    print_colored("You may now safely remove the USB device", "green");

    return 0;
}