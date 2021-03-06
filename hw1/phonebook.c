// SPDX-License-Identifier: GPL-2.0-only

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "phonebook_device"
#define CLASS_NAME  "phonebook"
#define BUFFER_SIZE 256
#define MAX_USERS   256

MODULE_LICENSE("GPL");

struct User {
    const char *name, *surname, *phone, *email, *to_split;
    long       age;
    int        successfully_created;
};

static struct User users[MAX_USERS];
static size_t      users_count = 0;

static int           major_number;
static char          user_buffer[BUFFER_SIZE] = {0}; // messages from the user
static int           user_msg_size;
static int           user_buffer_needs_parsing = 0;
static char          device_buffer[BUFFER_SIZE] = {0}; // messages to the user
static int           device_msg_size;
static int           device_write_opened_count = 0;
static int           device_read_opened_count = 0;
static struct class  *phonebook_class = NULL;
static struct device *phonebook_device = NULL;
static struct mutex  flush_mutex;

static int     dev_open(struct inode *, struct file *);
static int     dev_flush(struct file *, fl_owner_t id);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char __user *, size_t, loff_t *);

static struct file_operations fops = {
    .open    = dev_open,
    .flush   = dev_flush,
    .release = dev_release,
    .read    = dev_read,
    .write   = dev_write,
};

static struct User new_user(const char *data);
static ssize_t     find_user(const char *surname);
static int         add_user(const struct User user);
static int         remove_user(size_t index);

static int parse_user_buffer(void);

static int __init phonebook_init(void) {
    printk(KERN_INFO "Phonebook: initializing the module\n");

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "Phonebook: failed to allocate a major number\n");
        return major_number;
    }

    printk(KERN_INFO "Phonebook: successfully registered with major number %d\n", major_number);

    phonebook_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(phonebook_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Phonebook: failed to register a device class\n");
        return PTR_ERR(phonebook_class);
    }

    printk(KERN_INFO "Phonebook: successfully registered the device class\n");

    phonebook_device = device_create(phonebook_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(phonebook_device)) {
        class_unregister(phonebook_class);
        class_destroy(phonebook_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Phonebook: failed to register a device\n");
        return PTR_ERR(phonebook_device);
    }

    mutex_init(&flush_mutex);

    printk(KERN_INFO "Phonebook: successfully initialized\n");
    return 0;
}

