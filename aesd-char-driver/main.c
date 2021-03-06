/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#include "aesdchar.h"
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Rajat Chaple"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

/***********************************************************************************
 * aesd char driver function for (userspace) open call
 * *********************************************************************************/
int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev;

	PDEBUG("open");

	//Get pointer to our device
	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);

	//Once our device is found, store a pointer to it in private_data field
	filp->private_data = dev; /* for other methods */

	return 0;
}

/***********************************************************************************
 * aesd char driver function for (userspace) open call
 * *********************************************************************************/
int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release");
	return 0;
}

/***********************************************************************************
 * aesd char driver function for (userspace) read call
 * *********************************************************************************/
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
				loff_t *f_pos)
{
	struct aesd_buffer_entry *read_entry = NULL;
	size_t read_entry_off = 0;
	ssize_t rc = 0;ssize_t retval = 0;
	struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
	

	if (mutex_lock_interruptible(&dev->driver_lock) != 0)
		return -ERESTARTSYS;

	PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

	//handling read from circular buffer
	read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &read_entry_off);

	if (read_entry == 0)
	{
		*f_pos = 0;
		retval = 0;
		goto out;
	}

	retval = (read_entry->size - read_entry_off);

	if (count < retval)
	{
		retval = count;
	}

	//copying from kernel space to user space
	rc = copy_to_user(buf, (read_entry->buffptr + read_entry_off), retval);
	if (rc)
	{
		PDEBUG("copy_to_user %ld", rc);
		retval = -EFAULT;
		goto out;
	}

	*f_pos += retval;

out:
	mutex_unlock(&dev->driver_lock);
	return retval;
}

/***********************************************************************************
 * aesd char driver function for (userspace) writer call
 * *********************************************************************************/
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
				loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	ssize_t bytes_not_written = 0;
	char *newline = NULL;
	const char *entry = NULL;
	struct aesd_dev *dev = NULL;

	dev = filp->private_data;

	if ((mutex_lock_interruptible(&dev->driver_lock)) != 0)
		return -ERESTARTSYS;

	PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

	if (dev->entry.size == 0)
	{
		dev->entry.buffptr = kmalloc(count, GFP_KERNEL); //allocating memory for buffer string
		if (dev->entry.buffptr == 0)
		{
			retval = -ENOMEM;
			goto out;
		}
	}
	else
	{
		dev->entry.buffptr = krealloc(dev->entry.buffptr, dev->entry.size + count, GFP_KERNEL);
		if (dev->entry.buffptr == 0)
		{
			kfree((void *)dev->entry.buffptr);
			retval = -ENOMEM;
			goto out;
		}
	}

	//copying from user space to kernel space
	bytes_not_written = copy_from_user((void *)(&dev->entry.buffptr[dev->entry.size]), buf, count);
	if (bytes_not_written != 0)
	{
		PDEBUG("copy_from_user %ld", bytes_not_written);
	}
	retval = count - bytes_not_written;
	dev->entry.size += retval;

	newline = (char *)memchr(dev->entry.buffptr, '\n', dev->entry.size);
	if (newline != NULL)
	{
		//cicrcular buffer write
		entry = aesd_circular_buffer_add_entry(&(dev->buffer), &(dev->entry));
		if (entry != NULL)
		{
			kfree(entry);
		}

		dev->entry.buffptr = NULL;
		dev->entry.size = 0;
		newline = NULL;
	}

	*f_pos = 0;

out:
	mutex_unlock(&dev->driver_lock);
	return retval;
}
struct file_operations aesd_fops = {
	.owner = THIS_MODULE,
	.read = aesd_read,
	.write = aesd_write,
	.open = aesd_open,
	.release = aesd_release,
};

/***********************************************************************************
 * aesd char driver function for (userspace) cdev call
 * *********************************************************************************/
static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
	{
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}

/***********************************************************************************
 * aesd char driver module initialization
 * *********************************************************************************/
int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1,
								"aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0)
	{
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device, 0, sizeof(struct aesd_dev));

	//Initializing aesd driver specific data structures
	aesd_circular_buffer_init(&aesd_device.buffer);
	mutex_init(&aesd_device.driver_lock);

	result = aesd_setup_cdev(&aesd_device);

	if (result)
	{
		unregister_chrdev_region(dev, 1);
	}

	PDEBUG("AESD DRIVER MODULE INITIALIZED");

	return result;
}

/***********************************************************************************
 * aesd char driver module cleanup. Automatically called at module exit.
 * *********************************************************************************/
void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	aesd_cicular_buffer_free(&aesd_device.buffer);
	unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);