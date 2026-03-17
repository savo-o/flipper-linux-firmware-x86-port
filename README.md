Запуск Linux из Flipper One в QEMU

Недавно команда Flipper Devices поделилась с сообществом исходниками ядра для будущего девайса.
Flipper One - портативный мини-компьютер на базе ARM процессора. И ради интереса, я захотел попробовать запустить образ системы на обычном x86 ПК.

Для начала нам понадобится:

1. Linux. Я использовал Kali, но в теории можно любой дистрибутив

2. QEMU

3. Пару-тройку свободных часов

На момент эксперимента нам доступно только ядро и rootfs системы. В принципе, нам этого хватает.

Запуск:

Пример буду показывать на Debian-дистрибутивах

1. Установка зависимостей
sudo apt update
sudo apt install git build-essential bc bison flex libssl-dev \
libncurses-dev qemu-system-arm qemu-system-aarch64

Далее есть выбор, собрать ядро самому, или скачать мое, которое уже запускает ОС

Готовое ядро: 

Скачайте ядро и rootfs в разделе Release

Далее в терминале:

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

Базовый конфиг с которым система хотя бы запустится.

Сборка исходников:

1. Клонирование репозитория
git clone --depth=1 https://github.com/flipperdevices/flipper-linux-kernel
cd flipper-linux-kernel

2. Настройки ядра: 
make ARCH=arm64 menuconfig

Включаем (если не включено):
VirtIO
VirtIO GPU
Framebuffer console

3. Сборка ядра:

make ARCH=arm64 -j$(nproc)

4. Подготовка rootfs:

Скачиваем файл с офф. сайта - тык

dd if=debian-4096-generic-build-854.img of=rootfs.img bs=1 skip=16777216

5. Запуск:

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

Сейчас, на момент написания этой статьи, у Flipper OS на ПК смысла особо нет. Это просто эксперимент, возможно ли это. Возможно, смысл появится когда будет интерфейс и т.д, но пока что всего этого нет. Проект будет развиваться. Следите за обновлениями.
