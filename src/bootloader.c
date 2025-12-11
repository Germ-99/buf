#include "../include/buf.h"

// This entire file is windows-specific crap
// Using GRUB, just incase a user is on a BIOS-based system
// By the way, if your computer is still running BIOS, why? 
int install_grub(const char *target_mountpoint, const char *target_device) {
    char command[MAX_PATH];
    char grub_cmd[256];
    
    log_write(g_log_ctx, LOG_STEP, "Installing GRUB to: %s", target_device);
    
    snprintf(grub_cmd, sizeof(grub_cmd), "which grub-install >/dev/null 2>&1");
    if (run_command(grub_cmd) == 0) {
        // Use grub-install if available
        snprintf(command, sizeof(command), 
                "grub-install --target=i386-pc --boot-directory='%s' --force '%s' 2>/dev/null", 
                target_mountpoint, target_device);
        log_write(g_log_ctx, LOG_INFO, "Using grub-install command");
    } else {
        // Fall back to grub2 if grub-install fails
        snprintf(command, sizeof(command), 
                "grub2-install --target=i386-pc --boot-directory='%s' --force '%s' 2>/dev/null", 
                target_mountpoint, target_device);
        log_write(g_log_ctx, LOG_INFO, "Using grub2-install command");
    }
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Error: GRUB installation failed\n");
        log_write(g_log_ctx, LOG_ERROR, "GRUB installation command failed");
        return -1;
    }
    
    log_write(g_log_ctx, LOG_SUCCESS, "GRUB installed successfully");
    return 0;
}

// Create a very basic GRUB config that chains to windows bootmgr
int install_grub_config(const char *target_mountpoint) {
    char grub_cfg_path[MAX_PATH];
    char grub_dir[MAX_PATH];
    FILE *cfg_file;
    char test_cmd[MAX_PATH];
    
    log_write(g_log_ctx, LOG_STEP, "Creating GRUB configuration");
    
    // Determine the GRUB dir name (grub vs grub2)
    snprintf(test_cmd, sizeof(test_cmd), "which grub-install >/dev/null 2>&1");
    if (run_command(test_cmd) == 0) {
        snprintf(grub_dir, sizeof(grub_dir), "%s/grub", target_mountpoint);
        log_write(g_log_ctx, LOG_INFO, "Using grub directory");
    } else {
        snprintf(grub_dir, sizeof(grub_dir), "%s/grub2", target_mountpoint);
        log_write(g_log_ctx, LOG_INFO, "Using grub2 directory");
    }
    
    if (make_directory(grub_dir) != 0) {
        fprintf(stderr, "Error: Failed to create GRUB directory\n");
        log_write(g_log_ctx, LOG_ERROR, "Failed to create GRUB directory: %s", grub_dir);
        return -1;
    }
    
    snprintf(grub_cfg_path, sizeof(grub_cfg_path), "%s/grub.cfg", grub_dir);
    log_write(g_log_ctx, LOG_INFO, "Creating GRUB config at: %s", grub_cfg_path);
    
    // Create and write the GRUB config
    cfg_file = fopen(grub_cfg_path, "w");
    if (cfg_file == NULL) {
        fprintf(stderr, "Error: Failed to create GRUB config file\n");
        log_write(g_log_ctx, LOG_ERROR, "Failed to create GRUB config file: %s", grub_cfg_path);
        return -1;
    }
    
    fprintf(cfg_file, "ntldr /bootmgr\n"); // Load bootmgr as NTLDR
    fprintf(cfg_file, "boot\n"); // Boot it
    
    fclose(cfg_file);
    
    log_write(g_log_ctx, LOG_SUCCESS, "GRUB configuration created");
    return 0;
}

// Windows 7 ISOs lack a proper UEFI bootloader, so we extract it from install.wim
// This lets windows 7 boot on UEFI systems
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
    
    log_write(g_log_ctx, LOG_INFO, "Checking for Windows 7 UEFI workaround requirement");
    
    // Check if this is windows 7 by looking at cversion.ini
    snprintf(cversion_path, sizeof(cversion_path), "%s/sources/cversion.ini", source_mountpoint);
    if (file_exists(cversion_path)) {
        // Check for windows 7 version string (7xxx.x format)
        snprintf(command, sizeof(command), "grep -E '^MinServer=7[0-9]{3}\\.[0-9]' '%s'", cversion_path);
        if (run_command(command) == 0) {
            is_win7 = 1;
            log_write(g_log_ctx, LOG_INFO, "Detected Windows 7 installation media");
        }
    }
    
    // If bootmgr.efi doesn't exist, it's another indicator of windows 7
    snprintf(command, sizeof(command), "%s/bootmgr.efi", source_mountpoint);
    if (!file_exists(command) && !is_win7) {
        log_write(g_log_ctx, LOG_INFO, "Windows 7 UEFI workaround not needed");
        return 0;
    }
    
    print_colored("Applying Windows 7 UEFI workaround...", "");
    log_write(g_log_ctx, LOG_STEP, "Applying Windows 7 UEFI workaround");
    
    // Find EFI directory (case-insensitive)
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
    
    log_write(g_log_ctx, LOG_INFO, "EFI directory: %s", efi_dir);
    
    // Find EFI boot directory (case-insensitive)
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
    
    log_write(g_log_ctx, LOG_INFO, "EFI boot directory: %s", efi_boot_dir);
    
    // Check if EFI bootloader already exists. If so, skip this workaround
    snprintf(command, sizeof(command), "find '%s' -ipath '%s/efi/boot/boot*.efi' 2>/dev/null", 
            target_mountpoint, target_mountpoint);
    
    pipe = popen(command, "r");
    if (pipe != NULL) {
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            pclose(pipe);
            print_colored("Existing EFI bootloader found, skipping workaround", "");
            log_write(g_log_ctx, LOG_INFO, "Existing EFI bootloader found, skipping workaround");
            return 0;
        }
        pclose(pipe);
    }
    
    // Create EFI boot directory
    if (make_directory(efi_boot_dir) != 0) {
        fprintf(stderr, "Warning: Failed to create EFI boot directory\n");
        log_write(g_log_ctx, LOG_WARNING, "Failed to create EFI boot directory: %s", efi_boot_dir);
        return -1;
    }
    
    log_write(g_log_ctx, LOG_INFO, "Created EFI boot directory");
    
    snprintf(bootloader_path, sizeof(bootloader_path), "%s/bootx64.efi", efi_boot_dir);
    snprintf(sources_install, sizeof(sources_install), "%s/sources/install.wim", source_mountpoint);
    
    log_write(g_log_ctx, LOG_STEP, "Extracting EFI bootloader from install.wim");
    
    // Extract bootmgfw.efi from install.wim and rename to bootx64.efi
    snprintf(command, sizeof(command), 
            "7z e -so '%s' Windows/Boot/EFI/bootmgfw.efi > '%s' 2>/dev/null", 
            sources_install, bootloader_path);
    
    if (run_command(command) != 0) {
        fprintf(stderr, "Warning: Failed to extract EFI bootloader\n");
        log_write(g_log_ctx, LOG_WARNING, "Failed to extract EFI bootloader from install.wim");
        return -1;
    }
    
    log_write(g_log_ctx, LOG_SUCCESS, "EFI bootloader extracted successfully: %s", bootloader_path);
    return 0;
}