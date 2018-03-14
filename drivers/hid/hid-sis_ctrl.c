/*
 *  Character device driver for SIS multitouch panels control
 *
 *  Copyright (c) 2009 SIS, Ltd.
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/hid.h>
#include <linux/module.h>
#include <linux/usb.h>
#include "usbhid/usbhid.h"
#include <linux/init.h>

//update FW
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>	//copy_from_user() & copy_to_user()

#include "hid-ids.h"
#include "hid-sis_ctrl.h"

struct sis_device_data{
	dev_t	dev;
	bool	activated;
        int 	char_major;
	char 	device_name[50];
        struct  hid_device *hid_dev_backup;
        struct  urb *urb_backup;
	struct 	class *char_class;
        struct 	cdev char_cdev;
};
static unsigned device_count = 0;
static struct sis_device_data device_data[MAX_DEVICE];
static int sis_char_devs_count = 1;        /* device count */

#ifdef CONFIG_DEBUG_HID_HP_UPDATE_FW
	#define DBG_FW(fmt, arg...)	printk( fmt, ##arg )
	void sis_dbg_dump_array( u8 *ptr, u32 size)
	{
		u32 i;
		for (i=0; i<size; i++)  
		{
			DBG_FW ("%02X ", ptr[i]);
			if( ((i+1)&0xF) == 0)
				DBG_FW ("\n");
		}
		if( size & 0xF)
			DBG_FW ("\n");
	}
#else
	#define DBG_FW(...)
	#define sis_dbg_dump_array(...)
#endif	// CONFIG_DEBUG_HID_HP_UPDATE_FW

static ssize_t sis_char_path_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int id;
	for (id = 0; id < MAX_DEVICE; id++) {
		if (&device_data[id].hid_dev_backup->dev == dev)
			break;
	}
	printk(KERN_INFO "device %s\n", device_data[id].device_name);

	return scnprintf(buf, PAGE_SIZE, "/dev/%s\n", device_data[id].device_name);
}

static DEVICE_ATTR(char_path, S_IWUSR | S_IRUGO, sis_char_path_show, NULL);

static struct attribute *sysfs_attrs[] = {
	&dev_attr_char_path.attr,
	NULL
};

static const struct attribute_group sis_attribute_group = {
	.attrs = sysfs_attrs
};

int sis_cdev_open(struct inode *inode, struct file *filp)	//20120306 Yuger ioctl for tool
{
	int curr;
	struct usbhid_device *usbhid; 

	DBG_FW( "%s\n" , __FUNCTION__ );
	printk(KERN_INFO "sis_cdev_open\n");
	
	for(curr = 0; curr < MAX_DEVICE; curr++){
		if(MAJOR(device_data[curr].dev) == MAJOR(inode->i_rdev))
			break;
	}

	if ( !device_data[curr].hid_dev_backup ){
		printk( KERN_INFO "sis_cdev_open : (stop)hid_dev_backup is not initialized yet" );
		return -1;
	}

	usbhid = device_data[curr].hid_dev_backup->driver_data;


	if( !usbhid ){
		printk( KERN_INFO "sis_cdev_open : (stop)usbhid is not initialized yet" );
		return -1;
	}
	else if ( !usbhid->urbin ){
		printk( KERN_INFO "sis_cdev_open : (stop)usbhid->urbin is not initialized yet" );
		return -1;
	}
	else if (device_data[curr].hid_dev_backup->vendor == USB_VENDOR_ID_SIS_TOUCH){
		usb_fill_int_urb(device_data[curr].urb_backup, usbhid->urbin->dev, usbhid->urbin->pipe,
			usbhid->urbin->transfer_buffer, usbhid->urbin->transfer_buffer_length,
			usbhid->urbin->complete, usbhid->urbin->context, usbhid->urbin->interval);

                clear_bit( HID_STARTED, &usbhid->iofl );
                set_bit( HID_DISCONNECTED, &usbhid->iofl );

                usb_kill_urb( usbhid->urbin );
                usb_free_urb( usbhid->urbin );
                usbhid->urbin = NULL;
		return 0;
	}
        else{
		printk (KERN_INFO "This is not a SiS device");
		return -801;
	}
}

