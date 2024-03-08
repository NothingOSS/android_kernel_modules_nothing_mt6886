/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

/*! \file gl_cmd_validate.h
 *   \brief The file contains the declaration of Mediatek private command
 *          and NL80211 string commands, and the declaration of validation
 *          functions.
 */

#ifndef _WSYS_CMD_HANDLER_DRIVER_H
#define _WSYS_CMD_HANDLER_DRIVER_H

/*****************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
******************************************************************************
*/

/*****************************************************************************
*                              C O N S T A N T S
******************************************************************************
*/
#define MIRACAST_MODE_OFF	0
#define MIRACAST_MODE_SOURCE	1
#define MIRACAST_MODE_SINK	2
#define	CMD_BAND_TYPE_AUTO	0
#define	CMD_BAND_TYPE_5G	1
#define	CMD_BAND_TYPE_2G	2
#define	CMD_BAND_TYPE_ALL	3

#if CFG_CHIP_RESET_HANG
#define CMD_SET_RST_HANG_ARG_NUM	2
#endif /* CFG_CHIP_RESET_HANG */

#if CFG_SUPPORT_ADVANCE_CONTROL
#define CMD_SW_DBGCTL_ADVCTL_SET_ID	0xa1260000
#define CMD_SW_DBGCTL_ADVCTL_GET_ID	0xb1260000
#endif /* CFG_SUPPORT_ADVANCE_CONTROL */

/*------------------------------------------------------------------------------
 *  Mediatek ioctl private commnad
 *------------------------------------------------------------------------------
 */
