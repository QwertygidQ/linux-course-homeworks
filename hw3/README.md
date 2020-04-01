# Syscalls for the Phonebook module

Exported syscalls:
* `get_user(const char *surname, unsigned int len, struct user_data *output_data)`
* `add_user(struct user_data *input_data)`
* `del_user(const char *surname, unsigned int len)`

(`struct user_data` is defined in linux-5.5.10/include/linux/user_data.h)


## Prerequisites
* QEMU
* Arch Linux ISO (kernel version 5.5.10)
* Linux v5.5.10 source code

## Preparing a VM
```
qemu-img create -f qcow2 image.raw 6G
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

Back in zsh:
```
mkfs.ext4 /dev/sda1
pacman -Syy
pacman -S reflector
reflector --verbose --latest 5 --sort rate --save /etc/pacman.d/mirrorlist
mount /dev/sda1 /mnt
pacstrap /mnt base linux linux-firmware
genfstab -U /mnt >> /mnt/etc/fstab
arch-chroot /mnt
pacman -S grub dhcpcd
grub-install /dev/sda
grub-mkconfig -o /boot/grub/grub.cfg
exit
poweroff
```

Now you can run the VM without the ISO and log in as root.

## Running the VM
```
qemu-system-x86_64 image.raw -m 512M --enable-kvm
dhcpcd
```

## Patching the kernel
```
rsync -a path/to/hw3/linux-5.5.10/ path/to/kernel/source/linux-5.5.10/
```

## Setting up the kernel
(Compile the kernel on your host machine (I used `make defconfig` and changed `CONFIG_LOCALVERSION`) and upload it to the VM (e.g., using `sshfs`))

(Script taken from https://brennan.io/2016/11/14/kernel-dev-ep3/)
```
cd linux-5.5.10/
SUFFIX="-PHONEBOOK"
make modules_install
cp arch/x86_64/boot/bzImage /boot/vmlinuz-linux$SUFFIX
sed s/linux/linux$SUFFIX/g \
    </etc/mkinitcpio.d/linux.preset \
    >/etc/mkinitcpio.d/linux$SUFFIX.preset
mkinitcpio -p linux$SUFFIX
grub-mkconfig -o /boot/grub/grub.cfg
reboot
```

(Select "Advanced options for Arch Linux" and then "Arch Linux, with Linux linux-PHONEBOOK")

## Testing
(Save in test.c)

```
#include <stdio.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>

int main()
{
    // hello_world syscall
    long result = syscall(436);
    printf("syscall returned %d\n", result);

    return 0;
}
```

```
gcc test.c
./a.out
dmesg | tail | grep "Hello, world!"
```

## Example
(Download, make and insert the Phonebook module from hw1/)

(Save in example.c)
```
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>

#define get_user 437
#define add_user 438
#define del_user 439

struct user_data
{
	char *name, *surname, *phone, *email;
	size_t name_len, surname_len, phone_len, email_len;
	long       age;
};

#define BUFFER_SIZE 256  // Phonebook module buffer size

void allocate(struct user_data *user)
{
	user->name = (char *)malloc(sizeof(char) * BUFFER_SIZE);
	user->surname = (char *)malloc(sizeof(char) * BUFFER_SIZE);
	user->phone = (char *)malloc(sizeof(char) * BUFFER_SIZE);
	user->email = (char *)malloc(sizeof(char) * BUFFER_SIZE);
}

void deallocate(struct user_data *user)
{
	free(user->surname);
	free(user->name);
	free(user->phone);
	free(user->email);
}

int main()
{
	// User struct initialization
	struct user_data data;

	data.surname = "Surname";
	data.surname_len = strlen(data.surname);

	data.name = "Name";
	data.name_len = strlen(data.name);

	data.phone = "+1(111)1111";
	data.phone_len = strlen(data.phone);

	data.email = "email@example.com";
	data.email_len = strlen(data.email);

	data.age = 20;

	// Adding the user
	long result;

	result = syscall(add_user, &data);
	printf("add_user returned %d\n", result);

	// Finding the user
	struct user_data found;
	allocate(&found);

	result = syscall(get_user, data.surname, strlen(data.surname), &found);
	printf("get_user returned %d\n", result);
	printf(
		"Surname: %s, Name: %s, Email: %s, Phone: %s, Age: %ld\n",
		found.surname,
		found.name,
		found.email,
		found.phone,
		found.age
	);
	deallocate(&found);

	// Deleting the user
	result = syscall(del_user, data.surname, strlen(data.surname));
	printf("del_user returned %d\n", result);

	// Finding a nonexistant user should fail
	allocate(&found);

	result = syscall(get_user, data.surname, strlen(data.surname), &found);
	printf("get_user returned %d for a deleted user\n", result);

	deallocate(&found);

	return 0;
}
```

```
gcc example.c
./a.out
dmesg | tail
```
