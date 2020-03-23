#include <linux/kernel.h>
#include <linux/user_data.h>

/*SYSCALL_DEFINE3(get_user, const char *, surname, unsigned int, len,
		struct user_data *, output_data)
*/

asmlinkage long sys_get_user(
    const char *surname,
    unsigned int len,
    struct user_data *output_data
)
{
	printk(KERN_INFO "Hello, world!");
	return 0;
}
