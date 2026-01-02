/*
    buf - Command line tool for flashing ISO images onto USB drives.
    Copyright (C) 2026  Bryson Kelly

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
    
*/


#ifndef BUF_H
#define BUF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <sys/statvfs.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>

#define VERSION "1.6.1"
#define APP_NAME "buf"
#define DEFAULT_FS_LABEL "BOOTABLE USB" // Default label buf gives the USB after flashing (can be changed via --label flag)
#define MAX_PATH 4096 // Max path for file operations
#define MAX_DEVICES 64 // Max number of devices that will be listed when using --list flag
#define FAT32_MAX_FILESIZE 4294967295ULL // FAT32 has a maximum file size of 4GB - 1 byte

typedef enum {
    MODE_NONE,
    MODE_WIPE,
    MODE_PARTITION
} InstallMode;

typedef enum {
    LOG_INFO,
    LOG_SUCCESS,
    LOG_WARNING,
    LOG_ERROR,
    LOG_STEP
} LogLevel;

typedef enum {
    FS_FAT,
    FS_NTFS
} FilesystemType;

typedef enum {
    ISO_UNKNOWN,
    ISO_WINDOWS,
    ISO_LINUX,
    ISO_OTHER
} ISOType;

typedef struct {
    InstallMode mode;
    char source[MAX_PATH];
    char target[MAX_PATH];
    char target_device[MAX_PATH];
    char target_partition[MAX_PATH];
    FilesystemType filesystem;
    char label[256];
    int verbose;
    int no_log;
    ISOType iso_type;
} Config;

typedef struct {
    char source_mountpoint[MAX_PATH];
    char target_mountpoint[MAX_PATH];
    char temp_directory[MAX_PATH];
} MountPoints;

typedef struct {
    FILE *file;
    char filepath[MAX_PATH];
    int enabled;
    time_t start_time;
    int error_count;
    int warning_count;
} LogContext;

int check_root_privileges(void);
int parse_arguments(int argc, char *argv[], Config *config);
void print_usage(const char *program_name);
void print_version(void);
void print_colored(const char *text, const char *color);
int list_removable_devices(void);

int check_dependencies(void);
int check_source_media(const char *source);
int check_target_media(const char *target, InstallMode mode);
int determine_target_parameters(Config *config);
ISOType detect_iso_type(const char *source_mountpoint);

int is_device_busy(const char *device);
int unmount_device(const char *device);

int create_mountpoints(MountPoints *mounts);
int mount_source(const char *source, const char *mountpoint);
int mount_target(const char *target, const char *mountpoint);

int wipe_device(const char *device);
int create_partition_table(const char *device);
int create_partition(const char *device, const char *partition, FilesystemType fs_type, const char *label);
int create_uefi_ntfs_partition(const char *device);
int install_uefi_ntfs(const char *partition, const char *temp_dir);

unsigned long long get_directory_size(const char *path);
unsigned long long get_free_space(const char *path);
int check_fat32_limitation(const char *source_mountpoint, FilesystemType *fs_type);
int check_free_space(const char *source_mountpoint, const char *target_mountpoint, const char *target_partition);

int copy_filesystem_files(const char *source, const char *target, int verbose);
int copy_file(const char *source, const char *target);
int copy_directory_recursive(const char *source, const char *target, int verbose);

int install_grub(const char *target_mountpoint, const char *target_device);
int install_grub_config(const char *target_mountpoint);
int workaround_win7_uefi(const char *source_mountpoint, const char *target_mountpoint);

int cleanup_mountpoint(const char *mountpoint);
void cleanup(MountPoints *mounts, const char *target_media);

int run_command(const char *command);
int run_command_with_output(const char *command, char *output, size_t output_size);
char *trim_whitespace(char *str);
int file_exists(const char *path);
int is_block_device(const char *path);
int is_directory(const char *path);
int make_directory(const char *path);
void make_system_realize_partition_changed(const char *device);

int log_init(LogContext *ctx, const char *home_dir);
void log_close(LogContext *ctx, int success);
void log_write(LogContext *ctx, LogLevel level, const char *format, ...);
void log_section(LogContext *ctx, const char *section_name);
void log_system_info(LogContext *ctx);
void log_config(LogContext *ctx, Config *config);
void log_command(LogContext *ctx, const char *command, int result);
void log_command_invocation(LogContext *ctx, int argc, char *argv[]);

extern LogContext *g_log_ctx;

#endif
