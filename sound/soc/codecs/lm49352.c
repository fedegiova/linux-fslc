/*
 * lm49352.c  --  lm49352 ALSA SoC Audio driver
 *
 * Copyright 2014 Chipwork s.n.c.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * TODO: Input DSP support
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/of_device.h>

#include "lm49352.h"

//#define DEBUG

#define	SND_SOC_BIAS_OFF	0
#define	SND_SOC_BIAS_STANDBY	1
#define	SND_SOC_BIAS_PREPARE	2
#define	SND_SOC_BIAS_ON		3

enum Synth_Num  {
SYNTH_128_128,
SYNTH_100_128,
SYNTH_96_128,
SYNTH_80_128,
SYNTH_72_128,
SYNTH_64_128,
SYNTH_48_128,
SYNTH_0_128
};

/* codec private data */
struct lm49352_priv {
	struct regmap *regmap;
	int sysclk[2];
};


#define LM49352_DAP_REG_OFFSET	0x00
#define LM49352_MAX_REG_OFFSET	0xff

/* default value of sgtl5000 registers */
static const struct reg_default lm49352_reg_defs[] = {
    {PMC_CLOCK_DIV,           0x50},
    {DAC_BASIC,               0x02},
    {DAC_CLOCK,               0x03},
    {ADC_EFFECTS_ADC_ALC4,   0x0A},
    {ADC_EFFECTS_ADC_ALC5,   0x0A},
    {ADC_EFFECTS_ADC_ALC6,   0x0A},
    {ADC_EFFECTS_ADC_ALC7,   0x1F},
    {ADC_EFFECTS_ADC_L_LEVEL, 0x33},
    {ADC_EFFECTS_ADC_R_LEVEL, 0x33},
    {DAC_EFFECTS_DAC_ALC4,   0x0A},
    {DAC_EFFECTS_DAC_ALC5,   0x0A},
    {DAC_EFFECTS_DAC_ALC6,   0x0A},
    {DAC_EFFECTS_DAC_ALC7,   0x33},
    {DAC_EFFECTS_DAC_L_LEVEL, 0x33},
    {DAC_EFFECTS_DAC_R_LEVEL, 0x33},
    {SPREAD_SPECTRUM_RESET,   0x02},
};


static int lm49352_reset(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, SPREAD_SPECTRUM_RESET, 0x20);
	udelay(10);
	return snd_soc_write(codec, SPREAD_SPECTRUM_RESET, 0x00);
}



static int lm49352_out_vu(struct snd_kcontrol *kcontrol,
                         struct snd_ctl_elem_value *ucontrol)
{
       struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
       int reg = kcontrol->private_value & 0xff;
       int ret;
       u16 val = 0;

       /* Resetting the reg cache value. */
//       lm49352_write_reg_cache(codec, reg, 0);

       /* Callback to set the value of a single mixer control. */
       ret = snd_soc_put_volsw(kcontrol, ucontrol);

       if (ret < 0)
               return ret;

       /* Reading the written new value. */
//       val = lm49352_read_reg_cache(codec, reg);
       /* Oring the value with 0x100 and writing back to reg cavhe. */

       return snd_soc_write(codec,reg, val);

//lm49352_write(codec, reg, val | 0x0100);
}

static const DECLARE_TLV_DB_SCALE(hp_tlv, -1800,300 , 0);
static const DECLARE_TLV_DB_SCALE(dac_tlv, -7650, 150, 0);
static const DECLARE_TLV_DB_SCALE(adc_tlv, -4650, 150, 0);

/* Macro for defining and adding single mixer controls. */
#define SOC_LM49352_OUT_SINGLE_R_TLV(xname, reg, shift, max, invert, tlv_array)\
{      .iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
       .access = SNDRV_CTL_ELEM_ACCESS_TLV_READ \
                 | SNDRV_CTL_ELEM_ACCESS_READWRITE,\
       .tlv.p = (tlv_array), \
       .info = snd_soc_info_volsw, \
       .get = snd_soc_get_volsw, .put = lm49352_out_vu, \
       .private_value = SOC_SINGLE_VALUE(reg, shift, max, invert, 0 ) }


