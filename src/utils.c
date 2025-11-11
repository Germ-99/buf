#include "../include/buf.h"

int check_root_privileges(void) {
    return (geteuid() == 0);
}

void print_colored(const char *text, const char *color) {
    if (strcmp(color, "red") == 0) {
        fprintf(stderr, "\033[31m%s\033[0m\n", text);
    } else if (strcmp(color, "green") == 0) {
        printf("\033[32m%s\033[0m\n", text);
    } else if (strcmp(color, "yellow") == 0) {
        printf("\033[33m%s\033[0m\n", text);
    } else {
        printf("%s\n", text);
    }
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("Create a bootable USB installer from an ISO image\n\n");
    printf("Required options:\n");
    printf("  -s, --source=PATH          Source ISO file or DVD device\n");
    printf("  -t, --target=PATH          Target USB device or partition\n");
    printf("  -w, --wipe                 Wipe mode (wipe entire USB)\n");
    printf("  -p, --partition            Partition mode (use existing partition)\n\n");
    printf("Optional:\n");
    printf("  -l, --label=LABEL          Filesystem label (default: 'BOOTABLE USB')\n");
    printf("  -v, --verbose              Verbose output\n");
    printf("  -ls, --list                List all removable drives\n");
    printf("  --version                  Show version information\n");
    printf("  -h, --help                 Show this help message\n\n");
    printf("Examples:\n");
    printf("  sudo %s -w -s=/path/to/image.iso -t=/dev/sdb\n", program_name);
    printf("  sudo %s -p -s=/path/to/windows.iso -t=/dev/sdb1\n", program_name);
}

void print_version(void) {
    printf("Version %s\n", VERSION);
    printf("Software written by Bryson Kelly\n");
    printf("Source code available at: github.com/Germ-99/buf\n");
}

char *trim_whitespace(char *str) {
    char *end;
    
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0) return str;
    
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    *(end + 1) = '\0';
    
    return str;
}

int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}

int is_block_device(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISBLK(st.st_mode);
}

int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

int make_directory(const char *path) {
    char tmp[MAX_PATH];
    char *p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    
    return 0;
}

int run_command(const char *command) {
    int ret = system(command);
    return WEXITSTATUS(ret);
}

int run_command_with_output(const char *command, char *output, size_t output_size) {
    FILE *fp;
    size_t bytes_read;
    
    fp = popen(command, "r");
    if (fp == NULL) {
        return -1;
    }
    
    bytes_read = fread(output, 1, output_size - 1, fp);
    output[bytes_read] = '\0';
    
    pclose(fp);
    return 0;
}

unsigned long long get_directory_size(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    unsigned long long total = 0;
    char full_path[MAX_PATH];
    
    dir = opendir(path);
    if (dir == NULL) {
        return 0;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        if (lstat(full_path, &st) != 0) {
            continue;
        }
        
        if (S_ISDIR(st.st_mode)) {
            total += get_directory_size(full_path);
        } else if (S_ISREG(st.st_mode)) {
            total += st.st_size;
        }
    }
    
    closedir(dir);
    return total;
}

unsigned long long get_free_space(const char *path) {
    struct statvfs st;
    
    if (statvfs(path, &st) != 0) {
        return 0;
    }
    
    return (unsigned long long)st.f_bavail * st.f_bsize;
}

void make_system_realize_partition_changed(const char *device) {
    char command[MAX_PATH];
    
    print_colored("Refreshing partition table...", "");
    
    snprintf(command, sizeof(command), "blockdev --rereadpt %s 2>/dev/null", device);
    run_command(command);
    
    sleep(3);
}

int list_removable_devices(void) {
    FILE *pipe;
    char buffer[1024];
    char command[MAX_PATH];
    char **devices = NULL;
    int device_count = 0;
    int i;
    char device_path[MAX_PATH];
    char size[64];
    char model[256];
    char type[64];
    char cmd[MAX_PATH];
    
    printf("\033[1mRemovable Devices:\033[0m\n");
    printf("%-15s %-20s %-10s %-10s\n", "DEVICE", "MODEL", "SIZE", "TYPE");
    printf("================================================================\n");
    
    snprintf(command, sizeof(command), 
             "lsblk -d -o NAME,TRAN,TYPE -n | awk '($2==\"usb\" || $2==\"uas\") && $3==\"disk\" {print $1}'");
    
    pipe = popen(command, "r");
    if (pipe == NULL) {
        fprintf(stderr, "Error: Failed to list devices\n");
        return -1;
    }
    
    devices = (char **)malloc(MAX_DEVICES * sizeof(char *));
    if (devices == NULL) {
        pclose(pipe);
        fprintf(stderr, "Error: Memory allocation failed\n");
        return -1;
    }
    
    while (fgets(buffer, sizeof(buffer), pipe) != NULL && device_count < MAX_DEVICES) {
        buffer[strcspn(buffer, "\n")] = 0;
        devices[device_count] = strdup(buffer);
        if (devices[device_count] == NULL) {
            pclose(pipe);
            for (i = 0; i < device_count; i++) {
                free(devices[i]);
            }
            free(devices);
            fprintf(stderr, "Error: Memory allocation failed\n");
            return -1;
        }
        device_count++;
    }
    
    pclose(pipe);
    
    if (device_count == 0) {
        printf("No removable devices found.\n");
        free(devices);
        return 0;
    }
    
    for (i = 0; i < device_count; i++) {
        size[0] = '\0';
        model[0] = '\0';
        type[0] = '\0';
        
        snprintf(device_path, sizeof(device_path), "/dev/%s", devices[i]);
        
        snprintf(cmd, sizeof(cmd), 
                 "lsblk -d -o SIZE -n '%s' 2>/dev/null", device_path);
        pipe = popen(cmd, "r");
        if (pipe != NULL) {
            if (fgets(size, sizeof(size), pipe) != NULL) {
                size[strcspn(size, "\n")] = 0;
                char *trimmed = trim_whitespace(size);
                memmove(size, trimmed, strlen(trimmed) + 1);
            }
            pclose(pipe);
        }
        
        snprintf(cmd, sizeof(cmd), 
                 "lsblk -d -o MODEL -n '%s' 2>/dev/null", device_path);
        pipe = popen(cmd, "r");
        if (pipe != NULL) {
            if (fgets(model, sizeof(model), pipe) != NULL) {
                model[strcspn(model, "\n")] = 0;
                char *trimmed = trim_whitespace(model);
                memmove(model, trimmed, strlen(trimmed) + 1);
            }
            pclose(pipe);
        }
        
        snprintf(cmd, sizeof(cmd), 
                 "lsblk -d -o TRAN -n '%s' 2>/dev/null", device_path);
        pipe = popen(cmd, "r");
        if (pipe != NULL) {
            if (fgets(type, sizeof(type), pipe) != NULL) {
                type[strcspn(type, "\n")] = 0;
                trim_whitespace(type);
            }
            pclose(pipe);
        }
        
        if (strlen(model) == 0) {
            strncpy(model, "Unknown", sizeof(model) - 1);
        }
        
        if (strlen(type) == 0) {
            strncpy(type, "usb", sizeof(type) - 1);
        }
        
        printf("%-15s %-20s %-10s %-10s\n", device_path, model, size, type);
        
        free(devices[i]);
    }
    
    free(devices);
    
    printf("\n\033[33mNote: Run 'sudo buf -h' for help with creating a bootable USB\033[0m\n");
    
    return 0;
}