int sis_cdev_release(struct inode *inode, struct file *filp)
{
	int curr, ret;
	struct usbhid_device *usbhid;
	unsigned long flags;
	
	DBG_FW( "%s: " , __FUNCTION__ );
	printk(KERN_INFO "sis_cdev_release");
	
	for(curr = 0; curr < MAX_DEVICE; curr++){
		if(MAJOR(device_data[curr].dev) == MAJOR(inode->i_rdev))
			break;
	}

	if ( !device_data[curr].hid_dev_backup ){
		printk( KERN_INFO "sis_cdev_release : hid_dev_backup is not initialized yet" );
		return -1;
	}

	usbhid = device_data[curr].hid_dev_backup->driver_data;


	if( !usbhid ){
		printk( KERN_INFO "sis_cdev_release : usbhid is not initialized yet" );
		return -1;
	}

	if( !device_data[curr].urb_backup ){
		printk( KERN_INFO "sis_cdev_release : backup_urb is not initialized yet" );
		return -1;
	}

	clear_bit( HID_DISCONNECTED, &usbhid->iofl );
	usbhid->urbin = usb_alloc_urb( 0, GFP_KERNEL );

	if( !device_data[curr].urb_backup->interval ){
		printk( KERN_INFO "sis_cdev_release : backup_urb->interval does not exist" );
		return -1;
	}

	usb_fill_int_urb(usbhid->urbin, device_data[curr].urb_backup->dev, device_data[curr].urb_backup->pipe, 
		device_data[curr].urb_backup->transfer_buffer, device_data[curr].urb_backup->transfer_buffer_length, 
		device_data[curr].urb_backup->complete, device_data[curr].urb_backup->context, device_data[curr].urb_backup->interval);
	usbhid->urbin->transfer_dma = usbhid->inbuf_dma;
	usbhid->urbin->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	set_bit( HID_STARTED, &usbhid->iofl );

	//method at hid_start_in
	spin_lock_irqsave( &usbhid->lock, flags );		
	ret = usb_submit_urb( usbhid->urbin, GFP_ATOMIC );
	spin_unlock_irqrestore( &usbhid->lock, flags );

	DBG_FW( "ret = %d", ret );

	return ret;
}

ssize_t sis_cdev_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int actual_length = 0, timeout = 0;
	u8 *rep_data = NULL;
	u16 size = 0;
	long rep_ret;
	int curr;
        struct usb_interface *intf;
        struct usb_device *dev;

	for(curr = 0; curr < MAX_DEVICE; curr++){
		if(MAJOR(device_data[curr].dev) == MAJOR(file->f_path.dentry->d_inode->i_rdev))
			break;
	}
	intf = to_usb_interface(device_data[curr].hid_dev_backup->dev.parent);
	dev = interface_to_usbdev(intf);

	DBG_FW( "%s\n" , __FUNCTION__ );
	
	size = (((u16)(buf[64] & 0xff)) << 24) + (((u16)(buf[65] & 0xff)) << 16) + 
		(((u16)(buf[66] & 0xff)) << 8) + (u16)(buf[67] & 0xff);
	timeout = (((int)(buf[68] & 0xff)) << 24) + (((int)(buf[69] & 0xff)) << 16) + 
		(((int)(buf[70] & 0xff)) << 8) + (int)(buf[71] & 0xff);

	rep_data = kzalloc(size, GFP_KERNEL);
	if (!rep_data)
		return -12;

	if ( copy_from_user( rep_data, (void*)buf, size) ) 
	{
		printk( KERN_INFO "sis_cdev_read : copy_to_user fail\n" );
		//free allocated data
		kfree( rep_data );
		rep_data = NULL;
		return -19;
	}

	rep_ret = usb_interrupt_msg(dev, device_data[curr].urb_backup->pipe,
		rep_data, size, &actual_length, timeout);

	DBG_FW( "%s: rep_data = ", __FUNCTION__);
	sis_dbg_dump_array( rep_data, 8);
		
	if( rep_ret == 0 ){
		rep_ret = actual_length;
		if ( copy_to_user( (void*)buf, rep_data, rep_ret ) ){
			printk( KERN_INFO "sis_cdev_read : copy_to_user fail\n" );
			//free allocated data
			kfree( rep_data );
			rep_data = NULL;
			return -19;
		}
	}

	//free allocated data
	kfree( rep_data );
	rep_data = NULL;
	DBG_FW( "%s: rep_ret = %ld\n", __FUNCTION__,rep_ret );
	return rep_ret;
}

