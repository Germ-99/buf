# buf
![logo](res/buf.png)
buf is a command-line utility for flashing ISOs to create bootable USB drives

Is it Bryson's USB Flasher or Bootable USB Flasher?

logo by my good friend [mia](https://github.com/marshmallow-mia). Go check her github out, she's cool :)

**NOTE**:
This is only for Linux, (at the moment) adding windows support soon :)

OK, quick rant. I CANNOT FIGURE OUT HOW TO PORT THIS!!!>!!!!>>!>!. The windows API is absolute trash aswell as the documentation for it. Why such weird semantics??? Why??? Has I ever????

# Installing
Run the following command:
```
curl -fsSL https://raw.githubusercontent.com/Germ-99/buf/main/scripts/install.sh | bash
```

# Building

**The following dependencies are required for BUILDING:**
- gcc
- make

**The following dependencies are needed for RUNNING:**
- mount/umount
- wipefs
- lsblk
- blockdev
- df
- parted
- 7z
- dosfstools
- ntfs-3g 
- grub2-common/grub-pc-bin
- wget

**Commands for getting dependencies**
```
# Ubuntu/Debian
sudo apt install build-essential parted dosfstools ntfs-3g grub2-common grub-pc-bin p7zip-full wget

# Arch Linux
sudo pacman -S base-devel parted dosfstools ntfs-3g grub p7zip wget

# Fedora/RHEL
sudo dnf install gcc make parted dosfstools ntfs-3g grub2-tools p7zip p7zip-plugins wget
```

## After you have the dependencies
Run the following commands:
```
git clone https://github.com/Germ-99/buf.git

cd buf

make && make install
```

## After Building
Run ``sudo buf -h`` to verify it works (if it isn't recognized as a command, restart your shell and try again)

You're done!
