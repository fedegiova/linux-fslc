/*
 * drivers/media/video/mxc/capture/tvp5150.c
 *
 * TI tvp5150 Decoder MXC V4L2 Driver
 *
 * Copyright (C) 2013 Chipwork s.n.c. ITALY
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

//#define DEBUG

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/wait.h>
#include <linux/videodev2.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>


#include <linux/fsl_devices.h>
#include <media/v4l2-chip-ident.h>
#include "v4l2-int-device.h"
#include "mxc_v4l2_capture.h"
#include "tvp5150_reg.h"


#define TVP5150_VOLTAGE_ANALOG               1800000
#define TVP5150_VOLTAGE_DIGITAL_CORE         1800000
#define TVP5150_VOLTAGE_DIGITAL_IO           3300000
#define TVP5150_VOLTAGE_PLL                  1800000

static int pwn_gpio;

struct i2c_reg_value {
	unsigned char reg;
	unsigned char value;
};

static struct regulator *dvddio_regulator;
static struct regulator *dvdd_regulator;
static struct regulator *avdd_regulator;
static struct regulator *pvdd_regulator;


static int tvp5150_probe(struct i2c_client *adapter,
			 const struct i2c_device_id *id);
static int tvp5150_detach(struct i2c_client *client);

static const struct i2c_device_id tvp5150_id[] = {
	{"tvp5150", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, tvp5150_id);

static struct i2c_driver tvp5150_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "tvp5150",
		   },
	.probe = tvp5150_probe,
	.remove = tvp5150_detach,
	.id_table = tvp5150_id,
};


/* Default values as suggested at TVP5150AM1 datasheet */
static const struct i2c_reg_value tvp5150_init_default[] = {
	{ /* 0x00 */
		TVP5150_VD_IN_SRC_SEL_1,0x00
	},
	{ /* 0x01 */
		TVP5150_ANAL_CHL_CTL,0x15
	},
	{ /* 0x02 */
		TVP5150_OP_MODE_CTL,0x00
	},
	{ /* 0x03 */
		TVP5150_MISC_CTL,0x2d  //Chip Default 0x01
	},
	{ /* 0x06 */
		TVP5150_COLOR_KIL_THSH_CTL,0x10
	},
	{ /* 0x07 */
		TVP5150_LUMA_PROC_CTL_1,0x60
	},
	{ /* 0x08 */
		TVP5150_LUMA_PROC_CTL_2,0x00
	},
	{ /* 0x09 */
		TVP5150_BRIGHT_CTL,0x80
	},
	{ /* 0x0a */
		TVP5150_SATURATION_CTL,0x80
	},
	{ /* 0x0b */
		TVP5150_HUE_CTL,0x00
	},
	{ /* 0x0c */
		TVP5150_CONTRAST_CTL,0x80
	},
	{ /* 0x0d */
		TVP5150_DATA_RATE_SEL,0x47
	},
	{ /* 0x0e */
		TVP5150_LUMA_PROC_CTL_3,0x00
	},
	{ /* 0x0f */
		TVP5150_CONF_SHARED_PIN,0x08
	},
	{ /* 0x11 */
		TVP5150_ACT_VD_CROP_ST_MSB,0x00
	},
	{ /* 0x12 */
		TVP5150_ACT_VD_CROP_ST_LSB,0x00
	},
	{ /* 0x13 */
		TVP5150_ACT_VD_CROP_STP_MSB,0x00
	},
	{ /* 0x14 */
		TVP5150_ACT_VD_CROP_STP_LSB,0x00
	},
	{ /* 0x15 */
		TVP5150_GENLOCK,0x01
	},
	{ /* 0x16 */
		TVP5150_HORIZ_SYNC_START,0x80
	},
	{ /* 0x18 */
		TVP5150_VERT_BLANKING_START,0x00
	},
	{ /* 0x19 */
		TVP5150_VERT_BLANKING_STOP,0x00
	},
	{ /* 0x1a */
		TVP5150_CHROMA_PROC_CTL_1,0x0c
	},
	{ /* 0x1b */
		TVP5150_CHROMA_PROC_CTL_2,0x14
	},
	{ /* 0x1c */
		TVP5150_INT_RESET_REG_B,0x00
	},
	{ /* 0x1d */
		TVP5150_INT_ENABLE_REG_B,0x00
	},
	{ /* 0x1e */
		TVP5150_INTT_CONFIG_REG_B,0x00
	},
	{ /* 0x28 */
		TVP5150_VIDEO_STD,0x00
	},
	{ /* 0x2e */
		TVP5150_MACROVISION_ON_CTR,0x0f
	},
	{ /* 0x2f */
		TVP5150_MACROVISION_OFF_CTR,0x01
	},
	{ /* 0xbb */
		TVP5150_TELETEXT_FIL_ENA,0x00
	},
	{ /* 0xc0 */
		TVP5150_INT_STATUS_REG_A,0x00
	},
	{ /* 0xc1 */
		TVP5150_INT_ENABLE_REG_A,0x00
	},
	{ /* 0xc2 */
		TVP5150_INT_CONF,0x04
	},
	{ /* 0xc8 */
		TVP5150_FIFO_INT_THRESHOLD,0x80
	},
	{ /* 0xc9 */
		TVP5150_FIFO_RESET,0x00
	},
	{ /* 0xca */
		TVP5150_LINE_NUMBER_INT,0x00
	},
	{ /* 0xcb */
		TVP5150_PIX_ALIGN_REG_LOW,0x4e
	},
	{ /* 0xcc */
		TVP5150_PIX_ALIGN_REG_HIGH,0x00
	},
	{ /* 0xcd */
		TVP5150_FIFO_OUT_CTRL,0x01
	},
	{ /* 0xcf */
		TVP5150_FULL_FIELD_ENA,0x00
	},
	{ /* 0xd0 */
		TVP5150_LINE_MODE_INI,0x00
	},
	{ /* 0xfc */
		TVP5150_FULL_FIELD_MODE_REG,0x7f
	},
	{ /* end of data */
		0xff,0xff
	}
};


