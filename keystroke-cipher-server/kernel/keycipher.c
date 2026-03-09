#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include "keycipher.h"
#include "fifo_buffer.h"
#include "proc_stats.h"
#include "cipher.h"
#include "input_intercept.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Team");
MODULE_DESCRIPTION("Kernel-level encrypted P2P messaging driver");
MODULE_VERSION("1.0");


#define MINOR_OUT 0
#define MINOR_IN  1
#define NUM_DEVS  2

static dev_t dev_base;
static struct cdev cdev_out, cdev_in;

struct keycipher_file_state {
    int mode; /* MODE_READ or MODE_WRITE */
};

static int keycipher_open(struct inode *inode, struct file *file)
{
    struct keycipher_file_state *state = kzalloc(sizeof(*state), GFP_KERNEL);
    if (!state)
        return -ENOMEM;

    state->mode = MODE_WRITE; /* default: encrypt outgoing */
    file->private_data = state;
    return 0;
}

static int keycipher_release(struct inode *inode, struct file *file)
{
    kfree(file->private_data);
    file->private_data = NULL;
    return 0;
}

/*
 * keycipher_read - reads one message from incoming_fifo, decrypts it, copies to userspace
 * blocks if incoming_fifo is empty (fifo_read does down on slots_used internally)
 */
static ssize_t keycipher_read(struct file *file, char __user *buf,
                               size_t len, loff_t *offset)
{
    struct keycipher_message msg;
    int ret;

    if (len < sizeof(msg))
        return -EINVAL;

    ret = fifo_read(&inbox_fifo, &msg);
    if (ret)
        return ret;

    rot13_decrypt(msg.data, msg.len);

    if (copy_to_user(buf, &msg, sizeof(msg)))
        return -EFAULT;

    return sizeof(msg);
}

/*
 * keycipher_write - copies one message from userspace, encrypts it, pushes to outgoing_fifo
 * blocks if outgoing_fifo is full (fifo_write does down on slots_free internally)
 */
static ssize_t keycipher_write(struct file *file, const char __user *buf,
                                size_t len, loff_t *offset)
{
    struct keycipher_message msg;
    int ret;

    if (len < sizeof(msg))
        return -EINVAL;

    if (copy_from_user(&msg, buf, sizeof(msg)))
        return -EFAULT;

    rot13_encrypt(msg.data, msg.len);

    ret = fifo_write(&outbox_fifo, &msg);
    if (ret)
        return ret;

    return sizeof(msg);
}

static long keycipher_ioctl(struct file *file, unsigned int cmd,
                             unsigned long arg)
{
    struct keycipher_file_state *state = file->private_data;

    switch (cmd) {
        case KEYCIPHER_SET_MODE_READ:
            state->mode = MODE_READ;
            break;
        case KEYCIPHER_SET_MODE_WRITE:
            state->mode = MODE_WRITE;
            break;
        case KEYCIPHER_FLUSH_IN:
            /* flush entire incoming FIFO — called when frontend clicks READ ALL */
            fifo_flush(&inbox_fifo);
            break;
        case KEYCIPHER_GET_STATS: {
            struct keycipher_stats stats;
            stats.incoming_used  = fifo_count(&inbox_fifo);
            stats.incoming_free  = FIFO_SIZE - stats.incoming_used;
            stats.outgoing_used  = fifo_count(&outbox_fifo);
            stats.outgoing_free  = FIFO_SIZE - stats.outgoing_used;
            stats.total_sent     = stat_total_sent;
            stats.total_received = stat_total_received;
            stats.total_decrypted = stat_total_blocked;
            if (copy_to_user((void __user *)arg, &stats, sizeof(stats)))
                return -EFAULT;
            break;
        }
        default:
            return -EINVAL;
    }
    return 0;
}

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = keycipher_open,
    .release        = keycipher_release,
    .read           = keycipher_read,
    .write          = keycipher_write,
    .unlocked_ioctl = keycipher_ioctl,
};

static int __init keycipher_init(void)
{
    int ret;
    ret = alloc_chrdev_region(&dev_base, 0, NUM_DEVS, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "KeyCipher: failed to allocate device numbers: %d\n", ret);
        return ret;
    }

    cdev_init(&cdev_out, &fops);
    cdev_out.owner = THIS_MODULE;
    ret = cdev_add(&cdev_out, MKDEV(MAJOR(dev_base), MINOR_OUT), 1);
    if (ret) {
        printk(KERN_ERR "KeyCipher: failed to add cdev_out: %d\n", ret);
        goto err_unreg;
    }

    
    cdev_init(&cdev_in, &fops);
    cdev_in.owner = THIS_MODULE;
    ret = cdev_add(&cdev_in, MKDEV(MAJOR(dev_base), MINOR_IN), 1);
    if (ret) {
        printk(KERN_ERR "KeyCipher: failed to add cdev_in: %d\n", ret);
        goto err_del_out;
    }

    /* Initialise both FIFOs inboxes */
    fifo_init(&inbox_fifo);
    fifo_init(&outbox_fifo);

    /* Create /proc/keycipher/stats */
    ret = proc_stats_init();
    if (ret) {
        printk(KERN_ERR "KeyCipher: failed to init proc stats: %d\n", ret);
        goto err_del_in;
    }

    /* Hook into input subsystem to start capturing keystrokes */
    ret = input_intercept_init();
    if (ret) {
        printk(KERN_ERR "KeyCipher: failed to init input intercept: %d\n", ret);
        goto err_proc;
    }

    /* Print major/minor numbers so load.sh can mknod the devices */
    printk(KERN_INFO "KeyCipher: loaded. major=%d minor_out=%d minor_in=%d\n",
           MAJOR(dev_base), MINOR_OUT, MINOR_IN);
    return 0;

err_proc:
    proc_stats_exit();
err_del_in:
    cdev_del(&cdev_in);
err_del_out:
    cdev_del(&cdev_out);
err_unreg:
    unregister_chrdev_region(dev_base, NUM_DEVS);
    return ret;
}

static void __exit keycipher_exit(void)
{
    input_intercept_exit();
    proc_stats_exit();
    cdev_del(&cdev_in);
    cdev_del(&cdev_out);
    unregister_chrdev_region(dev_base, NUM_DEVS);
    printk(KERN_INFO "KeyCipher: unloaded\n");
}

module_init(keycipher_init);
module_exit(keycipher_exit);