/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "gl_os.h"

#include <uapi/linux/sched/types.h>
#include <linux/sched/task.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>

#include "precomp.h"
#include "wmt_exp.h"

#ifdef CONFIG_WLAN_MTK_EMI
#include <soc/mediatek/emi.h>
#define WIFI_EMI_MEM_OFFSET    0x2A0000
#define WIFI_EMI_MEM_SIZE      0x160000
#define DOMAIN_AP	0
#define DOMAIN_CONN	2
#endif

#define DEFAULT_CPU_FREQ (0)
#define MAX_CPU_FREQ (3 * 1024 * 1024) /* in kHZ */
#define MAX_CLUSTER_NUM  3
#define CPU_ALL_CORE (0xff)
#define CPU_BIG_CORE (0xc0)
#define CPU_LITTLE_CORE (CPU_ALL_CORE - CPU_BIG_CORE)

/* Used to get address of saving fw version offset.               */
/* EMI_base + fw ver offset(0x3F400). */
#define FW_VERSION_OFFSET	0x3F400

enum ENUM_CPU_BOOST_STATUS {
	ENUM_CPU_BOOST_STATUS_INIT = 0,
	ENUM_CPU_BOOST_STATUS_START,
	ENUM_CPU_BOOST_STATUS_STOP,
	ENUM_CPU_BOOST_STATUS_NUM
};

uint32_t kalGetCpuBoostThreshold(void)
{
	DBGLOG(SW4, TRACE, "enter kalGetCpuBoostThreshold\n");
	/* 5, stands for 250Mbps */
	return 5;
}

int32_t kalCheckTputLoad(struct ADAPTER *prAdapter,
			uint32_t u4CurrPerfLevel,
			uint32_t u4TarPerfLevel,
			int32_t i4Pending,
			uint32_t u4Used)
{
	uint32_t pendingTh =
		CFG_TX_STOP_NETIF_PER_QUEUE_THRESHOLD *
		prAdapter->rWifiVar.u4PerfMonPendingTh / 100;
	uint32_t usedTh = (HIF_TX_MSDU_TOKEN_NUM / 2) *
		prAdapter->rWifiVar.u4PerfMonUsedTh / 100;

	if (cnmIsMccMode(prAdapter)
		&& prAdapter->rWifiVar.u4PerfMonTpTh[1]
			== PERF_MON_MCC_TP_THRESHOLD
		&& u4TarPerfLevel >= 2) {
		return TRUE;
	}

	return u4TarPerfLevel >= 3 &&
	       u4TarPerfLevel < prAdapter->rWifiVar.u4BoostCpuTh &&
	       i4Pending >= pendingTh &&
	       u4Used >= usedTh ?
	       TRUE : FALSE;
}

void kalSetTaskUtilMinPct(int pid, unsigned int min)
{
	int ret = 0;
	unsigned int blc_1024;
	struct task_struct *p;
	struct sched_attr attr = {};

	if (pid < 0)
		return;

	/* Fill in sched_attr */
	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;

	if (min == 0) {
		attr.sched_util_min = -1;
		attr.sched_util_max = -1;
	} else {
		blc_1024 = (min << 10) / 100U;
		blc_1024 = clamp(blc_1024, 1U, 1024U);
		attr.sched_util_min = (blc_1024 << 10) / 1280;
		attr.sched_util_max = (blc_1024 << 10) / 1280;
	}

	/* get task_struct */
	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (likely(p))
		get_task_struct(p);
	rcu_read_unlock();

	/* sched_setattr */
	if (likely(p)) {
		ret = sched_setattr(p, &attr);
		put_task_struct(p);
	}
}

static LIST_HEAD(wlan_policy_list);
struct wlan_policy {
	struct freq_qos_request	qos_req;
	struct list_head	list;
	int cpu;
};

void kalSetCpuFreq(int32_t freq)
{
	int cpu, ret;
	struct cpufreq_policy *policy;
	struct wlan_policy *wReq;

	if (freq < 0)
		freq = DEFAULT_CPU_FREQ;

	if (list_empty(&wlan_policy_list)) {
		for_each_possible_cpu(cpu) {
			policy = cpufreq_cpu_get(cpu);
			if (!policy)
				continue;

			wReq = kzalloc(sizeof(struct wlan_policy), GFP_KERNEL);
			if (!wReq)
				break;
			wReq->cpu = cpu;

			ret = freq_qos_add_request(&policy->constraints,
				&wReq->qos_req, FREQ_QOS_MIN, DEFAULT_CPU_FREQ);
			if (ret < 0) {
				DBGLOG(INIT, INFO,
					"freq_qos_add_request fail cpu%d ret=%d\n",
					wReq->cpu, ret);
				kfree(wReq);
				break;
			}

			list_add_tail(&wReq->list, &wlan_policy_list);
			cpufreq_cpu_put(policy);
		}
	}

	list_for_each_entry(wReq, &wlan_policy_list, list) {
		ret = freq_qos_update_request(&wReq->qos_req, freq);
		if (ret < 0) {
			DBGLOG(INIT, INFO,
				"freq_qos_update_request fail cpu%d freq=%d ret=%d\n",
				wReq->cpu, freq, ret);
		}
	}
}

void kalSetDramBoost(struct ADAPTER *prAdapter, u_int8_t onoff)
{
	/* TODO */
}