struct sensor {
	struct v4l2_int_device *v4l2_int_device;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	struct v4l2_captureparm streamcap;
	bool on;
	struct sensor_data sen;

	/* control settings */
	int brightness;
	int hue;
	int contrast;
	int saturation;
	u32 input;
	u32 output;
	v4l2_std_id std_id;
	int csi;
} tvp5150_data;

/*Supported formats*/
typedef enum {
	TVP5150_NTSC = 0,	/*!< Locked on (M) NTSC video signal. */
	TVP5150_PAL,		/*!< (B, G, H, I, N)PAL video signal. */
	TVP5150_NOT_LOCKED,	/*!< Not locked on a signal. */
} video_fmt_idx;

#define TVP5150_STD_MAX		(TVP5150_PAL + 1)

/*Video format struct*/
typedef struct {
	int v4l2_id;		/*!< Video for linux ID. */
	char name[16];		/*!< Name (e.g., "NTSC", "PAL", etc.) */
	u16 raw_width;		/*!< Raw width. */
	u16 raw_height;		/*!< Raw height. */
	u16 active_width;	/*!< Active width. */
	u16 active_height;	/*!< Active height. */
} video_fmt_t;


/*! Supported video structs
 *
 *  PAL: raw=720x625, active=720x576.
 *  NTSC: raw=720x525, active=720x480.
 */
static video_fmt_t video_fmts[] = {
	{			/*! NTSC */
	 .v4l2_id = V4L2_STD_NTSC,
	 .name = "NTSC",
	 .raw_width = 720,	/* SENS_FRM_WIDTH */
	 .raw_height = 525,	/* SENS_FRM_HEIGHT */
	 .active_width = 720,	/* ACT_FRM_WIDTH plus 1 */
	 .active_height = 480,	/* ACT_FRM_WIDTH plus 1 */
	 },
	{			/*! (B, G, H, I, N) PAL */
	 .v4l2_id = V4L2_STD_PAL,
	 .name = "PAL",
	 .raw_width = 720,
	 .raw_height = 625,
	 .active_width = 720,
	 .active_height = 576,
	 },
	{			/*! Unlocked standard */
	 .v4l2_id = V4L2_STD_ALL,
	 .name = "Autodetect",
	 .raw_width = 720,
	 .raw_height = 625,
	 .active_width = 720,
	 .active_height = 576,
	 },
};

static video_fmt_idx video_idx = TVP5150_PAL; //TVP5150_NTSC;//TVP5150_PAL;

/*! @brief This mutex is used to provide mutual exclusion.
 *
 *  Create a mutex that can be used to provide mutually exclusive
 *  read/write access to the globally accessible data structures
 *  and variables that were defined above.
 */
static DEFINE_MUTEX(mutex);

#define IF_NAME                    "tvp5150"


/*Supported controls*/
/*(Not finished)*/
static struct v4l2_queryctrl tvp5150_qctrl[] = {
	{
	.id = V4L2_CID_BRIGHTNESS,
	.type = V4L2_CTRL_TYPE_INTEGER,
	.name = "Brightness",
	.minimum = 0,		/* check this value */
	.maximum = 255,		/* check this value */
	.step = 1,		/* check this value */
	.default_value = 128,	/* check this value */
	.flags = 0,
	}, {
	.id = V4L2_CID_SATURATION,
	.type = V4L2_CTRL_TYPE_INTEGER,
	.name = "Saturation",
	.minimum = 0,		/* check this value */
	.maximum = 255,		/* check this value */
	.step = 0x1,		/* check this value */
	.default_value = 128	,	/* check this value */
	.flags = 0,
	},{
	.id = V4L2_CID_CONTRAST,
	.type = V4L2_CTRL_TYPE_INTEGER,
	.name = "Constrast",
	.minimum = 0,		/* check this value */
	.maximum = 255,		/* check this value */
	.step = 1,		/* check this value */
	.default_value = 128,	/* check this value */
	.flags = 0,
	},{
	.id = V4L2_CID_HUE,
	.type = V4L2_CTRL_TYPE_INTEGER,
	.name = "Hue",
	.minimum = 0,		/* check this value */
	.maximum = 255,		/* check this value */
	.step = 1,		/* check this value */
	.default_value = 0,	/* check this value */
	.flags = 0,
	}

};

