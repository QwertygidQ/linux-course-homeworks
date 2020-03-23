## Prerequisites
* qemu
* Arch Linux ISO

## Preparing a VM
```
qemu-img create -f qcow2 image.raw 4G
qemu-system-x86_64 -cdrom archlinux.iso -boot order=d -drive file=image.raw,format=qcow2 -m 512M
```

Inside the VM:
```
fdisk /dev/sda
n
p
```
(Create a default partition spanning the entire disk)
```
w
```

(Back in zsh)
```
mkfs.ext4 /dev/sda1
pacman -Syy
pacman -S reflector
reflector --verbose --latest 5 --sort rate --save /etc/pacman.d/mirrorlist
mount /dev/sda1 /mnt
pacstrap /mnt base linux linux-firmware
genfstab -U /mnt >> /mnt/etc/fstab
arch-chroot /mnt
pacman -S grub nano dhcpcd
grub-install /dev/sda
grub-mkconfig -o /boot/grub/grub.cfg
exit
poweroff
```

(Now you can run the VM without the ISO and log in as root)

## Running the VM
```
qemu-system-x86_64 image.raw -m 512M
dhcpcd
```

## Setting up the kernel
```
pacman -S git bc base-devel
git clone https://github.com/QwertygidQ/linux-course-homeworks
curl https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.5.10.tar.xz --output linux_src.tar.xz
tar xvf linux_src.tar.xz
cd linux-5.5.10
```
(Move all the files from this homework's cloned repository to their corresponding folders in linux-5.5.10)
