// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include "../../../../conf/include/conninfra_conf.h"
#include "../../../../include/conninfra.h"
#include "../include/clock_mng.h"
#include "../include/consys_hw.h"
#include "../include/consys_reg_util.h"
#include "../include/plat_library.h"
#include "include/mt6985.h"
#include "include/mt6985_consys_reg_offset.h"
#include "include/mt6985_pos_gen.h"
#include "include/mt6985_soc.h"

#include "dvfsrc-common.h"

static int connsys_pm_dev_drv_probe(struct platform_device *pdev);

static struct platform_device *connsys_pm_device = NULL;
const struct of_device_id consys_pm_mt6985_of_ids[] = {
	{.compatible = "mediatek,mt6985-connsys-pm",},
	{}
};

static struct platform_driver connsys_pm_dev_drv_mt6985 = {
	.probe = connsys_pm_dev_drv_probe,
	.driver = {
		.name = "mt6985-consys-pm",
#ifdef CONFIG_OF
		.of_match_table = consys_pm_mt6985_of_ids,
#endif
		.probe_type = PROBE_FORCE_SYNCHRONOUS,
	},
};

int consys_co_clock_type_mt6985(void)
{
	const struct conninfra_conf *conf;
	/* In 6985, clock for connsys is always 26M (RFCK2B).
	 * We don't read clock ic register to indetify clock rate.
	 */
	int clock_type = CONNSYS_CLOCK_SCHEMATIC_26M_COTMS;
	unsigned char tcxo_gpio = 0;

	/* Default solution */
	conf = conninfra_conf_get_cfg();
	if (NULL == conf)
		pr_notice("[%s] Get conf fail", __func__);
	else
		tcxo_gpio = conf->tcxo_gpio;

	if (tcxo_gpio != 0 || conn_hw_env.tcxo_support)
		clock_type = CONNSYS_CLOCK_SCHEMATIC_26M_EXTCXO;
	pr_info("[%s] conf->tcxo_gpio=%d conn_hw_env.tcxo_support=%d, %s",
		__func__, tcxo_gpio, conn_hw_env.tcxo_support,
		clock_mng_get_schematic_name(clock_type));

	return clock_type;
}

static int connsys_pm_dev_drv_probe(struct platform_device *pdev)
{
	if (pdev == NULL) {
		pr_notice("[%s] invalid input", __func__);
		return -1;
	}

	pr_info("[%s] ---\n", __func__);
	connsys_pm_device = pdev;

	pm_runtime_enable(&pdev->dev);
	dev_pm_syscore_device(&pdev->dev, true);

	return 0;
}

int consys_clk_get_from_dts_mt6985(struct platform_device *pdev)
{
	int iret = 0;

	pr_info("[%s] register connsys_pm_dev_drv_mt6985\n", __func__);

	pr_info("[%s] deny OPP0\n", __func__);
	mtk_dvfsrc_dynamic_opp0(VCOREOPP_GPS, true);

	iret = platform_driver_register(&connsys_pm_dev_drv_mt6985);
	if (iret)
		pr_notice("[%s] register connsys_pm_dev_drv_mt6985 fail(%d)\n", __func__, iret);
	else
		pr_info("[%s] register connsys_pm_dev_drv_mt6985 succesfully\n", __func__);

	mtk_dvfsrc_dynamic_opp0(VCOREOPP_GPS, false);
	pr_info("[%s] allow OPP0\n", __func__);

	return 0;
}

int consys_clock_buffer_ctrl_mt6985(unsigned int enable)
{
	return 0;
}

unsigned int consys_soc_chipid_get_mt6985(void)
{
	return PLATFORM_SOC_CHIP_MT6985;
}

int consys_platform_spm_conn_ctrl_mt6985(unsigned int enable)
{
	int ret = 0;
	struct platform_device *pdev = connsys_pm_device;

	if (!pdev) {
		pr_info("get_consys_device fail.\n");
		return -1;
	}

	if (enable) {
		ret = pm_runtime_get_sync(&(pdev->dev));
		if (ret)
			pr_info("pm_runtime_get_sync() fail(%d)\n", ret);
		else
			pr_info("pm_runtime_get_sync() CONSYS ok\n");

		ret = device_init_wakeup(&(pdev->dev), true);
		if (ret)
			pr_info("device_init_wakeup(true) fail.\n");
		else
			pr_info("device_init_wakeup(true) CONSYS ok\n");
	} else {
		ret = device_init_wakeup(&(pdev->dev), false);
		if (ret)
			pr_info("device_init_wakeup(false) fail.\n");
		else
			pr_info("device_init_wakeup(false) CONSYS ok\n");

		ret = pm_runtime_put_sync(&(pdev->dev));
		if (ret)
			pr_info("pm_runtime_put_sync() fail.\n");
		else
			pr_info("pm_runtime_put_sync() CONSYS ok\n");
	}

	return ret;
}

void consys_set_if_pinmux_mt6985(unsigned int enable)
{
#ifndef CFG_CONNINFRA_ON_CTP
	struct pinctrl_state *tcxo_pinctrl_set;
	struct pinctrl_state *tcxo_pinctrl_clr;
	int ret = -1;
#endif
	int clock_type = consys_co_clock_type_mt6985();

	if (enable) {
		/* if(TCXO mode)
		 * 	Set GPIO135 pinmux for TCXO mode (Aux3)(CONN_TCXOENA_REQ)
		 */

		if (clock_type == CONNSYS_CLOCK_SCHEMATIC_26M_EXTCXO ||
			clock_type == CONNSYS_CLOCK_SCHEMATIC_52M_EXTCXO) {
	#if defined(CFG_CONNINFRA_ON_CTP)
			consys_set_gpio_tcxo_mode_mt6985_gen(1, 1);
	#else
			if (!IS_ERR(g_conninfra_pinctrl_ptr)) {
				tcxo_pinctrl_set = pinctrl_lookup_state(g_conninfra_pinctrl_ptr, "conninfra_tcxo_set");
				if (!IS_ERR(tcxo_pinctrl_set)) {
					ret = pinctrl_select_state(g_conninfra_pinctrl_ptr, tcxo_pinctrl_set);
					if (ret)
						pr_notice("[%s] set TCXO mode error: %d\n", __func__, ret);
				}
			}
	#endif /* defined(CFG_CONNINFRA_ON_CTP) */
		}
	} else {
		if (clock_type == CONNSYS_CLOCK_SCHEMATIC_26M_EXTCXO ||
			clock_type == CONNSYS_CLOCK_SCHEMATIC_52M_EXTCXO) {
	#if defined(CFG_CONNINFRA_ON_CTP)
			consys_set_gpio_tcxo_mode_mt6985_gen(1, 0);
	#else
			if (!IS_ERR(g_conninfra_pinctrl_ptr)) {
				tcxo_pinctrl_clr = pinctrl_lookup_state(g_conninfra_pinctrl_ptr, "conninfra_tcxo_clr");
				if (!IS_ERR(tcxo_pinctrl_clr)) {
					ret = pinctrl_select_state(g_conninfra_pinctrl_ptr, tcxo_pinctrl_clr);
					if (ret)
						pr_notice("[%s] clear TCXO mode error: %d\n", __func__, ret);
				}
			}
	#endif /* defined(CFG_CONNINFRA_ON_CTP) */
		}
	}
}
