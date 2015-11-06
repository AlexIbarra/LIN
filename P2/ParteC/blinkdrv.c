/*
 * Simple driver for the Blinkstick Strip USB device
 *
 * Copyright (C) 2015 Juan Carlos Saez (jcsaezal@ucm.es)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 *
 * This driver is based on the sample driver found in the
 * Linux kernel sources  (drivers/usb/usb-skeleton.c) 
 * 
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Blinkstick");
MODULE_AUTHOR("Miguel Higuera Romero & Alejandro Nicolás Ibarra Loik");

/* Get a minor range for your devices from the usb maintainer */
#define USB_BLINK_MINOR_BASE	0 



/* Structure to hold all of our device specific stuff */
struct usb_blink {
	struct usb_device	*udev;			/* the usb device for this device */
	struct usb_interface	*interface;		/* the interface for this device */
	struct kref		kref;
};
#define to_blink_dev(d) container_of(d, struct usb_blink, kref)

static struct usb_driver blink_driver;

/* 
 * Free up the usb_blink structure and
 * decrement the usage count associated with the usb device 
 */
static void blink_delete(struct kref *kref)
{
	struct usb_blink *dev = to_blink_dev(kref);

	usb_put_dev(dev->udev);
	vfree(dev);
}

/* Called when a user program invokes the open() system call on the device */
static int blink_open(struct inode *inode, struct file *file)
{
	struct usb_blink *dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	subminor = iminor(inode);
	
	/* Obtain reference to USB interface from minor number */
	interface = usb_find_interface(&blink_driver, subminor);
	if (!interface) {
		pr_err("%s - error, can't find device for minor %d\n",
			__func__, subminor);
		return -ENODEV;
	}

	/* Obtain driver data associated with the USB interface */
	dev = usb_get_intfdata(interface);
	if (!dev)
		return -ENODEV;

	/* increment our usage count for the device */
	kref_get(&dev->kref);

	/* save our object in the file's private structure */
	file->private_data = dev;

	return retval;
}

/* Called when a user program invokes the close() system call on the device */
static int blink_release(struct inode *inode, struct file *file)
{
	struct usb_blink *dev;

	dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;

	/* decrement the count on our device */
	kref_put(&dev->kref, blink_delete);
	return 0;
}


#define NR_LEDS 8
#define NR_BYTES_BLINK_MSG 6
#define NR_SAMPLE_COLORS 4
#define BUF_LEN 100

unsigned int sample_colors[]={0x000011, 0x110000, 0x001100, 0x000000};

/* Called when a user program invokes the write() system call on the device */
static ssize_t blink_write(struct file *file, const char *user_buffer,
			  size_t len, loff_t *off)
{
	struct usb_blink *dev=file->private_data;
	int retval = 0;
	int i=0;
	unsigned char message[NR_BYTES_BLINK_MSG];
	static int color_cnt=0;
	unsigned int color;
	
	
	
	/* Modificaciones Parte C */
	char kbuf[BUF_LEN];
	int leds[8];
	unsigned int val[8];
	int i=0;
	
	// Ejemplo de ejecucion
	// echo 1:0x001100,3:0x000007,5:0x090000 > /dev/usb/blinkstick0
	
	
	if ((*off) > 0) /* Tell the application that there is nothing left to read */
	  return 0;

	if(len > BUF_LEN - 1)
	  return -ENOSPC;

	if (copy_from_user( &kbuf[0], user_buffer, len ))
	  return -EFAULT;

	kbuf[len] = '\0';
	*off += len;
	
	
	for(i=0; i<len, i+=11)
	{
	
	  // Recojo los valores de los leds correspondientes
	  if(sscanf(&kbuf[0]+i, "%i:%0x6", leds[i], val[i]))
	  {
	    
	    if(leds[i] != NULL && leds[i] >= 0 && leds[i] <= 7)
	    {
	      
	      /*
		* Como los bits que se le pasan por la enrtada no coinciden con los bits de los leds
		* hay que hacer una conversion de la entrada al valor correspondiente que ira a los leds
		* Ejemplo: 0x2 => 0x4
		* 
		status=0;
		
		for(i=0; i<3; i++)
		{
		  if(val & (0x1 << i))
		    
		    switch(val)
		      
		      case1:
			
		      case2:
		      
		      case3:
			
		      default:
			
		      status |= BIT_CORRESPONDIENTE
		}
		  
	      */
	      
	    }
	    else
	    {
	      return -EINVAL;
	    }
	    
	  }
	  else
	  {
	    return -EINVAL;
	  }
	
	}
	
	/*-----------------------*/
	
	
	

	/* Pick a color and get ready for the next invocation*/		
	color=sample_colors[color_cnt++];

	/* Reset the color counter if necessary */	
	if (color_cnt == NR_SAMPLE_COLORS)
		color_cnt=0;
	
	/* zero fill*/
	memset(message,0,NR_BYTES_BLINK_MSG);

	/* Fill up the message accordingly */
	message[0]='\x05';
	message[1]=0x00;
	message[2]=0; 
	message[3]=((color>>16) & 0xff);
 	message[4]=((color>>8) & 0xff);
 	message[5]=(color & 0xff);


	for (i=0;i<NR_LEDS;i++){

		message[2]=i; /* Change Led number in message */
	
		/* 
		 * Send message (URB) to the Blinkstick device 
		 * and wait for the operation to complete 
		 */
		retval=usb_control_msg(dev->udev,	
			 usb_sndctrlpipe(dev->udev,00), /* Specify endpoint #0 */
			 USB_REQ_SET_CONFIGURATION, 
			 USB_DIR_OUT| USB_TYPE_CLASS | USB_RECIP_DEVICE,
			 0x5,	/* wValue */
			 0, 	/* wIndex=Endpoint # */
			 message,	/* Pointer to the message */ 
			 NR_BYTES_BLINK_MSG, /* message's size in bytes */
			 0);		

		if (retval<0){
			printk(KERN_ALERT "Executed with retval=%d\n",retval);
			goto out_error;		
		}
	}

	(*off)+=len;
	return len;

out_error:
	return retval;	
}