/***********************************************************************
 * I2C transfert.
 ***********************************************************************/

static inline int tvp5150_read(u8 reg)
{
	int val;
	val = i2c_smbus_read_byte_data(tvp5150_data.i2c_client, reg);
	if (val < 0) {
		printk(
			"%s:read reg error: reg=%2x \n", __func__, reg);
		return -1;
	}
	return val;
}

static int tvp5150_write_reg(u8 reg, u8 val)
{
	if (i2c_smbus_write_byte_data(tvp5150_data.i2c_client, reg, val) < 0) {
		printk(
			"%s:write reg error:reg=%2x,val=%2x\n", __func__,
			reg, val);
		return -1;
	}
	return 0;
}

/***********************************************************************
 * mxc_v4l2_capture interface.
 ***********************************************************************/

/*!
 * Return attributes of current video standard.
 * Since this device autodetects the current standard, this function also
 * sets the values that need to be changed if the standard changes.
 * There is no set std equivalent function.
 *
 *  @return		None.
*/
static void tvp5150_get_std(v4l2_std_id *std)
{
	int tmp;
	int idx;

#ifdef DEBUG
	printk("In tvp5150_get_std\n");
#endif

	/* Make sure power on */
/*	if (tvin_plat->pwdn)
		tvin_plat->pwdn(0);
*/
	tmp = tvp5150_read(TVP5150_STATUS_REG_5) & 0x0f;

	mutex_lock(&mutex);
	if (tmp == 0x03 || tmp == 0x05) {
		/* PAL */
		*std = V4L2_STD_PAL;
		idx = TVP5150_PAL;
#ifdef DEBUG
		printk("-> PAL video in\n");
#endif
	} else if (tmp == 0x01 || tmp == 0x09) {
		/*NTSC*/
		*std = V4L2_STD_NTSC;
		idx = TVP5150_NTSC;
#ifdef DEBUG
		printk("-> NTSC video in\n");
#endif
	} else {
		*std = V4L2_STD_ALL;
		idx = TVP5150_NOT_LOCKED;
		printk(
			"Got invalid video standard! \n");
	}
	mutex_unlock(&mutex);

	/* This assumes autodetect which this device uses. */
	if (*std != tvp5150_data.std_id) {
		video_idx = idx;
		tvp5150_data.std_id = *std;
		tvp5150_data.pix.width = video_fmts[video_idx].raw_width;
		tvp5150_data.pix.height = video_fmts[video_idx].raw_height;
	}
}



/***********************************************************************
 * IOCTL Functions from v4l2_int_ioctl_desc.
 ***********************************************************************/

/*!
 * ioctl_g_chip_ident - V4L2 sensor interface handler for
 *			VIDIOC_DBG_G_CHIP_IDENT ioctl
 * @s: pointer to standard V4L2 device structure
 * @id: pointer to int
 *
 * Return 0.
 */
static int ioctl_g_chip_ident(struct v4l2_int_device *s, int *id)
{
	((struct v4l2_dbg_chip_ident *)id)->match.type =
					V4L2_CHIP_MATCH_I2C_DRIVER;
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name,
						"tvp5150_decoder");
	((struct v4l2_dbg_chip_ident *)id)->ident = V4L2_IDENT_TVP5150;

	return 0;
}


/*!
 * ioctl_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
				 struct v4l2_frmsizeenum *fsize)
{
	if (fsize->index >= 1)
		return -EINVAL;

	fsize->discrete.width = video_fmts[video_idx].active_width;
	fsize->discrete.height  = video_fmts[video_idx].active_height;

	return 0;
}




#ifdef EXC_ALL_CODE
static int ioctl_s_video_routing_num(struct v4l2_int_device *s, struct v4l2_input *input)
{
/*
	((struct v4l2_dbg_chip_ident *)id)->match.type =
					V4L2_CHIP_MATCH_I2C_DRIVER;
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name,
						"tvp5150_decoder");
	((struct v4l2_dbg_chip_ident *)id)->ident = V4L2_IDENT_TVP5150;
*/
#ifdef DEBUG
	printk("In tvp5150_video_routing\n");
#endif


	return 0;

}
#endif


