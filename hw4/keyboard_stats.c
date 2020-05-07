// SPDX-License-Identifier: GPL-2.0-only

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

irqreturn_t irq_handler(int irq, void *dev_id, struct pt_regs *regs) {
    printk(KERN_INFO "Keyboard stats module: key pressed.\n");
    return IRQ_HANDLED;
}

static int __init keyboard_stats_init(void) {
    int err;

    printk(KERN_INFO "Keyboard stats module: initializing...\n");
    err = request_irq(1, (irq_handler_t) irq_handler, IRQF_SHARED, "keyboard_stats", (void *) irq_handler);

    // If the IRQ line is busy, free it and try again
    if (err == -EBUSY) {
        free_irq(1, NULL);
        return request_irq(1, (irq_handler_t) irq_handler, IRQF_SHARED, "keyboard_stats", (void *) irq_handler);
    }

    return err;
}

static void __exit keyboard_stats_exit(void) {
    printk(KERN_INFO "Keyboard stats module: exiting...\n");
    free_irq(1, (void *) irq_handler);
}

module_init(keyboard_stats_init);
module_exit(keyboard_stats_exit);
