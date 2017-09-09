#include <linux/module.h>  /* Needed by all kernel modules */
#include <linux/kernel.h>  /* Needed for loglevels (KERN_WARNING, KERN_EMERG, KERN_INFO, etc.) */
#include <linux/init.h>    /* Needed for __init and __exit macros. */
#include <linux/slab.h>    /* kmalloc */
#include <linux/device.h>  /* Needed to support the Kernel Driver Model*/
#include <linux/fs.h>      /* Needed for Linux file system support*/
#include <asm/uaccess.h>   /* Needed for the copy to user function*/

#define DEVICE_NAME "chardev" /* Dev name as it appears in /proc/devices*/
#define CLASS_NAME "chardev" // Device class

/*=============================================================================================*/
// module metadata
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mark Mester <mmester@parrylabs.com>");
MODULE_DESCRIPTION("A loadable Linux kernel module for a char device");

/*============================================================================================*/
// module globals
static int major_number; // stores the device number -- determined automatically
static char message[256]; // memory for the string passed from userspace
static short size_of_message; // used to remember the size of the string passed from userspace
static int number_opens = 0; // counts number of times device is opened
static struct class* char_class = NULL; // device-driver class struct pointer
static struct device* char_device = NULL; // device-driver device struct pointer

/*===============================================================================================*/
// prototypes for character driver -- must come before struct definition
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

/* Devices are represented as file structure in the kernel. The file_operations structure from
 * /linux/fs.h lists the callback functions that you wish to associated with your file operations
 * using a C99 syntax structure.
 */
static struct file_operations fops = 
{
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

/*===============================================================================================*/
// entry function
static int __init onload(void) {
    printk(KERN_INFO "Initializing LKM\n"); // print to /var/log/syslog

    // attempt to dynamically allocate major number for device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0){
        printk(KERN_ALERT "Failed to register a major number\n");
        return major_number;
    }
    printk(KERN_INFO "Registered correctly with major number %d\n", major_number);

    // register device class
    char_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(char_class)){ // Check for error and clean up if there is
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(char_class); // Correct way to return an error on a pointer
    }
    printk(KERN_INFO "Device class registered correctly\n");

    char_device = device_create(char_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(char_device)){ // Clean up if there is an error
        class_destroy(char_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(char_device);
    }
    printk(KERN_INFO "Device class created correctly\n");
    return 0;
}

/* Prototype Functions*/

static int dev_open(struct inode *inodep, struct file *filep){
    number_opens++;
    printk(KERN_INFO "Device has been opened %d times(s)\n", number_opens);
    return 0;
}

// read from device
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
    int error_count = 0;
    // copy_to_user has the format (* to, *from, size) and returns 0 on success
    error_count = copy_to_user(buffer, message, size_of_message);
    
    if (error_count ==  0){
        printk(KERN_INFO "Sent %d characters to the user\n", size_of_message);
        return (size_of_message=0); // clear the position to the start and return 0
    }
    else {
        printk(KERN_INFO "Failed to send %d characters to the user\n", error_count);
        return -EFAULT; // return a bad address message
    }
}

// write to device
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
    sprintf(message, "%s(%zu letters)", buffer, len);   // appending received string with its length
    size_of_message = strlen(message);                 // store the length of the stored message
    printk(KERN_INFO "Received %zu characters from the user\n", len);
    return len;
}

// release device
static int dev_release(struct inode *inodep, struct file *filep){
    printk(KERN_INFO "Device successfully closed\n");
    return 0;
}

/*==============================================================================================*/
// exit function
static void __exit onunload(void) {
    printk(KERN_INFO "Deregistering LKM\n");
    device_destroy(char_class, MKDEV(major_number, 0)); // remove device
    class_unregister(char_class); // unregister device class
    class_destroy(char_class); // remove device class
    printk(KERN_INFO "Deregistered LKM\n");
}

/*==============================================================================================*/
// register entry/exit point functions
module_init(onload);
module_exit(onunload);