static int ioctl_tvp5150_input_num(struct v4l2_int_device *s,  int *input)
{
   int tmp;

#ifdef DEBUG
	printk("Selected Video Input:%d\n",*input);
#endif

	tmp = tvp5150_read(TVP5150_VD_IN_SRC_SEL_1);

	if (*input == 0)
	 tmp = tmp & 0xfc;
         else
	 tmp = tmp | 0x02;

	 tvp5150_write_reg(TVP5150_VD_IN_SRC_SEL_1, tmp);

	return 0;
}




/*!
 * ioctl_g_ifparm - V4L2 sensor interface handler for vidioc_int_g_ifparm_num
 * s: pointer to standard V4L2 device structure
 * p: pointer to standard V4L2 vidioc_int_g_ifparm_num ioctl structure
 *
 * Gets slave interface parameters.
 * Calculates the required xclk value to support the requested
 * clock parameters in p.  This value is returned in the p
 * parameter.
 *
 * vidioc_int_g_ifparm returns platform-specific information about the
 * interface settings used by the sensor.
 *
 * Called on open.
 */
static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
#ifdef DEBUG
	printk("In tvp5150:ioctl_g_ifparm\n");
#endif

	if (s == NULL) {
		printk("<3>""   ERROR!! no slave device set!\n");
		return -1;
	}

	/* Initialize structure to 0s then set any non-0 values. */
	memset(p, 0, sizeof(*p));
	p->if_type = V4L2_IF_TYPE_BT656; /* This is the only possibility.*/
	p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT; //V4L2_IF_TYPE_BT656_MODE_BT_8BIT;
	p->u.bt656.nobt_hs_inv = 1;
	p->u.bt656.bt_sync_correct = 1;
	p->u.bt656.clock_curr = 0;  //Interlaced mode
	/* tvp5150 has a dedicated clock so no clock settings needed. */

	return 0;
}

/*!
 * Sets the camera power.
 *
 * s  pointer to the camera device
 * on if 1, power is to be turned on.  0 means power is to be turned off
 *
 * ioctl_s_power - V4L2 sensor interface handler for vidioc_int_s_power_num
 * @s: pointer to standard V4L2 device structure
 * @on: power state to which device is to be set
 *
 * Sets devices power state to requrested state, if possible.
 * This is called on open, close, suspend and resume.
 */
static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	struct sensor *sensor = s->priv;

#ifdef DEBUG
	printk("In tvp5150:ioctl_s_power\n");
#endif
/*
	if (on && !sensor->on) {
			gpio_sensor_active();

			}

	else if (!on && sensor->on){
			gpio_sensor_inactive();
			}

*/
	sensor->on = on;
	return 0;
}

/*!
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct sensor *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;

#ifdef DEBUG
	printk("In tvp5150:ioctl_g_parm\n");
#endif

	switch (a->type) {
	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
#ifdef DEBUG
		printk("<7>""   type is V4L2_BUF_TYPE_VIDEO_CAPTURE\n");
#endif
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		break;

	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		break;

	default:
#ifdef DEBUG
		printk("<7>""ioctl_g_parm:type is unknown %d\n", a->type);
#endif
		break;
	}

	return 0;
}

/*!
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 *
 * This driver cannot change these settings.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
#ifdef DEBUG
	printk("In tvp5150:ioctl_s_parm\n");
#endif

	switch (a->type) {
	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		break;

	default:
#ifdef DEBUG
		printk("<7>""   type is unknown - %d\n", a->type);
#endif
		break;
	}

	return 0;
}

/*!
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct sensor *sensor = s->priv;

#ifdef DEBUG
	printk("In tvp5150:ioctl_g_fmt_cap\n");
#endif

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
#ifdef DEBUG
		printk("<7>""   Returning size of %dx%d\n",
			 sensor->pix.width, sensor->pix.height);
#endif
		f->fmt.pix = sensor->pix;
		break;

	case V4L2_BUF_TYPE_PRIVATE: {
		v4l2_std_id std;
		tvp5150_get_std(&std);
		f->fmt.pix.pixelformat = (u32)std;
		}
		break;

	default:
		f->fmt.pix = sensor->pix;
		break;
	}

	return 0;
}

/*!
 * ioctl_queryctrl - V4L2 sensor interface handler for VIDIOC_QUERYCTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @qc: standard V4L2 VIDIOC_QUERYCTRL ioctl structure
 *
 * If the requested control is supported, returns the control information
 * from the video_control[] array.  Otherwise, returns -EINVAL if the
 * control is not supported.
 */
static int ioctl_queryctrl(struct v4l2_int_device *s,
			   struct v4l2_queryctrl *qc)
{
	int i;

#ifdef DEBUG
	printk("In tvp5150:ioctl_queryctrl\n");
#endif

	for (i = 0; i < ARRAY_SIZE(tvp5150_qctrl); i++)
		if (qc->id && qc->id == tvp5150_qctrl[i].id) {
			memcpy(qc, &(tvp5150_qctrl[i]),
				sizeof(*qc));
			return (0);
		}

	return -EINVAL;
}