#define CMD_LINKSPEED			"LINKSPEED"
#define CMD_SETSUSPENDMODE		"SETSUSPENDMODE"
#define CMD_SETBAND			"SETBAND"
#define CMD_AP_START			"AP_START"
#define CMD_GET_BSS_STATS		"GET_BSS_STATISTICS"
#define CMD_GET_STA_STATS		"GET_STA_STATISTICS"
#define CMD_GET_WTBL_INFO		"GET_WTBL"
#define CMD_GET_MIB_INFO		"GET_MIB"
#define CMD_GET_STA_INFO		"GET_STA"
#define CMD_SET_FW_LOG			"SET_FWLOG"
#define CMD_GET_QUE_INFO		"GET_QUE"
#define CMD_GET_MEM_INFO		"GET_MEM"
#define CMD_GET_HIF_INFO		"GET_HIF"
#define CMD_GET_TP_INFO			"GET_TP"
#define CMD_COUNTRY			"COUNTRY"
#define CMD_CSA				"CSA"
#define CMD_ECSA			"ECSA"
#define CMD_CSA_EX			"CSA_EX"
#define CMD_CSA_EX_EVENT		"EVENT_CSA_EX"
#define CMD_GET_COUNTRY			"GET_COUNTRY"
#define CMD_GET_CHANNELS		"GET_CHANNELS"
#define CMD_P2P_SET_NOA			"P2P_SET_NOA"
#define CMD_P2P_GET_NOA			"P2P_GET_NOA"
#define CMD_P2P_SET_PS			"P2P_SET_PS"
#define CMD_MIRACAST			"MIRACAST"
#define CMD_SETCASTMODE			"SET_CAST_MODE"
#define CMD_COEX_CONTROL		"COEX_CONTROL"
#define CMD_GET_CH_RANK_LIST		"GET_CH_RANK_LIST"
#define CMD_GET_CH_DIRTINESS		"GET_CH_DIRTINESS"
#define CMD_EFUSE			"EFUSE"
#define CMD_CCCR			"CCCR"
#define CMD_SET_MCR			"SET_MCR"
#define CMD_GET_MCR			"GET_MCR"
#define CMD_SUPPORT_NVRAM		"SUPPORT_NVRAM"
#define CMD_SET_DRV_MCR			"SET_DRV_MCR"
#define CMD_GET_DRV_MCR			"GET_DRV_MCR"
#define CMD_GET_EMI_MCR			"GET_EMI_MCR"
#define CMD_SET_UHW_MCR			"SET_UHW_MCR"
#define CMD_GET_UHW_MCR			"GET_UHW_MCR"
#define CMD_SET_SW_CTRL			"SET_SW_CTRL"
#define CMD_GET_SW_CTRL			"GET_SW_CTRL"
#define CMD_SET_CFG			"SET_CFG"
#define CMD_GET_CFG			"GET_CFG"
#define CMD_SET_EM_CFG			"SET_EM_CFG"
#define CMD_GET_EM_CFG			"GET_EM_CFG"
#define CMD_SET_CHIP			"SET_CHIP"
#define CMD_GET_CHIP			"GET_CHIP"
#define CMD_GET_WIFI_TYPE		"GET_WIFI_TYPE"
#define CMD_SET_PWR_CTRL		"SET_PWR_CTRL"
#define CMD_SET_FIXED_RATE		"FixedRate"
#define CMD_SET_AUTO_RATE		"AutoRate"
#define CMD_SET_PP_CAP_CTRL		"PpCapCtrl"
#define CMD_SET_PP_ALG_CTRL		"PpAlgCtrl"
#define CMD_GET_VERSION			"VER"
#define CMD_SET_TEST_MODE		"SET_TEST_MODE"
#define CMD_SET_TEST_CMD		"SET_TEST_CMD"
#define CMD_GET_TEST_RESULT		"GET_TEST_RESULT"
#define CMD_GET_STA_STAT		"STAT"
#define CMD_GET_STA_STAT2		"STAT2"
#define CMD_GET_STA_RX_STAT		"RX_STAT"
#define CMD_SET_POLICY_ACL		"SET_ACL_POLICY"
#define CMD_ADD_ACL_ENTRY		"ADD_ACL_ENTRY"
#define CMD_DEL_ACL_ENTRY		"DEL_ACL_ENTRY"
#define CMD_SHOW_ACL_ENTRY		"SHOW_ACL_ENTRY"
#define CMD_CLEAR_ACL_ENTRY		"CLEAR_ACL_ENTRY"
#define CMD_SET_RA_DBG			"RADEBUG"
#define CMD_SET_FIXED_FALLBACK		"FIXEDRATEFALLBACK"
#define CMD_GET_STA_IDX			"GET_STA_IDX"
#define CMD_GET_TX_POWER_INFO		"TxPowerInfo"
#define CMD_TX_POWER_MANUAL_SET		"TxPwrManualSet"
#define CMD_GET_HAPD_CHANNEL		"HAPD_GET_CHANNEL"
#define CMD_SET_HAPD_AXMODE		"HAPD_SET_AX_MODE"
#define CMD_SET_MDVT			"SET_MDVT"
#define CMD_SET_MLO_AGC_TX		"MLOAGCTX"
#define CMD_GET_MLD_REC			"MLDREC"
#define CMD_THERMAL_PROTECT_ENABLE	"thermal_protect_enable"
#define CMD_THERMAL_PROTECT_DISABLE	"thermal_protect_disable"
#define CMD_THERMAL_PROTECT_DUTY_CFG	"thermal_protect_duty_cfg"
#define CMD_THERMAL_PROTECT_INFO	"thermal_protect_info"
#define CMD_THERMAL_PROTECT_DUTY_INFO	"thermal_protect_duty_info"
#define CMD_THERMAL_PROTECT_STATE_ACT	"thermal_protect_state_act"
#define CMD_SET_USE_CASE		"SET_USE_CASE"
#define CMD_SET_BOOSTCPU		"BOOSTCPU"
/* neptune doens't support "show" entry, use "driver" to handle
 * MU GET request, and MURX_PKTCNT comes from RX_STATS,
 * so this command will reuse RX_STAT's flow
 */
