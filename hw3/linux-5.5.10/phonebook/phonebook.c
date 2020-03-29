#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/user_data.h>

#include <asm/errno.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/err.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include <linux/fs.h>
#include <linux/limits.h>
#include <linux/string.h>
#include <linux/types.h>

#define BUFFER_SIZE 256  // Phonebook module buffer size
#define USER_STRINGS 5

/*
struct file* open_file(const char *path, int flags, int rights)
{
    struct file *filp = NULL;
    mm_segment_t fs;

    fs = get_fs();
    set_fs(KERNEL_DS);
    filp = filp_open(path, flags, rights);
    set_fs(fs);

    if (IS_ERR(filp))
        return NULL;

    return filp;
}

void close_file(struct file *filp)
{
    filp_close(file, NULL);
}
*/

static struct file* open_file(const char *path, int flags)
{
    struct file *filp = filp_open(path, flags, 0);
    if (IS_ERR(filp))
        return NULL;

    return filp;
}

static void close_file(struct file *filp)
{
    filp_close(filp, NULL);
}

static ssize_t fill_add_message(struct user_data *user, char output_string[], size_t *output_len)
{
    char age_string[BUFFER_SIZE];
    size_t lengths[USER_STRINGS];
    size_t i;

    snprintf(age_string, BUFFER_SIZE, "%ld", user->age);

    // Overflow checking
    lengths[0] = strnlen(user->name,    BUFFER_SIZE);
    lengths[1] = strnlen(user->surname, BUFFER_SIZE);
    lengths[2] = strnlen(user->phone,   BUFFER_SIZE);
    lengths[3] = strnlen(user->email,   BUFFER_SIZE);
    lengths[4] = strnlen(age_string,    BUFFER_SIZE);

    *output_len = lengths[0];
    for (i = 1; i < USER_STRINGS; ++i)
    {
        if (lengths[i] > (BUFFER_SIZE - 1) - *output_len)
            return -EOVERFLOW;

        *output_len += lengths[i] + 1; // +1 for the space character
    }

    // Command ("a ", '\n' -- 3 chars)
    if (*output_len > (BUFFER_SIZE - 1) - 3)
        return -EOVERFLOW;

    *output_len += 3;

    snprintf(
        output_string,
        BUFFER_SIZE,
        "a %s %s %s %s %s\n",
        user->name,
        user->surname,
        user->phone,
        user->email,
        age_string
    );

    return 0;
}

SYSCALL_DEFINE0(hello_world)
{
	printk(KERN_INFO "Hello, world!");
	return 0;
}

SYSCALL_DEFINE3(
    get_user,
    const char __user *, surname,
    unsigned int, len,
	struct __user user_data *, output_data
)
{
	printk(KERN_INFO "get_user");
	return 0;
}

SYSCALL_DEFINE1(
    add_user,
    struct __user user_data *, input_data
)
{
    struct user_data user;

    ssize_t err;
    char add_message[BUFFER_SIZE];
    size_t add_message_len;

    struct file *filp;

    if (copy_from_user(&user, input_data, sizeof(struct user_data)))
        return -EFAULT;

    err = fill_add_message(&user, add_message, &add_message_len);
    if (err)
        return err;

    printk(KERN_INFO "Add message: %s", add_message);

    filp = open_file("/dev/phonebook_device", O_WRONLY);
    if (!filp)
        return -ENOENT;

    err = kernel_write(filp, add_message, sizeof(char) * add_message_len, &filp->f_pos);
    close_file(filp);

    if (err < 0)
        return err;

    return 0;
}

SYSCALL_DEFINE2(
    del_user,
    const char __user, *surname,
    unsigned int, len
)
{
    printk(KERN_INFO "del_user");
	return 0;
}
