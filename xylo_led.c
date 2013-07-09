#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/delay.h>

MODULE_DESCRIPTION("Xylo USB driver");
MODULE_LICENSE("GPL");

#define BUF_SIZE	16

#define VENDOR_ID	0x04B4
#define PRODUCT_ID	0x8613

/*
 * Data for a xylo device
 */
struct usb_xylo_led {
  struct usb_device *udev;
  int ledmask;
};

static struct usb_driver xylo_led_driver;

/*
 * List of devices that work with this driver
 */
static struct usb_device_id id_table [] = {
  { USB_DEVICE(VENDOR_ID, PRODUCT_ID) },
  {}
};

/* Declare the list of devices */
MODULE_DEVICE_TABLE (usb, id_table);

/**
 ** @brief Send a bulk message to the xylo card to update its ledmask
 **
 ** @param dev Device for which the ledmask should be updated
 **
 ** @returns return value of usb_bulk_msg
 */
int xylo_led_send_bulk_ledmask(struct usb_xylo_led *dev)
{
  int actual_length = 0;

  /*
   * Send bulk message
   */
  return usb_bulk_msg(dev->udev,
		      usb_sndbulkpipe(dev->udev, 2),
		      &dev->ledmask,
		      1,
		      &actual_length,
		      2 * HZ);
}

/**
 ** @brief Change the ledmask with the given char value,
 ** and send the bulk message to update the ledmask in the card.
 **
 ** @param dev Device for which to change the ledmask
 ** @param mask Mask to be set
 **
 ** @returns return value of xylo_led_send_bulk_ledmask
 */
int xylo_led_send_bulk_ledmask_char(struct usb_xylo_led *dev,
				    unsigned char mask)
{
  int ret = 0;

  dev->ledmask = mask;
  ret = xylo_led_send_bulk_ledmask(dev);

  if (ret < 0)
    printk (KERN_WARNING "xylo_led_send_bulk_ledmask_char: usb_bulk_msg() error %d\n",
	    ret);

  return ret;
}

/**
 ** @brief This function sets the xylo led mask given in the buf string,
 ** and sends the bulk message to the xylo card.
 **
 ** @param dev device for which to set the led mask
 ** @param buf string containing the mask in hex format
 **
 ** @returns 0 on success.
 */
int xylo_led_send_bulk_ledmask_buf (struct usb_xylo_led *dev,
				    const char *buf)
{
  int ret = 0;
  int mask = 0;

  /*
   * Convert the string to a char
   */
  sscanf (buf, "%x", &mask);

  /*
   * Update the mask for the device
   */
  dev->ledmask = (unsigned char) mask;

  /*
   * Send the bulk message to update the mask
   * in the card
   */
  ret = xylo_led_send_bulk_ledmask(dev);

  /*
   * If an error occurs, display a warning message
   */
  if (ret < 0)
    printk (KERN_WARNING "xylo_led_send_bulk_ledmask_buf: usb_bulk_msg() error %d\n",
	    ret);

  return ret;
}

/**
 ** @brief Display a small animation on the card to show that
 ** it is recognized.
 **
 ** @param dev Device for which to show the animation.
 */
void xylo_led_animation(struct usb_xylo_led *dev)
{
  char k = 0;

  while (k < 8)
  {
    xylo_led_send_bulk_ledmask_char(dev, k);
    msleep(50);
    k++;
  }
  for (k = 0; k < 4; k++)
  {
    xylo_led_send_bulk_ledmask_char(dev, 0xff * (k % 2));
    msleep(50);
  }
}

/**
 ** @brief Send several packets in order to initiate communication
 ** with the xylo card.
 **
 */