ssize_t sis_cdev_write( struct file *file, const char __user *buf, size_t count, loff_t *f_pos )
{
	int actual_length = 0;
	u8 *rep_data = NULL;
	u16 size;
	long rep_ret;
	int curr, timeout;
	struct usb_interface *intf;
        struct usb_device *dev;
        struct usbhid_device *usbhid;

	for(curr = 0; curr < MAX_DEVICE; curr++){
		if(MAJOR(device_data[curr].dev) == MAJOR(file->f_path.dentry->d_inode->i_rdev))
			break;
	}
	intf = to_usb_interface( device_data[curr].hid_dev_backup->dev.parent );
	dev = interface_to_usbdev( intf );
	usbhid = device_data[curr].hid_dev_backup->driver_data;

	size = (((u16)(buf[64] & 0xff)) << 24) + (((u16)(buf[65] & 0xff)) << 16) + 
		(((u16)(buf[66] & 0xff)) << 8) + (u16)(buf[67] & 0xff);
	timeout = (((int)(buf[68] & 0xff)) << 24) + (((int)(buf[69] & 0xff)) << 16) + 
		(((int)(buf[70] & 0xff)) << 8) + (int)(buf[71] & 0xff);
	
	DBG_FW( "%s: 817 method, " , __FUNCTION__ );
	DBG_FW("timeout = %d, size %d\n", timeout, size);

	rep_data = kzalloc(size, GFP_KERNEL);
	if (!rep_data)
		return -12;

	if ( copy_from_user( rep_data, (void*)buf, size) ){
		printk( KERN_INFO "sis_cdev_write : copy_to_user fail\n" );
		//free allocated data
		kfree( rep_data );
		rep_data = NULL;
		return -19;
	}

	rep_ret = usb_interrupt_msg( dev, usbhid->urbout->pipe,
		rep_data, size, &actual_length, timeout );
	
	DBG_FW( "%s: rep_data = ", __FUNCTION__);
	sis_dbg_dump_array( rep_data, size);
	
	if( rep_ret == 0 ){
		rep_ret = actual_length;
		if ( copy_to_user( (void*)buf, rep_data, rep_ret ) ){
			printk( KERN_INFO "copy_to_user fail\n" );
			//free allocated data
			kfree( rep_data );
			rep_data = NULL;
			return -19;
		}
	}
	DBG_FW( "%s: rep_ret = %ld\n", __FUNCTION__,rep_ret );
	
	//free allocated data
	kfree( rep_data );
	rep_data = NULL;

	DBG_FW( "End of sys_sis_HID_IO\n" );
	return rep_ret;
}


//for ioctl
static const struct file_operations sis_cdev_fops = {
	.owner	= THIS_MODULE,
	.read	= sis_cdev_read,
	.write	= sis_cdev_write,
	.open	= sis_cdev_open,
	.release= sis_cdev_release,
};