/*
 * Operations associated with the character device 
 * exposed by driver
 * 
 */
static const struct file_operations blink_fops = {
	.owner =	THIS_MODULE,
	.write =	blink_write,	 	/* write() operation on the file */
	.open =		blink_open,			/* open() operation on the file */
	.release =	blink_release, 		/* close() operation on the file */
};

/* 
 * Return permissions and pattern enabling udev 
 * to create device file names under /dev
 * 
 * For each blinkstick connected device a character device file
 * named /dev/usb/blinkstick<N> will be created automatically  
 */
char* set_device_permissions(struct device *dev, umode_t *mode) 
{
	if (mode)
		(*mode)=0666; /* RW permissions */
 	return kasprintf(GFP_KERNEL, "usb/%s", dev_name(dev)); /* Return formatted string */
}


/*
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver blink_class = {
	.name =		"blinkstick%d",  /* Pattern used to create device files */	
	.devnode=	set_device_permissions,	
	.fops =		&blink_fops,
	.minor_base =	USB_BLINK_MINOR_BASE,
};

/*
 * Invoked when the USB core detects a new
 * blinkstick device connected to the system.
 */
static int blink_probe(struct usb_interface *interface,
		      const struct usb_device_id *id)
{
	struct usb_blink *dev;
	int retval = -ENOMEM;

	/*
 	 * Allocate memory for a usb_blink structure.
	 * This structure represents the device state.
	 * The driver assigns a separate structure to each blinkstick device
 	 *
	 */
	dev = vmalloc(sizeof(struct usb_blink));

	if (!dev) {
		dev_err(&interface->dev, "Out of memory\n");
		goto error;
	}

	/* Initialize the various fields in the usb_blink structure */
	kref_init(&dev->kref);
	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);

	/* we can register the device now, as it is ready */
	retval = usb_register_dev(interface, &blink_class);
	if (retval) {
		/* something prevented us from registering this driver */
		dev_err(&interface->dev,
			"Not able to get a minor for this device.\n");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	/* let the user know what node this device is now attached to */	
	dev_info(&interface->dev,
		 "Blinkstick device now attached to blinkstick-%d",
		 interface->minor);
	return 0;

error:
	if (dev)
		/* this frees up allocated memory */
		kref_put(&dev->kref, blink_delete);
	return retval;
}

/*
 * Invoked when a blinkstick device is 
 * disconnected from the system.
 */
static void blink_disconnect(struct usb_interface *interface)
{
	struct usb_blink *dev;
	int minor = interface->minor;

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	/* give back our minor */
	usb_deregister_dev(interface, &blink_class);

	/* prevent more I/O from starting */
	dev->interface = NULL;

	/* decrement our usage count */
	kref_put(&dev->kref, blink_delete);

	dev_info(&interface->dev, "Blinkstick device #%d has been disconnected", minor);
}

/* Define these values to match your devices */
#define BLINKSTICK_VENDOR_ID	0X20A0
#define BLINKSTICK_PRODUCT_ID	0X41E5

/* table of devices that work with this driver */
static const struct usb_device_id blink_table[] = {
	{ USB_DEVICE(BLINKSTICK_VENDOR_ID,  BLINKSTICK_PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, blink_table);

static struct usb_driver blink_driver = {
	.name =		"blinkstick",
	.probe =	blink_probe,
	.disconnect =	blink_disconnect,
	.id_table =	blink_table,
};

/* Module initialization */
int blinkdrv_module_init(void)
{
   return usb_register(&blink_driver);
}

/* Module cleanup function */
void blinkdrv_module_cleanup(void)
{
  usb_deregister(&blink_driver);
}

module_init(blinkdrv_module_init);
module_exit(blinkdrv_module_cleanup);
