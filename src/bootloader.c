#include "../include/buf.h"

int install_grub(const char *target_mountpoint, const char *target_device) {
    char command[MAX_PATH];
    char grub_cmd[256];
    
    snprintf(grub_cmd, sizeof(grub_cmd), "which grub-install >/dev/null 2>&1");
    if (run_command(grub_cmd) == 0) {
        snprintf(command, sizeof(command), 
                "grub-install --target=i386-pc --boot-directory='%s' --force '%s' 2>/dev/null", 
                target_mountpoint, target_device);
    } else {
        snprintf(command, sizeof(command), 
                "grub2-install --target=i386-pc --boot-directory='%s' --force '%s' 2>/dev/null", 
                target_mountpoint, target_device);
    }
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: GRUB installation failed\n");
        return -1;
    }
    
    return 0;
}

int install_grub_config(const char *target_mountpoint) {
    char grub_cfg_path[MAX_PATH];
    char grub_dir[MAX_PATH];
    FILE *cfg_file;
    char test_cmd[MAX_PATH];
    
    snprintf(test_cmd, sizeof(test_cmd), "which grub-install >/dev/null 2>&1");
    if (run_command(test_cmd) == 0) {
        snprintf(grub_dir, sizeof(grub_dir), "%s/grub", target_mountpoint);
    } else {
        snprintf(grub_dir, sizeof(grub_dir), "%s/grub2", target_mountpoint);
    }
    
    if (make_directory(grub_dir) != 0) {
        fprintf(stderr, "Error: Failed to create GRUB directory\n");
        return -1;
    }
    
    snprintf(grub_cfg_path, sizeof(grub_cfg_path), "%s/grub.cfg", grub_dir);
    
    cfg_file = fopen(grub_cfg_path, "w");
    if (cfg_file == NULL) {
        fprintf(stderr, "Error: Failed to create GRUB config file\n");
        return -1;
    }
    
    fprintf(cfg_file, "ntldr /bootmgr\n");
    fprintf(cfg_file, "boot\n");
    
    fclose(cfg_file);
    
    return 0;
}

int workaround_win7_uefi(const char *source_mountpoint, const char *target_mountpoint) {
    char command[MAX_PATH];
    char cversion_path[MAX_PATH];
    char efi_dir[MAX_PATH];
    char efi_boot_dir[MAX_PATH];
    char bootloader_path[MAX_PATH];
    char sources_install[MAX_PATH];
    FILE *pipe;
    char buffer[1024];
    int is_win7 = 0;
    
    snprintf(cversion_path, sizeof(cversion_path), "%s/sources/cversion.ini", source_mountpoint);
    if (file_exists(cversion_path)) {
        snprintf(command, sizeof(command), "grep -E '^MinServer=7[0-9]{3}\\.[0-9]' '%s'", cversion_path);
        if (run_command(command) == 0) {
            is_win7 = 1;
        }
    }
    
    snprintf(command, sizeof(command), "%s/bootmgr.efi", source_mountpoint);
    if (!file_exists(command) && !is_win7) {
        return 0;
    }
    
    print_colored("Applying Windows 7 UEFI workaround...", "");
    
    snprintf(command, sizeof(command), "find '%s' -ipath '%s/efi' 2>/dev/null", 
            target_mountpoint, target_mountpoint);
    
    pipe = popen(command, "r");
    if (pipe != NULL) {
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            strncpy(efi_dir, buffer, sizeof(efi_dir) - 1);
        } else {
            snprintf(efi_dir, sizeof(efi_dir), "%s/efi", target_mountpoint);
        }
        pclose(pipe);
    } else {
        snprintf(efi_dir, sizeof(efi_dir), "%s/efi", target_mountpoint);
    }
    
    snprintf(command, sizeof(command), "find '%s' -ipath '%s/boot' 2>/dev/null", 
            target_mountpoint, target_mountpoint);
    
    pipe = popen(command, "r");
    if (pipe != NULL) {
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            strncpy(efi_boot_dir, buffer, sizeof(efi_boot_dir) - 1);
        } else {
            snprintf(efi_boot_dir, sizeof(efi_boot_dir), "%s/efi/boot", target_mountpoint);
        }
        pclose(pipe);
    } else {
        snprintf(efi_boot_dir, sizeof(efi_boot_dir), "%s/efi/boot", target_mountpoint);
    }
    
    snprintf(command, sizeof(command), "find '%s' -ipath '%s/efi/boot/boot*.efi' 2>/dev/null", 
            target_mountpoint, target_mountpoint);
    
    pipe = popen(command, "r");
    if (pipe != NULL) {
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            pclose(pipe);
            print_colored("Existing EFI bootloader found, skipping workaround", "");
            return 0;
        }
        pclose(pipe);
    }
    
    if (make_directory(efi_boot_dir) != 0) {
        fprintf(stderr, "Warning: Failed to create EFI boot directory\n");
        return -1;
    }
    
    snprintf(bootloader_path, sizeof(bootloader_path), "%s/bootx64.efi", efi_boot_dir);
    snprintf(sources_install, sizeof(sources_install), "%s/sources/install.wim", source_mountpoint);
    
    snprintf(command, sizeof(command), 
            "7z e -so '%s' Windows/Boot/EFI/bootmgfw.efi > '%s' 2>/dev/null", 
            sources_install, bootloader_path);
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Warning: Failed to extract EFI bootloader\n");
        return -1;
    }
    
    return 0;
}