/*
 * LM49352 Secondary Audio Driver for Delta 800 board
 *
 * Copyright (C) 2014, Chipwork s.n.c. ITALY
 *
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>


/* Addresses scanned */
static const unsigned short normal_i2c[] = { 0x1a, I2C_CLIENT_END };

#define DRVNAME			"lm49352_cntr"

#define BASIC_SETUP_PMC_SETUP                0x00
#define ANALOG_MIXER_AUX_OUT                 0x13
#define ANALOG_MIXER_OUTPUT_OPTIONS          0x14
#define ANALOG_MIXER_AUXL_LVL                0x18
#define ANALOG_MIXER_MONO_LVL                0x19


/*-----------------------------------------------------------------------*/


static ssize_t set_register(struct device *dev, struct device_attribute *da,
			const char *buf, size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	long temp;
	unsigned char value;

	int status = kstrtol(buf, 10, &temp);
	if (status < 0)
		return status;

	/* Write value */
	if (temp > 255)
 	temp = 0;
	value = (unsigned char) temp;

	i2c_smbus_write_byte_data(client, attr->index, value);
	return count;
}

static ssize_t show_register(struct device *dev, struct device_attribute *da,
			 char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char val;


	val = i2c_smbus_read_byte_data(client,attr->index);

	return sprintf(buf, "%d(0x%x)\n", val,val);
}


/*-----------------------------------------------------------------------*/

/* sysfs attributes for hwmon */

static SENSOR_DEVICE_ATTR(register_number0, S_IWUSR | S_IRUGO,show_register, set_register, BASIC_SETUP_PMC_SETUP);



static struct attribute *lm49352_cntr_attributes[] = {
	&sensor_dev_attr_register_number0.dev_attr.attr,
	NULL
};

static const struct attribute_group lm49352_cntr_group = {
	.attrs = lm49352_cntr_attributes,
};

/*-----------------------------------------------------------------------*/

/* device probe and removal */

static int
lm49352_cntr_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *hwmon_dev;
	int status;

	/* Register sysfs hooks */
	status = sysfs_create_group(&client->dev.kobj, &lm49352_cntr_group);
	if (status)
		return status;

	hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(hwmon_dev)) {
		status = PTR_ERR(hwmon_dev);
		goto exit_remove;
	}
	i2c_set_clientdata(client, hwmon_dev);

	dev_info(&client->dev, "%s: audio control '%s'\n",
		 dev_name(hwmon_dev), client->name);

	//Set AUXOUTPUT mode to AUX_LINE_OUT 
	i2c_smbus_write_byte_data(client, ANALOG_MIXER_OUTPUT_OPTIONS,0x20);
	//Active AUXIN+MONAUX MIXED ON AUXOUTPUT 
	i2c_smbus_write_byte_data(client, ANALOG_MIXER_AUX_OUT,0x30);
	//Setup Single Ended input and level for input AUXL
	i2c_smbus_write_byte_data(client, ANALOG_MIXER_AUXL_LVL,0x5f);
	//Setup Single Ended input and level for input AUXR routed to MONO
	i2c_smbus_write_byte_data(client, ANALOG_MIXER_MONO_LVL,0xdf);
	//On decoder
	i2c_smbus_write_byte_data(client, BASIC_SETUP_PMC_SETUP,0x17);

	return 0;

exit_remove:
	sysfs_remove_group(&client->dev.kobj, &lm49352_cntr_group);
	return status;
}

static int lm49352_cntr_remove(struct i2c_client *client)
{
	struct device *hwmon_dev = i2c_get_clientdata(client);

	hwmon_device_unregister(hwmon_dev);
	sysfs_remove_group(&client->dev.kobj, &lm49352_cntr_group);
	return 0;
}

static const struct i2c_device_id lm49352_cntr_ids[] = {
	{ "lm49352_cntr", 0 },
	{ /* LIST END */ }
};
MODULE_DEVICE_TABLE(i2c, lm49352_cntr_ids);

/* Return 0 if detection is successful, -ENODEV otherwise */
static int lm49352_cntr_detect(struct i2c_client *new_client,
			struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = new_client->adapter;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	strlcpy(info->type, "lm49352_cntr", I2C_NAME_SIZE);

	return 0;
}

static struct i2c_driver lm49352_cntr_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= "lm49352_cntr",
	},
	.probe		= lm49352_cntr_probe,
	.remove		= lm49352_cntr_remove,
	.id_table	= lm49352_cntr_ids,
	.detect		= lm49352_cntr_detect,
	.address_list	= normal_i2c,
};

/* module glue */

static int __init sensors_lm49352_cntr_init(void)
{
	return i2c_add_driver(&lm49352_cntr_driver);
}

static void __exit sensors_lm49352_cntr_exit(void)
{
	i2c_del_driver(&lm49352_cntr_driver);
}

MODULE_AUTHOR("Cerioli Alessandro Chipwork s.n.c.");
MODULE_DESCRIPTION("lm49352_cntr driver");
MODULE_LICENSE("GPL");

module_init(sensors_lm49352_cntr_init);
module_exit(sensors_lm49352_cntr_exit);