/*!
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int ret = 0;

#ifdef DEBUG
	printk("In tvp5150:ioctl_g_ctrl\n");
#endif


	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
#ifdef DEBUG
		printk(
			"   V4L2_CID_BRIGHTNESS\n");
#endif
		tvp5150_data.brightness = tvp5150_read(TVP5150_BRIGHT_CTL);
		vc->value = tvp5150_data.brightness;
		break;
	case V4L2_CID_CONTRAST:
#ifdef DEBUG
		printk(
			"   V4L2_CID_CONTRAST\n");
#endif
		tvp5150_data.contrast = tvp5150_read(TVP5150_CONTRAST_CTL);
		vc->value = tvp5150_data.contrast;
		break;
	case V4L2_CID_SATURATION:
#ifdef DEBUG
		printk(
			"   V4L2_CID_SATURATION\n");
#endif
		tvp5150_data.saturation = tvp5150_read(TVP5150_SATURATION_CTL);
		vc->value = tvp5150_data.saturation;
		break;
	case V4L2_CID_HUE:
#ifdef DEBUG
		printk(
			"   V4L2_CID_HUE\n");
#endif
		tvp5150_data.hue = tvp5150_read(TVP5150_HUE_CTL);
		vc->value = tvp5150_data.hue;
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
#ifdef DEBUG
		printk(
			"   V4L2_CID_AUTO_WHITE_BALANCE\n");
#endif
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
#ifdef DEBUG
		printk(
			"   V4L2_CID_DO_WHITE_BALANCE\n");
#endif
		break;
	case V4L2_CID_RED_BALANCE:
#ifdef DEBUG
		printk(
			"   V4L2_CID_RED_BALANCE\n");
#endif
		break;
	case V4L2_CID_BLUE_BALANCE:
#ifdef DEBUG
		printk(
			"   V4L2_CID_BLUE_BALANCE\n");
#endif
		break;
	case V4L2_CID_GAMMA:
#ifdef DEBUG
		printk(
			"   V4L2_CID_GAMMA\n");
#endif
		break;
	case V4L2_CID_EXPOSURE:
#ifdef DEBUG
		printk(
			"   V4L2_CID_EXPOSURE\n");
#endif
		break;
	case V4L2_CID_AUTOGAIN:
#ifdef DEBUG
		printk(
			"   V4L2_CID_AUTOGAIN\n");
#endif
		break;
	case V4L2_CID_GAIN:
#ifdef DEBUG
		printk(
			"   V4L2_CID_GAIN\n");
#endif
		break;
	case V4L2_CID_HFLIP:
#ifdef DEBUG
		printk(
			"   V4L2_CID_HFLIP\n");
#endif
		break;
	case V4L2_CID_VFLIP:
#ifdef DEBUG
		printk(
			"   V4L2_CID_VFLIP\n");
#endif
		break;
	default:
#ifdef DEBUG
		printk(
			"   Default case\n");
#endif
		vc->value = 0;
		ret = -EPERM;
		break;
	}

	return ret;
}

/*!
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int retval = 0;
	u8 tmp;

#ifdef DEBUG
		printk("In tvp5150:ioctl_s_ctrl\n");
#endif

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
#ifdef DEBUG
		printk(
			"   V4L2_CID_BRIGHTNESS\n");
#endif
		tmp = vc->value;
		tvp5150_write_reg(TVP5150_BRIGHT_CTL, tmp);
		tvp5150_data.brightness = vc->value;
		break;
	case V4L2_CID_CONTRAST:
#ifdef DEBUG
		printk(
			"   V4L2_CID_CONTRAST\n");
#endif
		tmp = vc->value;
		tvp5150_write_reg(TVP5150_CONTRAST_CTL, tmp);
		tvp5150_data.contrast = vc->value;
		break;
	case V4L2_CID_SATURATION:
#ifdef DEBUG
		printk(
			"   V4L2_CID_SATURATION\n");
#endif
		tmp = vc->value;
		tvp5150_write_reg(TVP5150_SATURATION_CTL, tmp);
		tvp5150_data.saturation = vc->value;
		break;
	case V4L2_CID_HUE:
#ifdef DEBUG
		printk(
			"   V4L2_CID_HUE\n");
#endif
		tmp = vc->value;
		tvp5150_write_reg(TVP5150_HUE_CTL, tmp);
		tvp5150_data.hue = vc->value;
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
#ifdef DEBUG
		printk(
			"   V4L2_CID_AUTO_WHITE_BALANCE\n");
#endif
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
#ifdef DEBUG
		printk(
			"   V4L2_CID_DO_WHITE_BALANCE\n");
#endif
		break;
	case V4L2_CID_RED_BALANCE:
#ifdef DEBUG
		printk(
			"   V4L2_CID_RED_BALANCE\n");
#endif
		break;
	case V4L2_CID_BLUE_BALANCE:
#ifdef DEBUG
		printk(
			"   V4L2_CID_BLUE_BALANCE\n");
#endif
		break;
	case V4L2_CID_GAMMA:
#ifdef DEBUG
		printk(
			"   V4L2_CID_GAMMA\n");
#endif
		break;
	case V4L2_CID_EXPOSURE:
#ifdef DEBUG
		printk(
			"   V4L2_CID_EXPOSURE\n");
#endif
		break;
	case V4L2_CID_AUTOGAIN:
#ifdef DEBUG
		printk(
			"   V4L2_CID_AUTOGAIN\n");
#endif
		break;
	case V4L2_CID_GAIN:
#ifdef DEBUG
		printk(
			"   V4L2_CID_GAIN\n");
#endif
		break;
	case V4L2_CID_HFLIP:
#ifdef DEBUG
		printk(
			"   V4L2_CID_HFLIP\n");
#endif
		break;
	case V4L2_CID_VFLIP:
#ifdef DEBUG
		printk(
			"   V4L2_CID_VFLIP\n");
#endif
		break;
	default:
#ifdef DEBUG
		printk(
			"   Default case\n");
#endif
		retval = -EPERM;
		break;
	}

	return retval;
}

/*!
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int ioctl_init(struct v4l2_int_device *s)
{
#ifdef DEBUG
		printk("In tvp5150:ioctl_init\n");
#endif
	return 0;
}

/*!
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
#ifdef DEBUG
	printk("In tvp5150:ioctl_dev_init\n");
#endif
	return 0;
}

/*!
 * This structure defines all the ioctls for this module.
 */static struct v4l2_int_ioctl_desc tvp5150_ioctl_desc[] = {

	{vidioc_int_dev_init_num, (v4l2_int_ioctl_func *)ioctl_dev_init},

	/*!
	 * Delinitialise the dev. at slave detach.
	 * The complement of ioctl_dev_init.
	 */
/*	{vidioc_int_dev_exit_num, (v4l2_int_ioctl_func *)ioctl_dev_exit}, */

	{vidioc_int_s_power_num, (v4l2_int_ioctl_func *)ioctl_s_power},
	{vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func *)ioctl_g_ifparm},
