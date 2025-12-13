# buf documentation

To preface, this documentation is on *how to use buf* and *how buf works*, not on how to install.
If you want to understand how to install buf, go [here](https://github.com/Germ-99/buf?tab=readme-ov-file#installing)

# Flags

buf has a set of flags that can be used, some are needed and some are optional. All flags (except for --version) have unix-style naming and allow long/short naming. For examples, we will use long style naming. When we list the flags, we will show both the short and long version of the flag.

Here is all of them:

## Required Flags

### Installation Mode (Choose One)

- **`-w` / `--wipe`**: This flag will tell buf to fully wipe the selected drive and create a new partition table. This erases ALL data on the device.
  ```bash
  sudo buf --wipe --source=eos.iso --target=/dev/sdb
  ```
  **NOTE:** This flag is not needed if you already have the `--partition` flag chosen.

- **`-p` / `--partition`**: This flag tells buf to flash the image onto an existing partition without wiping the entire device. The target must be a partition (block device ending in a number like sda1, sdb2).
  ```bash
  sudo buf --partition --source=linux.iso --target=/dev/sda1
  ```
  **NOTE:** This flag is NOT needed if you already have the `--wipe` flag chosen.

### Source and Target

- **`-s` / `--source`**: Specifies the ISO image file to flash.
  ```bash
  sudo buf --wipe --source=archlinux.iso --target=/dev/sda
  ```
  Accepts either a path to an ISO file or a block device (DVD drive).

- **`-t` / `--target`**: Specifies the target device or partition where the ISO will be flashed.
  ```bash
  sudo buf --wipe --target=/dev/sda --source=windows11.iso
  ```
  - For **wipe mode**: Must be a device (e.g., `/dev/sdb`)
  - For **partition mode**: Must be a partition (e.g., `/dev/sdb1`)

## Optional Flags

- **`-l` / `--label`**: Sets a custom filesystem label for the USB drive. Default is "BOOTABLE USB".
  ```bash
  sudo buf --wipe --source=ubuntu.iso --target=/dev/sdb --label="UBUNTU 24.04"
  ```

- **`-v` / `--verbose`**: Enables verbose output, showing detailed information during the flashing process including:
  - Target device and partition information
  - Individual file paths as they're copied
  - Immediate progress updates (not throttled to once per second)
  ```bash
  sudo buf --wipe --source=fedora.iso --target=/dev/sdb --verbose
  ```

- **`-nl` / `--no-log`**: Disables logging to a file. By default, buf creates a log file in your home directory with detailed information about the operation.
  ```bash
  sudo buf --wipe --source=debian.iso --target=/dev/sdb --no-log
  ```

## Information Flags

- **`-ls` / `--list`**: Lists all removable USB devices on your system with their model, size, and type.
  ```bash
  sudo buf --list
  ```
  Example output:
  ```
  DEVICE          MODEL                SIZE       TYPE
  ================================================================
  /dev/sdb        SanDisk USB         32G        usb
  /dev/sdc        Kingston DataTrav   16G        usb
  ```

- **`-h` / `--help`**: Displays help information showing all available flags and usage examples.
  ```bash
  sudo buf --help
  ```

- **`--version`**: Shows the current version of buf.
  ```bash
  buf --version
  ```

# Usage Examples

## Basic Usage

### Creating a bootable USB (wipe mode)
```bash
sudo buf --wipe --source=/path/to/ubuntu.iso --target=/dev/sdb
```

### Using an existing partition (partition mode)
```bash
sudo buf --partition --source=/path/to/arch.iso --target=/dev/sdb1
```

## With Optional Flags

### Verbose output with custom label
```bash
sudo buf --wipe --source=windows11.iso --target=/dev/sdb --verbose --label="WIN11 INSTALL"
```

### Disable logging
```bash
sudo buf --wipe --source=fedora.iso --target=/dev/sdb --no-log
```

## Short Flag Syntax

All flags can be used in short form:
```bash
sudo buf -w -s=ubuntu.iso -t=/dev/sdb -v -l="UBUNTU"
```

# Understanding Modes

## Wipe Mode (`--wipe`)

**What it does:**
1. Erases all data on the target device
2. Wipes all filesystem signatures
3. Creates a new MSDOS partition table
4. Creates a new partition
5. Formats the partition (FAT32 or NTFS)
6. Copies ISO contents
7. Installs bootloader (for Windows ISOs)

**When to use:**
- You want to ensure a clean installation
- The USB drive has multiple partitions or corrupted data
- You want buf to handle everything automatically

**Warning:** ALL data on the device will be permanently erased!

**Example:**
```bash
sudo buf --wipe --source=kubuntu.iso --target=/dev/sdb
```

## Partition Mode (`--partition`)

**What it does:**
1. Uses an existing partition on the device
2. Copies ISO contents to the partition
3. Installs bootloader (for Windows ISOs)

**When to use:**
- You've already prepared a partition
- You want to preserve other partitions on the device
- You're updating an existing bootable USB

**Requirements:**
- The partition must already exist
- The partition must be formatted (FAT32 or NTFS recommended)
- The partition must have enough free space

**Example:**
```bash
sudo buf --partition --source=manjaro.iso --target=/dev/sdb1
```

# ISO Type Detection

buf automatically detects the type of ISO you're flashing:

## Windows ISOs
- Automatically switches to NTFS if files larger than 4GB are detected
- Installs GRUB bootloader for BIOS boot support
- Applies Windows 7 UEFI workaround if needed
- Creates UEFI:NTFS partition for NTFS installations (UEFI boot support)

## Linux ISOs
- Uses FAT32 by default (best compatibility)
- No additional bootloader needed (Linux ISOs include their own)

## Other ISOs
- Treated as generic bootable media
- Uses FAT32 filesystem

# Filesystem Selection

buf automatically selects the appropriate filesystem:

- **FAT32** (default): Best compatibility, works with both BIOS and UEFI
- **NTFS** (automatic): Used when Windows ISOs contain files larger than 4GB

You cannot manually specify the filesystem type - buf makes this decision based on the ISO contents.

# Progress Tracking

During the copy process, buf shows:
```
Total size to copy: 4096 MB
Copying: 1234 MB / 4096 MB (30%) - install.wim
```

With `--verbose` flag:
```
Target device: /dev/sdb
Target partition: /dev/sdb1

Copying: /mnt/buf_source_12345/boot/bcd
Copying: 10 MB / 4096 MB (0%) - boot/bcd
Copying: /mnt/buf_source_12345/sources/install.wim
Copying: 1234 MB / 4096 MB (30%) - install.wim
```

# Log Files

By default, buf creates a log file in your home directory:
```
~/buf-MM-DD-YY.log
```

The log contains:
- System information (kernel, distribution, user)
- Configuration settings (mode, source, target, filesystem)
- All operations performed
- Any errors or warnings encountered
- Operation statistics (duration, error count, warning count)

**Disable logging:** Use the `--no-log` flag

**Example log location:**
```
/home/username/buf-12-12-25.log
```

# Troubleshooting

## "Error: buf must be run as sudo"
**Solution:** Run buf with sudo privileges:
```bash
sudo buf --wipe --source=iso.iso --target=/dev/sdb
```

## "Error: Target must be a device (e.g., /dev/sdb), not a partition"
You're using `--wipe` mode but specified a partition (e.g., /dev/sdb1).

**Solution:** Either:
- Use the device name: `--target=/dev/sdb`
- Switch to partition mode: `--partition --target=/dev/sdb1`

## "Error: Target must be a partition (e.g., /dev/sdb1), not a device"
You're using `--partition` mode but specified a device (e.g., /dev/sdb).

**Solution:** Either:
- Use a partition name: `--target=/dev/sdb1`
- Switch to wipe mode: `--wipe --target=/dev/sdb`

**Note that switching to wipe mode will erase all data on the selected device**

## "Error: Not enough space on target partition"
The target doesn't have enough free space for the ISO.

**Solution:**
- Use a larger USB drive
- Choose wipe mode to use the entire device
- Free up space on the partition

## "Error: Failed to mount target partition"
The partition may be corrupted or formatted with an unsupported filesystem.

**Solution:**
- Use wipe mode to reformat: `--wipe`
- Manually format the partition as FAT32 or NTFS first

## "Error: Required dependencies not found"
buf requires several system tools to function.

**Solution:** Install missing dependencies:

**Arch Linux:**
```bash
sudo pacman -S util-linux parted dosfstools ntfs-3g grub p7zip wget
```

**Ubuntu/Debian:**
```bash
sudo apt install util-linux parted dosfstools ntfs-3g grub2-common grub-pc-bin p7zip-full wget
```

**Fedora/RHEL:**
```bash
sudo dnf install util-linux parted dosfstools ntfs-3g grub2-tools p7zip p7zip-plugins wget
```

## USB drive not showing up
**Solution:** Use `--list` to see detected devices:
```bash
sudo buf --list
```

If your device isn't shown, it may not be detected as a removable drive.

## "Operation cancelled by user"
You answered 'N' to the wipe confirmation prompt.

**Solution:** Run the command again and answer 'Y' to proceed.

# Supported Platforms

**Operating Systems:**
- Arch Linux
- Ubuntu / Debian
- Fedora / RHEL
- openSUSE
- Manjaro
- Any Linux distribution with required dependencies

**Architectures:**
- x86_64 (Intel/AMD 64-bit)

**ISO Types:**
- Windows installation media (7, 8, 10, 11, Server)
- Linux distributions (Ubuntu, Fedora, Arch, Debian, etc.)
- Other bootable ISOs

# Getting Help

- **GitHub Issues:** https://github.com/Germ-99/buf/issues
- **Source Code:** https://github.com/Germ-99/buf
- **License:** GPL-3.0-or-later

# Contributing

Contributions are welcome! Please submit issues and pull requests on GitHub.

# Credits

**Author:** Bryson Kelly  
**Logo Design:** [mia](https://github.com/marshmallow-mia)  
**License:** GPL-3.0