static int xylo_led_init_xylo_card(struct usb_xylo_led *udev)
{
  int ret = 0;

  /*
   * Contains the code to be sent tothe xylo card
   */
  char packets[11][16] = {
    {0x75, 0x81, 0x5f, 0x90, 0xe6, 0x00, 0x74, 0x0a, 0xf0, 0x90, 0xe6, 0x7a, 0x74, 0x01, 0xf0, 0x11},
    {0x9b, 0x90, 0xe6, 0x18, 0x74, 0x10, 0xf0, 0x11, 0x9b, 0x90, 0xe6, 0x19, 0x74, 0x10, 0xf0, 0x11},
    {0x9b, 0x90, 0xe6, 0x1a, 0x74, 0x0c, 0xf0, 0x11, 0x9b, 0x90, 0xe6, 0x1b, 0x74, 0x0c, 0xf0, 0x11},
    {0x9b, 0x90, 0xe6, 0x02, 0x74, 0x98, 0xf0, 0x11, 0x9b, 0x90, 0xe6, 0x03, 0x74, 0xfe, 0xf0, 0x90},
    {0xe6, 0x70, 0x74, 0x80, 0xf0, 0x11, 0x9b, 0x90, 0xe6, 0x01, 0x74, 0x03, 0xf0, 0x90, 0xe6, 0x8d},
    {0xf0, 0xe5, 0xba, 0x20, 0xe1, 0xfb, 0x90, 0xe6, 0x8d, 0xe0, 0x60, 0x25, 0x90, 0xe7, 0x80, 0xb4},
    {0x04, 0x27, 0xe0, 0xf5, 0xb2, 0xa3, 0xe0, 0xf5, 0xb5, 0xa3, 0xe0, 0xf5, 0xb0, 0xa3, 0xe0, 0x90},
    {0xe6, 0x09, 0xf0, 0x90, 0xe7, 0xc0, 0xe5, 0xb0, 0xf0, 0x90, 0xe6, 0x8f, 0x74, 0x01, 0xf0, 0x80},
    {0xcc, 0x90, 0xe7, 0xc0, 0xe5, 0xaa, 0xf0, 0x80, 0xf0, 0xff, 0xe0, 0xa3, 0x7e, 0x08, 0x13, 0x92},
    {0x80, 0xc2, 0x81, 0xd2, 0x81, 0xde, 0xf7, 0xdf, 0xf1, 0x80, 0xb2, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22}
  };

  /*
   * Switch off the card
   */
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0xe600, 0x0000, "\x1", 0x0001, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0x0000, 0x0000, packets[0], 0x0010, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0x0010, 0x0000, packets[1], 0x0010, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0x0020, 0x0000, packets[2], 0x0010, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0x0030, 0x0000, packets[3], 0x0010, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0x0040, 0x0000, packets[4], 0x0010, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0x0050, 0x0000, packets[5], 0x0010, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0x0060, 0x0000, packets[6], 0x0010, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0x0070, 0x0000, packets[7], 0x0010, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0x0080, 0x0000, packets[8], 0x0010, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0x0090, 0x0000, packets[9], 0x0010, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0x00a0, 0x0000, packets[10], 0x000b, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);

  /*
   * Switch the card back on
   */
  if ((ret = usb_control_msg(udev->udev, usb_sndctrlpipe(udev->udev,0), 0xa0,
                             0x40, 0xe600, 0x0000, "\x0", 0x0001, 1000)))
    printk("xylo_led_init_xylo_card returns %d\n", ret);

  return ret;
}

/**
 ** @brief Called when the char device is open
 ** get the interface and the private data and set the private data
 ** to the file structure.
 **
 */
static int xylo_led_open (struct inode *inode,
			  struct file *file)
{
  struct usb_xylo_led *dev;
  struct usb_interface *interface;
  int minor;

  minor = iminor(inode);

  /*
   * Get the interface for the device
   */
  interface = usb_find_interface (&xylo_led_driver, minor);
  if (!interface)
    return -ENODEV;

  /*
   * Get the device's private data
   */
  dev = usb_get_intfdata (interface);
  if (dev == NULL) {
    printk (KERN_WARNING "xylo_led_open(): can't find device for minor %d\n", minor);
    return -ENODEV;
  }

  /*
   * Set the private data to the file structure
   */
  file->private_data = (void *)dev;

  return 0;
}

/**
 ** @brief
 */
static int xylo_led_release (struct inode *inode,
			     struct file *file)
{
  return 0;
}

/**
 ** @brief Called when the char device is written.
 ** Retrieve the user data, and send the corresponding mask to the card.
 */
static ssize_t xylo_led_write(struct file *file,
			      const char *buf,
			      size_t count,
			      loff_t *ppos)
{
  /* ??? */
  return 0;
}

/**
 ** @brief Called when the char device is read
 ** Send the current content of the ledmask buffer
 */
static ssize_t xylo_led_read(struct file *file,
			     char *buf,
			     size_t count,
			     loff_t *ppos)
{
  return 0;
  /* ??? */
}

/**
 ** @brief Define the FOPS
 */
static struct file_operations xylo_led_file_operations = {
  .open = xylo_led_open,
  .release = xylo_led_release,
  .write = xylo_led_write,
  .read = xylo_led_read
};