/*	{vidioc_int_g_needs_reset_num,
				(v4l2_int_ioctl_func *)ioctl_g_needs_reset}, */
/*	{vidioc_int_reset_num, (v4l2_int_ioctl_func *)ioctl_reset}, */
	{vidioc_int_init_num, (v4l2_int_ioctl_func *)ioctl_init},

	/*!
	 * VIDIOC_ENUM_FMT ioctl for the CAPTURE buffer type.
	 */
/*	{vidioc_int_enum_fmt_cap_num,
				(v4l2_int_ioctl_func *)ioctl_enum_fmt_cap}, */

	/*!
	 * VIDIOC_TRY_FMT ioctl for the CAPTURE buffer type.
	 * This ioctl is used to negotiate the image capture size and
	 * pixel format without actually making it take effect.
	 */
/*	{vidioc_int_try_fmt_cap_num,
				(v4l2_int_ioctl_func *)ioctl_try_fmt_cap}, */

	{vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func *)ioctl_g_fmt_cap},

	/*!
	 * If the requested format is supported, configures the HW to use that
	 * format, returns error code if format not supported or HW can't be
	 * correctly configured.
	 */
/*	{vidioc_int_s_fmt_cap_num, (v4l2_int_ioctl_func *)ioctl_s_fmt_cap}, */

	{vidioc_int_g_parm_num, (v4l2_int_ioctl_func *)ioctl_g_parm},
	{vidioc_int_s_parm_num, (v4l2_int_ioctl_func *)ioctl_s_parm},
	{vidioc_int_queryctrl_num, (v4l2_int_ioctl_func *)ioctl_queryctrl},
	{vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func *)ioctl_g_ctrl},
	{vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func *)ioctl_s_ctrl},
	{vidioc_int_enum_framesizes_num,
				(v4l2_int_ioctl_func *) ioctl_enum_framesizes},

	{vidioc_int_g_chip_ident_num,
				(v4l2_int_ioctl_func *)ioctl_g_chip_ident},
/*	{vidioc_int_s_video_routing_num,
				(v4l2_int_ioctl_func *)ioctl_s_video_routing_num},*/
	{vidioc_int_tvp5150_input_num,
				(v4l2_int_ioctl_func *)ioctl_tvp5150_input_num},
};

static struct v4l2_int_slave tvp5150_slave = {
	.ioctls = tvp5150_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(tvp5150_ioctl_desc),
};

static struct v4l2_int_device tvp5150_int_device = {
	.module = THIS_MODULE,
	.name = "tvp5150",
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &tvp5150_slave,
	},
};


/***********************************************************************
 * I2C client and driver.
 ***********************************************************************/

/*! tvp5150 Reset function.
 *
 *  @return		None.
 */