#define CMD_GET_MU_RX_PKTCNT		"hqa_get_murx_pktcnt"
#define CMD_RUN_HQA			"hqa"
#define CMD_CALIBRATION			"cal"
#define CMD_SET_ADV_PWS			"SET_ADV_PWS"
#define CMD_SET_MDTIM			"SET_MDTIM"
#define CMD_SET_DBDC			"SET_DBDC"
#define CMD_SET_STA1NSS			"SET_STA1NSS"
#define CMD_SET_AMPDU_TX		"SET_AMPDU_TX"
#define CMD_SET_AMPDU_RX		"SET_AMPDU_RX"
#define CMD_SET_BF			"SET_BF"
#define CMD_SET_NSS			"SET_NSS"
#define CMD_SET_AMSDU_TX		"SET_AMSDU_TX"
#define CMD_SET_AMSDU_RX		"SET_AMSDU_RX"
#define CMD_SET_QOS			"SET_QOS"
#define CMD_GET_CNM			"GET_CNM"
#define CMD_GET_CAPAB_RSDB		"GET_CAPAB_RSDB"
#define CMD_GET_TDLS_AVAILABLE		"GET_TDLS_AVAILABLE"
#define CMD_GET_TDLS_WIDER_BW		"GET_TDLS_WIDER_BW"
#define CMD_GET_TDLS_MAX_SESSION	"GET_TDLS_MAX_SESSION"
#define CMD_GET_TDLS_NUM_OF_SESSION	"GET_TDLS_NUM_OF_SESSION"
#define CMD_SET_TDLS_ENABLED		"SET_TDLS_ENABLED"
#define CMD_SET_SW_AMSDU_NUM		"SET_SW_AMSDU_NUM"
#define CMD_SET_SW_AMSDU_SIZE		"SET_SW_AMSDU_SIZE"
#define CMD_SET_DRV_SER			"SET_DRV_SER"
#define CMD_SHOW_TXD_INFO		"SHOW_TXD_INFO"
#define CMD_GET_MCU_INFO		"GET_MCU_INFO"
#define CMD_GET_BAINFO			"GET_BAINFO"
#define CMD_GET_SER			"GET_SER"
#define CMD_GET_EMI			"GET_EMI"
#define CMD_QUERY_THERMAL_TEMP		"QUERY_THERMAL_TEMP"
#if CFG_SUPPORT_NAN
#define CMD_NAN_START			"NAN_START"
#define CMD_NAN_GET_MASTER_IND		"NAN_GET_MASTER_IND"
#define CMD_NAN_GET_RANGE		"NAN_GET_RANGE"
#define CMD_FAW_RESET			"FAW_RESET"
#define CMD_FAW_CONFIG			"FAW_CONFIG"
#define CMD_FAW_APPLY			"FAW_APPLY"
#endif /* CFG_SUPPORT_NAN */
#if CFG_SUPPORT_QA_TOOL
#define CMD_GET_RX_STATISTICS		"GET_RX_STATISTICS"
#endif /* CFG_SUPPORT_QA_TOOL */
#if (CFG_SUPPORT_DFS_MASTER == 1)
#define CMD_SET_DFS_CHN_AVAILABLE	"SET_DFS_CHN_AVAILABLE"
#define CMD_SHOW_DFS_STATE		"SHOW_DFS_STATE"
#define CMD_SHOW_DFS_HELP		"SHOW_DFS_HELP"
#define CMD_SHOW_DFS_CAC_TIME		"SHOW_DFS_CAC_TIME"
#define CMD_SET_DFS_RDDREPORT		"RDDReport"
#define CMD_SET_DFS_RADARMODE		"RadarDetectMode"
#define CMD_SET_DFS_RADAREVENT		"RadarEvent"
#define CMD_SET_DFS_RDDOPCHNG		"RDDOpChng"
#endif /* CFG_SUPPORT_DFS_MASTER */
#if CFG_SUPPORT_IDC_CH_SWITCH
#define CMD_SET_IDC_BMP			"SetIdcBmp"
#define CMD_SET_IDC_RIL			"SET_RIL_BRIDGE"
#endif /* CFG_SUPPORT_IDC_CH_SWITCH */
#if CFG_CHIP_RESET_HANG
#define CMD_SET_RST_HANG		"RST_HANG_SET"
#endif /* CFG_CHIP_RESET_HANG */
#if CFG_SUPPORT_TSF_SYNC
#define CMD_GET_TSF_VALUE		"GET_TSF"
#endif /* CFG_SUPPORT_TSF_SYNC */
#if (CFG_SUPPORT_TWT == 1)
#define CMD_SET_TWT_PARAMS		"SET_TWT_PARAMS"
#endif /* CFG_SUPPORT_TWT */
#if (CFG_WLAN_ASSISTANT_NVRAM == 1)
#define CMD_SET_NVRAM			"SET_NVRAM"
#define CMD_GET_NVRAM			"GET_NVRAM"
#endif /* CFG_WLAN_ASSISTANT_NVRAM */
#if (CFG_SUPPORT_POWER_THROTTLING == 1)
#define CMD_SET_PWR_LEVEL		"SET_PWR_LEVEL"
#define CMD_SET_PWR_TEMP		"SET_PWR_TEMP"
#endif /* CFG_SUPPORT_POWER_THROTTLING */
#if ((CFG_SUPPORT_ICS == 1) || (CFG_SUPPORT_PHY_ICS == 1))
#define CMD_SET_SNIFFER			"SNIFFER"
#endif /* CFG_SUPPORT_ICS */
#ifdef CFG_SUPPORT_SNIFFER_RADIOTAP
#define CMD_SET_MONITOR			"MONITOR"
#endif
#if CFG_SUPPORT_CSI
#define CMD_SET_CSI			"SET_CSI"
#endif /* CFG_SUPPORT_CSI */
#if CFG_WOW_SUPPORT
#define CMD_WOW_START			"WOW_START"
#define CMD_SET_WOW_ENABLE		"SET_WOW_ENABLE"
#define CMD_SET_WOW_PAR			"SET_WOW_PAR"
#define CMD_SET_WOW_UDP			"SET_WOW_UDP"
#define CMD_SET_WOW_TCP			"SET_WOW_TCP"
#define CMD_GET_WOW_PORT		"GET_WOW_PORT"
#define CMD_GET_WOW_REASON		"GET_WOW_REASON"
#if CFG_SUPPORT_MDNS_OFFLOAD
#define CMD_SHOW_MDNS_RECORD		"SHOW_MDNS_RECORD"
#define CMD_ENABLE_MDNS			"ENABLE_MDNS_OFFLOAD"
#define CMD_DISABLE_MDNS		"DISABLE_MDNS_OFFLOAD"
#define CMD_MDNS_SET_WAKE_FLAG		"MDNS_SET_WAKE_FLAG"
#if TEST_CODE_FOR_MDNS
/* test code for mdns offload */
#define CMD_SEND_MDNS_RECORD		"SEND_MDNS_RECORD"
#define CMD_ADD_MDNS_RECORD		"ADD_MDNS_RECORD"
#define TEST_ADD_MDNS_RECORD		"TEST_ADD_MDNS_RECORD"
#endif /* TEST_CODE_FOR_MDNS */
#endif /* CFG_SUPPORT_MDNS_OFFLOAD */
#endif /* CFG_WOW_SUPPORT */
#if (CFG_SUPPORT_802_11AX == 1)
#define CMD_SET_BA_SIZE			"SET_BA_SIZE"
#define CMD_SET_RX_BA_SIZE		"SET_RX_BA_SIZE"
#define CMD_SET_TX_BA_SIZE		"SET_TX_BA_SIZE"
#define CMD_SET_TP_TEST_MODE		"SET_TP_TEST_MODE"
#define CMD_SET_MUEDCA_OVERRIDE		"MUEDCA_OVERRIDE"
#define CMD_SET_TX_MCSMAP		"SET_MCS_MAP"
#define CMD_SET_TX_EHTMCSMAP		"SET_EHT_MCS_MAP"
#define CMD_SET_TX_PPDU			"TX_PPDU"
#define CMD_SET_LDPC			"SET_LDPC"
#define CMD_FORCE_AMSDU_TX		"FORCE_AMSDU_TX"
#define CMD_SET_OM_CH_BW		"SET_OM_CHBW"
#define CMD_SET_OM_RX_NSS		"SET_OM_RXNSS"
#define CMD_SET_OM_TX_NSS		"SET_OM_TXNSTS"
#define CMD_SET_OM_MU_DISABLE		"SET_OM_MU_DISABLE"
#define CMD_SET_OM_MU_DATA_DISABLE	"SET_OM_MU_DATA_DISABLE"
#define CMD_SET_TX_OM_PACKET		"TX_OM_PACKET"
#define CMD_SET_EHT_OM_MODE		"SET_EHT_OM_MODE"
#define CMD_SET_EHT_OM_RX_NSS_EXT	"SET_EHT_OM_RXNSS_EXT"
#define CMD_SET_EHT_OM_CH_BW_EXT	"SET_EHT_OM_CHBW_EXT"
#define CMD_SET_EHT_OM_TX_NSTS_EXT	"SET_EHT_OM_TXNSTS_EXT"
#define CMD_SET_TX_CCK_1M_PWR		"TX_CCK_1M_PWR"
#define CMD_SET_PAD_DUR			"SET_PAD_DUR"
#define CMD_SET_SR_ENABLE		"SET_SR_ENABLE"
#define CMD_GET_SR_CAP			"GET_SR_CAP"
#define CMD_GET_SR_IND			"GET_SR_IND"
#define CMD_SET_PP_RX			"SET_PP_RX"
#define CMD_SET_RxCtrlToMutiBss		"SET_RX_CTRL_TO_MUTI_BSS"
#endif /* CFG_SUPPORT_802_11AX == 1 */
#ifdef UT_TEST_MODE
#define CMD_RUN_UT			"UT"
#endif /* UT_TEST_MODE */
#if CFG_SUPPORT_ADVANCE_CONTROL
#define CMD_SET_NOISE			"SET_NOISE"
#define CMD_GET_NOISE			"GET_NOISE"
#define CMD_SET_POP			"SET_POP"
#if (CFG_SUPPORT_DYNAMIC_EDCCA == 1)
#define CMD_SET_ED			"SET_ED"
#define CMD_GET_ED			"GET_ED"
#endif /* CFG_SUPPORT_DYNAMIC_EDCCA */
#define CMD_SET_PD			"SET_PD"
#define CMD_SET_MAX_RFGAIN		"SET_MAX_RFGAIN"
#endif /* CFG_SUPPORT_ADVANCE_CONTROL */
#if CFG_SUPPORT_WIFI_SYSDVT
#define CMD_WIFI_SYSDVT			"DVT"
#define CMD_SET_TXS_TEST		"TXS_TEST"
#define CMD_SET_TXS_TEST_RESULT		"TXS_RESULT"
#define CMD_SET_RXV_TEST		"RXV_TEST"
#define CMD_SET_RXV_TEST_RESULT		"RXV_RESULT"
#if CFG_TCP_IP_CHKSUM_OFFLOAD
#define CMD_SET_CSO_TEST		"CSO_TEST"
#endif /* CFG_TCP_IP_CHKSUM_OFFLOAD */
#define CMD_SET_TX_TEST			"TX_TEST"
#define CMD_SET_TX_AC_TEST		"TX_AC_TEST"
#define CMD_SET_SKIP_CH_CHECK		"SKIP_CH_CHECK"
#if (CFG_SUPPORT_DMASHDL_SYSDVT)
#define CMD_SET_DMASHDL_DUMP		"DMASHDL_DUMP_MEM"
#define CMD_SET_DMASHDL_DVT_ITEM	"DMASHDL_DVT_ITEM"
#endif /* CFG_SUPPORT_DMASHDL_SYSDVT */
#endif /* CFG_SUPPORT_WIFI_SYSDVT */
#if CFG_AP_80211KVR_INTERFACE
#define CMD_WHITELIST_STA		"White_sta"
#define CMD_BLACKLIST_STA		"Black_sta"
#define CMD_BSS_STATUS_REPORT		"BssStatus"
#define CMD_BSS_REPORT_INFO		"BssReportInfo"
#define CMD_STA_REPORT_INFO		"StaReportInfo"
#define CMD_STA_MEASUREMENT_ENABLE	"mnt_en"
#define CMD_STA_MEASUREMENT_INFO	"mnt_info"
#endif /* CFG_AP_80211KVR_INTERFACE */
#if (CFG_SUPPORT_CONNAC2X == 1)
#define CMD_GET_FWTBL_UMAC		"GET_UMAC_FWTBL"
#endif /* CFG_SUPPORT_CONNAC2X == 1 */
#if (CFG_SUPPORT_CONNAC2X == 1 || CFG_SUPPORT_CONNAC3X == 1)
#define CMD_GET_UWTBL			"GET_UWTBL"
#endif /* CFG_SUPPORT_CONNAC2X == 1 || CFG_SUPPORT_CONNAC3X == 1 */
#if (CFG_SUPPORT_802_11BE_MLO == 1)
#define CMD_DBG_SHOW_MLD		"show-mld"
#define CMD_DBG_SHOW_MLD_BSS		"show-mld-bss"
#define CMD_DBG_SHOW_MLD_STA		"show-mld-sta"
#endif /* CFG_SUPPORT_802_11BE_MLO */
#if CFG_WMT_RESET_API_SUPPORT
#define CMD_SET_WHOLE_CHIP_RESET	"SET_WHOLE_CHIP_RESET"
#define CMD_SET_WFSYS_RESET		"SET_WFSYS_RESET"
#endif /* CFG_WMT_RESET_API_SUPPORT */
#if CFG_MTK_WIFI_SW_WFDMA
#define CMD_SET_SW_WFDMA		"SET_SW_WFDMA"
#endif /* CFG_MTK_WIFI_SW_WFDMA */
#if CFG_SUPPORT_802_11K
#define CMD_NEIGHBOR_REQ		"neighbor-request"
#endif /* CFG_SUPPORT_802_11K */
#if CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT
#define CMD_BTM_QUERY			"bss-transition-query"
#endif /* CFG_SUPPORT_802_11V_BSS_TRANSITION_MGT */
#if (CFG_SUPPORT_DEBUG_SOP == 1)
#define CMD_GET_SLEEP_INFO		"GET_SLEEP_INFO"
#endif /* CFG_SUPPORT_DEBUG_SOP */
#if (CFG_SUPPORT_802_11BE_MLO == 1)
#define CMD_PRESET_LINKID		"PRESET_LINKID"
#define CMD_SET_ML_PROBEREQ		"SET_ML_PROBEREQ"
#define CMD_GET_ML_CAPA			"GET_ML_CAPA"
#define CMD_GET_ML_PREFER_FREQ_LIST	"GET_ML_PREFER_FREQ_LIST"
#define CMD_GET_ML_2ND_FREQ		"GET_ML_2ND_FREQ"
#endif /* CFG_SUPPORT_802_11BE_MLO */
#if (CFG_WIFI_GET_DPD_CACHE == 1)
#define CMD_GET_DPD_CACHE		"GET_DPD_CACHE"
#endif /* CFG_WIFI_GET_DPD_CACHE */
#if (CFG_WIFI_GET_MCS_INFO == 1)
#define CMD_GET_MCS_INFO		"GET_MCS_INFO"
#endif /* CFG_WIFI_GET_MCS_INFO */
#if CFG_AP_80211K_SUPPORT
#define CMD_STA_BEACON_REQUEST		"BeaconRequest"
#endif /* CFG_AP_80211K_SUPPORT */
#if CFG_AP_80211V_SUPPORT
#define CMD_STA_BTM_REQUEST		"BTMRequest"
#endif /* CFG_AP_80211V_SUPPORT */
#if CFG_AP_80211KVR_INTERFACE
#define CMD_WHITELIST_STA		"White_sta"
#define CMD_BLACKLIST_STA		"Black_sta"
#define CMD_BSS_STATUS_REPORT		"BssStatus"
#define CMD_BSS_REPORT_INFO		"BssReportInfo"
#define CMD_STA_REPORT_INFO		"StaReportInfo"
#define CMD_STA_MEASUREMENT_ENABLE	"mnt_en"
#define CMD_STA_MEASUREMENT_INFO	"mnt_info"
#endif /* CFG_AP_80211KVR_INTERFACE */
#if CFG_SUPPORT_PCIE_GEN_SWITCH
#define CMD_SET_PCIE_SPEED		"SET_PCIE_SPEED"
#endif

