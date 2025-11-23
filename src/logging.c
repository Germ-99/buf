#include "../include/buf.h"
#include <stdarg.h>

LogContext *g_log_ctx = NULL;

static const char *get_level_string(LogLevel level) {
    switch (level) {
        case LOG_INFO:    return "[INFO]   ";
        case LOG_SUCCESS: return "[SUCCESS]";
        case LOG_WARNING: return "[WARNING]";
        case LOG_ERROR:   return "[ERROR]  ";
        case LOG_STEP:    return "[STEP]   ";
        default:          return "[UNKNOWN]";
    }
}

static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

static const char *get_home_directory(void) {
    const char *home;
    const char *sudo_user;
    struct passwd *pw;
    
    sudo_user = getenv("SUDO_USER");
    if (sudo_user != NULL) {
        pw = getpwnam(sudo_user);
        if (pw != NULL) {
            return pw->pw_dir;
        }
    }
    
    home = getenv("HOME");
    if (home != NULL && strcmp(home, "/root") != 0) {
        return home;
    }
    
    pw = getpwuid(getuid());
    if (pw != NULL && strcmp(pw->pw_dir, "/root") != 0) {
        return pw->pw_dir;
    }
    
    return "/tmp";
}

int log_init(LogContext *ctx, const char *home_dir) {
    time_t now;
    struct tm *tm_info;
    char log_dir[MAX_PATH];
    char filename[256];
    
    if (ctx == NULL) {
        return -1;
    }
    
    ctx->enabled = 1;
    ctx->error_count = 0;
    ctx->warning_count = 0;
    ctx->start_time = time(NULL);
    
    if (home_dir == NULL) {
        home_dir = get_home_directory();
    }
    
    now = time(NULL);
    tm_info = localtime(&now);
    
    snprintf(filename, sizeof(filename), "buf-%02d-%02d-%02d.log",
             tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_year % 100);
    
    snprintf(log_dir, sizeof(log_dir), "%s", home_dir);
    
    snprintf(ctx->filepath, sizeof(ctx->filepath), "%s/%s", log_dir, filename);
    
    ctx->file = fopen(ctx->filepath, "w");
    if (ctx->file == NULL) {
        fprintf(stderr, "Warning: Could not create log file at %s\n", ctx->filepath);
        ctx->enabled = 0;
        return -1;
    }
    
    fprintf(ctx->file, "================================================================================\n");
    fprintf(ctx->file, "                          buf - Bootable USB Flasher                           \n");
    fprintf(ctx->file, "                                Version %s                                     \n", VERSION);
    fprintf(ctx->file, "================================================================================\n\n");
    
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(ctx->file, "Log started: %s\n\n", timestamp);
    
    fflush(ctx->file);
    
    return 0;
}

void log_close(LogContext *ctx, int success) {
    char timestamp[64];
    time_t end_time;
    int duration;
    
    if (ctx == NULL || !ctx->enabled || ctx->file == NULL) {
        return;
    }
    
    fprintf(ctx->file, "\n");
    fprintf(ctx->file, "================================================================================\n");
    fprintf(ctx->file, "                                        SUMMARY                                 \n");
    fprintf(ctx->file, "================================================================================\n\n");
    
    fprintf(ctx->file, "Final Status: %s\n\n", success ? "SUCCESS" : "FAILED");
    
    end_time = time(NULL);
    duration = (int)difftime(end_time, ctx->start_time);
    
    fprintf(ctx->file, "Statistics:\n");
    fprintf(ctx->file, "  - Total Errors:   %d\n", ctx->error_count);
    fprintf(ctx->file, "  - Total Warnings: %d\n", ctx->warning_count);
    fprintf(ctx->file, "  - Duration:       %d seconds (%d minutes, %d seconds)\n\n",
            duration, duration / 60, duration % 60);
    
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(ctx->file, "Log ended: %s\n\n", timestamp);
    
    if (!success && ctx->error_count > 0) {
        fprintf(ctx->file, "FAILURE REASON:\n");
        fprintf(ctx->file, "  The operation failed with %d error(s). Please review the error messages\n", ctx->error_count);
        fprintf(ctx->file, "  above for specific details about what went wrong.\n\n");
    }
    
    fprintf(ctx->file, "================================================================================\n");
    fprintf(ctx->file, "                                 END OF LOG                                     \n");
    fprintf(ctx->file, "================================================================================\n");
    
    fclose(ctx->file);
    
    if (success) {
        printf("\nLog file saved: %s\n", ctx->filepath);
    } else {
        fprintf(stderr, "\nLog file saved: %s\n", ctx->filepath);
    }
    
    ctx->enabled = 0;
}

