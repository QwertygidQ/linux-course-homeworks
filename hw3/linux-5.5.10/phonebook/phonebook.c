#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/user_data.h>

#include <asm/errno.h>
#include <asm/uaccess.h>
#include <linux/err.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <linux/string.h>
#include <linux/types.h>

#define DEVICE_FILE "/dev/phonebook_device"
#define BUFFER_SIZE 256 // Phonebook module buffer size
#define USER_STRINGS 5

static struct file *open_file(const char *path, int flags)
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

static int allocate_user_data(
    struct user_data *to,
    struct user_data *from)
{
    to->surname = NULL;
    to->name = NULL;
    to->phone = NULL;
    to->email = NULL;

    to->surname = (char *)kmalloc(sizeof(char) * (from->surname_len + 1), GFP_KERNEL);
    if (!to->surname)
        return -EFAULT;

    to->name = (char *)kmalloc(sizeof(char) * (from->name_len + 1), GFP_KERNEL);
    if (!to->name)
    {
        kfree(to->surname);
        to->surname = NULL;

        return -EFAULT;
    }

    to->phone = (char *)kmalloc(sizeof(char) * (from->phone_len + 1), GFP_KERNEL);
    if (!to->phone)
    {
        kfree(to->surname);
        to->surname = NULL;

        kfree(to->name);
        to->name = NULL;

        return -EFAULT;
    }

    to->email = (char *)kmalloc(sizeof(char) * (from->email_len + 1), GFP_KERNEL);
    if (!to->email)
    {
        kfree(to->surname);
        to->surname = NULL;

        kfree(to->name);
        to->name = NULL;

        kfree(to->phone);
        to->phone = NULL;

        return -EFAULT;
    }

    return 0;
}

static void deallocate_user_data(struct user_data *user)
{
    kfree(user->surname);
    kfree(user->name);
    kfree(user->phone);
    kfree(user->email);
}

static int copy_user_data_from_user(
    struct user_data *to,
    struct user_data __user *from)
{
    int err;
    struct user_data kern_from;

    if (!to || !from)
        return -EFAULT;

    if (copy_from_user(&kern_from, from, sizeof(struct user_data)))
        return -EFAULT;

    err = allocate_user_data(to, &kern_from);
    if (err)
        return err;

    if (
        copy_from_user(to->surname, kern_from.surname, sizeof(char) * kern_from.surname_len) ||
        copy_from_user(to->name, kern_from.name, sizeof(char) * kern_from.name_len) ||
        copy_from_user(to->phone, kern_from.phone, sizeof(char) * kern_from.phone_len) ||
        copy_from_user(to->email, kern_from.email, sizeof(char) * kern_from.email_len))
    {
        deallocate_user_data(to);
        return -EFAULT;
    }

    to->surname_len = kern_from.surname_len;
    to->name_len = kern_from.name_len;
    to->phone_len = kern_from.phone_len;
    to->email_len = kern_from.email_len;
    to->age = kern_from.age;

    to->surname[kern_from.surname_len] = '\0';
    to->name[kern_from.name_len] = '\0';
    to->phone[kern_from.phone_len] = '\0';
    to->email[kern_from.email_len] = '\0';

    return 0;
}

static int fill_add_message(struct user_data *user, char output_string[], size_t *output_len)
{
    char age_string[BUFFER_SIZE];
    size_t lengths[USER_STRINGS];
    size_t i;

    // Overflow checking
    lengths[0] = user->surname_len;
    lengths[1] = user->name_len;
    lengths[2] = user->phone_len;
    lengths[3] = user->email_len;
    lengths[4] = snprintf(age_string, BUFFER_SIZE, "%ld", user->age);

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
        age_string);

    return 0;
}

static int send_surname_message(
    const char command,
    const char __user *surname,
    unsigned int len)
{
    char ker_space_surname[BUFFER_SIZE], message[BUFFER_SIZE];
    struct file *filp;
    int err;

    // command char, space, '\n' -- 3 chars
    if (len > (BUFFER_SIZE - 1) - 3)
        return -EOVERFLOW;

    if (copy_from_user(&ker_space_surname, surname, len))
        return -EFAULT;

    ker_space_surname[len] = '\0';

    snprintf(message, BUFFER_SIZE, "%c %s\n", command, ker_space_surname);

    filp = open_file(DEVICE_FILE, O_WRONLY);
    if (!filp)
        return -ENOENT;

    err = kernel_write(filp, message, sizeof(char) * (len + 3), &filp->f_pos);
    close_file(filp);

    if (err < 0)
        return err;

    return 0;
}

static int parse_find_output(const char message[], const size_t len, struct user_data *user)
{
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
    struct __user user_data *, output_data)
{
    char user_message[BUFFER_SIZE];
    int err, message_len;
    struct file *filp;

    err = send_surname_message('f', surname, len);
    if (err)
        return err;

    filp = open_file(DEVICE_FILE, O_RDONLY);
    if (!filp)
        return -ENOENT;

    message_len = kernel_read(filp, user_message, BUFFER_SIZE, &filp->f_pos);
    close_file(filp);

    if (message_len < 0)
        return message_len;

    user_message[message_len - 1] = '\0'; // change '\n' to '\0'

    //struct user_data user;
    //err = parse_find_output(user_message, message_len, user);

    printk(KERN_INFO "'%s', %d\n", user_message, message_len);

    return 0;
}

SYSCALL_DEFINE1(
    add_user,
    struct __user user_data *, input_data)
{
    struct user_data user;

    int err;
    char add_message[BUFFER_SIZE];
    size_t add_message_len;

    struct file *filp;

    err = copy_user_data_from_user(&user, input_data);
    if (err)
        return err;

    err = fill_add_message(&user, add_message, &add_message_len);
    deallocate_user_data(&user);
    if (err)
        return err;

    filp = open_file(DEVICE_FILE, O_WRONLY);
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
    const char __user *, surname,
    unsigned int, len)
{
    return send_surname_message('d', surname, len);
}