static void __exit phonebook_exit(void) {
    size_t i;

    device_destroy(phonebook_class, MKDEV(major_number, 0));
    class_unregister(phonebook_class);
    class_destroy(phonebook_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    for (i = 0; i < users_count; i++)
        kfree(users[i].to_split);

    printk(KERN_INFO "Phonebook: successfully exited\n");
}

static int dev_open(struct inode *inode, struct file *file) {
    if (file->f_flags & O_RDWR)  // You can't open this device to both read and write
        return -ENOSYS;

    if (
        ((file->f_flags & O_WRONLY) && device_write_opened_count) ||
        ((file->f_flags & O_RDONLY) && device_read_opened_count)
    )
        return -EBUSY;

    if (file->f_flags & O_WRONLY)
        device_write_opened_count = 1;
    else
        device_read_opened_count = 1;

    try_module_get(THIS_MODULE);

    printk(KERN_INFO "Phonebook: device has been opened\n");
    return 0;
}

static int dev_flush(struct file *file, fl_owner_t id) {
    mutex_lock(&flush_mutex);

    if (user_buffer_needs_parsing) {
        if (parse_user_buffer()) { // parse_user_buffer returns 1 on error
            device_buffer[0] = 0;
            device_msg_size = 0;
            printk(KERN_INFO "Phonebook: cleared the device buffer\n");
        }
        user_buffer_needs_parsing = 0;
    } else {
        device_buffer[0] = 0;
        device_msg_size = 0;
        printk(KERN_INFO "Phonebook: cleared the device buffer\n");
    }

    user_buffer[0] = 0;
    user_msg_size = 0;
    printk(KERN_INFO "Phonebook: cleared the user buffer\n");

    mutex_unlock(&flush_mutex);
    return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
    if (file->f_flags & O_WRONLY)
        device_write_opened_count = 0;
    else
        device_read_opened_count = 0;

    module_put(THIS_MODULE);

    printk(KERN_INFO "Phonebook: device has been closed\n");
    return 0;
}

// Data path: device -> user
static ssize_t dev_read(struct file *file, char __user *buffer, size_t len, loff_t *offset) {
    int error_count, copy_len;

    copy_len = min(device_msg_size - *offset, len);
    if (copy_len <= 0)
        return 0;

    error_count = copy_to_user(buffer, device_buffer + *offset, copy_len);
    if (error_count != 0) {
        printk(KERN_ERR "Phonebook: failed to copy %d bytes to the user space\n", error_count);
        return -EFAULT;
    }

    *offset += copy_len;
    printk(KERN_INFO "Phonebook: successfully copied the message (%d chars) to user space\n", copy_len);
    return copy_len;
}

// Data path: user -> device
static ssize_t dev_write(struct file *file, const char __user *buffer, size_t len, loff_t *offset) {
    int error_count, copy_len;

    user_buffer_needs_parsing = 0;

    copy_len = min(BUFFER_SIZE - *offset, len);
    if (copy_len <= 0) {
        if (BUFFER_SIZE == *offset)
            printk(KERN_INFO "Phonebook: no more space left in the buffer, ignoring\n");
        else
            printk(KERN_INFO "Phonebook: nothing more to copy from user space to device\n");

        user_buffer_needs_parsing = 1;
        return len;
    }

    error_count = copy_from_user(user_buffer + *offset, buffer, copy_len);
    if (error_count != 0) {
        printk(KERN_ERR "Phonebook: failed to copy %d bytes from the user space\n", error_count);
        return -EFAULT;
    }

    *offset += copy_len;
    user_msg_size = max(*offset - 1, 0);
    user_buffer[user_msg_size] = 0;
    user_buffer[BUFFER_SIZE - 1] = 0; // guarantees that the user buffer is zero-terminated
    user_buffer_needs_parsing = 1;
    printk(KERN_INFO "Phonebook: received %zu characters from the user, new message size: %d\n", len, user_msg_size);
    return copy_len;
}

// Format: "name surname phone email age"
static struct User new_user(const char *data) {
    const size_t len = strlen(data);
    long age;
    char *to_split, *age_str;

    struct User user;
    user.successfully_created = 0;

    to_split = (char *)kmalloc(sizeof(char) * (len + 1), GFP_KERNEL);
    if (!to_split) {
        printk(KERN_ERR "Phonebook: failed to allocate memory for user info parsing\n");
        return user;
    } else {
        strcpy(to_split, data);
    }

    user.name = strsep(&to_split, " ");
    if (user.name == NULL) {
        printk(KERN_ERR "Phonebook: invalid user data format (failed to parse the name)\n");
        kfree(to_split);
        return user;
    }
    user.surname = strsep(&to_split, " ");
    if (user.surname == NULL) {
        printk(KERN_ERR "Phonebook: invalid user data format (failed to parse the surname)\n");
        kfree(to_split);
        return user;
    }
    user.phone = strsep(&to_split, " ");
    if (user.phone == NULL) {
        printk(KERN_ERR "Phonebook: invalid user data format (failed to parse the phone number)\n");
        kfree(to_split);
        return user;
    }
    user.email = strsep(&to_split, " ");
    if (user.email == NULL) {
        printk(KERN_ERR "Phonebook: invalid user data format (failed to parse the email adress)\n");
        kfree(to_split);
        return user;
    }

    age_str = strsep(&to_split, " ");
    if (age_str == NULL) {
        printk(KERN_ERR "Phonebook: invalid user data format (failed to parse the age)\n");
        kfree(to_split);
        return user;
    }

    if (kstrtol(age_str, 10, &age) != 0) {
        printk(KERN_ERR "Phonebook: invalid user data format (age should be a number)\n");
        kfree(to_split);
        return user;
    }

    user.age = age;
    user.to_split = to_split;
    user.successfully_created = 1;
    return user;
}

static ssize_t find_user(const char *surname) {
    size_t i;
    for (i = 0; i < users_count; i++) {
        if (strcmp(surname, users[i].surname) == 0)
            return i;
    }

    return -1;
}

static int add_user(const struct User user) {
    if (users_count == MAX_USERS) {
        printk(KERN_ERR "Phonebook: no more space in the users array\n");
        return 1;
    }

    users[users_count++] = user;
    return 0;
}

static int remove_user(size_t index) {
    size_t i;

    if (index >= users_count) {
        printk(KERN_ERR "Phonebook: can't remove user #%zu -- there are %zu total users\n", index, users_count);
        return 1;
    }

    kfree(users[index].to_split);

    for (i = index; i < users_count - 1; i++)
        users[i] = users[i + 1];

    users_count--;
    return 0;
}

/*
* Format:
* f surname -- get all user data by surname (finds the first user with this surname)
* a name surname phone email age -- add a user
* d surname -- remove a user by surname (finds the first user with this surname)
*/
static int parse_user_buffer(void) {
    ssize_t index;
    struct User user;

    if (user_msg_size < 3) { // command char, space, first char of the argument
        printk(KERN_ERR "Phonebook: failed to parse the user buffer -- not enough arguments\n");
        return 1;
    }

    switch (user_buffer[0]) { // command char
    case 'f':
        index = find_user(user_buffer + 2); // skip the first 2 chars
        if (index == -1) {
            printk(KERN_ERR "Phonebook: failed to parse the user buffer -- user not found in 'f'\n");
            return 1;
        }
        user = users[index];

        snprintf(
            device_buffer,
            BUFFER_SIZE,
            "%s %s %s %s %ld\n",
            user.name,
            user.surname,
            user.phone,
            user.email,
            user.age
        );
        device_msg_size = strlen(device_buffer);


        printk(
            KERN_INFO "Phonebook: found user %s\n",
            user_buffer + 2
        );
        break;
    case 'a':
        user = new_user(user_buffer + 2); // skip the first 2 chars
        if (!user.successfully_created) {
            printk(KERN_ERR "Phonebook: failed to parse the user buffer -- failed to create a new user\n");
            return 1;
        }
        if (add_user(user)) { // add_user returns 1 on error
            printk(KERN_ERR "Phonebook: failed to parse the user buffer -- failed to add the created user\n");
            return 1;
        }

        printk(
            KERN_INFO "Phonebook: new user -- '%s', '%s', '%s', '%s', '%ld'\n",
            user.name,
            user.surname,
            user.phone,
            user.email,
            user.age
        );
        break;
    case 'd':
        index = find_user(user_buffer + 2); // skip the first 2 chars
        if (index == -1) {
            printk(KERN_ERR "Phonebook: failed to parse the user buffer -- user not found in 'd'\n");
            return 1;
        }
        if (remove_user(index)) { // remove_user returns 1 on error
            printk(KERN_ERR "Phonebook: failed to parse the user buffer -- failed to remove the user\n");
            return 1;
        }

        printk(
            KERN_INFO "Phonebook: removed user %s\n",
            user_buffer + 2
        );
        break;
    default:
        printk(KERN_ERR "Phonebook: failed to parse the user buffer -- unknown command\n");
        return 1;
    }

    printk(KERN_INFO "Phonebook: finished parsing the user buffer\n");
    return 0;
}

module_init(phonebook_init);
module_exit(phonebook_exit);