static const struct snd_kcontrol_new lm49352_snd_controls[] =
{      /* Various controls of LM49352 Driver */
       SOC_LM49352_OUT_SINGLE_R_TLV("Left DAC1 Playback Volume",
               LM49352_DIGITAL_ATTENUATION_DACL1, 0, 0x3f, 0, dac_tlv),
       SOC_LM49352_OUT_SINGLE_R_TLV("Right DAC1 Playback Volume",
               LM49352_DIGITAL_ATTENUATION_DACR1, 0, 0x3f, 0, dac_tlv),
       SOC_LM49352_OUT_SINGLE_R_TLV("Headphone Playback Volume",
               ANALOG_MIXER_OUTPUT_OPTIONS, 1, 7, 1, hp_tlv),
       SOC_LM49352_OUT_SINGLE_R_TLV("ADC AUX Capture Volume",
               ANALOG_MIXER_AUXL_LVL, 0, 43, 0, adc_tlv),
       SOC_LM49352_OUT_SINGLE_R_TLV("ADC Left Capture Volume",
               ADC_EFFECTS_ADC_L_LEVEL, 0, 0x3f, 0, dac_tlv),
       SOC_LM49352_OUT_SINGLE_R_TLV("ADC Right Capture Volume",
               ADC_EFFECTS_ADC_R_LEVEL, 0, 0x3f, 0, dac_tlv),
/*
	SOC_LM49352_OUT_SINGLE_R_TLV("Aux Playback Volume",
               ANALOG_MIXER_OUTPUT_OPTIONS, 4, 1, 0, dac_tlv),
       SOC_LM49352_OUT_SINGLE_R_TLV("Mono Playback Volume",
               ANALOG_MIXER_OUTPUT_OPTIONS, 6, 3, 0, dac_tlv),
       SOC_SINGLE("DAC1 Deemphasis Switch", LM49352_DAC_CONTROL3, 2, 1, 0),
       SOC_SINGLE("DAC1 Left Invert Switch", LM49352_DAC_CONTROL4, 0, 1, 0),
       SOC_SINGLE("DAC1 Right Invert Switch", LM49352_DAC_CONTROL4, 1, 1, 0),
       SOC_SINGLE("DAC ZC Switch", DAC_BASIC, 5, 1, 0),
      SOC_SINGLE("DAC Mute Switch", DAC_BASIC, 2, 1, 0),
       SOC_SINGLE("ADCL Mute Switch", LM49352_ADC_CONTROL1, 2, 1, 0),
       SOC_SINGLE("ADCR Mute Switch", LM49352_ADC_CONTROL1, 3, 1, 0),
*/
};