static void tvp5150_hard_reset(void)
{
const struct i2c_reg_value *regs;
#ifdef DEBUG
	printk("In tvp5150:tvp5150_hard_reset\n");
#endif

	regs = tvp5150_init_default;

	while (regs->reg != 0xff) {
		tvp5150_write_reg(regs->reg, regs->value);
		regs++;
	}
}




static int tvp5150_regulator_enable(struct device *dev)
{
	int ret = 0;

	dvddio_regulator = devm_regulator_get(dev, "DOVDD");

	if (!IS_ERR(dvddio_regulator)) {
		regulator_set_voltage(dvddio_regulator,
				      TVP5150_VOLTAGE_DIGITAL_IO,
				      TVP5150_VOLTAGE_DIGITAL_IO);
		ret = regulator_enable(dvddio_regulator);
		if (ret) {
			dev_err(dev, "set io voltage failed\n");
			return ret;
		} else {
			dev_dbg(dev, "set io voltage ok\n");
		}
	} else {
		dev_warn(dev, "cannot get io voltage\n");
	}

	dvdd_regulator = devm_regulator_get(dev, "DVDD");
	if (!IS_ERR(dvdd_regulator)) {
		regulator_set_voltage(dvdd_regulator,
				      TVP5150_VOLTAGE_DIGITAL_CORE,
				      TVP5150_VOLTAGE_DIGITAL_CORE);
		ret = regulator_enable(dvdd_regulator);
		if (ret) {
			dev_err(dev, "set core voltage failed\n");
			return ret;
		} else {
			dev_dbg(dev, "set core voltage ok\n");
		}
	} else {
		dev_warn(dev, "cannot get core voltage\n");
	}

	avdd_regulator = devm_regulator_get(dev, "AVDD");
	if (!IS_ERR(avdd_regulator)) {
		regulator_set_voltage(avdd_regulator,
				      TVP5150_VOLTAGE_ANALOG,
				      TVP5150_VOLTAGE_ANALOG);
		ret = regulator_enable(avdd_regulator);
		if (ret) {
			dev_err(dev, "set analog voltage failed\n");
			return ret;
		} else {
			dev_dbg(dev, "set analog voltage ok\n");
		}
	} else {
		dev_warn(dev, "cannot get analog voltage\n");
	}

	pvdd_regulator = devm_regulator_get(dev, "PVDD");
	if (!IS_ERR(pvdd_regulator)) {
		regulator_set_voltage(pvdd_regulator,
				      TVP5150_VOLTAGE_PLL,
				      TVP5150_VOLTAGE_PLL);
		ret = regulator_enable(pvdd_regulator);
		if (ret) {
			dev_err(dev, "set pll voltage failed\n");
			return ret;
		} else {
			dev_dbg(dev, "set pll voltage ok\n");
		}
	} else {
		dev_warn(dev, "cannot get pll voltage\n");
	}

	return ret;
}

static inline void tvp5150_power_down(int enable)
{
	gpio_set_value_cansleep(pwn_gpio, !enable);
	msleep(2);
}


/*! tvp5150 I2C attach function.
 *
 *  @param *adapter	struct i2c_adapter *.
 *
 *  @return		Error code indicating success or failure.
 */

/*!
 * tvp5150 I2C probe function.
 * Function set in i2c_driver struct.
 * Called by insmod.
 *
 *  @param *adapter	I2C adapter descriptor.
 *
 *  @return		Error code indicating success or failure.
 */

static int tvp5150_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int ret = 0;
	struct pinctrl *pinctrl;
	struct device *dev = &client->dev;

#ifdef DEBUG
	printk("In tvp5150_probe\n");
