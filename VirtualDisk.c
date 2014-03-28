/* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/system.h> /* cli(), *_flags */
#include <asm/uaccess.h> /* copy_from/to_user */
#include <linux/device.h> /* class_creatre */
#include <linux/cdev.h> /* cdev_init */
//---------------------------------------------------------------------------
#define VIRTUALDISK_SIZE 0x200
#define MEM_CLEAR 0x1
#define PORT1_SET 0x2
#define PORT2_SET 0x3
#define VIRTUALDISK1_MAJOR 200
#define DEVICE1_NAME "VirtualDisk1"
#define CLASS_NAME "virtual_disk"
#define NODE1_NAME "VirtualDisk"


#define __DEBUG__

#ifdef __DEBUG__
    #define DEBUGI(format, ...) printk(KERN_INFO "[VirtualDisk]"format"\n",##__VA_ARGS__)
    #define DEBUGE(format, ...) printk(KERN_ERR "[VirtualDisk]"format"\n",##__VA_ARGS__)
#else
    #define DEBUGI(format, ...)
    #define DEBUGE(format, ...)
#endif

//for sysfs device node
struct class *virtual_disk_class = 0;

static int VirtualDisk1_major = VIRTUALDISK1_MAJOR;

//VirtualDisk device struct
struct VirtualDisk
{
  struct cdev cdev;
  unsigned char mem[VIRTUALDISK_SIZE];
  int port1;
  long port2;
  long count;      
};

static struct VirtualDisk *Virtualdisk_devp = 0;

//operation functions
int VirtualDisk_open(struct inode *inode, struct file *filp);
int VirtualDisk_release(struct inode *inode, struct file *filp);
ssize_t VirtualDisk_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos);
ssize_t VirtualDisk_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos);
loff_t VirtualDisk_llseek(struct file *filp, loff_t offset, int orig);
/*
* ioctl member field is removed from linux 2.6.11, new drivers could use 
* the improved interface (unlocked_ioctl/compat_ioctl). 
*/
//long VirtualDisk_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
long VirtualDisk_ioctl (struct file *flip, unsigned int cmd, unsigned long arg);


/*
* ioctl member field is removed from linux 2.6.11, new drivers could use 
* the improved interface (unlocked_ioctl/compat_ioctl). 
*/
static const struct file_operations VirtualDisk_fops = {
  
  .owner = THIS_MODULE,
  .llseek =  VirtualDisk_llseek,
  .read =  VirtualDisk_read,
  .write =  VirtualDisk_write,
  .unlocked_ioctl = VirtualDisk_ioctl,
  .compat_ioctl = VirtualDisk_ioctl,
  .open =  VirtualDisk_open,  
  .release =  VirtualDisk_release,  
    
};

static void VirtualDisk_setup_cdev(struct VirtualDisk *dev, int minor)
{
     int err;
     dev_t devno;
     
     DEBUGI("[%s] enter\n", __FUNCTION__);
     
     devno = MKDEV(VirtualDisk1_major, minor);
     DEBUGI("[%s] devno[%d]\n", __FUNCTION__, devno);
     
     /* populate sysfs entry */
     virtual_disk_class = class_create(THIS_MODULE, CLASS_NAME);
     
     cdev_init(&dev->cdev, &VirtualDisk_fops);
     dev->cdev.owner = THIS_MODULE;
     dev->cdev.ops = &VirtualDisk_fops;
     err = cdev_add(&dev->cdev, devno, 1);
     DEBUGI("[%s] cdev_add[%d]\n", __FUNCTION__, err);
     if(err) {
        DEBUGE("[%s] cdev_add fail\n", __FUNCTION__);
        return;
     }
     
     /* seend uevents to udev, so it'll create the /dev node*/
    device_create(virtual_disk_class, NULL, devno, NULL, NODE1_NAME);       
    
}

//operation function implement
int VirtualDisk_open(struct inode *inode, struct file *filp)
{
    struct VirtualDisk *devp = NULL;
    DEBUGI("[%s] enter\n", __FUNCTION__);
    filp->private_data = Virtualdisk_devp;
    devp = filp->private_data;
    devp->count++;
    
    return 0;
}

int VirtualDisk_release(struct inode *inode, struct file *filp)
{
    struct VirtualDisk *devp = filp->private_data;
    DEBUGI("[%s] enter\n", __FUNCTION__);    
    devp->count--;   
    return 0;    
}

ssize_t VirtualDisk_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct VirtualDisk *devp = filp->private_data;
    DEBUGI("[%s] enter\n", __FUNCTION__);
    if(p >= VIRTUALDISK_SIZE) {
        DEBUGE("[%s] Read error...\n", __FUNCTION__);
        return count ? -ENXIO : 0;      
    } 
    else {
        count = VIRTUALDISK_SIZE - p;    
    }    
            
    if(copy_to_user(buf, (void*)(devp->mem + p), count)) {
        DEBUGE("[%s] copy_to_user fail...\n", __FUNCTION__);    
        ret = -EFAULT;           
    }
    else {
        *ppos += count;
        ret = count;
            
        DEBUGI("[%s] read %lu bytes(s) from %lu\n", __FUNCTION__, count, p);
    }        
       
    return ret;    
}