/*
static const struct snd_kcontrol_new inmix_controls[] = {
SOC_DAPM_SINGLE("AIN1 Switch", WM8776_ADCMUX, 0, 1, 0),
SOC_DAPM_SINGLE("AIN2 Switch", WM8776_ADCMUX, 1, 1, 0),
SOC_DAPM_SINGLE("AIN3 Switch", WM8776_ADCMUX, 2, 1, 0),
SOC_DAPM_SINGLE("AIN4 Switch", WM8776_ADCMUX, 3, 1, 0),
SOC_DAPM_SINGLE("AIN5 Switch", WM8776_ADCMUX, 4, 1, 0),
};

static const struct snd_kcontrol_new outmix_controls[] = {
SOC_DAPM_SINGLE("DAC Switch", WM8776_OUTMUX, 0, 1, 0),
SOC_DAPM_SINGLE("AUX Switch", WM8776_OUTMUX, 1, 1, 0),
SOC_DAPM_SINGLE("Bypass Switch", WM8776_OUTMUX, 2, 1, 0),
};

static const struct snd_soc_dapm_widget lm49352_dapm_widgets[] = {
SND_SOC_DAPM_INPUT("AUX"),

SND_SOC_DAPM_INPUT("AIN1"),
SND_SOC_DAPM_INPUT("AIN2"),
SND_SOC_DAPM_INPUT("AIN3"),
SND_SOC_DAPM_INPUT("AIN4"),
SND_SOC_DAPM_INPUT("AIN5"),

SND_SOC_DAPM_MIXER("Input Mixer", WM8776_PWRDOWN, 6, 1,
		   inmix_controls, ARRAY_SIZE(inmix_controls)),

SND_SOC_DAPM_ADC("ADC", "Capture", WM8776_PWRDOWN, 1, 1),
SND_SOC_DAPM_DAC("DAC", "Playback", WM8776_PWRDOWN, 2, 1),

SND_SOC_DAPM_MIXER("Output Mixer", SND_SOC_NOPM, 0, 0,
		   outmix_controls, ARRAY_SIZE(outmix_controls)),

SND_SOC_DAPM_PGA("Headphone PGA", WM8776_PWRDOWN, 3, 1, NULL, 0),

SND_SOC_DAPM_OUTPUT("VOUT"),

SND_SOC_DAPM_OUTPUT("HPOUTL"),
SND_SOC_DAPM_OUTPUT("HPOUTR"),
};

static const struct snd_soc_dapm_route routes[] = {
	{ "Input Mixer", "AIN1 Switch", "AIN1" },
	{ "Input Mixer", "AIN2 Switch", "AIN2" },
	{ "Input Mixer", "AIN3 Switch", "AIN3" },
	{ "Input Mixer", "AIN4 Switch", "AIN4" },
	{ "Input Mixer", "AIN5 Switch", "AIN5" },

	{ "ADC", NULL, "Input Mixer" },

	{ "Output Mixer", "DAC Switch", "DAC" },
	{ "Output Mixer", "AUX Switch", "AUX" },
	{ "Output Mixer", "Bypass Switch", "Input Mixer" },

	{ "VOUT", NULL, "Output Mixer" },

	{ "Headphone PGA", NULL, "Output Mixer" },

	{ "HPOUTL", NULL, "Headphone PGA" },
	{ "HPOUTR", NULL, "Headphone PGA" },
};
*/

static const struct snd_kcontrol_new inmix_controls[] = {
SOC_DAPM_SINGLE("DACR_to_ADCR",	ANALOG_MIXER_ADC, 0, 1, 0),
SOC_DAPM_SINGLE("DACL_to_ADCL",	ANALOG_MIXER_ADC, 1, 1, 0),
SOC_DAPM_SINGLE("MIC_to_ADCR",	ANALOG_MIXER_ADC, 2, 1, 0),
SOC_DAPM_SINGLE("MIC_to_ADCL",	ANALOG_MIXER_ADC, 3, 1, 0),
SOC_DAPM_SINGLE("AUX_to_ADCR",	ANALOG_MIXER_ADC, 4, 1, 0),
SOC_DAPM_SINGLE("MONO_to_ADCL",	ANALOG_MIXER_ADC, 5, 1, 0),
};

static const struct snd_kcontrol_new outmix_controls_l[] = {
SOC_DAPM_SINGLE("DACR_HPL", ANALOG_MIXER_HEADPHONESL, 0, 1, 0),
SOC_DAPM_SINGLE("DACL_HPL", ANALOG_MIXER_HEADPHONESL, 1, 1, 0),
SOC_DAPM_SINGLE("MONO_HPL", ANALOG_MIXER_HEADPHONESL, 4, 1, 0),
SOC_DAPM_SINGLE("AUX_HPL", ANALOG_MIXER_HEADPHONESL, 5, 1, 0),
};

static const struct snd_kcontrol_new outmix_controls[] = {
SOC_DAPM_SINGLE("DACR_HPR", ANALOG_MIXER_HEADPHONESR, 0, 1, 0),
SOC_DAPM_SINGLE("DACL_HPR", ANALOG_MIXER_HEADPHONESR, 1, 1, 0),
SOC_DAPM_SINGLE("MONO_HPR", ANALOG_MIXER_HEADPHONESR, 4, 1, 0),
SOC_DAPM_SINGLE("AUX_HPR", ANALOG_MIXER_HEADPHONESR, 5, 1, 0),
};

