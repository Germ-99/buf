#!/bin/bash

set -e

echo "================================"
echo "buf - Bootable USB Flasher"
echo "Installation Script"
echo "================================"
echo ""

MISSING_DEPS=0

echo "Checking build dependencies..."

if ! command -v git &> /dev/null; then
    echo "  ✗ git is not installed"
    MISSING_DEPS=1
fi

if ! command -v make &> /dev/null; then
    echo "  ✗ make is not installed"
    MISSING_DEPS=1
fi

if ! command -v gcc &> /dev/null; then
    echo "  ✗ gcc is not installed"
    MISSING_DEPS=1
fi

echo ""
echo "Checking runtime dependencies..."

if ! command -v mount &> /dev/null; then
    echo "  ✗ mount is not installed"
    MISSING_DEPS=1
fi

if ! command -v umount &> /dev/null; then
    echo "  ✗ umount is not installed"
    MISSING_DEPS=1
fi

if ! command -v wipefs &> /dev/null; then
    echo "  ✗ wipefs is not installed"
    MISSING_DEPS=1
fi

if ! command -v lsblk &> /dev/null; then
    echo "  ✗ lsblk is not installed"
    MISSING_DEPS=1
fi

if ! command -v blockdev &> /dev/null; then
    echo "  ✗ blockdev is not installed"
    MISSING_DEPS=1
fi

if ! command -v df &> /dev/null; then
    echo "  ✗ df is not installed"
    MISSING_DEPS=1
fi

if ! command -v parted &> /dev/null; then
    echo "  ✗ parted is not installed"
    MISSING_DEPS=1
fi

if ! command -v 7z &> /dev/null; then
    echo "  ✗ 7z (p7zip) is not installed"
    MISSING_DEPS=1
fi

if ! command -v mkdosfs &> /dev/null && ! command -v mkfs.vfat &> /dev/null && ! command -v mkfs.fat &> /dev/null; then
    echo "  ✗ dosfstools (mkdosfs/mkfs.vfat) is not installed"
    MISSING_DEPS=1
fi

if ! command -v mkntfs &> /dev/null; then
    echo "  ✗ ntfs-3g (mkntfs) is not installed"
    MISSING_DEPS=1
fi

if ! command -v grub-install &> /dev/null && ! command -v grub2-install &> /dev/null; then
    echo "  ✗ grub (grub-install/grub2-install) is not installed"
    MISSING_DEPS=1
fi

if ! command -v wget &> /dev/null; then
    echo "  ✗ wget is not installed"
    MISSING_DEPS=1
fi

if [ $MISSING_DEPS -eq 1 ]; then
    echo ""
    echo "================================"
    echo "Missing dependencies detected!"
    echo "================================"
    echo ""
    echo "Please install the missing packages:"
    echo ""
    echo "Ubuntu/Debian:"
    echo "  sudo apt install git build-essential parted dosfstools ntfs-3g grub2-common grub-pc-bin p7zip-full wget util-linux"
    echo ""
    echo "Arch Linux:"
    echo "  sudo pacman -S git base-devel parted dosfstools ntfs-3g grub p7zip wget util-linux"
    echo ""
    echo "Fedora/RHEL:"
    echo "  sudo dnf install git gcc make parted dosfstools ntfs-3g grub2-tools p7zip p7zip-plugins wget util-linux"
    echo ""
    exit 1
fi

echo ""
echo "✓ All dependencies found!"
echo ""

echo "Cloning buf repository..."
if [ -d "buf" ]; then
    echo "Warning: buf directory already exists, removing it..."
    rm -rf buf
fi

git clone https://github.com/Germ-99/buf.git

cd buf

echo ""
echo "Building buf..."
make

echo ""
echo "Installing buf (requires sudo)..."
make install

echo ""
echo "================================"
echo "Installation complete!"
echo "================================"
echo ""
echo "Please restart your shell (or run: source ~/.bashrc)"
echo "Then test with: sudo buf -h"
echo ""