int32_t kalBoostCpu(struct ADAPTER *prAdapter,
			uint32_t u4TarPerfLevel,
			uint32_t u4BoostCpuTh)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t i4Freq = -1;
	static u_int8_t fgRequested = ENUM_CPU_BOOST_STATUS_INIT;

	WIPHY_PRIV(wlanGetWiphy(), prGlueInfo);
	i4Freq = (u4TarPerfLevel >= u4BoostCpuTh) ? MAX_CPU_FREQ : -1;

	if (fgRequested == ENUM_CPU_BOOST_STATUS_INIT) {
		/* initially enable rps working at small cores */
		kalSetRpsMap(prGlueInfo, CPU_LITTLE_CORE);
		fgRequested = ENUM_CPU_BOOST_STATUS_STOP;
	}

	if (u4TarPerfLevel >= u4BoostCpuTh) {
		if (fgRequested == ENUM_CPU_BOOST_STATUS_STOP) {
			pr_info("kalBoostCpu start (%d>=%d)\n",
				u4TarPerfLevel, u4BoostCpuTh);
			fgRequested = ENUM_CPU_BOOST_STATUS_START;

			kalSetTaskUtilMinPct(prGlueInfo->u4TxThreadPid, 100);
			kalSetTaskUtilMinPct(prGlueInfo->u4RxThreadPid, 100);
			kalSetTaskUtilMinPct(prGlueInfo->u4HifThreadPid, 100);
			kalSetRpsMap(prGlueInfo, CPU_BIG_CORE);
			kalSetCpuFreq(i4Freq);
			kalSetDramBoost(prAdapter, TRUE);
		}
	} else {
		if (fgRequested == ENUM_CPU_BOOST_STATUS_START) {
			pr_info("kalBoostCpu stop (%d<%d)\n",
				u4TarPerfLevel, u4BoostCpuTh);
			fgRequested = ENUM_CPU_BOOST_STATUS_STOP;

			kalSetTaskUtilMinPct(prGlueInfo->u4TxThreadPid, 0);
			kalSetTaskUtilMinPct(prGlueInfo->u4RxThreadPid, 0);
			kalSetTaskUtilMinPct(prGlueInfo->u4HifThreadPid, 0);
			kalSetRpsMap(prGlueInfo, CPU_LITTLE_CORE);
			kalSetCpuFreq(i4Freq);
			kalSetDramBoost(prAdapter, FALSE);
		}
	}
	kalTraceInt(fgRequested == ENUM_CPU_BOOST_STATUS_START, "kalBoostCpu");

	return 0;
}

uint32_t kalGetFwVerOffset(void)
{
#define EMI_BASE_6635_OFFSET	0x240000
	const uint32_t adie_chip_id = mtk_wcn_wmt_ic_info_get(WMTCHIN_ADIE);

	DBGLOG(INIT, TRACE, "adie_id: 0x%x\n", adie_chip_id);
	if (adie_chip_id == 0x6635) {
		return EMI_BASE_6635_OFFSET+FW_VERSION_OFFSET;
	}
	return FW_VERSION_OFFSET;
}

#ifdef CONFIG_WLAN_MTK_EMI
void kalSetEmiMpuProtection(phys_addr_t emiPhyBase, bool enable)
{
}

void kalSetDrvEmiMpuProtection(phys_addr_t emiPhyBase, uint32_t offset,
			       uint32_t size)
{
	struct emimpu_region_t region;
	unsigned long long start = emiPhyBase + offset;
	unsigned long long end = emiPhyBase + offset + size - 1;
	int ret;

	DBGLOG(INIT, INFO, "emiPhyBase: 0x%p, offset: %d, size: %d\n",
				emiPhyBase, offset, size);

	ret = mtk_emimpu_init_region(&region, 18);
	if (ret) {
		DBGLOG(INIT, ERROR, "mtk_emimpu_init_region failed, ret: %d\n",
				ret);
		return;
	}
	mtk_emimpu_set_addr(&region, start, end);
	mtk_emimpu_set_apc(&region, DOMAIN_AP, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_set_apc(&region, DOMAIN_CONN, MTK_EMIMPU_NO_PROTECTION);
	mtk_emimpu_lock_region(&region, MTK_EMIMPU_LOCK);
	ret = mtk_emimpu_set_protection(&region);
	if (ret)
		DBGLOG(INIT, ERROR,
			"mtk_emimpu_set_protection failed, ret: %d\n",
			ret);
	mtk_emimpu_free_region(&region);
}
#endif

int32_t kalGetFwFlavorByPlat(uint8_t *flavor)
{
	int32_t ret = 1;
	const uint32_t chip_id = mtk_wcn_wmt_ic_info_get(WMTCHIN_CHIPID);
	const uint32_t adie_chip_id = mtk_wcn_wmt_ic_info_get(WMTCHIN_ADIE);
	uint8_t aucFlavor[CFG_FW_FLAVOR_MAX_LEN] = {0};

	DBGLOG(INIT, INFO, "chip_id: 0x%x, adie_id: 0x%x\n",
		chip_id, adie_chip_id);
	switch (adie_chip_id) {
	case 0x6635:
	case 0x6631:
		/* length need to consider end character */
		kalSnprintf(&aucFlavor, 14, "mt%x_mt%x", chip_id, adie_chip_id);
		kalMemCopy(flavor, aucFlavor, kalStrLen(aucFlavor));
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

