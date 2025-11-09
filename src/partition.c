#include "../include/buf.h"

int wipe_device(const char *device) {
    char command[MAX_PATH];
    char output[1024];
    
    print_colored("Wiping device signatures...", "green");
    
    snprintf(command, sizeof(command), "wipefs --all '%s' 2>/dev/null", device);
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: Failed to wipe device\n");
        return -1;
    }
    
    print_colored("Verifying device is clean...", "");
    
    snprintf(command, sizeof(command), "lsblk --pairs --output NAME,TYPE '%s' | grep -c 'TYPE=\"part\"'", device);
    if (run_command_with_output(command, output, sizeof(output)) == 0) {
        if (atoi(trim_whitespace(output)) != 0) {
            fprintf(stderr, "Error: Device still has partitions after wiping\n");
            fprintf(stderr, "       Device may be write-protected\n");
            return -1;
        }
    }
    
    return 0;
}

int create_partition_table(const char *device) {
    char command[MAX_PATH];
    
    print_colored("Creating partition table...", "green");
    
    snprintf(command, sizeof(command), "parted --script '%s' mklabel msdos 2>/dev/null", device);
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: Failed to create partition table\n");
        return -1;
    }
    
    return 0;
}

int create_partition(const char *device, const char *partition, 
                    FilesystemType fs_type, const char *label) {
    char command[MAX_PATH];
    const char *fs_name = (fs_type == FS_NTFS) ? "ntfs" : "fat32";
    char mkfs_cmd[256];
    
    print_colored("Creating partition...", "green");
    
    if (fs_type == FS_FAT) {
        snprintf(command, sizeof(command), 
                "parted --script '%s' mkpart primary %s 4MiB 100%% 2>/dev/null", 
                device, fs_name);
    } else {
        snprintf(command, sizeof(command), 
                "parted --script '%s' mkpart primary %s 4MiB -- -2049s 2>/dev/null", 
                device, fs_name);
    }
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: Failed to create partition\n");
        return -1;
    }
    
    make_system_realize_partition_changed(device);
    
    print_colored("Formatting partition...", "green");
    
    if (fs_type == FS_FAT) {
        snprintf(mkfs_cmd, sizeof(mkfs_cmd), "which mkdosfs >/dev/null 2>&1");
        if (run_command(mkfs_cmd) == 0) {
            snprintf(command, sizeof(command), "mkdosfs -F 32 '%s' 2>/dev/null", partition);
        } else {
            snprintf(command, sizeof(command), "mkfs.vfat -F 32 '%s' 2>/dev/null", partition);
        }
    } else {
        snprintf(command, sizeof(command), 
                "mkntfs --quick --label '%s' '%s' 2>/dev/null", 
                label, partition);
    }
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: Failed to format partition\n");
        return -1;
    }
    
    return 0;
}

int create_uefi_ntfs_partition(const char *device) {
    char command[MAX_PATH];
    
    print_colored("Creating UEFI:NTFS support partition...", "");
    
    snprintf(command, sizeof(command), 
            "parted --align none --script '%s' mkpart primary fat16 -- -2048s -1s 2>/dev/null", 
            device);
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Warning: Failed to create UEFI:NTFS partition\n");
        return -1;
    }
    
    make_system_realize_partition_changed(device);
    
    return 0;
}

int install_uefi_ntfs(const char *partition, const char *temp_dir) {
    char command[MAX_PATH];
    char image_path[MAX_PATH];
    
    print_colored("Installing UEFI:NTFS support...", "");
    
    snprintf(image_path, sizeof(image_path), "%s/uefi-ntfs.img", temp_dir);
    
    snprintf(command, sizeof(command), 
            "wget -q -O '%s' https://github.com/pbatard/rufus/raw/master/res/uefi/uefi-ntfs.img 2>/dev/null", 
            image_path);
    
    if (run_command(command) != 0) {
        print_colored("Warning: Failed to download UEFI:NTFS image", "yellow");
        return -1;
    }
    
    snprintf(command, sizeof(command), "dd if='%s' of='%s' bs=1M 2>/dev/null", image_path, partition);
    if (run_command(command) != 0) {
        fprintf(stderr, "Warning: Failed to write UEFI:NTFS image\n");
        return -1;
    }
    
    return 0;
}