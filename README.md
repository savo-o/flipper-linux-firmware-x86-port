## Running Flipper One Linux in QEMU

Recently, the Flipper Devices team shared the kernel source code for their upcoming device with the community.
Flipper One is a portable mini-computer based on an ARM processor. Out of curiosity, I decided to try running the system image on a regular x86 PC.

## What you will need

1. Linux. I used Kali, but in theory any distribution should work.

2. QEMU

3. A few hours of free time

At the moment of this experiment, we only have access to the kernel and the system rootfs. In practice, that’s enough to try booting the system.

## Running the system

Examples will be shown on Debian-based distributions.

1. Installing dependencies

sudo apt update
sudo apt install git build-essential bc bison flex libssl-dev \
libncurses-dev qemu-system-arm qemu-system-aarch64

Next you have two options: build the kernel yourself, or download the one I compiled that already boots the OS.

## Prebuilt kernel

Download the kernel and rootfs from the Releases section.

Then run in the terminal:

qemu-system-aarch64 \
-M virt \
-cpu cortex-a72 \
-m 2048 \
-kernel flipper-linux-kernel/arch/arm64/boot/Image \
-append "root=/dev/vda rw console=tty1" \
-drive file=rootfs.img,format=raw,if=virtio \
-device virtio-gpu-pci \
-device virtio-keyboard-pci \
-device qemu-xhci \
-device usb-kbd \
-display gtk

This is a basic configuration that allows the system to boot.

## Building from source

1. Clone the repository

git clone --depth=1 https://github.com/flipperdevices/flipper-linux-kernel
cd flipper-linux-kernel

2. Kernel configuration

make ARCH=arm64 menuconfig

Enable (if not already enabled):

- VirtIO
- VirtIO GPU
- Framebuffer console

3. Build the kernel

make ARCH=arm64 -j$(nproc)

4. Preparing the rootfs

Download the file from the official website.

dd if=debian-4096-generic-build-854.img of=rootfs.img bs=1 skip=16777216

5. Running the system

qemu-system-aarch64 \
-M virt \
-cpu cortex-a72 \
-m 2048 \
-kernel flipper-linux-kernel/arch/arm64/boot/Image \
-append "root=/dev/vda rw console=tty1" \
-drive file=rootfs.img,format=raw,if=virtio \
-device virtio-gpu-pci \
-device virtio-keyboard-pci \
-device virtio-mouse-pci \
-display gtk

![screenshot](run.jpeg)

At the time of writing, running Flipper OS on a PC doesn’t have much practical value yet. This is mostly an experiment to see whether it’s possible.

It may become more useful once a proper interface and additional components appear. For now, this is just the beginning. The project will continue to evolve — stay tuned for updates.
