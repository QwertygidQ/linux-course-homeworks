// SPDX-License-Identifier: GPL-2.0-only

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/types.h>

#include <asm/atomic.h>

#define TIMER_EXPIRES 60000  // msec

MODULE_LICENSE("GPL");

static struct timer_list timer;
static atomic_t keys_pressed = ATOMIC_INIT(0);

static void reset_keys_counter(struct timer_list *current_timer) {
    printk(KERN_INFO "Keyboard stats module: %d keys pressed in the last minute.", atomic_read(&keys_pressed));
    atomic_set(&keys_pressed, 0);
    mod_timer(&timer, jiffies + msecs_to_jiffies(TIMER_EXPIRES));
}

static irqreturn_t irq_handler(int irq, void *dev_id, struct pt_regs *regs) {
    atomic_inc(&keys_pressed);
    return IRQ_HANDLED;
}

static int __init keyboard_stats_init(void) {
    int err;
    printk(KERN_INFO "Keyboard stats module: initializing.\n");

    timer_setup(&timer, reset_keys_counter, 0);
    mod_timer(&timer, jiffies + msecs_to_jiffies(TIMER_EXPIRES));

    err = request_irq(1, (irq_handler_t) irq_handler, IRQF_SHARED, "keyboard_stats", (void *) irq_handler);
    if (err == -EBUSY) {  // If the IRQ line is busy, free it and try again
        free_irq(1, NULL);
        return request_irq(1, (irq_handler_t) irq_handler, IRQF_SHARED, "keyboard_stats", (void *) irq_handler);
    }

    return err;
}

static void __exit keyboard_stats_exit(void) {
    printk(KERN_INFO "Keyboard stats module: exiting.\n");
    free_irq(1, (void *) irq_handler);
    del_timer(&timer);
}

module_init(keyboard_stats_init);
module_exit(keyboard_stats_exit);
