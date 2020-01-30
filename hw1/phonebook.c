#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DEVICE_NAME     "phonebook_char"
#define CLASS_NAME      "phonebook"
#define MSG_BUFFER_SIZE 16

MODULE_LICENSE("GPL");

static int           major_number;
static char          message_buffer[MSG_BUFFER_SIZE] = {0};
static int           message_size;
static int           device_opened_count = 0;
static struct class  *phonebook_class = NULL;
static struct device *phonebook_device = NULL;

static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char __user *, size_t, loff_t *);

static struct file_operations fops = {
    .open = dev_open,
    .release = dev_release,
    .read = dev_read,
    .write = dev_write,
};

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

    printk(KERN_INFO "Phonebook: successfully initialized\n");
    return 0;
}

static void __exit phonebook_exit(void) {
    device_destroy(phonebook_class, MKDEV(major_number, 0));
    class_unregister(phonebook_class);
    class_destroy(phonebook_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    printk(KERN_INFO "Phonebook: successfully exited\n");
}

static int dev_open(struct inode *inode, struct file *file) {
    if (device_opened_count)
        return -EBUSY;

    device_opened_count++;
    try_module_get(THIS_MODULE);

    printk(KERN_INFO "Phonebook: device has been opened\n");
    return 0;
}

static int dev_release(struct inode *inode, struct file *file) {
    device_opened_count--;
    module_put(THIS_MODULE);

    printk(KERN_INFO "Phonebook: device has been closed\n");
    return 0;
}

// Data path: device -> user
static ssize_t dev_read(struct file *file, char __user *buffer, size_t len, loff_t *offset) {
    int error_count;

    int copy_len = min(message_size - *offset, len);
    if (copy_len <= 0)
        return 0;

    error_count = copy_to_user(buffer, message_buffer + *offset, copy_len);
    if (error_count != 0) {
        printk(KERN_ERR "Phonebook: failed to copy %d bytes to the user space, copy_len: %d\n", error_count, copy_len);
        return -EFAULT;
    }

    *offset += copy_len;
    printk(KERN_INFO "Phonebook: successfully copied the message (%d chars) to user space\n", copy_len);
    return copy_len;
}

// Data path: user -> device
static ssize_t dev_write(struct file *file, const char __user *buffer, size_t len, loff_t *offset) {
    int error_count;

    int copy_len = min(MSG_BUFFER_SIZE - *offset, len);
    if (copy_len <= 0) {
        if (MSG_BUFFER_SIZE == *offset)
            printk(KERN_INFO "Phonebook: no more space left in the buffer, ignoring\n");
        else
            printk(KERN_INFO "Phonebook: nothing more to copy from user space to device\n");

        return len;
    }

    error_count = copy_from_user(message_buffer + *offset, buffer, copy_len);
    if (error_count != 0) {
        printk(KERN_ERR "Phonebook: failed to copy %d bytes from the user space\n", error_count);
        return -EFAULT;
    }

    *offset += copy_len;
    message_size = *offset;
    printk(KERN_INFO "Phonebook: received %zu characters from the user, new message size: %d\n", len, message_size);
    return copy_len;
}

module_init(phonebook_init);
module_exit(phonebook_exit);
