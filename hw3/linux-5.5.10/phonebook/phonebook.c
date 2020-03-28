#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/user_data.h>

SYSCALL_DEFINE0(hello_world)
{
	printk(KERN_INFO "Hello, world!");
	return 0;
}

SYSCALL_DEFINE3(
    get_user,
    const char *, surname,
    unsigned int, len,
	struct user_data *, output_data
)
{
	printk(KERN_INFO "get_user");
	return 0;
}

SYSCALL_DEFINE1(
    add_user,
    struct user_data *, input_data
)
{
    printk(KERN_INFO "add_user");
	return 0;
}

SYSCALL_DEFINE2(
    del_user,
    const char, *surname,
    unsigned int, len
) {
    printk(KERN_INFO "del_user");
	return 0;
}