static const struct snd_soc_dapm_widget lm49352_dapm_widgets[] = {
SND_SOC_DAPM_MIXER("Input Mixer", SND_SOC_NOPM, 0, 0,
		   inmix_controls, ARRAY_SIZE(inmix_controls)),
SND_SOC_DAPM_MIXER("Output Mixer Right", SND_SOC_NOPM, 0, 0,
		   outmix_controls, ARRAY_SIZE(outmix_controls)),
SND_SOC_DAPM_MIXER("Output Mixer Left", SND_SOC_NOPM, 0, 0,
		   outmix_controls_l, ARRAY_SIZE(outmix_controls_l)),
SND_SOC_DAPM_ADC("ADC", "Capture", ADC_BASIC, 7, 1),
SND_SOC_DAPM_DAC("DAC", "Playback", DAC_BASIC, 7, 1),
SND_SOC_DAPM_INPUT("LINE_IN"),
SND_SOC_DAPM_INPUT("MIC_IN"),
SND_SOC_DAPM_INPUT("DAC_IN"),
SND_SOC_DAPM_OUTPUT("LINE_OUT"),
};

static const struct snd_soc_dapm_route routes[] = {
	{"Input Mixer", "AUX_to_ADCR", "LINE_IN"},	/* line_in --> adc_mux */
	{"Input Mixer", "MIC_to_ADCR", "MIC_IN"},	/* line_in --> adc_mux */
	{ "ADC", NULL, "Input Mixer" },
	{"LINE_OUT", NULL, "DAC"},		/* dac --> line_out */
};



static int lm49352_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;

	u16 aif_val;
#ifdef DEBUG
	int i = 0,val = 0;
#endif
	int clk_phase = 0;
	int Justified = 0,Mode = 0;

	aif_val = 0x7; //Enable TX/RX

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		aif_val = aif_val + 0x10; //Master-> Frame Sync
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		aif_val = aif_val + 0x08; //Master-> Clock
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		aif_val =  aif_val + 0x18;//Master-> Frame Sync+Clock
		break;
	default:
		return -EINVAL;
	}


	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		Mode = (1 << 1);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		Mode = (1 << 1);
		Justified = 0;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		Mode = (1 << 1);
		Justified = 1;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		Mode = 0;
		clk_phase = (1 << 5);
		Justified = 1;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		Mode = 0;
		clk_phase = (1 << 5);
		Justified = 0;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, AUDIO_PORT1_BASIC,0x3f,(aif_val/* | mode*/ | clk_phase));
	snd_soc_write(codec, AUDIO_PORT1_RX_MODE,Justified | Mode);
	snd_soc_write(codec, AUDIO_PORT1_TX_MODE,Justified | Mode);

#ifdef DEBUG
	pr_info("LM49352 Register Dump Register\n");
	for (i = 0; i < LM49352_MAX_REG_OFFSET ; i++) {
    	val = snd_soc_read(codec,i);
	pr_info("(0x%x)->(0x%x)\n",i,val);
	}
#endif

	return 0;
}

static int lm49352_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
//	struct lm49352_priv *lm49352 = snd_soc_codec_get_drvdata(codec);
	int iface;
	int lenght = 0,pll_selector = 1;
	unsigned long clock,master_clock,result;
	int synth;

	iface = 0;

	/* Set word length */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		iface = 0x1b;
                snd_soc_write(codec, AUDIO_PORT1_SYNC_GEN, 0x2);
		lenght = 16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
                snd_soc_write(codec, AUDIO_PORT1_SYNC_GEN, 0x4);
		iface = 0x09;
		lenght = 20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
                snd_soc_write(codec, AUDIO_PORT1_SYNC_GEN, 0x5);
		iface = 0x00;
		lenght = 24;
		break;
//	case SNDRV_PCM_FORMAT_S32_LE:
//		break;
	default:
	        return -EINVAL;
	}

	snd_soc_write(codec, AUDIO_PORT1_DATA_WIDTH,iface);

#ifdef DEBUG
      pr_info("Data Lenght:%d\n",lenght);
      pr_info("DAC Rate:%d\n",params_rate(params));
