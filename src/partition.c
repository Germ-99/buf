#include "../include/buf.h"

// Wipe all filesystem signatures off a device
int wipe_device(const char *device) {
    char command[MAX_PATH];
    char output[1024];
    
    print_colored("Wiping device signatures...", "green");
    log_write(g_log_ctx, LOG_STEP, "Wiping device signatures from: %s", device);
    
    snprintf(command, sizeof(command), "wipefs --all '%s' 2>/dev/null", device);
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: Failed to wipe device\n");
        log_write(g_log_ctx, LOG_ERROR, "wipefs command failed");
        return -1;
    }
    
    print_colored("Verifying device is clean...", "");
    log_write(g_log_ctx, LOG_INFO, "Verifying device is clean");
    
    snprintf(command, sizeof(command), "lsblk --pairs --output NAME,TYPE '%s' | grep -c 'TYPE=\"part\"'", device);
    if (run_command_with_output(command, output, sizeof(output)) == 0) {
        if (atoi(trim_whitespace(output)) != 0) {
            fprintf(stderr, "Error: Device still has partitions after wiping\n");
            fprintf(stderr, "       Device may be write-protected\n");
            log_write(g_log_ctx, LOG_ERROR, "Device still has partitions after wiping - may be write-protected");
            return -1;
        }
    }
    
    log_write(g_log_ctx, LOG_SUCCESS, "Device wiped successfully");
    return 0;
}

// Create MBR partition table on target device
int create_partition_table(const char *device) {
    char command[MAX_PATH];
    
    print_colored("Creating partition table...", "green");
    log_write(g_log_ctx, LOG_STEP, "Creating MSDOS partition table on: %s", device);
    
    snprintf(command, sizeof(command), "parted --script '%s' mklabel msdos 2>/dev/null", device);
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: Failed to create partition table\n");
        log_write(g_log_ctx, LOG_ERROR, "parted mklabel command failed");
        return -1;
    }
    
    log_write(g_log_ctx, LOG_SUCCESS, "Partition table created");
    return 0;
}

int create_partition(const char *device, const char *partition, 
                    FilesystemType fs_type, const char *label) {
    char command[MAX_PATH];
    const char *fs_name = (fs_type == FS_NTFS) ? "ntfs" : "fat32";
    char mkfs_cmd[256];
    
    print_colored("Creating partition...", "green");
    log_write(g_log_ctx, LOG_STEP, "Creating %s partition: %s", fs_name, partition);
    
    if (fs_type == FS_FAT) {
        // We're using FAT32, so use the entire disk (start at 4MiB for alignment, end at 100%)
        snprintf(command, sizeof(command), 
                "parted --script '%s' mkpart primary %s 4MiB 100%% 2>/dev/null", 
                device, fs_name);
    } else {
        // We're using NTFS, leave 2MB at the end for UEFI:NTFS partition (start at 4MiB, and at -2049 sectors)
        snprintf(command, sizeof(command), 
                "parted --script '%s' mkpart primary %s 4MiB -- -2049s 2>/dev/null", 
                device, fs_name);
    }
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: Failed to create partition\n");
        log_write(g_log_ctx, LOG_ERROR, "parted mkpart command failed");
        return -1;
    }
    
    log_write(g_log_ctx, LOG_SUCCESS, "Partition created");
    
    // Force kernel to re-read partition table
    make_system_realize_partition_changed(device);
    
    print_colored("Formatting partition...", "green");
    log_write(g_log_ctx, LOG_STEP, "Formatting partition as %s", fs_name);
    
    // Format partition based off the filesystem type
    if (fs_type == FS_FAT) {
        // Check what is available for formatting FAT32
        snprintf(mkfs_cmd, sizeof(mkfs_cmd), "which mkdosfs >/dev/null 2>&1");
        if (run_command(mkfs_cmd) == 0) {
            // Use mkdosfs if available
            snprintf(command, sizeof(command), "mkdosfs -F 32 '%s' 2>/dev/null", partition);
        } else {
            // Fall back to mkfs.vfat
            snprintf(command, sizeof(command), "mkfs.vfat -F 32 '%s' 2>/dev/null", partition);
        }
    } else {
        // Use quick format and set label, NTFS
        snprintf(command, sizeof(command), 
                "mkntfs --quick --label '%s' '%s' 2>/dev/null", 
                label, partition);
    }
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: Failed to format partition\n");
        log_write(g_log_ctx, LOG_ERROR, "Filesystem creation failed");
        return -1;
    }
    
    log_write(g_log_ctx, LOG_SUCCESS, "Partition formatted as %s", fs_name);
    return 0;
}

// Create a small UEFI:NTFS partition at the end of the device
// This 1MB FAT16 partition will let UEFI systems boot from NTFS partitions
// Only needed because UEFI firmware blows and cannot natively read NTFS
int create_uefi_ntfs_partition(const char *device) {
    char command[MAX_PATH];
    
    print_colored("Creating UEFI:NTFS support partition...", "");
    log_write(g_log_ctx, LOG_STEP, "Creating UEFI:NTFS partition on: %s", device);
    
    // Create the FAT16 partition at the end (last 2048 sectors = 1MB)
    snprintf(command, sizeof(command), 
            "parted --align none --script '%s' mkpart primary fat16 -- -2048s -1s 2>/dev/null", 
            device);
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Warning: Failed to create UEFI:NTFS partition\n");
        log_write(g_log_ctx, LOG_WARNING, "Failed to create UEFI:NTFS partition");
        return -1;
    }
    
    // Tell that damn kernel to detect the new partition!
    make_system_realize_partition_changed(device);
    
    log_write(g_log_ctx, LOG_SUCCESS, "UEFI:NTFS partition created");
    return 0;
}

// Install UEFT:NTFS bootloader image to the FAT16 partition
// Big ups to pbatard for making Rufus
int install_uefi_ntfs(const char *partition, const char *temp_dir) {
    char command[MAX_PATH];
    char image_path[MAX_PATH];
    
    print_colored("Installing UEFI:NTFS support...", "");
    log_write(g_log_ctx, LOG_STEP, "Downloading UEFI:NTFS image");
    
    snprintf(image_path, sizeof(image_path), "%s/uefi-ntfs.img", temp_dir);
    
    snprintf(command, sizeof(command), 
            "wget -q -O '%s' https://github.com/pbatard/rufus/raw/master/res/uefi/uefi-ntfs.img 2>/dev/null", 
            image_path);
    
    if (run_command(command) != 0) {
        print_colored("Warning: Failed to download UEFI:NTFS image", "yellow");
        log_write(g_log_ctx, LOG_WARNING, "Failed to download UEFI:NTFS image from GitHub");
        return -1;
    }
    
    log_write(g_log_ctx, LOG_SUCCESS, "UEFI:NTFS image downloaded");
    log_write(g_log_ctx, LOG_STEP, "Writing UEFI:NTFS image to partition: %s", partition);
    
    // write bootloader image directly to partition
    snprintf(command, sizeof(command), "dd if='%s' of='%s' bs=1M 2>/dev/null", image_path, partition);
    if (run_command(command) != 0) {
        fprintf(stderr, "Warning: Failed to write UEFI:NTFS image\n");
        log_write(g_log_ctx, LOG_WARNING, "Failed to write UEFI:NTFS image with dd");
        return -1;
    }
    
    log_write(g_log_ctx, LOG_SUCCESS, "UEFI:NTFS image written successfully");
    return 0;
}