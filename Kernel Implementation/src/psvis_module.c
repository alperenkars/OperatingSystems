#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/slab.h>

#define MAX_PID_LENGTH 10
#define MAX_NAME_LENGTH 50
#define MAX_BUFFER_SIZE 1024

MODULE_LICENSE("GPL");
MODULE_AUTHOR("akars20");
MODULE_DESCRIPTION("psvis kernel module");

struct process_info {
    pid_t pid;
    char name[MAX_NAME_LENGTH];
    struct timespec start_time;
    struct list_head list;
};

static struct list_head processes;

static int __init psvis_module_init(void) {
    printk(KERN_INFO "psvis_module: Module loaded.\n");
    INIT_LIST_HEAD(&processes);
    return 0;
}

static void __exit psvis_module_exit(void) {
    struct process_info *info, *tmp;
    
    list_for_each_entry_safe(info, tmp, &processes, list) {
        list_del(&info->list);
        kfree(info);
    }
    
    printk(KERN_INFO "psvis_module: Module unloaded.\n");
}

static ssize_t psvis_read(struct file *file, char __user *buffer, size_t count, loff_t *offset) {
    char temp_buffer[MAX_BUFFER_SIZE];
    struct process_info *info;
    ssize_t bytes_written = 0;
    
    list_for_each_entry(info, &processes, list) {
        snprintf(temp_buffer, MAX_BUFFER_SIZE, "PID: %d, Name: %s, Start Time: %lu\n",
                 info->pid, info->name, info->start_time.tv_sec);
        int temp_len = strlen(temp_buffer);
        if (bytes_written + temp_len <= count) {
            if (copy_to_user(buffer + bytes_written, temp_buffer, temp_len) != 0) {
                return -EFAULT;
            }
            bytes_written += temp_len;
        } else {
            break;
        }
    }
    
    return bytes_written;
}

static struct file_operations psvis_fops = {
    .read = psvis_read,
};

module_init(psvis_module_init);
module_exit(psvis_module_exit);