#endif

       /* Setting the DAC and ADC clock dividers based on substream
          sample rate. */
       switch (params_rate(params)) {
       case 8000:
               snd_soc_write(codec, DAC_CLOCK, 0x17);
               snd_soc_write(codec, ADC_CLOCK, 0xb);
               break;
       case 11025:
               snd_soc_write(codec, DAC_CLOCK, 0xf);
               snd_soc_write(codec, ADC_CLOCK, 0x7);
	       pll_selector = 2;
               break;
       case 16000:
               snd_soc_write(codec, DAC_CLOCK, 0x0b);
               snd_soc_write(codec, ADC_CLOCK, 0x05);
               break;
       case 22050:
               snd_soc_write(codec, DAC_CLOCK, 0x07);
               snd_soc_write(codec, ADC_CLOCK, 0x03);
	       pll_selector = 2;
               break;
       case 32000:
               snd_soc_write(codec, DAC_CLOCK, 0x05);
               snd_soc_write(codec, ADC_CLOCK, 0x02);
               break;
       case 44100:
               snd_soc_write(codec, DAC_CLOCK, 0x03);
               snd_soc_write(codec, ADC_CLOCK, 0x01);
	       pll_selector = 2;
               break;
       case 48000:
               snd_soc_write(codec, DAC_CLOCK, 0x03);
               snd_soc_write(codec, ADC_CLOCK, 0x01);
               break;
       case 96000:
               snd_soc_write(codec, DAC_CLOCK, 0x01);
               break;
       default:
	        return -EINVAL;
               break;
       }


	if (pll_selector == 1)
        {        
	 //---------------------------Setup PLL to 12.2880 Mhz-------------------------
	 /*PLL M = 2.5*/	
	 snd_soc_write(codec,PLL_M,0x04);
	 /*PLL N = 32*/	
	 snd_soc_write(codec,PLL_N,0x20);
	 /*PLL MOD = 0*/	
	 snd_soc_write(codec,PLL_N_MOD,0x0);
	 /*PLL_P1 = 12.5  (Value * 2 - 1)*/	
	 snd_soc_write(codec,PLL_P1,0x18);
	 snd_soc_write(codec,DAC_BASIC,0x32);
	 snd_soc_write(codec,ADC_BASIC,0x30);
	 master_clock = 12288000;
        }          
	  else
          {	 
 	  //---------------------------Setup PLL to 11.2896 Mhz-------------------------
	  /*PLL M = 12.5*/	
	  snd_soc_write(codec,PLL_M,0x18);
	  /*PLL N = 147*/	
	  snd_soc_write(codec,PLL_N,0x93);
	  /*PLL MOD = 0*/	
	  snd_soc_write(codec,PLL_N_MOD,0x0);
	  /*PLL_P2 = 12.5  (Value * 2 - 1)*/	
	  snd_soc_write(codec,PLL_P2,0x18);
	  snd_soc_write(codec,DAC_BASIC,0x42);
	  snd_soc_write(codec,ADC_BASIC,0x40);
	  master_clock = 11289600;
          }


	//----> Calculates I2S Clock
        clock = params_rate(params) * lenght * 2;

        result = master_clock % clock;
	//TODO:Accept fractional value
	//---> Check if fractional 
	if (result != 0) 
	  return -EINVAL;
         
	result = master_clock / clock;
	//---> Check Divisor Limit 
        if (result == 48)
	{        
	result = 24; 	
	synth = (int) SYNTH_64_128;
	}
        else if (result > 32)
         return -EINVAL;
	 else
	  synth = (int) SYNTH_128_128;

      	snd_soc_write(codec,AUDIO_PORT1_CLKGEN2,synth);
	result = (result * 2) - 1;
      	snd_soc_write(codec,AUDIO_PORT1_CLKGEN1,result);

#ifdef DEBUG
      pr_info("PLL selected:%d,SYNTH DENOM:%d",pll_selector,synth);
      pr_info("I2S Clock:%lu Port Clock:%lu\n",clock,result);
#endif

	return 0;
}

static int lm49352_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;

	if (mute)
    	snd_soc_update_bits(codec, GPIO1, 0x40, 0x0);
	else
    	snd_soc_update_bits(codec, GPIO1, 0x40, 0x40);
	return 0;
}

static int lm49352_set_sysclk(struct snd_soc_dai *dai,
			     int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct lm49352_priv *lm49352 = snd_soc_codec_get_drvdata(codec);

	BUG_ON(dai->driver->id >= ARRAY_SIZE(lm49352->sysclk));

	lm49352->sysclk[dai->driver->id] = freq;

	return 0;
}

