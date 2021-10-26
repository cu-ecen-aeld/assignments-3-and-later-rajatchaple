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
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Rajat Chaple"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_trim(struct aesd_dev *dev)
{
    // struct scull_qset *next, *dptr;
    // int qset = dev->qset;   /* "dev" is not-null */
    // int i;

    // for (dptr = dev->data; dptr; dptr = next) { /* all the list items */
    //     if (dptr->data) {
    //         for (i = 0; i < qset; i++)
    //             kfree(dptr->data[i]);
    //         kfree(dptr->data);
    //         dptr->data = NULL;
    //     }
    //     next = dptr->next;
    //     kfree(dptr);
    // }
    // dev->size = 0;
    // dev->quantum = scull_quantum;
    // dev->qset = scull_qset;
    // dev->data = NULL;
    // return 0;
	uint8_t index;
	struct aesd_buffer_entry *entry;
	
	PDEBUG("trim");
	AESD_CIRCULAR_BUFFER_FOREACH(entry, dev->buffer, index) 
	{	PDEBUG("trim 1");
		if(entry->buffptr != NULL)
  			kfree(entry->buffptr);
		PDEBUG("trim 2");

 	 }

	return 0;
}


int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev; /* device information */

	PDEBUG("open");
	/**
	 * handling open
	 */
	//aesd_device.buffer = (struct aesd_circular_buffer*)kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);

	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev; /* for other methods */
PDEBUG("open 1");
	/* now trim to 0 the length of the device if open was write-only */
	if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
		if (mutex_lock_interruptible(&dev->lock))
			return -ERESTARTSYS;
			/** TODO: add function to clear all the buffers **/
PDEBUG("open 2");
		aesd_trim(dev); /* ignore errors */	
		mutex_unlock(&dev->lock);
PDEBUG("open done");		
	}

	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev = filp->private_data; 
	PDEBUG("release");
	/**
	 * TODO: handle release
	 */
	//free buffers
	PDEBUG("release 1");
	if (mutex_lock_interruptible(&dev->lock))
			return -ERESTARTSYS;
	PDEBUG("release 2");	
	aesd_trim(dev);
	PDEBUG("release 3");
	mutex_unlock(&dev->lock);

	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = 0;
	ssize_t entry_offset_byte_rtn = 0;
	struct aesd_dev *dev = filp->private_data; 
	struct aesd_buffer_entry* entryptr = NULL;
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	/*		  in
	//abc def ghij klmnop 000 
	      out
		  (4-entry_offset)
		  if(count > ())
		  char_offset = 0;
		  read()*/
	
	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;
PDEBUG("read 1");
	while(count > 0)
	{
		entryptr = aesd_circular_buffer_find_entry_offset_for_fpos(dev->buffer,
			*f_pos, &entry_offset_byte_rtn);
	
		retval += __copy_to_user(buf, &entryptr[entry_offset_byte_rtn], (entryptr->size - entry_offset_byte_rtn));
PDEBUG("read 2");
		*f_pos += entryptr->size - entry_offset_byte_rtn;
		count -= entryptr->size - entry_offset_byte_rtn;
		dev->buffer->out_offs  = (dev->buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
		if(dev->buffer->entry[dev->buffer->out_offs].buffptr == NULL)	
			break;
	}	


	/**
	 * TODO: handle read
	 */
PDEBUG("read 3");	
	
	mutex_unlock(&dev->lock);
	PDEBUG("read done");
	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	struct aesd_dev *dev = filp->private_data;
	uint8_t index;
	struct aesd_buffer_entry *entry;

	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	PDEBUG("writing %s",buf);
	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	PDEBUG("write 1");
	dev->buffer->entry->buffptr = krealloc(dev->buffer->entry->buffptr, count * sizeof(char), GFP_KERNEL);
	if(dev->buffer->entry->buffptr == NULL)
		goto out;

	PDEBUG("write 2");
	//memcpy(dev->add_entry, buf);
	retval = __copy_from_user ((char*)dev->buffer->entry->buffptr, buf, count);
	dev->buffer->entry->size += count;

	PDEBUG("write 3");
	if(memchr((char*)dev->buffer->entry->buffptr, '\n', dev->buffer->entry->size))
	{
		aesd_circular_buffer_add_entry(dev->buffer, dev->buffer->entry);	//storing command into circular buffer
		retval = count;
	}
	
	PDEBUG("write 4");
out:
	mutex_unlock(&dev->lock);

PDEBUG("write 5");
	AESD_CIRCULAR_BUFFER_FOREACH(entry,dev->buffer,index) 
	{
  		PDEBUG("|_|%s ",entry->buffptr);
 	 }

	return retval;
}

struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.read =     aesd_read,
	.write =    aesd_write,
	.open =     aesd_open,
	.release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);
	printk(KERN_WARNING "Inside aesd_setup_cdev");
	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}



int aesd_init_module(void)
{
	
	dev_t dev = 0;
	int result;


	printk(KERN_WARNING "Loading module AESDCHAR");
	PDEBUG("Loading module AESDCHAR");

	
	result = alloc_chrdev_region(&dev, aesd_minor, 1,
			"aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device,0,sizeof(struct aesd_dev));

	/**
	 * TODO: initialize the AESD specific portion of the device
	 */
		
	//aesd_device.add_entry->buffptr = NULL;	
	PDEBUG("init 1");
	aesd_device.buffer = (struct aesd_circular_buffer*)kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
	aesd_circular_buffer_init(aesd_device.buffer);
PDEBUG("init 2");
	result = aesd_setup_cdev(&aesd_device);
	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
PDEBUG("init 3");
	return result;

}

void aesd_cleanup_module(void)
{
	
	
	dev_t devno = MKDEV(aesd_major, aesd_minor);

PDEBUG("Exiting module AESDCHAR");

	cdev_del(&aesd_device.cdev);
	printk(KERN_WARNING "Exiting module AESDCHAR");

	/**
	 * TODO: cleanup AESD specific poritions here as necessary
	 */
	kfree(aesd_device.buffer);
	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
