/*
 * Dallas Semiconductor DS1624 Digital Thermometer and Memory
 *
 * Written by: www.spin-lock.com
 *
 * Copyright (C) 2013 www.spin-lock.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/sysfs.h>

#define DS1624_ACCESS_MEMORY    0x17
#define DS1624_ACCESS_CONFIG	0xAC
#define DS1624_READ_TEMP        0xAA
#define DS1624_START_CONV       0xEE
#define DS1624_STOP_CONV        0x22

static bool config_done = 0;

static ssize_t ds1624_read_temp(struct kobject *kobj, struct bin_attribute *attr,
				char *buf, loff_t off, size_t count)
{
	struct i2c_client *client = kobj_to_i2c_client(kobj);
	s32 ret;
	u16 temp;
	u8 command, value;

	if (config_done == 0) {
		/* configure 1 SHOT mode */
		command = DS1624_ACCESS_CONFIG;
		value = 0x01;

		ret = i2c_smbus_write_byte_data(client, command, value);

		if (ret)
			printk(KERN_ERR "%s(%d): configure 1-shot mode failed\n", __func__, __LINE__);
		else {
			msleep(30);
			config_done = 1;
		}
	}

	/* start temp conversion */
	command = DS1624_START_CONV;
	value = 0x0;
	ret = i2c_smbus_write_byte_data(client, command, value);

	if (ret)
		printk(KERN_ERR "%s(%d): START_CONV failed\n", __func__, __LINE__);

	/* give time for the device to sense temperature */
	msleep(1000);

	/* read temp */
	command = DS1624_READ_TEMP;
	ret = i2c_smbus_read_word_data(client, command);

	if (ret < 0)
		printk(KERN_ERR "%s(%d): READ_TEMP failed\n", __func__, __LINE__);
	else {
		temp = (((ret << 8) & 0xff00) |
			((ret >> 8) & 0xff));
	}

	/* stop temp conversion */
	command = DS1624_STOP_CONV;
	value = 0x0;
	ret = i2c_smbus_write_byte_data(client, command, value);

	if (ret) {
		printk(KERN_ERR "%s(%d): STP_CONV failed\n", __func__, __LINE__);
	}

	memcpy(buf, &temp, sizeof(u16));

	return sizeof(u16);	
}

static struct bin_attribute ds1624_bin_attr = {
	.attr = {
		.name = "ds1624_read_temp",
		.mode = S_IRUGO,
	},
	.size = 2,
	.read = ds1624_read_temp
};

static ssize_t ds1624_eeprom_read(struct kobject *kobj, struct bin_attribute *attr,
				  char *buf, loff_t off, size_t count)
{

	struct i2c_client *client = kobj_to_i2c_client(kobj);
        s32 ret;
        u8 command, val;
	static u8 data[256];
	int i;

	command = DS1624_ACCESS_MEMORY;
	
	for (i = 0; i<count; i++) {
		val = (u8)i;

		/* Write the offset (addr) to read */
		ret = i2c_smbus_write_byte_data(client, command, val);	
		if (ret < 0 ) {
			printk ("Error in reading eeprom  ret = %d %s : %d \n",ret, __func__,__LINE__);
			return ret;
		
		} else {
			/* Reads the actual data @ the given addr */
			ret = i2c_smbus_read_byte_data(client, command);	
			if (ret >= 0 ) {
				data[val] = ret & 0xff;
				msleep(30);
			} else {
				printk ("%s :%d ret:%d \n",__func__,__LINE__,ret);
				return ret;
			}
		
		}
	}

	memcpy(buf, data, count);
	
	return count;
}

static ssize_t ds1624_eeprom_write(struct kobject *kobj, struct bin_attribute *attr,
				   char *buf, loff_t off, size_t count)
{
	struct i2c_client *client = kobj_to_i2c_client(kobj);
    s32 ret;
    u8 command;
	u16 val;
	int i;

	command = DS1624_ACCESS_MEMORY;
	for (i = 0; i<count; i++) {
        
		/* Lower byte =  addr, Higher byte = data */
		val = (((i & 0xff) )| ((buf[i] & 0xff) << 8));
		ret = i2c_smbus_write_word_data(client, command, val);	
		if (ret < 0 ) {
			printk ("Error in reading eeprom  ret = %d %s : %d \n",ret, __func__,__LINE__);
			return ret;
		
		} else {
			msleep(30);
		}
	}

	return count;
}

static struct bin_attribute ds1624_bin_eeprom_attr = {
	.attr = {
		.name = "ds1624_eeprom_read_write",
		.mode = S_IRUGO,
	},
	.size = 256,
	.read = ds1624_eeprom_read,
	.write = ds1624_eeprom_write,
};

static int ds1624_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int rc;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE_DATA)) {
		printk(KERN_ERR "%s(%d): i2c bus doesn't support ds1624 byte data\n",
				__func__, __LINE__);
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_WORD_DATA)) {
		printk(KERN_ERR "%s(%d): i2c bus doesn't support ds1624 word data\n",
				__func__, __LINE__);
		return -ENODEV;
	}

	rc = sysfs_create_bin_file(&client->dev.kobj, &ds1624_bin_attr);

	if (rc) {
		printk(KERN_ERR "%s(%d): create bin file failed\n",
				__func__, __LINE__);
	}

	rc = sysfs_create_bin_file(&client->dev.kobj, &ds1624_bin_eeprom_attr);

	if (rc) {
            printk(KERN_ERR "%s(%d): create eeprom bin file failed\n",
                                __func__, __LINE__);
    }
	
	return rc;
}

static int ds1624_remove(struct i2c_client *client)
{
	sysfs_remove_bin_file(&client->dev.kobj, &ds1624_bin_attr);
	sysfs_remove_bin_file(&client->dev.kobj, &ds1624_bin_eeprom_attr);
	return 0;
}

static const struct i2c_device_id ds1624_id[] = {
	{ "ds1624", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ds1624_id);

static struct i2c_driver ds1624_driver = {
	.driver = {
		.name = "ds1624_driver",
	},
	.probe = ds1624_probe,
	.remove = ds1624_remove,
	.id_table = ds1624_id,
};

static int ds1624_init(void)
{
	int rc;

	rc = i2c_add_driver(&ds1624_driver);

	if (rc) {
		printk("%s(%d): i2c add driver failed\n", __func__, __LINE__);
	}

	return rc;
}

static void ds1624_exit(void)
{
	i2c_del_driver(&ds1624_driver);
}

module_init(ds1624_init);
module_exit(ds1624_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("www.spin-lock.com");
