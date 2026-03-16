#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include "proc_stats.h"
#include "fifo_buffer.h"
/*
Initialise the global counters to 0 for testing purposes
*/
int stat_total_sent     = 0;
int stat_total_received = 0;
int stat_total_blocked  = 0;
/*
 * stats_show - formats /proc/keycipher/stats output
 * called by the kernel when userspace reads the proc file
 * - use seq_printf to write each stat on its own line
 * - include: incoming_used, incoming_free, outgoing_used, outgoing_free
 *            chatroom_used, chatroom_free, total_sent, total_received, total_blocked
 * example output:
 *   incoming_used: 3
 *   incoming_free: 13
 *   chatroom_used: 2
 *   ...
 */
static int stats_show(struct seq_file *m, void *v)
{
    /* TODO: implement proc stats output */
    return 0;
}

/*
 * stats_open - called when /proc/keycipher/stats is opened
 * - use single_open(file, stats_show, NULL)
 */
static int stats_open(struct inode *inode, struct file *file)
{
    /* TODO: implement proc open */
    return 0;
}

static const struct proc_ops stats_ops = {
    .proc_open    = stats_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/*
 * proc_stats_init - create /proc/keycipher/ directory and stats file
 * - proc_mkdir("keycipher", NULL)
 * - proc_create("stats", 0444, proc_dir, &stats_ops)
 * returns 0 on success, -ENOMEM on failure
 */
int proc_stats_init(void)
{
    /* TODO: implement proc filesystem setup */
    return 0;
}

/*
 * proc_stats_exit - remove /proc/keycipher/stats and directory
 * - remove_proc_entry("stats", proc_dir)
 * - remove_proc_entry("keycipher", NULL)
 */
void proc_stats_exit(void)
{
    /* TODO: implement proc cleanup */
}
