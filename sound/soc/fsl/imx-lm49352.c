/*
 * SoC audio for lm49352 codec
 * Copyright (C) 2014 Chipwork s.n.c.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <sound/soc.h>

#include "../codecs/lm49352.h"
#include "imx-audmux.h"

#define DAI_NAME_SIZE	32

struct imx_lm49352_data {
	struct snd_soc_dai_link dai;
	struct snd_soc_card card;
	char codec_dai_name[DAI_NAME_SIZE];
	char platform_name[DAI_NAME_SIZE];
	struct clk *codec_clk;
	unsigned int clk_frequency;
};

static int imx_lm49352_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct imx_lm49352_data *data = container_of(rtd->card,
					struct imx_lm49352_data, card);
	struct device *dev = rtd->card->dev;
	int ret;

	ret = snd_soc_dai_set_sysclk(rtd->codec_dai, 0,
				     data->clk_frequency, SND_SOC_CLOCK_IN);
	if (ret) {
		dev_err(dev, "could not set codec driver clock params\n");
		return ret;
	}

	return 0;
}

static const struct snd_soc_dapm_widget imx_lm49352_dapm_widgets[] = {
	SND_SOC_DAPM_LINE("Line In Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
};

static int imx_lm49352_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *ssi_np, *codec_np;
	struct platform_device *ssi_pdev;
	struct i2c_client *codec_dev;
	struct imx_lm49352_data *data;
	int int_port, ext_port;
	int ret;

	ret = of_property_read_u32(np, "mux-int-port", &int_port);
	if (ret) {
		dev_err(&pdev->dev, "mux-int-port missing or invalid\n");
		return ret;
	}
	ret = of_property_read_u32(np, "mux-ext-port", &ext_port);
	if (ret) {
		dev_err(&pdev->dev, "mux-ext-port missing or invalid\n");
		return ret;
	}

	/*
	 * The port numbering in the hardware manual starts at 1, while
	 * the audmux API expects it starts at 0.
	 */
	int_port--;
	ext_port--;
	ret = imx_audmux_v2_configure_port(int_port,
			IMX_AUDMUX_V2_PTCR_SYN |
			IMX_AUDMUX_V2_PTCR_TFSEL(ext_port) |
			IMX_AUDMUX_V2_PTCR_TCSEL(ext_port) |
			IMX_AUDMUX_V2_PTCR_TFSDIR |
			IMX_AUDMUX_V2_PTCR_TCLKDIR,
			IMX_AUDMUX_V2_PDCR_RXDSEL(ext_port));
	if (ret) {
		dev_err(&pdev->dev, "audmux internal port setup failed\n");
		return ret;
	}
	ret = imx_audmux_v2_configure_port(ext_port,
			IMX_AUDMUX_V2_PTCR_SYN,
			IMX_AUDMUX_V2_PDCR_RXDSEL(int_port));
	if (ret) {
		dev_err(&pdev->dev, "audmux external port setup failed\n");
		return ret;
	}

	ssi_np = of_parse_phandle(pdev->dev.of_node, "ssi-controller", 0);
	codec_np = of_parse_phandle(pdev->dev.of_node, "audio-codec", 0);
	if (!ssi_np || !codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	ssi_pdev = of_find_device_by_node(ssi_np);
	if (!ssi_pdev) {
		dev_err(&pdev->dev, "failed to find SSI platform device\n");
		ret = -EINVAL;
		goto fail;
	}
	codec_dev = of_find_i2c_device_by_node(codec_np);
	if (!codec_dev) {
		dev_err(&pdev->dev, "failed to find codec platform device\n");
		return -EINVAL;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	data->codec_clk = clk_get(&codec_dev->dev, NULL);
	if (IS_ERR(data->codec_clk)) {
		/* assuming clock enabled by default */
		data->codec_clk = NULL;
		data->clk_frequency = 12000000; /* Hardware Fix Value*/
/*		ret = of_property_read_u32(codec_np, "clock-frequency",
					&data->clk_frequency);
		if (ret) {
			dev_err(&codec_dev->dev,
				"clock-frequency missing or invalid\n");
			goto fail;
		}
*/
	} else {
		data->clk_frequency = clk_get_rate(data->codec_clk);
		clk_prepare_enable(data->codec_clk);
	}

	data->dai.name = "HiFi";
	data->dai.stream_name = "HiFi";
	data->dai.codec_dai_name = "lm49352";
	data->dai.codec_of_node = codec_np;
	data->dai.cpu_of_node = ssi_np;
	data->dai.platform_of_node = ssi_np;
	data->dai.init = &imx_lm49352_dai_init;
	data->dai.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			    SND_SOC_DAIFMT_CBM_CFM;

	data->card.dev = &pdev->dev;
	ret = snd_soc_of_parse_card_name(&data->card, "model");
	if (ret)
		goto clk_fail;
	ret = snd_soc_of_parse_audio_routing(&data->card, "audio-routing");
	if (ret)
		goto clk_fail;
	data->card.num_links = 1;
	data->card.owner = THIS_MODULE;
	data->card.dai_link = &data->dai;
	data->card.dapm_widgets = imx_lm49352_dapm_widgets;
	data->card.num_dapm_widgets = ARRAY_SIZE(imx_lm49352_dapm_widgets);

	ret = snd_soc_register_card(&data->card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto clk_fail;
	}

	platform_set_drvdata(pdev, data);
clk_fail:
	clk_put(data->codec_clk);
fail:
	if (ssi_np)
		of_node_put(ssi_np);
	if (codec_np)
		of_node_put(codec_np);

	return ret;
}

static int imx_lm49352_remove(struct platform_device *pdev)
{
	struct imx_lm49352_data *data = platform_get_drvdata(pdev);

	if (data->codec_clk) {
		clk_disable_unprepare(data->codec_clk);
		clk_put(data->codec_clk);
	}
	snd_soc_unregister_card(&data->card);

	return 0;
}

static const struct of_device_id imx_lm49352_dt_ids[] = {
	{ .compatible = "fsl,imx-audio-lm49352", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, imx_lm49352_dt_ids);

static struct platform_driver imx_lm49352_driver = {
	.driver = {
		.name = "imx-lm49352",
		.owner = THIS_MODULE,
		.of_match_table = imx_lm49352_dt_ids,
	},
	.probe = imx_lm49352_probe,
	.remove = imx_lm49352_remove,
};
module_platform_driver(imx_lm49352_driver);

MODULE_AUTHOR("Cerioli Alessandro Chipwork s.n.c.");
MODULE_DESCRIPTION("ALSA SoC LM49352 Machine Layer Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:imx-lm49352");
