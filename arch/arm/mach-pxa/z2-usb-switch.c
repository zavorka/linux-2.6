/*
 * USB mode switcher for Z2
 *
 * Copyright (c) 2011 Vasily Khoruzhick
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <mach/pxa27x.h>
#include <mach/pxa27x-udc.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static ssize_t usb_mode_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	if (UP2OCR & UP2OCR_HXS)
		return sprintf(buf, "host\n");
	else
		return sprintf(buf, "device\n");
}

static ssize_t usb_mode_set(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	if (strncmp(buf, "host", MIN(count, 4)) == 0) {
		UP2OCR = UP2OCR_HXS | UP2OCR_HXOE | UP2OCR_DPPDE | UP2OCR_DMPDE;
		return count;
	} else if (strncmp(buf, "device", MIN(count, 6)) == 0) {
		UP2OCR = UP2OCR_HXOE | UP2OCR_DPPUE;
		return count;
	}
	return -EINVAL;
}

static DEVICE_ATTR(usb_mode, 0644, usb_mode_show, usb_mode_set);

static const struct attribute *attrs[] = {
	&dev_attr_usb_mode.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs	= (struct attribute **)attrs,
};

static int z2_usb_switch_probe(struct platform_device *dev)
{
	int res;

	res = sysfs_create_group(&dev->dev.kobj, &attr_group);
	if (res)
		return res;

	UP2OCR = UP2OCR_HXOE | UP2OCR_DPPUE;

	return 0;
}

static int __devexit z2_usb_switch_remove(struct platform_device *dev)
{
	UP2OCR = UP2OCR_HXOE;
	sysfs_remove_group(&dev->dev.kobj, &attr_group);

	return 0;
}

static struct platform_driver z2_usb_switch_driver = {
	.probe = z2_usb_switch_probe,
	.remove = __devexit_p(z2_usb_switch_remove),

	.driver = {
		.name = "z2-usb-switch",
		.owner = THIS_MODULE,
	},
};


static int __init z2_usb_switch_init(void)
{
	return platform_driver_register(&z2_usb_switch_driver);
}

static void __exit z2_usb_switch_exit(void)
{
	platform_driver_unregister(&z2_usb_switch_driver);
}

module_init(z2_usb_switch_init);
module_exit(z2_usb_switch_exit);
