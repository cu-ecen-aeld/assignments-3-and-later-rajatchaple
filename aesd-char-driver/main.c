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


	AESD_CIRCULAR_BUFFER_FOREACH(entry,dev->buffer,index) 
	{
  		PDEBUG("|X|%s ",entry->buffptr);
 	 }

	AESD_CIRCULAR_BUFFER_FOREACH(entry, dev->buffer, index) 
	{	PDEBUG("trim 1");
		if(entry->buffptr != NULL)
  			{
				  kfree(entry->buffptr);
				  entry->buffptr = NULL;
			  }
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
		//aesd_trim(dev); /* ignore errors */	
		mutex_unlock(&dev->lock);
PDEBUG("open done");		
	}

	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev = filp->private_data; 
	//PDEBUG("release");
	/**
	 * TODO: handle release
	 */
	//free buffers
	// PDEBUG("release 1");
	if (mutex_lock_interruptible(&dev->lock))
			return -ERESTARTSYS;
	// PDEBUG("release 2");	
	//aesd_trim(dev);
	// PDEBUG("release 3");
	mutex_unlock(&dev->lock);
// PDEBUG("release done");
	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	//ssize_t retval = 0;
	size_t length_read = 0;
	ssize_t entry_offset_byte_rtn = 0;
	//size_t iEntry = 0;
	struct aesd_dev *dev = filp->private_data; 
	struct aesd_buffer_entry* entryptr = NULL;
	
	//char* read_buffer;
	//size_t read_size_offset = 0;
	int status;
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

	//return 0;
	
	
	/*		  in
	//abc def ghij klmnop 000 
	      out
		  (4-entry_offset)
		  if(count > ())
		  char_offset = 0;
		  read()*/
	
	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;
//PDEBUG("read 1");
	
		entryptr = aesd_circular_buffer_find_entry_offset_for_fpos(dev->buffer,
			*f_pos, &entry_offset_byte_rtn);
		if(entryptr == NULL)
			goto error;

		if(entryptr->size < (count + entry_offset_byte_rtn))	
		{
			length_read = entryptr->size - entry_offset_byte_rtn;
		}
		else
		{
			length_read = count;
		}

		
	
	/*
	while(count > 0)
	{	
		read_buffer = krealloc(read_buffer, (entryptr->size - entry_offset_byte_rtn) * sizeof(char), GFP_KERNEL);
		strncat((read_buffer + read_size_offset), (char*)&entryptr[((entry_offset_byte_rtn++) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)], (entryptr->size - entry_offset_byte_rtn));
		
		read_size_offset += (entryptr->size - entry_offset_byte_rtn);
		entry_offset_byte_rtn = 0;
		retval += (entryptr->size - entry_offset_byte_rtn);
		
		// retval += __copy_to_user(buf, &entryptr[entry_offset_byte_rtn], (entryptr->size - entry_offset_byte_rtn));
//PDEBUG("read 2");
		//  *f_pos += entryptr->size - entry_offset_byte_rtn;
		//PDEBUG("read 3");
		count -= entryptr->size - entry_offset_byte_rtn;
		PDEBUG("|R|%s f_pos:%lld s:%ld o:%ld c:%ld",read_buffer, *f_pos, entryptr->size, entry_offset_byte_rtn, count);
		//PDEBUG("read 4");
		dev->buffer->out_offs  = (dev->buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
		//PDEBUG("read 5");
		iEntry++;
		if((dev->buffer->entry[dev->buffer->out_offs].buffptr == NULL)	|| iEntry == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
			break;
	}	
	*/
	status = _copy_to_user(buf, (entryptr->buffptr + entry_offset_byte_rtn), length_read);
	if(status != 0)
		return -EFAULT;

	*f_pos += length_read;

	PDEBUG("Read : e: %s  a: %s (%ld bytes)", (entryptr->buffptr + entry_offset_byte_rtn), buf, length_read);

	//retval = *f_pos;

	/**
	 * TODO: handle read
	 */
error:	
	mutex_unlock(&dev->lock);
	return length_read;
}












/*******************************************************************************************
 * AESD write
 * 
 *******************************************************************************************/
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	char* buffer_from_user = (void*)buf;
		ssize_t retval = -ENOMEM;
	struct aesd_dev *dev = filp->private_data;
	//uint8_t index;
	//struct aesd_buffer_entry *entry;

	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	PDEBUG("writing %s",buf);
	if (mutex_lock_interruptible(&dev->lock))
		return -ERESTARTSYS;

	//PDEBUG("write 1");

	// PDEBUG("Before: dev->add_entry->buffptr %p , string %s",dev->add_entry->buffptr, dev->add_entry->buffptr);
	PDEBUG("Before: dev->add_entry->size %ld ", dev->add_entry->size);
	dev->add_entry->buffptr = krealloc((dev->add_entry->buffptr), (count) * sizeof(char), GFP_KERNEL);
	if(dev->add_entry->buffptr == NULL)
	{	PDEBUG("Could not realloc");
		goto out;
	}

	
	//dev->add_entry->buffptr +=  dev->add_entry->size;
	//PDEBUG("write 2");
	//memcpy(dev->add_entry, buf);
	//buffer_from_user[count] = '\0';
	retval = __copy_from_user (((char *)dev->add_entry->buffptr + dev->add_entry->size), buffer_from_user, count);

	PDEBUG("After: dev->add_entry->buffptr %p , string %s",dev->add_entry->buffptr, dev->add_entry->buffptr);
	PDEBUG("Afterr: dev->add_entry->size %ld ", dev->add_entry->size);
	//dev->add_entry->buffptr[count] = '\0';
	dev->add_entry->size += count;
	


	//PDEBUG("write 3");
	retval = dev->add_entry->size;
	if(memchr((char*)dev->add_entry->buffptr, '\n', dev->add_entry->size) != NULL)
	{
		PDEBUG("writing--------- %s", dev->add_entry->buffptr);
		PDEBUG("|Size of buffptr (main)|%ld ", dev->add_entry->size);
		aesd_circular_buffer_add_entry(dev->buffer, dev->add_entry);	//storing command into circular buffer
		PDEBUG("|W|%s ", dev->add_entry->buffptr);
		if (dev->add_entry->buffptr != NULL)
		{
			//kfree(dev->add_entry->buffptr);
		}
		dev->add_entry->buffptr = NULL;
		dev->add_entry->size = 0;
		//retval = count;
	}
	
	//PDEBUG("write 4");
out:
	mutex_unlock(&dev->lock);

/*PDEBUG("write 5");
	AESD_CIRCULAR_BUFFER_FOREACH(entry,dev->buffer,index) 
	{
  		PDEBUG("|_|%s ",entry->buffptr);
 	 }*/
//PDEBUG("write done");
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
	printk(KERN_WARNING "setup1");
	dev->cdev.owner = THIS_MODULE;
	printk(KERN_WARNING "setup2");
	dev->cdev.ops = &aesd_fops;
	printk(KERN_WARNING "setup3");
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
	aesd_device.add_entry = (struct aesd_buffer_entry*)kmalloc(sizeof(struct aesd_buffer_entry ), GFP_KERNEL);
	
	PDEBUG("init 2");
	aesd_device.buffer = (struct aesd_circular_buffer*)kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
	aesd_circular_buffer_init(aesd_device.buffer);

	

PDEBUG("init 3");
	result = aesd_setup_cdev(&aesd_device);
	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
PDEBUG("init done");
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
	aesd_trim(&aesd_device);
	kfree(aesd_device.buffer);
	aesd_device.buffer = NULL;
	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);