ssize_t VirtualDisk_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    int ret = 0;
    unsigned int count = size;
    struct VirtualDisk *devp = filp->private_data;
    DEBUGI("[%s] enter\n", __FUNCTION__);
    if(p >= VIRTUALDISK_SIZE) {
        DEBUGE("[%s] Read error...\n", __FUNCTION__);
        return count ? -ENXIO : 0;      
    } 
    else {
        count = VIRTUALDISK_SIZE - p;    
    }  
    
    if(copy_from_user(devp->mem + p, buf, count)) {
        DEBUGE("[%s] copy_to_user fail...\n", __FUNCTION__);    
        ret = -EFAULT;           
    }
    else {
        *ppos += count;
        ret = count;
            
        DEBUGI("[%s] written %lu bytes(s) from %lu\n", __FUNCTION__, count, p);
    }
    
    
    return ret;    
}

loff_t VirtualDisk_llseek(struct file *filp, loff_t offset, int orig)
{
       
    loff_t ret = 0;
    DEBUGI("[%s] enter\n", __FUNCTION__);
    
    switch(orig) {
    case SEEK_SET:
        if(offset < 0) {
            DEBUGE("[%s] Offset is nagtive\n", __FUNCTION__);
            ret = -EINVAL;
            break;   
        }
        if((unsigned int)offset > VIRTUALDISK_SIZE) {
            DEBUGE("[%s] Offset is too larger\n", __FUNCTION__);
            ret = -EINVAL;
            break;   
        }
        filp->f_pos = (unsigned int)offset;    
        ret = filp->f_pos;    
        break;
            
    case SEEK_CUR:    
        if((unsigned int)offset > VIRTUALDISK_SIZE) {
            DEBUGE("[%s] Offset is too larger\n", __FUNCTION__);
            ret = -EINVAL;
            break;   
        }
        if((filp->f_pos + offset) < 0) {
            DEBUGE("[%s] invalid pointer\n", __FUNCTION__);
            ret = -EINVAL;
            break;   
        }
        filp->f_pos += offset;
        ret = filp->f_pos;        
        break;
        
    default:
        DEBUGE("[%s] default: invalid pointer\n", __FUNCTION__);
        ret = -EINVAL;
        break;    
    }
    
    return ret;    
}

/*
* ioctl member field is removed from linux 2.6.11, new drivers could use 
* the improved interface (unlocked_ioctl/compat_ioctl). 
*/
//int VirtualDisk_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
long VirtualDisk_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    struct VirtualDisk *devp = filp->private_data;
    DEBUGI("[%s] enter\n", __FUNCTION__);
    switch(cmd) {
    case MEM_CLEAR:
        memset(devp->mem, 0, VIRTUALDISK_SIZE);
        break;
    case PORT1_SET:
        devp->port1 = 0;
        break;
    case PORT2_SET:
        devp->port2 = 0;
        break;            
    default:
        DEBUGE("[%s] default: no oerator\n", __FUNCTION__);
        ret = -EINVAL;
        break;    
    }
    return ret;    
}

static int __init 
VirtualDisk_init(void)
{
    int result = 0;
    dev_t devno;
    DEBUGI("[%s] enter\n", __FUNCTION__);
    
    devno = MKDEV(VirtualDisk1_major, 0);
    
    if(VirtualDisk1_major) {
        result = register_chrdev_region(devno, 1, DEVICE1_NAME);   
    }
    else {
        result = alloc_chrdev_region(&devno, 0, 1, DEVICE1_NAME);
        VirtualDisk1_major = MAJOR(devno);        
    }
    
    if(result < 0) {
        DEBUGE("[%s] register fail...\n", __FUNCTION__);
        return result;   
    }    
    
    Virtualdisk_devp = kmalloc(sizeof(struct VirtualDisk), GFP_KERNEL);
    
    if(!Virtualdisk_devp) {
        DEBUGE("[%s] kmalloc fail...\n", __FUNCTION__);
        result =  -ENOMEM;
        goto fail_malloc;
    }

    VirtualDisk_setup_cdev(Virtualdisk_devp, 0);
    return 0;

fail_malloc:
    unregister_chrdev_region(devno, 1);       
    return result;
}

static void __exit 
VirtualDisk_exit(void)
{
    DEBUGI("[%s] enter\n", __FUNCTION__);
    
    cdev_del(&Virtualdisk_devp->cdev);
    kfree(Virtualdisk_devp);
    unregister_chrdev_region(MKDEV(VirtualDisk1_major, 0), 1);
    
    device_destroy(virtual_disk_class, MKDEV(VirtualDisk1_major, 0));
    class_destroy(virtual_disk_class);
    
}

//---------------------------------------------------------------------------
/* Declaration of the init and exit functions */
module_init(VirtualDisk_init);
module_exit(VirtualDisk_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Adam Chen <xxx@xxx.com.tw>");
MODULE_DESCRIPTION("VirtualDisk driver module");