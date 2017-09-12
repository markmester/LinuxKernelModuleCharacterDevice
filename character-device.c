/*=============================================================================================
 * Includes
 * ============================================================================================*/
#include <linux/module.h>  /* Needed by all kernel modules */
#include <linux/kernel.h>  /* Needed for loglevels (KERN_WARNING, KERN_EMERG, KERN_INFO, etc.) */
#include <linux/init.h>    /* Needed for __init and __exit macros. */
#include <linux/slab.h>    /* kmalloc */
#include <linux/device.h>  /* Needed to support the Kernel Driver Model*/
#include <linux/cdev.h>  /* Needed to support the Kernel Driver Model*/
#include <linux/fs.h>      /* Needed for Linux file system support*/
#include <linux/kdev_t.h>  /* Needed fir MAJOR, MINOR, & MKDEV macros for extracting maj/min numbers and creating dev*/
#include <linux/types.h>   /* Needed for dev_t type (major/minor numbers)*/
#include <asm/uaccess.h>   /* Needed for the copy to user function*/


/*=============================================================================================
 * Defines
 * ============================================================================================*/
#define DEVICE_NAME "chardev" /* Dev name as it appears in /proc/devices*/
#define CLASS_NAME "chardev" // Device class

/*============================================================================================
 * module globals
 * ===========================================================================================*/
static char message[256]; // memory for the string passed from userspace
static short size_of_message; // used to remember the size of the string passed from userspace
static int number_opens = 0; // counts number of times device is opened
static struct class *char_class = NULL; // device-driver class struct pointer (return val of device class create func)
static struct device *char_device = NULL; // device-driver device struct pointer (return val of device create func)
static struct cdev c_dev; // character device structure
static DEFINE_MUTEX(char_mutex); // macro to declare new mutex

/*===============================================================================================
 * prototypes for character driver -- must come before struct definitio
 * ==============================================================================================*/
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);
static dev_t dev_first; // first device number

/* ==============================================================================================
 * Devices are represented as file structure in the kernel. The file_operations structure from
 * /linux/fs.h lists the callback functions that you wish to associated with your file operations
 * using a C99 syntax structure.
 * ==============================================================================================*/
static struct file_operations fops = 
{
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

/*===============================================================================================
 * entry function
 * =============================================================================================*/
static int __init onload(void) { 
    int ret_val;
    printk(KERN_INFO "Initializing LKM\n"); // print to /var/log/syslog
    
    // attempt to dynamically allocate major number for device
    ret_val = alloc_chrdev_region(&dev_first, 0, 1, DEVICE_NAME); // dynamically allocate major/minor numbers
    if (ret_val < 0){
        printk(KERN_ALERT "Failed to allocate major/minor number\n");
        return -1;
    }
    printk(KERN_INFO "Registered correctly with major_no: %d Minor_no: %d\n", MAJOR(dev_first), MINOR(dev_first));

    // register device class
    char_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(char_class)){ // Check for error and clean up if there is
        unregister_chrdev_region(dev_first, 1);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(char_class); // Correct way to return an error on a pointer
    }
    printk(KERN_INFO "Device class registered correctly\n");

    // populate device info (<major, minor>) under created class
    char_device = device_create(char_class, NULL, dev_first, NULL, DEVICE_NAME);
    if (IS_ERR(char_device)){ // Clean up if there is an error
        class_destroy(char_class);
        unregister_chrdev_region(dev_first, 1);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(char_device);
    }
    printk(KERN_INFO "Device created correctly\n");

    cdev_init(&c_dev, &fops); // initialize cdev structure
    ret_val = cdev_add(&c_dev, dev_first, 1); // add char device to system
    if (ret_val < 0) {
        device_destroy(char_class, dev_first);
        class_destroy(char_class);
        unregister_chrdev_region(dev_first, 1);
        printk(KERN_ALERT "Failed to add char device to system\n");
        return PTR_ERR(char_device);
    }
    mutex_init(&char_mutex); // init mutex dynamically

    return 0;
}

/*===========================================================================================
 * Prototype Functions
 * ==========================================================================================*/

static int dev_open(struct inode *inodep, struct file *filep){
    if(!mutex_trylock(&char_mutex)) { // try to aquire a lock
        printk(KERN_ALERT "Device in in use by another process");
        return -EBUSY;
    }

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
    mutex_unlock(&char_mutex); // release mutex
    printk(KERN_INFO "Device successfully closed\n");
    return 0;
}

/*==============================================================================================*/
// exit function
static void __exit onunload(void) {
    printk(KERN_INFO "Deregistering LKM\n");
    mutex_destroy(&char_mutex); // destroy dynamically allocated mutex
    device_destroy(char_class, dev_first); // remove device
    class_destroy(char_class); // remove device class
    unregister_chrdev_region(dev_first, 1); // unregister device
    printk(KERN_INFO "Deregistered LKM\n");
}

/*==============================================================================================*/
// register entry/exit point functions
module_init(onload);
module_exit(onunload);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mark Mester <mmester@parrylabs.com>");
MODULE_DESCRIPTION("A loadable Linux kernel module for a char device");