void log_write(LogContext *ctx, LogLevel level, const char *format, ...) {
    va_list args;
    char timestamp[64];
    
    if (ctx == NULL || !ctx->enabled || ctx->file == NULL) {
        return;
    }
    
    if (level == LOG_ERROR) {
        ctx->error_count++;
    } else if (level == LOG_WARNING) {
        ctx->warning_count++;
    }
    
    get_timestamp(timestamp, sizeof(timestamp));
    
    fprintf(ctx->file, "%s %s ", timestamp, get_level_string(level));
    
    va_start(args, format);
    vfprintf(ctx->file, format, args);
    va_end(args);
    
    fprintf(ctx->file, "\n");
    fflush(ctx->file);
}

void log_section(LogContext *ctx, const char *section_name) {
    if (ctx == NULL || !ctx->enabled || ctx->file == NULL) {
        return;
    }
    
    fprintf(ctx->file, "\n");
    fprintf(ctx->file, "--------------------------------------------------------------------------------\n");
    fprintf(ctx->file, " %s\n", section_name);
    fprintf(ctx->file, "--------------------------------------------------------------------------------\n\n");
    fflush(ctx->file);
}

void log_system_info(LogContext *ctx) {
    char buffer[1024];
    FILE *pipe;
    
    if (ctx == NULL || !ctx->enabled || ctx->file == NULL) {
        return;
    }
    
    log_section(ctx, "SYSTEM INFORMATION");
    
    pipe = popen("uname -a 2>/dev/null", "r");
    if (pipe != NULL) {
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            log_write(ctx, LOG_INFO, "Kernel: %s", buffer);
        }
        pclose(pipe);
    }
    
    pipe = popen("cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d'=' -f2 | tr -d '\"'", "r");
    if (pipe != NULL) {
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            log_write(ctx, LOG_INFO, "Distribution: %s", buffer);
        }
        pclose(pipe);
    }
    
    pipe = popen("whoami 2>/dev/null", "r");
    if (pipe != NULL) {
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            log_write(ctx, LOG_INFO, "User: %s", buffer);
        }
        pclose(pipe);
    }
    
    fprintf(ctx->file, "\n");
    fflush(ctx->file);
}

void log_config(LogContext *ctx, Config *config) {
    const char *mode_str;
    const char *fs_str;
    const char *iso_str;
    
    if (ctx == NULL || !ctx->enabled || ctx->file == NULL || config == NULL) {
        return;
    }
    
    log_section(ctx, "CONFIGURATION");
    
    switch (config->mode) {
        case MODE_WIPE:      mode_str = "Wipe Mode (Full Device)"; break;
        case MODE_PARTITION: mode_str = "Partition Mode"; break;
        default:             mode_str = "Unknown"; break;
    }
    
    switch (config->filesystem) {
        case FS_FAT:  fs_str = "FAT32"; break;
        case FS_NTFS: fs_str = "NTFS"; break;
        default:      fs_str = "Unknown"; break;
    }
    
    switch (config->iso_type) {
        case ISO_WINDOWS: iso_str = "Windows"; break;
        case ISO_LINUX:   iso_str = "Linux"; break;
        case ISO_OTHER:   iso_str = "Other"; break;
        default:          iso_str = "Unknown"; break;
    }
    
    log_write(ctx, LOG_INFO, "Installation Mode: %s", mode_str);
    log_write(ctx, LOG_INFO, "Source Media: %s", config->source);
    log_write(ctx, LOG_INFO, "Target Device: %s", config->target);
    
    if (config->mode == MODE_WIPE) {
        log_write(ctx, LOG_INFO, "Target Partition (will be created): %s", config->target_partition);
    } else {
        log_write(ctx, LOG_INFO, "Target Partition (existing): %s", config->target_partition);
    }
    
    log_write(ctx, LOG_INFO, "Filesystem Type: %s", fs_str);
    log_write(ctx, LOG_INFO, "Filesystem Label: %s", config->label);
    log_write(ctx, LOG_INFO, "ISO Type: %s", iso_str);
    log_write(ctx, LOG_INFO, "Verbose Mode: %s", config->verbose ? "Enabled" : "Disabled");
    
    fprintf(ctx->file, "\n");
    fflush(ctx->file);
}

void log_command(LogContext *ctx, const char *command, int result) {
    if (ctx == NULL || !ctx->enabled || ctx->file == NULL) {
        return;
    }
    
    if (result == 0) {
        log_write(ctx, LOG_INFO, "Command succeeded: %s", command);
    } else {
        log_write(ctx, LOG_ERROR, "Command failed (exit code %d): %s", result, command);
    }
}