static int lm49352_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
    int ret;
    struct lm49352_priv * lm49352 = snd_soc_codec_get_drvdata(codec);
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {

			/* Disable the global powerdown; DAPM does the rest */
			snd_soc_update_bits(codec, BASIC_SETUP_PMC_SETUP, 1, 0);
			regcache_cache_only(lm49352->regmap, false);

			ret = regcache_sync(lm49352->regmap);
			if (ret != 0) {
				dev_err(codec->dev,
					"Failed to restore cache: %d\n", ret);

				regcache_cache_only(lm49352->regmap, true);
				return ret;
			}
		}

		break;
	case SND_SOC_BIAS_OFF:
		regcache_cache_only(lm49352->regmap, true);
		snd_soc_update_bits(codec, BASIC_SETUP_PMC_SETUP, 1, 1);
		break;
	}

	codec->dapm.bias_level = level;
	return 0;
}

#define LM49352_RATES SNDRV_PCM_RATE_8000_96000

#define LM49352_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
			SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_ops lm49352_ops = {
	.digital_mute	= lm49352_mute,
	.hw_params      = lm49352_hw_params,
	.set_fmt        = lm49352_set_fmt,
	.set_sysclk     = lm49352_set_sysclk,
};

static struct snd_soc_dai_driver lm49352_dai = {
		.name = "lm49352",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = LM49352_RATES,
			.formats = LM49352_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = LM49352_RATES,
			.formats = LM49352_FORMATS,
		},
		.ops = &lm49352_ops,
};

#ifdef CONFIG_SUSPEND
static int lm49352_suspend(struct snd_soc_codec *codec)
{
	lm49352_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int lm49352_resume(struct snd_soc_codec *codec)
{
    //removed fede
	//int i;
	//u8 data[2];
	//u16 *cache = codec->reg_cache;

	///* Sync reg_cache with the hardware */
	//for (i = 0; i < ARRAY_SIZE(lm49352_reg); i++) {
	//	if (cache[i] == lm49352_reg[i])
	//		continue;
	//	data[0] = i;
	//	data[1] = cache[i] ;
	//	codec->hw_write(codec->control_data, data, 2);
	//}

	lm49352_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}
#else
#define lm49352_suspend NULL
#define lm49352_resume NULL
#endif

static int lm49352_probe(struct snd_soc_codec *codec)
{
//	struct lm49352_priv *lm49352 = snd_soc_codec_get_drvdata(codec);
//	struct snd_soc_dapm_context *dapm = &codec->dapm;
#ifdef DEBUG
	int val;
#endif
	int ret = 0;

    pr_info("### probe called \n");

	ret = lm49352_reset(codec);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to issue reset: %d\n", ret);
		return ret;
	}

	lm49352_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* Latch the update bits; right channel only since we always
	 * update both. */

	/*Chip Enable ,force MCLK input activation and PLL1/2 on*/
	snd_soc_write(codec,BASIC_SETUP_PMC_SETUP,0x17);
	/*MCLK input for PMC*/
	snd_soc_write(codec,BASIC_SETUP_PMC_CLOCK,0x0);
	/*MCLK divisor*/
	snd_soc_write(codec,PMC_CLOCK_DIV,0x50);

	//---------------------------Setup PLL to 12.2880 Mhz-------------------------
	/*PLL M = 2.5*/
	snd_soc_write(codec,PLL_M,0x04);
	/*PLL N = 32*/
	snd_soc_write(codec,PLL_N,0x20);
	/*PLL MOD = 0*/
	snd_soc_write(codec,PLL_N_MOD,0x0);
	/*PLL_P1 = 12.5  (Value * 2 - 1)*/
	snd_soc_write(codec,PLL_P1,0x18);
	snd_soc_write(codec,PLL_P2,0x18);

	/*Setup PORT1 TX/RX Link*/
	snd_soc_write(codec,DIGITAL_MIXER_OPPORT1,0x05);
	/*Setup DAC Input*/
	snd_soc_write(codec,DIGITAL_MIXER_OPDAC,0x09);

//------>Enable Power Amplifer when HP is ON
	snd_soc_write(codec,GPIO1,0x0a);
//	snd_soc_write(codec,DAC_BASIC,0x32);
//	snd_soc_write(codec,AUDIO_PORT1_CLKGEN1,0xf);

	/*Set Headphone output*/
	snd_soc_write(codec,ANALOG_MIXER_HEADPHONESL,0x02);
	snd_soc_write(codec,ANALOG_MIXER_HEADPHONESR,0x01);

#ifdef DEBUG
	val = snd_soc_read(codec,BASIC_SETUP_PMC_SETUP);
	pr_info("LM49352 Status=0x%x\n",val);
#endif
/*
	snd_soc_add_controls(codec, lm49352_snd_controls,
			     ARRAY_SIZE(lm49352_snd_controls));

	snd_soc_dapm_new_controls(dapm, lm49352_dapm_widgets,
				  ARRAY_SIZE(lm49352_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, routes, ARRAY_SIZE(routes));
*/
	pr_info("Probe LM49352 ALSA SoC Codec Driver %d\n",ret);

	return ret;
}