/*------------------------------------------------------------------------------
 *  nl80211 vendor string command
 *------------------------------------------------------------------------------
 */
#define CMD_TDLS_PS				"tdls-ps"
#define CMD_NEIGHBOR_REQUEST			"NEIGHBOR-REQUEST"
#define CMD_BSS_TRAN_QUERY			"BSS-TRANSITION-QUERY"
#define CMD_OSHARE				"OSHAREMOD"
#define CMD_EXAMPLE				"CMD_EXAMPLE"
#define CMD_REASSOC				"REASSOC"
#define CMD_ADD_ROAM_SCN_CHNL			"ADDROAMSCANCHANNELS_LEGACY"
#define CMD_SET_AX_BLACKLIST                    "SET_AX_BLACKLIST"
#define CMD_RTT_GET_CAP				"RttGetCap"
#if CFG_SUPPORT_NCHO
/* NCHO related command definition. Setting by supplicant */
#define CMD_NCHO_ROAM_TRIGGER_GET		"GETROAMTRIGGER"
#define CMD_NCHO_ROAM_TRIGGER_SET		"SETROAMTRIGGER"
#define CMD_NCHO_ROAM_DELTA_GET			"GETROAMDELTA"
#define CMD_NCHO_ROAM_DELTA_SET			"SETROAMDELTA"
#define CMD_NCHO_ROAM_SCAN_PERIOD_GET		"GETROAMSCANPERIOD"
#define CMD_NCHO_ROAM_SCAN_PERIOD_SET		"SETROAMSCANPERIOD"
#define CMD_NCHO_ROAM_SCAN_CHANNELS_GET		"GETROAMSCANCHANNELS"
#define CMD_NCHO_ROAM_SCAN_CHANNELS_SET		"SETROAMSCANCHANNELS"
#define CMD_NCHO_ROAM_SCAN_CHANNELS_ADD		"ADDROAMSCANCHANNELS"
#define CMD_NCHO_ROAM_SCAN_CONTROL_GET		"GETROAMSCANCONTROL"
#define CMD_NCHO_ROAM_SCAN_CONTROL_SET		"SETROAMSCANCONTROL"
#define CMD_NCHO_MODE_SET			"SETNCHOMODE"
#define CMD_NCHO_MODE_GET			"GETNCHOMODE"
#endif
#define CMD_REPORT_VENDOR_SPECIFIED		"EnVendorSpecifiedRpt"