static struct usb_class_driver xylo_led_class_driver = {
  .name = "usb/xylo_led%d",
  .fops = &xylo_led_file_operations,
  .minor_base = 0
};

/*
 ** This function will be called when sys entry is read.
 */
static ssize_t
xylo_led_sys_read (struct device *dev, struct device_attribute *attr,
		   char *buf)
{
  struct usb_interface *interface;
  struct usb_xylo_led *mydev;

  /*
   * Retrieve the interface
   */
  interface = to_usb_interface (dev);

  /*
   * Retrieve the device data
   */
  mydev = usb_get_intfdata (interface);

  /*
   * Write the data to the buffer and return the length
   */
  return sprintf (buf, "0x%x\n",  mydev->ledmask);
}

/*
 ** This function will be called when sys entry is written.
 */
static ssize_t
xylo_led_sys_write (struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
  struct usb_interface *interface;
  struct usb_xylo_led *mydev;
  int ret;

  interface = to_usb_interface (dev);
  mydev = usb_get_intfdata (interface);

  /* Now set the ledmask value */
  if ((ret = xylo_led_send_bulk_ledmask_buf (mydev, buf)) < 0)
    return ret;
  else
    return count;
}

/*
 * This macro generates a struct device_attribute
 */
static DEVICE_ATTR (ledmask, S_IRUGO | S_IWUGO,
		    xylo_led_sys_read,
		    xylo_led_sys_write);

/**
 ** @brief Called when the xylo device is probed
 **
 ** @param interface
 ** @param id
 **
 ** @returns
 */
static int xylo_led_probe (struct usb_interface *interface,
			   const struct usb_device_id *id)
{
  struct usb_device *udev = interface_to_usbdev (interface);
  struct usb_xylo_led *xylo_led_dev;
  int ret;

  printk (KERN_WARNING "xylo_led_probe()\n");

  /* Register char device interface */
  /* ??? */

  xylo_led_dev = kmalloc (sizeof(struct usb_xylo_led), GFP_KERNEL);
  if (xylo_led_dev == NULL) {
    dev_err (&interface->dev, "Out of memory\n");
    return -ENOMEM;
  }

  /*
   * Fill private structure and save it
   */
  memset (xylo_led_dev, 0, sizeof (*xylo_led_dev));
  xylo_led_dev->udev = usb_get_dev(udev);
  xylo_led_dev->ledmask = 0;
  usb_set_intfdata (interface, xylo_led_dev);

  /*
   * Create the /sys entry
   */
  ret = device_create_file (&interface->dev, &dev_attr_ledmask);
  if (ret < 0) {
    printk (KERN_WARNING "xylo_led_probe: device_create_file() error\n");
    return ret;
  }

  /*
   * Set the interface to alternate 1
   */
  ret = usb_set_interface(udev, 0, 1);
  if (ret < 0) {
    printk (KERN_WARNING "xylo_led_probe: usb_set_interface() error\n");
    return ret;
  }

  /*
   * Launch initialisations with URBs
   */
  ret = xylo_led_init_xylo_card(xylo_led_dev);
  if (ret < 0)
  {
    printk (KERN_WARNING "xylo_led_probe: xylo_led_init_xylo_card() returns %d\n", ret);
    return ret;
  }

  /*
   * Start small animation to show that the device is recognized
   */
  xylo_led_animation(xylo_led_dev);

  return 0;
}

/**
 ** @brief Called when the xylo devie is disconnected
 **
 ** @param interface
 */
static void xylo_led_disconnect(struct usb_interface *interface)
{
  struct usb_xylo_led *dev;

  dev = usb_get_intfdata(interface);
  usb_deregister_dev(interface, &xylo_led_class_driver);
  device_remove_file (&interface->dev, &dev_attr_ledmask);

  usb_set_intfdata(interface, NULL);
  kfree(dev);

  dev_info(&interface->dev, "Xylo now disconnected\n");
}

static struct usb_driver xylo_led_driver = {
  .name = "xylo_led",
  .probe = xylo_led_probe,
  .disconnect = xylo_led_disconnect,
  .id_table = id_table,
};

static int __init usb_xylo_led_init(void)
{
  int retval = 0;

  retval = usb_register(&xylo_led_driver);
  if (retval)
  {
    printk("usb_register failed. Error number %d", retval);
    return retval;
  }

  return retval;
}

static void __exit usb_xylo_led_exit(void)
{
  usb_deregister(&xylo_led_driver);
}

module_init (usb_xylo_led_init);
module_exit (usb_xylo_led_exit);
