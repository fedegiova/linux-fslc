/*
 * Copyright (c) Chipwork s.n.c.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation.
*/

#ifndef _LM49352_H
#define _LM49352_H


/* LM49352 registers*/
#define BASIC_SETUP_PMC_SETUP                0x00
#define BASIC_SETUP_PMC_CLOCK                0x01
#define PMC_CLOCK_DIV 		             0x02
#define PLL_CLK_SEL                          0x03
#define PLL_M                                0x04
#define PLL_N                                0x05
#define PLL_N_MOD                            0x06
#define PLL_P1                               0x07
#define PLL_P2                               0x08

#define ANALOG_MIXER_CLASSD                  0x10
#define ANALOG_MIXER_HEADPHONESL             0x11
#define ANALOG_MIXER_HEADPHONESR             0x12
#define ANALOG_MIXER_AUX_OUT                 0x13
#define ANALOG_MIXER_OUTPUT_OPTIONS          0x14
#define ANALOG_MIXER_ADC                     0x15
#define ANALOG_MIXER_MIC_LVL                 0x16

#define ANALOG_MIXER_AUXL_LVL                0x18
#define ANALOG_MIXER_MONO_LVL                0x19

#define ANALOG_MIXER_HP_SENSE                0x1b

#define ADC_BASIC                            0x20
#define ADC_CLOCK                            0x21

#define ADC_MIXER                            0x23

#define DAC_BASIC                            0x30
#define DAC_CLOCK                            0x31

#define DIGITAL_MIXER_IPLVL1                 0x40
#define DIGITAL_MIXER_IPLVL2                 0x41
#define DIGITAL_MIXER_OPPORT1                0x42
#define DIGITAL_MIXER_OPPORT2                0x43
#define DIGITAL_MIXER_OPDAC                  0x44
#define DIGITAL_MIXER_OPDECI                 0x45

#define AUDIO_PORT1_BASIC                    0x50
#define AUDIO_PORT1_CLKGEN1                  0x51
#define AUDIO_PORT1_CLKGEN2                  0x52
#define AUDIO_PORT1_SYNC_GEN                 0x53
#define AUDIO_PORT1_DATA_WIDTH               0x54
#define AUDIO_PORT1_RX_MODE	             0x55
#define AUDIO_PORT1_TX_MODE	             0x56

#define AUDIO_PORT2_BASIC                    0x60
#define AUDIO_PORT2_CLKGEN1                  0x61
#define AUDIO_PORT2_CLKGEN2                  0x62
#define AUDIO_PORT2_SYNC_GEN                 0x63
#define AUDIO_PORT2_DATA_WIDTH               0x64
#define AUDIO_PORT2_RX_MODE	             0x65
#define AUDIO_PORT2_TX_MODE	             0x66

#define ADC_FX			             0x70
#define DAC_FX			             0x71

#define ADC_EFFECTS_HPF                      0x80
#define ADC_EFFECTS_ADC_ALC1                 0x81
#define ADC_EFFECTS_ADC_ALC2                 0x82
#define ADC_EFFECTS_ADC_ALC3                 0x83
#define ADC_EFFECTS_ADC_ALC4                 0x84
#define ADC_EFFECTS_ADC_ALC5                 0x85
#define ADC_EFFECTS_ADC_ALC6                 0x86
#define ADC_EFFECTS_ADC_ALC7                 0x87
#define ADC_EFFECTS_ADC_ALC8                 0x88
#define ADC_EFFECTS_ADC_L_LEVEL              0x89
#define ADC_EFFECTS_ADC_R_LEVEL              0x8a
#define ADC_EFFECTS_EQ_BAND1	             0x8b
#define ADC_EFFECTS_EQ_BAND2	             0x8c
#define ADC_EFFECTS_EQ_BAND3	             0x8d
#define ADC_EFFECTS_EQ_BAND4	             0x8e
#define ADC_EFFECTS_EQ_BAND5	             0x8f
#define ADC_EFFECTS_SOFT_CLIP1	             0x90
#define ADC_EFFECTS_SOFT_CLIP2	             0x91
#define ADC_EFFECTS_SOFT_CLIP3	             0x92

#define ADC_EFFECTS_MONITOR_L_LEVEL          0x98
#define ADC_EFFECTS_MONITOR_R_LEVEL          0x99
#define ADC_EFFECTS_MONITOR_FX_CLIP          0x9a
#define ADC_EFFECTS_MONITOR_L_ALC	     0x9b
#define ADC_EFFECTS_MONITOR_R_ALC            0x9c


#define DAC_EFFECTS_DAC_ALC1                 0xa0
#define DAC_EFFECTS_DAC_ALC2                 0xa1
#define DAC_EFFECTS_DAC_ALC3                 0xa2
#define DAC_EFFECTS_DAC_ALC4                 0xa3
#define DAC_EFFECTS_DAC_ALC5                 0xa4
#define DAC_EFFECTS_DAC_ALC6                 0xa5
#define DAC_EFFECTS_DAC_ALC7                 0xa6
#define DAC_EFFECTS_DAC_ALC8                 0xa7
#define DAC_EFFECTS_DAC_L_LEVEL              0xa8
#define DAC_EFFECTS_DAC_R_LEVEL              0xa9
#define DAC_EFFECTS_EQ_BAND1		     0xab
#define DAC_EFFECTS_EQ_BAND2		     0xac
#define DAC_EFFECTS_EQ_BAND3		     0xad
#define DAC_EFFECTS_EQ_BAND4		     0xae
#define DAC_EFFECTS_EQ_BAND5		     0xaf
#define DAC_EFFECTS_SOFT_CLIP1		     0xb0
#define DAC_EFFECTS_SOFT_CLIP2		     0xb1
#define DAC_EFFECTS_SOFT_CLIP3		     0xb2

#define DAC_EFFECTS_MONITOR_L_LEVEL          0xb8
#define DAC_EFFECTS_MONITOR_R_LEVEL          0xb9
#define DAC_EFFECTS_MONITOR_L_ALC	     0xbb
#define DAC_EFFECTS_MONITOR_R_ALC            0xbc

#define DAC_EFFECTS_MONITOR_R_ALC            0xbc

#define GPIO1			             0xe0
#define GPIO2			             0xe1

#define SPREAD_SPECTRUM_RESET                0xf0
#define SPREAD_SPECTRUM_SS                   0xf1
#define SPREAD_SPECTRUM_FORCE                0xfe


#define LM49352_DAC_CONTROL3                  0x71
#define LM49352_DAC_CONTROL4                  0xff
#define LM49352_DAC_CONTROL5                  0x30
#define LM49352_RESET                         0xf0
#define LM49352_ADC_CONTROL1                  0x20
#define LM49352_MUTE                          0x0C
#define LM49352_DIGITAL_ATTENUATION_DACL1     0xA8
#define LM49352_DIGITAL_ATTENUATION_DACR1     0xA9


#define LM49352_DAI_DAC 0
#define LM49352_DAI_ADC 1

#endif