/*****************************************************************************
*                             D A T A   T Y P E S
******************************************************************************
*/
typedef int(*PRIV_CMD_FUNCTION) (
	struct net_device *prNetDev,
	char *pcCommand,
	int i4TotalLen);

typedef int(*STR_CMD_FUNCTION) (
	struct wiphy *wiphy,
	struct wireless_dev *wdev,
	char *pcCommand,
	int i4TotalLen);

enum ARG_NUM_POLICY {
	VERIFY_MIN_ARG_NUM	= 0,
	VERIFY_EXACT_ARG_NUM	= 1,
};

struct CMD_VALIDATE_POLICY {
	uint8_t  type;
	uint16_t len;
	uint32_t min;
	uint32_t max;
};

struct PRIV_CMD_HANDLER {
	uint8_t *pcCmdStr;
	PRIV_CMD_FUNCTION pfHandler;
	enum ARG_NUM_POLICY argPolicy;
	uint8_t ucArgNum; /* include CMD */
	struct CMD_VALIDATE_POLICY *policy;
	uint32_t u4PolicySize;
};

struct STR_CMD_HANDLER {
	uint8_t *pcCmdStr;
	STR_CMD_FUNCTION pfHandler;
	enum ARG_NUM_POLICY argPolicy;
	uint8_t ucArgNum; /* include CMD */
	struct CMD_VALIDATE_POLICY *policy;
	uint32_t u4PolicySize;
};

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
PRIV_CMD_FUNCTION get_priv_cmd_handler(uint8_t *cmd, int32_t len);
STR_CMD_FUNCTION get_str_cmd_handler(uint8_t *cmd, int32_t len);

#endif /* _WSYS_CMD_HANDLER_DRIVER_H */
