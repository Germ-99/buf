#include "../include/buf.h"

int parse_arguments(int argc, char *argv[], Config *config) {
    int i;
    int has_mode = 0;   // Track if installation mode was specified 
    int mode_count = 0; // Count number of modes specified (if this is over 1, we have an issue)
    int has_source = 0; // Track if source was specified
    int has_target = 0; // Track if target was specified
    
    if (argc < 2) {
        return -1;
    }
    
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];
        char *value = NULL;
        
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
        
        if (strcmp(arg, "--version") == 0) {
            print_version();
            exit(0);
        }
        
        if (strcmp(arg, "-ls") == 0 || strcmp(arg, "--list") == 0) {
            list_removable_devices();
            exit(0);
        }
        
        if (strcmp(arg, "-w") == 0 || strcmp(arg, "--wipe") == 0) {
            config->mode = MODE_WIPE;
            has_mode = 1;
            mode_count++;
            continue;
        }
        
        if (strcmp(arg, "-p") == 0 || strcmp(arg, "--partition") == 0) {
            config->mode = MODE_PARTITION;
            has_mode = 1;
            mode_count++;
            continue;
        }
        
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            config->verbose = 1;
            continue;
        }
        
        if (strcmp(arg, "-nl") == 0 || strcmp(arg, "--no-log") == 0) {
            config->no_log = 1;
            continue;
        }
        
        if (strncmp(arg, "-s=", 3) == 0 || strncmp(arg, "--source=", 9) == 0) {
            value = strchr(arg, '=') + 1;
            strncpy(config->source, value, sizeof(config->source) - 1);
            has_source = 1;
            continue;
        }
        
        if (strncmp(arg, "-t=", 3) == 0 || strncmp(arg, "--target=", 9) == 0) {
            value = strchr(arg, '=') + 1;
            strncpy(config->target, value, sizeof(config->target) - 1);
            has_target = 1;
            continue;
        }
        
        if (strncmp(arg, "-l=", 3) == 0 || strncmp(arg, "--label=", 8) == 0) {
            value = strchr(arg, '=') + 1;
            strncpy(config->label, value, sizeof(config->label) - 1);
            continue;
        }
        
        if (i + 1 < argc) {
            if (strcmp(arg, "-s") == 0 || strcmp(arg, "--source") == 0) {
                strncpy(config->source, argv[++i], sizeof(config->source) - 1);
                has_source = 1;
                continue;
            }
            
            if (strcmp(arg, "-t") == 0 || strcmp(arg, "--target") == 0) {
                strncpy(config->target, argv[++i], sizeof(config->target) - 1);
                has_target = 1;
                continue;
            }
            
            if (strcmp(arg, "-l") == 0 || strcmp(arg, "--label") == 0) {
                strncpy(config->label, argv[++i], sizeof(config->label) - 1);
                continue;
            }
        }
        
        fprintf(stderr, "Error: Unknown argument '%s'\n", arg);
        fprintf(stderr, "Run `sudo buf -h` for help on commands\n");
        return -1;
    }
    
    if (mode_count > 1) {
        fprintf(stderr, "Error: Cannot use both --wipe and --partition modes\n");
        fprintf(stderr, "Run `sudo buf -h` for help on commands\n");
        return -1;
    }
    
    if (!has_mode) {
        fprintf(stderr, "Error: Installation mode not specified (use -w or -p)\n");
        fprintf(stderr, "Run `sudo buf -h` for help on commands\n");
        return -1;
    }
    
    if (!has_source) {
        fprintf(stderr, "Error: Source media not specified\n");
        fprintf(stderr, "Run `sudo buf -h` for help on commands\n");
        return -1;
    }
    
    if (!has_target) {
        fprintf(stderr, "Error: Target media not specified\n");
        fprintf(stderr, "Run `sudo buf -h` for help on commands\n");
        return -1;
    }
    
    if (config->mode == MODE_WIPE) {
        char response[10];
        printf("\nWARNING: The --wipe/-w flag will erase ALL DATA on this device, are you sure you want to continue? Y/N: ");
        fflush(stdout);
        
        if (fgets(response, sizeof(response), stdin) == NULL) {
            fprintf(stderr, "\nError: Couldn't read input\n");
            fprintf(stderr, "Run `sudo buf -h` for help on commands\n");
            return -1;
        }
        
        if (response[0] != 'Y' && response[0] != 'y') {
            fprintf(stderr, "Operation cancelled by user\n");
            return -1;
        }
    }
    
    return 0;
}