#endif

	/* pinctrl */
	pinctrl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(pinctrl)) {
		dev_err(dev, "setup pinctrl failed\n");
		return PTR_ERR(pinctrl);
	}

	/* request power down pin */
	pwn_gpio = of_get_named_gpio(dev->of_node, "pwn-gpios", 0);
	if (!gpio_is_valid(pwn_gpio)) {
		dev_err(dev, "no sensor pwdn pin available\n");
		return -ENODEV;
	}

	ret = devm_gpio_request_one(dev, pwn_gpio, GPIOF_OUT_INIT_HIGH,
					"tvp5150_pwdn");
	if (ret < 0) {
		dev_err(dev, "no power pin available!\n");
		return ret;
	}

        tvp5150_regulator_enable(dev);
	tvp5150_power_down(0);

	msleep(1);

	/* Set initial values for the sensor struct. */
	memset(&tvp5150_data, 0, sizeof(tvp5150_data));
	tvp5150_data.i2c_client = client;
	tvp5150_data.streamcap.timeperframe.denominator = 25;
	tvp5150_data.streamcap.timeperframe.numerator = 1;
	tvp5150_data.std_id = V4L2_STD_ALL;
	video_idx = TVP5150_NOT_LOCKED;
	tvp5150_data.pix.width = video_fmts[video_idx].raw_width;
	tvp5150_data.pix.height = video_fmts[video_idx].raw_height;
	/*
	 * Posible v4l2 YUV 4:2:2 values:
	 *
	 * #define V4L2_PIX_FMT_YUYV    v4l2_fourcc('Y', 'U', 'Y', 'V') // 16  YUV 4:2:2
	 * #define V4L2_PIX_FMT_UYVY    v4l2_fourcc('U', 'Y', 'V', 'Y') // 16  YUV 4:2:2
	 * #define V4L2_PIX_FMT_VYUY    v4l2_fourcc('V', 'Y', 'U', 'Y') // 16  YUV 4:2:2
	 * #define V4L2_PIX_FMT_YUV422P v4l2_fourcc('4', '2', '2', 'P') // 16  YVU422 planar
	 */
	tvp5150_data.pix.pixelformat = V4L2_PIX_FMT_UYVY;  /* YUV422 */
	tvp5150_data.pix.priv = 1;  /* 1 is used to indicate TV in */
	tvp5150_data.on = true;

	ret = of_property_read_u32(dev->of_node, "csi_id",
					&(tvp5150_data.csi));
	if (ret) {
		dev_err(dev, "csi_id invalid\n");
		return ret;
	}


	tvp5150_data.sen.sensor_clk = devm_clk_get(dev, "csi_mclk");
	if (IS_ERR(tvp5150_data.sen.sensor_clk)) {
		dev_err(dev, "get mclk failed\n");
		return PTR_ERR(tvp5150_data.sen.sensor_clk);
	}

	ret = of_property_read_u32(dev->of_node, "mclk",
					&tvp5150_data.sen.mclk);
	if (ret) {
		dev_err(dev, "mclk frequency is invalid\n");
		return ret;
	}

	ret = of_property_read_u32(
		dev->of_node, "mclk_source",
		(u32 *) &(tvp5150_data.sen.mclk_source));
	if (ret) {
		dev_err(dev, "mclk_source invalid\n");
		return ret;
	}

	ret = of_property_read_u32(dev->of_node, "csi_id",
					&(tvp5150_data.sen.csi));
	if (ret) {
		dev_err(dev, "csi_id invalid\n");
		return ret;
	}

	clk_prepare_enable(tvp5150_data.sen.sensor_clk);



//#ifdef DEBUG
	printk(
		"%s:tvp5150 probe i2c address is 0x%02X \n",
		__func__, tvp5150_data.i2c_client->addr);
//#endif

	/*! tvp5150 initialization. */
	tvp5150_hard_reset();

#ifdef DEBUG
	printk("<7>""   type is %d (expect %d)\n",
		 tvp5150_int_device.type, v4l2_int_type_slave);
#endif

#ifdef DEBUG
	printk("<7>""   num ioctls is %d\n",
		 tvp5150_int_device.u.slave->num_ioctls);
#endif

	/* This function attaches this structure to the /dev/video0 device.
	 * The pointer in priv points to the tvp5150_data structure here.*/
	tvp5150_int_device.priv = &tvp5150_data;
	ret = v4l2_int_device_register(&tvp5150_int_device);

	clk_disable_unprepare(tvp5150_data.sen.sensor_clk);


	return ret;
}

 static int tvp5150_detach(struct i2c_client *client)
{

#ifdef DEBUG
	printk(
		"%s:Removing %s video decoder @ 0x%02X from adapter %s \n",
		__func__, IF_NAME, client->addr, client->adapter->name);
#endif

	/*if (plat_data->pwdn)
		plat_data->pwdn(1);
        */
	if (dvddio_regulator) {
		regulator_disable(dvddio_regulator);
		regulator_put(dvddio_regulator);
	}

	if (dvdd_regulator) {
		regulator_disable(dvdd_regulator);
		regulator_put(dvdd_regulator);
	}

	if (avdd_regulator) {
		regulator_disable(avdd_regulator);
		regulator_put(avdd_regulator);
	}

	if (pvdd_regulator) {
		regulator_disable(pvdd_regulator);
		regulator_put(pvdd_regulator);
	}

	v4l2_int_device_unregister(&tvp5150_int_device);

	return 0;
}

static __init int tvp5150_init(void)
{
	u8 err = 0;

#ifdef DEBUG
	printk("In tvp5150_init\n");
#endif

	err = i2c_add_driver(&tvp5150_i2c_driver);
	if (err != 0)
		printk("<3>""%s:driver registration failed, error=%d \n",
			__func__, err);

	return err;
}


static void __exit tvp5150_exit(void)
{
#ifdef DEBUG
	printk("In tvp5150_clean\n");
#endif
	i2c_del_driver(&tvp5150_i2c_driver);
//	gpio_sensor_inactive();
}

module_init(tvp5150_init);
module_exit(tvp5150_exit);

MODULE_AUTHOR("Chipwork");
MODULE_DESCRIPTION("Texas Instrument TVP5150 MXC V4L2 video decoder driver");
MODULE_LICENSE("GPL");