/* power down chip */
static int lm49352_remove(struct snd_soc_codec *codec)
{
	lm49352_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_lm49352 = {
	.probe = 	lm49352_probe,
	.remove = 	lm49352_remove,
	.suspend = 	lm49352_suspend,
	.resume =	lm49352_resume,
	.set_bias_level = lm49352_set_bias_level,
	.controls = lm49352_snd_controls,
	.num_controls = ARRAY_SIZE(lm49352_snd_controls),
	.dapm_widgets = lm49352_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(lm49352_dapm_widgets),
	.dapm_routes = routes,
	.num_dapm_routes = ARRAY_SIZE(routes),

};

static const struct regmap_config lm49352_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = LM49352_MAX_REG_OFFSET,
	.reg_defaults = lm49352_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(lm49352_reg_defs),
	.cache_type = REGCACHE_RBTREE,
};

static  int lm49352_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct lm49352_priv *lm49352;
	int ret;

	int val;
	val = i2c_smbus_read_byte_data(i2c, PMC_CLOCK_DIV);
	printk("LM 49352 Register 2 Value:%x",val);

	lm49352 = kzalloc(sizeof(struct lm49352_priv), GFP_KERNEL);
	if (lm49352 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, lm49352);

	lm49352->regmap = devm_regmap_init_i2c(i2c, &lm49352_regmap_config);
	if (IS_ERR(lm49352->regmap)) {
		ret = PTR_ERR(lm49352->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	ret =  snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_lm49352, &lm49352_dai,1);
	if (ret < 0)
	{
		pr_info("LM49352 registration error!!!\n");
		kfree(lm49352);
    }
    else
		pr_info("LM49352 registration OK %d!\n",ret);
	return ret;
}

static  int lm49352_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}


static const struct i2c_device_id lm49352_i2c_id[] = {
	{ "lm49352", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm49352_i2c_id);


static const struct of_device_id lm49352_dt_ids[] = {
	{ .compatible = "fsl,lm49352", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, lm49352_dt_ids);


static struct i2c_driver lm49352_i2c_driver = {
	.driver = {
		.name = "lm49352",
		.owner = THIS_MODULE,
		.of_match_table = lm49352_dt_ids,
	},
	.probe =    lm49352_i2c_probe,
	.remove =   lm49352_i2c_remove,
	.id_table = lm49352_i2c_id,
};

static int __init lm49352_modinit(void)
{
	int ret = 0;
	ret = i2c_add_driver(&lm49352_i2c_driver);
	pr_info("LM49352 ALSA SoC Codec Driver module init\n");
	if (ret != 0) {
		printk(KERN_ERR "Failed to register lm49352 I2C driver: %d\n",
		       ret);
	}
	return ret;
}
module_init(lm49352_modinit);

static void __exit lm49352_exit(void)
{
	i2c_del_driver(&lm49352_i2c_driver);
}
module_exit(lm49352_exit);

MODULE_DESCRIPTION("ASoC LM49352 driver");
MODULE_AUTHOR("Chipwork s.n.c. ITALY");
MODULE_LICENSE("GPL");