//for ioctl
int sis_setup_chardev(struct hid_device *hdev)
{
	int 	alloc_ret = 0;
	int 	cdev_err = 0;
	int 	input_err = 0;
	struct 	device *class_dev = NULL;
	void 	*ptr_err;
	char 	device_name[50];
	int 	curr;
	char 	devnum[64];
    dev_t   dev;
	int	sys_err = 0;

	/* device_data update */ 
	if(device_count >= MAX_DEVICE){
		printk(KERN_INFO "sis_setup_chardev error : too much devices!");
		return -ENOMEM;
	}

	for(curr = 0; curr < MAX_DEVICE; curr++){
		if(device_data[curr].activated == false)
	 		break;
	}
	if(curr == MAX_DEVICE){
		printk(KERN_INFO "sis_setup_chardev error : no more inactivated devices!");
		return -ENOMEM;
	}
	dev = MKDEV(device_data[curr].char_major, 0);
	device_data[curr].hid_dev_backup = hdev;
	device_data[curr].urb_backup = usb_alloc_urb(0, GFP_KERNEL);
	if(!device_data[curr].urb_backup){
		dev_err(&hdev->dev, "Cannot allocate urb_backup\n");
		return -ENOMEM;
	}
	/* device_data update end */   
	
	printk("sis_setup_chardev.\n");
	
	/* dynamic allocate driver handle */
   
	if (hdev->product == USB_DEVICE_ID_SISF817_TOUCH)
		strcpy(device_name, SISF817_DEVICE_NAME);
	else
		strcpy(device_name, SIS817_DEVICE_NAME);

	alloc_ret = alloc_chrdev_region(&dev, 0, sis_char_devs_count, device_name);
	
	if (alloc_ret)
		goto error;
		
	device_data[curr].char_major = MAJOR(dev);
	cdev_init(&(device_data[curr].char_cdev), &sis_cdev_fops);
	device_data[curr].char_cdev.owner = THIS_MODULE;
	cdev_err = cdev_add(&(device_data[curr].char_cdev), dev, sis_char_devs_count);
	if (cdev_err) 
		goto error;

	device_data[curr].dev = dev;
	sprintf(devnum, "%d", MAJOR(dev));	//int to char
	strcat(device_name, devnum);	//add device number
	strcpy(device_data[curr].device_name, device_name);

	printk(KERN_INFO "%s driver(major %d) installed.\n", device_name,
                device_data[curr].char_major);

	// register class
	device_data[curr].char_class = class_create(THIS_MODULE, device_name);
	if(IS_ERR(ptr_err = device_data[curr].char_class)){
		goto err2;
	}

	class_dev = device_create(device_data[curr].char_class, NULL, dev, 
		NULL, device_name);
	if(IS_ERR(ptr_err = class_dev)){
		goto err;
	}

	/* device_data update */
	device_data[curr].activated = true;
	device_count++;
	printk(KERN_INFO "end of sis_setup_chardev : device_count = %d\n", device_count);

	/* create sysfs */
	sys_err = sysfs_create_group(&hdev->dev.kobj, &sis_attribute_group);
	if (sys_err)
		hid_warn(hdev, "can't create SiS sysfs attribute err: %d\n", sys_err);

	return 0;
error:
	if (cdev_err == 0)
		cdev_del(&(device_data[curr].char_cdev));
	if (alloc_ret == 0)
		unregister_chrdev_region(dev, sis_char_devs_count);
	if(input_err != 0){
		printk("sis_ts_bak error!\n");
	}
err:
	device_destroy(device_data[curr].char_class, dev);
err2:
	class_destroy(device_data[curr].char_class);
	return -1;
}
EXPORT_SYMBOL(sis_setup_chardev);

void sis_deinit_chardev(struct hid_device *hdev)
{
	//for ioctl
	int curr;
    dev_t dev;
	printk(KERN_INFO "sis_remove\n");
	
	for(curr = 0; curr < MAX_DEVICE; curr++){
		if (&device_data[curr].hid_dev_backup->dev == &hdev->dev)
			break;
	}
	sysfs_remove_group(&hdev->dev.kobj, &sis_attribute_group);

	dev = MKDEV(device_data[curr].char_major, 0);
	usb_kill_urb(device_data[curr].urb_backup);
	usb_free_urb(device_data[curr].urb_backup);
	device_data[curr].urb_backup = NULL;
	device_data[curr].activated = false;
	device_count--;
	printk("sis_deinit : id=%d Major=%d device_count = %d\n", 
		curr, device_data[curr].char_major, device_count);

	cdev_del(&(device_data[curr].char_cdev));
	unregister_chrdev_region(dev, MAX_DEVICE);
	device_destroy(device_data[curr].char_class, dev);
	class_destroy(device_data[curr].char_class);
}
EXPORT_SYMBOL(sis_deinit_chardev);

MODULE_DESCRIPTION("SiS 817 Touchscreen Control Driver");
MODULE_LICENSE("GPL");
