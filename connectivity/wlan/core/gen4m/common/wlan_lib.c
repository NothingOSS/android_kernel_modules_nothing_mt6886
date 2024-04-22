/*******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/
/*! \file   wlan_lib.c
 *    \brief  Internal driver stack will export the required procedures here for
 *            GLUE Layer.
 *
 *    This file contains all routines which are exported from MediaTek 802.11
 *    Wireless LAN driver stack to GLUE Layer.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"
#include "mgmt/ais_fsm.h"
#include "mddp.h"
#include "conn_dbg.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* 6.1.1.2 Interpretation of priority parameter in MAC service primitives */
/* Static convert the Priority Parameter/TID(User Priority/TS Identifier) to
 * Traffic Class
 */
const uint8_t aucPriorityParam2TC[] = {
	TC1_INDEX,
	TC0_INDEX,
	TC0_INDEX,
	TC1_INDEX,
	TC2_INDEX,
	TC2_INDEX,
	TC3_INDEX,
	TC3_INDEX
};

#if (CFG_WOW_SUPPORT == 1)
/* HIF suspend should wait for cfg80211 suspend done.
 * by experience, 5ms is enough, and worst case ~= 250ms.
 * if > 250 ms --> treat as no cfg80211 suspend
 */
#define HIF_SUSPEND_MAX_WAIT_TIME 50 /* unit: 5ms */
#endif

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
struct CODE_MAPPING {
	uint32_t u4RegisterValue;
	int32_t u4TxpowerOffset;
};

struct NVRAM_TAG_FRAGMENT_GROUP {
	uint8_t u1StartTagID;
	uint8_t u1EndTagID;
};

struct NVRAM_FRAGMENT_RANGE {
	uint32_t startOfs;
	uint32_t endOfs;
};


/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
u_int8_t fgIsMcuOff = FALSE;
u_int8_t fgIsBusAccessFailed = FALSE;
#if IS_ENABLED(CFG_MTK_WIFI_CONNV3_SUPPORT)
u_int8_t fgTriggerDebugSop = FALSE;
#endif

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
/* data rate mapping table for CCK */
struct cckDataRateMappingTable_t {
	uint32_t rate[4];
} g_rCckDataRateMappingTable = {
	{10, 20, 55, 110}
};
/* data rate mapping table for OFDM */
struct ofdmDataRateMappingTable_t {
	uint32_t rate[8];
} g_rOfdmDataRateMappingTable = {
	{60, 90, 120, 180, 240, 360, 480, 540}
};
/* data rate mapping table for 802.11n and 802.11ac */
struct dataRateMappingTable_t {
	struct nsts_t {
		struct bw_t {
			struct sgi_t {
				uint32_t rate[12];
			} sgi[2];
		} bw[4];
	} nsts[3];
} g_rDataRateMappingTable = {
{ { { { { /* HT/VHT NSTS=1, 20MHz */
	{ /* no SGI */
	{65, 130, 195, 260, 390, 520, 585, 650, 780, 867, 975, 1083}
	},
	{ /* SGI */
	{72, 144, 217, 289, 433, 578, 650, 722, 867, 963, 1083, 1204}
	}
} },
{
	{ /* HT/VHT NSTS=1, 40MHz */
	{ /* no SGI */
	{135, 270, 405, 540, 810, 1080, 1215, 1350, 1620, 1800, 2025, 2250}
	},
	{ /* SGI */
	{150, 300, 450, 600, 900, 1200, 1350, 1500, 1800, 2000, 2250, 2500}
	}
} },
{
	{ /* HT/VHT NSTS=1, 80MHz */
	{ /* no SGI */
	{293, 585, 878, 1170, 1755, 2340, 2633, 2925, 3510, 3900, 4388, 4875}
	},
	{ /* SGI */
	{325, 650, 975, 1300, 1950, 2600, 2925, 3250, 3900, 4333, 4875, 5417}
	}
} },
{
	{ /* HT/VHT NSTS=1, 160MHz */
	{ /* no SGI */
	{585, 1170, 1755, 2340, 3510, 4680, 5265, 5850, 7020, 7800, 8775, 9750}
	},
	{ /* SGI */
	{650, 1300, 1950, 2600, 3900, 5200, 5850, 6500, 7800, 8667, 9750, 10833}
	}
} } } },
{ { {
	{ /* HT/VHT NSTS=2, 20MHz */
	{ /* no SGI */
	{130, 260, 390, 520, 780, 1040, 1170, 1300, 1560, 1733, 1950, 2167}
	},
	{ /* SGI */
	{144, 289, 433, 578, 867, 1156, 1300, 1444, 1733, 1927, 2167, 2407}
	}
} },
{
	{ /* HT/VHT NSTS=2, 40MHz */
	{ /* no SGI */
	{270, 540, 810, 1080, 1620, 2160, 2430, 2700, 3240, 3600, 4050, 4500}
	},
	{ /* SGI */
	{300, 600, 900, 1200, 1800, 2400, 2700, 3000, 3600, 4000, 4500, 5000}
	}
} },
{
	{ /* HT/VHT NSTS=2, 80MHz */
	{ /* no SGI */
	{585, 1170, 1755, 2340, 3510, 4680, 5265, 5850, 7020, 7800, 8775, 9750}
	},
	{ /* SGI */
	{650, 1300, 1950, 2600, 3900, 5200, 5850, 6500, 7800, 8667, 9750, 10833}
	}
} },
{
	{ /* HT/VHT NSTS=2, 160MHz */
	{ /* no SGI */
	{1170, 2340, 3510, 4680, 7020, 9360, 10530, 11700, 14040, 15600,
	 17550, 19500}
	},
	{ /* SGI */
	{1300, 2600, 3900, 5200, 7800, 10400, 11700, 13000, 15600, 17333,
	 19500, 21667}
	}
} } } },
{ { {
	{ /* HT/VHT NSTS=3, 20MHz */
	{ /* no SGI */
	{195, 390, 585, 780, 1170, 1560, 1755, 1950, 2340, 2600, 2925, 3250}
	},
	{ /* SGI */
	{217, 433, 650, 867, 1300, 1733, 1950, 2167, 2600, 2889, 3250, 3611}
	}
} },
{
	{ /* HT/VHT NSTS=3, 40MHz */
	{ /* no SGI */
	{405, 810, 1215, 1620, 2430, 3240, 3645, 4050, 4860, 5400, 6075, 6750}
	},
	{ /* SGI */
	{450, 900, 1350, 1800, 2700, 3600, 4050, 4500, 5400, 6000, 6750, 7500}
	}
} },
{
	{ /* HT/VHT NSTS=3, 80MHz */
	{ /* no SGI */
	{878, 1755, 2633, 3510, 5265, 7020, 0, 8775, 10530, 11700, 13163, 14625}
	},
	{ /* SGI */
	{975, 1950, 2925, 3900, 5850, 7800, 0, 9750, 11700, 13000, 14625, 16250}
	}
} },
{
	{ /* HT/VHT NSTS=3, 160MHz */
	{ /* no SGI */
	{1755, 3510, 5265, 7020, 10530, 14040, 15795, 17550, 21060, 23400,
	 26325, 29250}
	},
	{ /* SGI */
	{1950, 3900, 5850, 7800, 11700, 15600, 17550, 19500, 23400, 26000,
	 29250, 32500}
	}
} } } } }
};

/* data rate mapping table for ax */
struct axDataRateMappingTable_t {
	struct axNsts_t {
		struct axBw_t {
			struct axGi_t {
				uint32_t rate[16];
			} gi[3];
		} bw[5];
	} nsts[4];
} g_rAxDataRateMappingTable = {
{ { { { { /* HE/EHT NSTS=1, 20MHz */
	{ /* GI 0.8 */
	{86, 172, 258, 344, 516, 688, 774, 860, 1032, 1147, 1290, 1434,
		1549, 1721, 0, 43}
	},
	{ /* GI 1.6 */
	{81, 163, 244, 325, 488, 650, 731, 813, 975, 1083, 1219, 1354,
		1463, 1625, 0, 40}
	},
	{ /* GI 3.2 */
	{73, 146, 219, 293, 439, 585, 658, 731, 878, 975, 1097, 1219,
		1316, 1463, 0, 36}
	}
} },
{
	{ /* HE/EHT NSTS=1, 40MHz */
	{ /* GI 0.8 */
	{172, 344, 516, 688, 1032, 1376, 1549, 1721, 2065, 2294, 2581, 2868,
		3097, 3441, 0, 86}
	},
	{ /* GI 1.6 */
	{163, 325, 488, 650, 975, 1300, 1463, 1625, 1950, 2167, 2438, 2708,
		2925, 3250, 0, 81}
	},
	{ /* GI 3.2 */
	{146, 293, 439, 585, 878, 1170, 1316, 1463, 1755, 1950, 2194, 2438,
		2633, 2925, 0, 73}
	}
} },
{
	{ /* HE/EHT NSTS=1, 80MHz */
	{ /* GI 0.8 */
	{360, 721, 1081, 1441, 2162, 2882, 3243, 3603, 4324, 4804, 5404, 6005,
		6485, 7206, 0, 180}
	},
	{ /* GI 1.6 */
	{340, 681, 1021, 1361, 2042, 2722, 3063, 3403, 4083, 4537, 5104, 5671,
		6125, 6806, 0, 170}
	},
	{ /* GI 3.2 */
	{306, 613, 919, 1225, 1838, 2450, 2756, 3063, 3675, 4083, 4594, 5104,
		5513, 6125, 0, 153}
	}
} },
{
	{ /* HE/EHT NSTS=1, 160MHz */
	{ /* GI 0.8 */
	{721, 1441, 2162, 2882, 4324, 5765, 6485, 7206, 8647, 9608, 10809,
		12010, 12971, 14412, 0, 360}
	},
	{ /* GI 1.6 */
	{681, 1361, 2042, 2722, 4083, 5444, 6125, 6806, 8167, 9074, 10208,
		11343, 12250, 13611, 0, 340}
	},
	{ /* GI 3.2 */
	{613, 1225, 1838, 2450, 3675, 4900, 5513, 6125, 7350, 8167, 9188, 10208,
		11025, 12250, 0, 306}
	}
} },
{
	{ /* HE/EHT NSTS=1, 320MHz */
	{ /* GI 0.8 */
	{1441, 2882, 4324, 5765, 8647, 11529, 12971, 14412, 17294, 19215, 21618,
		24019, 25941, 28824, 0, 721}
	},
	{ /* GI 1.6 */
	{1361, 2722, 4083, 5444, 8167, 10889, 12250, 13611, 16333, 18148, 20417,
		22685, 24500, 27222, 0, 681}
	},
	{ /* GI 3.2 */
	{1225, 2450, 3675, 4900, 7350, 9800, 11025, 12250, 14700, 16333, 18375,
		20416, 22050, 24500, 0, 613}
	}
} } } },
{ { {
	{ /* HE/EHT NSTS=2, 20MHz */
	{ /* GI 0.8 */
	{172, 344, 516, 688, 1032, 1376, 1549, 1721, 2065, 2294, 2581, 2868,
		3097, 3441, 0, 86}
	},
	{ /* GI 1.6 */
	{163, 325, 488, 650, 975, 1300, 1463, 1625, 1950, 2167, 2438, 2708,
		2925, 3250, 0, 81}
	},
	{ /* GI 3.2 */
	{146, 293, 439, 585, 878, 1170, 1316, 1463, 1755, 1950, 2194, 2438,
		2633, 2925, 0, 73}
	}
} },
{
	{ /* HE/EHT NSTS=2, 40MHz */
	{ /* GI 0.8 */
	{344, 688, 1032, 1376, 2065, 2753, 3097, 3441, 4129, 4588, 5162, 5735,
		6194, 6882, 0, 172}
	},
	{ /* GI 1.6 */
	{325, 650, 975, 1300, 1950, 2600, 2925, 3250, 3900, 4333, 4875, 5417,
		5850, 6500, 0, 162}
	},
	{ /* GI 3.2 */
	{293, 585, 878, 1170, 1755, 2340, 2633, 2925, 3510, 3900, 4388, 4875,
		5326, 5850, 0, 292}
	}
} },
{
	{ /* HE/EHT NSTS=2, 80MHz */
	{ /* GI 0.8 */
	{721, 1441, 2162, 2882, 4324, 5765, 6485, 7206, 8647, 9608, 10809,
		12010, 12971, 14412, 0, 360}
	},
	{ /* GI 1.6 */
	{681, 1361, 2042, 2722, 4083, 5444, 6125, 6806, 8167, 9074, 10208,
		11343, 12250, 13611, 0, 340}
	},
	{ /* GI 3.2 */
	{613, 1225, 1838, 2450, 3675, 4900, 5513, 6125, 7350, 8167, 9188, 10208,
		11025, 12250, 0, 306}
	}
} },
{
	{ /* HE/EHT NSTS=2, 160MHz */
	{ /* GI 0.8 */
	{1441, 2882, 4324, 5765, 8647, 11529, 12971, 14412, 17294, 19215, 21618,
		24019, 25941, 28824, 0, 721}
	},
	{ /* GI 1.6 */
	{1361, 2722, 4083, 5444, 8167, 10889, 12250, 13611, 16333, 18148, 20417,
		22685, 24500, 27222, 0, 681}
	},
	{ /* GI 3.2 */
	{1225, 2450, 3675, 4900, 7350, 9800, 11025, 12250, 14700, 16333, 18375,
		20416, 22050, 24500, 0, 613}
	}
} },
{
	{ /* HE/EHT NSTS=2, 320MHz */
	{ /* GI 0.8 */
	{2882, 5765, 8647, 11529, 17294, 23059, 25941, 28824, 34588, 38431,
		43235, 48039, 51882, 57648, 0, 1442}
	},
	{ /* GI 1.6 */
	{2722, 5444, 8167, 10889, 16333, 21778, 24500, 27222, 32667, 36296,
		40833, 45370, 49000, 54444, 0, 1362}
	},
	{ /* GI 3.2 */
	{2450, 4900, 7350, 9800, 14700, 19600, 22050, 24500, 29400, 32666,
		36750, 40833, 44100, 49000, 0, 1226}
	}
} } } },
{ { {
	{ /* HE/EHT NSTS=3, 20MHz */
	{ /* GI 0.8 */
	{258, 516, 774, 1032, 1549, 2065, 2323, 2581, 3097, 3441, 3871, 4301,
		4646, 5162, 0, 129}
	},
	{ /* GI 1.6 */
	{244, 488, 731, 975, 1463, 1950, 2194, 2438, 2925, 3250, 3656, 4063,
		4388, 4875, 0, 122}
	},
	{ /* GI 3.2 */
	{219, 439, 658, 878, 1316, 1755, 1974, 2194, 2633, 2925, 3291, 3656,
		3949, 4388, 0, 109}
	}
} },
{
	{ /* HE/EHT NSTS=3, 40MHz */
	{ /* GI 0.8 */
	{516, 1032, 1549, 2065, 3097, 4129, 4646, 5162, 6194, 6882, 7743, 8603,
		9292, 10324, 0, 258}
	},
	{ /* GI 1.6 */
	{488, 975, 1463, 1950, 2925, 3900, 4388, 4875, 5850, 6500, 7313, 8125,
		8776, 9750, 0, 244}
	},
	{ /* GI 3.2 */
	{439, 878, 1316, 1755, 2633, 3510, 3949, 4388, 5265, 5850, 6581, 7313,
		7898, 8776, 0, 218}
	}
} },
{
	{ /* HE/EHT NSTS=3, 80MHz */
	{ /* GI 0.8 */
	{1081, 2162, 3243, 4324, 6485, 8647, 9728, 10809, 12971, 14412, 16213,
		18015, 19456, 21618, 0, 540}
	},
	{ /* GI 1.6 */
	{1021, 2042, 3063, 4083, 6125, 8167, 9188, 10208, 12250, 13611, 15313,
		17014, 18375, 20417, 0, 510}
	},
	{ /* GI 3.2 */
	{919, 1838, 2756, 3675, 5513, 7350, 8269, 9188, 11025, 12250, 13781,
		15313, 16538, 18375, 0, 459}
	}
} },
{
	{ /* HE/EHT NSTS=3, 160MHz */
	{ /* GI 0.8 */
	{2162, 4324, 6485, 8647, 12971, 17294, 19456, 21618, 25941, 28824,
		32426, 36029, 38912, 43236, 0, 1080}
	},
	{ /* GI 1.6 */
	{2042, 4083, 6125, 8167, 12250, 16333, 18375, 20417, 24500, 27222,
		30625, 34028, 36750, 40834, 0, 1020}
	},
	{ /* GI 3.2 */
	{1838, 3675, 5513, 7350, 11025, 14700, 16538, 18375, 22050, 24500,
		27563, 30625, 33076, 36750, 0, 918}
	}
} },
{
	{ /* HE/EHT NSTS=3, 320MHz */
	{ /* GI 0.8 */
	{4324, 8647, 12971, 17294, 25941, 34588, 38912, 43235, 51882, 57647,
		64853, 72059, 77824, 86472, 0, 2160}
	},
	{ /* GI 1.6 */
	{4083, 8167, 12250, 16333, 24500, 32667, 36750, 40833, 49000, 54444,
		61250, 68056, 73500, 81668, 0, 2040}
	},
	{ /* GI 3.2 */
	{3675, 7350, 11025, 14700, 22050, 29400, 33075, 36750, 44100, 49000,
		55125, 61250, 66152, 73500, 0, 1836}
	}
} } } },
{ { {
	{ /* HE/EHT NSTS=4, 20MHz */
	{ /* GI 0.8 */
	{344, 688, 1032, 1376, 2065, 2753, 3097, 3441, 4129, 4588, 5162, 5735,
		6194, 6882, 0, 172}
	},
	{ /* GI 1.6 */
	{325, 650, 975, 1300, 1950, 2600, 2925, 3250, 3900, 4333, 4875, 5417,
		5850, 6500, 0, 162}
	},
	{ /* GI 3.2 */
	{293, 585, 878, 1170, 1755, 2340, 2633, 2925, 3510, 3900, 4388, 4875,
		5326, 5850, 0, 146}
	}
} },
{
	{ /* HE/EHT NSTS=4, 40MHz */
	{ /* GI 0.8 */
	{688, 1376, 2065, 2753, 4129, 5506, 6194, 6882, 8259, 9176, 10324,
		11471, 12388, 13764, 0, 344}
	},
	{ /* GI 1.6 */
	{650, 1300, 1950, 2600, 3900, 5200, 5850, 6500, 7800, 8667, 9750, 10833,
		11700, 13000, 0, 324}
	},
	{ /* GI 3.2 */
	{585, 1170, 1755, 2340, 3510, 4680, 5265, 5850, 7020, 7800, 8775, 9750,
		10652, 11700, 0, 292}
	}
} },
{
	{ /* HE/EHT NSTS=4, 80MHz */
	{ /* GI 0.8 */
	{1441, 2882, 4324, 5765, 8647, 11529, 12971, 14412, 17294, 19215, 21618,
		24019, 25941, 28824, 0, 721}
	},
	{ /* GI 1.6 */
	{1361, 2722, 4083, 5444, 8167, 10889, 12250, 13611, 16333, 18148, 20417,
		22685, 24500, 27222, 0, 681}
	},
	{ /* GI 3.2 */
	{1225, 2450, 3675, 4900, 7350, 9800, 11025, 12250, 14700, 16333, 18375,
		20416, 22050, 24500, 0, 613}
	}
} },
{
	{ /* HE/EHT NSTS=4, 160MHz */
	{ /* GI 0.8 */
	{2882, 5765, 8647, 11529, 17294, 23059, 25941, 28824, 34588, 38431,
		43235, 48039, 51882, 57648, 0, 1442}
	},
	{ /* GI 1.6 */
	{2722, 5444, 8167, 10889, 16333, 21778, 24500, 27222, 32667, 36296,
		40833, 45370, 49000, 54444, 0, 1362}
	},
	{ /* GI 3.2 */
	{2450, 4900, 7350, 9800, 14700, 19600, 22050, 24500, 29400, 32666,
		36750, 40833, 44100, 49000, 0, 1226}
	}
} },
{
	{ /* HE/EHT NSTS=4, 320MHz */
	{ /* GI 0.8 */
	{5765, 11529, 17294, 23059, 34588, 46118, 51882, 57647, 69176, 76863,
		86471, 96078, 103764, 115296, 0, 2884}
	},
	{ /* GI 1.6 */
	{5444, 10889, 16333, 21778, 32667, 43556, 49000, 54444, 65333, 72592,
		81667, 90740, 98000, 108888, 0, 2724}
	},
	{ /* GI 3.2 */
	{4900, 9800, 14700, 19600, 29400, 39200, 44100, 49000, 58800, 65333,
		73500, 81666, 88200, 98000, 0, 2452}
	}
} } } } }
};

struct PARAM_CUSTOM_KEY_CFG_STRUCT g_rEmCfgBk[WLAN_CFG_REC_ENTRY_NUM_MAX];


struct PARAM_CUSTOM_KEY_CFG_STRUCT g_rDefaulteSetting[] = {
	/*format :
	*: {
	*	"firmware config parameter",
	*	"firmware config value",
	*	"Operation:default 0"
	*   }
	*/
	{"AdapScan", "0x0", WLAN_CFG_DEFAULT},
#if CFG_SUPPORT_IOT_AP_BLACKLIST
	/*Fill Iot AP blacklist here*/
	/*AS AX89X, OUI=0x8CFDF0, NSS=8, PHY=WiFi6, Action=2:Disable SG*/
	{"IOTAP31", "0:8CFDF0:::::8:6::2"},
#endif
#if CFG_TC3_FEATURE
	{"ScreenOnBeaconTimeoutCount", "20"},
	{"ScreenOffBeaconTimeoutCount", "10"},
	{"AgingPeriod", "0x19"},
	{"DropPacketsIPV4Low", "0x1"},
	{"DropPacketsIPV6Low", "0x1"},
	{"Sta2gBw", "1"},
#endif
};

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define SIGNED_EXTEND(n, _sValue) \
	(((_sValue) & BIT((n)-1)) ? ((_sValue) | BITS(n, 31)) : \
	 ((_sValue) & ~BITS(n, 31)))

/* TODO: Check */
/* OID set handlers without the need to access HW register */
PFN_OID_HANDLER_FUNC apfnOidSetHandlerWOHwAccess[] = {
	wlanoidSetChannel,
	wlanoidSetBeaconInterval,
	wlanoidSetAtimWindow,
	wlanoidSetFrequency,
};

/* TODO: Check */
/* OID query handlers without the need to access HW register */
PFN_OID_HANDLER_FUNC apfnOidQueryHandlerWOHwAccess[] = {
	wlanoidQueryBssid,
	wlanoidQuerySsid,
	wlanoidQueryInfrastructureMode,
	wlanoidQueryAuthMode,
	wlanoidQueryEncryptionStatus,
	wlanoidQueryNetworkTypeInUse,
	wlanoidQueryBssidList,
	wlanoidQueryAcpiDevicePowerState,
	wlanoidQuerySupportedRates,
	wlanoidQuery802dot11PowerSaveProfile,
	wlanoidQueryBeaconInterval,
	wlanoidQueryAtimWindow,
	wlanoidQueryFrequency,
};

/* OID set handlers allowed in RF test mode */
PFN_OID_HANDLER_FUNC apfnOidSetHandlerAllowedInRFTest[] = {
	wlanoidRftestSetTestMode,
	wlanoidRftestSetAbortTestMode,
	wlanoidRftestSetAutoTest,
	wlanoidSetMcrWrite,
	wlanoidSetEepromWrite
};

/* OID query handlers allowed in RF test mode */
PFN_OID_HANDLER_FUNC apfnOidQueryHandlerAllowedInRFTest[] = {
	wlanoidRftestQueryAutoTest,
	wlanoidQueryMcrRead,
	wlanoidQueryEepromRead
}

;

PFN_OID_HANDLER_FUNC apfnOidWOTimeoutCheck[] = {
	wlanoidRftestSetTestMode,
	wlanoidRftestSetAbortTestMode,
	wlanoidSetAcpiDevicePowerState,
};

#define NVRAM_TAG_HDR_SIZE  3 /*ID+Len MSB+Len LSB*/

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/*!
 * \brief This is a private routine, which is used to check if HW access is
 *        needed for the OID query/ set handlers.
 *
 * \param[IN] pfnOidHandler Pointer to the OID handler.
 * \param[IN] fgSetInfo     It is a Set information handler.
 *
 * \retval TRUE This function needs HW access
 * \retval FALSE This function does not need HW access
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanIsHandlerNeedHwAccess(PFN_OID_HANDLER_FUNC
				   pfnOidHandler, u_int8_t fgSetInfo)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerWOHwAccess;
	uint32_t i;
	uint32_t u4NumOfElem;

	if (fgSetInfo) {
		apfnOidHandlerWOHwAccess = apfnOidSetHandlerWOHwAccess;
		u4NumOfElem = sizeof(apfnOidSetHandlerWOHwAccess) / sizeof(
				      PFN_OID_HANDLER_FUNC);
	} else {
		apfnOidHandlerWOHwAccess = apfnOidQueryHandlerWOHwAccess;
		u4NumOfElem = sizeof(apfnOidQueryHandlerWOHwAccess) /
			      sizeof(PFN_OID_HANDLER_FUNC);
	}

	for (i = 0; i < u4NumOfElem; i++) {
		if (apfnOidHandlerWOHwAccess[i] == pfnOidHandler)
			return FALSE;
	}

	return TRUE;
} /* wlanIsHandlerNeedHwAccess */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set flag for later handling card
 *        ejected event.
 *
 * \param[in] prAdapter Pointer to the Adapter structure.
 *
 * \return (none)
 *
 * \note When surprised removal happens, Glue layer should invoke this
 *       function to notify WPDD not to do any hw access.
 */
/*----------------------------------------------------------------------------*/
void wlanCardEjected(struct ADAPTER *prAdapter)
{
	DEBUGFUNC("wlanCardEjected");
	/* INITLOG(("\n")); */

	ASSERT(prAdapter);

	/* mark that the card is being ejected, NDIS will shut us down soon */
	nicTxRelease(prAdapter, FALSE);

} /* wlanCardEjected */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to check driver ready state
 *
 * \param[in] prGlueInfo Pointer to the GlueInfo structure.
 *
 * \retval TRUE Driver is ready for kernel access
 * \retval FALSE Driver is not ready
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanIsDriverReady(struct GLUE_INFO *prGlueInfo,
				  uint32_t u4Check)
{
	u_int8_t fgIsReady = TRUE;
	uint8_t u1State = 0;

	if (!prGlueInfo)
		fgIsReady = FALSE;
	else {
		if ((u4Check & WLAN_DRV_READY_CHECK_WLAN_ON) &&
			(prGlueInfo->u4ReadyFlag == FALSE))
			fgIsReady = FALSE;

		if ((u4Check & WLAN_DRV_READY_CHECK_HIF_SUSPEND) &&
			(!halIsHifStateReady(prGlueInfo, &u1State))) {
			DBGLOG(REQ, WARN, "driver state[%d]\n", u1State);
			fgIsReady = FALSE;
		}

		if ((u4Check & WLAN_DRV_READY_CHECK_RESET) &&
			kalIsResetting())
			fgIsReady = FALSE;
	}

	return fgIsReady;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Create adapter object
 *
 * \param prAdapter This routine is call to allocate the driver software
 *		    objects. If fails, return NULL.
 * \retval NULL If it fails, NULL is returned.
 * \retval NOT NULL If the adapter was initialized successfully.
 */
/*----------------------------------------------------------------------------*/
struct ADAPTER *wlanAdapterCreate(struct GLUE_INFO
				  *prGlueInfo)
{
	struct ADAPTER *prAdpater = (struct ADAPTER *) NULL;

	DEBUGFUNC("wlanAdapterCreate");

	do {
		prAdpater = (struct ADAPTER *) kalMemAlloc(sizeof(
					struct ADAPTER), VIR_MEM_TYPE);

		if (!prAdpater) {
			DBGLOG(INIT, ERROR,
			       "Allocate ADAPTER memory ==> FAILED\n");
			break;
		}
#if QM_TEST_MODE
		g_rQM.prAdapter = prAdpater;
#endif
		kalMemZero(prAdpater, sizeof(struct ADAPTER));
		prAdpater->prGlueInfo = prGlueInfo;

	} while (FALSE);

	return prAdpater;
}				/* wlanAdapterCreate */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Destroy adapter object
 *
 * \param prAdapter This routine is call to destroy the driver software objects.
 *                  If fails, return NULL.
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanAdapterDestroy(struct ADAPTER *prAdapter)
{
	if (!prAdapter)
		return;

	scanLogCacheFlushAll(prAdapter,
		&(prAdapter->rWifiVar.rScanInfo.rScanLogCache),
		LOG_SCAN_D2D);

	kalMemFree(prAdapter, VIR_MEM_TYPE, sizeof(struct ADAPTER));
}

void wlanOnPreAllocAdapterMem(struct ADAPTER *prAdapter,
			  const u_int8_t bAtResetFlow)
{
	uint32_t i = 0, j = 0;

	DBGLOG(INIT, TRACE, "start.\n");

	if (!bAtResetFlow) {
		/* 4 <0> Reset variables in ADAPTER_T */
		/* prAdapter->fgIsFwOwn = TRUE; */
		prAdapter->fgIsEnterD3ReqIssued = FALSE;
		prAdapter->ucHwBssIdNum = MAX_BSSID_NUM;
		prAdapter->ucWmmSetNum = MAX_BSSID_NUM;
		prAdapter->ucP2PDevBssIdx = MAX_BSSID_NUM;
		prAdapter->ucWtblEntryNum = WTBL_SIZE;
		prAdapter->ucTxDefaultWlanIndex = prAdapter->ucWtblEntryNum - 1;

		prAdapter->u4HifDbgFlag = 0;
		prAdapter->u4HifChkFlag = 0;
		prAdapter->u4HifDbgMod = 0;
		prAdapter->u4HifDbgBss = 0;
		prAdapter->u4HifDbgReason = 0;
		prAdapter->u4HifTxHangDumpBitmap = 0;
		prAdapter->u4HifTxHangDumpIdx = 0;
		prAdapter->u4HifTxHangDumpNum = 0;
		prAdapter->ulNoMoreRfb = 0;
		prAdapter->u4WaitRecIdx = 0;
		prAdapter->u4CompRecIdx = 0;
		prAdapter->fgSetLogOnOff = true;
		prAdapter->fgSetLogLevel = true;

		/* Initialize rWlanInfo */
		kalMemSet(&(prAdapter->rWlanInfo), 0,
			sizeof(struct WLAN_INFO));

		/* Initialize aprBssInfo[].
		 * Important: index shall be same
		 *            when mapping between aprBssInfo[]
		 *            and arBssInfoPool[].rP2pDevInfo
		 *            is indexed to final one.
		 */
		for (i = 0; i < MAX_BSSID_NUM; i++)
			prAdapter->aprBssInfo[i] =
				&prAdapter->rWifiVar.arBssInfoPool[i];
		prAdapter->aprBssInfo[prAdapter->ucP2PDevBssIdx] =
			&prAdapter->rWifiVar.rP2pDevInfo;

#if (CFG_SUPPORT_POWER_THROTTLING == 1)
		LINK_INITIALIZE(&prAdapter->rPwrLevelHandlerList);
#endif
	} else {
		/* need to reset these values after the reset flow */
		prAdapter->ulNoMoreRfb = 0;
	}

	prAdapter->u4OwnFailedCount = 0;
	prAdapter->u4OwnFailedLogCount = 0;
	prAdapter->ucCmdSeqNum = 0;
	prAdapter->u4PwrCtrlBlockCnt = 0;
	prAdapter->fgIsPostponeTxEAPOLM3 = FALSE;

	if (bAtResetFlow) {
		for (i = 0; i < (prAdapter->ucHwBssIdNum + 1); i++)
			UNSET_NET_ACTIVE(prAdapter, i);

#if CFG_CE_ASSERT_DUMP
		/* Core dump maybe not complete,so need set
		 * back to FALSE or will result can't fw own.
		 */
		prAdapter->fgN9AssertDumpOngoing = FALSE;
#endif
#if defined(_HIF_SDIO)
		prAdapter->fgMBAccessFail = FALSE;
		prAdapter->prGlueInfo->rHifInfo.fgSkipRx = FALSE;
#endif
		prAdapter->fgIsChipNoAck = FALSE;
	}

	QUEUE_INITIALIZE(&(prAdapter->rPendingCmdQueue));
#if CFG_SUPPORT_MULTITHREAD
	QUEUE_INITIALIZE(&prAdapter->rTxCmdQueue);
	QUEUE_INITIALIZE(&prAdapter->rTxCmdDoneQueue);
#if CFG_FIX_2_TX_PORT
	QUEUE_INITIALIZE(&prAdapter->rTxP0Queue);
	QUEUE_INITIALIZE(&prAdapter->rTxP1Queue);
#else
	for (i = 0; i < MAX_BSSID_NUM; i++)
		for (j = 0; j < TC_NUM; j++)
			QUEUE_INITIALIZE(&prAdapter->rTxPQueue[i][j]);
#if (CFG_TX_HIF_PORT_QUEUE == 1)
	for (i = 0; i < MAX_BSSID_NUM; i++)
		for (j = 0; j < TC_NUM; j++)
			QUEUE_INITIALIZE(&prAdapter->rTxHifPQueue[i][j]);
#endif
#endif
	QUEUE_INITIALIZE(&prAdapter->rRxQueue);
	QUEUE_INITIALIZE(&prAdapter->rTxDataDoneQueue);
#endif

#if (CFG_TX_MGMT_BY_DATA_Q == 1)
	QUEUE_INITIALIZE(&prAdapter->rMgmtDirectTxQueue);
#endif /* CFG_TX_MGMT_BY_DATA_Q == 1 */

	/* 4 <0.1> reset fgIsBusAccessFailed */
	fgIsBusAccessFailed = FALSE;
	fgIsMcuOff = FALSE;
#if IS_ENABLED(CFG_MTK_WIFI_CONNV3_SUPPORT)
	fgTriggerDebugSop = FALSE;
#endif
}

void wlanOnPostNicInitAdapter(struct ADAPTER *prAdapter,
	struct REG_INFO *prRegInfo,
	const u_int8_t bAtResetFlow)
{
	DBGLOG(INIT, TRACE, "start.\n");

	/* 4 <2.1> Initialize System Service (MGMT Memory pool and
	 *	   STA_REC)
	 */
	nicInitSystemService(prAdapter, bAtResetFlow);

	if (!bAtResetFlow) {

		/* 4 <2.2> Initialize Feature Options */
		wlanInitFeatureOption(prAdapter);
#if CFG_SUPPORT_MTK_SYNERGY
#if 0 /* u2FeatureReserved is 0 on 6765 */
		if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE) {
			if (prRegInfo->prNvramSettings->u2FeatureReserved &
					BIT(MTK_FEATURE_2G_256QAM_DISABLED))
				prAdapter->rWifiVar.aucMtkFeature[0] &=
					~(MTK_SYNERGY_CAP_SUPPORT_24G_MCS89);
		}
#endif
#endif

		/* 4 <2.3> Overwrite debug level settings */
		wlanCfgSetDebugLevel(prAdapter);

		/* 4 <3> Initialize Tx */
		nicTxInitialize(prAdapter);
	} /* end of bAtResetFlow == FALSE */
#if defined(_HIF_SDIO)
	else {
		/* For SER L0.5, need reset resource before first CMD,
		 *  or maybe will get resource fail (resource exhaustion
		 * before SER).
		 */
		nicTxResetResource(prAdapter);
	}
#endif
	/* 4 <4> Initialize Rx */
	nicRxInitialize(prAdapter);
}

#if CFG_SUPPORT_NCHO

uint32_t wlanNchoSetFWEnable(struct ADAPTER *prAdapter, uint8_t fgEnable)
{
	char cmd[NCHO_CMD_MAX_LENGTH] = { 0 };
	uint32_t status = WLAN_STATUS_FAILURE;

	kalSnprintf(cmd, sizeof(cmd), "%s %d",
		FW_CFG_KEY_NCHO_ENABLE, fgEnable);
	status = wlanFwCfgParse(prAdapter, cmd);
	if (status != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, WARN, "set disable fail %d\n", status);
	return status;
}

uint32_t wlanNchoSetFWRssiTrigger(struct ADAPTER *prAdapter,
	int32_t i4RoamTriggerRssi)
{
	char cmd[NCHO_CMD_MAX_LENGTH] = { 0 };
	uint32_t status = WLAN_STATUS_FAILURE;

	kalSnprintf(cmd, sizeof(cmd), "%s %d",
		FW_CFG_KEY_NCHO_ROAM_RCPI, dBm_TO_RCPI(i4RoamTriggerRssi));
	status =  wlanFwCfgParse(prAdapter, cmd);
	if (status != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, WARN, "set roam rcpi fail %d\n", status);
	return status;
}

uint32_t wlanNchoSetFWScanPeriod(struct ADAPTER *prAdapter,
	uint32_t u4RoamScanPeriod)
{
	char cmd[NCHO_CMD_MAX_LENGTH] = { 0 };
	uint32_t status = WLAN_STATUS_FAILURE;

	kalSnprintf(cmd, sizeof(cmd), "%s %d",
		FW_CFG_KEY_NCHO_SCAN_PERIOD, u4RoamScanPeriod);
	status = wlanFwCfgParse(prAdapter, cmd);
	if (status != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, WARN, "set scan period fail %d\n", status);
	return status;
}

void wlanNchoInit(struct ADAPTER *prAdapter, uint8_t fgFwSync)
{
	uint8_t sync = fgFwSync && prAdapter->rNchoInfo.fgNCHOEnabled;

	/* NCHO Initialization */
	prAdapter->rNchoInfo.fgNCHOEnabled = 0;
	prAdapter->rNchoInfo.eBand = NCHO_BAND_AUTO;
	prAdapter->rNchoInfo.fgChGranted = FALSE;
	prAdapter->rNchoInfo.fgIsSendingAF = FALSE;
	prAdapter->rNchoInfo.u4RoamScanControl = 0;
	prAdapter->rNchoInfo.rRoamScnChnl.ucChannelListNum = 0;
	prAdapter->rNchoInfo.rAddRoamScnChnl.ucChannelListNum = 0;
	prAdapter->rNchoInfo.eDFSScnMode = NCHO_DFS_SCN_ENABLE1;
	prAdapter->rNchoInfo.i4RoamTrigger = -75;
	prAdapter->rNchoInfo.i4RoamDelta = 10;
	prAdapter->rNchoInfo.u4RoamScanPeriod =	ROAMING_DISCOVER_TIMEOUT_SEC;
	prAdapter->rNchoInfo.u4ScanChannelTime = 50;
	prAdapter->rNchoInfo.u4ScanHomeTime = 120;
	prAdapter->rNchoInfo.u4ScanHomeawayTime = 120;
	prAdapter->rNchoInfo.u4ScanNProbes = 2;
	prAdapter->rNchoInfo.u4WesMode = 0;
	prAdapter->rAddRoamScnChnl.ucChannelListNum = 0;

	/* sync with FW to let FW reset params */
	if (sync)
		wlanNchoSetFWEnable(prAdapter, 0);
}

#endif

void wlanOnPostFirmwareReady(struct ADAPTER *prAdapter,
		struct REG_INFO *prRegInfo)
{
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;

	DBGLOG(INIT, TRACE, "start.\n");
	/* OID timeout timer initialize */
	cnmTimerInitTimer(prAdapter,
			  &prAdapter->rOidTimeoutTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) wlanReleasePendingOid,
			  (uintptr_t) NULL);

	prAdapter->ucOidTimeoutCount = 0;

	prAdapter->fgIsChipNoAck = FALSE;

	/* Return Indicated Rfb list timer */
	cnmTimerInitTimer(prAdapter,
			  &prAdapter->rPacketDelaySetupTimer,
			  (PFN_MGMT_TIMEOUT_FUNC)
				wlanReturnPacketDelaySetupTimeout,
			  (uintptr_t) NULL);

	/* Power state initialization */
	prAdapter->fgWiFiInSleepyState = FALSE;
	prAdapter->rAcpiState = ACPI_STATE_D0;

#if 0
	/* Online scan option */
	if (prRegInfo->fgDisOnlineScan == 0)
		prAdapter->fgEnOnlineScan = TRUE;
	else
		prAdapter->fgEnOnlineScan = FALSE;

	/* Beacon lost detection option */
	if (prRegInfo->fgDisBcnLostDetection != 0)
		prAdapter->fgDisBcnLostDetection = TRUE;
#else
	if (prAdapter->rWifiVar.fgDisOnlineScan == 0)
		prAdapter->fgEnOnlineScan = TRUE;
	else
		prAdapter->fgEnOnlineScan = FALSE;

	/* Beacon lost detection option */
	if (prAdapter->rWifiVar.fgDisBcnLostDetection != 0)
		prAdapter->fgDisBcnLostDetection = TRUE;
	if (prAdapter->rWifiVar.fgDisAgingLostDetection != 0)
		prAdapter->fgDisStaAgingTimeoutDetection = TRUE;
#endif

	/* Load compile time constant */
	prAdapter->rWlanInfo.u2BeaconPeriod =
		CFG_INIT_ADHOC_BEACON_INTERVAL;
	prAdapter->rWlanInfo.u2AtimWindow =
		CFG_INIT_ADHOC_ATIM_WINDOW;

#if 1				/* set PM parameters */
	prAdapter->u4PsCurrentMeasureEn =
		prRegInfo->u4PsCurrentMeasureEn;
#if 0
	prAdapter->fgEnArpFilter = prRegInfo->fgEnArpFilter;
	prAdapter->u4UapsdAcBmp = prRegInfo->u4UapsdAcBmp;
	prAdapter->u4MaxSpLen = prRegInfo->u4MaxSpLen;
#else
	prAdapter->fgEnArpFilter =
		prAdapter->rWifiVar.fgEnArpFilter;
	prAdapter->u4UapsdAcBmp = prAdapter->rWifiVar.u4UapsdAcBmp;
	prAdapter->u4MaxSpLen = prAdapter->rWifiVar.u4MaxSpLen;
	prAdapter->u4P2pUapsdAcBmp = prAdapter->rWifiVar.u4P2pUapsdAcBmp;
	prAdapter->u4P2pMaxSpLen = prAdapter->rWifiVar.u4P2pMaxSpLen;
#endif
	DBGLOG(INIT, TRACE,
	       "[1] fgEnArpFilter:0x%x, u4UapsdAcBmp:0x%x, u4MaxSpLen:0x%x",
	       prAdapter->fgEnArpFilter, prAdapter->u4UapsdAcBmp,
	       prAdapter->u4MaxSpLen);

	prAdapter->fgEnCtiaPowerMode = FALSE;

#endif
	/* QA_TOOL and ICAP info struct */
	prAdapter->rIcapInfo.eIcapState = ICAP_STATE_INIT;
	prAdapter->rIcapInfo.u2DumpIndex = 0;
	prAdapter->rIcapInfo.u4CapNode = 0;

	/* MGMT Initialization */
	nicInitMGMT(prAdapter, prRegInfo);

#if CFG_SUPPORT_NCHO
	wlanNchoInit(prAdapter, FALSE);
#endif

	/* Enable WZC Disassociation */
	prAdapter->rWifiVar.fgSupportWZCDisassociation = TRUE;

	/* Apply Rate Setting */
	if ((enum ENUM_REGISTRY_FIXED_RATE) (prRegInfo->u4FixedRate)
	    < FIXED_RATE_NUM)
		prAdapter->rWifiVar.eRateSetting =
				(enum ENUM_REGISTRY_FIXED_RATE)
				(prRegInfo->u4FixedRate);
	else
		prAdapter->rWifiVar.eRateSetting = FIXED_RATE_NONE;

	if (prAdapter->rWifiVar.eRateSetting == FIXED_RATE_NONE) {
		/* Enable Auto (Long/Short) Preamble */
		prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_AUTO;
	} else if ((prAdapter->rWifiVar.eRateSetting >=
		    FIXED_RATE_MCS0_20M_400NS &&
		    prAdapter->rWifiVar.eRateSetting <=
		    FIXED_RATE_MCS7_20M_400NS)
		   || (prAdapter->rWifiVar.eRateSetting >=
		       FIXED_RATE_MCS0_40M_400NS &&
		       prAdapter->rWifiVar.eRateSetting <=
		       FIXED_RATE_MCS32_400NS)) {
		/* Force Short Preamble */
		prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_SHORT;
	} else {
		/* Force Long Preamble */
		prAdapter->rWifiVar.ePreambleType = PREAMBLE_TYPE_LONG;
	}

	/* Disable Hidden SSID Join */
	prAdapter->rWifiVar.fgEnableJoinToHiddenSSID = FALSE;

	/* Enable Short Slot Time */
	prAdapter->rWifiVar.fgIsShortSlotTimeOptionEnable = TRUE;

	/* Disable skip dfs during scan*/
	prAdapter->rWifiVar.rScanInfo.fgSkipDFS = 0;

	/* configure available PHY type set */
	nicSetAvailablePhyTypeSet(prAdapter);

#if 0				/* Marked for MT6630 */
#if 1				/* set PM parameters */
		{
#if CFG_SUPPORT_PWR_MGT
			prAdapter->u4PowerMode = prRegInfo->u4PowerMode;
#if CFG_ENABLE_WIFI_DIRECT
			prAdapter->rWlanInfo.
			arPowerSaveMode[NETWORK_TYPE_P2P_INDEX].ucNetTypeIndex
				= NETWORK_TYPE_P2P_INDEX;
			prAdapter->rWlanInfo.
			arPowerSaveMode[NETWORK_TYPE_P2P_INDEX].ucPsProfile
				= ENUM_PSP_FAST_SWITCH;
#endif
#else
			prAdapter->u4PowerMode = ENUM_PSP_CONTINUOUS_ACTIVE;
#endif

			nicConfigPowerSaveProfile(prAdapter,
					  prAdapter->prAisBssInfo->ucBssIndex,
					  prAdapter->u4PowerMode, FALSE);
		}

#endif
#endif

#if CFG_SUPPORT_DYNAMIC_PWR_LIMIT
	/* dynamic tx power control initialization */
	/* note: call this API before loading NVRAM */
	txPwrCtrlInit(prAdapter);
#endif

	/* Check hardware 5g band support */
	if (prAdapter->fgIsHw5GBandDisabled)
		prAdapter->fgEnable5GBand = FALSE;
	else
		prAdapter->fgEnable5GBand = TRUE;

#if CFG_SUPPORT_NVRAM
	/* load manufacture data */
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE)
		wlanLoadManufactureData(prAdapter, prRegInfo);
	else
		DBGLOG(INIT, WARN, "%s: load manufacture data fail\n",
			       __func__);
#endif

#if 0
		/* Update Auto rate parameters in FW */
		nicRlmArUpdateParms(prAdapter, prRegInfo->u4ArSysParam0,
				    prRegInfo->u4ArSysParam1,
				    prRegInfo->u4ArSysParam2,
				    prRegInfo->u4ArSysParam3);
#endif

	/* Default QM RX BA timeout */
	prAdapter->u4QmRxBaMissTimeout = prWifiVar->u4BaMissTimeoutMs;

#if CFG_SUPPORT_LOWLATENCY_MODE
	wlanAdapterStartForLowLatency(prAdapter);
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */
#if CFG_SUPPORT_DYNAMIC_PWR_LIMIT
	/* dynamic tx power control load configuration */
	/* note: call this API after loading NVRAM */
	txPwrCtrlLoadConfig(prAdapter);
#endif
#if (CFG_WIFI_GET_MCS_INFO == 1)
	prAdapter->fgIsMcsInfoValid = FALSE;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Initialize the adapter. The sequence is
 *        1. Disable interrupt
 *        2. Read adapter configuration from EEPROM and registry, verify chip
 *	     ID.
 *        3. Create NIC Tx/Rx resource.
 *        4. Initialize the chip
 *        5. Initialize the protocol
 *        6. Enable Interrupt
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \retval WLAN_STATUS_SUCCESS: Success
 * \retval WLAN_STATUS_FAILURE: Failed
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanAdapterStart(struct ADAPTER *prAdapter,
					struct REG_INFO *prRegInfo,
					const u_int8_t bAtResetFlow)
{
	struct BUS_INFO *prBusInfo = NULL;
#if CFG_MTK_WIFI_SW_WFDMA
	struct SW_WFDMA_INFO *prSwWfdmaInfo = NULL;
#endif
	uint32_t u4Status = WLAN_STATUS_SUCCESS;
	enum ENUM_ADAPTER_START_FAIL_REASON {
		ALLOC_ADAPTER_MEM_FAIL,
		DRIVER_OWN_FAIL,
		INIT_ADAPTER_FAIL,
		INIT_HIFINFO_FAIL,
		SET_CHIP_ECO_INFO_FAIL,
		RAM_CODE_DOWNLOAD_FAIL,
		WAIT_FIRMWARE_READY_FAIL,
		FAIL_REASON_MAX
	} eFailReason;

	ASSERT(prAdapter);

	DEBUGFUNC("wlanAdapterStart");

	prBusInfo = prAdapter->chip_info->bus_info;
#if CFG_MTK_WIFI_SW_WFDMA
	prSwWfdmaInfo = &prBusInfo->rSwWfdmaInfo;
#endif

	eFailReason = FAIL_REASON_MAX;

	wlanOnPreAllocAdapterMem(prAdapter, bAtResetFlow);

	do {
		if (!bAtResetFlow) {
			u4Status = nicAllocateAdapterMemory(prAdapter);
			if (u4Status != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, ERROR,
						"nicAllocateAdapterMemory Error!\n");
				u4Status = WLAN_STATUS_FAILURE;
				eFailReason = ALLOC_ADAPTER_MEM_FAIL;
				break;
			}

			prAdapter->u4OsPacketFilter
				= PARAM_PACKET_FILTER_SUPPORTED;
		}
		/* set FALSE after wifi init flow or reset (not reinit WFDMA) */
		prAdapter->fgIsFwDownloaded = FALSE;

		DBGLOG(INIT, TRACE,
		       "wlanAdapterStart(): Acquiring LP-OWN\n");
		prAdapter->fgIsWiFiOnDrvOwn = TRUE;
		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
		prAdapter->fgIsWiFiOnDrvOwn = FALSE;
		DBGLOG(INIT, TRACE,
		       "wlanAdapterStart(): Acquiring LP-OWN-end\n");

#if (CFG_ENABLE_FULL_PM == 0)
		nicpmSetDriverOwn(prAdapter);
#endif

#if !defined(_HIF_USB)
		if (prAdapter->fgIsFwOwn == TRUE) {
			DBGLOG(INIT, ERROR, "nicpmSetDriverOwn() failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			eFailReason = DRIVER_OWN_FAIL;
			GL_DEFAULT_RESET_TRIGGER(prAdapter,
						 RST_WIFI_ON_DRV_OWN_FAIL);
			break;
		}
#endif

#if CFG_MTK_MDDP_SUPPORT
		setMddpSupportRegister(prAdapter);
#endif
		if (!bAtResetFlow) {
			/* 4 <1> Initialize the Adapter */
			u4Status = nicInitializeAdapter(prAdapter);
			if (u4Status != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, ERROR,
					"nicInitializeAdapter failed!\n");
				conn_dbg_add_log(CONN_DBG_LOG_TYPE_HW_ERR,
					"nicInitializeAdapter failed!\n");
				u4Status = WLAN_STATUS_FAILURE;
				eFailReason = INIT_ADAPTER_FAIL;
				break;
			}
		}

		wlanOnPostNicInitAdapter(prAdapter, prRegInfo, bAtResetFlow);

#if CFG_MTK_WIFI_SW_WFDMA
		if (prSwWfdmaInfo->fgIsEnAfterFwdl) {
			if (prSwWfdmaInfo->rOps.enable)
				prSwWfdmaInfo->rOps.enable(
					prAdapter->prGlueInfo, false);
		}
#endif
		u4Status = wlanWakeUpWiFi(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "wlanWakeUpWiFi failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}

		/* 4 <5> HIF SW info initialize */
		if (!halHifSwInfoInit(prAdapter)) {
			DBGLOG(INIT, ERROR, "halHifSwInfoInit failed!\n");
			conn_dbg_add_log(CONN_DBG_LOG_TYPE_HW_ERR,
				"halHifSwInfoInit failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			eFailReason = INIT_HIFINFO_FAIL;
			break;
		}

		fw_log_init(prAdapter);

		/* 4 <6> Enable HIF cut-through to N9 mode, not visiting CR4 */
		HAL_ENABLE_FWDL(prAdapter, TRUE);

		/* 4 <7> Get ECO Version */
		if (wlanSetChipEcoInfo(prAdapter) != WLAN_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, "wlanSetChipEcoInfo failed!\n");
			conn_dbg_add_log(CONN_DBG_LOG_TYPE_HW_ERR,
					"wlanSetChipEcoInfo failed!\n");
			u4Status = WLAN_STATUS_FAILURE;
			eFailReason = SET_CHIP_ECO_INFO_FAIL;
			break;
		}

		/* recheck Asic capability depends on ECO version */
		wlanCheckAsicCap(prAdapter);

#if CFG_ENABLE_FW_DOWNLOAD
		/* 4 <8> FW/patch download */

		/* 1. disable interrupt, download is done by polling mode only
		 */
		nicDisableInterrupt(prAdapter);

		/* 2. Initialize Tx Resource to fw download state */
		nicTxInitResetResource(prAdapter);

		u4Status = wlanDownloadFW(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
			eFailReason = RAM_CODE_DOWNLOAD_FAIL;

			GL_DEFAULT_RESET_TRIGGER(prAdapter, RST_FW_DL_FAIL);
			break;
		}
#endif

#if CFG_MTK_WIFI_SW_WFDMA
		if (prSwWfdmaInfo->fgIsEnAfterFwdl) {
			if (prSwWfdmaInfo->rOps.enable)
				prSwWfdmaInfo->rOps.enable(
					prAdapter->prGlueInfo, true);
		}
#endif

		DBGLOG(INIT, INFO, "Waiting for Ready bit..\n");

		/* 4 <9> check Wi-Fi FW asserts ready bit */
		u4Status = wlanCheckWifiFunc(prAdapter, TRUE);

		if (u4Status == WLAN_STATUS_SUCCESS) {
#if defined(_HIF_SDIO)
			uint32_t *pu4WHISR = NULL;
			uint16_t au2TxCount[SDIO_TX_RESOURCE_NUM];

			pu4WHISR = (uint32_t *)kalMemAlloc(sizeof(uint32_t),
							   PHY_MEM_TYPE);
			if (!pu4WHISR) {
				/* Every break should have a fail reason
				 * for driver clean up.
				 */
				eFailReason = RAM_CODE_DOWNLOAD_FAIL;
				DBGLOG(INIT, ERROR,
				       "Allocate pu4WHISR fail\n");
				u4Status = WLAN_STATUS_FAILURE;
				break;
			}
			/* 1. reset interrupt status */
			HAL_READ_INTR_STATUS(prAdapter, sizeof(uint32_t),
					     (uint8_t *)pu4WHISR);
			if (HAL_IS_TX_DONE_INTR(*pu4WHISR))
				HAL_READ_TX_RELEASED_COUNT(prAdapter,
							   au2TxCount);

			if (pu4WHISR)
				kalMemFree(pu4WHISR, PHY_MEM_TYPE,
					   sizeof(uint32_t));
#endif
			/* Set FW download success flag */
			prAdapter->fgIsFwDownloaded = TRUE;

			fw_log_start(prAdapter);

			/* 2. query & reset TX Resource for normal operation */
			wlanQueryNicResourceInformation(prAdapter);

#if (CFG_SUPPORT_NIC_CAPABILITY == 1)
			if (!bAtResetFlow) {
				/* 2.9 Workaround for Capability
				*CMD packet lost issue
				*wlanSendDummyCmd(prAdapter, TRUE);
				*/

				/* Connsys Fw log setting */
				wlanSetConnsysFwLog(prAdapter);

				/* 3. query for NIC capability */
				if (prAdapter->chip_info->isNicCapV1)
					wlanQueryNicCapability(prAdapter);

				/* 4. query for NIC capability V2 */
				u4Status = wlanQueryNicCapabilityV2(prAdapter);
				if (u4Status !=  WLAN_STATUS_SUCCESS) {
					DBGLOG(INIT, WARN,
						"wlanQueryNicCapabilityV2 failed.\n");
					RECLAIM_POWER_CONTROL_TO_PM(
						prAdapter, FALSE);
					eFailReason = WAIT_FIRMWARE_READY_FAIL;
					break;
				}
			}

			/* 5. reset TX Resource for normal operation
			 *    based on the information reported from
			 *    CMD_NicCapabilityV2
			 */
			wlanUpdateNicResourceInformation(prAdapter);

			wlanPrintVersion(prAdapter);
#endif

			/* 6. update basic configuration */
			wlanUpdateBasicConfig(prAdapter);

			if (!bAtResetFlow) {
				/* 7. Override network address */
				wlanUpdateNetworkAddress(prAdapter);

				/* 8. Apply Network Address */
				nicApplyNetworkAddress(prAdapter);
			}
		}

		if (u4Status != WLAN_STATUS_SUCCESS) {
			eFailReason = WAIT_FIRMWARE_READY_FAIL;
			break;
		}

		if (!bAtResetFlow)
			wlanOnPostFirmwareReady(prAdapter, prRegInfo);
		else {
#if CFG_SUPPORT_NVRAM
			/* load manufacture data */
			if (kalIsConfigurationExist(prAdapter->prGlueInfo)
				== TRUE)
				wlanLoadManufactureData(prAdapter, prRegInfo);
			else
				DBGLOG(INIT, WARN,
				"%s: load manufacture data fail\n", __func__);
#endif
		}

		/* restore to hardware default */
		HAL_SET_INTR_STATUS_READ_CLEAR(prAdapter);
		HAL_SET_MAILBOX_READ_CLEAR(prAdapter, FALSE);

		/* Enable interrupt */
		nicEnableInterrupt(prAdapter);

		/* init SER module */
		nicSerInit(prAdapter, bAtResetFlow);

#if (CFG_SUPPORT_POWER_THROTTLING == 1)
		/* init thermal protection */
		if (!bAtResetFlow) {
			thrmInit(prAdapter);
		}
#endif

		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);
	} while (FALSE);

	if (u4Status != WLAN_STATUS_SUCCESS) {
		prAdapter->u4HifDbgFlag |= DEG_HIF_DEFAULT_DUMP;
		halPrintHifDbgInfo(prAdapter);
		DBGLOG(INIT, WARN, "Fail reason: %d\n", eFailReason);

		/* Don't do error handling in chip reset flow, leave it to
		 * coming wlanRemove for full clean
		 */
		if (!bAtResetFlow) {
			/* release allocated memory */
			switch (eFailReason) {
			case WAIT_FIRMWARE_READY_FAIL:
			case RAM_CODE_DOWNLOAD_FAIL:
			case SET_CHIP_ECO_INFO_FAIL:
				fw_log_deinit(prAdapter);
				halHifSwInfoUnInit(prAdapter->prGlueInfo);
			/* fallthrough */
			case INIT_HIFINFO_FAIL:
				nicRxUninitialize(prAdapter);
				nicTxRelease(prAdapter, FALSE);
				/* System Service Uninitialization */
				nicUninitSystemService(prAdapter);
			/* fallthrough */
			case INIT_ADAPTER_FAIL:
			/* fallthrough */
			case DRIVER_OWN_FAIL:
				nicReleaseAdapterMemory(prAdapter);
				break;
			case ALLOC_ADAPTER_MEM_FAIL:
			default:
				break;
			}
		}
	}
#if CFG_SUPPORT_CUSTOM_NETLINK
	glCustomGenlInit();
#endif

#if (CFG_SUPPORT_CONNAC2X == 1)
	if (prAdapter->chip_info->checkbushang)
		prAdapter->chip_info->checkbushang((void *) prAdapter, TRUE);
#endif

	return u4Status;
}				/* wlanAdapterStart */

void wlanOffClearAllQueues(struct ADAPTER *prAdapter)
{
	DBGLOG(INIT, INFO, "wlanOffClearAllQueues(): start.\n");

	/* Release all CMD/MGMT/CmdData frame in command queue */
	kalClearCommandQueue(prAdapter->prGlueInfo, TRUE);

	/* Release all CMD in pending command queue */
	wlanClearPendingCommandQueue(prAdapter);

#if CFG_SUPPORT_MULTITHREAD

	/* Flush all items in queues for multi-thread */
	wlanClearTxCommandQueue(prAdapter);

	wlanClearTxCommandDoneQueue(prAdapter);

	wlanClearDataQueue(prAdapter);

	wlanClearRxToOsQueue(prAdapter);

#endif
}

void wlanOffUninitNicModule(struct ADAPTER *prAdapter,
	const u_int8_t bAtResetFlow)
{
	DBGLOG(INIT, INFO, "wlanOffUninitNicModule(): start.\n");
	nicRxUninitialize(prAdapter);

	nicTxRelease(prAdapter, FALSE);

	if (!bAtResetFlow) {
		/* MGMT - unitialization */
		nicUninitMGMT(prAdapter);

		/* System Service Uninitialization */
		nicUninitSystemService(prAdapter);
#if CFG_SUPPORT_DYNAMIC_PWR_LIMIT
		/* dynamic tx power control uninitialization */
		txPwrCtrlUninit(prAdapter);
#endif
		nicReleaseAdapterMemory(prAdapter);

#if defined(_HIF_SPI)
		/* Note: restore the SPI Mode Select from 32 bit to default */
		nicRestoreSpiDefMode(prAdapter);
#endif
	}  else {
		/* Timer Destruction */
		cnmTimerDestroy(prAdapter);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Uninitialize the adapter
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \retval WLAN_STATUS_SUCCESS: Success
 * \retval WLAN_STATUS_FAILURE: Failed
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanAdapterStop(struct ADAPTER *prAdapter,
		const u_int8_t bAtResetFlow)
{
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);

	wlanOffClearAllQueues(prAdapter);

	fw_log_stop(prAdapter);

	/* Hif power off wifi */
	if (prAdapter->rAcpiState == ACPI_STATE_D0 &&
		!wlanIsChipNoAck(prAdapter)
		&& !kalIsCardRemoved(prAdapter->prGlueInfo)) {
		wlanPowerOffWifi(prAdapter);
	} else {
		DBGLOG(INIT, ERROR, "Cannot WF pwr-off, release HIF TRX-res");
		HAL_CANCEL_TX_RX(prAdapter);
	}

	fw_log_deinit(prAdapter);
	halHifSwInfoUnInit(prAdapter->prGlueInfo);
	wlanOffUninitNicModule(prAdapter, bAtResetFlow);

#if CFG_SUPPORT_CUSTOM_NETLINK
	glCustomGenlDeinit();
#endif

	fgIsBusAccessFailed = FALSE;
	fgIsMcuOff = FALSE;
#if IS_ENABLED(CFG_MTK_WIFI_CONNV3_SUPPORT)
	fgTriggerDebugSop = FALSE;
#endif

	return u4Status;
}				/* wlanAdapterStop */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called by ISR (interrupt).
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \retval TRUE: NIC's interrupt
 * \retval FALSE: Not NIC's interrupt
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanISR(struct ADAPTER *prAdapter,
		 u_int8_t fgGlobalIntrCtrl)
{
	ASSERT(prAdapter);

	if (fgGlobalIntrCtrl) {
		nicDisableInterrupt(prAdapter);

		/* wlanIST(prAdapter); */
	}

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called by IST (task_let).
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanIST(struct ADAPTER *prAdapter, bool fgEnInt)
{
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);

	if (prAdapter->fgIsFwOwn == FALSE) {
		u4Status = nicProcessIST(prAdapter);
		if (u4Status != WLAN_STATUS_SUCCESS) {
#if defined(_HIF_PCIE) || defined(_HIF_AXI)
			GLUE_INC_REF_CNT(prAdapter->rHifStats.u4IsrNotIndCount);
#else
			DBGLOG_LIMITED(
				REQ, INFO,
				"Fail: nicProcessIST! status [%x]\n",
				u4Status);
#endif
		}
#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
		if (KAL_WAKE_LOCK_ACTIVE(prAdapter,
					 prAdapter->prGlueInfo->rIntrWakeLock))
			KAL_WAKE_UNLOCK(prAdapter,
					prAdapter->prGlueInfo->rIntrWakeLock);
#endif
	}

	if (fgEnInt)
		nicEnableInterrupt(prAdapter);
}

void wlanClearPendingInterrupt(struct ADAPTER *prAdapter)
{
	uint32_t i;

	i = 0;
	while (i < CFG_IST_LOOP_COUNT
	       && nicProcessIST(prAdapter) != WLAN_STATUS_NOT_INDICATING) {
		i++;
	};
}

void wlanCheckAsicCap(struct ADAPTER *prAdapter)
{
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(prAdapter);

	prChipInfo = prAdapter->chip_info;

	if (prChipInfo->wlanCheckAsicCap)
		prChipInfo->wlanCheckAsicCap(prAdapter);
}

uint32_t wlanCheckWifiFunc(struct ADAPTER *prAdapter,
			   u_int8_t fgRdyChk)
{
	u_int8_t fgResult, fgTimeout;
	uint32_t u4Result = 0, u4Status, u4StartTime, u4CurTime;
	const uint32_t ready_bits =
		prAdapter->chip_info->sw_ready_bits;

	u4StartTime = kalGetTimeTick();
	fgTimeout = FALSE;

#if defined(_HIF_USB)
	if (prAdapter->prGlueInfo->rHifInfo.state ==
	    USB_STATE_LINK_DOWN)
		return WLAN_STATUS_FAILURE;
#endif

	while (TRUE) {
		DBGLOG(INIT, TRACE,
			"Check ready_bits(=0x%x)\n", ready_bits);
		if (fgRdyChk)
			HAL_WIFI_FUNC_READY_CHECK(prAdapter,
					ready_bits /* WIFI_FUNC_READY_BITS */,
					&fgResult);
		else {
			HAL_WIFI_FUNC_OFF_CHECK(prAdapter,
					ready_bits /* WIFI_FUNC_READY_BITS */,
					&fgResult);
#if defined(_HIF_USB) || defined(_HIF_SDIO)
			if (nicProcessIST(prAdapter) !=
				WLAN_STATUS_NOT_INDICATING)
				DBGLOG_LIMITED(INIT, INFO,
				       "Handle pending interrupt\n");
#endif /* _HIF_USB or _HIF_SDIO */
		}
		u4CurTime = kalGetTimeTick();

		if (CHECK_FOR_TIMEOUT(u4CurTime, u4StartTime,
				      CFG_RESPONSE_POLLING_TIMEOUT *
				      CFG_RESPONSE_POLLING_DELAY)) {

			fgTimeout = TRUE;
		}

		if (fgResult) {
			if (fgRdyChk)
				DBGLOG(INIT, INFO,
					"Ready bit asserted\n");
			else
				DBGLOG(INIT, INFO,
					"Wi-Fi power off done!\n");

			u4Status = WLAN_STATUS_SUCCESS;

			break;
		} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
			   || fgIsBusAccessFailed == TRUE) {
			u4Status = WLAN_STATUS_FAILURE;

			break;
		} else if (fgTimeout) {
			HAL_WIFI_FUNC_GET_STATUS(prAdapter, u4Result);
			DBGLOG(INIT, ERROR,
			       "Waiting for %s: Timeout, Status=0x%08x\n",
			       fgRdyChk ? "ready bit" : "power off", u4Result);
			GL_DEFAULT_RESET_TRIGGER(prAdapter,
						 RST_CHECK_READY_BIT_TIMEOUT);
			u4Status = WLAN_STATUS_FAILURE;
			break;
		}
		kalMsleep(CFG_RESPONSE_POLLING_DELAY);

	}

	return u4Status;
}

uint32_t wlanPowerOffWifi(struct ADAPTER *prAdapter)
{
	uint32_t rStatus;
	/* Hif power off wifi */
	rStatus = halHifPowerOffWifi(prAdapter);
	prAdapter->fgIsCr4FwDownloaded = FALSE;

	return rStatus;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will check command queue to find out if any could be
 *	  dequeued and/or send to HIF to MT6620
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 * \param prCmdQue       Pointer of Command Queue (in Glue Layer)
 *
 * \retval WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanProcessCommandQueue(struct ADAPTER
				 *prAdapter, struct QUE *prCmdQue)
{
	uint32_t rStatus;
	struct QUE rTempCmdQue, rMergeCmdQue, rStandInCmdQue;
	struct QUE *prTempCmdQue, *prMergeCmdQue, *prStandInCmdQue;
	struct QUE_ENTRY *prQueueEntry = NULL;
	struct CMD_INFO *prCmdInfo;
	struct MSDU_INFO *prMsduInfo;
	enum ENUM_FRAME_ACTION eFrameAction = FRAME_ACTION_DROP_PKT;
#if CFG_DBG_MGT_BUF
	struct MEM_TRACK *prMemTrack = NULL;
#endif

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prCmdQue);

	prTempCmdQue = &rTempCmdQue;
	prMergeCmdQue = &rMergeCmdQue;
	prStandInCmdQue = &rStandInCmdQue;

	QUEUE_INITIALIZE(prTempCmdQue);
	QUEUE_INITIALIZE(prMergeCmdQue);
	QUEUE_INITIALIZE(prStandInCmdQue);

	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;
#if CFG_DBG_MGT_BUF
		if (prCmdInfo->pucInfoBuffer &&
				!IS_FROM_BUF(prAdapter,
					prCmdInfo->pucInfoBuffer)) {
			prMemTrack =
				(struct MEM_TRACK *)
					((uint8_t *)prCmdInfo->pucInfoBuffer -
						sizeof(struct MEM_TRACK));

			if (prMemTrack) {
				prMemTrack->u2CmdIdAndWhere &= 0x00FF;
				/* 0x11 means the CmdId drop in driver
				 */
				prMemTrack->u2CmdIdAndWhere |= 0x1100;
			}
		}
#endif
		switch (prCmdInfo->eCmdType) {
		case COMMAND_TYPE_NETWORK_IOCTL:
			/* command packet will be always sent */
			eFrameAction = FRAME_ACTION_TX_PKT;
			break;

		case COMMAND_TYPE_DATA_FRAME:
			/* inquire with QM */
			prMsduInfo = prCmdInfo->prMsduInfo;

			eFrameAction = qmGetFrameAction(prAdapter,
						prMsduInfo->ucBssIndex,
						prMsduInfo->ucStaRecIndex,
						NULL, FRAME_TYPE_802_1X,
						prCmdInfo->u2InfoBufLen);
			break;

		case COMMAND_TYPE_MANAGEMENT_FRAME:
			/* inquire with QM */
			prMsduInfo = prCmdInfo->prMsduInfo;

			eFrameAction = qmGetFrameAction(prAdapter,
						prMsduInfo->ucBssIndex,
						prMsduInfo->ucStaRecIndex,
						prMsduInfo, FRAME_TYPE_MMPDU,
						prMsduInfo->u2FrameLength);
			break;

		default:
			ASSERT(0);
			break;
		}
#if (CFG_SUPPORT_STATISTICS == 1)
		wlanWakeLogCmd(prCmdInfo->ucCID);
#endif
		/* 4 <3> handling upon dequeue result */
		if (eFrameAction == FRAME_ACTION_DROP_PKT) {
			DBGLOG(INIT, INFO,
			       "DROP CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
			       prCmdInfo->eCmdType, prCmdInfo->ucCID,
			       prCmdInfo->ucCmdSeqNum);
#if CFG_DBG_MGT_BUF
			if (prMemTrack) {
				prMemTrack->u2CmdIdAndWhere &= 0x00FF;
				/* 0x12 means the CmdId drop in driver
				 */
				prMemTrack->u2CmdIdAndWhere |= 0x1200;
			}
#endif
			wlanReleaseCommand(prAdapter, prCmdInfo,
					   TX_RESULT_DROPPED_IN_DRIVER);
			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		} else if (eFrameAction == FRAME_ACTION_QUEUE_PKT) {
			DBGLOG(INIT, TRACE,
			       "QUE back CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
			       prCmdInfo->eCmdType, prCmdInfo->ucCID,
			       prCmdInfo->ucCmdSeqNum);
#if CFG_DBG_MGT_BUF
			if (prMemTrack) {
				prMemTrack->u2CmdIdAndWhere &= 0x00FF;
				/* 0x13 means the CmdId queue back to rCmdQueue
				 */
				prMemTrack->u2CmdIdAndWhere |= 0x1300;
			}
#endif
			QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);
		} else if (eFrameAction == FRAME_ACTION_TX_PKT) {
			/* 4 <4> Send the command */
#if CFG_SUPPORT_MULTITHREAD
			rStatus = wlanSendCommandMthread(prAdapter, prCmdInfo);
			/* no more TC4 resource for further
			 * transmission
			 */
			if (rStatus == WLAN_STATUS_RESOURCES) {
				/*
				 * Because 7961 cmd res is not sufficient,
				 * so will dump log here frequently, and
				 * will result system busy.
				 */
				if (prAdapter->CurNoResSeqID !=
					prCmdInfo->ucCmdSeqNum) {
					DBGLOG(INIT, WARN,
				    "NO Res CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
				    prCmdInfo->eCmdType, prCmdInfo->ucCID,
				    prCmdInfo->ucCmdSeqNum);
				}
				prAdapter->CurNoResSeqID =
					prCmdInfo->ucCmdSeqNum;

				prAdapter->u4HifDbgFlag |= DEG_HIF_ALL;
				kalSetHifDbgEvent(prAdapter->prGlueInfo);

				QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);

				/*
				 * We reserve one TC4 resource for CMD
				 * specially, only break checking the left tx
				 * request if no resource for true CMD.
				 */
				if (prCmdInfo->eCmdType !=
					COMMAND_TYPE_MANAGEMENT_FRAME &&
				    prCmdInfo->eCmdType !=
					COMMAND_TYPE_DATA_FRAME)
					break;
			} else if (rStatus == WLAN_STATUS_PENDING) {
				/* Do nothing */
			} else if (rStatus == WLAN_STATUS_SUCCESS) {
				/* Do nothing */
			} else {
				struct CMD_INFO *prCmdInfo = (struct CMD_INFO *)
							     prQueueEntry;

				if (prCmdInfo->fgIsOid) {
					kalOidComplete(prAdapter->prGlueInfo,
						       prCmdInfo,
						       prCmdInfo->u4SetInfoLen,
						       rStatus);
				}
				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}

#else
			rStatus = wlanSendCommand(prAdapter, prCmdInfo);

			if (rStatus == WLAN_STATUS_RESOURCES) {
				/* no more TC4 resource for further
				 * transmission
				 */

				DBGLOG(INIT, WARN,
				       "NO Resource for CMD TYPE[%u] ID[0x%02X] SEQ[%u]\n",
				       prCmdInfo->eCmdType, prCmdInfo->ucCID,
				       prCmdInfo->ucCmdSeqNum);

				QUEUE_INSERT_TAIL(prMergeCmdQue, prQueueEntry);
				break;
			} else if (rStatus == WLAN_STATUS_PENDING) {
				/* command packet which needs further handling
				 * upon response
				 */
				KAL_ACQUIRE_PENDING_CMD_LOCK(prAdapter);
				QUEUE_INSERT_TAIL(
						&(prAdapter->rPendingCmdQueue),
						prQueueEntry);
				KAL_RELEASE_PENDING_CMD_LOCK(prAdapter);
			} else {
				struct CMD_INFO *prCmdInfo = (struct CMD_INFO *)
							     prQueueEntry;

				if (rStatus == WLAN_STATUS_SUCCESS) {
					if (prCmdInfo->pfCmdDoneHandler) {
						prCmdInfo->pfCmdDoneHandler(
						    prAdapter, prCmdInfo,
						    prCmdInfo->pucInfoBuffer);
					}
				} else {
					if (prCmdInfo->fgIsOid) {
						kalOidComplete(
						    prAdapter->prGlueInfo,
						    prCmdInfo,
						    prCmdInfo->u4SetInfoLen,
						    rStatus);
					}
				}

				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			}
#endif
		} else {
			ASSERT(0);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}

	/* 4 <3> Merge back to original queue */
	/* 4 <3.1> Merge prMergeCmdQue & prTempCmdQue */
	QUEUE_CONCATENATE_QUEUES(prMergeCmdQue, prTempCmdQue);

	/* 4 <3.2> Move prCmdQue to prStandInQue, due to prCmdQue might differ
	 *         due to incoming 802.1X frames
	 */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);
	QUEUE_MOVE_ALL(prStandInCmdQue, prCmdQue);

	/* 4 <3.3> concatenate prStandInQue to prMergeCmdQue */
	QUEUE_CONCATENATE_QUEUES(prMergeCmdQue, prStandInCmdQue);

	/* 4 <3.4> then move prMergeCmdQue to prCmdQue */
	QUEUE_MOVE_ALL(prCmdQue, prMergeCmdQue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_CMD_QUE);

#if CFG_SUPPORT_MULTITHREAD
	kalSetTxCmdEvent2Hif(prAdapter->prGlueInfo);
#endif

	return WLAN_STATUS_SUCCESS;
}				/* end of wlanProcessCommandQueue() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will take CMD_INFO_T which carry some information of
 *        incoming OID and notify the NIC_TX to send CMD.
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 * \param prCmdInfo      Pointer of P_CMD_INFO_T
 *
 * \retval WLAN_STATUS_SUCCESS   : CMD was written to HIF and be freed(CMD Done)
 *				   immediately.
 * \retval WLAN_STATUS_RESOURCE  : No resource for current command, need to wait
 *				   for previous
 *                                 frame finishing their transmission.
 * \retval WLAN_STATUS_FAILURE   : Get failure while access HIF or been
 *				   rejected.
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanSendCommand(struct ADAPTER *prAdapter,
			 struct CMD_INFO *prCmdInfo)
{
	struct TX_CTRL *prTxCtrl;
	uint8_t ucTC;		/* "Traffic Class" SW(Driver) resource
				 * classification
				 */
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	prTxCtrl = &prAdapter->rTxCtrl;

	while (1) {
		/* <0> card removal check */
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
		    || fgIsBusAccessFailed == TRUE) {
			rStatus = WLAN_STATUS_FAILURE;
			break;
		}

		/* <1.1> Assign Traffic Class(TC) */
		ucTC = nicTxGetCmdResourceType(prCmdInfo);

		/* <1.2> Check if pending packet or resource was exhausted */
		rStatus = nicTxAcquireResource(prAdapter, ucTC,
			       nicTxGetCmdPageCount(prAdapter, prCmdInfo),
			       TRUE);
		if (rStatus == WLAN_STATUS_RESOURCES) {
			if (nicTxPollingResource(prAdapter, ucTC) !=
			    WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, ERROR,
				       "Fail to get TX resource return within timeout\n");
				rStatus = WLAN_STATUS_FAILURE;
				prAdapter->fgIsChipNoAck = TRUE;
				break;
			}
			continue;
		}

		GLUE_INC_REF_CNT(prAdapter->rHifStats.u4CmdInCount);
		/* <1.3> Forward CMD_INFO_T to NIC Layer */
		rStatus = nicTxCmd(prAdapter, prCmdInfo, ucTC);

		/* <1.4> Set Pending in response to Query Command/Need Response
		 */
		if (rStatus == WLAN_STATUS_SUCCESS) {
			if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp))
				rStatus = WLAN_STATUS_PENDING;
		}

		break;
	}
	return rStatus;
}				/* end of wlanSendCommand() */

#if CFG_SUPPORT_MULTITHREAD

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will take CMD_INFO_T which carry some information of
 *        incoming OID and notify the NIC_TX to send CMD.
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 * \param prCmdInfo      Pointer of P_CMD_INFO_T
 *
 * \retval WLAN_STATUS_SUCCESS   : CMD was written to HIF and be freed(CMD Done)
 *				   immediately.
 * \retval WLAN_STATUS_RESOURCE  : No resource for current command, need to wait
 *				   for previous
 *                                 frame finishing their transmission.
 * \retval WLAN_STATUS_FAILURE   : Get failure while access HIF or been
 *				   rejected.
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanSendCommandMthread(struct ADAPTER
				*prAdapter, struct CMD_INFO *prCmdInfo)
{
	struct TX_CTRL *prTxCtrl;
	uint8_t ucTC;		/* "Traffic Class" SW(Driver) resource
				 * classification
				 */
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue;

#if CFG_DBG_MGT_BUF
	struct MEM_TRACK *prMemTrack = NULL;
#endif

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	prTxCtrl = &prAdapter->rTxCtrl;

#if CFG_DBG_MGT_BUF
	if (prCmdInfo->pucInfoBuffer &&
			!IS_FROM_BUF(prAdapter, prCmdInfo->pucInfoBuffer))
		prMemTrack =
			(struct MEM_TRACK *)
				((uint8_t *)prCmdInfo->pucInfoBuffer -
					sizeof(struct MEM_TRACK));
#endif

	prTempCmdQue = &rTempCmdQue;
	QUEUE_INITIALIZE(prTempCmdQue);

	do {
		/* <0> card removal check */
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
		    || fgIsBusAccessFailed == TRUE) {
			rStatus = WLAN_STATUS_FAILURE;
#if CFG_DBG_MGT_BUF
			if (prMemTrack) {
				prMemTrack->u2CmdIdAndWhere &= 0x00FF;
				/* 0x14 means the CmdId can't enqueue to
				 *  TxCmdQueue due to card removal
				 */
				prMemTrack->u2CmdIdAndWhere |= 0x1400;
			}
#endif
			break;
		}
		/* <1> Normal case of sending CMD Packet */
		/* <1.1> Assign Traffic Class(TC) */
		ucTC = nicTxGetCmdResourceType(prCmdInfo);

		/* <1.2> Check if pending packet or resource was exhausted */
		rStatus = nicTxAcquireResource(prAdapter, ucTC,
			       nicTxGetCmdPageCount(prAdapter, prCmdInfo),
			       TRUE);
		if (rStatus == WLAN_STATUS_RESOURCES) {
#if 0
			DBGLOG(INIT, WARN,
			       "%s: NO Resource for CMD TYPE[%u] ID[0x%02X] SEQ[%u] TC[%u]\n",
			       __func__, prCmdInfo->eCmdType, prCmdInfo->ucCID,
			       prCmdInfo->ucCmdSeqNum, ucTC);
#endif
#if CFG_DBG_MGT_BUF
			if (prMemTrack) {
				prMemTrack->u2CmdIdAndWhere &= 0x00FF;
				/* 0x15 means the CmdId can't enqueue
				 *  to TxCmdQueue due to out of resource
				 */
				prMemTrack->u2CmdIdAndWhere |= 0x1500;
			}
#endif

			break;
		}

		/* Process to pending command queue firest */
		if ((!prCmdInfo->fgSetQuery) || (prCmdInfo->fgNeedResp)) {
			/* command packet which needs further handling upon
			 * response
			 */
		}

#if CFG_DBG_MGT_BUF
		if (prMemTrack) {
			prMemTrack->u2CmdIdAndWhere &= 0x00FF;
			/* 0x20 means the CmdId is in TxCmdQueue
			 *  and is waiting for main_thread handling
			 */
			prMemTrack->u2CmdIdAndWhere |= 0x2000;
		}
#endif

		QUEUE_INSERT_TAIL(prTempCmdQue, prCmdInfo);

		/* <1.4> Set Pending in response to Query Command/Need Response
		 */
		if (rStatus == WLAN_STATUS_SUCCESS) {
			if (!prCmdInfo->fgSetQuery ||
			    prCmdInfo->fgNeedResp ||
			    prCmdInfo->eCmdType == COMMAND_TYPE_DATA_FRAME) {
				rStatus = WLAN_STATUS_PENDING;
			}
		}
	} while (FALSE);

	GLUE_ADD_REF_CNT(prTempCmdQue->u4NumElem,
			prAdapter->rHifStats.u4CmdInCount);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
	QUEUE_CONCATENATE_QUEUES(&(prAdapter->rTxCmdQueue),
				 prTempCmdQue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

	return rStatus;
}				/* end of wlanSendCommandMthread() */

void wlanTxCmdDoneCb(struct ADAPTER *prAdapter,
		     struct CMD_INFO *prCmdInfo)
{
	struct MSDU_INFO *prMsduInfo = prCmdInfo->prMsduInfo;
	struct TX_CTRL *prTxCtrl = &prAdapter->rTxCtrl;
#if CFG_DBG_MGT_BUF
	struct MEM_TRACK *prMemTrack = NULL;
#endif

	KAL_SPIN_LOCK_DECLARATION();

	KAL_ACQUIRE_PENDING_CMD_LOCK(prAdapter);
	if (!prCmdInfo->fgSetQuery || prCmdInfo->fgNeedResp) {
		removeDuplicatePendingCmd(prAdapter, prCmdInfo);

		DBGLOG(TX, TRACE, "Add command: %p, %ps, cmd=0x%02X, seq=%u",
			prCmdInfo, prCmdInfo->pfCmdDoneHandler,
			prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum);

#if CFG_DBG_MGT_BUF
		if (prCmdInfo->pucInfoBuffer &&
				!IS_FROM_BUF(prAdapter,
					prCmdInfo->pucInfoBuffer))
			prMemTrack =
				(struct MEM_TRACK *)
					((uint8_t *)prCmdInfo->pucInfoBuffer -
						sizeof(struct MEM_TRACK));

		if (prMemTrack) {
			prMemTrack->u2CmdIdAndWhere &= 0x00FF;
			/* 0x50 means the CmdId is sent to
			 * WFDMA by HIF
			 */
			prMemTrack->u2CmdIdAndWhere |= 0x5000;
		}
#endif
		QUEUE_INSERT_TAIL(&prAdapter->rPendingCmdQueue, prCmdInfo);
	}
	KAL_RELEASE_PENDING_CMD_LOCK(prAdapter);

	if (prMsduInfo && prMsduInfo->pfTxDoneHandler) {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter,
			SPIN_LOCK_TXING_MGMT_LIST);
		QUEUE_INSERT_TAIL(&(prTxCtrl->rTxMgmtTxingQueue),
				prMsduInfo);
		KAL_RELEASE_SPIN_LOCK(prAdapter,
			SPIN_LOCK_TXING_MGMT_LIST);
		DBGLOG(TX, INFO, "Insert msdu WIDX:PID[%u:%u]\n",
			prMsduInfo->ucWlanIndex, prMsduInfo->ucPID);
	}
}

uint32_t wlanTxCmdMthread(struct ADAPTER *prAdapter)
{
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue;
	struct QUE rTempCmdDoneQue;
	struct QUE *prTempCmdDoneQue;
	struct QUE_ENTRY *prQueueEntry = NULL;
	struct CMD_INFO *prCmdInfo;
	/* P_CMD_ACCESS_REG prCmdAccessReg;
	 * P_CMD_ACCESS_REG prEventAccessReg;
	 * UINT_32 u4Address;
	 */
	uint32_t u4TxDoneQueueSize, u4Ret;
#if CFG_DBG_MGT_BUF
	struct MEM_TRACK *prMemTrack = NULL;
#endif

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	if (halIsHifStateSuspend(prAdapter)) {
		DBGLOG(TX, WARN, "Suspend TxCmdMthread\n");
		return WLAN_STATUS_SUCCESS;
	}

	prTempCmdQue = &rTempCmdQue;
	QUEUE_INITIALIZE(prTempCmdQue);

	prTempCmdDoneQue = &rTempCmdDoneQue;
	QUEUE_INITIALIZE(prTempCmdDoneQue);

	KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);

	/* TX Command Queue */
	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;
		prCmdInfo->pfHifTxCmdDoneCb = wlanTxCmdDoneCb;
#if CFG_DBG_MGT_BUF
		prMemTrack = NULL;
		if (prCmdInfo->pucInfoBuffer &&
				!IS_FROM_BUF(prAdapter,
					prCmdInfo->pucInfoBuffer))
			prMemTrack =
				(struct MEM_TRACK *)
					((uint8_t *)prCmdInfo->pucInfoBuffer -
						sizeof(struct MEM_TRACK));
		if (prMemTrack) {
			if (!prCmdInfo->fgSetQuery || prCmdInfo->fgNeedResp) {
				prMemTrack->u2CmdIdAndWhere &= 0x00FF;
				/* 0x30 means the CmdId needs to send to
				 * FW via HIF
				 */
				prMemTrack->u2CmdIdAndWhere |= 0x3000;
			} else {
				prMemTrack->u2CmdIdAndWhere &= 0x00FF;
				/* 0x40 means the CmdId enqueues to
				 * TxCmdDone queue
				 */
				prMemTrack->u2CmdIdAndWhere |= 0x4000;
			}
		}
#endif

		u4Ret = nicTxCmd(prAdapter, prCmdInfo, TC4_INDEX);
		if (u4Ret != WLAN_STATUS_SUCCESS)
			DBGLOG(INIT, WARN, "nicTxCmd returns error[0x%08x]\n",
			       u4Ret);

		if (u4Ret == WLAN_STATUS_RESOURCES) {
			QUEUE_INSERT_HEAD(prTempCmdQue, prQueueEntry);
			break;
		}

		if (prCmdInfo->fgSetQuery && !prCmdInfo->fgNeedResp)
			QUEUE_INSERT_TAIL(prTempCmdDoneQue, prQueueEntry);

		/* DBGLOG(INIT, INFO, "==> TX CMD QID: %d (Q:%d)\n",
		 *        prCmdInfo->ucCID, prTempCmdQue->u4NumElem));
		 */

		GLUE_DEC_REF_CNT(prAdapter->prGlueInfo->i4TxPendingCmdNum);
		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}

	if (!QUEUE_IS_EMPTY(prTempCmdQue)) {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
		QUEUE_CONCATENATE_QUEUES(prTempCmdQue, &prAdapter->rTxCmdQueue);
		QUEUE_CONCATENATE_QUEUES(&prAdapter->rTxCmdQueue, prTempCmdQue);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
	}

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
	QUEUE_CONCATENATE_QUEUES(&prAdapter->rTxCmdDoneQueue,
				 prTempCmdDoneQue);
	u4TxDoneQueueSize = prAdapter->rTxCmdDoneQueue.u4NumElem;
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);

	KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);

	/* call tx thread to work */
	if (u4TxDoneQueueSize > 0)
		kalSetTxCmdDoneEvent(prAdapter->prGlueInfo);

	return WLAN_STATUS_SUCCESS;
}

uint32_t wlanTxCmdDoneMthread(struct ADAPTER *prAdapter)
{
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue;
	struct QUE_ENTRY *prQueueEntry = NULL;
	struct CMD_INFO *prCmdInfo;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);

	if (halIsHifStateSuspend(prAdapter)) {
		DBGLOG(TX, WARN, "Suspend TxCmdDoneMthread\n");
		return WLAN_STATUS_SUCCESS;
	}

	prTempCmdQue = &rTempCmdQue;
	QUEUE_INITIALIZE(prTempCmdQue);

	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdDoneQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
						    prCmdInfo->pucInfoBuffer);
		/* Not pending cmd, free it after TX succeed! */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to clear all commands in TX command queue
 * \param prAdapter  Pointer of Adapter Data Structure
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void wlanClearTxCommandQueue(struct ADAPTER *prAdapter)
{
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempCmdQue);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
	QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->pfCmdTimeoutHandler)
			prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
		else
			wlanReleaseCommand(prAdapter, prCmdInfo,
					   TX_RESULT_QUEUE_CLEARANCE);

		/* Release Tx resource for CMD which resource is allocated but
		 * not used
		 */
		nicTxReleaseResource_PSE(prAdapter,
			nicTxGetCmdResourceType(prCmdInfo),
			nicTxGetCmdPageCount(prAdapter, prCmdInfo), TRUE);

		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to clear OID commands in TX command queue
 * \param prAdapter  Pointer of Adapter Data Structure
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void wlanClearTxOidCommand(struct ADAPTER *prAdapter)
{
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempCmdQue);

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);

	QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rTxCmdQueue);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);

	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->fgIsOid) {
			DBGLOG(OID, INFO,
				"Clear pending OID CMD ID[0x%02X] SEQ[%u]\n",
				prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum);
			if (prCmdInfo->pfCmdTimeoutHandler)
				prCmdInfo->pfCmdTimeoutHandler(prAdapter,
							       prCmdInfo);
			else
				wlanReleaseCommand(prAdapter, prCmdInfo,
						   TX_RESULT_QUEUE_CLEARANCE);

			/* Release Tx resource for CMD which resource is
			 * allocated but not used
			 */
			nicTxReleaseResource_PSE(prAdapter,
				nicTxGetCmdResourceType(prCmdInfo),
				nicTxGetCmdPageCount(prAdapter, prCmdInfo),
				TRUE);

			cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		} else {
			QUEUE_INSERT_TAIL(&prAdapter->rTxCmdQueue,
					prQueueEntry);
		}

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}

	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_QUE);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to clear all commands in TX command done queue
 * \param prAdapter  Pointer of Adapter Data Structure
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void wlanClearTxCommandDoneQueue(struct ADAPTER
				 *prAdapter)
{
	struct QUE rTempCmdDoneQue;
	struct QUE *prTempCmdDoneQue = &rTempCmdDoneQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempCmdDoneQue);

	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);
	QUEUE_MOVE_ALL(prTempCmdDoneQue,
		       &prAdapter->rTxCmdDoneQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_CMD_DONE_QUE);

	/* 4 <2> Dequeue from head and check it is able to be sent */
	QUEUE_REMOVE_HEAD(prTempCmdDoneQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->pfCmdDoneHandler)
			prCmdInfo->pfCmdDoneHandler(prAdapter, prCmdInfo,
						    prCmdInfo->pucInfoBuffer);
		/* Not pending cmd, free it after TX succeed! */
		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		QUEUE_REMOVE_HEAD(prTempCmdDoneQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to clear all buffer in port 0/1 queue
 * \param prAdapter  Pointer of Adapter Data Structure
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void wlanClearDataQueue(struct ADAPTER *prAdapter)
{
	if (HAL_IS_TX_DIRECT(prAdapter))
		nicTxDirectClearHifQ(prAdapter);
	else {
#if CFG_FIX_2_TX_PORT
		struct QUE qDataPort0, qDataPort1;
		struct QUE *prDataPort0, *prDataPort1;
		struct MSDU_INFO *prMsduInfo;

		KAL_SPIN_LOCK_DECLARATION();

		prDataPort0 = &qDataPort0;
		prDataPort1 = &qDataPort1;

		QUEUE_INITIALIZE(prDataPort0);
		QUEUE_INITIALIZE(prDataPort1);

		/* <1> Move whole list of CMD_INFO to temp queue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);
		QUEUE_MOVE_ALL(prDataPort0, &prAdapter->rTxP0Queue);
		QUEUE_MOVE_ALL(prDataPort1, &prAdapter->rTxP1Queue);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);

		/* <2> Release Tx resource */
		nicTxReleaseMsduResource(prAdapter,
				QUEUE_GET_HEAD(prDataPort0));
		nicTxReleaseMsduResource(prAdapter,
				QUEUE_GET_HEAD(prDataPort1));

		/* <3> Return sk buffer */
		nicTxReturnMsduInfo(prAdapter, QUEUE_GET_HEAD(prDataPort0));
		nicTxReturnMsduInfo(prAdapter, QUEUE_GET_HEAD(prDataPort1));

		/* <4> Clear pending MSDU info in data done queue */
		KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
		while (QUEUE_IS_NOT_EMPTY(&prAdapter->rTxDataDoneQueue)) {
			QUEUE_REMOVE_HEAD(&prAdapter->rTxDataDoneQueue,
					  prMsduInfo, struct MSDU_INFO *);
			if (prMsduInfo == NULL) {
				DBGLOG(TX, WARN, "prMsduInfo is NULL\n");
				break;
			}
			nicTxFreePacket(prAdapter, prMsduInfo, FALSE);
			nicTxReturnMsduInfo(prAdapter, prMsduInfo);
		}
		KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
#else

		struct QUE qDataPort[MAX_BSSID_NUM][TC_NUM];
		struct QUE *prDataPort[MAX_BSSID_NUM][TC_NUM];
		struct MSDU_INFO *prMsduInfo = NULL;
		int32_t i, j;

		KAL_SPIN_LOCK_DECLARATION();
#if (CFG_TX_MGMT_BY_DATA_Q == 1)
		nicTxClearMgmtDirectTxQ(prAdapter);
#endif /* CFG_TX_MGMT_BY_DATA_Q == 1 */

		for (i = 0; i < MAX_BSSID_NUM; i++) {
			for (j = 0; j < TC_NUM; j++) {
				prDataPort[i][j] = &qDataPort[i][j];
				QUEUE_INITIALIZE(prDataPort[i][j]);
			}
		}

		/* <1> Move whole list of CMD_INFO to temp queue */
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);
		for (i = 0; i < MAX_BSSID_NUM; i++) {
			for (j = 0; j < TC_NUM; j++) {
				QUEUE_MOVE_ALL(prDataPort[i][j],
					&prAdapter->rTxPQueue[i][j]);
				kalTraceEvent("Move TxPQueue%d_%d %d", i, j,
					prDataPort[i][j]->u4NumElem);
			}
		}
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_TX_PORT_QUE);

#if (CFG_TX_HIF_PORT_QUEUE == 1)
		for (i = 0; i < MAX_BSSID_NUM; i++) {
			for (j = 0; j < TC_NUM; j++) {
				QUEUE_CONCATENATE_QUEUES_HEAD(prDataPort[i][j],
					&prAdapter->rTxHifPQueue[i][j]);
				kalTraceEvent("Move TxHifPQueue%d_%d %d", i, j,
					prDataPort[i][j]->u4NumElem);
			}
		}
#endif

		/* <2> Return sk buffer */
		for (i = 0; i < MAX_BSSID_NUM; i++) {
			for (j = 0; j < TC_NUM; j++) {
				if (!QUEUE_GET_HEAD(prDataPort[i][j]))
					continue;
				nicTxReleaseMsduResource(prAdapter,
					QUEUE_GET_HEAD(prDataPort[i][j]));
				nicTxFreeMsduInfoPacket(prAdapter,
					QUEUE_GET_HEAD(prDataPort[i][j]));
				nicTxReturnMsduInfo(prAdapter,
					QUEUE_GET_HEAD(prDataPort[i][j]));
			}
		}

		/* <3> Clear pending MSDU info in data done queue */
		KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
		while (QUEUE_IS_NOT_EMPTY(&prAdapter->rTxDataDoneQueue)) {
			QUEUE_REMOVE_HEAD(&prAdapter->rTxDataDoneQueue,
					  prMsduInfo, struct MSDU_INFO *);

			nicTxFreePacket(prAdapter, prMsduInfo, FALSE);
			nicTxReturnMsduInfo(prAdapter, prMsduInfo);
		}
		KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_DATA_DONE_QUE);
#endif
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to clear all buffer in port 0/1 queue
 * \param prAdapter  Pointer of Adapter Data Structure
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void wlanClearRxToOsQueue(struct ADAPTER *prAdapter)
{
	struct QUE rTempRxQue;
	struct QUE *prTempRxQue = &rTempRxQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempRxQue);

	/* 4 <1> Move whole list of CMD_INFO to temp queue */
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);
	QUEUE_MOVE_ALL(prTempRxQue, &prAdapter->rRxQueue);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_TO_OS_QUE);

	/* 4 <2> Remove all skbuf */
	QUEUE_REMOVE_HEAD(prTempRxQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		kalPacketFree(prAdapter->prGlueInfo,
				(void *) GLUE_GET_PKT_DESCRIPTOR(prQueueEntry));
		QUEUE_REMOVE_HEAD(prTempRxQue, prQueueEntry,
				struct QUE_ENTRY *);
	}

}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is used to clear all commands in pending command queue
 * \param prAdapter  Pointer of Adapter Data Structure
 *
 * \retval none
 */
/*----------------------------------------------------------------------------*/
void wlanClearPendingCommandQueue(struct ADAPTER *prAdapter)
{
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	KAL_SPIN_LOCK_DECLARATION();
	QUEUE_INITIALIZE(prTempCmdQue);

	ASSERT(prAdapter);

	KAL_ACQUIRE_PENDING_CMD_LOCK(prAdapter);
	QUEUE_MOVE_ALL(prTempCmdQue, &prAdapter->rPendingCmdQueue);
	KAL_RELEASE_PENDING_CMD_LOCK(prAdapter);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
			  struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;

		if (prCmdInfo->pfCmdTimeoutHandler)
			prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
		else
			wlanReleaseCommand(prAdapter, prCmdInfo,
					   TX_RESULT_QUEUE_CLEARANCE);

		nicTxCancelSendingCmd(prAdapter, prCmdInfo);

		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}
}

void wlanReleaseCommand(struct ADAPTER *prAdapter,
			struct CMD_INFO *prCmdInfo,
			enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	wlanReleaseCommandEx(prAdapter, prCmdInfo, rTxDoneStatus, TRUE);
}


/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will release thd CMD_INFO upon its attribution
 *
 * \param prAdapter  Pointer of Adapter Data Structure
 * \param prCmdInfo  Pointer of CMD_INFO_T
 * \param rTxDoneStatus  Tx done status
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanReleaseCommandEx(struct ADAPTER *prAdapter,
			struct CMD_INFO *prCmdInfo,
			enum ENUM_TX_RESULT_CODE rTxDoneStatus,
			u_int8_t fgIsNeedHandler)
{
	struct TX_CTRL *prTxCtrl;
	struct MSDU_INFO *prMsduInfo;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prTxCtrl = &prAdapter->rTxCtrl;

	switch (prCmdInfo->eCmdType) {
	case COMMAND_TYPE_NETWORK_IOCTL:
		DBGLOG(INIT, INFO,
		       "Free CMD: ID[0x%x] SeqNum[%u] OID[%u]\n",
		       prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum,
		       prCmdInfo->fgIsOid);

		if (prCmdInfo->fgIsOid) {
			kalOidComplete(prAdapter->prGlueInfo,
				       prCmdInfo,
				       prCmdInfo->u4SetInfoLen,
				       WLAN_STATUS_FAILURE);
		}
		break;

	case COMMAND_TYPE_MANAGEMENT_FRAME:
	case COMMAND_TYPE_DATA_FRAME:
		prMsduInfo = prCmdInfo->prMsduInfo;

		if (prCmdInfo->eCmdType == COMMAND_TYPE_DATA_FRAME) {
			kalCmdDataFrameSendComplete(prAdapter->prGlueInfo,
						     prCmdInfo->prPacket,
						     WLAN_STATUS_FAILURE);
			/* Avoid skb multiple free */
			prMsduInfo->prPacket = NULL;
		}

		DBGLOG(INIT, INFO,
			"Free %s Frame: B[%u]W:P[%u:%u]TS[%u]STA[%u]RSP[%u]CS[%u]\n",
		    prCmdInfo->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME ?
				"MGMT" : "DATA",
		    prMsduInfo->ucBssIndex,
		    prMsduInfo->ucWlanIndex,
		    prMsduInfo->ucPID,
		    prMsduInfo->ucTxSeqNum,
		    prMsduInfo->ucStaRecIndex,
		    prMsduInfo->pfTxDoneHandler ? TRUE : FALSE,
		    prCmdInfo->ucCmdSeqNum);

		/* invoke callbacks */
		if (fgIsNeedHandler) {
			if (prMsduInfo->pfTxDoneHandler != NULL)
				prMsduInfo->pfTxDoneHandler(prAdapter,
					prMsduInfo, rTxDoneStatus);
		} else {
			nicDumpMsduInfo(prMsduInfo);
		}

		if (prCmdInfo->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME)
			GLUE_DEC_REF_CNT(prTxCtrl->i4TxMgmtPendingNum);

		cnmMgtPktFree(prAdapter, prMsduInfo);
		break;

	default:
		ASSERT(0);
		break;
	}

}				/* end of wlanReleaseCommand() */

uint32_t wlanGetThreadWakeUp(struct ADAPTER *prAdapter)
{
	return prAdapter->rWifiVar.u4WakeLockThreadWakeup;
}

uint32_t wlanGetTxdAppendSize(struct ADAPTER *prAdapter)
{
	return prAdapter->chip_info->txd_append_size;
}


/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will search the CMD Queue to look for the pending OID
 *	  and compelete it immediately when system request a reset.
 *
 * \param prAdapter  ointer of Adapter Data Structure
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanReleasePendingOid(struct ADAPTER *prAdapter,
			   uintptr_t ulParamPtr)
{
	struct QUE *prCmdQue;
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;
#if (CFG_SUPPORT_DEBUG_SOP == 1)
	struct CHIP_DBG_OPS *prChipDbg = (struct CHIP_DBG_OPS *) NULL;
#endif

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("wlanReleasePendingOid");

	if (prAdapter == NULL) {
		DBGLOG(INIT, WARN, "prAdapter is NULL!\n");

		return;
	}

	do {
		if (ulParamPtr == 1)
			break;

		prAdapter->ucOidTimeoutCount++;
		if (prAdapter->ucOidTimeoutCount >=
		    WLAN_OID_NO_ACK_THRESHOLD) {
			if (!prAdapter->fgIsChipNoAck) {
				DBGLOG(INIT, WARN,
				       "No response from chip for %u times, set NoAck flag!\n",
				       prAdapter->ucOidTimeoutCount);
#if (CFG_SUPPORT_DEBUG_SOP == 1)
				prChipDbg = prAdapter->chip_info->prDebugOps;
				prChipDbg->show_debug_sop_info(prAdapter,
				  SLAVENORESP);
#endif
			}

			prAdapter->u4HifDbgFlag |= DEG_HIF_ALL;
			kalSetHifDbgEvent(prAdapter->prGlueInfo);
		}
	} while (FALSE);

	do {
#if CFG_SUPPORT_MULTITHREAD
		KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);
#endif
		/* 1: Clear pending OID in glue layer command queue */
		kalOidCmdClearance(prAdapter->prGlueInfo);

#if CFG_SUPPORT_MULTITHREAD
		/* Clear pending OID in main_thread to hif_thread command queue
		 */
		wlanClearTxOidCommand(prAdapter);
#endif

		/* 2: Clear Pending OID in prAdapter->rPendingCmdQueue */
		KAL_ACQUIRE_PENDING_CMD_LOCK(prAdapter);

		prCmdQue = &prAdapter->rPendingCmdQueue;
		QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
		while (prQueueEntry) {
			prCmdInfo = (struct CMD_INFO *) prQueueEntry;

			if (prCmdInfo->fgIsOid) {
				DBGLOG(OID, INFO,
				       "Clear pending OID CMD ID[0x%02X] SEQ[%u] buf[0x%p]\n",
				       prCmdInfo->ucCID, prCmdInfo->ucCmdSeqNum,
				       prCmdInfo->pucInfoBuffer);

				if (prCmdInfo->pfCmdTimeoutHandler) {
					prCmdInfo->pfCmdTimeoutHandler(
							prAdapter, prCmdInfo);
				} else {
					kalOidComplete(prAdapter->prGlueInfo,
						       prCmdInfo,
						       0, WLAN_STATUS_FAILURE);
				}

				KAL_RELEASE_PENDING_CMD_LOCK(prAdapter);
				nicTxCancelSendingCmd(prAdapter, prCmdInfo);
				KAL_ACQUIRE_PENDING_CMD_LOCK(prAdapter);

				cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
			} else {
				QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
			}

			QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
					  struct QUE_ENTRY *);
		}

		KAL_RELEASE_PENDING_CMD_LOCK(prAdapter);

		/* 3: Clear pending OID queued in pvOidEntry with REQ_FLAG_OID
		 *    set
		 */
		kalOidClearance(prAdapter->prGlueInfo);

#if CFG_SUPPORT_MULTITHREAD
		KAL_RELEASE_MUTEX(prAdapter, MUTEX_TX_CMD_CLEAR);
#endif
	} while (FALSE);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will search the CMD Queue to look for the pending
 *	  CMD/OID for specific
 *        NETWORK TYPE and compelete it immediately when system request a reset.
 *
 * \param prAdapter  ointer of Adapter Data Structure
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanReleasePendingCMDbyBssIdx(struct ADAPTER
				   *prAdapter, uint8_t ucBssIndex)
{

}				/* wlanReleasePendingCMDbyBssIdx */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Return the indicated packet buffer and reallocate one to the RFB
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \retval WLAN_STATUS_SUCCESS: Success
 * \retval WLAN_STATUS_FAILURE: Failed
 */
/*----------------------------------------------------------------------------*/
void wlanReturnPacketDelaySetup(struct ADAPTER *prAdapter)
{
	struct RX_CTRL *prRxCtrl;
	struct SW_RFB *prSwRfb = NULL;

	KAL_SPIN_LOCK_DECLARATION();
	uint32_t status = WLAN_STATUS_SUCCESS;

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	DBGLOG(RX, LOUD, "IndicatedRfbList num = %u\n",
	       RX_GET_INDICATED_RFB_CNT(prRxCtrl));

	while (QUEUE_IS_NOT_EMPTY(&prRxCtrl->rIndicatedRfbList)) {
		KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
		QUEUE_REMOVE_HEAD(&prRxCtrl->rIndicatedRfbList, prSwRfb,
				  struct SW_RFB *);
		KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);

		if (prSwRfb) {
#if CFG_RFB_TRACK
			RX_RFB_TRACK_UPDATE(prAdapter, prSwRfb,
				RFB_TRACK_PACKET_SETUP);
#endif /* CFG_RFB_TRACK */
			status = nicRxSetupRFB(prAdapter, prSwRfb);
			nicRxReturnRFB(prAdapter, prSwRfb);
		} else {
			log_dbg(RX, WARN, "prSwRfb == NULL\n");
			break;
		}

		if (status != WLAN_STATUS_SUCCESS)
			break;
	}

	if (status != WLAN_STATUS_SUCCESS) {
		DBGLOG(RX, WARN,
			"Restart ReturnIndicatedRfb Timer (%ums) I,F:%u,%u\n",
			RX_RETURN_INDICATED_RFB_TIMEOUT_MSEC,
			RX_GET_INDICATED_RFB_CNT(prRxCtrl),
			RX_GET_FREE_RFB_CNT(prRxCtrl));
		/* restart timer */
		cnmTimerStartTimer(prAdapter,
			&prAdapter->rPacketDelaySetupTimer,
			RX_RETURN_INDICATED_RFB_TIMEOUT_MSEC);
	}
}

#if (CFG_SUPPORT_RETURN_TASK == 1)
void wlanReturnPacketDelaySetupTasklet(uintptr_t data)
{
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *)data;

	wlanReturnPacketDelaySetup(prGlueInfo->prAdapter);
}
#endif

void wlanReturnPacketDelaySetupTimeout(struct ADAPTER
				       *prAdapter, uintptr_t ulParamPtr)
{
#if (CFG_SUPPORT_RETURN_TASK == 1)
	kal_tasklet_hi_schedule(&prAdapter->prGlueInfo->rRxRfbRetTask);
#elif CFG_SUPPORT_RETURN_WORK
	kalRxRfbReturnWorkSchedule(prAdapter->prGlueInfo);
#else
	wlanReturnPacketDelaySetup(prAdapter);
#endif /* CFG_SUPPORT_RETURN_TASK */
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Return the packet buffer and reallocate one to the RFB
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 * \param pvPacket       Pointer of returned packet
 *
 * \retval WLAN_STATUS_SUCCESS: Success
 * \retval WLAN_STATUS_FAILURE: Failed
 */
/*----------------------------------------------------------------------------*/
void wlanReturnPacket(struct ADAPTER *prAdapter,
		      void *pvPacket)
{
	struct RX_CTRL *prRxCtrl;
	struct SW_RFB *prSwRfb = NULL;

	KAL_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("wlanReturnPacket");

	ASSERT(prAdapter);

	prRxCtrl = &prAdapter->rRxCtrl;
	ASSERT(prRxCtrl);

	if (pvPacket) {
		kalPacketFree(prAdapter->prGlueInfo, pvPacket);
		RX_ADD_CNT(prRxCtrl, RX_DATA_RETURNED_COUNT, 1);
#if CFG_NATIVE_802_11
		if (GLUE_TEST_FLAG(prAdapter->prGlueInfo, GLUE_FLAG_HALT)) {
			/*Todo:: nothing */
			/*Todo:: nothing */
		}
#endif
	}

	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	QUEUE_REMOVE_HEAD(&prRxCtrl->rIndicatedRfbList, prSwRfb,
			  struct SW_RFB *);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_RX_FREE_QUE);
	if (!prSwRfb) {
		DBGLOG(RX, WARN, "No free SwRfb!\n");
		return;
	}

	if (nicRxSetupRFB(prAdapter, prSwRfb)) {
		DBGLOG(RX, WARN,
		       "Cannot allocate packet buffer for SwRfb!\n");
		if (!timerPendingTimer(
		    &prAdapter->rPacketDelaySetupTimer)) {
			DBGLOG(RX, WARN,
			       "Start ReturnIndicatedRfb Timer (%ums) I,F:%u,%u\n",
			       RX_RETURN_INDICATED_RFB_TIMEOUT_MSEC,
			       RX_GET_INDICATED_RFB_CNT(prRxCtrl),
			       RX_GET_FREE_RFB_CNT(prRxCtrl));
			cnmTimerStartTimer(prAdapter,
			    &prAdapter->rPacketDelaySetupTimer,
			    RX_RETURN_INDICATED_RFB_TIMEOUT_MSEC);
		}
	}
	nicRxReturnRFB(prAdapter, prSwRfb);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is a required function that returns information about
 *        the capabilities and status of the driver and/or its network adapter.
 *
 * \param[IN] prAdapter		Pointer to the Adapter structure.
 * \param[IN] pfnOidQryHandler	Function pointer for the OID query handler.
 * \param[IN] pvInfoBuf		Points to a buffer for return the query
 *				information.
 * \param[IN] u4QueryBufferLen Specifies the number of bytes at pvInfoBuf.
 * \param[OUT] pu4QueryInfoLen	Points to the number of bytes it written or is
 *				needed.
 *
 * \retval WLAN_STATUS_xxx Different WLAN_STATUS code returned by different
 *				handlers.
 *
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanQueryInformation(struct ADAPTER *prAdapter,
		     PFN_OID_HANDLER_FUNC pfnOidQryHandler,
		     void *pvInfoBuf, uint32_t u4InfoBufLen,
		     uint32_t *pu4QryInfoLen)
{
	uint32_t status = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4QryInfoLen);

	/* ignore any OID request after connected, under PS current measurement
	 * mode
	 */
	DBGLOG(NIC, TRACE, "u4PsCurrentMeasureEn=%u, aisGetConnectedBssInfo=%p",
		prAdapter->u4PsCurrentMeasureEn,
		aisGetConnectedBssInfo(prAdapter));
	if (prAdapter->u4PsCurrentMeasureEn &&
	    aisGetConnectedBssInfo(prAdapter)) {
		/* note: return WLAN_STATUS_FAILURE or
		 * WLAN_STATUS_SUCCESS for blocking OIDs during current
		 * measurement ??
		 */
		return WLAN_STATUS_SUCCESS;
	}
	/* most OID handler will just queue a command packet
	 * for power state transition OIDs, handler will acquire power control
	 * by itself
	 */
	status = pfnOidQryHandler(prAdapter, pvInfoBuf,
				  u4InfoBufLen, pu4QryInfoLen);
	DBGLOG(NIC, TRACE, "%ps returns %u", pfnOidQryHandler, status);

	return status;

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is a required function that allows bound protocol
 *	  drivers, or NDIS, to request changes in the state information that
 *	  the miniport maintains for particular object identifiers, such as
 *	  changes in multicast addresses.
 *
 * \param[IN] prAdapter     Pointer to the Glue info structure.
 * \param[IN] pfnOidSetHandler     Points to the OID set handlers.
 * \param[IN] pvInfoBuf     Points to a buffer containing the OID-specific data
 *			    for the set.
 * \param[IN] u4InfoBufLen  Specifies the number of bytes at prSetBuffer.
 * \param[OUT] pu4SetInfoLen Points to the number of bytes it read or is needed.
 *
 * \retval WLAN_STATUS_xxx Different WLAN_STATUS code returned by different
 *			   handlers.
 *
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanSetInformation(struct ADAPTER *prAdapter,
		   PFN_OID_HANDLER_FUNC pfnOidSetHandler,
		   void *pvInfoBuf, uint32_t u4InfoBufLen,
		   uint32_t *pu4SetInfoLen)
{
	uint32_t status = WLAN_STATUS_FAILURE;

	ASSERT(prAdapter);
	ASSERT(pu4SetInfoLen);

	/* ignore any OID request after connected, under PS current measurement
	 * mode
	 */
	DBGLOG(NIC, TRACE, "u4PsCurrentMeasureEn=%u, aisGetConnectedBssInfo=%p",
		prAdapter->u4PsCurrentMeasureEn,
		aisGetConnectedBssInfo(prAdapter));
	if (prAdapter->u4PsCurrentMeasureEn &&
	    aisGetConnectedBssInfo(prAdapter)) {
		/* note: return WLAN_STATUS_FAILURE or WLAN_STATUS_SUCCESS
		 * for blocking OIDs during current measurement ??
		 */
		return WLAN_STATUS_SUCCESS;
	}
	/* most OID handler will just queue a command packet
	 * for power state transition OIDs, handler will acquire power control
	 * by itself
	 */
	status = pfnOidSetHandler(prAdapter, pvInfoBuf,
				  u4InfoBufLen, pu4SetInfoLen);
	DBGLOG(NIC, TRACE, "%ps returns %u", pfnOidSetHandler, status);

	return status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called to set RX filter to Promiscuous Mode.
 *
 * \param[IN] prAdapter        Pointer to the Adapter structure.
 * \param[IN] fgEnablePromiscuousMode Enable/ disable RX Promiscuous Mode.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanSetPromiscuousMode(struct ADAPTER *prAdapter,
			    u_int8_t fgEnablePromiscuousMode)
{
	ASSERT(prAdapter);

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called to set RX filter to allow to receive
 *        broadcast address packets.
 *
 * \param[IN] prAdapter        Pointer to the Adapter structure.
 * \param[IN] fgEnableBroadcast Enable/ disable broadcast packet to be received.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void wlanRxSetBroadcast(struct ADAPTER *prAdapter,
			u_int8_t fgEnableBroadcast)
{
	ASSERT(prAdapter);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called to send out CMD_ID_DUMMY command packet
 *
 * \param[IN] prAdapter        Pointer to the Adapter structure.
 *
 * \return WLAN_STATUS_SUCCESS
 * \return WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanSendDummyCmd(struct ADAPTER *prAdapter,
			  u_int8_t fgIsReqTxRsrc)
{
#ifdef CFG_SUPPORT_UNIFIED_COMMAND
	return wlanSendSetQueryCmdHelper(
#else
	return wlanSendSetQueryCmdAdv(
#endif
		prAdapter, CMD_ID_DUMMY_RSV, 0, TRUE,
		FALSE, TRUE, NULL, NULL, 0, NULL, NULL, 0,
		fgIsReqTxRsrc ? CMD_SEND_METHOD_REQ_RESOURCE :
		CMD_SEND_METHOD_TX);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called to send out CMD_NIC_POWER_CTRL command packet
 *
 * \param[IN] prAdapter        Pointer to the Adapter structure.
 * \param[IN] ucPowerMode      refer to CMD/EVENT document
 *
 * \return WLAN_STATUS_SUCCESS
 * \return WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanSendNicPowerCtrlCmd(struct ADAPTER
				 *prAdapter, uint8_t ucPowerMode)
{
	uint32_t status = WLAN_STATUS_SUCCESS;
	struct CMD_NIC_POWER_CTRL rNicPwrCtrl = {0};

	ASSERT(prAdapter);

	rNicPwrCtrl.ucPowerMode = ucPowerMode;

#ifdef CFG_SUPPORT_UNIFIED_COMMAND
	status = wlanSendSetQueryCmdHelper(
#else
	status = wlanSendSetQueryCmdAdv(
#endif
		prAdapter, CMD_ID_NIC_POWER_CTRL, 0, TRUE,
		FALSE, TRUE, NULL, NULL,
		sizeof(struct CMD_NIC_POWER_CTRL),
		(uint8_t *)&rNicPwrCtrl, NULL, 0,
		CMD_SEND_METHOD_REQ_RESOURCE);

	/* 5. Add flag */
	if (ucPowerMode == 1)
		prAdapter->fgIsEnterD3ReqIssued = TRUE;

	return status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is called to check if it is RF test mode and
 *        the OID is allowed to be called or not
 *
 * \param[IN] prAdapter        Pointer to the Adapter structure.
 * \param[IN] fgEnableBroadcast Enable/ disable broadcast packet to be received.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanIsHandlerAllowedInRFTest(PFN_OID_HANDLER_FUNC pfnOidHandler,
				      u_int8_t fgSetInfo)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerAllowedInRFTest;
	uint32_t i;
	uint32_t u4NumOfElem;

	if (fgSetInfo) {
		apfnOidHandlerAllowedInRFTest =
			apfnOidSetHandlerAllowedInRFTest;
		u4NumOfElem = sizeof(apfnOidSetHandlerAllowedInRFTest) /
			      sizeof(PFN_OID_HANDLER_FUNC);
	} else {
		apfnOidHandlerAllowedInRFTest =
			apfnOidQueryHandlerAllowedInRFTest;
		u4NumOfElem = sizeof(apfnOidQueryHandlerAllowedInRFTest) /
			      sizeof(PFN_OID_HANDLER_FUNC);
	}

	for (i = 0; i < u4NumOfElem; i++) {
		if (apfnOidHandlerAllowedInRFTest[i] == pfnOidHandler)
			return TRUE;
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to get the chip information
 *
 * @param prAdapter      Pointer to the Adapter structure.
 *
 * @return
 */
/*----------------------------------------------------------------------------*/

uint32_t wlanSetChipEcoInfo(struct ADAPTER *prAdapter)
{
	uint32_t hw_version = 0, sw_version = 0;
	struct mt66xx_chip_info *prChipInfo = prAdapter->chip_info;
	uint32_t chip_id = prChipInfo->chip_id;
	/* WLAN_STATUS status; */
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	DEBUGFUNC("wlanSetChipEcoInfo.\n");

	if (wlanAccessRegister(prAdapter,
		prChipInfo->top_hvr, &hw_version, 0, 0) !=
	    WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "wlanSetChipEcoInfo >> get TOP_HVR failed.\n");
		u4Status = WLAN_STATUS_FAILURE;
	} else if (wlanAccessRegister(prAdapter,
		prChipInfo->top_fvr, &sw_version, 0, 0) !=
	    WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       "wlanSetChipEcoInfo >> get TOP_FVR failed.\n");
		u4Status = WLAN_STATUS_FAILURE;
	} else {
		/* success */
		nicSetChipHwVer((uint8_t)(GET_HW_VER(hw_version) & 0xFF));
		nicSetChipFactoryVer((uint8_t)((GET_HW_VER(hw_version) >> 8) &
				     0xF));
		nicSetChipSwVer((uint8_t)GET_FW_VER(sw_version));

		/* Assign current chip version */
		prAdapter->chip_info->eco_ver = nicGetChipEcoVer(prAdapter);
	}

	DBGLOG(INIT, INFO,
	       "Chip ID[%04X] Version[E%u] HW[0x%08x] SW[0x%08x]\n",
	       chip_id, prAdapter->chip_info->eco_ver, hw_version,
	       sw_version);

	return u4Status;
}

#if (CFG_ENABLE_FW_DOWNLOAD == 1)
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to read/write a certain N9
 *        register by inband command in blocking mode in ROM code stage
 *
 * @param prAdapter      Pointer to the Adapter structure.
 *        u4DestAddr     Address of destination address
 *        u4ImgSecSize   Length of the firmware block
 *        fgReset        should be set to TRUE if this is the 1st configuration
 *
 * @return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanAccessRegister(struct ADAPTER *prAdapter,
	uint32_t u4Addr, uint32_t *pru4Result,
	uint32_t u4Data, uint8_t ucSetQuery)
{
	struct INIT_CMD_ACCESS_REG rCmd = {0};
	void *prEvt;
	uint8_t ucEvtId;
	uint32_t u4EvtSize;
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	rCmd.ucSetQuery = ucSetQuery;
	rCmd.u4Address = u4Addr;
	rCmd.u4Data = u4Data;

	if (ucSetQuery == 1) {
		ucEvtId = INIT_EVENT_ID_CMD_RESULT;
		u4EvtSize = sizeof(struct INIT_EVENT_CMD_RESULT);
	} else {
		ucEvtId = INIT_EVENT_ID_ACCESS_REG;
		u4EvtSize = sizeof(struct INIT_EVENT_ACCESS_REG);
	}

	prEvt = kalMemAlloc(u4EvtSize, VIR_MEM_TYPE);
	if (!prEvt) {
		DBGLOG(INIT, ERROR, "Alloc evt packet failed.\n");
		u4Status = WLAN_STATUS_FAILURE;
		goto exit;
	}

	u4Status = wlanSendInitSetQueryCmd(prAdapter,
		INIT_CMD_ID_ACCESS_REG, &rCmd, sizeof(rCmd),
		TRUE, FALSE,
		ucEvtId, prEvt, u4EvtSize);
	if (u4Status != WLAN_STATUS_SUCCESS)
		goto exit;

	if (ucSetQuery == 1) {
		struct INIT_EVENT_CMD_RESULT *prEvent =
			(struct INIT_EVENT_CMD_RESULT *)prEvt;

		if (prEvent->ucStatus != 0) {
			DBGLOG(INIT, ERROR, "Event status: %d\n",
				prEvent->ucStatus);
			u4Status = WLAN_STATUS_FAILURE;
			goto exit;
		}
	} else {
		struct INIT_EVENT_ACCESS_REG *prEvent =
			(struct INIT_EVENT_ACCESS_REG *)prEvt;

		if (prEvent->u4Address != u4Addr) {
			DBGLOG(INIT, ERROR,
			       "Address incorrect. 0x%08x, 0x%08x.\n",
			       u4Addr, prEvent->u4Address);
			u4Status = WLAN_STATUS_FAILURE;
			goto exit;
		}
		*pru4Result = prEvent->u4Data;
	}

exit:
	if (prEvt)
		kalMemFree(prEvt, VIR_MEM_TYPE, u4EvtSize);

	if (u4Status != WLAN_STATUS_SUCCESS)
		GL_DEFAULT_RESET_TRIGGER(prAdapter,
					 RST_ACCESS_REG_FAIL);
	return u4Status;
}
#endif /* CFG_ENABLE_FW_DOWNLOAD == 1 */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to process queued RX packets
 *
 * @param prAdapter          Pointer to the Adapter structure.
 *        prSwRfbListHead    Pointer to head of RX packets link list
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanProcessQueuedSwRfb(struct ADAPTER
				*prAdapter, struct SW_RFB *prSwRfbListHead)
{
	struct SW_RFB *prSwRfb, *prNextSwRfb;
	struct TX_CTRL *prTxCtrl;
	struct RX_CTRL *prRxCtrl;
	struct STA_RECORD *prStaRec;

	ASSERT(prAdapter);
	ASSERT(prSwRfbListHead);

	prTxCtrl = &prAdapter->rTxCtrl;
	prRxCtrl = &prAdapter->rRxCtrl;

	prSwRfb = prSwRfbListHead;

	do {
		/* save next first */
		prNextSwRfb = QUEUE_GET_NEXT_ENTRY(prSwRfb);

		switch (prSwRfb->eDst) {
		case RX_PKT_DESTINATION_HOST:
			prStaRec = cnmGetStaRecByIndex(prAdapter,
						       prSwRfb->ucStaRecIdx);
			if (prStaRec && IS_STA_IN_AIS(prStaRec)) {
#if ARP_MONITER_ENABLE
				qmHandleRxArpPackets(prAdapter, prSwRfb);
#endif
			}

			nicRxProcessPktWithoutReorder(prAdapter, prSwRfb);
			break;

		case RX_PKT_DESTINATION_FORWARD:
			nicRxProcessForwardPkt(prAdapter, prSwRfb);
			break;

		case RX_PKT_DESTINATION_HOST_WITH_FORWARD:
			nicRxProcessGOBroadcastPkt(prAdapter, prSwRfb);
			break;

		case RX_PKT_DESTINATION_NULL:
			nicRxReturnRFB(prAdapter, prSwRfb);
			break;

		default:
			break;
		}

#if CFG_HIF_RX_STARVATION_WARNING
		prRxCtrl->u4DequeuedCnt++;
#endif
		prSwRfb = prNextSwRfb;
	} while (prSwRfb);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to purge queued TX packets
 *        by indicating failure to OS and returned to free list
 *
 * @param prAdapter          Pointer to the Adapter structure.
 *        prMsduInfoListHead Pointer to head of TX packets link list
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanProcessQueuedMsduInfo(struct ADAPTER *prAdapter,
				   struct MSDU_INFO *prMsduInfoListHead)
{
	ASSERT(prAdapter);
	ASSERT(prMsduInfoListHead);

	nicTxFreeMsduInfoPacket(prAdapter, prMsduInfoListHead);
	nicTxReturnMsduInfo(prAdapter, prMsduInfoListHead);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to check if the OID handler needs timeout
 *
 * @param prAdapter          Pointer to the Adapter structure.
 *        pfnOidHandler      Pointer to the OID handler
 *
 * @return TRUE
 *         FALSE
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanoidTimeoutCheck(struct ADAPTER *prAdapter,
			     PFN_OID_HANDLER_FUNC pfnOidHandler)
{
	PFN_OID_HANDLER_FUNC *apfnOidHandlerWOTimeoutCheck;
	uint32_t i;
	uint32_t u4NumOfElem;
	uint32_t u4OidTimeout;

	apfnOidHandlerWOTimeoutCheck = apfnOidWOTimeoutCheck;
	u4NumOfElem = sizeof(apfnOidWOTimeoutCheck) / sizeof(
			      PFN_OID_HANDLER_FUNC);

	for (i = 0; i < u4NumOfElem; i++) {
		if (apfnOidHandlerWOTimeoutCheck[i] == pfnOidHandler)
			return FALSE;
	}

	/* Decrease OID timeout threshold if chip NoAck/resetting */
	if (wlanIsChipNoAck(prAdapter)) {
		u4OidTimeout = WLAN_OID_TIMEOUT_THRESHOLD_IN_RESETTING;
		DBGLOG(INIT, INFO,
		       "Decrease OID timeout to %ums due to NoACK/CHIP-RESET\n",
		       u4OidTimeout);
	} else {
		u4OidTimeout = WLAN_OID_TIMEOUT_THRESHOLD;
	}

	/* Set OID timer for timeout check */
	cnmTimerStartTimer(prAdapter,
			   &(prAdapter->rOidTimeoutTimer), u4OidTimeout);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to clear any pending OID timeout check
 *
 * @param prAdapter          Pointer to the Adapter structure.
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanoidClearTimeoutCheck(struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	cnmTimerStopTimer(prAdapter, &(prAdapter->rOidTimeoutTimer));
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to override network address
 *        if NVRAM has a valid value
 *
 * @param prAdapter          Pointer to the Adapter structure.
 *
 * @return WLAN_STATUS_FAILURE   The request could not be processed
 *         WLAN_STATUS_PENDING   The request has been queued for later
 *				 processing
 *         WLAN_STATUS_SUCCESS   The request has been processed
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanUpdateNetworkAddress(struct ADAPTER
				  *prAdapter)
{
	const uint8_t aucZeroMacAddr[] = NULL_MAC_ADDR;
	uint8_t rMacAddr[PARAM_MAC_ADDR_LEN];
	uint32_t u4SysTime;

	DEBUGFUNC("wlanUpdateNetworkAddress");

	ASSERT(prAdapter);

	if (kalRetrieveNetworkAddress(prAdapter->prGlueInfo, rMacAddr) == FALSE
	    || IS_BMCAST_MAC_ADDR(rMacAddr)
	    || EQUAL_MAC_ADDR(aucZeroMacAddr, rMacAddr)) {
		/* eFUSE has a valid address, don't do anything */
		if (prAdapter->fgIsEmbbededMacAddrValid == TRUE) {
#if CFG_SHOW_MACADDR_SOURCE
			DBGLOG(INIT, INFO, "Using embedded MAC address");
#endif
			return WLAN_STATUS_SUCCESS;
		}
#if CFG_SHOW_MACADDR_SOURCE
		DBGLOG(INIT, INFO,
		       "Using dynamically generated MAC address");
#endif
		/* dynamic generate */
		u4SysTime = (uint32_t) kalGetTimeTick();

		rMacAddr[0] = 0x00;
		rMacAddr[1] = 0x08;
		rMacAddr[2] = 0x22;

		kalMemCopy(&rMacAddr[3], &u4SysTime, 3);

	} else {
#if CFG_SHOW_MACADDR_SOURCE
		DBGLOG(INIT, INFO, "Using host-supplied MAC address");
#endif
	}

#if WLAN_INCLUDE_SYS
	sysMacAddrOverride(rMacAddr);
#endif

	COPY_MAC_ADDR(prAdapter->rWifiVar.aucMacAddress, rMacAddr);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to update basic configuration into firmware
 *        domain
 *
 * @param prAdapter		Pointer to the Adapter structure.
 *
 * @return WLAN_STATUS_FAILURE	The request could not be processed
 *         WLAN_STATUS_PENDING	The request has been queued for later
 *				processing
 *         WLAN_STATUS_SUCCESS	The request has been processed
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanUpdateBasicConfig(struct ADAPTER *prAdapter)
{
	struct CMD_BASIC_CONFIG rCmdBasicConfig = {0};
	struct CMD_BASIC_CONFIG *prCmdBasicConfig = &rCmdBasicConfig;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;

	DEBUGFUNC("wlanUpdateBasicConfig");

	ASSERT(prAdapter);

	prCmdBasicConfig->ucNative80211 = 0;
	prCmdBasicConfig->rCsumOffload.u2RxChecksum = 0;
	prCmdBasicConfig->rCsumOffload.u2TxChecksum = 0;
	prCmdBasicConfig->ucCtrlFlagAssertPath =
		prWifiVar->ucCtrlFlagAssertPath;
	prCmdBasicConfig->ucCtrlFlagDebugLevel =
		prWifiVar->ucCtrlFlagDebugLevel;

#if CFG_TCP_IP_CHKSUM_OFFLOAD
	if (prAdapter->fgIsSupportCsumOffload) {
		if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_TCP)
			prCmdBasicConfig->rCsumOffload.u2TxChecksum |= BIT(2);

		if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_UDP)
			prCmdBasicConfig->rCsumOffload.u2TxChecksum |= BIT(1);

		if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_TX_IP)
			prCmdBasicConfig->rCsumOffload.u2TxChecksum |= BIT(0);

		if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_RX_TCP)
			prCmdBasicConfig->rCsumOffload.u2RxChecksum |= BIT(2);

		if (prAdapter->u4CSUMFlags & CSUM_OFFLOAD_EN_RX_UDP)
			prCmdBasicConfig->rCsumOffload.u2RxChecksum |= BIT(1);

		if (prAdapter->u4CSUMFlags & (CSUM_OFFLOAD_EN_RX_IPv4 |
					      CSUM_OFFLOAD_EN_RX_IPv6))
			prCmdBasicConfig->rCsumOffload.u2RxChecksum |= BIT(0);
	}
#endif

#ifdef CFG_SUPPORT_UNIFIED_COMMAND
	return wlanSendSetQueryCmdHelper(
#else
	return wlanSendSetQueryCmdAdv(
#endif
		prAdapter, CMD_ID_BASIC_CONFIG, 0, TRUE,
		FALSE, TRUE, NULL, NULL,
		sizeof(struct CMD_BASIC_CONFIG),
		(uint8_t *)prCmdBasicConfig, NULL, 0,
		CMD_SEND_METHOD_REQ_RESOURCE);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to check if the device is in RF test mode
 *
 * @param pfnOidHandler      Pointer to the OID handler
 *
 * @return TRUE
 *         FALSE
 */
/*----------------------------------------------------------------------------*/
u_int8_t wlanQueryTestMode(struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	return prAdapter->fgTestMode;
}

u_int8_t wlanProcessTxFrame(struct ADAPTER *prAdapter,
			    void *prPacket)
{
	uint32_t u4SysTime;
	uint8_t ucMacHeaderLen;
	struct TX_PACKET_INFO rTxPacketInfo;
	struct mt66xx_chip_info *prChipInfo = NULL;

	ASSERT(prAdapter);
	ASSERT(prPacket);
	prChipInfo = prAdapter->chip_info;

	if (kalQoSFrameClassifierAndPacketInfo(
		    prAdapter->prGlueInfo, prPacket, &rTxPacketInfo)) {

		/* Save the value of Priority Parameter */
		GLUE_SET_PKT_TID(prPacket, rTxPacketInfo.ucPriorityParam);

		if (rTxPacketInfo.u2Flag) {
			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_1X)) {
				struct STA_RECORD *prStaRec;

				DBGLOG(RSN, INFO, "T1X len=%d\n",
				       rTxPacketInfo.u4PacketLen);

				prStaRec = cnmGetStaRecByAddress(prAdapter,
						GLUE_GET_PKT_BSS_IDX(prPacket),
						rTxPacketInfo.aucEthDestAddr);

				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_1X);
			}

			if (rTxPacketInfo.u2Flag &
			    BIT(ENUM_PKT_NON_PROTECTED_1X))
				GLUE_SET_PKT_FLAG(prPacket,
						  ENUM_PKT_NON_PROTECTED_1X);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_802_3))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_802_3);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_VLAN_EXIST)
			    && FEAT_SUP_LLC_VLAN_TX(prChipInfo))
				GLUE_SET_PKT_FLAG(prPacket,
						  ENUM_PKT_VLAN_EXIST);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_DHCP))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_DHCP);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_ARP))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_ARP);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_ICMP))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_ICMP);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_TDLS))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_TDLS);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_DNS))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_DNS);

			if (rTxPacketInfo.u2Flag & BIT(ENUM_PKT_ICMPV6))
				GLUE_SET_PKT_FLAG(prPacket, ENUM_PKT_ICMPV6);
		}

		ucMacHeaderLen = ETHER_HEADER_LEN;

		/* Save the value of Header Length */
		GLUE_SET_PKT_HEADER_LEN(prPacket, ucMacHeaderLen);

		/* Save the value of Frame Length */
		GLUE_SET_PKT_FRAME_LEN(prPacket,
				       (uint16_t) rTxPacketInfo.u4PacketLen);

		/* Save the value of Arrival Time */
		u4SysTime = (OS_SYSTIME) kalGetTimeTick();
		GLUE_SET_PKT_ARRIVAL_TIME(prPacket, u4SysTime);

		return TRUE;
	}

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to send data frame via firmware and queued
 *        into command queue
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param prPacket       Pointer of native packet
 *
 * @return TRUE
 *         FALSE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanProcessCmdDataFrame(struct ADAPTER *prAdapter,
				 void *prPacket)
{
	struct CMD_INFO *prCmdInfo;
	struct MSDU_INFO *prMsduInfo;

	ASSERT(prAdapter);
	ASSERT(prPacket);

	/* Allocate command buffer */
	prCmdInfo = cmdBufAllocateCmdInfo(prAdapter, 0);
	if (!prCmdInfo) {
		DBGLOG_LIMITED(TX, INFO,
			   "Failed to alloc CMD buffer for data frame!!\n");
		return WLAN_STATUS_RESOURCES;
	}

	/* Get MSDU_INFO buffer */
	prMsduInfo = cnmPktAlloc(prAdapter, 0);
	if (!prMsduInfo) {
		DBGLOG_LIMITED(TX, INFO,
			   "Failed to alloc MSDU Info for data frame!!\n");

		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		return WLAN_STATUS_RESOURCES;
	}

	/* Fill-up MSDU_INFO and TxD*/
	if (!nicTxFillMsduInfo(prAdapter, prMsduInfo, prPacket) ||
		!nicTxProcessCmdDataPacket(prAdapter, prMsduInfo)) {

		kalSendComplete(prAdapter->prGlueInfo, prPacket,
				WLAN_STATUS_INVALID_PACKET);

		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		cnmPktFree(prAdapter, prMsduInfo);

		return WLAN_STATUS_INVALID_PACKET;
	}

	/* Prepare command and enqueue */
	prCmdInfo->eCmdType = COMMAND_TYPE_DATA_FRAME;
	prCmdInfo->u2InfoBufLen = prMsduInfo->u2FrameLength;
	prCmdInfo->pucInfoBuffer = NULL;
	prCmdInfo->prPacket = prMsduInfo->prPacket;
	prCmdInfo->prMsduInfo = prMsduInfo;
	prCmdInfo->pfCmdDoneHandler = wlanCmdDataFrameTxDone;
	prCmdInfo->pfCmdTimeoutHandler = wlanCmdDataFrameTxTimeout;
	prCmdInfo->fgIsOid = FALSE;
	prCmdInfo->fgSetQuery = TRUE;
	prCmdInfo->fgNeedResp = FALSE;
	prCmdInfo->ucCmdSeqNum = prMsduInfo->ucTxSeqNum;

	DBGLOG(TX, INFO,
		"DPP EN-Q MSDU[0x%p] SEQ[%u] BSS[%u] STA[%u] Len[%u]to CMD Q\n",
		prMsduInfo,
		prMsduInfo->ucTxSeqNum,
		prMsduInfo->ucBssIndex,
		prMsduInfo->ucStaRecIndex,
		prMsduInfo->u2FrameLength);

	kalEnqueueCommand(prAdapter->prGlueInfo,
			  (struct QUE_ENTRY *) prCmdInfo);

	GLUE_SET_EVENT(prAdapter->prGlueInfo);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called when command data frame has been
 *        sent to firmware
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param prCmdInfo      Pointer of CMD_INFO_T
 * @param pucEventBuf    meaningless, only for API compatibility
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanCmdDataFrameTxDone(struct ADAPTER *prAdapter,
			     struct CMD_INFO *prCmdInfo,
			     uint8_t *pucEventBuf)
{

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	kalCmdDataFrameSendComplete(prAdapter->prGlueInfo,
				     prCmdInfo->prPacket, WLAN_STATUS_SUCCESS);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called when Command Data frame TX timeout
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param prCmdInfo      Pointer of CMD_INFO_T
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanCmdDataFrameTxTimeout(struct ADAPTER *prAdapter,
		struct CMD_INFO *prCmdInfo)
{
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	kalCmdDataFrameSendComplete(prAdapter->prGlueInfo,
				     prCmdInfo->prPacket, WLAN_STATUS_FAILURE);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called before AIS is starting a new scan
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanClearScanningResult(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex)
{
	u_int8_t fgKeepCurrOne = FALSE;
	uint32_t i;
	struct WLAN_INFO *prWlanInfo;
	struct PARAM_BSSID_EX *prCurrBssid;

	ASSERT(prAdapter);
	prWlanInfo = &(prAdapter->rWlanInfo);

	prCurrBssid = aisGetCurrBssId(prAdapter,
		ucBssIndex);

	/* clear scanning result */
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo,
		ucBssIndex) ==
	    MEDIA_STATE_CONNECTED) {

		for (i = 0; i < prWlanInfo->u4ScanResultNum; i++) {

		if (EQUAL_MAC_ADDR(
		    prCurrBssid->arMacAddress,
		    prWlanInfo->arScanResult[i].arMacAddress)) {
			fgKeepCurrOne = TRUE;

			if (i != 0) {
				/* copy structure */
				kalMemCopy(
				    &(prWlanInfo->arScanResult[0]),
				    &(prWlanInfo->arScanResult[i]),
				    OFFSET_OF(struct PARAM_BSSID_EX,
					      aucIEs));
			}

			if (prWlanInfo->arScanResult[i].u4IELength > 0) {
				if (prWlanInfo->apucScanResultIEs[i] !=
				    &(prWlanInfo->aucScanIEBuf[0])) {

				/* move IEs to head */
				kalMemCopy(prWlanInfo->aucScanIEBuf,
					   prWlanInfo->apucScanResultIEs[i],
					   prWlanInfo->arScanResult[i]
					   .u4IELength);
				}

				/* modify IE pointer */
				prWlanInfo->apucScanResultIEs[0] =
					&(prWlanInfo->aucScanIEBuf[0]);

			} else {
				prWlanInfo->apucScanResultIEs[0] = NULL;
			}

			break;
		} /* if */
		} /* for */
	}

	if (fgKeepCurrOne == TRUE) {
		prWlanInfo->u4ScanResultNum = 1;
		prWlanInfo->u4ScanIEBufferUsage =
		    ALIGN_4(prWlanInfo->arScanResult[0].u4IELength);
	} else {
		prWlanInfo->u4ScanResultNum = 0;
		prWlanInfo->u4ScanIEBufferUsage = 0;
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called when AIS received a beacon timeout event
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param arBSSID        MAC address of the specified BSS
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanClearBssInScanningResult(struct ADAPTER
				  *prAdapter, uint8_t *arBSSID)
{
	uint32_t i, j, u4IELength = 0, u4IEMoveLength;
	uint8_t *pucIEPtr;
	struct WLAN_INFO *prWlanInfo;

	ASSERT(prAdapter);
	prWlanInfo = &(prAdapter->rWlanInfo);

	/* clear scanning result */
	i = 0;
	while (1) {
		if (i >= prWlanInfo->u4ScanResultNum)
			break;

		if (EQUAL_MAC_ADDR(arBSSID,
				   prWlanInfo->arScanResult[i].arMacAddress)) {
			/* backup current IE length */
			u4IELength =
				ALIGN_4(prWlanInfo->arScanResult[i].u4IELength);
			pucIEPtr = prWlanInfo->apucScanResultIEs[i];

			/* removed from middle */
			for (j = i + 1; j < prWlanInfo->u4ScanResultNum; j++) {
				kalMemCopy(&(prWlanInfo->arScanResult[j - 1]),
					   &(prWlanInfo->arScanResult[j]),
					   OFFSET_OF(struct PARAM_BSSID_EX,
					   aucIEs));

				prWlanInfo->apucScanResultIEs[j - 1] =
					prWlanInfo->apucScanResultIEs[j];
			}

			prWlanInfo->u4ScanResultNum--;

			/* remove IE buffer if needed := move rest of IE buffer
			 */
			if (u4IELength > 0) {
				u4IEMoveLength = prWlanInfo->u4ScanIEBufferUsage
					- (((uintptr_t) pucIEPtr)
					+ u4IELength
					- ((uintptr_t)
					(&(prWlanInfo->aucScanIEBuf[0]))));

				kalMemCopy(pucIEPtr,
					   (uint8_t *) (((uintptr_t)
					   pucIEPtr) + u4IELength),
					   u4IEMoveLength);

				prWlanInfo->u4ScanIEBufferUsage -=
								u4IELength;

				/* correction of pointers to IE buffer */
				for (j = 0; j < prWlanInfo->u4ScanResultNum;
				     j++) {
					if (prWlanInfo->apucScanResultIEs[j] >
					    pucIEPtr) {
					prWlanInfo->apucScanResultIEs[j] =
					    (uint8_t *)((uintptr_t)
					    (prWlanInfo->apucScanResultIEs[j]) -
					    u4IELength);
					}
				}
			}
		}

		i++;
	}
}

#if CFG_TEST_WIFI_DIRECT_GO
void wlanEnableP2pFunction(struct ADAPTER *prAdapter)
{
#if 0
	P_MSG_P2P_FUNCTION_SWITCH_T prMsgFuncSwitch =
		(P_MSG_P2P_FUNCTION_SWITCH_T) NULL;

	prMsgFuncSwitch =
		(P_MSG_P2P_FUNCTION_SWITCH_T) cnmMemAlloc(prAdapter,
			RAM_TYPE_MSG, sizeof(MSG_P2P_FUNCTION_SWITCH_T));
	if (!prMsgFuncSwitch) {
		ASSERT(FALSE);
		return;
	}

	prMsgFuncSwitch->rMsgHdr.eMsgId = MID_MNY_P2P_FUN_SWITCH;
	prMsgFuncSwitch->fgIsFuncOn = TRUE;

	mboxSendMsg(prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prMsgFuncSwitch, MSG_SEND_METHOD_BUF);
#endif

}

void wlanEnableATGO(struct ADAPTER *prAdapter)
{

	struct MSG_P2P_CONNECTION_REQUEST *prMsgConnReq =
		(struct MSG_P2P_CONNECTION_REQUEST *) NULL;
	uint8_t aucTargetDeviceID[MAC_ADDR_LEN] = { 0xFF, 0xFF, 0xFF, 0xFF,
						    0xFF, 0xFF };

	prMsgConnReq =
		(struct MSG_P2P_CONNECTION_REQUEST *) cnmMemAlloc(prAdapter,
		RAM_TYPE_MSG, sizeof(struct MSG_P2P_CONNECTION_REQUEST));
	if (!prMsgConnReq) {
		ASSERT(FALSE);
		return;
	}

	prMsgConnReq->rMsgHdr.eMsgId = MID_MNY_P2P_CONNECTION_REQ;

	/*=====Param Modified for test=====*/
	COPY_MAC_ADDR(prMsgConnReq->aucDeviceID, aucTargetDeviceID);
	prMsgConnReq->fgIsTobeGO = TRUE;
	prMsgConnReq->fgIsPersistentGroup = FALSE;

	/*=====Param Modified for test=====*/

	mboxSendMsg(prAdapter, MBOX_ID_0,
		    (struct MSG_HDR *) prMsgConnReq, MSG_SEND_METHOD_BUF);

}
#endif

void wlanPrintVersion(struct ADAPTER *prAdapter)
{
	uint8_t aucBuf[512];

	kalMemZero(aucBuf, 512);

#if CFG_ENABLE_FW_DOWNLOAD
	fwDlGetFwdlInfo(prAdapter, aucBuf, 512);
#endif
	DBGLOG(SW4, INFO, "%s", aucBuf);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to retrieve NIC capability from firmware
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanQueryNicCapability(struct ADAPTER
				*prAdapter)
{
	struct mt66xx_chip_info *prChipInfo;
	uint8_t aucZeroMacAddr[] = NULL_MAC_ADDR;
	uint32_t u4RxPktLength;
	uint8_t *aucBuffer;
	uint32_t u4EventSize;
	struct WIFI_EVENT *prEvent;
	struct EVENT_NIC_CAPABILITY *prEventNicCapability;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;

	DEBUGFUNC("wlanQueryNicCapability");

	wlanSendSetQueryCmdAdv(
		prAdapter, CMD_ID_GET_NIC_CAPABILITY, 0,
		FALSE, TRUE, FALSE, NULL, NULL, 0, NULL, NULL, 0,
		CMD_SEND_METHOD_REQ_RESOURCE);

	u4EventSize = prChipInfo->rxd_size + prChipInfo->event_hdr_size +
		sizeof(struct EVENT_NIC_CAPABILITY);
	aucBuffer = kalMemAlloc(u4EventSize, PHY_MEM_TYPE);
	if (!aucBuffer) {
		DBGLOG(INIT, ERROR, "Not enough memory for aucBuffer\n");
		return WLAN_STATUS_FAILURE;
	}

	while (TRUE) {
		if (nicRxWaitResponse(prAdapter, 1, aucBuffer, u4EventSize,
				      &u4RxPktLength) != WLAN_STATUS_SUCCESS) {

			DBGLOG(INIT, WARN, "%s: wait for event failed!\n",
			       __func__);
			kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);
			return WLAN_STATUS_FAILURE;
		}
		/* header checking .. */
		if (NIC_RX_GET_U2_SW_PKT_TYPE(aucBuffer)
			!= prChipInfo->u2RxSwPktEvent) {
			DBGLOG(INIT, WARN,
			       "%s: skip unexpected Rx pkt type[0x%04x]\n",
			       __func__, NIC_RX_GET_U2_SW_PKT_TYPE(aucBuffer));
			continue;
		}

		prEvent = (struct WIFI_EVENT *)
			(aucBuffer + prChipInfo->rxd_size);
		prEventNicCapability =
			(struct EVENT_NIC_CAPABILITY *)prEvent->aucBuffer;

		if (prEvent->ucEID != EVENT_ID_NIC_CAPABILITY) {
			DBGLOG(INIT, WARN,
			      "%s: skip unexpected event ID[0x%02x]\n",
			      __func__, prEvent->ucEID);
			continue;
		} else {
			break;
		}
	}

	prEventNicCapability = (struct EVENT_NIC_CAPABILITY *) (
				       prEvent->aucBuffer);

	prAdapter->rVerInfo.u2FwProductID =
		prEventNicCapability->u2ProductID;
	kalMemCopy(prAdapter->rVerInfo.aucFwBranchInfo,
		   prEventNicCapability->aucBranchInfo, 4);
	prAdapter->rVerInfo.u2FwOwnVersion =
		prEventNicCapability->u2FwVersion;
	prAdapter->rVerInfo.ucFwBuildNumber =
		prEventNicCapability->ucFwBuildNumber;
	kalMemCopy(prAdapter->rVerInfo.aucFwDateCode,
		   prEventNicCapability->aucDateCode, 16);
	prAdapter->rVerInfo.u2FwPeerVersion =
		prEventNicCapability->u2DriverVersion;
	prAdapter->fgIsHw5GBandDisabled =
			(u_int8_t)prEventNicCapability->ucHw5GBandDisabled;
	prAdapter->fgIsEepromUsed =
			(u_int8_t)prEventNicCapability->ucEepromUsed;
	prAdapter->fgIsEmbbededMacAddrValid =
			(u_int8_t)(!IS_BMCAST_MAC_ADDR(
				prEventNicCapability->aucMacAddr) &&
				!EQUAL_MAC_ADDR(aucZeroMacAddr,
				prEventNicCapability->aucMacAddr));

	COPY_MAC_ADDR(prAdapter->rWifiVar.aucPermanentAddress,
		      prEventNicCapability->aucMacAddr);
	COPY_MAC_ADDR(prAdapter->rWifiVar.aucMacAddress,
		      prEventNicCapability->aucMacAddr);

	prAdapter->rWifiVar.ucStaVht &=
				(!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucApVht &=
				(!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucP2pGoVht &=
				(!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucP2pGcVht &=
				(!(prEventNicCapability->ucHwNotSupportAC));
	prAdapter->rWifiVar.ucHwNotSupportAC =
				prEventNicCapability->ucHwNotSupportAC;

	prAdapter->u4FwCompileFlag0 =
		prEventNicCapability->u4CompileFlag0;
	prAdapter->u4FwCompileFlag1 =
		prEventNicCapability->u4CompileFlag1;
	prAdapter->u4FwFeatureFlag0 =
		prEventNicCapability->u4FeatureFlag0;
	prAdapter->u4FwFeatureFlag1 =
		prEventNicCapability->u4FeatureFlag1;

	if (prEventNicCapability->ucHwSetNss1x1)
		prAdapter->rWifiVar.ucNSS = 1;

#if CFG_SUPPORT_DBDC
	if (prEventNicCapability->ucHwNotSupportDBDC)
		prAdapter->rWifiVar.eDbdcMode = ENUM_DBDC_MODE_DISABLED;
#endif
	if (prEventNicCapability->ucHwBssIdNum > 0
	    && prEventNicCapability->ucHwBssIdNum <= MAX_BSSID_NUM) {
		prAdapter->ucHwBssIdNum =
			prEventNicCapability->ucHwBssIdNum;
		prAdapter->ucP2PDevBssIdx = prAdapter->ucHwBssIdNum;
		/* v1 event does not report WmmSetNum,
		 * Assume it is the same as HwBssNum
		 */
		prAdapter->ucWmmSetNum =
			prEventNicCapability->ucHwBssIdNum;
		prAdapter->aprBssInfo[prAdapter->ucP2PDevBssIdx] =
			&prAdapter->rWifiVar.rP2pDevInfo;
	}

#if CFG_ENABLE_CAL_LOG
	DBGLOG(INIT, TRACE,
	       "RF CAL FAIL  = (%d),BB CAL FAIL  = (%d)\n",
	       prEventNicCapability->ucRfCalFail,
	       prEventNicCapability->ucBbCalFail);
#endif
	kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);

	return WLAN_STATUS_SUCCESS;
}

#if TXPWR_USE_PDSLOPE

/*----------------------------------------------------------------------------*/
/*!
 * @brief
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanQueryPdMcr(struct ADAPTER *prAdapter,
			struct PARAM_MCR_RW_STRUCT *prMcrRdInfo)
{
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4RxPktLength;
	uint8_t *aucBuffer;
	uint32_t u4EventSize;
	struct WIFI_EVENT *prEvent;
	struct CMD_ACCESS_REG *prCmdMcrQuery;
	uint16_t u2PacketType;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;

	u4EventSize = prChipInfo->rxd_size + prChipInfo->event_hdr_size +
		struct CMD_ACCESS_REG;
	aucBuffer = kalMemAlloc(u4EventSize, PHY_MEM_TYPE);

#ifdef CFG_SUPPORT_UNIFIED_COMMAND
	wlanSendSetQueryCmdHelper(
#else
	wlanSendSetQueryCmdAdv(
#endif
		prAdapter, CMD_ID_ACCESS_REG, 0, FALSE,
		TRUE, FALSE, NULL, nicOidCmdTimeoutCommon,
		sizeof(struct CMD_ACCESS_REG),
		(uint8_t *)prMcrRdInfo, NULL, 0,
		CMD_SEND_METHOD_REQ_RESOURCE);

	if (nicRxWaitResponse(prAdapter, 1, aucBuffer, u4EventSize,
			      &u4RxPktLength) != WLAN_STATUS_SUCCESS) {
		kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);
		return WLAN_STATUS_FAILURE;
	}
	/* header checking .. */
	u2PacketType = NIC_RX_GET_PKT_TYPE(aucBuffer);
	if (u2PacketType != RX_PKT_TYPE_SW_DEFINED) {
		kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);
		return WLAN_STATUS_FAILURE;
	}

	prEvent = (struct WIFI_EVENT *)
		(aucBuffer + prChipInfo->rxd_size);
	if (prEvent->ucEID != EVENT_ID_ACCESS_REG) {
		kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);
		return WLAN_STATUS_FAILURE;
	}

	prCmdMcrQuery = (struct CMD_ACCESS_REG *) (
				prEvent->aucBuffer);
	prMcrRdInfo->u4McrOffset = prCmdMcrQuery->u4Address;
	prMcrRdInfo->u4McrData = prCmdMcrQuery->u4Data;

	kalMemFree(aucBuffer, PHY_MEM_TYPE, u4EventSize);

	return WLAN_STATUS_SUCCESS;
}

static int32_t wlanIntRound(int32_t au4Input)
{

	if (au4Input >= 0) {
		if ((au4Input % 10) == 5) {
			au4Input = au4Input + 5;
			return au4Input;
		}
	}

	if (au4Input < 0) {
		if ((au4Input % 10) == -5) {
			au4Input = au4Input - 5;
			return au4Input;
		}
	}

	return au4Input;
}

static int32_t wlanCal6628EfuseForm(struct ADAPTER
				    *prAdapter, int32_t au4Input)
{

	struct PARAM_MCR_RW_STRUCT rMcrRdInfo;
	int32_t au4PdSlope, au4TxPwrOffset, au4TxPwrOffset_Round;
	int8_t auTxPwrOffset_Round;

	rMcrRdInfo.u4McrOffset = 0x60205c68;
	rMcrRdInfo.u4McrData = 0;
	au4TxPwrOffset = au4Input;
	wlanQueryPdMcr(prAdapter, &rMcrRdInfo);

	au4PdSlope = (rMcrRdInfo.u4McrData) & BITS(0, 6);
	au4TxPwrOffset_Round = wlanIntRound((au4TxPwrOffset *
					     au4PdSlope)) / 10;

	au4TxPwrOffset_Round = -au4TxPwrOffset_Round;

	if (au4TxPwrOffset_Round < -128)
		au4TxPwrOffset_Round = 128;
	else if (au4TxPwrOffset_Round < 0)
		au4TxPwrOffset_Round += 256;
	else if (au4TxPwrOffset_Round > 127)
		au4TxPwrOffset_Round = 127;

	auTxPwrOffset_Round = (uint8_t) au4TxPwrOffset_Round;

	return au4TxPwrOffset_Round;
}

#endif

#define NVRAM_OFS(elment) OFFSET_OF(struct WIFI_CFG_PARAM_STRUCT, elment)

uint32_t wlanGetMiniTxPower(struct ADAPTER *prAdapter,
				  enum ENUM_BAND eBand,
				  enum ENUM_PHY_MODE_TYPE ePhyMode,
				  int8_t *pTxPwr)
{
#ifdef SOC3_0
	struct REG_INFO *prRegInfo;
	struct WIFI_CFG_PARAM_STRUCT *prNvramSettings;
	uint8_t *pu1Addr;
	int8_t bandIdx = 0;

	int8_t minTxPwr = 0x7F;
	int8_t nvTxPwr = 0;
	uint32_t rateIdx = 0;
	uint32_t startOfs = 0;
	uint32_t endOfs = 0;

	struct NVRAM_FRAGMENT_RANGE arRange[2][PHY_MODE_TYPE_NUM] = {
		/*BAND2G4*/
		{
			/*CCK*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrCck1M),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrCck11M)},
			/*OFDM*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrOfdm6M),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrOfdm54M)},
			/*HT20*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrHt20Mcs0),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrHt20Mcs7)},
			/*HT40*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrHt40Mcs0),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrHt40Mcs32)},
			/*VHT20*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrVht20Mcs0),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrVht20Mcs9)},
			/*VHT40*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrVht40Mcs0),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrVht40Mcs9)},
			/*VHT80*/
			{0, 0},
			/*SU20*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU242Mcs0),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU242Mcs11)},
			/*SU40*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU484Mcs0),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU484Mcs11)},
			/*SU80*/
			{0, 0},
			/*RU26*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU26Mcs0),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU26Mcs11)},
			/*RU52*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU52Mcs0),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU52Mcs11)},
			/*RU106*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU106Mcs0),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU106Mcs11)},
			/*RU242*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU242Mcs0),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU242Mcs11)},
			/*RU484*/
			{NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU484Mcs0),
			NVRAM_OFS(r2G4Pwr.uc2G4TxPwrRU484Mcs11)},
			/*RU996*/
			{0, 0},
		},
		/*BAND5G*/
		{
			/*CCK*/
			{0, 0},
			/*OFDM*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrOfdm6M),
			NVRAM_OFS(r5GPwr.uc5GTxPwrOfdm54M)},
			/*HT20*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrHt20Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrHt20Mcs7)},
			/*HT40*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrHt40Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrHt40Mcs32)},
			/*VHT20*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrVht20Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrVht20Mcs9)},
			/*VHT40*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrVht40Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrVht40Mcs9)},
			/*VHT80*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrVht80Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrVht80Mcs9)},
			/*SU20*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrRU242Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrRU242Mcs11)},
			/*SU40*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrRU484Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrRU484Mcs11)},
			/*SU80*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrRU996Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrRU996Mcs11)},
			/*RU26*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrRU26Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrRU26Mcs11)},
			/*RU52*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrRU52Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrRU52Mcs11)},
			/*RU106*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrRU106Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrRU106Mcs11)},
			/*RU242*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrRU242Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrRU242Mcs11)},
			/*RU484*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrRU484Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrRU484Mcs11)},
			/*RU996*/
			{NVRAM_OFS(r5GPwr.uc5GTxPwrRU996Mcs0),
			NVRAM_OFS(r5GPwr.uc5GTxPwrRU996Mcs11)},
		}
	};

	ASSERT(prAdapter);
	prRegInfo = &prAdapter->prGlueInfo->rRegInfo;

	ASSERT(prRegInfo);
	prNvramSettings = prRegInfo->prNvramSettings;
	ASSERT(prNvramSettings);

	if (prAdapter->prGlueInfo->fgNvramAvailable == FALSE) {
		DBGLOG(INIT, WARN, "check Nvram fail\n");
		return WLAN_STATUS_NOT_ACCEPTED;
	}

	if (eBand == BAND_2G4)
		bandIdx = 0;
	else if (eBand == BAND_5G)
		bandIdx = 1;
	else {
		DBGLOG(INIT, WARN, "check Band fail(%d)\n",
			eBand);
		return WLAN_STATUS_NOT_ACCEPTED;
	}
	if (ePhyMode >= PHY_MODE_TYPE_NUM) {
		DBGLOG(INIT, WARN, "check phy mode fail(%d)\n",
			ePhyMode);
		return WLAN_STATUS_NOT_ACCEPTED;
	}

	startOfs = arRange[(uint8_t)bandIdx][(uint8_t)ePhyMode].startOfs;
	endOfs = arRange[(uint8_t)bandIdx][(uint8_t)ePhyMode].endOfs;


	/*NVRAM start addreess :0*/
	pu1Addr = (uint8_t *)&prNvramSettings->u2Part1OwnVersion;

	for (rateIdx = startOfs; rateIdx <= endOfs; rateIdx++) {
		nvTxPwr = *(pu1Addr + rateIdx);

		if (nvTxPwr < minTxPwr)
			minTxPwr = nvTxPwr;
	}

	/*NVRAM resolution is S6.1 format*/
	*pTxPwr = minTxPwr >> 1;

	DBGLOG(INIT, INFO, "Band[%s],PhyMode[%d],get mini txpwr=%d\n",
		(eBand == BAND_2G4)?"2G4":"5G",
		ePhyMode,
		*pTxPwr);

	return WLAN_STATUS_SUCCESS;
#else
	DBGLOG(INIT, WARN, "Not supported!");
	return WLAN_STATUS_NOT_SUPPORTED;
#endif
}


/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to load manufacture data from NVRAM
 * if available and valid
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param prRegInfo      Pointer of REG_INFO_T
 *
 * @return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanLoadManufactureData(struct ADAPTER
				 *prAdapter, struct REG_INFO *prRegInfo)
{

#if CFG_SUPPORT_RDD_TEST_MODE
	struct CMD_RDD_CH rRddParam;
#endif
	uint8_t index = 0;
	uint8_t *pu1Addr;
	uint8_t u1TypeID;
	uint8_t u1LenLSB;
	uint8_t u1LenMSB;
	uint8_t fgSupportFragment = FALSE;
	uint32_t u4NvramStartOffset = 0, u4NvramOffset = 0;
	uint32_t u4NvramFragmentSize = 0;
	struct CMD_NVRAM_FRAGMENT *prCmdNvramFragment;
	struct WIFI_NVRAM_TAG_FORMAT *prTagDataCurr;
	struct CMD_NVRAM_SETTING *prCmdNvramSettings;

	if (prRegInfo == NULL) {
		DBGLOG(INIT, ERROR, "prRegInfo = NULL");
		return WLAN_STATUS_FAILURE;
	}
	ASSERT(prAdapter);

	fgSupportFragment = prAdapter->chip_info->is_support_nvram_fragment;


	/* 1. Version Check */
	if (prAdapter->prGlueInfo->fgNvramAvailable == TRUE) {
		prAdapter->rVerInfo.u2Part1CfgOwnVersion =
			prRegInfo->prNvramSettings->u2Part1OwnVersion;
		prAdapter->rVerInfo.u2Part1CfgPeerVersion =
			prRegInfo->prNvramSettings->u2Part1PeerVersion;
		prAdapter->rVerInfo.u2Part2CfgOwnVersion = 0;
			/* prRegInfo->prNvramSettings->u2Part2OwnVersion; */
		prAdapter->rVerInfo.u2Part2CfgPeerVersion = 0;
			/* prRegInfo->prNvramSettings->u2Part2PeerVersion; */
	}

	/* 3. Check if needs to support 5GHz */
	if (prRegInfo->ucEnable5GBand) {
		/* check if it is disabled by hardware */
		if (prAdapter->fgIsHw5GBandDisabled
		    || prRegInfo->ucSupport5GBand == 0)
			prAdapter->fgEnable5GBand = FALSE;
		else
			prAdapter->fgEnable5GBand = TRUE;
	} else
		prAdapter->fgEnable5GBand = FALSE;

	DBGLOG(INIT, INFO, "Enable5GBand = %d, Detail = [%d,%d,%d]",
		prAdapter->fgEnable5GBand,
		prRegInfo->ucEnable5GBand,
		prRegInfo->ucSupport5GBand,
		prAdapter->fgIsHw5GBandDisabled);

	/* 5. Get 16-bits Country Code and Bandwidth */
	prAdapter->rWifiVar.u2CountryCode =
		(((uint16_t) prRegInfo->au2CountryCode[0]) << 8) | (((
			uint16_t) prRegInfo->au2CountryCode[1]) & BITS(0, 7));

	/* 6. Set domain and channel information to chip
	 * Note :
	 * Skip send dynamic txpower result to FW
	 * when send NVRAM to FW during Wi-Fi on,
	 * due to config haven't been load yet.
	 * During Wi-Fi on, we will send power limit only once
	 * after load config.
	 * The result of power limit send to FW will be the minimum value
	 * of country power limit and config setting
	 */
	rlmDomainSendCmd(prAdapter, FALSE);

	/* Update supported channel list in channel table */
	wlanUpdateChannelTable(prAdapter->prGlueInfo);

	/* 9. Send the full Parameters of NVRAM to FW */

	/*NVRAM Parameter fragment, if NVRAM length is over 2048 bytes*/
	/*Driver will split NVRAM by TLV Group ,each TX CMD is 1564 bytes*/
	if (fgSupportFragment) {
		prCmdNvramFragment = (struct CMD_NVRAM_FRAGMENT *)kalMemAlloc(
					  sizeof(struct CMD_NVRAM_FRAGMENT),
					  VIR_MEM_TYPE);
		if (prCmdNvramFragment == NULL) {
			DBGLOG(INIT, ERROR,
				"prCmdNvramFragment allocate fail\n");
			return WLAN_STATUS_FAILURE;
		}

		u4NvramStartOffset = 0;
		pu1Addr = (uint8_t *)
			&prRegInfo->prNvramSettings->u2Part1OwnVersion;

		prTagDataCurr =
			(struct WIFI_NVRAM_TAG_FORMAT *)(pu1Addr
			+ OFFSET_OF(struct WIFI_CFG_PARAM_STRUCT, ucTypeID0));
		u1TypeID = prTagDataCurr->u1NvramTypeID;
		u1LenLSB = prTagDataCurr->u1NvramTypeLenLsb;
		u1LenMSB = prTagDataCurr->u1NvramTypeLenMsb;
		u4NvramOffset +=
			OFFSET_OF(struct WIFI_CFG_PARAM_STRUCT, ucTypeID0);

		DBGLOG(INIT, TRACE,
			   "NVRAM-Frag Startofs[0x%08X]ID[%d][0x%08X]Len:%d\n"
			    , u4NvramStartOffset
			    , u1TypeID
			    , u4NvramOffset
			    , (u1LenMSB << 8) | (u1LenLSB));

		while (1) {

			kalMemZero(prCmdNvramFragment,
				 sizeof(struct CMD_NVRAM_FRAGMENT));

			u4NvramOffset += NVRAM_TAG_HDR_SIZE;
			u4NvramOffset += (u1LenMSB << 8) | (u1LenLSB);
			u4NvramFragmentSize = u4NvramOffset-u4NvramStartOffset;

			DBGLOG(INIT, TRACE,
			   "NVRAM Fragement(%d)Startofs[0x%08X]ID[%d]Len:%d\n",
			   index, u4NvramStartOffset,
			   u1TypeID, u4NvramFragmentSize);

			if (u4NvramFragmentSize >
				sizeof(struct CMD_NVRAM_FRAGMENT)) {
				DBGLOG(INIT, ERROR,
				"ID[%d]copy size[%d]bigger than buf size[%d]\n",
				u1TypeID,
				u4NvramFragmentSize,
				sizeof(struct CMD_NVRAM_FRAGMENT));
				return WLAN_STATUS_FAILURE;
			}

			kalMemCopy(prCmdNvramFragment,
					   (pu1Addr + u4NvramStartOffset),
					   u4NvramFragmentSize);

			wlanSendSetQueryCmd(prAdapter,
				CMD_ID_SET_NVRAM_SETTINGS,
				TRUE,
				FALSE,
				FALSE, NULL, NULL,
				sizeof(struct CMD_NVRAM_FRAGMENT),
				(uint8_t *) prCmdNvramFragment, NULL, 0);

			/*update the next Fragment group start offset*/
			u4NvramStartOffset = u4NvramOffset;
			index++;

			/*get the nex TLV format*/
			prTagDataCurr = (struct WIFI_NVRAM_TAG_FORMAT *)
				(pu1Addr + u4NvramOffset);

			u1TypeID = prTagDataCurr->u1NvramTypeID;
			u1LenLSB = prTagDataCurr->u1NvramTypeLenLsb;
			u1LenMSB = prTagDataCurr->u1NvramTypeLenMsb;
			/*sanity check*/
			if ((u1TypeID == 0) &&
				(u1LenLSB == 0) && (u1LenMSB == 0)) {
				DBGLOG(INIT, INFO,
					"TLV is Null, last index = %d\n",
					index);
				break;
			}
		}

		kalMemFree(prCmdNvramFragment, VIR_MEM_TYPE,
			 sizeof(struct CMD_NVRAM_FRAGMENT));

	} else {

		prCmdNvramSettings = (struct CMD_NVRAM_SETTING *)kalMemAlloc(
				      sizeof(struct CMD_NVRAM_SETTING),
				      VIR_MEM_TYPE);

		kalMemCopy(&prCmdNvramSettings->rNvramSettings,
			   &prRegInfo->prNvramSettings->u2Part1OwnVersion,
			   sizeof(struct CMD_NVRAM_SETTING));
		ASSERT(sizeof(struct WIFI_CFG_PARAM_STRUCT) == 2048);
		wlanSendSetQueryCmd(prAdapter,
				    CMD_ID_SET_NVRAM_SETTINGS,
				    TRUE,
				    FALSE,
				    FALSE, NULL, NULL,
				    sizeof(*prCmdNvramSettings),
				    (uint8_t *) prCmdNvramSettings, NULL, 0);

		kalMemFree(prCmdNvramSettings, VIR_MEM_TYPE,
				   sizeof(struct CMD_NVRAM_SETTING));
	}


	wlanNvramSetState(NVRAM_STATE_SEND_TO_FW);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to check if any pending timer has expired
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanTimerTimeoutCheck(struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	cnmTimerDoTimeOutCheck(prAdapter);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to check if any pending mailbox message
 *        to be handled
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanProcessMboxMessage(struct ADAPTER
				*prAdapter)
{
	uint32_t i;

	ASSERT(prAdapter);

	for (i = 0; i < MBOX_ID_TOTAL_NUM; i++)
		mboxRcvAllMsg(prAdapter, (enum ENUM_MBOX_ID) i);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to enqueue a single TX packet into CORE
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *        prNativePacket Pointer of Native Packet
 *
 * @return WLAN_STATUS_SUCCESS
 *         WLAN_STATUS_RESOURCES
 *         WLAN_STATUS_INVALID_PACKET
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanEnqueueTxPacket(struct ADAPTER *prAdapter,
			     void *prNativePacket)
{
	struct TX_CTRL *prTxCtrl;
	struct MSDU_INFO *prMsduInfo;

	ASSERT(prAdapter);

	prTxCtrl = &prAdapter->rTxCtrl;

	prMsduInfo = cnmPktAlloc(prAdapter, 0);

	if (!prMsduInfo)
		return WLAN_STATUS_RESOURCES;

	if (nicTxFillMsduInfo(prAdapter, prMsduInfo,
			      prNativePacket)) {
		TX_INC_CNT(&prAdapter->rTxCtrl, TX_MSDUINFO_COUNT);

		/* prMsduInfo->eSrc = TX_PACKET_OS; */

		/* Tx profiling */
		wlanTxProfilingTagMsdu(prAdapter, prMsduInfo,
				       TX_PROF_TAG_DRV_ENQUE);

#if CFG_FAST_PATH_SUPPORT
		/* Check if need to send a MSCS request */
		if (mscsIsNeedRequest(prAdapter, prNativePacket)) {
			/* Request a mscs frame if needed */
			mscsRequest(prAdapter, prNativePacket, MSCS_REQUEST,
				FRAME_CLASSIFIER_TYPE_4);
		}
#endif

		/* enqueue to QM */
		nicTxEnqueueMsdu(prAdapter, prMsduInfo);

		return WLAN_STATUS_SUCCESS;
	}
	kalSendComplete(prAdapter->prGlueInfo, prNativePacket,
			WLAN_STATUS_INVALID_PACKET);

	nicTxReturnMsduInfo(prAdapter, prMsduInfo);

	return WLAN_STATUS_INVALID_PACKET;

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to flush pending TX packets in CORE
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanFlushTxPendingPackets(struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	return nicTxFlush(prAdapter);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief this function sends pending MSDU_INFO_T to MT6620
 *
 * @param prAdapter      Pointer to the Adapter structure.
 * @param pfgHwAccess    Pointer for tracking LP-OWN status
 *
 * @retval WLAN_STATUS_SUCCESS   Reset is done successfully.
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanTxPendingPackets(struct ADAPTER *prAdapter,
			      u_int8_t *pfgHwAccess)
{
	struct TX_CTRL *prTxCtrl;
	struct MSDU_INFO *prMsduInfo;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

#if !CFG_SUPPORT_MULTITHREAD
	ASSERT(pfgHwAccess);
#endif

	/* <1> dequeue packet by txDequeuTxPackets() */
#if CFG_SUPPORT_MULTITHREAD
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
	prMsduInfo = qmDequeueTxPacketsMthread(prAdapter,
					       &prTxCtrl->rTc);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
#else
	KAL_ACQUIRE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
	prMsduInfo = qmDequeueTxPackets(prAdapter, &prTxCtrl->rTc);
	KAL_RELEASE_SPIN_LOCK(prAdapter, SPIN_LOCK_QM_TX_QUEUE);
#endif
	if (prMsduInfo != NULL) {
		if (kalIsCardRemoved(prAdapter->prGlueInfo) == FALSE) {
#if !CFG_SUPPORT_MULTITHREAD
			/* <2> Acquire LP-OWN if necessary */
			if (*pfgHwAccess == FALSE) {
				*pfgHwAccess = TRUE;

				wlanAcquirePowerControl(prAdapter);
			}
#endif
			/* <3> send packets */
#if CFG_SUPPORT_MULTITHREAD
			nicTxMsduInfoListMthread(prAdapter, prMsduInfo);
#else
			nicTxMsduInfoList(prAdapter, prMsduInfo);
#endif
			/* <4> update TC by txAdjustTcQuotas() */
			nicTxAdjustTcq(prAdapter);
		} else
			wlanProcessQueuedMsduInfo(prAdapter, prMsduInfo);
	}

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to acquire power control from firmware
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanAcquirePowerControl(struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	/* DBGLOG(INIT, INFO, ("Acquire Power Ctrl\n")); */

#if CFG_ENABLE_FULL_PM
	if (nicpmSetDriverOwn(prAdapter) != TRUE)
		return WLAN_STATUS_FAILURE;
#endif

	/* Reset sleepy state */
	if (prAdapter->fgWiFiInSleepyState == TRUE)
		prAdapter->fgWiFiInSleepyState = FALSE;

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to release power control to firmware
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanReleasePowerControl(struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	/* DBGLOG(INIT, INFO, ("Release Power Ctrl\n")); */

	RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is called to report currently pending TX frames count
 *        (command packets are not included)
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return number of pending TX frames
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanGetTxPendingFrameCount(struct ADAPTER *prAdapter)
{
	struct TX_CTRL *prTxCtrl;
	uint32_t u4Num;

	ASSERT(prAdapter);
	prTxCtrl = &prAdapter->rTxCtrl;

	u4Num = kalGetTxPendingFrameCount(prAdapter->prGlueInfo) +
		(uint32_t) GLUE_GET_REF_CNT(
			prTxCtrl->i4PendingFwdFrameCount);

	return u4Num;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to report current ACPI state
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return ACPI_STATE_D0 Normal Operation Mode
 *         ACPI_STATE_D3 Suspend Mode
 */
/*----------------------------------------------------------------------------*/
enum ENUM_ACPI_STATE wlanGetAcpiState(struct ADAPTER *prAdapter)
{
	ASSERT(prAdapter);

	return prAdapter->rAcpiState;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to update current ACPI state only
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param ePowerState    ACPI_STATE_D0 Normal Operation Mode
 *                       ACPI_STATE_D3 Suspend Mode
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanSetAcpiState(struct ADAPTER *prAdapter,
		      enum ENUM_ACPI_STATE ePowerState)
{
	ASSERT(prAdapter);
	ASSERT(ePowerState <= ACPI_STATE_D3);

	prAdapter->rAcpiState = ePowerState;

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to query ECO version from HIFSYS CR
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return zero      Unable to retrieve ECO version information
 *         non-zero  ECO version (1-based)
 */
/*----------------------------------------------------------------------------*/
uint8_t wlanGetEcoVersion(struct ADAPTER *prAdapter)
{
	uint8_t ucEcoVersion;

	ASSERT(prAdapter);

#if CFG_MULTI_ECOVER_SUPPORT
	ucEcoVersion = nicGetChipEcoVer(prAdapter);
	DBGLOG(INIT, TRACE, "%s: %u\n", __func__, ucEcoVersion);
	return ucEcoVersion;
#else
	if (nicVerifyChipID(prAdapter) == TRUE)
		return prAdapter->ucRevID + 1;
	else
		return 0;
#endif
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to query ROM version from HIFSYS CR
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return zero      Unable to retrieve ROM version information
 *         non-zero  ROM version (1-based)
 */
/*----------------------------------------------------------------------------*/
uint8_t wlanGetRomVersion(struct ADAPTER *prAdapter)
{
	uint8_t ucRomVersion;

	ASSERT(prAdapter);

	ucRomVersion = nicGetChipSwVer();
	DBGLOG(INIT, TRACE, "%s: %u\n", __func__, ucRomVersion);
	return ucRomVersion;

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to
 *        set preferred band configuration corresponding to network type
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param eBand          Given band
 * @param ucBssIndex     BSS Info Index
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanSetPreferBandByNetwork(struct ADAPTER *prAdapter,
				enum ENUM_BAND eBand, uint8_t ucBssIndex)
{
	ASSERT(prAdapter);
	ASSERT(eBand <= BAND_NUM);
	ASSERT(ucBssIndex <= prAdapter->ucHwBssIdNum);


	/* 1. set prefer band according to network type */
	prAdapter->aePreferBand[ucBssIndex] = eBand;

	/* 2. remove buffered BSS descriptors correspondingly */
	if (eBand == BAND_2G4)
		scanRemoveBssDescByBandAndNetwork(prAdapter, BAND_5G,
						  ucBssIndex);
	else if (eBand == BAND_5G)
		scanRemoveBssDescByBandAndNetwork(prAdapter, BAND_2G4,
						  ucBssIndex);
#if (CFG_SUPPORT_WIFI_6G == 1)
	else if (eBand == BAND_6G)
		scanRemoveBssDescByBandAndNetwork(prAdapter, BAND_6G,
						  ucBssIndex);
#endif

}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to
 *        get channel information corresponding to specified network type
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param ucBssIndex     BSS Info Index
 *
 * @return channel number
 */
/*----------------------------------------------------------------------------*/
uint8_t wlanGetChannelNumberByNetwork(struct ADAPTER
				      *prAdapter, uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;

	ASSERT(prAdapter);
	ASSERT(ucBssIndex <= prAdapter->ucHwBssIdNum);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prBssInfo) {
		DBGLOG(INIT, ERROR,
			"Invalid bss idx: %d\n",
			ucBssIndex);
		return 1;
	}

	return prBssInfo->ucPrimaryChannel;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to
 *        get band information corresponding to specified network type
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 * @param ucBssIndex     BSS Info Index
 *
 * @return Band index
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanGetBandIndexByNetwork(struct ADAPTER
				      *prAdapter, uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;

	ASSERT(prAdapter);
	ASSERT(ucBssIndex <= prAdapter->ucHwBssIdNum);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (!prBssInfo) {
		DBGLOG(INIT, ERROR,
			"Invalid bss idx: %d\n",
			ucBssIndex);
		return BAND_2G4;
	}

	return prBssInfo->eBand;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to
 *        check unconfigured system properties and generate related message on
 *        scan list to notify users
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanCheckSystemConfiguration(struct ADAPTER
				      *prAdapter)
{
#if (CFG_NVRAM_EXISTENCE_CHECK == 1) || (CFG_SW_NVRAM_VERSION_CHECK == 1)
	const uint8_t aucZeroMacAddr[] = NULL_MAC_ADDR;
	u_int8_t fgIsConfExist = TRUE;
	u_int8_t fgGenErrMsg = FALSE;
	struct REG_INFO *prRegInfo = NULL;
#if 0
	const uint8_t aucBCAddr[] = BC_MAC_ADDR;
	struct WLAN_BEACON_FRAME *prBeacon = NULL;
	struct IE_SSID *prSsid = NULL;
	uint32_t u4ErrCode = 0;
	uint8_t aucErrMsg[32];
	struct PARAM_SSID rSsid;
	struct PARAM_802_11_CONFIG rConfiguration;
	uint8_t rSupportedRates[PARAM_MAX_LEN_RATES_EX];
#endif
#endif

	DEBUGFUNC("wlanCheckSystemConfiguration");

	ASSERT(prAdapter);

#if (CFG_NVRAM_EXISTENCE_CHECK == 1)
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) ==
	    FALSE) {
		fgIsConfExist = FALSE;
		fgGenErrMsg = TRUE;
	}
#endif

#if (CFG_SW_NVRAM_VERSION_CHECK == 1)
	prRegInfo = kalGetConfiguration(prAdapter->prGlueInfo);

	if (prRegInfo == NULL) {
		DBGLOG(INIT, ERROR, "prRegInfo = NULL");
		return WLAN_STATUS_FAILURE;
	}

#if (CFG_SUPPORT_PWR_LIMIT_COUNTRY == 1)
	if (fgIsConfExist == TRUE
	    && (prAdapter->rVerInfo.u2Part1CfgPeerVersion >
		CFG_DRV_OWN_VERSION
		|| prAdapter->rVerInfo.u2Part2CfgPeerVersion >
		CFG_DRV_OWN_VERSION
#if (CFG_DRV_PEER_VERSION > 0)
		|| prAdapter->rVerInfo.u2Part1CfgOwnVersion <
		CFG_DRV_PEER_VERSION
		|| prAdapter->rVerInfo.u2Part2CfgOwnVersion <
		CFG_DRV_PEER_VERSION/* NVRAM */
		|| prAdapter->rVerInfo.u2FwOwnVersion < CFG_DRV_PEER_VERSION
#endif
		|| prAdapter->rVerInfo.u2FwPeerVersion > CFG_DRV_OWN_VERSION
		|| (prAdapter->fgIsEmbbededMacAddrValid == FALSE &&
		    (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
		     || EQUAL_MAC_ADDR(aucZeroMacAddr, prRegInfo->aucMacAddr)))
		|| prAdapter->fgIsPowerLimitTableValid == FALSE))
		fgGenErrMsg = TRUE;
#else
	if (fgIsConfExist == TRUE
	    && (prAdapter->rVerInfo.u2Part1CfgPeerVersion >
		CFG_DRV_OWN_VERSION
		|| prAdapter->rVerInfo.u2Part2CfgPeerVersion >
		CFG_DRV_OWN_VERSION
		|| prAdapter->rVerInfo.u2Part1CfgOwnVersion <
		CFG_DRV_PEER_VERSION
		|| prAdapter->rVerInfo.u2Part2CfgOwnVersion <
		CFG_DRV_PEER_VERSION/* NVRAM */
		|| prAdapter->rVerInfo.u2FwPeerVersion > CFG_DRV_OWN_VERSION
		|| prAdapter->rVerInfo.u2FwOwnVersion < CFG_DRV_PEER_VERSION
		|| (prAdapter->fgIsEmbbededMacAddrValid == FALSE &&
		    (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
		     || EQUAL_MAC_ADDR(aucZeroMacAddr, prRegInfo->aucMacAddr)))
		))
		fgGenErrMsg = TRUE;
#endif
#endif
#if 0/* remove NVRAM WARNING in scan result */
	if (fgGenErrMsg == TRUE) {
		prBeacon = cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
				       sizeof(struct WLAN_BEACON_FRAME) +
				       sizeof(struct IE_SSID));

		/* initialization */
		kalMemZero(prBeacon, sizeof(struct WLAN_BEACON_FRAME) +
			   sizeof(struct IE_SSID));

		/* prBeacon initialization */
		prBeacon->u2FrameCtrl = MAC_FRAME_BEACON;
		COPY_MAC_ADDR(prBeacon->aucDestAddr, aucBCAddr);
		COPY_MAC_ADDR(prBeacon->aucSrcAddr, aucZeroMacAddr);
		COPY_MAC_ADDR(prBeacon->aucBSSID, aucZeroMacAddr);
		prBeacon->u2BeaconInterval = 100;
		prBeacon->u2CapInfo = CAP_INFO_ESS;

		/* prSSID initialization */
		prSsid = (struct IE_SSID *) (&prBeacon->aucInfoElem[0]);
		prSsid->ucId = ELEM_ID_SSID;

		/* rConfiguration initialization */
		rConfiguration.u4Length = sizeof(struct
						 PARAM_802_11_CONFIG);
		rConfiguration.u4BeaconPeriod = 100;
		rConfiguration.u4ATIMWindow = 1;
		rConfiguration.u4DSConfig = 2412;
		rConfiguration.rFHConfig.u4Length = sizeof(
				struct PARAM_802_11_CONFIG_FH);

		/* rSupportedRates initialization */
		kalMemZero(rSupportedRates,
			   (sizeof(uint8_t) * PARAM_MAX_LEN_RATES_EX));
	}
#if (CFG_NVRAM_EXISTENCE_CHECK == 1)
#define NVRAM_ERR_MSG "NVRAM WARNING: Err = 0x01"
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) ==
	    FALSE) {
		COPY_SSID(prSsid->aucSSID, prSsid->ucLength, NVRAM_ERR_MSG,
			  (uint8_t) (strlen(NVRAM_ERR_MSG)));

		kalIndicateBssInfo(prAdapter->prGlueInfo,
				   (uint8_t *) prBeacon,
				   OFFSET_OF(struct WLAN_BEACON_FRAME,
					     aucInfoElem) + OFFSET_OF(
					     struct IE_SSID, aucSSID) +
					     prSsid->ucLength, 1, 0);

		COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, NVRAM_ERR_MSG,
			  strlen(NVRAM_ERR_MSG));
		nicAddScanResult(prAdapter,
				 prBeacon->aucBSSID,
				 &rSsid,
				 0,
				 0,
				 PARAM_NETWORK_TYPE_FH,
				 &rConfiguration,
				 NET_TYPE_INFRA,
				 rSupportedRates,
				 OFFSET_OF(struct WLAN_BEACON_FRAME,
					aucInfoElem) + OFFSET_OF(
					struct IE_SSID, aucSSID) +
					prSsid->ucLength -
					WLAN_MAC_MGMT_HEADER_LEN,
				 (uint8_t *) ((uintptr_t) (prBeacon) +
					WLAN_MAC_MGMT_HEADER_LEN));
	}
#endif

#if (CFG_SW_NVRAM_VERSION_CHECK == 1)
#define VER_ERR_MSG     "NVRAM WARNING: Err = 0x%02X"
	if (fgIsConfExist == TRUE) {
		if ((prAdapter->rVerInfo.u2Part1CfgPeerVersion >
		     CFG_DRV_OWN_VERSION
		     || prAdapter->rVerInfo.u2Part2CfgPeerVersion >
		     CFG_DRV_OWN_VERSION
		     || prAdapter->rVerInfo.u2Part1CfgOwnVersion <
		     CFG_DRV_PEER_VERSION
		     || prAdapter->rVerInfo.u2Part2CfgOwnVersion <
		     CFG_DRV_PEER_VERSION	/* NVRAM */
		     || prAdapter->rVerInfo.u2FwPeerVersion >
			CFG_DRV_OWN_VERSION
		     || prAdapter->rVerInfo.u2FwOwnVersion <
		     CFG_DRV_PEER_VERSION))
			u4ErrCode |= NVRAM_ERROR_VERSION_MISMATCH;

		if (prAdapter->fgIsEmbbededMacAddrValid == FALSE
		    && (IS_BMCAST_MAC_ADDR(prRegInfo->aucMacAddr)
			|| EQUAL_MAC_ADDR(aucZeroMacAddr,
					  prRegInfo->aucMacAddr))) {
			u4ErrCode |= NVRAM_ERROR_INVALID_MAC_ADDR;
		}
#if CFG_SUPPORT_PWR_LIMIT_COUNTRY
		if (prAdapter->fgIsPowerLimitTableValid == FALSE)
			u4ErrCode |= NVRAM_POWER_LIMIT_TABLE_INVALID;
#endif
		if (u4ErrCode != 0) {
			sprintf(aucErrMsg, VER_ERR_MSG,
				(unsigned int)u4ErrCode);
			COPY_SSID(prSsid->aucSSID, prSsid->ucLength, aucErrMsg,
				  (uint8_t) (strlen(aucErrMsg)));

			kalIndicateBssInfo(prAdapter->prGlueInfo,
					   (uint8_t *) prBeacon,
					   OFFSET_OF(struct WLAN_BEACON_FRAME,
						     aucInfoElem) + OFFSET_OF(
						     struct IE_SSID, aucSSID) +
						     prSsid->ucLength, 1, 0);

			COPY_SSID(rSsid.aucSsid, rSsid.u4SsidLen, NVRAM_ERR_MSG,
				  strlen(NVRAM_ERR_MSG));
			nicAddScanResult(prAdapter, prBeacon->aucBSSID, &rSsid,
					 0, 0, PARAM_NETWORK_TYPE_FH,
					 &rConfiguration, NET_TYPE_INFRA,
					 rSupportedRates,
					 OFFSET_OF(struct WLAN_BEACON_FRAME,
						aucInfoElem) +
					 OFFSET_OF(struct IE_SSID,
						aucSSID) + prSsid->ucLength -
						WLAN_MAC_MGMT_HEADER_LEN,
					 (uint8_t *) ((uintptr_t) (prBeacon)
						+ WLAN_MAC_MGMT_HEADER_LEN));
		}
	}
#endif

	if (fgGenErrMsg == TRUE)
		cnmMemFree(prAdapter, prBeacon);
#endif
	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanoidQueryBssStatistics(struct ADAPTER *prAdapter,
			  void *pvQueryBuffer, uint32_t u4QueryBufferLen,
			  uint32_t *pu4QueryInfoLen)
{
	struct PARAM_GET_BSS_STATISTICS *prQueryBssStatistics;
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	uint32_t rResult = WLAN_STATUS_FAILURE;
	uint8_t ucBssIndex;
	enum ENUM_WMM_ACI eAci;

	DEBUGFUNC("wlanoidQueryBssStatistics");

	do {
		ASSERT(pvQueryBuffer);

		/* 4 1. Sanity test */
		if ((prAdapter == NULL) || (pu4QueryInfoLen == NULL))
			break;

		if ((u4QueryBufferLen) && (pvQueryBuffer == NULL))
			break;

		if (u4QueryBufferLen <
		    sizeof(struct PARAM_GET_BSS_STATISTICS *)) {
			*pu4QueryInfoLen =
				sizeof(struct PARAM_GET_BSS_STATISTICS *);
			rResult = WLAN_STATUS_BUFFER_TOO_SHORT;
			break;
		}

		prQueryBssStatistics = (struct PARAM_GET_BSS_STATISTICS *)
				       pvQueryBuffer;
		*pu4QueryInfoLen = sizeof(struct PARAM_GET_BSS_STATISTICS);

		ucBssIndex = prQueryBssStatistics->ucBssIndex;
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

		if (prBssInfo) {	/*AIS*/
			if (prBssInfo->eCurrentOPMode ==
			    OP_MODE_INFRASTRUCTURE) {
			struct WIFI_WMM_AC_STAT *prQueryLss = NULL;
			struct WIFI_WMM_AC_STAT *prStaLss = NULL;
			struct WIFI_WMM_AC_STAT *prBssLss = NULL;

			prQueryLss = prQueryBssStatistics->arLinkStatistics;
			prBssLss = prBssInfo->arLinkStatistics;
			prStaRec = prBssInfo->prStaRecOfAP;
			if (prStaRec) {
				prStaLss = prStaRec->arLinkStatistics;
				for (eAci = 0;
				     eAci < WMM_AC_INDEX_NUM; eAci++) {
				prQueryLss[eAci].u4TxMsdu =
					prStaLss[eAci].u4TxMsdu;
				prQueryLss[eAci].u4RxMsdu =
					prStaLss[eAci].u4RxMsdu;
				prQueryLss[eAci].u4TxDropMsdu =
					prStaLss[eAci].u4TxDropMsdu +
					prBssLss[eAci].u4TxDropMsdu;
				prQueryLss[eAci].u4TxFailMsdu =
					prStaLss[eAci].u4TxFailMsdu;
				prQueryLss[eAci].u4TxRetryMsdu =
					prStaLss[eAci].u4TxRetryMsdu;
				}
			}
			}
			rResult = WLAN_STATUS_SUCCESS;

			/*P2P */
			/* TODO */

			/*BOW*/
			/* TODO */
		}

	} while (FALSE);

	return rResult;

}

void wlanDumpBssStatistics(struct ADAPTER *prAdapter,
			   uint8_t ucBssIdx)
{
	struct BSS_INFO *prBssInfo;
	struct STA_RECORD *prStaRec;
	enum ENUM_WMM_ACI eAci;
	struct WIFI_WMM_AC_STAT arLLStats[WMM_AC_INDEX_NUM];
	uint8_t ucIdx;

	if (ucBssIdx > prAdapter->ucHwBssIdNum) {
		DBGLOG(SW4, INFO, "Invalid BssInfo index[%u], skip dump!\n",
		       ucBssIdx);
		return;
	}

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);
	if (!prBssInfo) {
		DBGLOG(SW4, INFO, "Invalid BssInfo index[%u], skip dump!\n",
		       ucBssIdx);
		return;
	}
	/* <1> fill per-BSS statistics */
#if 0
	/*AIS*/ if (prBssInfo->eCurrentOPMode ==
		    OP_MODE_INFRASTRUCTURE) {
		prStaRec = prBssInfo->prStaRecOfAP;
		if (prStaRec) {
			for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
				prBssInfo->arLinkStatistics[eAci].u4TxMsdu
					= prStaRec->arLinkStatistics[eAci]
					  .u4TxMsdu;
				prBssInfo->arLinkStatistics[eAci].u4RxMsdu
					= prStaRec->arLinkStatistics[eAci]
					  .u4RxMsdu;
				prBssInfo->arLinkStatistics[eAci].u4TxDropMsdu
					+= prStaRec->arLinkStatistics[eAci]
					   .u4TxDropMsdu;
				prBssInfo->arLinkStatistics[eAci].u4TxFailMsdu
					= prStaRec->arLinkStatistics[eAci]
					  .u4TxFailMsdu;
				prBssInfo->arLinkStatistics[eAci].u4TxRetryMsdu
					= prStaRec->arLinkStatistics[eAci]
					  .u4TxRetryMsdu;
			}
		}
	}
#else
	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		arLLStats[eAci].u4TxMsdu =
			prBssInfo->arLinkStatistics[eAci].u4TxMsdu;
		arLLStats[eAci].u4RxMsdu =
			prBssInfo->arLinkStatistics[eAci].u4RxMsdu;
		arLLStats[eAci].u4TxDropMsdu =
			prBssInfo->arLinkStatistics[eAci].u4TxDropMsdu;
		arLLStats[eAci].u4TxFailMsdu =
			prBssInfo->arLinkStatistics[eAci].u4TxFailMsdu;
		arLLStats[eAci].u4TxRetryMsdu =
			prBssInfo->arLinkStatistics[eAci].u4TxRetryMsdu;
	}

	for (ucIdx = 0; ucIdx < CFG_STA_REC_NUM; ucIdx++) {
		prStaRec = cnmGetStaRecByIndex(prAdapter, ucIdx);
		if (!prStaRec)
			continue;
		if (prStaRec->ucBssIndex != ucBssIdx)
			continue;
		/* now the valid sta_rec is valid */
		for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
			arLLStats[eAci].u4TxMsdu +=
				prStaRec->arLinkStatistics[eAci].u4TxMsdu;
			arLLStats[eAci].u4RxMsdu +=
				prStaRec->arLinkStatistics[eAci].u4RxMsdu;
			arLLStats[eAci].u4TxDropMsdu +=
				prStaRec->arLinkStatistics[eAci].u4TxDropMsdu;
			arLLStats[eAci].u4TxFailMsdu +=
				prStaRec->arLinkStatistics[eAci].u4TxFailMsdu;
			arLLStats[eAci].u4TxRetryMsdu +=
				prStaRec->arLinkStatistics[eAci].u4TxRetryMsdu;
		}
	}
#endif

	/* <2>Dump BSS statistics */
	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		DBGLOG(SW4, INFO,
		       "LLS BSS[%u] %s: T[%06u] R[%06u] T_D[%06u] T_F[%06u]\n",
		       prBssInfo->ucBssIndex, apucACI2Str[eAci],
		       arLLStats[eAci].u4TxMsdu,
		       arLLStats[eAci].u4RxMsdu, arLLStats[eAci].u4TxDropMsdu,
		       arLLStats[eAci].u4TxFailMsdu);
	}
}

void wlanDumpAllBssStatistics(struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prBssInfo;
	/* ENUM_WMM_ACI_T eAci; */
	uint32_t ucIdx;

	/* wlanUpdateAllBssStatistics(prAdapter); */

	for (ucIdx = 0; ucIdx < prAdapter->ucHwBssIdNum; ucIdx++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucIdx);
		if (prBssInfo && !IS_BSS_ACTIVE(prBssInfo)) {
			DBGLOG(SW4, TRACE,
			       "Invalid BssInfo index[%u], skip dump!\n",
			       ucIdx);
			continue;
		}

		wlanDumpBssStatistics(prAdapter, ucIdx);
	}
}

uint32_t
wlanoidQueryStaStatistics(struct ADAPTER *prAdapter,
			  void *pvQueryBuffer,
			  uint32_t u4QueryBufferLen,
			  uint32_t *pu4QueryInfoLen)
{
#if CFG_SUPPORT_LINK_QUALITY_MONITOR
	uint8_t ucBssIndex ;

	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);
	if (ucBssIndex == aisGetDefaultLinkBssIndex(prAdapter) &&
			!CHECK_FOR_TIMEOUT(kalGetTimeTick(),
			prAdapter->u4LastLinkQuality,
			SEC_TO_MSEC(CFG_LQ_MONITOR_FREQUENCY))
	) {
		kalMemCopy((struct PARAM_GET_STA_STATISTICS *)pvQueryBuffer,
			   &prAdapter->rQueryStaStatistics,
			   sizeof(struct PARAM_GET_STA_STATISTICS));
		return WLAN_STATUS_SUCCESS;
	} else {
		uint32_t r;

		DBGLOG(REQ, TRACE, "Call, pvQueryBuffer=%p, pu4QueryInfoLen=%p",
			pvQueryBuffer, pu4QueryInfoLen);
		r = wlanQueryStaStatistics(prAdapter, pvQueryBuffer,
				u4QueryBufferLen, pu4QueryInfoLen, TRUE);
		DBGLOG(REQ, TRACE, "r=%u, pvQueryBuffer=%p, pu4QueryInfoLen=%p",
				r, pvQueryBuffer, pu4QueryInfoLen);
		return r;
	}
#else
	return wlanQueryStaStatistics(prAdapter, pvQueryBuffer,
				      u4QueryBufferLen, pu4QueryInfoLen, TRUE);
#endif
}

uint32_t
updateStaStats(struct ADAPTER *prAdapter,
	struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics)
{
	struct STA_RECORD *prStaRec, *prTempStaRec;
	uint8_t ucStaRecIdx;
	struct QUE_MGT *prQM;
	uint8_t ucIdx;
	enum ENUM_WMM_ACI eAci;

	prQM = &prAdapter->rQM;
	/* 4 5. Get driver global QM counter */
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++) {
		prQueryStaStatistics->au4TcAverageQueLen[ucIdx] =
			prQM->au4AverageQueLen[ucIdx];
		prQueryStaStatistics->au4TcCurrentQueLen[ucIdx] =
			prQM->au4CurrentTcResource[ucIdx];
	}
#endif

	/* 4 2. Get StaRec by MAC address */
	prStaRec = NULL;

	for (ucStaRecIdx = 0; ucStaRecIdx < CFG_STA_REC_NUM;
	     ucStaRecIdx++) {
		prTempStaRec = &(prAdapter->arStaRec[ucStaRecIdx]);
		if (prTempStaRec->fgIsValid &&
		    prTempStaRec->fgIsInUse) {
			if (EQUAL_MAC_ADDR(prTempStaRec->aucMacAddr,
			    prQueryStaStatistics->aucMacAddr)) {
				prStaRec = prTempStaRec;
				break;
			}
		}
	}

	if (!prStaRec)
		return WLAN_STATUS_INVALID_DATA;

	prQueryStaStatistics->u4Flag |= BIT(0);

#if CFG_ENABLE_PER_STA_STATISTICS
	/* 4 3. Get driver statistics */
	prQueryStaStatistics->u4TxTotalCount =
		prStaRec->u4TotalTxPktsNumber;
	prQueryStaStatistics->u4RxTotalCount =
		prStaRec->u4TotalRxPktsNumber;
	prQueryStaStatistics->u4TxExceedThresholdCount =
		prStaRec->u4ThresholdCounter;
	prQueryStaStatistics->u4TxMaxTime =
		prStaRec->u4MaxTxPktsTime;
	prQueryStaStatistics->u4TxMaxHifTime =
		prStaRec->u4MaxTxPktsHifTime;

	if (prStaRec->u4TotalTxPktsNumber) {
		prQueryStaStatistics->u4TxAverageProcessTime =
			(prStaRec->u4TotalTxPktsTime /
			 prStaRec->u4TotalTxPktsNumber);
		prQueryStaStatistics->u4TxAverageHifTime =
			prStaRec->u4TotalTxPktsHifTxTime /
			prStaRec->u4TotalTxPktsNumber;
	} else
		prQueryStaStatistics->u4TxAverageProcessTime = 0;

	/*link layer statistics */
	for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
		prQueryStaStatistics->arLinkStatistics[eAci].u4TxMsdu =
			prStaRec->arLinkStatistics[eAci].u4TxMsdu;
		prQueryStaStatistics->arLinkStatistics[eAci].u4RxMsdu =
			prStaRec->arLinkStatistics[eAci].u4RxMsdu;
		prQueryStaStatistics->arLinkStatistics[
			eAci].u4TxDropMsdu =
			prStaRec->arLinkStatistics[eAci].u4TxDropMsdu;
	}

	for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++) {
		prQueryStaStatistics->au4TcResourceEmptyCount[ucIdx] =
			prQM->au4QmTcResourceEmptyCounter[
			prStaRec->ucBssIndex][ucIdx];
		/* Reset */
		prQM->au4QmTcResourceEmptyCounter[
			prStaRec->ucBssIndex][ucIdx] = 0;
		prQueryStaStatistics->au4TcResourceBackCount[ucIdx] =
			prQM->au4QmTcResourceBackCounter[ucIdx];
		prQM->au4QmTcResourceBackCounter[ucIdx] = 0;
		prQueryStaStatistics->au4DequeueNoTcResource[ucIdx]
			= prQM->au4DequeueNoTcResourceCounter[ucIdx];
		prQM->au4DequeueNoTcResourceCounter[ucIdx] = 0;
		prQueryStaStatistics->au4TcResourceUsedPageCount[ucIdx]
			= prQM->au4QmTcUsedPageCounter[ucIdx];
		prQM->au4QmTcUsedPageCounter[ucIdx] = 0;
		prQueryStaStatistics->au4TcResourceWantedPageCount[
			ucIdx] = prQM->au4QmTcWantedPageCounter[ucIdx];
		prQM->au4QmTcWantedPageCounter[ucIdx] = 0;
	}

	prQueryStaStatistics->u4EnqueueCounter =
		prQM->u4EnqueueCounter;
	prQueryStaStatistics->u4EnqueueStaCounter =
		prStaRec->u4EnqueueCounter;

	prQueryStaStatistics->u4DequeueCounter =
		prQM->u4DequeueCounter;
	prQueryStaStatistics->u4DequeueStaCounter =
		prStaRec->u4DeqeueuCounter;

	prQueryStaStatistics->IsrCnt =
		prAdapter->prGlueInfo->IsrCnt;
	prQueryStaStatistics->IsrPassCnt =
		prAdapter->prGlueInfo->IsrPassCnt;
	prQueryStaStatistics->TaskIsrCnt =
		prAdapter->prGlueInfo->TaskIsrCnt;

	prQueryStaStatistics->IsrAbnormalCnt =
		prAdapter->prGlueInfo->IsrAbnormalCnt;
	prQueryStaStatistics->IsrSoftWareCnt =
		prAdapter->prGlueInfo->IsrSoftWareCnt;
	prQueryStaStatistics->IsrRxCnt =
		prAdapter->prGlueInfo->IsrRxCnt;
	prQueryStaStatistics->IsrTxCnt =
		prAdapter->prGlueInfo->IsrTxCnt;

	/* 4 4.1 Reset statistics */
	if (prQueryStaStatistics->ucReadClear) {
		prStaRec->u4ThresholdCounter = 0;
		prStaRec->u4TotalTxPktsNumber = 0;
		prStaRec->u4TotalTxPktsHifTxTime = 0;

		prStaRec->u4TotalTxPktsTime = 0;
		prStaRec->u4TotalRxPktsNumber = 0;
		prStaRec->u4MaxTxPktsTime = 0;
		prStaRec->u4MaxTxPktsHifTime = 0;
		prQM->u4EnqueueCounter = 0;
		prQM->u4DequeueCounter = 0;
		prStaRec->u4EnqueueCounter = 0;
		prStaRec->u4DeqeueuCounter = 0;

		prAdapter->prGlueInfo->IsrCnt = 0;
		prAdapter->prGlueInfo->IsrPassCnt = 0;
		prAdapter->prGlueInfo->TaskIsrCnt = 0;

		prAdapter->prGlueInfo->IsrAbnormalCnt = 0;
		prAdapter->prGlueInfo->IsrSoftWareCnt = 0;
		prAdapter->prGlueInfo->IsrRxCnt = 0;
		prAdapter->prGlueInfo->IsrTxCnt = 0;
	}
	/*link layer statistics */
	if (prQueryStaStatistics->ucLlsReadClear) {
		for (eAci = 0; eAci < WMM_AC_INDEX_NUM; eAci++) {
			prStaRec->arLinkStatistics[eAci].u4TxMsdu = 0;
			prStaRec->arLinkStatistics[eAci].u4RxMsdu = 0;
			prStaRec->arLinkStatistics[eAci].u4TxDropMsdu
								  = 0;
		}
	}
#endif

	for (ucIdx = TC0_INDEX; ucIdx <= TC3_INDEX; ucIdx++)
		prQueryStaStatistics->au4TcQueLen[ucIdx] =
			prStaRec->aprTargetQueue[ucIdx]->u4NumElem;

	return WLAN_STATUS_SUCCESS;
}

uint32_t
wlanQueryStaStatistics(struct ADAPTER *prAdapter,
		       void *pvQueryBuffer,
		       uint32_t u4QueryBufferLen,
		       uint32_t *pu4QueryInfoLen,
		       u_int8_t fgIsOid)
{
	uint8_t ucStaRecIdx;
	uint32_t rResult = WLAN_STATUS_FAILURE;
	struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics;
	struct CMD_GET_STA_STATISTICS rQueryCmdStaStatistics = {0};


	DEBUGFUNC("wlanoidQueryStaStatistics");

	if (prAdapter == NULL)
		return WLAN_STATUS_FAILURE;

	if (prAdapter->fgIsEnableLpdvt)
		return WLAN_STATUS_NOT_SUPPORTED;

	do {
		struct STA_RECORD *prStaRec, *prTempStaRec;

		prStaRec = NULL;

		ASSERT(pvQueryBuffer);

		/* 4 1. Sanity test */
		if (pu4QueryInfoLen == NULL)
			break;

		if ((u4QueryBufferLen) && (pvQueryBuffer == NULL))
			break;

		if (u4QueryBufferLen <
		    sizeof(struct PARAM_GET_STA_STATISTICS)) {
			*pu4QueryInfoLen =
					sizeof(struct PARAM_GET_STA_STATISTICS);
			rResult = WLAN_STATUS_BUFFER_TOO_SHORT;
			break;
		}

		prQueryStaStatistics = (struct PARAM_GET_STA_STATISTICS *)
				       pvQueryBuffer;
		*pu4QueryInfoLen = sizeof(struct PARAM_GET_STA_STATISTICS);

		rResult = updateStaStats(prAdapter, prQueryStaStatistics);
		if (rResult != WLAN_STATUS_SUCCESS)
			break;

		for (ucStaRecIdx = 0; ucStaRecIdx < CFG_STA_REC_NUM;
		     ucStaRecIdx++) {
			prTempStaRec = &(prAdapter->arStaRec[ucStaRecIdx]);
			if (prTempStaRec->fgIsValid &&
			    prTempStaRec->fgIsInUse) {
				if (EQUAL_MAC_ADDR(prTempStaRec->aucMacAddr,
				    prQueryStaStatistics->aucMacAddr)) {
					prStaRec = prTempStaRec;
					break;
				}
			}
		}

		if (!prStaRec)
			return WLAN_STATUS_INVALID_DATA;

		/* 4 6. Ensure FW supports get station link status */
		rQueryCmdStaStatistics.ucIndex = prStaRec->ucIndex;
		COPY_MAC_ADDR(rQueryCmdStaStatistics.aucMacAddr,
			      prQueryStaStatistics->aucMacAddr);
		rQueryCmdStaStatistics.ucReadClear =
			prQueryStaStatistics->ucReadClear;
		rQueryCmdStaStatistics.ucLlsReadClear =
			prQueryStaStatistics->ucLlsReadClear;
		rQueryCmdStaStatistics.ucResetCounter =
			prQueryStaStatistics->ucResetCounter;

		DBGLOG(REQ, TRACE, "Call fgIsOid=%u, pvQueryBuffer=%p",
				fgIsOid, pvQueryBuffer);
		rResult = wlanSendSetQueryCmd(prAdapter,
				      CMD_ID_GET_STA_STATISTICS,
				      FALSE,
				      TRUE,
				      fgIsOid,
				      nicCmdEventQueryStaStatistics,
				      nicOidCmdTimeoutCommon,
				      sizeof(struct CMD_GET_STA_STATISTICS),
				      (uint8_t *)&rQueryCmdStaStatistics,
				      pvQueryBuffer, u4QueryBufferLen);
		DBGLOG(REQ, TRACE, "rResult=%u, fgIsOid=%u, pvQueryBuffer=%p",
				rResult, fgIsOid, pvQueryBuffer);

		if ((!fgIsOid) && (rResult == WLAN_STATUS_PENDING))
			rResult = WLAN_STATUS_SUCCESS;

		prQueryStaStatistics->u4Flag |= BIT(1);

	} while (FALSE);

	return rResult;
}				/* wlanoidQueryP2pVersion */

uint32_t
wlanQueryStatistics(struct ADAPTER *prAdapter,
		       void *pvQueryBuffer, uint32_t u4QueryBufferLen,
		       uint32_t *pu4QueryInfoLen, uint8_t fgIsOid)
{
	struct CMD_QUERY_STATISTICS rQueryCmdStatistics = {0};

	DEBUGFUNC("wlanQueryStatistics");

	ASSERT(prAdapter);
	if (u4QueryBufferLen)
		ASSERT(pvQueryBuffer);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(struct PARAM_802_11_STATISTICS_STRUCT);

	if (prAdapter->rAcpiState == ACPI_STATE_D3) {
		DBGLOG(REQ, WARN,
		       "Fail in query receive error! (Adapter not ready). ACPI=D%d, Radio=%d\n",
		       prAdapter->rAcpiState, prAdapter->fgIsRadioOff);
		*pu4QueryInfoLen = sizeof(uint32_t);
		return WLAN_STATUS_ADAPTER_NOT_READY;
	} else if (u4QueryBufferLen < sizeof(struct
					     PARAM_802_11_STATISTICS_STRUCT)) {
		DBGLOG(REQ, WARN, "Too short length %u\n",
		       u4QueryBufferLen);
		return WLAN_STATUS_INVALID_LENGTH;
	}
#if CFG_ENABLE_STATISTICS_BUFFERING
	if (IsBufferedStatisticsUsable(prAdapter) == TRUE) {
		struct PARAM_802_11_STATISTICS_STRUCT *prStatistics;

		*pu4QueryInfoLen = sizeof(struct
					  PARAM_802_11_STATISTICS_STRUCT);
		prStatistics = (struct PARAM_802_11_STATISTICS_STRUCT *)
			       pvQueryBuffer;

		prStatistics->u4Length = sizeof(struct
						PARAM_802_11_STATISTICS_STRUCT);
		prStatistics->rTransmittedFragmentCount =
			prAdapter->rStatStruct.rTransmittedFragmentCount;
		prStatistics->rMulticastTransmittedFrameCount =
			prAdapter->rStatStruct.rMulticastTransmittedFrameCount;
		prStatistics->rFailedCount =
			prAdapter->rStatStruct.rFailedCount;
		prStatistics->rRetryCount =
			prAdapter->rStatStruct.rRetryCount;
		prStatistics->rMultipleRetryCount =
			prAdapter->rStatStruct.rMultipleRetryCount;
		prStatistics->rRTSSuccessCount =
			prAdapter->rStatStruct.rRTSSuccessCount;
		prStatistics->rRTSFailureCount =
			prAdapter->rStatStruct.rRTSFailureCount;
		prStatistics->rACKFailureCount =
			prAdapter->rStatStruct.rACKFailureCount;
		prStatistics->rFrameDuplicateCount =
			prAdapter->rStatStruct.rFrameDuplicateCount;
		prStatistics->rReceivedFragmentCount =
			prAdapter->rStatStruct.rReceivedFragmentCount;
		prStatistics->rMulticastReceivedFrameCount =
			prAdapter->rStatStruct.rMulticastReceivedFrameCount;
		prStatistics->rFCSErrorCount =
			prAdapter->rStatStruct.rFCSErrorCount;
		prStatistics->rTKIPLocalMICFailures.QuadPart = 0;
		prStatistics->rTKIPICVErrors.QuadPart = 0;
		prStatistics->rTKIPCounterMeasuresInvoked.QuadPart = 0;
		prStatistics->rTKIPReplays.QuadPart = 0;
		prStatistics->rCCMPFormatErrors.QuadPart = 0;
		prStatistics->rCCMPReplays.QuadPart = 0;
		prStatistics->rCCMPDecryptErrors.QuadPart = 0;
		prStatistics->rFourWayHandshakeFailures.QuadPart = 0;
		prStatistics->rWEPUndecryptableCount.QuadPart = 0;
		prStatistics->rWEPICVErrorCount.QuadPart = 0;
		prStatistics->rDecryptSuccessCount.QuadPart = 0;
		prStatistics->rDecryptFailureCount.QuadPart = 0;

		return WLAN_STATUS_SUCCESS;
	}
#endif

	kalMemZero(&rQueryCmdStatistics,
		   sizeof(struct CMD_QUERY_STATISTICS));
	rQueryCmdStatistics.ucBssIndex = aisGetDefaultLinkBssIndex(prAdapter);

	return wlanSendSetQueryCmd(prAdapter,
				CMD_ID_GET_STATISTICS,
				FALSE,
				TRUE,
				fgIsOid,
				nicCmdEventQueryStatistics,
				nicOidCmdTimeoutCommon,
				sizeof(struct CMD_QUERY_STATISTICS),
				(uint8_t *)&rQueryCmdStatistics,
				pvQueryBuffer, u4QueryBufferLen);

} /* wlanQueryStatistics */

#if (CFG_SUPPORT_STATS_ONE_CMD == 1)
uint32_t
wlanQueryStatsOneCmd(struct ADAPTER *prAdapter,
		       void *pvQueryBuffer, uint32_t u4QueryBufferLen,
		       uint32_t *pu4QueryInfoLen, uint8_t fgIsOid)
{
	uint32_t rResult = WLAN_STATUS_SUCCESS;
	struct STA_RECORD *prStaRec, *prTempStaRec;
	uint8_t ucStaRecIdx;
	uint8_t i, ucBssIndex;
	struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics;
	struct UNI_CMD_GET_STATISTICS *uni_cmd;
	struct UNI_CMD_BASIC_STATISTICS *basicStatsTag;
	struct UNI_CMD_LINK_QUALITY *lQTag;
	struct UNI_CMD_STA_STATISTICS *staStatsTag;
	struct UNI_CMD_LINK_LAYER_STATS *llsTag;
	uint8_t *buf;
	uint32_t max_cmd_len;
	struct BSS_INFO *prBssInfo;
	uint8_t ucConnBss[MAX_BSSID_NUM] = {0};
	struct LINK_SPEED_EX_ *prLq;

	ucBssIndex = GET_IOCTL_BSSIDX(prAdapter);
	if (unlikely(ucBssIndex >= BSSID_NUM))
		return WLAN_STATUS_INVALID_DATA;

	DEBUGFUNC("wlanQueryStatsOneCmd");
	DBGLOG(NIC, TRACE, "lastAllStatsUpdateTime:%u\n",
		prAdapter->rAllStatsUpdateTime);

	/* caller should get from cache directly */
	/* basic_stats: prAdapter->rStat */
	/* linkQuality: prAdapter->rLinkQuality.rLq[ucBssIndex] */
	/* staStats: prAdapter->rQueryStaStatistics[ucBssIndex] */

	prLq = &prAdapter->rLinkQuality.rLq[ucBssIndex];
	DBGLOG(NIC, TRACE, "bssIdx:%u curTime:%u LRValid:%u\n",
		ucBssIndex, kalGetTimeTick(),
		prLq->fgIsLinkRateValid);
	if (prLq->fgIsLinkRateValid &&
		!CHECK_FOR_TIMEOUT(kalGetTimeTick(),
			prAdapter->rAllStatsUpdateTime,
			SEC_TO_MSEC(CFG_LQ_MONITOR_FREQUENCY)))
		return rResult;

	prAdapter->rAllStatsUpdateTime = kalGetTimeTick();

	/* prepare staStats driver stuff */
	max_cmd_len = sizeof(struct UNI_CMD_GET_STATISTICS) +
		sizeof(struct UNI_CMD_BASIC_STATISTICS) +
		sizeof(struct UNI_CMD_LINK_QUALITY) +
		sizeof(struct UNI_CMD_LINK_LAYER_STATS);

	for (i = 0; i < MAX_BSSID_NUM; i++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, i);
		if (!prBssInfo || !IS_BSS_AIS(prBssInfo) ||
			kalGetMediaStateIndicated(prAdapter->prGlueInfo, i) !=
				MEDIA_STATE_CONNECTED)
			continue;

		prQueryStaStatistics = &prAdapter->rQueryStaStatistics[i];
		COPY_MAC_ADDR(prQueryStaStatistics->aucMacAddr,
			prBssInfo->aucBSSID);

		rResult = updateStaStats(prAdapter, prQueryStaStatistics);

		if (rResult != WLAN_STATUS_SUCCESS)
			continue;

		ucConnBss[i] = 1;
		max_cmd_len += sizeof(struct UNI_CMD_STA_STATISTICS);
	}


	DBGLOG(REQ, TRACE, "Call pvQueryBuffer=%p",
			pvQueryBuffer);

	uni_cmd = (struct UNI_CMD_GET_STATISTICS *) cnmMemAlloc(
			prAdapter,
			RAM_TYPE_MSG, max_cmd_len);
	if (!uni_cmd) {
		DBGLOG(INIT, ERROR,
		       "Allocate UNI_CMD_GET_STATISTICS ==> FAILED.\n");
		return WLAN_STATUS_FAILURE;
	}

	/* prepare unified cmd tags */
	buf = uni_cmd->aucTlvBuffer;

	/* UNI_CMD_GET_STATISTICS_TAG_BASIC */
	basicStatsTag = (struct UNI_CMD_BASIC_STATISTICS *) buf;
	basicStatsTag->u2Tag = UNI_CMD_GET_STATISTICS_TAG_BASIC;
	basicStatsTag->u2Length = sizeof(*basicStatsTag);
	buf += sizeof(*basicStatsTag);

	/* UNI_CMD_GET_STATISTICS_TAG_LINK_QUALITY */
	lQTag = (struct UNI_CMD_LINK_QUALITY *) buf;
	lQTag->u2Tag = UNI_CMD_GET_STATISTICS_TAG_LINK_QUALITY;
	lQTag->u2Length = sizeof(*lQTag);
	buf += sizeof(*lQTag);

	/* UNI_CMD_GET_STATISTICS_TAG_STA for connected AIS BSS */
	for (i = 0; i < MAX_BSSID_NUM; i++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, i);
		if (!prBssInfo || !IS_BSS_AIS(prBssInfo) ||
			kalGetMediaStateIndicated(prAdapter->prGlueInfo, i) !=
				MEDIA_STATE_CONNECTED
			|| ucConnBss[i] == 0)
			continue;

		prQueryStaStatistics = &prAdapter->rQueryStaStatistics[i];
		for (ucStaRecIdx = 0; ucStaRecIdx < CFG_STA_REC_NUM;
			ucStaRecIdx++) {
			prTempStaRec = &(prAdapter->arStaRec[ucStaRecIdx]);
			if (prTempStaRec->fgIsValid &&
			    prTempStaRec->fgIsInUse) {
				if (EQUAL_MAC_ADDR(prTempStaRec->aucMacAddr,
				    prQueryStaStatistics->aucMacAddr)) {
					prStaRec = prTempStaRec;
					break;
				}
			}
		}
		if (!prStaRec)
			continue;

		staStatsTag = (struct UNI_CMD_STA_STATISTICS *) buf;
		staStatsTag->u2Tag = UNI_CMD_GET_STATISTICS_TAG_STA;
		staStatsTag->u2Length = sizeof(*staStatsTag);
		/* FW starec idx is WTBL idx */
		staStatsTag->u1Index = prStaRec->ucWlanIndex;
		staStatsTag->ucReadClear =
			prQueryStaStatistics->ucReadClear;
		staStatsTag->ucLlsReadClear =
			prQueryStaStatistics->ucLlsReadClear;
		staStatsTag->ucResetCounter =
			prQueryStaStatistics->ucResetCounter;
		buf += sizeof(*staStatsTag);
	}

	/* UNI_CMD_GET_STATISTICS_TAG_LINK_LAYER_STATS */
	llsTag = (struct UNI_CMD_LINK_LAYER_STATS *) buf;
	llsTag->u2Tag = UNI_CMD_GET_STATISTICS_TAG_LINK_LAYER_STATS;
	llsTag->u2Length = sizeof(*llsTag);

	rResult = wlanSendSetQueryUniCmd(prAdapter,
			      UNI_CMD_ID_GET_STATISTICS,
			      FALSE,
			      TRUE,
			      fgIsOid,
			      nicUniEventAllStatsOneCmd,
			      nicUniCmdTimeoutCommon,
			      max_cmd_len,
			      (void *)uni_cmd,
			      pvQueryBuffer, u4QueryBufferLen);
	DBGLOG(REQ, TRACE, "rResult=%u, pvQueryBuffer=%p",
			rResult, pvQueryBuffer);
	cnmMemFree(prAdapter, uni_cmd);

	for (i = 0; i < MAX_BSSID_NUM; i++) {
		if (ucConnBss[i]) {
			prQueryStaStatistics = (
				&prAdapter->rQueryStaStatistics[i]);
			prQueryStaStatistics->u4Flag |= BIT(1);
		}
	}
	return rResult;

}
#endif

/**
 * Called to save STATS_LLS_BSSLOAD_INFO information on scan operation.
 * Update slot by bit mask in fgIsConnected.
 */
void updateLinkStatsApRec(struct ADAPTER *prAdapter, struct BSS_DESC *prBssDesc)
{
#if CFG_SUPPORT_LLS
	struct STATS_LLS_PEER_AP_REC *prPeerApRec;
	int32_t i = 0;

	if (!prBssDesc->fgIsConnected)
		return;

	for (i = 0; i < KAL_AIS_NUM; i++) {
		if ((prBssDesc->fgIsConnected >> i) & 0x1)
			break;
	}
	if (i == KAL_AIS_NUM) {
		DBGLOG(REQ, WARN, "AP connected flag set %u over limit", i);
		return;
	}

	prPeerApRec = &prAdapter->rPeerApRec[i];
	COPY_MAC_ADDR(prPeerApRec->mac_addr, prBssDesc->aucBSSID);
	prPeerApRec->sta_count = prBssDesc->u2StaCnt;
	prPeerApRec->chan_util = prBssDesc->ucChnlUtilization;

	if (prAdapter->rWifiVar.fgLinkStatsDump)
		DBGLOG(REQ, INFO, "Update record %u: " MACSTR " %u %u",
				i, MAC2STR(prBssDesc->aucBSSID),
				prBssDesc->u2StaCnt,
				prBssDesc->ucChnlUtilization);
#endif
}

#if CFG_SUPPORT_LLS
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to get LLS data from FW
 *
 * \param[in]  prAdapter        A pointer to the Adapter structure.
 * \param[in]  pvQueryBuffer    Buffer holding query command, also used to hold
 *                              the returned result if any
 * \param[in]  u4QueryBufferLen The size of query command buffer
 * \param[out] pu4QueryInfoLen  Pointer to integer to pass returned size copied
 *                              to return buffer
 *
 * \retval WLAN_STATUS_SUCCESS: Send command success
 *         WLAN_STATUS_FAILURE: Send command failed
 *         WLAN_STATUS_PENDING: Pending for waiting event
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanQueryLinkStats(struct ADAPTER *prAdapter,
		void *pvQueryBuffer, uint32_t u4QueryBufferLen,
		uint32_t *pu4QueryInfoLen)
{
	struct CMD_GET_STATS_LLS rQuery;
	struct CMD_GET_STATS_LLS *cmd =
		(struct CMD_GET_STATS_LLS *)pvQueryBuffer;

	DBGLOG(REQ, TRACE, "cmd: u4Tag=%08x, args=%u/%u/%u/%u, len=%u",
			cmd->u4Tag, cmd->ucArg0, cmd->ucArg1,
			cmd->ucArg2, cmd->ucArg3, *pu4QueryInfoLen);
	rQuery = *cmd;

#if CFG_MTK_MDDP_SUPPORT && CFG_SUPPORT_LLS_MDDP
	if (cmd->u4Tag == STATS_LLS_TAG_LLS_DATA)
		mddpGetMdLlsStats(prAdapter);
#endif

	return wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
			    CMD_ID_GET_STATS_LLS,	/* ucCID */
			    FALSE,	/* fgSetQuery */
			    TRUE,	/* fgNeedResp */
			    TRUE,	/* fgIsOid */
			    nicCmdEventQueryLinkStats,    /* pfCmdDoneHandler */
			    nicOidCmdTimeoutCommon, /* pfCmdTimeoutHandler */
			    *pu4QueryInfoLen,    /* u4SetQueryInfoLen */
			    (uint8_t *)&rQuery,  /* pucInfoBuffer */
			    pvQueryBuffer,       /* pvSetQueryBuffer */
			    u4QueryBufferLen);   /* u4SetQueryBufferLen */
}
#endif


/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to query Nic resource information
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
void wlanQueryNicResourceInformation(struct ADAPTER *prAdapter)
{
	/* 3 1. Get Nic resource information from FW */

	/* 3 2. Setup resource parameter */

	/* 3 3. Reset Tx resource */
	nicTxResetResource(prAdapter);
}

#ifndef CFG_SUPPORT_UNIFIED_COMMAND
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to query Nic resource information
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanQueryNicCapabilityV2(struct ADAPTER *prAdapter)
{
	uint32_t u4RxPktLength;
	uint8_t *prEventBuff;
	struct WIFI_EVENT *prEvent;
	struct mt66xx_chip_info *prChipInfo;
	uint32_t chip_id;
	uint32_t u4Time;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;
	chip_id = prChipInfo->chip_id;

	ASSERT(prAdapter);

	/* Get Nic resource information from FW */
	if (!prChipInfo->isNicCapV1
	    || (prAdapter->u4FwFeatureFlag0 &
		FEATURE_FLAG0_NIC_CAPABILITY_V2)) {

		DBGLOG(INIT, INFO, "Support NIC_CAPABILITY_V2 feature\n");

		wlanSendSetQueryCmdAdv(
			prAdapter, CMD_ID_GET_NIC_CAPABILITY_V2, 0, FALSE,
			TRUE, FALSE, NULL, NULL, 0, NULL, NULL, 0,
			CMD_SEND_METHOD_REQ_RESOURCE);

		/*
		 * receive nic_capability_v2 event
		 */

		/* allocate event buffer */
		prEventBuff = cnmMemAlloc(prAdapter, RAM_TYPE_BUF,
					  CFG_RX_MAX_PKT_SIZE);
		if (!prEventBuff) {
			DBGLOG(INIT, WARN, "%s: event buffer alloc failed!\n",
			       __func__);
			return WLAN_STATUS_FAILURE;
		}

		/* get event */
		u4Time = kalGetTimeTick();
		while (TRUE) {
			if (nicRxWaitResponse(prAdapter,
					      1,
					      prEventBuff,
					      CFG_RX_MAX_PKT_SIZE,
					      &u4RxPktLength)
			    != WLAN_STATUS_SUCCESS) {
				DBGLOG(INIT, WARN,
				       "%s: wait for event failed!\n",
				       __func__);

				/* free event buffer */
				cnmMemFree(prAdapter, prEventBuff);

				return WLAN_STATUS_FAILURE;
			}

			if (CHECK_FOR_TIMEOUT(kalGetTimeTick(), u4Time,
				MSEC_TO_SYSTIME(3000))) {
				DBGLOG(HAL, ERROR,
					"Query nic capability timeout\n");
				return WLAN_STATUS_FAILURE;
			}

			/* header checking .. */
			if ((NIC_RX_GET_U2_SW_PKT_TYPE(prEventBuff) &
				prChipInfo->u2RxSwPktBitMap) !=
				prChipInfo->u2RxSwPktEvent) {
				DBGLOG(INIT, WARN,
				       "%s: skip unexpected Rx pkt type[0x%04x]\n",
				       __func__,
				       NIC_RX_GET_U2_SW_PKT_TYPE(prEventBuff));

				continue;
			}

			prEvent = (struct WIFI_EVENT *)
				(prEventBuff + prChipInfo->rxd_size);
			if (prEvent->ucEID != EVENT_ID_NIC_CAPABILITY_V2) {
				DBGLOG(INIT, WARN,
				       "%s: skip unexpected event ID[0x%02x]\n",
				       __func__, prEvent->ucEID);

				continue;
			} else {
				/* hit */
				break;
			}

		}

		/*
		 * parsing elemens
		 */

		nicCmdEventQueryNicCapabilityV2(prAdapter,
						prEvent->aucBuffer);

		/*
		 * free event buffer
		 */
		cnmMemFree(prAdapter, prEventBuff);
	}

	/* Fill capability for different Chip version */
	if (chip_id == HQA_CHIP_ID_6632) {
		/* 6632 only */
		prAdapter->fgIsSupportBufferBinSize16Byte = TRUE;
		prAdapter->fgIsSupportDelayCal = FALSE;
		prAdapter->fgIsSupportGetFreeEfuseBlockCount = FALSE;
		prAdapter->fgIsSupportQAAccessEfuse = FALSE;
		prAdapter->fgIsSupportPowerOnSendBufferModeCMD = FALSE;
		prAdapter->fgIsSupportGetTxPower = FALSE;
	} else {
		prAdapter->fgIsSupportBufferBinSize16Byte = FALSE;
		prAdapter->fgIsSupportDelayCal = TRUE;
		prAdapter->fgIsSupportGetFreeEfuseBlockCount = TRUE;
		prAdapter->fgIsSupportQAAccessEfuse = TRUE;
		prAdapter->fgIsSupportPowerOnSendBufferModeCMD = TRUE;
		prAdapter->fgIsSupportGetTxPower = TRUE;
	}

	return WLAN_STATUS_SUCCESS;
}
#endif

void wlanSetNicResourceParameters(struct ADAPTER
				  *prAdapter)
{
	uint8_t string[128], idx;
	uint32_t u4share;
	uint32_t u4MaxDataPageCntPerFrame =
		prAdapter->rTxCtrl.u4MaxDataPageCntPerFrame;
	uint32_t u4MaxCmdPageCntPerFrame =
		prAdapter->rTxCtrl.u4MaxCmdPageCntPerFrame;
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	struct QUE_MGT *prQM = &prAdapter->rQM;
#endif

	/*
	 * Use the settings in config file first,
	 * else, use the settings reported from firmware.
	 */


	/*
	 * 1. assign free page count for each TC
	 */

	/* 1 1. update free page count in TC control: MCU and LMAC */
	prWifiVar->au4TcPageCount[TC4_INDEX] =
		prAdapter->nicTxReousrce.u4CmdTotalResource *
		u4MaxCmdPageCntPerFrame;	 /* MCU */

	u4share = prAdapter->nicTxReousrce.u4DataTotalResource /
		  (TC_NUM - 1); /* LMAC. Except TC_4, which is MCU */
	for (idx = TC0_INDEX; idx < TC_NUM; idx++) {
		if (idx != TC4_INDEX)
			prWifiVar->au4TcPageCount[idx] =
				u4share * u4MaxDataPageCntPerFrame;
	}

	/* 1 2. if there is remaings, give them to TC_3, which is VO */
	prWifiVar->au4TcPageCount[TC3_INDEX] +=
		(prAdapter->nicTxReousrce.u4DataTotalResource %
		 (TC_NUM - 1)) * u4MaxDataPageCntPerFrame;

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	/*
	 * 2. assign guaranteed page count for each TC
	 */

	/* 2 1. update guaranteed page count in QM */
	for (idx = 0; idx < TC_NUM; idx++)
		prQM->au4GuaranteedTcResource[idx] =
			prWifiVar->au4TcPageCount[idx];
#endif


#if CFG_SUPPORT_CFG_FILE
	/*
	 * 3. Use the settings in config file first,
	 *    else, use the settings reported from firmware.
	 */

	/* 3 1. update for free page count */
	for (idx = 0; idx < TC_NUM; idx++) {

		/* construct prefix: Tc0Page, Tc1Page... */
		memset(string, 0, sizeof(string) / sizeof(uint8_t));
		kalSnprintf(string, sizeof(string) / sizeof(uint8_t),
			 "Tc%xPage", idx);

		/* update the final value */
		prWifiVar->au4TcPageCount[idx] =
			(uint32_t) wlanCfgGetUint32(prAdapter, string,
						prWifiVar->au4TcPageCount[idx]);
	}

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	/* 3 2. update for guaranteed page count */
	for (idx = 0; idx < TC_NUM; idx++) {

		/* construct prefix: Tc0Grt, Tc1Grt... */
		memset(string, 0, sizeof(string) / sizeof(uint8_t));
		kalSnprintf(string, sizeof(string) / sizeof(uint8_t),
			 "Tc%xGrt", idx);

		/* update the final value */
		prQM->au4GuaranteedTcResource[idx] =
			(uint32_t) wlanCfgGetUint32(prAdapter, string,
					prQM->au4GuaranteedTcResource[idx]);
	}
#endif /* end of #if QM_ADAPTIVE_TC_RESOURCE_CTRL */

#endif /* end of #if CFG_SUPPORT_CFG_FILE */
}


#if CFG_SUPPORT_IOT_AP_BLACKLIST
void wlanCfgDumpIotApRule(struct ADAPTER *prAdapter)
{
	uint8_t ucRuleIdx;
	struct WLAN_IOT_AP_RULE_T *prIotApRule;

	ASSERT(prAdapter);
	for (ucRuleIdx = 0; ucRuleIdx < CFG_IOT_AP_RULE_MAX_CNT; ucRuleIdx++) {
		prIotApRule = &prAdapter->rIotApRule[ucRuleIdx];
		if (!prIotApRule->u2MatchFlag)
			continue;
		DBGLOG(INIT, TRACE, "IOTAP%d is valid rule\n", ucRuleIdx);
		DBGLOG(INIT, TRACE, "IOTAP%d Flag:0x%X Ver:0x%X\n",
			ucRuleIdx, prIotApRule->u2MatchFlag,
			prIotApRule->ucVersion);
		DBGLOG(INIT, TRACE, "IOTAP%d OUI:%02X:%02X:%02X\n",
			ucRuleIdx,
			prIotApRule->aVendorOui[0],
			prIotApRule->aVendorOui[1],
			prIotApRule->aVendorOui[2]);
		DBGLOG(INIT, TRACE, "IOTAP%d Data:"MACSTR" Mask:"MACSTR"\n",
			ucRuleIdx, MAC2STR(prIotApRule->aVendorData),
			MAC2STR(prIotApRule->aVendorDataMask));
		DBGLOG(INIT, TRACE, "IOTAP%d aBssid:"MACSTR" Mask:"MACSTR"\n",
			ucRuleIdx, MAC2STR(prIotApRule->aBssid),
			MAC2STR(prIotApRule->aBssidMask));
		DBGLOG(INIT, TRACE, "IOTAP%d NSS:%X HT:%X Band:%X Act:%X\n",
			ucRuleIdx, prIotApRule->ucNss,
			prIotApRule->ucHtType,
			prIotApRule->ucBand,
			prIotApRule->ucAction);
	}
}


void wlanCfgLoadIotApRule(struct ADAPTER *prAdapter)
{
	uint8_t  ucCnt;
	uint8_t  *pOffset;
	uint8_t  *pCurTok;
	uint8_t  ucTokId;
	uint8_t  *pNexTok;
	uint8_t  ucStatus;
	uint8_t  aucCfgKey[WLAN_CFG_KEY_LEN_MAX];
	uint8_t  aucCfgVal[WLAN_CFG_VALUE_LEN_MAX];
	struct WLAN_IOT_AP_RULE_T *prIotApRule = NULL;
	int8_t  aucEleSize[] = {
		sizeof(prIotApRule->ucVersion),
		sizeof(prIotApRule->aVendorOui),
		sizeof(prIotApRule->aVendorData),
		sizeof(prIotApRule->aVendorDataMask),
		sizeof(prIotApRule->aBssid),
		sizeof(prIotApRule->aBssidMask),
		sizeof(prIotApRule->ucNss),
		sizeof(prIotApRule->ucHtType),
		sizeof(prIotApRule->ucBand),
		sizeof(prIotApRule->ucAction)
		};

	ASSERT(prAdapter);
	ASSERT(prAdapter->rIotApRule);

	DBGLOG(INIT, INFO, "IOTAP: Start Parsing Rules\n");
	for (ucCnt = 0; ucCnt < CFG_IOT_AP_RULE_MAX_CNT; ucCnt++) {
		prIotApRule = &prAdapter->rIotApRule[ucCnt];
		kalMemSet(prIotApRule, '\0', sizeof(struct WLAN_IOT_AP_RULE_T));
		kalMemSet(aucCfgVal, '\0', WLAN_CFG_VALUE_LEN_MAX);
		kalMemSet(aucCfgKey, '\0', WLAN_CFG_KEY_LEN_MAX);
		pOffset = (uint8_t *)prIotApRule +
			OFFSET_OF(struct WLAN_IOT_AP_RULE_T, ucVersion);
		pCurTok = &aucCfgVal[0];
		pNexTok = &aucCfgVal[0];
		kalSprintf(aucCfgKey, "IOTAP%d", ucCnt);
		ucStatus = wlanCfgGet(prAdapter, aucCfgKey, aucCfgVal, NULL, 0);
		/*Skip empty rule*/
		if (ucStatus != WLAN_STATUS_SUCCESS)
			continue;

		/*Rule String Check*/
		ucStatus = 0;
		while (*pCurTok != '\0') {
			if (*pCurTok == ':')
				ucStatus++;
			else if (wlanHexToNum(*pCurTok) == -1) {
				ucStatus = -EINVAL;
				break;
			}
			pCurTok++;
		}
		if (ucStatus != WLAN_IOT_AP_FG_MAX-1) {
			DBGLOG(INIT, INFO,
				"Invalid rule IOTAP%d with status %d\n",
				ucCnt, ucStatus);
			continue;
		}

		for (ucTokId = 0; ucTokId < WLAN_IOT_AP_FG_MAX; ucTokId++) {
			pCurTok = kalStrSep((char **)&pNexTok, ":");
			if (pCurTok)
				ucStatus = wlanHexToArrayR(pCurTok, pOffset,
							   aucEleSize[ucTokId]);
			else {
				DBGLOG(INIT, TRACE,
				       "Invalid Tok IOTAP%d\n", ucCnt);
				continue;
			}
			DBGLOG(INIT, TRACE,
				"IOTAP%d tok:%d Str:%s status:%d len:%d flag:0x%x\n",
				ucCnt, ucTokId, pCurTok, ucStatus,
				aucEleSize[ucTokId], prIotApRule->u2MatchFlag);

			if (ucStatus) {
				prIotApRule->u2MatchFlag |= BIT(ucTokId);
				/*Record vendor data length*/
				if (ucTokId == WLAN_IOT_AP_FG_DATA)
					prIotApRule->ucDataLen = ucStatus;
				if (ucTokId == WLAN_IOT_AP_FG_DATA_MASK)
					prIotApRule->ucDataMaskLen = ucStatus;
			}
			pOffset += aucEleSize[ucTokId];
		}
		/*Rule Check*/
		if (prIotApRule->ucDataMaskLen &&
			prIotApRule->ucDataMaskLen != prIotApRule->ucDataLen)
			prIotApRule->u2MatchFlag = 0;
		if (prIotApRule->u2MatchFlag == 0)
			DBGLOG(INIT, INFO, "Invalid Rule IOTAP%d\n", ucCnt);
	}

}
#endif



/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to re-assign tx resource based on firmware's report
 *
 * @param prAdapter      Pointer of Adapter Data Structure
 *
 * @return WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
void wlanUpdateNicResourceInformation(struct ADAPTER
				      *prAdapter)
{
	/*
	 * 3 1. Query TX resource
	 */

	/* information is not got from firmware, use default value */
	if (prAdapter->fgIsNicTxReousrceValid != TRUE)
		return;

	/* 3 2. Setup resource parameters */
	if (prAdapter->nicTxReousrce.txResourceInit)
		prAdapter->nicTxReousrce.txResourceInit(prAdapter);
	else
		wlanSetNicResourceParameters(prAdapter);/* 6632, 7668 ways*/

	/* 3 3. Reset Tx resource */
	nicTxResetResource(prAdapter);

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	/* 3 4. Reset QM resource */
	qmResetTcControlResource(
		prAdapter); /*CHIAHSUAN, TBD, NO PLE YET*/
#endif

	halTxResourceResetHwTQCounter(prAdapter);
}


#if 0
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to SET network interface index for a network
 *        interface.
 *        A network interface is a TX/RX data port hooked to OS.
 *
 * @param prGlueInfo                     Pointer of prGlueInfo Data Structure
 * @param ucNetInterfaceIndex            Index of network interface
 * @param ucBssIndex                     Index of BSS
 *
 * @return VOID
 */
/*----------------------------------------------------------------------------*/
void wlanBindNetInterface(struct GLUE_INFO *prGlueInfo,
			  uint8_t ucNetInterfaceIndex,
			  void *pvNetInterface)
{
	struct NET_INTERFACE_INFO *prNetIfInfo;

	prNetIfInfo =
		&prGlueInfo->arNetInterfaceInfo[ucNetInterfaceIndex];

	prNetIfInfo->pvNetInterface = pvNetInterface;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to SET BSS index for a network interface.
 *           A network interface is a TX/RX data port hooked to OS.
 *
 * @param prGlueInfo                     Pointer of prGlueInfo Data Structure
 * @param ucNetInterfaceIndex            Index of network interface
 * @param ucBssIndex                     Index of BSS
 *
 * @return VOID
 */
/*----------------------------------------------------------------------------*/
void wlanBindBssIdxToNetInterface(struct GLUE_INFO *prGlueInfo,
				  uint8_t ucBssIndex,
				  void *pvNetInterface)
{
	struct NET_INTERFACE_INFO *prNetIfInfo;

	if (ucBssIndex >= prGlueInfo->prAdapter->ucHwBssIdNum) {
		DBGLOG(INIT, ERROR,
		       "Array index out of bound, ucBssIndex=%u\n", ucBssIndex);
		return;
	}

	DBGLOG(INIT, LOUD,
		"ucBssIndex = %d, pvNetInterface = %p\n",
		ucBssIndex, pvNetInterface);

	prNetIfInfo = &prGlueInfo->arNetInterfaceInfo[ucBssIndex];

	prNetIfInfo->ucBssIndex = ucBssIndex;
	prNetIfInfo->pvNetInterface = pvNetInterface;
	/* prGlueInfo->aprBssIdxToNetInterfaceInfo[ucBssIndex] = prNetIfInfo; */
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to GET BSS index for a network interface.
 *           A network interface is a TX/RX data port hooked to OS.
 *
 * @param prGlueInfo                     Pointer of prGlueInfo Data Structure
 * @param ucNetInterfaceIndex       Index of network interface
 *
 * @return UINT_8                         Index of BSS
 */
/*----------------------------------------------------------------------------*/
uint8_t wlanGetBssIdxByNetInterface(struct GLUE_INFO *prGlueInfo,
			    void *pvNetInterface)
{
	uint8_t ucIdx = 0;

	for (ucIdx = 0; ucIdx < HW_BSSID_NUM; ucIdx++) {
		if (prGlueInfo->arNetInterfaceInfo[ucIdx].pvNetInterface ==
		    pvNetInterface)
			break;
	}

	return ucIdx;
}
/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to GET network interface for a BSS.
 *           A network interface is a TX/RX data port hooked to OS.
 *
 * @param prGlueInfo                     Pointer of prGlueInfo Data Structure
 * @param ucBssIndex                     Index of BSS
 *
 * @return PVOID                         pointer of network interface structure
 */
/*----------------------------------------------------------------------------*/
void *wlanGetNetInterfaceByBssIdx(struct GLUE_INFO
				  *prGlueInfo, uint8_t ucBssIndex)
{
	return prGlueInfo->arNetInterfaceInfo[ucBssIndex].pvNetInterface;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to initialize WLAN feature options
 *
 * @param prAdapter  Pointer of ADAPTER_T
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanInitFeatureOption(struct ADAPTER *prAdapter)
{
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	struct QUE_MGT *prQM = &prAdapter->rQM;
#endif
	uint32_t u4TxHifRes = 0, u4Idx = 0;
	uint32_t u4PlatformBoostCpuTh = 1;
#if CFG_SUPPORT_LITTLE_CPU_BOOST
	uint32_t u4PlatformBoostLittleCpuTh = 1;
#endif /* CFG_SUPPORT_LITTLE_CPU_BOOST */
	struct mt66xx_chip_info *prChipInfo = prAdapter->chip_info;

	/* Feature options will be filled by config file */
#if CFG_SUPPORT_IOT_AP_BLACKLIST
	prWifiVar->fgEnDefaultIotApRule = (uint8_t) wlanCfgGetUint32(prAdapter,
					"EnDefaultIotApRule",
					FEATURE_ENABLED);
#endif
	prWifiVar->ucQoS = (uint8_t) wlanCfgGetUint32(prAdapter, "Qos",
					FEATURE_ENABLED);

	prWifiVar->ucStaHt = (uint8_t) wlanCfgGetUint32(prAdapter, "StaHT",
					FEATURE_ENABLED);
	prWifiVar->ucStaVht = (uint8_t) wlanCfgGetUint32(prAdapter, "StaVHT",
					FEATURE_ENABLED);

#if (CFG_SUPPORT_802_11AX == 1)
	if (fgEfuseCtrlAxOn == 1) {
		prWifiVar->ucStaHe = (uint8_t)
			wlanCfgGetUint32(prAdapter, "StaHE", FEATURE_ENABLED);
		prWifiVar->ucApHe = (uint8_t)
			wlanCfgGetUint32(prAdapter, "ApHE", FEATURE_ENABLED);
		prWifiVar->ucP2pGoHe = (uint8_t)
			wlanCfgGetUint32(prAdapter, "P2pGoHE",
					FEATURE_ENABLED);
		prWifiVar->ucP2pGcHe = (uint8_t)
			wlanCfgGetUint32(prAdapter, "P2pGcHE",
					FEATURE_ENABLED);
	}
#endif

#if (CFG_SUPPORT_802_11BE == 1)
	prWifiVar->ucStaEht = (uint8_t)
		wlanCfgGetUint32(prAdapter, "StaEHT", FEATURE_ENABLED);
	prWifiVar->ucApEht = (uint8_t)
		wlanCfgGetUint32(prAdapter, "ApEHT", FEATURE_FORCE_ENABLED);
	prWifiVar->ucP2pGoEht = (uint8_t)
		wlanCfgGetUint32(prAdapter, "P2pGoEHT",
				FEATURE_FORCE_ENABLED);
	prWifiVar->ucP2pGcEht = (uint8_t)
		wlanCfgGetUint32(prAdapter, "P2pGcEHT",
				FEATURE_ENABLED);
	prWifiVar->u2RxEhtBaSize = wlanCfgGetUint32(prAdapter, "RxEhtBaSize",
			WLAN_EHT_MAX_BA_SIZE);
	prWifiVar->u2TxEhtBaSize = wlanCfgGetUint32(prAdapter, "TxEhtBaSize",
			WLAN_EHT_MAX_BA_SIZE);

	prWifiVar->fgMoveWinOnMissingLast = wlanCfgGetUint32(prAdapter,
			"MoveWinOnMissingLast", FEATURE_DISABLED);

	prWifiVar->u2BaExtSize = wlanCfgGetUint32(prAdapter, "BaExtSize",
			WLAN_RX_BA_EXT_SIZE);
	if (prWifiVar->u2BaExtSize > WLAN_RX_BA_EXT_MAX_SIZE)
		prWifiVar->u2BaExtSize = WLAN_RX_BA_EXT_MAX_SIZE;

	prWifiVar->u4BaVerboseLogging = wlanCfgGetUint32(prAdapter,
			"BaVerboseLog", 0);

	prWifiVar->ucEhtAmsduInAmpduRx = (uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtAmsduInAmpduRx", FEATURE_ENABLED);
	prWifiVar->ucEhtAmsduInAmpduTx = (uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtAmsduInAmpduTx", FEATURE_ENABLED);
	prWifiVar->ucStaEhtBfee = (uint8_t) wlanCfgGetUint32(prAdapter,
		"StaEHTBfee", FEATURE_ENABLED);
	prWifiVar->ucEhtOMCtrl = (uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtOMCtrl", FEATURE_ENABLED);
	prWifiVar->ucStaEht242ToneRUWt20M =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"StaEht242ToneRUWt20M", FEATURE_ENABLED);
	prWifiVar->ucEhtNDP4xLTF3dot2usGI =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtNDP4xLTF3dot2usGI", FEATURE_ENABLED);
	prWifiVar->ucEhtSUBfer =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtSUBfer", FEATURE_ENABLED);
	prWifiVar->ucEhtSUBfee =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtSUBfee", FEATURE_ENABLED);
	prWifiVar->ucEhtBfeeSSLeEq80m =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtBfeeSSLeEq80m", 3);
	prWifiVar->ucEhtBfee160m =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtBfee160m", 3);
	prWifiVar->ucEhtBfee320m =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtBfee320m", 3);
	prWifiVar->ucEhtSUBfee =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtSUBfee", FEATURE_ENABLED);
	prWifiVar->ucEhtNG16SUFeedback =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtNG16SUFeedback", FEATURE_ENABLED);
	prWifiVar->ucEhtNG16MUFeedback =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtNG16MUFeedback", FEATURE_ENABLED);
	prWifiVar->ucEhtCodebook75MuFeedback =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtCodebook75MuFeedback", FEATURE_ENABLED);
	prWifiVar->ucEhtTrigedSUBFFeedback =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtTrigedSUBFFeedback", FEATURE_ENABLED);
	prWifiVar->ucEhtTrigedMUBFPartialBWFeedback =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtTrigedMUBFPartialBWFeedback", FEATURE_ENABLED);
	prWifiVar->ucEhtTrigedCQIFeedback =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtTrigedCQIFeedback", FEATURE_ENABLED);
	prWifiVar->ucEhtPartialBwDLMUMIMO =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtPartialBwDLMUMIMO", FEATURE_ENABLED);
	prWifiVar->ucEhtMUPPDU4xEHTLTFdot8usGI =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtMUPPDU4xEHTLTFdot8usGI", FEATURE_ENABLED);
	prWifiVar->ucEhtNonTrigedCQIFeedback =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtNonTrigedCQIFeedback", FEATURE_ENABLED);
	prWifiVar->ucEhtTx1024QAM4096QAMLe242ToneRU =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtTx1024QAM4096QAMLe242ToneRU", FEATURE_ENABLED);
	prWifiVar->ucEhtRx1024QAM4096QAMLe242ToneRU =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtRx1024QAM4096QAMLe242ToneRU", FEATURE_ENABLED);
	prWifiVar->ucEhtTx1024QAM4096QAMLe242ToneRU =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtTx1024QAM4096QAMLe242ToneRU", FEATURE_ENABLED);
	prWifiVar->ucEhtCommonNominalPktPadding =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtCommonNominalPktPadding", COMMON_NOMINAL_PAD_16_US);
	prWifiVar->ucEhtMaxLTFNum =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtMaxLTFNum", 0x0B);
	prWifiVar->ucEhtMCS15 =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtMCS15", 0xff);
	prWifiVar->ucEhtDup6G =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"EhtDup6G", FEATURE_ENABLED);
	prWifiVar->ucEht20MRxNDPWiderBW =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"Eht20MRxNDPWiderBW", FEATURE_ENABLED);
	prWifiVar->ucPresetLinkId = MLD_LINK_ID_NONE;
	prWifiVar->ucMldLinkMax = (uint8_t) wlanCfgGetUint32(prAdapter,
		"MldLinkMax", MLD_LINK_MAX);
	prWifiVar->ucStaMldLinkMax = (uint8_t) wlanCfgGetUint32(prAdapter,
		"StaMldLinkMax", MLD_LINK_MAX);
	prWifiVar->ucP2pMldLinkMax = (uint8_t) wlanCfgGetUint32(prAdapter,
		"P2pMldLinkMax", MLD_LINK_MAX);
	if (prWifiVar->ucStaMldLinkMax > prWifiVar->ucMldLinkMax) {
		DBGLOG(INIT, WARN,
			"StaMldLinkMax %d => %d\n",
			prWifiVar->ucStaMldLinkMax, prWifiVar->ucMldLinkMax);
		prWifiVar->ucStaMldLinkMax = prWifiVar->ucMldLinkMax;
	}
	if (prWifiVar->ucP2pMldLinkMax > prWifiVar->ucMldLinkMax) {
		DBGLOG(INIT, WARN,
			"P2pMldLinkMax %d => %d\n",
			prWifiVar->ucP2pMldLinkMax, prWifiVar->ucMldLinkMax);
		prWifiVar->ucP2pMldLinkMax = prWifiVar->ucMldLinkMax;
	}

	prWifiVar->ucApMldMainLinkIdx = (uint8_t) wlanCfgGetInt32(
		prAdapter, "ApMldMainLinkIdx", MLD_LINK_ID_NONE);
	prWifiVar->ucStaMldMainLinkIdx = (uint8_t) wlanCfgGetInt32(
		prAdapter, "StaMldMainLinkIdx", MLD_LINK_ID_NONE);
	prWifiVar->ucStaPreferMldAddr = (uint8_t) wlanCfgGetUint32(prAdapter,
		"StaPreferMldAddr", FEATURE_DISABLED);
	wlanCfgGet(prAdapter, "MloP2pPreferFreq",
		prWifiVar->aucMloP2pPreferFreq,
		"2462 5180 5975", 0);
	prWifiVar->ucMlProbeRetryLimit = (uint8_t) wlanCfgGetInt32(
		prAdapter, "MlProbeRetryLimit", ML_PROBE_RETRY_COUNT);
	prWifiVar->ucEnableMlo = (uint8_t) wlanCfgGetUint32(prAdapter,
		"EnableMlo", FEATURE_ENABLED);
	prWifiVar->ucMaxSimultaneousLinks = (uint8_t)
		wlanCfgGetUint32(prAdapter, "MaxSimultaneousLinks", 0xff);
	prWifiVar->ucMldRetryCount = (uint8_t)
		wlanCfgGetUint32(prAdapter, "MldRetryCount", MLD_RETRY_COUNT);

#endif /* CFG_SUPPORT_802_11BE */
	prWifiVar->ucApHt = (uint8_t) wlanCfgGetUint32(prAdapter, "ApHT",
					FEATURE_ENABLED);
#if CFG_TC1_FEATURE
	prWifiVar->ucApVht = (uint8_t) wlanCfgGetUint32(prAdapter, "ApVHT",
					FEATURE_FORCE_ENABLED);
#else
	prWifiVar->ucApVht = (uint8_t) wlanCfgGetUint32(prAdapter, "ApVHT",
					FEATURE_ENABLED);
#endif

	prWifiVar->ucP2pGoHt = (uint8_t) wlanCfgGetUint32(prAdapter, "P2pGoHT",
					FEATURE_ENABLED);
	prWifiVar->ucP2pGoVht = (uint8_t) wlanCfgGetUint32(prAdapter,
					"P2pGoVHT", FEATURE_ENABLED);

	prWifiVar->ucP2pGcHt = (uint8_t) wlanCfgGetUint32(prAdapter, "P2pGcHT",
					FEATURE_ENABLED);
	prWifiVar->ucP2pGcVht = (uint8_t) wlanCfgGetUint32(prAdapter,
					"P2pGcVHT", FEATURE_ENABLED);

	prWifiVar->ucAmpduRx = (uint8_t) wlanCfgGetUint32(prAdapter, "AmpduRx",
					FEATURE_ENABLED);
	prWifiVar->ucAmpduTx = (uint8_t) wlanCfgGetUint32(prAdapter, "AmpduTx",
					FEATURE_ENABLED);

#if (CFG_SUPPORT_HOST_OFFLOAD == 1)
	prWifiVar->ucAmsduInAmpduRx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"AmsduInAmpduRx", FEATURE_DISABLED);
#else
	prWifiVar->ucAmsduInAmpduRx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"AmsduInAmpduRx", FEATURE_ENABLED);
#endif /* CFG_SUPPORT_HOST_OFFLOAD == 1 */
	prWifiVar->ucAmsduInAmpduTx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"AmsduInAmpduTx", FEATURE_ENABLED);
	prWifiVar->ucHtAmsduInAmpduRx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"HtAmsduInAmpduRx", FEATURE_ENABLED);
	prWifiVar->ucHtAmsduInAmpduTx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"HtAmsduInAmpduTx", FEATURE_ENABLED);
	prWifiVar->ucVhtAmsduInAmpduRx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"VhtAmsduInAmpduRx", FEATURE_ENABLED);
	prWifiVar->ucVhtAmsduInAmpduTx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"VhtAmsduInAmpduTx", FEATURE_ENABLED);

	prWifiVar->ucTspec = (uint8_t) wlanCfgGetUint32(prAdapter, "Tspec",
					FEATURE_DISABLED);

	prWifiVar->ucUapsd = (uint8_t) wlanCfgGetUint32(prAdapter, "Uapsd",
					FEATURE_ENABLED);
	prWifiVar->ucStaUapsd = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaUapsd", FEATURE_DISABLED);
	prWifiVar->ucApUapsd = (uint8_t) wlanCfgGetUint32(prAdapter,
					"ApUapsd", FEATURE_DISABLED);
	prWifiVar->ucP2pUapsd = (uint8_t) wlanCfgGetUint32(prAdapter,
					"P2pUapsd", FEATURE_ENABLED);
#if (CFG_ENABLE_WIFI_DIRECT && CFG_MTK_ANDROID_WMT)
	prWifiVar->u4RegP2pIfAtProbe = (uint8_t) wlanCfgGetUint32(prAdapter,
					"RegP2pIfAtProbe", FEATURE_ENABLED);
#else
	prWifiVar->u4RegP2pIfAtProbe = (uint8_t) wlanCfgGetUint32(prAdapter,
					"RegP2pIfAtProbe", FEATURE_DISABLED);
#endif

	prWifiVar->ucRegP2pMode = (uint8_t) wlanCfgGetUint32(prAdapter,
					"RegP2pMode", DEFAULT_RUNNING_P2P_MODE);

	prWifiVar->ucP2pShareMacAddr = (uint8_t) wlanCfgGetUint32(prAdapter,
					"P2pShareMacAddr", FEATURE_DISABLED);

	prWifiVar->ucTxShortGI = (uint8_t) wlanCfgGetUint32(prAdapter, "SgiTx",
					FEATURE_ENABLED);
	prWifiVar->ucRxShortGI = (uint8_t) wlanCfgGetUint32(prAdapter, "SgiRx",
					FEATURE_ENABLED);

	prWifiVar->ucTxLdpc = (uint8_t) wlanCfgGetUint32(prAdapter, "LdpcTx",
					FEATURE_ENABLED);
	prWifiVar->ucRxLdpc = (uint8_t) wlanCfgGetUint32(prAdapter, "LdpcRx",
					FEATURE_ENABLED);

	prWifiVar->ucTxStbc = (uint8_t) wlanCfgGetUint32(prAdapter, "StbcTx",
					FEATURE_ENABLED);
	prWifiVar->ucRxStbc = (uint8_t) wlanCfgGetUint32(prAdapter, "StbcRx",
					FEATURE_ENABLED);
	prWifiVar->ucRxStbcNss = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StbcRxNss", 1);

	prWifiVar->ucTxGf = (uint8_t) wlanCfgGetUint32(prAdapter, "GfTx",
					FEATURE_ENABLED);
	prWifiVar->ucRxGf = (uint8_t) wlanCfgGetUint32(prAdapter, "GfRx",
					FEATURE_ENABLED);

	prWifiVar->ucMCS32 = (uint8_t) wlanCfgGetUint32(prAdapter, "MCS32",
					FEATURE_ENABLED);

#if (CFG_SUPPORT_802_11AX == 1)
	if (fgEfuseCtrlAxOn == 1) {
	prWifiVar->ucHeAmsduInAmpduRx = (uint8_t) wlanCfgGetUint32(prAdapter,
		"HeAmsduInAmpduRx", FEATURE_ENABLED);
	prWifiVar->ucHeAmsduInAmpduTx = (uint8_t) wlanCfgGetUint32(prAdapter,
		"HeAmsduInAmpduTx", FEATURE_ENABLED);
	prWifiVar->ucTrigMacPadDur = (uint8_t) wlanCfgGetUint32(prAdapter,
		"TrigMacPadDur", HE_CAP_TRIGGER_PAD_DURATION_16);
	prWifiVar->ucVcoreBoostEnable = (uint8_t) wlanCfgGetUint32(prAdapter,
		"HeVcoreBoostEnable", FEATURE_DISABLED);
	prWifiVar->ucMaxAmpduLenExp = (uint8_t) wlanCfgGetUint32(prAdapter,
		"MaxAmpduLenExt", HE_CAP_MAX_AMPDU_LEN_EXP);
	prWifiVar->fgEnableSR = (uint8_t) wlanCfgGetUint32(prAdapter,
		"SREnable", FEATURE_DISABLED);
	}
#endif

#if (CFG_SUPPORT_TWT == 1)
	prWifiVar->ucTWTRequester = (uint8_t)
		wlanCfgGetUint32(prAdapter, "TWTRequester", FEATURE_ENABLED);
	prWifiVar->ucTWTResponder = (uint8_t)
		wlanCfgGetUint32(prAdapter, "TWTResponder", FEATURE_DISABLED);
	prWifiVar->ucTWTStaBandBitmap = (uint8_t)
		wlanCfgGetUint32(prAdapter,
			"TWTStaBandBitmap",
			BIT(BAND_2G4) | BIT(BAND_5G)
#if (CFG_SUPPORT_WIFI_6G == 1)
			| BIT(BAND_6G)
#endif
		);
#endif

#if (CFG_SUPPORT_TWT_HOTSPOT == 1)
	prWifiVar->ucTWTHotSpotSupport = (uint8_t)
		wlanCfgGetUint32(prAdapter,
			"TWTHotSpotSupport",
			FEATURE_ENABLED);
#endif

#if (CFG_SUPPORT_BTWT == 1)
	prWifiVar->ucBTWTSupport = (uint8_t)
		wlanCfgGetUint32(prAdapter, "BTWTSupport", FEATURE_DISABLED);
#endif

	prWifiVar->ucSigTaRts = (uint8_t) wlanCfgGetUint32(prAdapter,
					"SigTaRts", FEATURE_DISABLED);
	prWifiVar->ucDynBwRts = (uint8_t) wlanCfgGetUint32(prAdapter,
					"DynBwRts", FEATURE_DISABLED);
	prWifiVar->ucTxopPsTx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"TxopPsTx", FEATURE_DISABLED);
	/* HT BFee has IOT issue
	 * only support HT BFee when force mode for testing
	 */
	prWifiVar->ucStaHtBfee = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaHTBfee", FEATURE_DISABLED);
	prWifiVar->ucStaVhtBfee = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaVHTBfee", FEATURE_ENABLED);
	prWifiVar->ucStaVhtMuBfee = (uint8_t)wlanCfgGetUint32(prAdapter,
					"StaVHTMuBfee", FEATURE_ENABLED);
	prWifiVar->ucStaHtBfer = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaHTBfer", FEATURE_DISABLED);
	prWifiVar->ucStaVhtBfer = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaVHTBfer", FEATURE_DISABLED);

#if (CFG_SUPPORT_802_11AX == 1)
	if (fgEfuseCtrlAxOn == 1) {
		prWifiVar->ucStaHeBfee = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaHEBfee", FEATURE_ENABLED);
		prWifiVar->ucStaHeSuBfer = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaHESUBfer", FEATURE_DISABLED);
	}
	prWifiVar->ucHeOMCtrl = (uint8_t) wlanCfgGetUint32(prAdapter,
					"HeOMCtrl", FEATURE_ENABLED);

	prWifiVar->ucRxCtrlToMutiBss = (uint8_t) wlanCfgGetUint32(prAdapter,
					"RxCtrlToMutiBss", FEATURE_ENABLED);

	prWifiVar->ucStaHePpRx = (uint8_t) wlanCfgGetUint32(prAdapter,
					"StaHePpRx", FEATURE_DISABLED);
	prWifiVar->ucHeDynamicSMPS = (uint8_t) wlanCfgGetUint32(prAdapter,
					"HeDynamicSMPS", FEATURE_DISABLED);
	prWifiVar->ucHeHTC = (uint8_t) wlanCfgGetUint32(prAdapter, "HeHTC", FEATURE_ENABLED);
#endif

	prWifiVar->ucBtmCap = (uint8_t) wlanCfgGetUint32(prAdapter,
					"BtmCap", FEATURE_ENABLED);
	/* 0: disabled
	 * 1: Tx done event to driver
	 * 2: Tx status to FW only
	 */
	prWifiVar->ucDataTxDone = (uint8_t) wlanCfgGetUint32(
					prAdapter, "DataTxDone", 0);
	prWifiVar->ucDataTxRateMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "DataTxRateMode",
					DATA_RATE_MODE_AUTO);
	prWifiVar->u4DataTxRateCode = wlanCfgGetUint32(
					prAdapter, "DataTxRateCode", 0x0);

	prWifiVar->ucApWpsMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ApWpsMode", 0);
	DBGLOG(INIT, TRACE, "ucApWpsMode = %u\n", prWifiVar->ucApWpsMode);

	prWifiVar->ucThreadScheduling = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ThreadSched", 0);
	prWifiVar->ucThreadPriority = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ThreadPriority",
					WLAN_THREAD_TASK_PRIORITY);
	prWifiVar->cThreadNice = (int8_t) wlanCfgGetInt32(
					prAdapter, "ThreadNice",
					WLAN_THREAD_TASK_NICE);

	prAdapter->rQM.u4MaxForwardBufferCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "ApForwardBufferCnt",
					QM_FWD_PKT_QUE_THRESHOLD);

	/* AP channel setting
	 * 0: auto
	 */
	prWifiVar->ucApChannel = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ApChannel", 0);
	prWifiVar->u2ApFreq = (uint16_t) wlanCfgGetUint32(
					prAdapter, "ApFreq", 0);
	prWifiVar->ucApAcsChannel[0] = (uint16_t) wlanCfgGetUint32(
					prAdapter, "ApAcs2gChannel", 0);
	prWifiVar->ucApAcsChannel[1] = (uint16_t) wlanCfgGetUint32(
					prAdapter, "ApAcs5gChannel", 0);
	prWifiVar->ucApAcsChannel[2] = (uint16_t) wlanCfgGetUint32(
					prAdapter, "ApAcs6gChannel", 0);

	/*
	 * 0: SCN
	 * 1: SCA
	 * 2: RES
	 * 3: SCB
	 */
	prWifiVar->ucApSco = (uint8_t) wlanCfgGetUint32(
						prAdapter, "ApSco", 0);
	prWifiVar->ucP2pGoSco = (uint8_t) wlanCfgGetUint32(
						prAdapter, "P2pGoSco", 0);

	/* Max bandwidth setting
	 * 0: 20Mhz
	 * 1: 40Mhz
	 * 2: 80Mhz
	 * 3: 160Mhz
	 * 4: 80+80Mhz
	 * Note: For VHT STA, BW 80Mhz is a must!
	 */
	prWifiVar->ucStaBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "StaBw", MAX_BW_320_2MHZ);
	//2.4G WIFI 40MHZ requirements modify
	prWifiVar->ucSta2gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "Sta2gBw", MAX_BW_40MHZ);
	prWifiVar->ucSta5gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "Sta5gBw", MAX_BW_160MHZ);
	prWifiVar->ucSta6gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "Sta6gBw", MAX_BW_320_2MHZ);
	/* GC,GO */
	prWifiVar->ucP2p2gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "P2p2gBw", MAX_BW_20MHZ);
	prWifiVar->ucP2p5gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "P2p5gBw", MAX_BW_80MHZ);
	prWifiVar->ucP2p6gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "P2p6gBw", MAX_BW_320_1MHZ);
	prWifiVar->ucApBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "ApBw", MAX_BW_320_2MHZ);
	prWifiVar->ucAp2gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "Ap2gBw", MAX_BW_20MHZ);
	prWifiVar->ucAp5gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "Ap5gBw", MAX_BW_80MHZ);
	prWifiVar->ucAp6gBandwidth = (uint8_t) wlanCfgGetUint32(
				prAdapter, "Ap6gBw", MAX_BW_320_1MHZ);
	prWifiVar->ucApChnlDefFromCfg = (uint8_t) wlanCfgGetUint32(
				prAdapter, "ApChnlDefFromCfg", FEATURE_ENABLED);
	prWifiVar->ucApAllowHtVhtTkip = (uint8_t) wlanCfgGetUint32(
				prAdapter, "ApAllowHtVhtTkip",
				FEATURE_DISABLED);

	prWifiVar->ucApForceSleep = (uint8_t) wlanCfgGetUint32(
				prAdapter, "ApForceSleep", FEATURE_ENABLED);

	prWifiVar->ucNSS = (uint8_t) wlanCfgGetUint32
				(prAdapter, "Nss", DEFAULT_NSS);

#ifdef CFG_FORCE_AP1NSS
	prWifiVar->ucAp6gNSS = (uint8_t)wlanCfgGetUint32
				(prAdapter, "Ap6gNss", 1);
	prWifiVar->ucAp5gNSS = (uint8_t)wlanCfgGetUint32
				(prAdapter, "Ap5gNss", 1);
	prWifiVar->ucAp2gNSS = (uint8_t) wlanCfgGetUint32
				(prAdapter, "Ap2gNss", 1);
#else
	prWifiVar->ucAp6gNSS = (uint8_t)wlanCfgGetUint32
				(prAdapter, "Ap6gNss", DEFAULT_NSS);
	prWifiVar->ucAp5gNSS = (uint8_t)wlanCfgGetUint32
				(prAdapter, "Ap5gNss", DEFAULT_NSS);
	prWifiVar->ucAp2gNSS = (uint8_t) wlanCfgGetUint32
				(prAdapter, "Ap2gNss", DEFAULT_NSS);
#endif
	prWifiVar->ucGo6gNSS = (uint8_t) wlanCfgGetUint32
				(prAdapter, "Go6gNss", DEFAULT_NSS);
	prWifiVar->ucGo5gNSS = (uint8_t) wlanCfgGetUint32
				(prAdapter, "Go5gNss", DEFAULT_NSS);
	prWifiVar->ucGo2gNSS = (uint8_t) wlanCfgGetUint32
				(prAdapter, "Go2gNss", DEFAULT_NSS);

	/* Max Rx MPDU length setting
	 * 0: 3k
	 * 1: 8k
	 * 2: 11k
	 */
	prWifiVar->ucRxMaxMpduLen = (uint8_t) wlanCfgGetUint32(
					prAdapter, "RxMaxMpduLen",
					VHT_CAP_INFO_MAX_MPDU_LEN_3K);

#if (CFG_SUPPORT_RX_QUOTA_INFO == 1)
	prWifiVar->ucRxQuotaInfoEn = (uint8_t) wlanCfgGetUint32(
					prAdapter, "RxQuotaInfoEn",
					FEATURE_ENABLED);
#endif
	/* Max Tx AMSDU in AMPDU length *in BYTES* */
	prWifiVar->u4HtTxMaxAmsduInAmpduLen = wlanCfgGetUint32(
					prAdapter, "HtTxMaxAmsduInAmpduLen",
					WLAN_TX_MAX_AMSDU_IN_AMPDU_LEN);
	prWifiVar->u4VhtTxMaxAmsduInAmpduLen = wlanCfgGetUint32(
					prAdapter, "VhtTxMaxAmsduInAmpduLen",
					WLAN_TX_MAX_AMSDU_IN_AMPDU_LEN);
	prWifiVar->u4TxMaxAmsduInAmpduLen = wlanCfgGetUint32(
					prAdapter, "TxMaxAmsduInAmpduLen",
					WLAN_TX_MAX_AMSDU_IN_AMPDU_LEN);

	prWifiVar->ucTcRestrict = (uint8_t) wlanCfgGetUint32(
					prAdapter, "TcRestrict", 0xFF);
	/* Max Tx dequeue limit: 0 => auto */
	prWifiVar->u4MaxTxDeQLimit = (uint32_t) wlanCfgGetUint32(
					prAdapter, "MaxTxDeQLimit", 0x0);
	prWifiVar->ucAlwaysResetUsedRes = (uint32_t) wlanCfgGetUint32(
					prAdapter, "AlwaysResetUsedRes", 0x0);

#if CFG_SUPPORT_MTK_SYNERGY
	prWifiVar->ucMtkOui = (uint8_t) wlanCfgGetUint32(
					prAdapter, "MtkOui", FEATURE_ENABLED);
	prWifiVar->u4MtkOuiCap = (uint32_t) wlanCfgGetUint32(
					prAdapter, "MtkOuiCap", 0);
	prWifiVar->aucMtkFeature[0] = 0xff;
	prWifiVar->aucMtkFeature[1] = 0xff;
	prWifiVar->aucMtkFeature[2] = 0xff;
	prWifiVar->aucMtkFeature[3] = 0xff;
	prWifiVar->ucGbandProbe256QAM = (uint8_t) wlanCfgGetUint32(
					prAdapter, "Probe256QAM",
					FEATURE_ENABLED);
#endif
#if CFG_SUPPORT_VHT_IE_IN_2G
	prWifiVar->ucVhtIeIn2g = (uint8_t) wlanCfgGetUint32(
			prAdapter, "VhtIeIn2G", FEATURE_ENABLED);
#endif
	prWifiVar->ucCmdRsvResource = (uint8_t) wlanCfgGetUint32(
					prAdapter, "TxCmdRsv",
					QM_CMD_RESERVED_THRESHOLD);
	prWifiVar->u4MgmtQueueDelayTimeout =
		(uint32_t) wlanCfgGetUint32(prAdapter, "TxMgmtQueTO",
					QM_MGMT_QUEUED_TIMEOUT); /* ms */

	/* Performance related */
	prWifiVar->u4HifIstLoopCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "IstLoop",
					CFG_IST_LOOP_COUNT);
	prWifiVar->u4Rx2OsLoopCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Rx2OsLoop", 4);
	prWifiVar->u4HifTxloopCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "HifTxLoop", 1);
	prWifiVar->u4TxFromOsLoopCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "OsTxLoop", 1);
	prWifiVar->u4TxRxLoopCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Rx2ReorderLoop", 1);
	prWifiVar->u4TxIntThCount = (uint32_t) wlanCfgGetUint32(
					prAdapter, "IstTxTh",
					HIF_IST_TX_THRESHOLD);

	prWifiVar->u4NetifStopTh = (uint32_t) wlanCfgGetUint32(
					prAdapter, "NetifStopTh",
					CFG_TX_STOP_NETIF_PER_QUEUE_THRESHOLD);
	prWifiVar->u4NetifStartTh = (uint32_t) wlanCfgGetUint32(
					prAdapter, "NetifStartTh",
					CFG_TX_START_NETIF_PER_QUEUE_THRESHOLD);
	prWifiVar->ucTxBaSize = (uint8_t) wlanCfgGetUint32(
					prAdapter, "TxBaSize",
					WLAN_LEGACY_MAX_BA_SIZE);
	prWifiVar->ucRxHtBaSize = (uint8_t) wlanCfgGetUint32(
					prAdapter, "RxHtBaSize",
					WLAN_LEGACY_MAX_BA_SIZE);
	prWifiVar->ucRxVhtBaSize = (uint8_t) wlanCfgGetUint32(
					prAdapter, "RxVhtBaSize",
					WLAN_LEGACY_MAX_BA_SIZE);
#if (CFG_SUPPORT_802_11AX == 1)
	if (fgEfuseCtrlAxOn == 1) {
		prWifiVar->u2RxHeBaSize = (uint8_t)
			wlanCfgGetUint32(prAdapter, "RxHeBaSize",
				WLAN_HE_MAX_BA_SIZE);
		prWifiVar->u2TxHeBaSize = (uint8_t)
			wlanCfgGetUint32(prAdapter, "TxHeBaSize",
				WLAN_HE_MAX_BA_SIZE);
	}
#endif

#if CFG_SUPPORT_SMART_GEAR
	prWifiVar->ucSGCfg = (uint8_t) wlanCfgGetUint32(
					prAdapter, "SGCfg", FEATURE_ENABLED);
	/* 2.4G default is WF0 when enable SG SISO mode*/
	prWifiVar->ucSG24GFavorANT = (uint8_t) wlanCfgGetUint32(
					prAdapter, "SG24GFavorANT",
					FEATURE_DISABLED);
	/* 5G default is WF1 when enable SG SISO mode*/
	prWifiVar->ucSG5GFavorANT = (uint8_t) wlanCfgGetUint32(
					prAdapter, "SG5GFavorANT",
					FEATURE_ENABLED);
#endif
	/* Tx Buffer Management */
	prWifiVar->ucExtraTxDone = (uint32_t) wlanCfgGetUint32(
					prAdapter, "ExtraTxDone", 1);
	prWifiVar->ucTxDbg = (uint32_t) wlanCfgGetUint32(prAdapter, "TxDbg", 0);

	kalMemZero(prWifiVar->au4TcPageCount,
					sizeof(prWifiVar->au4TcPageCount));

	prWifiVar->au4TcPageCount[TC0_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc0Page",
					NIC_TX_PAGE_COUNT_TC0);
	prWifiVar->au4TcPageCount[TC1_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc1Page",
					NIC_TX_PAGE_COUNT_TC1);
	prWifiVar->au4TcPageCount[TC2_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc2Page",
					NIC_TX_PAGE_COUNT_TC2);
	prWifiVar->au4TcPageCount[TC3_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc3Page",
					NIC_TX_PAGE_COUNT_TC3);
	prWifiVar->au4TcPageCount[TC4_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc4Page",
					NIC_TX_PAGE_COUNT_TC4);
#if (CFG_TX_RSRC_WMM_ENHANCE == 1)
	prWifiVar->au4TcPageCount[TC5_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc5Page",
					NIC_TX_PAGE_COUNT_TC0);
	prWifiVar->au4TcPageCount[TC6_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc6Page",
					NIC_TX_PAGE_COUNT_TC1);
	prWifiVar->au4TcPageCount[TC7_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc7Page",
					NIC_TX_PAGE_COUNT_TC2);
	prWifiVar->au4TcPageCount[TC8_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc8Page",
					NIC_TX_PAGE_COUNT_TC3);
	prWifiVar->au4TcPageCount[TC9_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc9Page",
					NIC_TX_PAGE_COUNT_TC0);
	prWifiVar->au4TcPageCount[TC10_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc10Page",
					NIC_TX_PAGE_COUNT_TC1);
	prWifiVar->au4TcPageCount[TC11_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc11Page",
					NIC_TX_PAGE_COUNT_TC2);
	prWifiVar->au4TcPageCount[TC12_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc12Page",
					NIC_TX_PAGE_COUNT_TC3);
	prWifiVar->au4TcPageCount[TC13_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc13Page",
					NIC_TX_PAGE_COUNT_TC1);
#endif

	prWifiVar->ucTxMsduQueue = (uint32_t) wlanCfgGetUint32(
		prAdapter, "NicTxMsduQueue", 0);

	prWifiVar->ucTxMsduQueueInit = prWifiVar->ucTxMsduQueue;

	/* 1 resource for AC_BK(TC0_INDEX), AC_BE(TC1_INDEX) */
	/* 2 resource for AC_VI(TC2_INDEX) */
	/* 4 resource for AC_VO(TC3_INDEX) */
	/* 1 resource for MGMT(TC4_INDEX) & TC_NUM */
	u4TxHifRes = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxHifResCtl", 0x00114211);
	prWifiVar->u4TxHifRes = u4TxHifRes;
	for (u4Idx = 0; u4Idx < TC_NUM && u4TxHifRes; u4Idx++) {
		prAdapter->au4TxHifResCtl[u4Idx] = u4TxHifRes & BITS(0, 3);
		u4TxHifRes = u4TxHifRes >> 4;
	}

#if QM_ADAPTIVE_TC_RESOURCE_CTRL
	prQM->au4MinReservedTcResource[TC0_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc0MinRsv",
					QM_MIN_RESERVED_TC0_RESOURCE);
	prQM->au4MinReservedTcResource[TC1_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc1MinRsv",
					QM_MIN_RESERVED_TC1_RESOURCE);
	prQM->au4MinReservedTcResource[TC2_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc2MinRsv",
					QM_MIN_RESERVED_TC2_RESOURCE);
	prQM->au4MinReservedTcResource[TC3_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc3MinRsv",
					QM_MIN_RESERVED_TC3_RESOURCE);
	prQM->au4MinReservedTcResource[TC4_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc4MinRsv",
					QM_MIN_RESERVED_TC4_RESOURCE);

#if (CFG_TX_RSRC_WMM_ENHANCE == 1)
#define __STR_MIN_RESERVED(_idx_) STR_HELPER(Tc##_idx_##MinRsv)
#define CONF_QM_TC_MIN_RESERVED_TEMPLATE(_idx_)\
	(prQM->au4MinReservedTcResource[TC##_idx_##_INDEX] =\
		(uint32_t) wlanCfgGetUint32(prAdapter,\
		__STR_MIN_RESERVED(_idx_),\
		QM_MIN_RESERVED_TC##_idx_##_RESOURCE))
	CONF_QM_TC_MIN_RESERVED_TEMPLATE(5);
	CONF_QM_TC_MIN_RESERVED_TEMPLATE(6);
	CONF_QM_TC_MIN_RESERVED_TEMPLATE(7);
	CONF_QM_TC_MIN_RESERVED_TEMPLATE(8);
	CONF_QM_TC_MIN_RESERVED_TEMPLATE(9);
	CONF_QM_TC_MIN_RESERVED_TEMPLATE(10);
	CONF_QM_TC_MIN_RESERVED_TEMPLATE(11);
	CONF_QM_TC_MIN_RESERVED_TEMPLATE(12);
	CONF_QM_TC_MIN_RESERVED_TEMPLATE(13);
#undef CONF_QM_TC_MIN_RESERVED_TEMPLATE
#undef __STR_MIN_RESERVED
#endif

	prQM->au4GuaranteedTcResource[TC0_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc0Grt",
					QM_GUARANTEED_TC0_RESOURCE);
	prQM->au4GuaranteedTcResource[TC1_INDEX] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "Tc1Grt",
					    QM_GUARANTEED_TC1_RESOURCE);
	prQM->au4GuaranteedTcResource[TC2_INDEX] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "Tc2Grt",
					    QM_GUARANTEED_TC2_RESOURCE);
	prQM->au4GuaranteedTcResource[TC3_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc3Grt",
					QM_GUARANTEED_TC3_RESOURCE);
	prQM->au4GuaranteedTcResource[TC4_INDEX] = (uint32_t) wlanCfgGetUint32(
					prAdapter, "Tc4Grt",
					QM_GUARANTEED_TC4_RESOURCE);

#if (CFG_TX_RSRC_WMM_ENHANCE == 1)
#define __STR_GUARANTEED(_idx_) STR_HELPER(Tc##_idx_##Grt)
#define CONF_QM_TC_GUARANTEED_TEMPLATE(_idx_)\
	(prQM->au4GuaranteedTcResource[TC##_idx_##_INDEX] =\
		(uint32_t) wlanCfgGetUint32(prAdapter,\
		__STR_GUARANTEED(_idx_),\
		QM_GUARANTEED_TC##_idx_##_RESOURCE))

	CONF_QM_TC_GUARANTEED_TEMPLATE(5);
	CONF_QM_TC_GUARANTEED_TEMPLATE(6);
	CONF_QM_TC_GUARANTEED_TEMPLATE(7);
	CONF_QM_TC_GUARANTEED_TEMPLATE(8);
	CONF_QM_TC_GUARANTEED_TEMPLATE(9);
	CONF_QM_TC_GUARANTEED_TEMPLATE(10);
	CONF_QM_TC_GUARANTEED_TEMPLATE(11);
	CONF_QM_TC_GUARANTEED_TEMPLATE(12);
	CONF_QM_TC_GUARANTEED_TEMPLATE(13);
#undef CONF_QM_TC_GUARANTEED_TEMPLATE
#undef __STR_MIN_RESERVED
#endif

	prQM->u4TimeToAdjustTcResource = (uint32_t) wlanCfgGetUint32(
					prAdapter, "TcAdjustTime",
					QM_INIT_TIME_TO_ADJUST_TC_RSC);
	prQM->u4TimeToUpdateQueLen = (uint32_t) wlanCfgGetUint32(
					prAdapter, "QueLenUpdateTime",
					QM_INIT_TIME_TO_UPDATE_QUE_LEN);
	prQM->u4QueLenMovingAverage = (uint32_t) wlanCfgGetUint32(
					prAdapter, "QueLenMovingAvg",
					QM_QUE_LEN_MOVING_AVE_FACTOR);
	prQM->u4ExtraReservedTcResource = (uint32_t) wlanCfgGetUint32(
					prAdapter, "TcExtraRsv",
					QM_EXTRA_RESERVED_RESOURCE_WHEN_BUSY);
#endif

	/* Stats log */
	prWifiVar->u4StatsLogTimeout = (uint32_t) wlanCfgGetUint32(
					prAdapter, "StatsLogTO",
					WLAN_TX_STATS_LOG_TIMEOUT);
	prWifiVar->u4StatsLogDuration = (uint32_t) wlanCfgGetUint32(
					prAdapter, "StatsLogDur",
					WLAN_TX_STATS_LOG_DURATION);

	prWifiVar->ucDhcpTxDone = (uint8_t) wlanCfgGetUint32(
					prAdapter, "DhcpTxDone", 1);
	prWifiVar->ucArpTxDone = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ArpTxDone", 1);

	prWifiVar->ucMacAddrOverride = (uint8_t) wlanCfgGetInt32(
				       prAdapter, "MacOverride", 0);
	if (wlanCfgGet(prAdapter, "MacAddr", prWifiVar->aucMacAddrStr,
	    "00:0c:e7:66:32:e1", 0))
		DBGLOG(INIT, TRACE, "get MacAddr fail, use defaul\n");

	prWifiVar->ucCtiaMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "CtiaMode", 0);

	/* Combine ucTpTestMode and ucSigmaTestMode in one flag */
	/* ucTpTestMode == 0, for normal driver */
	/* ucTpTestMode == 1, for pure throughput test mode (ex: RvR) */
	/* ucTpTestMode == 2, for sigma TGn/TGac/PMF */
	/* ucTpTestMode == 3, for sigma WMM PS */
	prWifiVar->ucTpTestMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "TpTestMode", 0);
#if CFG_SUPPORT_TPENHANCE_MODE
	/* tp enhance config */
	prWifiVar->ucTpEnhanceEnable = (uint8_t) wlanCfgGetUint32(
					prAdapter, "TpEnhanceEnable", 0);
	prWifiVar->ucTpEnhancePktNum = (uint8_t) wlanCfgGetUint32(
					prAdapter, "TpEnhancePktNum", 20);
	prWifiVar->u4TpEnhanceInterval = (uint32_t) wlanCfgGetUint32(
					prAdapter, "TpEnhanceInterval", 6000);
	prWifiVar->cTpEnhanceRSSI = (int8_t) wlanCfgGetInt32(
					prAdapter, "TpEnhanceRSSI", -65);
	prWifiVar->u4TpEnhanceThreshold = (uint32_t) wlanCfgGetUint32(
					prAdapter, "TpEnhanceThreshold", 300);
#endif /* CFG_SUPPORT_TPENHANCE_MODE */

#if 0
	prWifiVar->ucSigmaTestMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "SigmaTestMode", 0);
#endif

#if CFG_SUPPORT_DBDC
	prWifiVar->eDbdcMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "DbdcMode",
					DEFAULT_DBDC_MODE);
#else
	prWifiVar->eDbdcMode = ENUM_DBDC_MODE_DISABLED;
	prWifiVar->fgDbDcModeEn = false;
#endif /*CFG_SUPPORT_DBDC*/

	prWifiVar->ucCsaDeauthClient = (uint8_t) wlanCfgGetUint32(
					prAdapter, "CsaDeauthClient",
					FEATURE_ENABLED);

#if (CFG_EFUSE_BUFFER_MODE_DELAY_CAL == 1)
	prWifiVar->ucEfuseBufferModeCal = (uint8_t) wlanCfgGetUint32(
					prAdapter, "EfuseBufferModeCal", 0);
#endif
	prWifiVar->ucCalTimingCtrl = (uint8_t) wlanCfgGetUint32(
					prAdapter, "CalTimingCtrl",
					0 /* power on full cal */);
	prWifiVar->ucWow = (uint8_t) wlanCfgGetUint32(
					prAdapter, "Wow", FEATURE_DISABLED);
	prWifiVar->ucOffload = (uint8_t) wlanCfgGetUint32(
					prAdapter, "Offload", FEATURE_DISABLED);
	prWifiVar->ucAdvPws = (uint8_t) wlanCfgGetUint32(
					prAdapter, "AdvPws", FEATURE_DISABLED);
	prWifiVar->ucWowOnMdtim = (uint8_t) wlanCfgGetUint32(
					prAdapter, "WowOnMdtim", 1);
	prWifiVar->ucWowOffMdtim = (uint8_t) wlanCfgGetUint32(
					prAdapter, "WowOffMdtim", 3);
	prWifiVar->ucEapolSuspendOffload = (uint8_t) wlanCfgGetUint32(
					prAdapter, "EapolSuspendOffload",
					FEATURE_DISABLED);

#if CFG_WOW_SUPPORT
	prAdapter->rWowCtrl.fgWowEnable = (uint8_t) wlanCfgGetUint32(
					prAdapter, "WowEnable",
					FEATURE_ENABLED);
	prAdapter->rWowCtrl.ucScenarioId = (uint8_t) wlanCfgGetUint32(
					prAdapter, "WowScenarioId", 0);
	prAdapter->rWowCtrl.ucBlockCount = (uint8_t) wlanCfgGetUint32(
					prAdapter, "WowPinCnt", 1);
	prAdapter->rWowCtrl.astWakeHif[0].ucWakeupHif =
					(uint8_t) wlanCfgGetUint32(
						prAdapter, "WowHif",
						ENUM_HIF_TYPE_GPIO);
	prAdapter->rWowCtrl.astWakeHif[0].ucGpioPin =
		(uint8_t) wlanCfgGetUint32(prAdapter, "WowGpioPin", 0xFF);
	prAdapter->rWowCtrl.astWakeHif[0].ucTriggerLvl =
		(uint8_t) wlanCfgGetUint32(prAdapter, "WowTriigerLevel", 3);
	prAdapter->rWowCtrl.astWakeHif[0].u4GpioInterval =
		wlanCfgGetUint32(prAdapter, "GpioInterval", 0);
	prWifiVar->ucWowDetectType = (uint8_t) wlanCfgGetUint32(
		prAdapter, "WowDetectType", WOWLAN_DETECT_TYPE_MAGIC);
	prWifiVar->u2WowDetectTypeExt = (uint16_t) wlanCfgGetUint32(
		prAdapter, "WowDetectTypeExt", WOWLAN_DETECT_TYPE_EXT_PORT);
#endif

	prWifiVar->u4TxHangFullDumpMode = wlanCfgGetUint32(
			prAdapter, "TxHangFullDumpMode", 0);

	/* SW Test Mode: Mainly used for Sigma */
	prWifiVar->u4SwTestMode = (uint8_t) wlanCfgGetUint32(
					prAdapter, "SwTestMode",
					ENUM_SW_TEST_MODE_NONE);
	prWifiVar->ucCtrlFlagAssertPath = (uint8_t) wlanCfgGetUint32(
					prAdapter, "AssertPath",
					DBG_ASSERT_PATH_DEFAULT);
	prWifiVar->ucCtrlFlagDebugLevel = (uint8_t) wlanCfgGetUint32(
					prAdapter, "AssertLevel",
					DBG_ASSERT_CTRL_LEVEL_DEFAULT);
	prWifiVar->u4ScanCtrl = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ScanCtrl",
					SCN_CTRL_DEFAULT_SCAN_CTRL);
	prWifiVar->ucScanChannelListenTime = (uint8_t) wlanCfgGetUint32(
					prAdapter, "ScnChListenTime", 0);

	/* Wake lock related configuration */
	prWifiVar->u4WakeLockRxTimeout = wlanCfgGetUint32(
					prAdapter, "WakeLockRxTO",
					WAKE_LOCK_RX_TIMEOUT);
	prWifiVar->u4WakeLockThreadWakeup = wlanCfgGetUint32(
					prAdapter, "WakeLockThreadTO",
					WAKE_LOCK_THREAD_WAKEUP_TIMEOUT);

	prWifiVar->ucSmartRTS = (uint8_t) wlanCfgGetUint32(
					prAdapter, "SmartRTS", 0);
	prWifiVar->ePowerMode = (enum PARAM_POWER_MODE) wlanCfgGetUint32(
					prAdapter, "PowerSave",
					Param_PowerModeMax);

	/* add more cfg from RegInfo */
	prWifiVar->u4UapsdAcBmp = (uint32_t) wlanCfgGetUint32(
					prAdapter, "UapsdAcBmp", 0);
	prWifiVar->u4MaxSpLen = (uint32_t) wlanCfgGetUint32(
					prAdapter, "MaxSpLen", 0);
#if CFG_P2P_UAPSD_SUPPORT
	prWifiVar->u4P2pUapsdAcBmp = (uint32_t) wlanCfgGetUint32(
					prAdapter, "P2pUapsdAcBmp",
					PM_UAPSD_ALL);
	prWifiVar->u4P2pMaxSpLen = (uint32_t) wlanCfgGetUint32(
					prAdapter, "P2pMaxSpLen",
					WMM_MAX_SP_LENGTH_2);
#else
	prWifiVar->u4P2pUapsdAcBmp = (uint32_t) wlanCfgGetUint32(
					prAdapter, "P2pUapsdAcBmp",
					0);
	prWifiVar->u4P2pMaxSpLen = (uint32_t) wlanCfgGetUint32(
					prAdapter, "P2pMaxSpLen",
					0);
#endif
	prWifiVar->fgDisOnlineScan = (uint32_t) wlanCfgGetUint32(
					prAdapter, "DisOnlineScan", 0);
	prWifiVar->fgDisBcnLostDetection = (uint32_t) wlanCfgGetUint32(
					prAdapter, "DisBcnLostDetection", 0);
	prWifiVar->fgDisAgingLostDetection = (uint32_t) wlanCfgGetUint32(
					prAdapter, "DisAgingLostDetection", 0);
	prWifiVar->fgDisRoaming = (uint32_t) wlanCfgGetUint32(
					prAdapter, "DisRoaming", 0);
	prWifiVar->u4AisRoamingNumber = (uint32_t) wlanCfgGetUint32(
					prAdapter, "AisRoamingNumber",
					KAL_AIS_NUM);
	prWifiVar->fgEnArpFilter = (uint32_t) wlanCfgGetUint32(
					prAdapter, "EnArpFilter",
					FEATURE_ENABLED);

	/* Driver Flow Control Dequeue Quota. Now is only used by DBDC */
	prWifiVar->uDeQuePercentEnable =
		(uint8_t) wlanCfgGetUint32(prAdapter, "DeQuePercentEnable", 1);
	prWifiVar->u4DeQuePercentVHT80Nss1 =
		(uint32_t) wlanCfgGetUint32(prAdapter, "DeQuePercentVHT80NSS1",
					QM_DEQUE_PERCENT_VHT80_NSS1);
	prWifiVar->u4DeQuePercentVHT40Nss1 =
		(uint32_t) wlanCfgGetUint32(prAdapter, "DeQuePercentVHT40NSS1",
					QM_DEQUE_PERCENT_VHT40_NSS1);
	prWifiVar->u4DeQuePercentVHT20Nss1 =
		(uint32_t) wlanCfgGetUint32(prAdapter, "DeQuePercentVHT20NSS1",
					QM_DEQUE_PERCENT_VHT20_NSS1);
	prWifiVar->u4DeQuePercentHT40Nss1 =
		(uint32_t) wlanCfgGetUint32(prAdapter, "DeQuePercentHT40NSS1",
					QM_DEQUE_PERCENT_HT40_NSS1);
	prWifiVar->u4DeQuePercentHT20Nss1 =
		(uint32_t) wlanCfgGetUint32(prAdapter, "DeQuePercentHT20NSS1",
					QM_DEQUE_PERCENT_HT20_NSS1);

	/* Support TDLS 5.5.4.2 optional case */
	prWifiVar->fgTdlsBufferSTASleep = (u_int8_t) wlanCfgGetUint32(prAdapter,
					"TdlsBufferSTASleep", FEATURE_ENABLED);

	prWifiVar->u4PerfMonUpdatePeriod =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonPeriod",
					    PERF_MON_UPDATE_INTERVAL);
	if (prWifiVar->u4PerfMonUpdatePeriod < PERF_MON_UPDATE_MIN_INTERVAL) {
		prWifiVar->u4PerfMonUpdatePeriod = PERF_MON_UPDATE_MIN_INTERVAL;
		DBGLOG(INIT, TRACE, "u4PerfMonUpdatePeriod set to min(%d).\n",
				PERF_MON_UPDATE_MIN_INTERVAL);
	}

	prWifiVar->u4PerfMonTpTh[0] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv1", 20);
	prWifiVar->u4PerfMonTpTh[1] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv2", 50);
	prWifiVar->u4PerfMonTpTh[2] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv3", 100);
	prWifiVar->u4PerfMonTpTh[3] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv4", 180);
	prWifiVar->u4PerfMonTpTh[4] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv5", 250);
	prWifiVar->u4PerfMonTpTh[5] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv6", 300);
	prWifiVar->u4PerfMonTpTh[6] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv7", 700);
	prWifiVar->u4PerfMonTpTh[7] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv8", 1200);
	prWifiVar->u4PerfMonTpTh[8] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv9", 2000);
	prWifiVar->u4PerfMonTpTh[9] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "PerfMonLv10", 3500);

#if CFG_DYNAMIC_RFB_ADJUSTMENT
	prWifiVar->u4RfbBoostTpTh[0] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "RfbBoostTpTh0", 50);
	prWifiVar->u4RfbBoostTpTh[1] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "RfbBoostTpTh1", 300);
	prWifiVar->u4RfbBoostTpTh[2] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "RfbBoostTpTh2", 1200);
	prWifiVar->u4RfbInUseCnt[0] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "RfbInUseCnt0",
			(CFG_RX_MAX_PKT_NUM >> 2) * 3);
	prWifiVar->u4RfbInUseCnt[1] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "RfbInUseCnt1",
			(CFG_RX_MAX_PKT_NUM >> 1));
	prWifiVar->u4RfbInUseCnt[2] =
		(uint32_t) wlanCfgGetUint32(prAdapter, "RfbInUseCnt2", 0);
	/* just set it, not need to adjust it immediately */
	nicRxSetInUseCnt(prAdapter, prWifiVar->u4RfbInUseCnt[0], FALSE);
#endif /* CFG_DYNAMIC_RFB_ADJUSTMENT */

#if CFG_SUPPORT_MCC_BOOST_CPU
	prWifiVar->u4MccBoostTputLvTh =
		(uint32_t) wlanCfgGetUint32(prAdapter, "MccBoostTputLvTh",
			MCC_BOOST_LEVEL);
	prWifiVar->u4MccBoostPresentTime =
		(uint32_t) wlanCfgGetUint32(prAdapter,
			"MccBoostPresentTimeMin", MCC_BOOST_MIN_TIME);
#endif /* CFG_SUPPORT_MCC_BOOST_CPU */

#if CFG_SUPPORT_LLS
	prWifiVar->fgLinkStatsDump = (bool)wlanCfgGetUint32(
		prAdapter, "LinkStatsDump", 0);
	prWifiVar->ucLinkStatsQueryMode = (enum LLS_QUERY_MODE)wlanCfgGetUint32(
		prAdapter, "LinkStatsQuery", ALWAYS_SEND_CMD);
#endif

#if CFG_SUPPORT_TX_LATENCY_STATS
	prWifiVar->fgPacketLatencyLog = (bool)wlanCfgGetUint32(
		prAdapter, "TxLatencyPacketLog", 0);

	prWifiVar->fgTxLatencyKeepCounting = (bool)wlanCfgGetUint32(
		prAdapter, "TxLatencyKeepCounting", 0);

	prWifiVar->fgTxLatencyPerBss = (bool)wlanCfgGetUint32(
		prAdapter, "TxLatencyPerBss", 0);

	prWifiVar->u4MsduStatsUpdateInterval = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyUpdateInterval",
		TX_LATENCY_STATS_UPDATE_INTERVAL);
	prWifiVar->u4ContinuousTxFailThreshold = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyContinuousFailThrehold",
		TX_LATENCY_STATS_CONTINUOUS_FAIL_THREHOLD);

	prWifiVar->au4DriverTxDelayMax[0] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyDriverDelayMaxL1",
		TX_LATENCY_STATS_MAX_DRIVER_DELAY_L1);
	prWifiVar->au4DriverTxDelayMax[1] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyDriverDelayMaxL2",
		TX_LATENCY_STATS_MAX_DRIVER_DELAY_L2);
	prWifiVar->au4DriverTxDelayMax[2] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyDriverDelayMaxL3",
		TX_LATENCY_STATS_MAX_DRIVER_DELAY_L3);
	prWifiVar->au4DriverTxDelayMax[3] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyDriverDelayMaxL4",
		TX_LATENCY_STATS_MAX_DRIVER_DELAY_L4);
	prWifiVar->au4DriverTxDelayMax[4] = UINT_MAX;

	prWifiVar->au4ConnsysTxDelayMax[0] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyConnsysDelayMaxL1",
		TX_LATENCY_STATS_MAX_CONNSYS_DELAY_L1);
	prWifiVar->au4ConnsysTxDelayMax[1] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyConnsysDelayMaxL2",
		TX_LATENCY_STATS_MAX_CONNSYS_DELAY_L2);
	prWifiVar->au4ConnsysTxDelayMax[2] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyConnsysDelayMaxL3",
		TX_LATENCY_STATS_MAX_CONNSYS_DELAY_L3);
	prWifiVar->au4ConnsysTxDelayMax[3] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyConnsysDelayMaxL4",
		TX_LATENCY_STATS_MAX_CONNSYS_DELAY_L4);
	prWifiVar->au4ConnsysTxDelayMax[4] = UINT_MAX;

	prWifiVar->au4MacTxDelayMax[0] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyMacDelayMaxL1",
		TX_LATENCY_STATS_MAX_MAC_DELAY_L1);
	prWifiVar->au4MacTxDelayMax[1] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyMacDelayMaxL2",
		TX_LATENCY_STATS_MAX_MAC_DELAY_L2);
	prWifiVar->au4MacTxDelayMax[2] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyMacDelayMaxL3",
		TX_LATENCY_STATS_MAX_MAC_DELAY_L3);
	prWifiVar->au4MacTxDelayMax[3] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyMacDelayMaxL4",
		TX_LATENCY_STATS_MAX_MAC_DELAY_L4);
	prWifiVar->au4MacTxDelayMax[4] = UINT_MAX;

	prWifiVar->au4AirTxDelayMax[0] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyAirDelayMaxL1",
		TX_LATENCY_STATS_MAX_AIR_DELAY_L1);
	prWifiVar->au4AirTxDelayMax[1] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyAirDelayMaxL2",
		TX_LATENCY_STATS_MAX_AIR_DELAY_L2);
	prWifiVar->au4AirTxDelayMax[2] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyAirDelayMaxL3",
		TX_LATENCY_STATS_MAX_AIR_DELAY_L3);
	prWifiVar->au4AirTxDelayMax[3] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyAirDelayMaxL4",
		TX_LATENCY_STATS_MAX_AIR_DELAY_L4);
	prWifiVar->au4AirTxDelayMax[4] = UINT_MAX;

	prWifiVar->au4ConnsysTxFailDelayMax[0] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyFailConnsysDelayMaxL1",
		TX_LATENCY_STATS_MAX_FAIL_CONNSYS_DELAY_L1);
	prWifiVar->au4ConnsysTxFailDelayMax[1] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyFailConnsysDelayMaxL2",
		TX_LATENCY_STATS_MAX_FAIL_CONNSYS_DELAY_L2);
	prWifiVar->au4ConnsysTxFailDelayMax[2] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyFailConnsysDelayMaxL3",
		TX_LATENCY_STATS_MAX_FAIL_CONNSYS_DELAY_L3);
	prWifiVar->au4ConnsysTxFailDelayMax[3] = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TxLatencyFailConnsysDelayMaxL4",
		TX_LATENCY_STATS_MAX_FAIL_CONNSYS_DELAY_L4);
	prWifiVar->au4ConnsysTxFailDelayMax[4] = UINT_MAX;
#endif /* CFG_SUPPORT_TX_LATENCY_STATS */

	prWifiVar->fgBoostCpuEn =
		(uint32_t) wlanCfgGetUint32(prAdapter, "BoostCpuEn",
			FEATURE_ENABLED);
	u4PlatformBoostCpuTh = kalGetCpuBoostThreshold();
	prWifiVar->u4BoostCpuTh =
		(uint32_t) wlanCfgGetUint32(prAdapter, "BoostCpuTh",
			u4PlatformBoostCpuTh);
	prWifiVar->fgIsBoostCpuThAdjustable = FALSE;

	if (!wlanCfgGetEntry(prAdapter, "BoostCpuTh", FALSE)) {
		prWifiVar->fgIsBoostCpuThAdjustable  = TRUE;
		DBGLOG(INIT, TRACE, "BoostCPUTh is not config, adjustable\n");
	}
	prWifiVar->au4CpuBoostMinFreq = (uint32_t)wlanCfgGetUint32(
		prAdapter, "CpuBoostMinFreq", 1300);
#if CFG_SUPPORT_LITTLE_CPU_BOOST
	u4PlatformBoostLittleCpuTh = kalGetLittleCpuBoostThreshold();
	prWifiVar->u4BoostLittleCpuTh =
		(uint32_t) wlanCfgGetUint32(prAdapter, "BoostLittleCpuTh",
			u4PlatformBoostLittleCpuTh);
#endif /* CFG_SUPPORT_LITTLE_CPU_BOOST */

	/**
	 * A debugging switch for development phase to check the difference of
	 * tput imposed by SW reordering, including the operation workload and
	 * frame drops, etc.
	 */
	prWifiVar->fgSwRxReordering = wlanCfgGetUint32(prAdapter,
					"SwRxReordering", FEATURE_ENABLED);

	prWifiVar->fgFlushRxReordering = wlanCfgGetUint32(prAdapter,
					"FlushRxReordering", FEATURE_ENABLED);


	/**
	 * A debugging switch enables RXD, RXP dumping when driver drops packets
	 * for ICV error.
	 */
	prWifiVar->fgRxIcvErrDbg = wlanCfgGetUint32(prAdapter,
					"RxIcvErrDbg", FEATURE_DISABLED);
	/**
	 * Switching dumping TX/RX memory, set by bitmap format.
	 * TXP(0x04),        TXDMAD(0x02), TXD(0x01),
	 * RXDSEGMENT(0x40), RXDMAD(0x20), RXD(0x10).
	 */
	prWifiVar->u4TxRxDescDump = wlanCfgGetUint32(prAdapter,
					"TRXDescDump", 0x40);
	DBGLOG(INIT, TRACE,
		"TxP,TxDmad,TxD/RxDsegment,RxDmad,RxD,RxEvt=%u,%u,%u/%u,%u,%u,%u",
		prWifiVar->fgDumpTxP, prWifiVar->fgDumpTxDmad,
		prWifiVar->fgDumpTxD,
		prWifiVar->fgDumpRxDsegment, prWifiVar->fgDumpRxDmad,
		prWifiVar->fgDumpRxD, prWifiVar->fgDumpRxEvt);

#if CFG_SUPPORT_LOWLATENCY_MODE
	prWifiVar->u4BaShortMissTimeoutMs = wlanCfgGetUint32(prAdapter,
					"BaShortMissTimeoutMs",
					QM_RX_BA_ENTRY_MISS_TIMEOUT_MS_SHORT);
#endif
	prWifiVar->u4BaMissTimeoutMs = wlanCfgGetUint32(prAdapter,
					"BaMissTimeoutMs",
					QM_RX_BA_ENTRY_MISS_TIMEOUT_MS);

	prWifiVar->u4PerfMonPendingTh = (uint8_t)wlanCfgGetUint32(prAdapter,
						"PerfMonPendingTh", 80);

	prWifiVar->u4PerfMonUsedTh = (uint8_t)wlanCfgGetUint32(prAdapter,
						"PerfMonUsedTh", 80);

	/* SER related fields */

	/* for L0 SER */
	prWifiVar->fgEnableSerL0 = (uint8_t)wlanCfgGetUint32(prAdapter,
							     "EnableSerL0",
							     FEATURE_ENABLED);

	/* for L0.5 SER */
	prWifiVar->eEnableSerL0p5 = (enum ENUM_FEATURE_OPTION_IN_SER)
				     wlanCfgGetUint32(prAdapter,
						      "EnableSerL0p5",
						      FEATURE_OPT_SER_ENABLE);

	/* for L1 SER */
	prWifiVar->eEnableSerL1 = (enum ENUM_FEATURE_OPTION_IN_SER)
				     wlanCfgGetUint32(prAdapter,
							  "EnableSerL1",
						      FEATURE_OPT_SER_ENABLE);

	/* for L0 SER using WDT on some legacy CE USB project like MT7668 and
	 * MT7663. The difference between fgEnableSerL0 and fgChipResetRecover
	 * is that the former one toggles reset pin and the latter one sends
	 * USB EP0 vendor request to fw, which makes fw assert and triggers
	 * L0 reset when watchdog timeout. The purpose to reserve this mothod
	 * is that some customers might not have dedicated reset pin on the
	 * platform.
	 */
	prWifiVar->fgChipResetRecover = (u_int8_t) wlanCfgGetUint32(prAdapter,
					"ChipResetRecover", FEATURE_DISABLED);

	prWifiVar->fgRstRecover = (uint8_t) wlanCfgGetUint32(prAdapter,
					"RstRecover", FEATURE_DISABLED);
	/*
	 * For Certification purpose,forcibly set
	 * "Compressed Steering Number of Beamformer Antennas Supported" to our
	 * own capability.
	 */
	prWifiVar->fgForceSTSNum = (uint8_t)wlanCfgGetUint32(
					   prAdapter, "ForceSTSNum", 0);
#if CFG_SUPPORT_IDC_CH_SWITCH
	prWifiVar->ucChannelSwtichColdownTime = (uint8_t) wlanCfgGetUint32(
			prAdapter, "CSACdTime", 60);/*Second*/
	prWifiVar->fgCrossBandSwitchEn = (uint8_t) wlanCfgGetUint32(
			prAdapter, "SapCrossBandSwitchEn", 0);
#endif
#if CFG_SUPPORT_PERF_IND
	prWifiVar->fgPerfIndicatorEn = (uint8_t) wlanCfgGetUint32(
			prAdapter, "PerfIndicatorEn", 1);
#endif
#if CFG_SUPPORT_SPE_IDX_CONTROL
	prWifiVar->ucSpeIdxCtrl = (uint8_t) wlanCfgGetUint32(
					prAdapter, "SpeIdxCtrl", 2);
#endif

#if CFG_SUPPORT_LOWLATENCY_MODE
	prWifiVar->ucLowLatencyModeScan = (uint32_t) wlanCfgGetUint32(
			prAdapter, "LowLatencyModeScan", FEATURE_ENABLED);
	prWifiVar->ucLowLatencyModeReOrder = (uint32_t) wlanCfgGetUint32(
			prAdapter, "LowLatencyModeReOrder", FEATURE_ENABLED);
	prWifiVar->ucLowLatencyModePower = (uint32_t) wlanCfgGetUint32(
			prAdapter, "LowLatencyModePower", FEATURE_ENABLED);
	prWifiVar->ucLowLatencyPacketPriority = (uint32_t) wlanCfgGetUint32(
			prAdapter, "LowLatencyPacketPriority", BITS(0, 1));
	prWifiVar->ucLowLatencyCmdData = (uint8_t) wlanCfgGetUint32(
			prAdapter, "LowLatencyCmdData", FEATURE_ENABLED);
	prWifiVar->ucLowLatencyCmdDataAllPacket = (uint8_t) wlanCfgGetUint32(
		prAdapter, "LowLatencyCmdDataAllPacket", FEATURE_DISABLED);
#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

#if CFG_SUPPORT_SPE_IDX_CONTROL
	prWifiVar->ucSpeIdxCtrl = (uint8_t) wlanCfgGetUint32(
					prAdapter, "SpeIdxCtrl", 2);
#endif

	prWifiVar->u4MTU = wlanCfgGetUint32(prAdapter, "MTU", 0);

#if CFG_SUPPORT_RX_GRO
	prWifiVar->ucGROFlushTimeout = (uint32_t) wlanCfgGetUint32(
			prAdapter, "GROFlushTimeout", 1);
	prWifiVar->ucGROEnableTput = (uint32_t) wlanCfgGetUint32(
			prAdapter, "GROEnableTput", 6250000);
#endif
	prWifiVar->ucMsduReportTimeout =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"MsduReportTimeout", NIC_MSDU_REPORT_DUMP_TIMEOUT);
	prWifiVar->ucMsduReportTimeoutSerTime =
		(uint8_t) wlanCfgGetUint32(prAdapter,
		"MsduReportTimeoutSerTime", NIC_MSDU_REPORT_TIMEOUT_SER_TIME);

#if CFG_SUPPORT_DATA_STALL
	prWifiVar->u4PerHighThreshole = (uint32_t) wlanCfgGetUint32(
			prAdapter, "PerHighThreshole",
			EVENT_PER_HIGH_THRESHOLD);
	prWifiVar->u4TxLowRateThreshole = (uint32_t) wlanCfgGetUint32(
			prAdapter, "TxLowRateThreshole",
			EVENT_TX_LOW_RATE_THRESHOLD);
	prWifiVar->u4RxLowRateThreshole = (uint32_t) wlanCfgGetUint32(
			prAdapter, "RxLowRateThreshole",
			EVENT_RX_LOW_RATE_THRESHOLD);
	prWifiVar->u4ReportEventInterval = (uint32_t) wlanCfgGetUint32(
			prAdapter, "ReportEventInterval",
			REPORT_EVENT_INTERVAL);
	prWifiVar->u4TrafficThreshold = (uint32_t) wlanCfgGetUint32(
			prAdapter, "TrafficThreshold",
			TRAFFIC_RHRESHOLD);
#endif

#if CFG_SUPPORT_HE_ER
	prWifiVar->u4ExtendedRange = (uint32_t) wlanCfgGetUint32(
			prAdapter, "ExtendedRange",
			FEATURE_ENABLED);
	prWifiVar->fgErTx = (uint32_t) wlanCfgGetUint32(
			prAdapter, "ErTx",
			FEATURE_ENABLED);
	prWifiVar->fgErRx = (uint32_t) wlanCfgGetUint32(
			prAdapter, "ErRx",
			FEATURE_ENABLED);
	prWifiVar->fgErSuRx = (uint32_t) wlanCfgGetUint32(
			prAdapter, "ErSuRx",
			FEATURE_ENABLED);
#endif
#if (CFG_SUPPORT_BSS_MAX_IDLE_PERIOD == 1)
	prWifiVar->fgBssMaxIdle = (uint32_t) wlanCfgGetUint32(
			prAdapter, "BssMaxIdle",
			FEATURE_DISABLED);
	prWifiVar->u2BssMaxIdlePeriod = (uint32_t) wlanCfgGetUint32(
			prAdapter, "BssMaxIdlePeriod",
			BSS_MAX_IDLE_PERIOD_VALUE);
#endif

#if (CFG_SUPPORT_P2PGO_ACS == 1)
	prWifiVar->ucP2pGoACS = (uint32_t) wlanCfgGetUint32(
			prAdapter, "P2pGoACSEnable",
			FEATURE_ENABLED);
#endif

#if CFG_SUPPORT_NAN
	prWifiVar->ucMasterPref =
		(uint8_t)wlanCfgGetUint32(prAdapter, "NanMasterPref", 2);
	prWifiVar->ucConfig5gChannel =
		(uint8_t)wlanCfgGetUint32(prAdapter, "NanConfig5gChannel", 1);
	prWifiVar->ucChannel5gVal =
		(uint8_t)wlanCfgGetUint32(prAdapter, "NanChannel5gVal", 149);
	prWifiVar->ucAisQuotaVal =
		(uint8_t)wlanCfgGetUint32(prAdapter, "NanAisQuota", 8);
	prWifiVar->ucDftNdlQuotaVal =
		(uint8_t)wlanCfgGetUint32(prAdapter, "NanDftNdlQuota",
#if (CFG_NAN_SCHEDULER_VERSION == 1)
		NAN_DEFAULT_NDL_QUOTA_UP_BOUND);
#else
		5);
#endif
	prWifiVar->ucDftRangQuotaVal =
		(uint8_t)wlanCfgGetUint32(prAdapter, "NanDftRangQuota", 1);
	prWifiVar->ucDftQuotaStartOffset =
		(uint8_t)wlanCfgGetUint32(prAdapter,
		"NanDftQuotaStartOffset", 2);
	prWifiVar->ucDftNdcStartOffset =
		(uint8_t)wlanCfgGetUint32(prAdapter, "NanDftNdcStartOffset", 0);
	prWifiVar->ucNanFixChnl =
		(uint8_t)wlanCfgGetUint32(prAdapter, "NanFixChnl", 0);
	prWifiVar->fgEnableNDPE =
		wlanCfgGetUint32(prAdapter, "NanEnableNDPE", 1);
	prWifiVar->u2DftNdlQosLatencyVal =
		wlanCfgGetUint32(prAdapter, "NanDftNdlQosLatency", 0);
	prWifiVar->ucDftNdlQosQuotaVal =
		(uint8_t)wlanCfgGetUint32(prAdapter, "NanDftNdlQosQuota", 0);
	prWifiVar->fgEnNanVHT =
		(unsigned char)wlanCfgGetUint32(prAdapter, "NanVHT", 1);
	prWifiVar->ucNanFtmBw = (uint8_t)wlanCfgGetUint32(
		prAdapter, "NanFtmBw", FTM_FORMAT_BW_HT_MIXED_BW20);
	prWifiVar->ucNanDiscBcnInterval =
		(uint8_t)wlanCfgGetUint32(prAdapter, "NanDiscBcnInterval", 100);
	prWifiVar->ucNanCommittedDw =
		(uint8_t)wlanCfgGetUint32(prAdapter, "NanDftCommittedDw", 1);
	prWifiVar->fgNoPmf =
		(unsigned char)wlanCfgGetUint32(prAdapter, "NanForceNoPmf", 0);
	prWifiVar->ucNan2gBandwidth = (uint8_t) wlanCfgGetUint32(
			prAdapter, "Nan2gBw", MAX_BW_20MHZ);
	prWifiVar->ucNan5gBandwidth = (uint8_t) wlanCfgGetUint32(
			prAdapter, "Nan5gBw", MAX_BW_80MHZ);
	prWifiVar->ucNdlFlowCtrlVer =
		(unsigned char)wlanCfgGetUint32(prAdapter,
		"NanNdlFlowCtrlVer",
		CFG_SUPPORT_NAN_ADVANCE_DATA_CONTROL);
#endif

	prWifiVar->fgReuseRSNIE = (uint32_t) wlanCfgGetUint32(
			prAdapter, "ReuseRSNIE",
			FEATURE_DISABLED);

#if CFG_COALESCING_INTERRUPT
	prWifiVar->u2CoalescingIntMaxPk = (uint16_t) wlanCfgGetUint32(
			prAdapter, "CoalescingIntMaxPkt",
			COALESCING_INT_MAX_PKT);
	prWifiVar->u2CoalescingIntMaxTime = (uint16_t) wlanCfgGetUint32(
			prAdapter, "CoalescingIntMaxTime",
			COALESCING_INT_MAX_TIME);
	prWifiVar->u2CoalescingIntuFilterMask = (uint16_t) wlanCfgGetUint32(
			prAdapter, "CoalescingIntFilterMask",
			CMD_PF_CF_COALESCING_INT_FILTER_MASK_DEFAULT);
	prWifiVar->u4PerfMonTpCoalescingIntTh = (uint32_t) wlanCfgGetUint32(
			prAdapter, "PerfMonTpCoalescingIntTh",
			6);
	prWifiVar->fgCoalescingIntEn = (u_int8_t) wlanCfgGetUint32(
			prAdapter, "CoalescingIntEn",
			FEATURE_DISABLED);
#endif

#if CFG_MTK_MDDP_SUPPORT
	prWifiVar->fgMddpSupport = (u_int8_t) wlanCfgGetUint32(
		prAdapter, "MddpSupport", FEATURE_ENABLED);
	wlanCfgSetUint32(prAdapter, "MddpSupport", prWifiVar->fgMddpSupport);
#else
	wlanCfgSetUint32(prAdapter, "MddpSupport", FEATURE_ENABLED);
#endif

	prWifiVar->u4DiscoverTimeout = (uint32_t) wlanCfgGetUint32(
		prAdapter, "DiscoverTimeout", ROAMING_DISCOVER_TIMEOUT_SEC);

#if (CFG_DBDC_SW_FOR_P2P_LISTEN == 1)
	prWifiVar->ucDbdcP2pLisEn =
		(uint8_t) wlanCfgGetUint32(
				prAdapter, "DbdcP2pLisEn",
				FEATURE_ENABLED);

	prWifiVar->u4DbdcP2pLisSwDelayTime =
	(uint32_t) wlanCfgGetUint32(
			prAdapter, "DbdcP2pLisSwDelayTime",
			DBDC_P2P_LISTEN_SW_DELAY_TIME);
#endif

	prWifiVar->ucDisallowBand2G = (uint8_t) wlanCfgGetUint32(
		prAdapter, "DisallowBand2G", 0);
	prWifiVar->ucDisallowBand5G = (uint8_t) wlanCfgGetUint32(
		prAdapter, "DisallowBand5G", 0);
#if (CFG_SUPPORT_WIFI_6G == 1)
	prWifiVar->ucDisallowBand6G = (uint8_t) wlanCfgGetUint32(
		prAdapter, "DisallowBand6G", 0);
#endif
	prWifiVar->u4InactiveTimeout = (uint32_t) wlanCfgGetUint32(
		prAdapter, "InactiveTimeout", ROAMING_INACTIVE_TIMEOUT_SEC);

	prWifiVar->u4BtmDelta = (uint32_t) wlanCfgGetUint32(
		prAdapter, "BtmDelta", ROAMING_BTM_DELTA);
	prWifiVar->u4BtmDisTimerThreshold = (uint32_t) wlanCfgGetUint32(
		prAdapter, "BtmDisTimerThreshold", AIS_BTM_DIS_IMMI_TIMEOUT);

#if ARP_MONITER_ENABLE
	prWifiVar->uArpMonitorNumber = (uint32_t) wlanCfgGetUint32(
		prAdapter, "ArpMonitorNumber", 5);
	prWifiVar->uArpMonitorRxPktNum = (uint32_t) wlanCfgGetUint32(
		prAdapter, "ArpMonitorRxPktNum", 0);
	prWifiVar->uArpMonitorCriticalThres = (uint8_t) wlanCfgGetUint32(
		prAdapter, "ArpMonitorCriticalThres", 2);
	prWifiVar->ucArpMonitorUseRule = (uint8_t) wlanCfgGetUint32(
		prAdapter, "ArpMonitorUseRule", 1);
#endif /* ARP_MONITER_ENABLE */


#if CFG_RFB_TRACK
	prWifiVar->fgRfbTrackEn = (uint8_t) wlanCfgGetUint32(
		prAdapter, "RfbTrackEn", FEATURE_DISABLED);
	/* unit: second */
	prWifiVar->u4RfbTrackInterval = (uint32_t) wlanCfgGetUint32(
		prAdapter, "RfbTrackInterval", RFB_TRACK_INTERVAL);
	prWifiVar->u4RfbTrackTimeout = (uint32_t) wlanCfgGetUint32(
		prAdapter, "RfbTrackTimeout", RFB_TRACK_TIMEOUT);
#endif /* CFG_RFB_TRACK */

#if CFG_SUPPORT_SCAN_NO_AP_RECOVERY
	prWifiVar->ucScanNoApRecover = (uint32_t) wlanCfgGetUint32(
		prAdapter, "ScanNoApRecover", FEATURE_ENABLED);
	prWifiVar->ucScanNoApRecoverTh = (uint32_t) wlanCfgGetUint32(
		prAdapter, "ucScanNoApRecoverTh", 3);

#endif

	prWifiVar->fgSapCheckPmkidInDriver = (uint32_t) wlanCfgGetUint32(
		prAdapter, "SapCheckPmkidInDriver", FEATURE_ENABLED);

	prWifiVar->fgSapOffload = (uint32_t) wlanCfgGetUint32(
		prAdapter, "SapOffload", FEATURE_DISABLED);

	prWifiVar->fgSapSkipObss = (uint32_t) wlanCfgGetUint32(
		prAdapter, "SapSkipObss", FEATURE_DISABLED);

	prWifiVar->fgP2pGcCsa = (uint32_t) wlanCfgGetUint32(
		prAdapter, "P2pGcCsa", FEATURE_ENABLED);

	prWifiVar->fgSkipP2pIe = (uint32_t) wlanCfgGetUint32(
		prAdapter, "SkipP2pIe", FEATURE_ENABLED);

	prWifiVar->fgSkipP2pProbeResp = (uint32_t) wlanCfgGetUint32(
		prAdapter, "SkipP2pProbeResp", FEATURE_ENABLED);

	prWifiVar->fgSapChannelSwitchPolicy = (uint32_t) wlanCfgGetUint32(
		prAdapter, "SapChannelSwitchPolicy",
		P2P_CHANNEL_SWITCH_POLICY_SCC);

	prWifiVar->fgSapConcurrencyPolicy = (uint32_t) wlanCfgGetUint32(
		prAdapter, "SapConcurrencyPolicy",
		P2P_CONCURRENCY_POLICY_REMOVE);

	prWifiVar->fgSapAuthPolicy = (uint32_t) wlanCfgGetUint32(
		prAdapter, "SapAuthPolicy",
		P2P_AUTH_POLICY_NONE);

	prWifiVar->fgSapOverwriteAcsChnlBw = (uint32_t) wlanCfgGetUint32(
		prAdapter, "SapOverwriteAcsChnlBw", FEATURE_ENABLED);

	prWifiVar->fgSapAddTPEIE = (uint32_t) wlanCfgGetUint32(
		prAdapter, "SapAddTPEIE", FEATURE_DISABLED);

	prWifiVar->ucDfsRegion = (uint32_t) wlanCfgGetUint32(
		prAdapter, "DfsRegion", NL80211_DFS_UNSET);
	if (prWifiVar->ucDfsRegion)
		rlmDomainSetDfsRegion(prWifiVar->ucDfsRegion);

	prWifiVar->u4ByPassCacTime = (uint32_t) wlanCfgGetUint32(
		prAdapter, "ByPassCacTime", 0);
	if (prWifiVar->u4ByPassCacTime) {
		p2pFuncEnableManualCac();
		p2pFuncSetDriverCacTime(prWifiVar->u4ByPassCacTime);
	}

	prWifiVar->u4CC2Region = (uint32_t) wlanCfgGetUint32(
		prAdapter, "CC2Region", FEATURE_ENABLED);

#if CFG_SUPPORT_TDLS_P2P_AUTO
	prWifiVar->u4TdlsP2pAuto = (uint32_t) wlanCfgGetUint32(
		prAdapter, "TdlsP2pAuto", FEATURE_ENABLED);
#endif

	prWifiVar->u4ApChnlHoldTime = (uint32_t) wlanCfgGetUint32(
		prAdapter, "ApChnlHoldTime", P2P_AP_CHNL_HOLD_TIME_MS);

	prWifiVar->u4P2pChnlHoldTime = (uint32_t) wlanCfgGetUint32(
		prAdapter, "P2pChnlHoldTime", P2P_AP_CHNL_HOLD_TIME_MS);

	prWifiVar->u4ProbeRspRetryLimit = (uint32_t) wlanCfgGetUint32(
		prAdapter, "ProbeRspRetryLimit",
		DEFAULT_P2P_PROBERESP_RETRY_LIMIT);

	prWifiVar->fgAllowSameBandDualSta = (uint8_t) wlanCfgGetUint32(
		prAdapter, "AllowSameBandDualSta", FEATURE_ENABLED);
#if CFG_MODIFY_TX_POWER_BY_BAT_VOLT
	prWifiVar->u4BackoffLevel = (uint32_t) wlanCfgGetUint32(
		prAdapter, "BackoffLevel", 0);
#endif

#if (CFG_SUPPORT_POWER_THROTTLING == 1)
	prWifiVar->ucAbnWakeupDetectEn = (uint32_t) wlanCfgGetUint32(
		prAdapter, "AbnWakeupDetectEn", FEATURE_ENABLED);
	prWifiVar->ucAbnWakeupPktThld = (uint32_t) wlanCfgGetUint32(
			prAdapter, "AbnWakeupPktThld", 10);
	prWifiVar->ucAbnWakeupDetectIntv = (uint32_t) wlanCfgGetUint32(
			prAdapter, "AbnWakeupDetectIntv", 50);
#endif

#if (CFG_SUPPORT_APF == 1)
	prWifiVar->ucApfEnable = (uint32_t) wlanCfgGetUint32(
			prAdapter, "ApfEnable", FEATURE_ENABLED);
#endif

#if CFG_SUPPORT_DISABLE_DATA_DDONE_INTR
	prWifiVar->u4TputThresholdMbps = (uint32_t) wlanCfgGetUint32(
			prAdapter, "TputThresholdMbps", 50);
#endif /* CFG_SUPPORT_DISABLE_DATA_DDONE_INTR */

	prWifiVar->u4RxRateProtoFilterMask = (uint32_t) wlanCfgGetUint32(
		prAdapter, "RxRateProtoFilterMask", BIT(ENUM_PKT_ARP));

#if CFG_SUPPORT_BAR_DELAY_INDICATION
	prWifiVar->fgBARDelayIndicationEn = (uint8_t) wlanCfgGetUint32(
		prAdapter, "BARDelayIndicationEn", FEATURE_ENABLED);
#endif /* CFG_SUPPORT_BAR_DELAY_INDICATION */
	prWifiVar->u4MultiStaPrimaryQuoteTime = (uint32_t) wlanCfgGetUint32(
		prAdapter, "MultiStaPrimaryQuoteTime", 300000);
	prWifiVar->u4MultiStaSecondaryQuoteTime = (uint32_t) wlanCfgGetUint32(
		prAdapter, "MultiStaSecondaryQuoteTime", 120000);
#if CFG_SUPPORT_LIMITED_PKT_PID
	prWifiVar->u4PktPIDTimeout = (uint32_t) wlanCfgGetUint32(
		prAdapter, "PktPIDTimeout", 1000);
#endif /* CFG_SUPPORT_LIMITED_PKT_PID */
#if CFG_SUPPORT_ICS_TIMESYNC
	prWifiVar->u4IcsTimeSyncCnt = (uint32_t) wlanCfgGetUint32(
		prAdapter, "IcsTimeSyncCnt", 1000);
#endif /* CFG_SUPPORT_ICS_TIMESYNC */
#if (CFG_SUPPORT_WIFI_6G == 1)
	prWifiVar->fgEnOnlyScan6g = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnableOnlyScan6g", FEATURE_DISABLED);
#endif /* CFG_SUPPORT_LIMITED_PKT_PID */

#if (CFG_SUPPORT_DYNAMIC_EDCCA == 1)
	prWifiVar->i4Ed2GNonEU = (int32_t) wlanCfgGetInt32(prAdapter,
		"Ed2GNonEU", ED_CCA_BW20_2G_DEFAULT);
	prWifiVar->i4Ed5GNonEU = (int32_t) wlanCfgGetInt32(prAdapter,
		"Ed5GNonEU", ED_CCA_BW20_5G_DEFAULT);
	prWifiVar->i4Ed2GEU = (int32_t) wlanCfgGetInt32(prAdapter,
		"Ed2GEU", ED_CCA_BW20_2G_DEFAULT);
	prWifiVar->i4Ed5GEU = (int32_t) wlanCfgGetInt32(prAdapter,
		"Ed5GEU", ED_CCA_BW20_5G_DEFAULT);
#endif

#if CFG_MTK_FPGA_PLATFORM
	prWifiVar->u4FpgaSpeedFactor = wlanCfgGetUint32(prAdapter,
			"FpgaSpeedFactor", 0);
#endif
#if (CFG_SUPPORT_HOST_OFFLOAD == 1)
	prWifiVar->fgEnableMawd = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnableMawd", FEATURE_ENABLED);
	prWifiVar->fgEnableMawdTx = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnableMawdTx", FEATURE_DISABLED);
	prWifiVar->fgEnableSdo = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnableSdo", FEATURE_ENABLED);
	prWifiVar->fgEnableRro = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnableRro", FEATURE_ENABLED);
	prWifiVar->fgEnableRro2Md = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnableRro2Md", FEATURE_DISABLED);
	prWifiVar->fgEnableRroPreFillRxRing = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnableRroPreFillRxRing", FEATURE_ENABLED);
	prWifiVar->fgEnableRroDbg = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnableRroDbg", FEATURE_DISABLED);
	prWifiVar->fgEnableRroAdvDump = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnableRroAdvDump", FEATURE_DISABLED);

	if (IS_FEATURE_FORCE_ENABLED(prWifiVar->fgEnableMawd))
		prWifiVar->fgEnableMawd = FEATURE_ENABLED;
	else if (!prChipInfo->is_support_mawd || !kalIsSupportMawd())
		prWifiVar->fgEnableMawd = FEATURE_DISABLED;

	if (IS_FEATURE_FORCE_ENABLED(prWifiVar->fgEnableSdo))
		prWifiVar->fgEnableSdo = FEATURE_ENABLED;
	else if (!prChipInfo->is_support_sdo || !kalIsSupportSdo())
		prWifiVar->fgEnableSdo = FEATURE_DISABLED;

	if (IS_FEATURE_FORCE_ENABLED(prWifiVar->fgEnableRro))
		prWifiVar->fgEnableRro = FEATURE_ENABLED;
	else if (!prChipInfo->is_support_rro || !kalIsSupportRro())
		prWifiVar->fgEnableRro = FEATURE_DISABLED;
#endif /* CFG_SUPPORT_HOST_OFFLOAD == 1 */
#if (CFG_WFD_SCC_BALANCE_SUPPORT == 1)
	for (u4Idx = 0; u4Idx < MAX_BSSID_NUM; u4Idx++) {
		prWifiVar->i4BssCount[u4Idx] = wlanCfgGetInt32(
			prAdapter, "wfdSccBalanceBssCount", 0);
	}
	prWifiVar->u4WfdSccBalanceMode = wlanCfgGetUint32(
		prAdapter, "wfdSccBalanceMode", 0);
#if (CFG_WFD_SCC_BALANCE_DEF_ENABLE == 1)
	prWifiVar->u4WfdSccBalanceEnable = wlanCfgGetUint32(
		prAdapter, "wfdSccBalanceEnable", FEATURE_ENABLED);
#else
	prWifiVar->u4WfdSccBalanceEnable = wlanCfgGetUint32(
		prAdapter, "wfdSccBalanceEnable", FEATURE_DISABLED);
#endif
#endif
	prWifiVar->fgIcmpTxs = wlanCfgGetInt32(prAdapter, "IcmpTxs",
			FEATURE_ENABLED);
	prWifiVar->u4DrvOwnMode = wlanCfgGetUint32(prAdapter,
			"drvOwnMode", 0);

	/* Fast Path Config */
	prWifiVar->ucUdpTspecUp = (uint8_t) wlanCfgGetUint32(
				prAdapter, "UdpTspecUp", 7);
	prWifiVar->ucTcpTspecUp = (uint8_t) wlanCfgGetUint32(
				prAdapter, "TcpTspecUp", 5);
	prWifiVar->u4UdpDelayBound = wlanCfgGetUint32(
			prAdapter, "UdpDelayBound", 7000);
	prWifiVar->u4TcpDelayBound = wlanCfgGetUint32(
			prAdapter, "TcpDelayBound", 10000);
	prWifiVar->ucDataRate = (uint8_t) wlanCfgGetUint32(
			prAdapter, "TspecDataRate", 0);
	prWifiVar->ucSupportProtocol = (uint8_t) wlanCfgGetUint32(
			prAdapter, "SupportProtocol", 0);
	prWifiVar->ucCheckBeacon = (uint8_t) wlanCfgGetUint32(
			prAdapter, "MscsCheckBeacon", FEATURE_ENABLED);
	prWifiVar->ucEnableFastPath = (uint8_t) wlanCfgGetUint32(
			prAdapter, "EnableFastPath", FEATURE_ENABLED);
	prWifiVar->ucFastPathAllPacket = (uint8_t) wlanCfgGetUint32(
			prAdapter, "FastPathAllPacket", FEATURE_DISABLED);
#if (CFG_TX_HIF_PORT_QUEUE == 1)
	prWifiVar->ucEnableTxHifPortQ = (uint8_t) wlanCfgGetUint32(
			prAdapter, "EnableTxHifPortQ", FEATURE_ENABLED);
#endif
#if (CFG_VOLT_INFO == 1)
	prWifiVar->fgVnfEn = (bool) wlanCfgGetUint32(
		prAdapter, "VoltInfoEnable", FEATURE_ENABLED);
	prWifiVar->u4VnfDebTimes = (uint32_t) wlanCfgGetUint32(
		prAdapter, "VoltInfoDebTimes",
		VOLT_INFO_DEBOUNCE_TIMES);
	prWifiVar->u4VnfDebInterval = (uint32_t) wlanCfgGetUint32(
		prAdapter, "VoltInfoDebInterval",
		VOLT_INFO_DEBOUNCE_INTERVAL);
	prWifiVar->u4VnfDelta = (uint32_t) wlanCfgGetUint32(
		prAdapter, "VoltInfoDelta",
		VOLT_INFO_DELTA);
#endif /* CFG_VOLT_INFO  */

#if CFG_SUPPORT_MLR
	prWifiVar->fgEnForceTxFrag = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnForceTxFrag", FEATURE_DISABLED);
	prWifiVar->u2TxFragThr = (uint16_t) wlanCfgGetUint32(
		prAdapter, "TxFragSplitThr", 1000);
	prWifiVar->u2TxFragSplitSize = (uint16_t) wlanCfgGetUint32(
		prAdapter, "TxFragSplitSize", 0);
	prWifiVar->ucTxMlrRateRcpiThr = (uint8_t) wlanCfgGetUint32(
		prAdapter, "TxMlrRateRcpiThr", 40);
	prWifiVar->fgEnTxFragDebug = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnTxFragDebug", FEATURE_DISABLED);
	prWifiVar->fgEnTxFragTxDone = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnTxFragTxDone", FEATURE_DISABLED);
	prWifiVar->ucErrPos = (uint8_t) wlanCfgGetUint32(
		prAdapter, "ErrPos", 0);
#endif

#if (CFG_SUPPORT_TX_DATA_DELAY == 1)
	prWifiVar->u4TxDataDelayTimeout = wlanCfgGetUint32(prAdapter,
			"TxDataDelayTimeout", 2);
	prWifiVar->u4TxDataDelayCnt = wlanCfgGetUint32(prAdapter,
			"TxDataDelayCnt", 10);
#endif /* CFG_SUPPORT_TX_DATA_DELAY == 1 */

#if (CFG_SUPPORT_POWER_THROTTLING == 1)
	prWifiVar->i4ThrmCtrlTemp = wlanCfgGetInt32(
		prAdapter, "ThrmCtrlTemp", THRM_PROT_DUTY_CTRL_TEMP);
	prWifiVar->i4ThrmRadioOffTemp = wlanCfgGetInt32(
		prAdapter, "ThrmRadioOffTemp", THRM_PROT_RADIO_OFF_TEMP);
	prWifiVar->aucThrmLvTxDuty[0] = (uint8_t) wlanCfgGetInt32(
		prAdapter, "ThrmLv0TxDuty", THRM_PROT_DEFAULT_LV0_DUTY);
	prWifiVar->aucThrmLvTxDuty[1] = (uint8_t) wlanCfgGetInt32(
		prAdapter, "ThrmLv1TxDuty", THRM_PROT_DEFAULT_LV1_DUTY);
	prWifiVar->aucThrmLvTxDuty[2] = (uint8_t) wlanCfgGetInt32(
		prAdapter, "ThrmLv2TxDuty", THRM_PROT_DEFAULT_LV2_DUTY);
	prWifiVar->aucThrmLvTxDuty[3] = (uint8_t) wlanCfgGetInt32(
		prAdapter, "ThrmLv3TxDuty", THRM_PROT_DEFAULT_LV3_DUTY);
	prWifiVar->aucThrmLvTxDuty[4] = (uint8_t) wlanCfgGetInt32(
		prAdapter, "ThrmLv4TxDuty", THRM_PROT_DEFAULT_LV4_DUTY);
	prWifiVar->aucThrmLvTxDuty[5] = (uint8_t) wlanCfgGetInt32(
		prAdapter, "ThrmLv5TxDuty", THRM_PROT_DEFAULT_LV5_DUTY);
#endif

#if CFG_SUPPORT_PCIE_ASPM
	prWifiVar->fgPcieEnableL1ss = (uint8_t) wlanCfgGetUint32(
		prAdapter, "PcieEnableL1ss", 1);
#endif

#if CFG_SUPPORT_PCIE_GEN_SWITCH
	prWifiVar->u4PcieGenSwitchTputThr = (uint8_t) wlanCfgGetUint32(
		prAdapter, "PcieGenSwitchTputThr", 100);
	prWifiVar->u4PcieGenSwitchJudgeTime = (uint8_t) wlanCfgGetUint32(
		prAdapter, "PcieGenSwitchJudgeTime", 10);
#endif

	prWifiVar->fgEnWfdmaNoMmioRead = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnWfdmaNoMmioRead", FEATURE_ENABLED);
	if (IS_FEATURE_FORCE_ENABLED(prWifiVar->fgEnWfdmaNoMmioRead))
		prWifiVar->fgEnWfdmaNoMmioRead = FEATURE_ENABLED;
	else if (!prChipInfo->is_en_wfdma_no_mmio_read)
		prWifiVar->fgEnWfdmaNoMmioRead = FEATURE_DISABLED;

#if CFG_MTK_WIFI_EN_SW_EMI_READ
	prWifiVar->fgEnSwEmiRead = (uint8_t) wlanCfgGetUint32(
		prAdapter, "EnSwEmiRead", FEATURE_ENABLED);
	if (IS_FEATURE_FORCE_ENABLED(prWifiVar->fgEnSwEmiRead))
		prWifiVar->fgEnSwEmiRead = FEATURE_ENABLED;
	else if (!prChipInfo->is_en_sw_emi_read)
		prWifiVar->fgEnSwEmiRead = FEATURE_DISABLED;
#endif

	prWifiVar->u4PrdcIntTime = (uint32_t)wlanCfgGetUint32(
		prAdapter, "PrdcIntTime", 5); /* unit: 20us */
	prWifiVar->fgEnDlyInt = (uint32_t)wlanCfgGetUint32(
		prAdapter, "EnDlyInt", 1);
	prWifiVar->u4DlyIntTime = (uint32_t)wlanCfgGetUint32(
		prAdapter, "DlyIntTime", 2); /* unit: 20us */
	prWifiVar->u4DlyIntCnt = (uint32_t)wlanCfgGetUint32(
		prAdapter, "DlyIntCnt", 0); /* 0: No check by count */

#if CFG_SUPPORT_DYNAMIC_PAGE_POOL
#if CFG_DYNAMIC_RFB_ADJUSTMENT
	prWifiVar->u4PagePoolMinCnt = (uint32_t)wlanCfgGetUint32(
		prAdapter, "PagePoolMinCnt",
		CFG_RX_MAX_PKT_NUM - nicRxGetInUseCnt(prAdapter));
#else
	prWifiVar->u4PagePoolMinCnt = (uint32_t)wlanCfgGetUint32(
		prAdapter, "PagePoolMinCnt", CFG_RX_MAX_PKT_NUM);
#endif /* CFG_DYNAMIC_RFB_ADJUSTMENT */
	prWifiVar->u4PagePoolMaxCnt = (uint32_t)wlanCfgGetUint32(
		prAdapter, "PagePoolMaxCnt", CFG_RX_MAX_PKT_NUM * 3);
	kalSetupPagePoolPageMaxMinNum(prWifiVar->u4PagePoolMinCnt,
				      prWifiVar->u4PagePoolMaxCnt);
#endif /* CFG_SUPPORT_DYNAMIC_PAGE_POOL */

	prWifiVar->fgEnSwAmsduSorting = (uint8_t)wlanCfgGetUint32(
		prAdapter, "EnSwAmsduSorting", FEATURE_DISABLED);

	prWifiVar->u4PmkRefreshThreshold = (uint32_t)wlanCfgGetUint32(
		prAdapter, "PmkRefreshThresholdSec",
		PMK_REFRESH_THRESHOLD_SEC);
}

void wlanCfgSetSwCtrl(struct ADAPTER *prAdapter)
{
	uint32_t i = 0;
	int8_t aucKey[WLAN_CFG_VALUE_LEN_MAX];
	int8_t aucValue[WLAN_CFG_VALUE_LEN_MAX];

	const int8_t acDelim[] = " ";
	int8_t *pcPtr = NULL;
	int8_t *pcDupValue = NULL;
	uint32_t au4Values[2] = {0};
	uint32_t u4TokenCount = 0;
	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;
	int32_t u4Ret = 0;

	for (i = 0; i < WLAN_CFG_SET_SW_CTRL_LEN_MAX; i++) {
		kalMemZero(aucValue, WLAN_CFG_VALUE_LEN_MAX);
		kalMemZero(aucKey, WLAN_CFG_VALUE_LEN_MAX);
		kalSprintf(aucKey, "SwCtrl%d", i);

		/* get nothing */
		if (wlanCfgGet(prAdapter, aucKey, aucValue, "",
			       0) != WLAN_STATUS_SUCCESS)
			continue;
		if (!kalStrCmp(aucValue, ""))
			continue;

		pcDupValue = aucValue;
		u4TokenCount = 0;

		while ((pcPtr = kalStrSep((char **)(&pcDupValue), acDelim))
		       != NULL) {

			if (!kalStrCmp(pcPtr, ""))
				continue;

			/* au4Values[u4TokenCount] = kalStrtoul(pcPtr, NULL, 0);
			 */
			u4Ret = kalkStrtou32(pcPtr, 0,
					     &(au4Values[u4TokenCount]));
			if (u4Ret)
				DBGLOG(INIT, LOUD,
				       "parse au4Values error u4Ret=%d\n",
				       u4Ret);
			u4TokenCount++;

			/* Only need 2 tokens */
			if (u4TokenCount >= 2)
				break;
		}

		if (u4TokenCount != 2)
			continue;

		rSwCtrlInfo.u4Id = au4Values[0];
		rSwCtrlInfo.u4Data = au4Values[1];

		rStatus = kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite,
				   &rSwCtrlInfo, sizeof(rSwCtrlInfo),
				   &u4BufLen);

	}
}

void wlanCfgSetChip(struct ADAPTER *prAdapter)
{
	uint32_t i = 0;
	int8_t aucKey[WLAN_CFG_VALUE_LEN_MAX];
	int8_t aucValue[WLAN_CFG_VALUE_LEN_MAX];

	uint32_t u4BufLen = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rChipConfigInfo;

	for (i = 0; i < WLAN_CFG_SET_CHIP_LEN_MAX; i++) {
		kalMemZero(aucValue, WLAN_CFG_VALUE_LEN_MAX);
		kalMemZero(aucKey, WLAN_CFG_VALUE_LEN_MAX);
		kalSnprintf(aucKey, sizeof(aucKey), "SetChip%d", i);

		/* get nothing */
		if (wlanCfgGet(prAdapter, aucKey, aucValue, "",
			       0) != WLAN_STATUS_SUCCESS)
			continue;
		if (!kalStrCmp(aucValue, ""))
			continue;

		kalMemZero(&rChipConfigInfo, sizeof(rChipConfigInfo));

		rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_WO_RESPONSE;
		rChipConfigInfo.u2MsgSize = kalStrnLen(aucValue,
						       WLAN_CFG_VALUE_LEN_MAX);
		kalStrnCpy(rChipConfigInfo.aucCmd, aucValue,
			   CHIP_CONFIG_RESP_SIZE);

		rStatus = kalIoctl(prGlueInfo, wlanoidSetChipConfig,
				   &rChipConfigInfo, sizeof(rChipConfigInfo),
				   &u4BufLen);
	}
}

void wlanCfgSetDebugLevel(struct ADAPTER *prAdapter)
{
	uint32_t i = 0;
	int8_t aucKey[WLAN_CFG_VALUE_LEN_MAX];
	int8_t aucValue[WLAN_CFG_VALUE_LEN_MAX];
	const int8_t acDelim[] = " ";
	int8_t *pcDupValue;
	int8_t *pcPtr = NULL;

	uint32_t au4Values[2] = {0};
	uint32_t u4TokenCount = 0;
	uint32_t u4DbgIdx = 0;
	uint32_t u4DbgMask = 0;
	int32_t u4Ret = 0;

	for (i = 0; i < WLAN_CFG_SET_DEBUG_LEVEL_LEN_MAX; i++) {
		kalMemZero(aucValue, WLAN_CFG_VALUE_LEN_MAX);
		kalMemZero(aucKey, WLAN_CFG_VALUE_LEN_MAX);
		kalSprintf(aucKey, "DbgLevel%d", i);

		/* get nothing */
		if (wlanCfgGet(prAdapter, aucKey, aucValue, "",
			       0) != WLAN_STATUS_SUCCESS)
			continue;
		if (!kalStrCmp(aucValue, ""))
			continue;

		pcDupValue = aucValue;
		u4TokenCount = 0;

		while ((pcPtr = kalStrSep((char **)(&pcDupValue),
					  acDelim)) != NULL) {

			if (!kalStrCmp(pcPtr, ""))
				continue;

			/* au4Values[u4TokenCount] =
			 *			kalStrtoul(pcPtr, NULL, 0);
			 */
			u4Ret = kalkStrtou32(pcPtr, 0,
					     &(au4Values[u4TokenCount]));
			if (u4Ret)
				DBGLOG(INIT, LOUD,
				       "parse au4Values error u4Ret=%d\n",
				       u4Ret);
			u4TokenCount++;

			/* Only need 2 tokens */
			if (u4TokenCount >= 2)
				break;
		}

		if (u4TokenCount != 2)
			continue;

		u4DbgIdx = au4Values[0];
		u4DbgMask = au4Values[1];

		/* DBG level special control */
		if (u4DbgIdx == 0xFFFFFFFF) {
			wlanSetDriverDbgLevel(DBG_ALL_MODULE_IDX, u4DbgMask);
			DBGLOG(INIT, INFO,
			       "Set ALL DBG module log level to [0x%02x]!",
			       (uint8_t) u4DbgMask);
		} else if (u4DbgIdx == 0xFFFFFFFE) {
			wlanDebugInit();
			DBGLOG(INIT, INFO,
			       "Reset ALL DBG module log level to DEFAULT!");
		} else if (u4DbgIdx < DBG_MODULE_NUM) {
			wlanSetDriverDbgLevel(u4DbgIdx, u4DbgMask);
			DBGLOG(INIT, INFO,
			       "Set DBG module[%u] log level to [0x%02x]!",
			       u4DbgIdx, (uint8_t) u4DbgMask);
		}
	}
}

void wlanCfgSetCountryCode(struct ADAPTER *prAdapter)
{
	int8_t aucValue[WLAN_CFG_VALUE_LEN_MAX];

	/* Apply COUNTRY Config */
	if (wlanCfgGet(prAdapter, "Country", aucValue, "",
		       0) == WLAN_STATUS_SUCCESS) {
		prAdapter->rWifiVar.u2CountryCode =
			(((uint16_t) aucValue[0]) << 8) |
			((uint16_t) aucValue[1]);

		DBGLOG(INIT, TRACE, "u2CountryCode=0x%04x\n",
		       prAdapter->rWifiVar.u2CountryCode);

		if (regd_is_single_sku_en()) {
			rlmDomainOidSetCountry(prAdapter, aucValue, 2);
			return;
		}

		/* Force to re-search country code in regulatory domains */
		prAdapter->prDomainInfo = NULL;
		rlmDomainSendCmd(prAdapter, TRUE);

		/* Update supported channel list in channel table based on
		 * current country domain
		 */
		wlanUpdateChannelTable(prAdapter->prGlueInfo);
	}
}

#if CFG_SUPPORT_CFG_FILE

struct WLAN_CFG_ENTRY *wlanCfgGetEntry(struct ADAPTER *prAdapter,
				       const int8_t *pucKey,
				       uint32_t u4Flags)
{

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg = NULL;
	struct WLAN_CFG_REC *prWlanCfgRec = NULL;
	struct WLAN_CFG *prWlanCfgEm = NULL;
	uint32_t i, u32MaxNum;

	if (u4Flags == WLAN_CFG_REC) {
		prWlanCfgRec = prAdapter->prWlanCfgRec;
		u32MaxNum = WLAN_CFG_REC_ENTRY_NUM_MAX;
		ASSERT(prWlanCfgRec);
	} else if (u4Flags == WLAN_CFG_EM) {
		prWlanCfgEm = prAdapter->prWlanCfgEm;
		u32MaxNum = WLAN_CFG_REC_ENTRY_NUM_MAX;
		ASSERT(prWlanCfgEm);
	} else {
		prWlanCfg = prAdapter->prWlanCfg;
		u32MaxNum = WLAN_CFG_ENTRY_NUM_MAX;
		ASSERT(prWlanCfg);
	}


	ASSERT(pucKey);

	prWlanCfgEntry = NULL;

	for (i = 0; i < u32MaxNum; i++) {
		if (u4Flags == WLAN_CFG_REC)
			prWlanCfgEntry = &prWlanCfgRec->arWlanCfgBuf[i];
		else if (u4Flags == WLAN_CFG_EM)
			prWlanCfgEntry = &prWlanCfgEm->arWlanCfgBuf[i];
		else
			prWlanCfgEntry = &prWlanCfg->arWlanCfgBuf[i];

		if (prWlanCfgEntry->aucKey[0] != '\0') {
			if (kalStrnCmp(pucKey, prWlanCfgEntry->aucKey,
				       WLAN_CFG_KEY_LEN_MAX - 1) == 0)
				return prWlanCfgEntry;
		}
	}

	return NULL;

}

uint32_t wlanCfgGetTotalCfgNum(
	struct ADAPTER *prAdapter, uint32_t flag)
{
	uint32_t i = 0;
	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	uint32_t count = 0;

	for (i = 0; i < WLAN_CFG_REC_ENTRY_NUM_MAX; i++) {
		prWlanCfgEntry = wlanCfgGetEntryByIndex(prAdapter, i, flag);

		if ((!prWlanCfgEntry) || (prWlanCfgEntry->aucKey[0] == '\0'))
			break;

		count++;
	}
	return count;
}


struct WLAN_CFG_ENTRY *wlanCfgGetEntryByIndex(
	struct ADAPTER *prAdapter, const uint8_t ucIdx,
	uint32_t flag)
{

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg;
	struct WLAN_CFG_REC *prWlanCfgRec;
	struct WLAN_CFG *prWlanCfgEm;

	if (!prAdapter) {
		DBGLOG(INIT, WARN, "prAdapter is NULL\n");
		return NULL;
	}

	prWlanCfg = prAdapter->prWlanCfg;
	prWlanCfgRec = prAdapter->prWlanCfgRec;
	prWlanCfgEm = prAdapter->prWlanCfgEm;

	ASSERT(prWlanCfg);
	ASSERT(prWlanCfgRec);
	ASSERT(prWlanCfgEm);


	prWlanCfgEntry = NULL;

	if (flag == WLAN_CFG_REC)
		prWlanCfgEntry = &prWlanCfgRec->arWlanCfgBuf[ucIdx];
	else if (flag == WLAN_CFG_EM)
		prWlanCfgEntry = &prWlanCfgEm->arWlanCfgBuf[ucIdx];
	else
		prWlanCfgEntry = &prWlanCfg->arWlanCfgBuf[ucIdx];

	if (prWlanCfgEntry->aucKey[0] != '\0') {
		DBGLOG(INIT, LOUD, "get Index(%d) saved key %s\n", ucIdx,
		       prWlanCfgEntry->aucKey);
		return prWlanCfgEntry;
	}

	DBGLOG(INIT, TRACE,
	       "wifi config there is no entry at index(%d)\n", ucIdx);
	return NULL;

}



uint32_t wlanCfgGet(struct ADAPTER *prAdapter,
		    const int8_t *pucKey, int8_t *pucValue, int8_t *pucValueDef,
		    uint32_t u4Flags)
{

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;

	ASSERT(pucValue);

	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, u4Flags);

	if (prWlanCfgEntry) {
		kalStrnCpy(pucValue, prWlanCfgEntry->aucValue,
			   WLAN_CFG_VALUE_LEN_MAX);
		return WLAN_STATUS_SUCCESS;
	}
	if (pucValueDef)
		kalStrnCpy(pucValue, pucValueDef,
			   WLAN_CFG_VALUE_LEN_MAX);
	return WLAN_STATUS_FAILURE;


}

void wlanCfgRecordValue(struct ADAPTER *prAdapter,
			const int8_t *pucKey, uint32_t u4Value)
{
	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	uint8_t aucBuf[WLAN_CFG_VALUE_LEN_MAX];

	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, WLAN_CFG_REC);

	kalMemZero(aucBuf, sizeof(aucBuf));

	kalSnprintf(aucBuf, WLAN_CFG_VALUE_LEN_MAX, "0x%x",
		    (unsigned int)u4Value);

	wlanCfgSet(prAdapter, pucKey, aucBuf, WLAN_CFG_REC);
}



uint32_t wlanCfgGetUint32(struct ADAPTER *prAdapter,
			  const int8_t *pucKey, uint32_t u4ValueDef)
{
	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg;
	uint32_t u4Value;
	int32_t u4Ret;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	u4Value = u4ValueDef;
	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, WLAN_CFG_DEFAULT);

	if (prWlanCfgEntry) {
		/* u4Ret = kalStrtoul(prWlanCfgEntry->aucValue, NULL, 0); */
		u4Ret = kalkStrtou32(prWlanCfgEntry->aucValue, 0, &u4Value);
		if (u4Ret)
			DBGLOG(INIT, LOUD, "parse aucValue error u4Ret=%d\n",
			       u4Ret);
	}

	wlanCfgRecordValue(prAdapter, pucKey, u4Value);

	return u4Value;
}

int32_t wlanCfgGetInt32(struct ADAPTER *prAdapter,
			const int8_t *pucKey, int32_t i4ValueDef)
{
	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg;
	int32_t i4Value = 0;
	int32_t i4Ret = 0;

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	i4Value = i4ValueDef;
	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, WLAN_CFG_DEFAULT);

	if (prWlanCfgEntry) {
		/* i4Ret = kalStrtol(prWlanCfgEntry->aucValue, NULL, 0); */
		i4Ret = kalkStrtos32(prWlanCfgEntry->aucValue, 0, &i4Value);
		if (i4Ret)
			DBGLOG(INIT, LOUD, "parse aucValue error i4Ret=%d\n",
			       i4Ret);
	}

	wlanCfgRecordValue(prAdapter, pucKey, (uint32_t)i4Value);

	return i4Value;
}

uint32_t wlanCfgSet(struct ADAPTER *prAdapter,
		    const int8_t *pucKey, int8_t *pucValue, uint32_t u4Flags)
{

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg = NULL;
	struct WLAN_CFG *prWlanCfgEm = NULL;
	struct WLAN_CFG_REC *prWlanCfgRec = NULL;
	uint32_t u4EntryIndex;
	uint32_t i;
	uint8_t ucExist;


	ASSERT(pucKey);

	DBGLOG(INIT, LOUD, "[%s]:[%s] OP:%d\n", pucKey, pucValue, u4Flags);

	/* Find the exist */
	ucExist = 0;
	if (u4Flags == WLAN_CFG_REC) {
		prWlanCfgEntry =
			wlanCfgGetEntry(prAdapter, pucKey, WLAN_CFG_REC);
		prWlanCfgRec = prAdapter->prWlanCfgRec;
		ASSERT(prWlanCfgRec);
	} else if (u4Flags == WLAN_CFG_EM) {
		prWlanCfgEntry =
			wlanCfgGetEntry(prAdapter, pucKey, WLAN_CFG_EM);
		prWlanCfgEm = prAdapter->prWlanCfgEm;
		ASSERT(prWlanCfgEm);
	} else {
		prWlanCfgEntry =
			wlanCfgGetEntry(prAdapter, pucKey, WLAN_CFG_DEFAULT);
		prWlanCfg = prAdapter->prWlanCfg;
		ASSERT(prWlanCfg);
	}

	if (!prWlanCfgEntry) {
		/* Find the empty */
		for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {
			if (u4Flags == WLAN_CFG_REC)
				prWlanCfgEntry = &prWlanCfgRec->arWlanCfgBuf[i];
			else if (u4Flags == WLAN_CFG_EM)
				prWlanCfgEntry = &prWlanCfgEm->arWlanCfgBuf[i];
			else
				prWlanCfgEntry = &prWlanCfg->arWlanCfgBuf[i];

			if (prWlanCfgEntry->aucKey[0] == '\0')
				break;
		}

		u4EntryIndex = i;
		if (u4EntryIndex < WLAN_CFG_ENTRY_NUM_MAX) {
			if (u4Flags == WLAN_CFG_REC)
				prWlanCfgEntry =
				    &prWlanCfgRec->arWlanCfgBuf[u4EntryIndex];
			else if (u4Flags == WLAN_CFG_EM)
				prWlanCfgEntry =
				    &prWlanCfgEm->arWlanCfgBuf[u4EntryIndex];
			else
				prWlanCfgEntry =
				    &prWlanCfg->arWlanCfgBuf[u4EntryIndex];
			kalMemZero(prWlanCfgEntry,
				   sizeof(struct WLAN_CFG_ENTRY));
		} else {
			prWlanCfgEntry = NULL;
			DBGLOG(INIT, ERROR,
			       "wifi config there is no empty entry\n");
		}
	} /* !prWlanCfgEntry */
	else
		ucExist = 1;

	if (prWlanCfgEntry) {
		if (ucExist == 0) {
			kalStrnCpy(prWlanCfgEntry->aucKey, pucKey,
				   WLAN_CFG_KEY_LEN_MAX - 1);
			prWlanCfgEntry->aucKey[WLAN_CFG_KEY_LEN_MAX - 1] = '\0';
		}
		if (pucValue && pucValue[0] != '\0') {
			kalStrnCpy(prWlanCfgEntry->aucValue, pucValue,
				   WLAN_CFG_VALUE_LEN_MAX - 1);
			prWlanCfgEntry->aucValue[WLAN_CFG_VALUE_LEN_MAX - 1] =
									'\0';

			if (ucExist) {
				if (prWlanCfgEntry->pfSetCb)
					prWlanCfgEntry->pfSetCb(prAdapter,
						prWlanCfgEntry->aucKey,
						prWlanCfgEntry->aucValue,
						prWlanCfgEntry->pPrivate, 0);
			}
		} else {
			/* Call the pfSetCb if value is empty ? */
			/* remove the entry if value is empty */
			kalMemZero(prWlanCfgEntry,
				   sizeof(struct WLAN_CFG_ENTRY));
		}

	}
	/* prWlanCfgEntry */
	if (prWlanCfgEntry) {
		return WLAN_STATUS_SUCCESS;
	}
	if (pucKey)
		DBGLOG(INIT, ERROR, "Set wifi config error key \'%s\'\n",
		       pucKey);

	if (pucValue)
		DBGLOG(INIT, ERROR, "Set wifi config error value \'%s\'\n",
		       pucValue);

	return WLAN_STATUS_FAILURE;


}

uint32_t
wlanCfgSetCb(struct ADAPTER *prAdapter,
	     const int8_t *pucKey, WLAN_CFG_SET_CB pfSetCb,
	     void *pPrivate, uint32_t u4Flags)
{

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct WLAN_CFG *prWlanCfg;

	prWlanCfg = prAdapter->prWlanCfg;
	ASSERT(prWlanCfg);

	/* Find the exist */
	prWlanCfgEntry = wlanCfgGetEntry(prAdapter, pucKey, FALSE);

	if (prWlanCfgEntry) {
		prWlanCfgEntry->pfSetCb = pfSetCb;
		prWlanCfgEntry->pPrivate = pPrivate;
	}

	if (prWlanCfgEntry)
		return WLAN_STATUS_SUCCESS;
	else
		return WLAN_STATUS_FAILURE;

}

uint32_t wlanCfgSetUint32(struct ADAPTER *prAdapter,
			  const int8_t *pucKey, uint32_t u4Value)
{

	struct WLAN_CFG *prWlanCfg;
	uint8_t aucBuf[WLAN_CFG_VALUE_LEN_MAX];

	prWlanCfg = prAdapter->prWlanCfg;

	ASSERT(prWlanCfg);

	kalMemZero(aucBuf, sizeof(aucBuf));

	kalSnprintf(aucBuf, WLAN_CFG_VALUE_LEN_MAX, "0x%x",
		    (unsigned int)u4Value);

	return wlanCfgSet(prAdapter, pucKey, aucBuf, WLAN_CFG_DEFAULT);
}

enum {
	STATE_EOF = 0,
	STATE_TEXT = 1,
	STATE_NEWLINE = 2
};

struct WLAN_CFG_PARSE_STATE_S {
	int8_t *ptr;
	int8_t *text;
#if CFG_SUPPORT_EASY_DEBUG
	uint32_t textsize;
#endif
	int32_t nexttoken;
	uint32_t maxSize;
};

int32_t wlanCfgFindNextToken(struct WLAN_CFG_PARSE_STATE_S
			     *state)
{
	int8_t *x = state->ptr;
	int8_t *s;

	if (state->nexttoken) {
		int32_t t = state->nexttoken;

		state->nexttoken = 0;
		return t;
	}

	for (;;) {
		switch (*x) {
		case 0:
			state->ptr = x;
			return STATE_EOF;
		case '\n':
			x++;
			state->ptr = x;
			return STATE_NEWLINE;
		case ' ':
		case ',':
		/*case ':':  should not including : , mac addr would be fail*/
		case '\t':
		case '\r':
			x++;
			continue;
		case '#':
			while (*x && (*x != '\n'))
				x++;
			if (*x == '\n') {
				state->ptr = x + 1;
				return STATE_NEWLINE;
			}
			state->ptr = x;
			return STATE_EOF;

		default:
			goto text;
		}
	}

textdone:
	state->ptr = x;
	*s = 0;
	return STATE_TEXT;
text:
	state->text = s = x;
textresume:
	for (;;) {
		switch (*x) {
		case 0:
			goto textdone;
		case ' ':
		case ',':
		/* case ':': */
		case '\t':
		case '\r':
			x++;
			goto textdone;
		case '\n':
			state->nexttoken = STATE_NEWLINE;
			x++;
			goto textdone;
		case '"':
			x++;
			for (;;) {
				switch (*x) {
				case 0:
					/* unterminated quoted thing */
					state->ptr = x;
					return STATE_EOF;
				case '"':
					x++;
					goto textresume;
				default:
					*s++ = *x++;
				}
			}
			break;
		case '\\':
			x++;
			switch (*x) {
			case 0:
				goto textdone;
			case 'n':
				*s++ = '\n';
				break;
			case 'r':
				*s++ = '\r';
				break;
			case 't':
				*s++ = '\t';
				break;
			case '\\':
				*s++ = '\\';
				break;
			case '\r':
				/* \ <cr> <lf> -> line continuation */
				if (x[1] != '\n') {
					x++;
					continue;
				}
				/* fallthrough */
			case '\n':
				/* \ <lf> -> line continuation */
				x++;
				/* eat any extra whitespace */
				while ((*x == ' ') || (*x == '\t'))
					x++;
				continue;
			default:
				/* unknown escape -- just copy */
				*s++ = *x++;
			}
			continue;
		default:
			*s++ = *x++;
#if CFG_SUPPORT_EASY_DEBUG
			state->textsize++;
#endif
		}
	}
	return STATE_EOF;
}

uint32_t wlanCfgParseArgument(int8_t *cmdLine,
			      int32_t *argc, int8_t *argv[])
{
	struct WLAN_CFG_PARSE_STATE_S state;
	int8_t **args;
	int32_t nargs;

	if (cmdLine == NULL || argc == NULL || argv == NULL) {
		DBGLOG(INIT, ERROR, "parameter is NULL: %p, %p, %p\n",
		       cmdLine, argc, argv);
		return WLAN_STATUS_FAILURE;
	}
	args = argv;
	nargs = 0;
	state.ptr = cmdLine;
	state.nexttoken = 0;
	state.maxSize = 0;
#if CFG_SUPPORT_EASY_DEBUG
	state.textsize = 0;
#endif

	if (kalStrnLen(cmdLine, 512) >= 512) {
		DBGLOG(INIT, ERROR, "cmdLine >= 512\n");
		return WLAN_STATUS_FAILURE;
	}

	for (;;) {
		switch (wlanCfgFindNextToken(&state)) {
		case STATE_EOF:
			goto exit;
		case STATE_NEWLINE:
			goto exit;
		case STATE_TEXT:
			if (nargs < WLAN_CFG_ARGV_MAX)
				args[nargs++] = state.text;
			break;
		}
	}

exit:
	*argc = nargs;
	return WLAN_STATUS_SUCCESS;
}

#if CFG_WOW_SUPPORT
uint32_t wlanCfgParseArgumentLong(int8_t *cmdLine,
				  int32_t *argc, int8_t *argv[])
{
	struct WLAN_CFG_PARSE_STATE_S state;
	int8_t **args;
	int32_t nargs;

	if (cmdLine == NULL || argc == NULL || argv == NULL) {
		DBGLOG(INIT, ERROR, "parameter is NULL: %p, %p, %p\n",
		       cmdLine, argc, argv);
		return WLAN_STATUS_FAILURE;
	}
	args = argv;
	nargs = 0;
	state.ptr = cmdLine;
	state.nexttoken = 0;
	state.maxSize = 0;
#if CFG_SUPPORT_EASY_DEBUG
	state.textsize = 0;
#endif

	if (kalStrnLen(cmdLine, 512) >= 512) {
		DBGLOG(INIT, ERROR, "cmdLine >= 512\n");
		return WLAN_STATUS_FAILURE;
	}

	for (;;) {
		switch (wlanCfgFindNextToken(&state)) {
		case STATE_EOF:
			goto exit;
		case STATE_NEWLINE:
			goto exit;
		case STATE_TEXT:
			if (nargs < WLAN_CFG_ARGV_MAX_LONG)
				args[nargs++] = state.text;
			break;
		}
	}

exit:
	*argc = nargs;
	return WLAN_STATUS_SUCCESS;
}
#endif

uint32_t
wlanCfgParseAddEntry(struct ADAPTER *prAdapter,
		     uint8_t *pucKeyHead, uint8_t *pucKeyTail,
		     uint8_t *pucValueHead, uint8_t *pucValueTail)
{

	uint8_t aucKey[WLAN_CFG_KEY_LEN_MAX];
	uint8_t aucValue[WLAN_CFG_VALUE_LEN_MAX];
	uint32_t u4Len;

	kalMemZero(aucKey, sizeof(aucKey));
	kalMemZero(aucValue, sizeof(aucValue));

	if ((pucKeyHead == NULL)
	    || (pucValueHead == NULL)
	   )
		return WLAN_STATUS_FAILURE;

	if (pucKeyTail) {
		if (pucKeyHead > pucKeyTail)
			return WLAN_STATUS_FAILURE;
		u4Len = pucKeyTail - pucKeyHead + 1;
	} else
		u4Len = kalStrnLen(pucKeyHead, WLAN_CFG_KEY_LEN_MAX - 1);

	if (u4Len >= WLAN_CFG_KEY_LEN_MAX)
		u4Len = WLAN_CFG_KEY_LEN_MAX - 1;

	kalStrnCpy(aucKey, pucKeyHead, u4Len);

	if (pucValueTail) {
		if (pucValueHead > pucValueTail)
			return WLAN_STATUS_FAILURE;
		u4Len = pucValueTail - pucValueHead + 1;
	} else
		u4Len = kalStrnLen(pucValueHead,
				   WLAN_CFG_VALUE_LEN_MAX - 1);

	if (u4Len >= WLAN_CFG_VALUE_LEN_MAX)
		u4Len = WLAN_CFG_VALUE_LEN_MAX - 1;

	kalStrnCpy(aucValue, pucValueHead, u4Len);

	return wlanCfgSet(prAdapter, aucKey, aucValue, WLAN_CFG_DEFAULT);
}

enum {
	WAIT_KEY_HEAD = 0,
	WAIT_KEY_TAIL,
	WAIT_VALUE_HEAD,
	WAIT_VALUE_TAIL,
	WAIT_COMMENT_TAIL
};

#if CFG_SUPPORT_EASY_DEBUG

uint32_t wlanCfgParseToFW(int8_t **args, int8_t *args_size,
			  uint8_t nargs, int8_t *buffer, uint8_t times)
{
	uint8_t *data = NULL;
	char ch;
	int32_t i = 0, j = 0;
	uint32_t bufferindex = 0, base = 0;
	uint32_t sum = 0, startOffset = 0;
	struct CMD_FORMAT_V1 cmd_v1;

	memset(&cmd_v1, 0, sizeof(struct CMD_FORMAT_V1));

#if 0
	cmd_v1.itemType = kalAtoi(*args[ED_ITEMTYPE_SITE]);
#else
	cmd_v1.itemType = ITEM_TYPE_DEC;
#endif
	if (buffer == NULL ||
	    args_size[ED_STRING_SITE] == 0 ||
	    args_size[ED_VALUE_SITE] == 0 ||
	    (cmd_v1.itemType < ITEM_TYPE_DEC
	     || cmd_v1.itemType > ITEM_TYPE_STR)) {
		DBGLOG(INIT, ERROR, "cfg args wrong\n");
		return WLAN_STATUS_FAILURE;
	}

	cmd_v1.itemStringLength = args_size[ED_STRING_SITE];
	strncpy(cmd_v1.itemString, args[ED_STRING_SITE],
		cmd_v1.itemStringLength);
	DBGLOG(INIT, INFO, "itemString:");
	for (i = 0; i <  cmd_v1.itemStringLength; i++)
		DBGLOG(INIT, INFO, "%c", cmd_v1.itemString[i]);
	DBGLOG(INIT, INFO, "\n");

	DBGLOG(INIT, INFO, "cmd_v1.itemType = %d\n",
	       cmd_v1.itemType);
	if (cmd_v1.itemType == ITEM_TYPE_DEC
	    || cmd_v1.itemType == ITEM_TYPE_HEX) {
		data = args[ED_VALUE_SITE];

		switch (cmd_v1.itemType) {
		case ITEM_TYPE_DEC:
			base = 10;
			startOffset = 0;
			break;
		case ITEM_TYPE_HEX:
			ch = *data;
			if (args_size[ED_VALUE_SITE] < 3 || ch != '0') {
				DBGLOG(INIT, WARN,
				       "Hex args must have prefix '0x'\n");
				return WLAN_STATUS_FAILURE;
			}

			data++;
			ch = *data;
			if (ch != 'x' && ch != 'X') {
				DBGLOG(INIT, WARN,
				       "Hex args must have prefix '0x'\n");
				return WLAN_STATUS_FAILURE;
			}
			data++;
			base = 16;
			startOffset = 2;
			break;
		}

		for (j = args_size[ED_VALUE_SITE] - 1 - startOffset; j >= 0;
		     j--) {
			sum = sum * base + kalAtoi(*data);
			DBGLOG(INIT, WARN, "size:%d data[%d]=%u, sum=%u\n",
			       args_size[ED_VALUE_SITE], j,
				   kalAtoi(*data), sum);

			data++;
		}

		bufferindex = 0;
		do {
			cmd_v1.itemValue[bufferindex++] = sum & 0xFF;
			sum = sum >> 8;
		} while (sum > 0);
		cmd_v1.itemValueLength = bufferindex;
	} else if (cmd_v1.itemType == ITEM_TYPE_STR) {
		cmd_v1.itemValueLength = args_size[ED_VALUE_SITE];
		strncpy(cmd_v1.itemValue, args[ED_VALUE_SITE],
			cmd_v1.itemValueLength);
	}

	DBGLOG(INIT, INFO, "Length = %d itemValue:",
	       cmd_v1.itemValueLength);
	for (i = cmd_v1.itemValueLength - 1; i >= 0; i--)
		DBGLOG(INIT, ERROR, "%d,", cmd_v1.itemValue[i]);
	DBGLOG(INIT, INFO, "\n");
	memcpy(((struct CMD_FORMAT_V1 *)buffer) + times, &cmd_v1,
	       sizeof(struct CMD_FORMAT_V1));

	return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
 * @brief This function is to send WLAN feature options to firmware
 *
 * @param prAdapter  Pointer of ADAPTER_T
 *
 * @return none
 */
/*----------------------------------------------------------------------------*/
void wlanFeatureToFw(struct ADAPTER *prAdapter, uint32_t u4Flag)
{

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	uint32_t i;
	struct CMD_HEADER rCmdV1Header;
	uint32_t rStatus;
	struct CMD_FORMAT_V1 rCmd_v1;
	uint8_t  ucTimes = 0;



	rCmdV1Header.cmdType = CMD_TYPE_SET;
	rCmdV1Header.cmdVersion = CMD_VER_1;
	rCmdV1Header.cmdBufferLen = 0;
	rCmdV1Header.itemNum = 0;

	kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
	kalMemSet(&rCmd_v1, 0, sizeof(struct CMD_FORMAT_V1));


	prWlanCfgEntry = NULL;

	for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {

		prWlanCfgEntry = wlanCfgGetEntryByIndex(prAdapter, i, u4Flag);

		if (prWlanCfgEntry) {

			rCmd_v1.itemType = ITEM_TYPE_STR;


			/*send string format to firmware */
			rCmd_v1.itemStringLength = kalStrLen(
							prWlanCfgEntry->aucKey);
			if (rCmd_v1.itemStringLength > MAX_CMD_NAME_MAX_LENGTH)
				continue;
			kalMemZero(rCmd_v1.itemString, MAX_CMD_NAME_MAX_LENGTH);
			kalMemCopy(rCmd_v1.itemString, prWlanCfgEntry->aucKey,
				   rCmd_v1.itemStringLength);


			rCmd_v1.itemValueLength = kalStrLen(
						  prWlanCfgEntry->aucValue);
			if (rCmd_v1.itemValueLength > MAX_CMD_VALUE_MAX_LENGTH)
				continue;
			kalMemZero(rCmd_v1.itemValue, MAX_CMD_VALUE_MAX_LENGTH);
			kalMemCopy(rCmd_v1.itemValue, prWlanCfgEntry->aucValue,
				   rCmd_v1.itemValueLength);



			DBGLOG(INIT, TRACE,
			       "Send key word (%s) WITH (%s) to firmware\n",
			       rCmd_v1.itemString, rCmd_v1.itemValue);

			kalMemCopy(((struct CMD_FORMAT_V1 *)rCmdV1Header.buffer)
				   + ucTimes,
				   &rCmd_v1, sizeof(struct CMD_FORMAT_V1));


			ucTimes++;
			rCmdV1Header.cmdBufferLen +=
						sizeof(struct CMD_FORMAT_V1);
			rCmdV1Header.itemNum += ucTimes;

			if (ucTimes == MAX_CMD_ITEM_MAX) {
				/* Send to FW */
				rCmdV1Header.itemNum = ucTimes;

				rStatus = wlanSendSetQueryCmd(
						/* prAdapter */
						prAdapter,
						/* 0x70 */
						CMD_ID_GET_SET_CUSTOMER_CFG,
						/* fgSetQuery */
						TRUE,
						/* fgNeedResp */
						FALSE,
						/* fgIsOid */
						FALSE,
						/* pfCmdDoneHandler*/
						NULL,
						/* pfCmdTimeoutHandler */
						NULL,
						/* u4SetQueryInfoLen */
						sizeof(struct CMD_HEADER),
						/* pucInfoBuffer */
						(uint8_t *)&rCmdV1Header,
						/* pvSetQueryBuffer */
						NULL,
						/* u4SetQueryBufferLen */
						0);

				if (rStatus == WLAN_STATUS_FAILURE)
					DBGLOG(INIT, INFO,
					       "[Fail]kalIoctl wifiSefCFG fail 0x%x\n",
					       rStatus);

				DBGLOG(INIT, TRACE,
				       "kalIoctl wifiSefCFG num:%d\n", ucTimes);
				kalMemSet(rCmdV1Header.buffer, 0,
					  MAX_CMD_BUFFER_LENGTH);
				rCmdV1Header.cmdBufferLen = 0;
				ucTimes = 0;
			}


		} else {
			break;
		}
	}


	if (ucTimes != 0) {
		/* Send to FW */
		rCmdV1Header.itemNum = ucTimes;

		rStatus = wlanSendSetQueryCmd(
			  prAdapter,	/* prAdapter */
			  CMD_ID_GET_SET_CUSTOMER_CFG,	/* 0x70 */
			  TRUE,		/* fgSetQuery */
			  FALSE,	/* fgNeedResp */
			  FALSE,	/* fgIsOid */
			  NULL,		/* pfCmdDoneHandler*/
			  NULL,		/* pfCmdTimeoutHandler */
			  sizeof(struct CMD_HEADER),	/* u4SetQueryInfoLen */
			  (uint8_t *)&rCmdV1Header,/* pucInfoBuffer */
			  NULL,	/* pvSetQueryBuffer */
			  0);	/* u4SetQueryBufferLen */

		if (rStatus == WLAN_STATUS_FAILURE)
			DBGLOG(INIT, INFO,
			       "[Fail]kalIoctl wifiSefCFG fail 0x%x\n",
			       rStatus);

		DBGLOG(INIT, TRACE,
			"cmdV1Header.itemNum:%d, kalIoctl wifiSefCFG num:%d\n",
			rCmdV1Header.itemNum,
			ucTimes);
		kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
		rCmdV1Header.cmdBufferLen = 0;
		ucTimes = 0;
	}

#if CFG_SUPPORT_SMART_GEAR
	/*Send Event, Notify Fwks*/
	#if CFG_SUPPORT_DATA_STALL
	if (prAdapter->rWifiVar.ucSGCfg == 0x00) {
		enum ENUM_VENDOR_DRIVER_EVENT eEvent = EVENT_SG_DISABLE;

		KAL_REPORT_ERROR_EVENT(prAdapter,
			eEvent, (uint16_t)sizeof(u_int8_t),
			0,
			TRUE);
	}
	#endif /* CFG_SUPPORT_DATA_STALL */
#endif

}

uint32_t wlanCfgParse(struct ADAPTER *prAdapter,
		      uint8_t *pucConfigBuf, uint32_t u4ConfigBufLen,
		      u_int8_t isFwConfig)
{
	struct WLAN_CFG_PARSE_STATE_S state;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int8_t **ppcArgs;
	int32_t i4Nargs;
	int8_t   arcArgv_size[WLAN_CFG_ARGV_MAX];
	uint8_t  ucTimes = 0;
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	struct CMD_HEADER rCmdV1Header;
	int8_t ucTmp[WLAN_CFG_VALUE_LEN_MAX];
	uint8_t i;

	uint8_t *pucCurrBuf = ucTmp;
	uint32_t u4CurrSize = ARRAY_SIZE(ucTmp);
	uint32_t u4RetSize = 0;

	rCmdV1Header.cmdType = CMD_TYPE_SET;
	rCmdV1Header.cmdVersion = CMD_VER_1;
	rCmdV1Header.cmdBufferLen = 0;
	kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);

	if (pucConfigBuf == NULL) {
		DBGLOG(INIT, ERROR, "pucConfigBuf is NULL\n");
		return WLAN_STATUS_FAILURE;
	}
	if (kalStrnLen(pucConfigBuf, 4000) >= 4000) {
		DBGLOG(INIT, ERROR, "pucConfigBuf >= 4000\n");
		return WLAN_STATUS_FAILURE;
	}
	if (u4ConfigBufLen == 0)
		return WLAN_STATUS_FAILURE;

	ppcArgs = apcArgv;
	i4Nargs = 0;
	state.ptr = pucConfigBuf;
	state.nexttoken = 0;
	state.textsize = 0;
	state.maxSize = u4ConfigBufLen;
	DBGLOG(INIT, INFO, "wlanCfgParse()\n");

	for (;;) {
		switch (wlanCfgFindNextToken(&state)) {
		case STATE_EOF:
			if (i4Nargs < 2)
				goto exit;

			DBGLOG(INIT, INFO, "STATE_EOF\n");

			/* 3 parmeter mode transforation */
			if (i4Nargs == 3 && !isFwConfig &&
			    arcArgv_size[0] == 1) {

				/* parsing and transfer the format
				 * Format  1:Dec 2.Hex 3.String
				 */

				kalMemZero(ucTmp, WLAN_CFG_VALUE_LEN_MAX);
				pucCurrBuf = ucTmp;
				u4CurrSize = ARRAY_SIZE(ucTmp);

				if ((*ppcArgs[0] == '2') &&
				    (*(ppcArgs[2]) != '0') &&
				    (*(ppcArgs[2] + 1) != 'x')) {
					DBGLOG(INIT, WARN,
					       "config file got a hex format\n"
					       );
					kalSnprintf(pucCurrBuf, u4CurrSize,
						    "0x%s", ppcArgs[2]);
				} else {
					kalSnprintf(pucCurrBuf, u4CurrSize,
						    "%s", ppcArgs[2]);
				}
				DBGLOG(INIT, WARN,
				       "[3 parameter mode][%s],[%s],[%s]\n",
				       ppcArgs[0], ppcArgs[1], ucTmp);
				wlanCfgParseAddEntry(prAdapter, ppcArgs[1],
						     NULL, ucTmp, NULL);
				kalMemSet(arcArgv_size, 0, WLAN_CFG_ARGV_MAX);
				kalMemSet(apcArgv, 0,
					WLAN_CFG_ARGV_MAX * sizeof(int8_t *));
				i4Nargs = 0;
				goto exit;

			}

			wlanCfgParseAddEntry(prAdapter, ppcArgs[0], NULL,
					     ppcArgs[1], NULL);

			if (isFwConfig) {
				uint32_t ret;

				ret = wlanCfgParseToFW(ppcArgs, arcArgv_size,
						       i4Nargs,
						       rCmdV1Header.buffer,
						       ucTimes);
				if (ret == WLAN_STATUS_SUCCESS) {
					ucTimes++;
					rCmdV1Header.cmdBufferLen +=
						sizeof(struct CMD_FORMAT_V1);
				}
			}

			goto exit;


		case STATE_NEWLINE:
			if (i4Nargs < 2)
				break;

			DBGLOG(INIT, INFO, "STATE_NEWLINE\n");
#if 1
			/* 3 parmeter mode transforation */
			if (i4Nargs == 3 && !isFwConfig &&
			    arcArgv_size[0] == 1) {
				/* parsing and transfer the format
				 * Format  1:Dec 2.Hex 3.String
				 */
				kalMemZero(ucTmp, WLAN_CFG_VALUE_LEN_MAX);
				pucCurrBuf = ucTmp;
				u4CurrSize = ARRAY_SIZE(ucTmp);

				if ((*ppcArgs[0] == '2') &&
				    (*(ppcArgs[2]) != '0') &&
				    (*(ppcArgs[2] + 1) != 'x')) {
					DBGLOG(INIT, WARN,
					       "config file got a hex format\n");
					kalSnprintf(pucCurrBuf, u4CurrSize,
						    "0x%s", ppcArgs[2]);

				} else {
					kalSnprintf(pucCurrBuf, u4CurrSize,
						    "%s", ppcArgs[2]);
				}


				DBGLOG(INIT, WARN,
				       "[3 parameter mode][%s],[%s],[%s]\n",
				       ppcArgs[0], ppcArgs[1], ucTmp);
				wlanCfgParseAddEntry(prAdapter, ppcArgs[1],
						     NULL, ucTmp, NULL);
				kalMemSet(arcArgv_size, 0, WLAN_CFG_ARGV_MAX);
				kalMemSet(apcArgv, 0,
					WLAN_CFG_ARGV_MAX * sizeof(int8_t *));
				i4Nargs = 0;
				break;

			}
#if 1
			/*combine the argument to save in temp*/
			pucCurrBuf = ucTmp;
			u4CurrSize = ARRAY_SIZE(ucTmp);

			kalMemZero(ucTmp, WLAN_CFG_VALUE_LEN_MAX);

			if (i4Nargs == 2) {
				/* no space for it, driver can't accept space in
				 * the end of the line
				 */
				/* ToDo: skip the space when parsing */
				kalSnprintf(pucCurrBuf, u4CurrSize, "%s",
					    ppcArgs[1]);
			} else {
				for (i = 1; i < i4Nargs; i++) {
					if (u4CurrSize <= 1) {
						DBGLOG(INIT, ERROR,
						       "write to pucCurrBuf out of bound, i=%d\n",
						       i);
						break;
					}
					u4RetSize = kalScnprintf(pucCurrBuf,
							      u4CurrSize, "%s ",
							      ppcArgs[i]);
					pucCurrBuf += u4RetSize;
					u4CurrSize -= u4RetSize;
				}
			}

			DBGLOG(INIT, WARN,
			       "Save to driver temp buffer as [%s]\n",
			       ucTmp);
			wlanCfgParseAddEntry(prAdapter, ppcArgs[0], NULL, ucTmp,
					     NULL);
#else
			wlanCfgParseAddEntry(prAdapter, ppcArgs[0], NULL,
					     ppcArgs[1], NULL);
#endif

			if (isFwConfig) {

				uint32_t ret;

				ret = wlanCfgParseToFW(ppcArgs, arcArgv_size,
					i4Nargs, rCmdV1Header.buffer, ucTimes);
				if (ret == WLAN_STATUS_SUCCESS) {
					ucTimes++;
					rCmdV1Header.cmdBufferLen +=
						sizeof(struct CMD_FORMAT_V1);
				}

				if (ucTimes == MAX_CMD_ITEM_MAX) {
					/* Send to FW */
					rCmdV1Header.itemNum = ucTimes;
					rStatus = wlanSendSetQueryCmd(
						/* prAdapter */
						prAdapter,
						/* 0x70 */
						CMD_ID_GET_SET_CUSTOMER_CFG,
						/* fgSetQuery */
						TRUE,
						/* fgNeedResp */
						FALSE,
						/* fgIsOid */
						FALSE,
						/* pfCmdDoneHandler*/
						NULL,
						/* pfCmdTimeoutHandler */
						NULL,
						/* u4SetQueryInfoLen */
						sizeof(struct CMD_HEADER),
						/* pucInfoBuffer */
						(uint8_t *) &rCmdV1Header,
						/* pvSetQueryBuffer */
						NULL,
						/* u4SetQueryBufferLen */
						0);

					if (rStatus == WLAN_STATUS_FAILURE)
						DBGLOG(INIT, INFO,
						       "kalIoctl wifiSefCFG fail 0x%x\n",
						       rStatus);
					DBGLOG(INIT, INFO,
					       "kalIoctl wifiSefCFG num:%d X\n",
					       ucTimes);
					kalMemSet(rCmdV1Header.buffer, 0,
						  MAX_CMD_BUFFER_LENGTH);
					rCmdV1Header.cmdBufferLen = 0;
					ucTimes = 0;
				}

			}

#endif
			kalMemSet(arcArgv_size, 0, WLAN_CFG_ARGV_MAX);
			kalMemSet(apcArgv, 0,
				WLAN_CFG_ARGV_MAX * sizeof(int8_t *));
			i4Nargs = 0;
			break;

		case STATE_TEXT:
			if (i4Nargs >= 0 && i4Nargs < WLAN_CFG_ARGV_MAX) {
				ppcArgs[i4Nargs++] = state.text;
				arcArgv_size[i4Nargs - 1] = state.textsize;
				state.textsize = 0;
				DBGLOG(INIT, INFO,
				       " nargs= %d STATE_TEXT = %s, SIZE = %d\n",
				       i4Nargs - 1, ppcArgs[i4Nargs - 1],
				       arcArgv_size[i4Nargs - 1]);
			}
			break;
		}
	}

exit:
	if (ucTimes != 0 && isFwConfig) {
		/* Send to FW */
		rCmdV1Header.itemNum = ucTimes;

		DBGLOG(INIT, INFO, "cmdV1Header.itemNum:%d\n",
		       rCmdV1Header.itemNum);
		rStatus = wlanSendSetQueryCmd(
			  prAdapter,	/* prAdapter */
			  CMD_ID_GET_SET_CUSTOMER_CFG,	/* 0x70 */
			  TRUE,		/* fgSetQuery */
			  FALSE,	/* fgNeedResp */
			  FALSE,	/* fgIsOid */
			  NULL,		/* pfCmdDoneHandler*/
			  NULL,		/* pfCmdTimeoutHandler */
			  sizeof(struct CMD_HEADER), /* u4SetQueryInfoLen */
			  (uint8_t *) &rCmdV1Header, /* pucInfoBuffer */
			  NULL,		/* pvSetQueryBuffer */
			  0		/* u4SetQueryBufferLen */
			  );

		if (rStatus == WLAN_STATUS_FAILURE)
			DBGLOG(INIT, WARN, "kalIoctl wifiSefCFG fail 0x%x\n",
			       rStatus);

		DBGLOG(INIT, WARN, "kalIoctl wifiSefCFG num:%d X\n",
		       ucTimes);
		kalMemSet(rCmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);
		rCmdV1Header.cmdBufferLen = 0;
		ucTimes = 0;
	}
	return WLAN_STATUS_SUCCESS;
}

#else
uint32_t wlanCfgParse(struct ADAPTER *prAdapter,
		      uint8_t *pucConfigBuf, uint32_t u4ConfigBufLen)
{

	struct WLAN_CFG_PARSE_STATE_S state;
	int8_t *apcArgv[WLAN_CFG_ARGV_MAX];
	int8_t **args;
	int32_t nargs;

	if (pucConfigBuf == NULL) {
		DBGLOG(INIT, ERROR, "pucConfigBuf is NULL\n");
		return WLAN_STATUS_FAILURE;
	}
	if (kalStrnLen(pucConfigBuf, 4000) >= 4000) {
		DBGLOG(INIT, ERROR, "pucConfigBuf >= 4000\n");
		return WLAN_STATUS_FAILURE;
	}
	if (u4ConfigBufLen == 0)
		return WLAN_STATUS_FAILURE;
	args = apcArgv;
	nargs = 0;
	state.ptr = pucConfigBuf;
	state.nexttoken = 0;
	state.maxSize = u4ConfigBufLen;

	for (;;) {
		switch (wlanCfgFindNextToken(&state)) {
		case STATE_EOF:
			if (nargs > 1)
				wlanCfgParseAddEntry(prAdapter, args[0], NULL,
						     args[1], NULL);
			goto exit;
		case STATE_NEWLINE:
			if (nargs > 1)
				wlanCfgParseAddEntry(prAdapter, args[0], NULL,
						     args[1], NULL);
			/*args[0] is parameter, args[1] is the value*/
			nargs = 0;
			break;
		case STATE_TEXT:
			if (nargs < WLAN_CFG_ARGV_MAX)
				args[nargs++] = state.text;
			break;
		}
	}

exit:
	return WLAN_STATUS_SUCCESS;

#if 0
	/* Old version */
	uint32_t i;
	uint8_t c;
	uint8_t *pbuf;
	uint8_t ucState;
	uint8_t *pucKeyTail = NULL;
	uint8_t *pucKeyHead = NULL;
	uint8_t *pucValueHead = NULL;
	uint8_t *pucValueTail = NULL;

	ucState = WAIT_KEY_HEAD;
	pbuf = pucConfigBuf;

	for (i = 0; i < u4ConfigBufLen; i++) {
		c = pbuf[i];
		if (c == '\r' || c == '\n') {

			if (ucState == WAIT_VALUE_TAIL) {
				/* Entry found */
				if (pucValueHead)
					wlanCfgParseAddEntry(prAdapter,
						pucKeyHead, pucKeyTail,
						pucValueHead, pucValueTail);
			}
			ucState = WAIT_KEY_HEAD;
			pucKeyTail = NULL;
			pucKeyHead = NULL;
			pucValueHead = NULL;
			pucValueTail = NULL;

		} else if (c == '=') {
			if (ucState == WAIT_KEY_TAIL) {
				pucKeyTail = &pbuf[i - 1];
				ucState = WAIT_VALUE_HEAD;
			}
		} else if (c == ' ' || c == '\t') {
			if (ucState == WAIT_KEY_TAIL) {
				pucKeyTail = &pbuf[i - 1];
				ucState = WAIT_VALUE_HEAD;
			}
		} else {

			if (c == '#') {
				/* comments */
				if (ucState == WAIT_KEY_HEAD)
					ucState = WAIT_COMMENT_TAIL;
				else if (ucState == WAIT_VALUE_TAIL)
					pucValueTail = &pbuf[i];

			} else {
				if (ucState == WAIT_KEY_HEAD) {
					pucKeyHead = &pbuf[i];
					pucKeyTail = &pbuf[i];
					ucState = WAIT_KEY_TAIL;
				} else if (ucState == WAIT_VALUE_HEAD) {
					pucValueHead = &pbuf[i];
					pucValueTail = &pbuf[i];
					ucState = WAIT_VALUE_TAIL;
				} else if (ucState == WAIT_VALUE_TAIL)
					pucValueTail = &pbuf[i];
			}
		}

	}			/* for */

	if (ucState == WAIT_VALUE_TAIL) {
		/* Entry found */
		if (pucValueTail)
			wlanCfgParseAddEntry(prAdapter, pucKeyHead, pucKeyTail,
					     pucValueHead, pucValueTail);
	}
#endif

	return WLAN_STATUS_SUCCESS;
}
#endif


uint32_t wlanCfgInit(struct ADAPTER *prAdapter,
		     uint8_t *pucConfigBuf, uint32_t u4ConfigBufLen,
		     uint32_t u4Flags)
{
	struct WLAN_CFG *prWlanCfg;
	struct WLAN_CFG_REC *prWlanCfgRec;
	struct WLAN_CFG *prWlanCfgEm;

	/* P_WLAN_CFG_ENTRY_T prWlanCfgEntry; */
	prAdapter->prWlanCfg = &prAdapter->rWlanCfg;
	prWlanCfg = prAdapter->prWlanCfg;

	prAdapter->prWlanCfgRec = &prAdapter->rWlanCfgRec;
	prWlanCfgRec = prAdapter->prWlanCfgRec;

	prAdapter->prWlanCfgEm = &prAdapter->rWlanCfgEm;
	prWlanCfgEm = prAdapter->prWlanCfgEm;

	kalMemZero(prWlanCfg, sizeof(struct WLAN_CFG));
	ASSERT(prWlanCfg);

	kalMemZero(prWlanCfgEm, sizeof(struct WLAN_CFG));
	ASSERT(prWlanCfgEm);

	prWlanCfg->u4WlanCfgEntryNumMax = WLAN_CFG_ENTRY_NUM_MAX;
	prWlanCfg->u4WlanCfgKeyLenMax = WLAN_CFG_KEY_LEN_MAX;
	prWlanCfg->u4WlanCfgValueLenMax = WLAN_CFG_VALUE_LEN_MAX;

	prWlanCfgRec->u4WlanCfgEntryNumMax =
		WLAN_CFG_REC_ENTRY_NUM_MAX;
	prWlanCfgRec->u4WlanCfgKeyLenMax =
		WLAN_CFG_KEY_LEN_MAX;
	prWlanCfgRec->u4WlanCfgValueLenMax =
		WLAN_CFG_VALUE_LEN_MAX;

	prWlanCfgEm->u4WlanCfgEntryNumMax = WLAN_CFG_ENTRY_NUM_MAX;
	prWlanCfgEm->u4WlanCfgKeyLenMax = WLAN_CFG_KEY_LEN_MAX;
	prWlanCfgEm->u4WlanCfgValueLenMax = WLAN_CFG_VALUE_LEN_MAX;


	DBGLOG(INIT, INFO, "Init wifi config len %u max entry %u\n",
	       u4ConfigBufLen, prWlanCfg->u4WlanCfgEntryNumMax);
#if DBG
	/* self test */
	wlanCfgSet(prAdapter, "ConfigValid", "0x123", WLAN_CFG_DEFAULT);
	if (wlanCfgGetUint32(prAdapter, "ConfigValid", WLAN_CFG_DEFAULT)
		!= 0x123)
		DBGLOG(INIT, INFO, "wifi config error %u\n", __LINE__);

	wlanCfgSet(prAdapter, "ConfigValid", "1", WLAN_CFG_DEFAULT);
	if (wlanCfgGetUint32(prAdapter, "ConfigValid", WLAN_CFG_DEFAULT) != 1)
		DBGLOG(INIT, INFO, "wifi config error %u\n", __LINE__);

#endif
	/*load default value because kalMemZero in this function*/
	wlanLoadDefaultCustomerSetting(prAdapter);

	/* Parse the pucConfigBuf */
	if (pucConfigBuf && (u4ConfigBufLen > 0))
#if CFG_SUPPORT_EASY_DEBUG
		wlanCfgParse(prAdapter, pucConfigBuf, u4ConfigBufLen,
			     FALSE);
#else
		wlanCfgParse(prAdapter, pucConfigBuf, u4ConfigBufLen);
#endif
	return WLAN_STATUS_SUCCESS;
}

#endif /* CFG_SUPPORT_CFG_FILE */

int32_t wlanHexToNum(int8_t c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

int32_t wlanHexToByte(int8_t *hex)
{
	int32_t a, b;

	a = wlanHexToNum(*hex++);
	if (a < 0)
		return -1;
	b = wlanHexToNum(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

int32_t wlanHexToArray(int8_t *hexString, int8_t *hexArray, uint8_t arrayLen)
{
		int8_t index = 0;
		uint8_t converted = 0;
		int strLen = strlen(hexString);
		int8_t result;

		for (index = strLen-1;
		converted < KAL_MIN(arrayLen*2, strLen); index--) {
			if ((strLen-converted) >= 2) {
				index--;
				result = wlanHexToByte(hexString+index);
				converted += 2;
			} else {
				result = wlanHexToNum(*(hexString+index));
				converted++;
			}
			if (result == -1)
				return 0;
			hexArray[(converted-1)/2] = result;
		}
		return converted;
}

int32_t wlanHexToArrayR(int8_t *hexString, int8_t *hexArray, uint8_t arrayLen)
{
	int8_t converted = 0;
	int len = strlen(hexString)/2;
	int8_t result;

	len = arrayLen < len ? arrayLen : len;
	if (*(hexString+1) == '\0') {
		result = wlanHexToNum(*(hexString));
		if (result != -1) {
			hexArray[converted] = result;
			converted++;
		}
	} else {
		for (converted = 0; converted < len; converted++) {
			result = wlanHexToByte(hexString + converted*2);
			hexArray[converted] = result;
		}
	}
	return converted;
}



int32_t wlanHwAddrToBin(int8_t *txt, uint8_t *addr)
{
	int32_t i;
	int8_t *pos = txt;

	for (i = 0; i < 6; i++) {
		int32_t a, b;

		while (*pos == ':' || *pos == '.' || *pos == '-')
			pos++;

		a = wlanHexToNum(*pos++);
		if (a < 0)
			return -1;
		b = wlanHexToNum(*pos++);
		if (b < 0)
			return -1;
		*addr++ = (a << 4) | b;
	}

	return pos - txt;
}

u_int8_t wlanIsChipNoAck(struct ADAPTER *prAdapter)
{
	u_int8_t fgIsNoAck = fgIsBusAccessFailed;

#if CFG_CHIP_RESET_SUPPORT
	fgIsNoAck = fgIsNoAck || kalIsResetting();
#endif
	if (prAdapter)
		fgIsNoAck = fgIsNoAck || prAdapter->fgIsChipNoAck;

	return fgIsNoAck;
}

u_int8_t wlanIsChipRstRecEnabled(struct ADAPTER
				 *prAdapter)
{
	return prAdapter->rWifiVar.fgChipResetRecover;
}

u_int8_t wlanIsChipAssert(struct ADAPTER *prAdapter)
{
	return prAdapter->rWifiVar.fgChipResetRecover
		&& prAdapter->fgIsChipAssert;
}

void wlanChipRstPreAct(struct ADAPTER *prAdapter)
{
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	int32_t i4BssIdx;
	uint32_t u4ClientCount = 0;
	struct STA_RECORD *prCurrStaRec = (struct STA_RECORD *)
					  NULL;
	struct STA_RECORD *prNextCurrStaRec = (struct STA_RECORD *)
					      NULL;
	struct LINK *prClientList;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;

	KAL_ACQUIRE_MUTEX(prAdapter, MUTEX_CHIP_RST);
	if (prAdapter->fgIsChipAssert) {
		KAL_RELEASE_MUTEX(prAdapter, MUTEX_CHIP_RST);
		return;
	}
	prAdapter->fgIsChipAssert = TRUE;
	KAL_RELEASE_MUTEX(prAdapter, MUTEX_CHIP_RST);

	for (i4BssIdx = 0; i4BssIdx < prAdapter->ucHwBssIdNum;
	     i4BssIdx++) {
		prBssInfo = prAdapter->aprBssInfo[i4BssIdx];

		if (!prBssInfo->fgIsInUse)
			continue;

		if (prBssInfo->eNetworkType == NETWORK_TYPE_P2P) {
			if (prBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
				u4ClientCount = bssGetClientCount(prAdapter,
								  prBssInfo);
				if (u4ClientCount == 0)
					continue;

				prClientList = &prBssInfo->rStaRecOfClientList;
				LINK_FOR_EACH_ENTRY_SAFE(prCurrStaRec,
				    prNextCurrStaRec, prClientList,
				    rLinkEntry, struct STA_RECORD) {
					if (!prCurrStaRec)
						break;
					kalP2PGOStationUpdate(
					    prAdapter->prGlueInfo,
					    (uint8_t) prBssInfo->u4PrivateData,
					    prCurrStaRec, FALSE);
					LINK_REMOVE_KNOWN_ENTRY(prClientList,
					    &prCurrStaRec->rLinkEntry);
				}
			} else if (prBssInfo->eCurrentOPMode ==
				   OP_MODE_INFRASTRUCTURE) {
				if (prBssInfo->prStaRecOfAP == NULL)
					continue;
				kalP2PGCIndicateConnectionStatus(prGlueInfo,
					(uint8_t) prBssInfo->u4PrivateData,
					NULL, NULL, 0,
					REASON_CODE_DEAUTH_LEAVING_BSS,
					WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY);
				prBssInfo->prStaRecOfAP = NULL;

			}
		}
	}
}

#if CFG_SUPPORT_TX_LATENCY_STATS
/**
 * wlanCountTxDelayOverLimit() - Count and check whether the TX delay over limit
 *
 * @prAdapter: pointer to Adapter
 * @type: delay type, could be DRIVER_DELAY or MAC_DELAY
 * @u4Latency: the measured latency for one transmitted MSDU
 *
 * This function shall be called on handling each MSDU transmission result.
 */
void wlanCountTxDelayOverLimit(struct ADAPTER *prAdapter,
		enum ENUM_TX_OVER_LIMIT_DELAY_TYPE type,
		uint32_t u4Latency)
{
	struct TX_DELAY_OVER_LIMIT_REPORT_STATS *stats =
			&prAdapter->rTxDelayOverLimitStats;

	if (!stats->fgTxDelayOverLimitReportEnabled)
		return;

	if (stats->eTxDelayOverLimitStatsType == REPORT_AVERAGE) {
		stats->u4Delay[type] += u4Latency;
		stats->u4DelayNum[type]++;
	} else if (u4Latency >= stats->u4DelayLimit[type] &&
		   !stats->fgReported[type]) {
		wlanReportTxDelayOverLimit(prAdapter, type, u4Latency);
		stats->fgReported[type] = true;
	}
}

static void halAddDriverLatencyCount(struct ADAPTER *prAdapter,
	uint8_t ucBssIndex, uint32_t u4DriverLatency)
{
	uint32_t *pMaxDriverDelay = prAdapter->rWifiVar.au4DriverTxDelayMax;
	uint32_t *pDriverDelay =
		prAdapter->rMsduReportStats.rCounting.au4DriverLatency
							[ucBssIndex];
	int i;

	for (i = 0; i < LATENCY_STATS_MAX_SLOTS; i++) {
		if (u4DriverLatency <= *pMaxDriverDelay++) {
			GLUE_INC_REF_CNT(pDriverDelay[i]);
			break;
		}
	}

	wlanCountTxDelayOverLimit(prAdapter, DRIVER_DELAY, u4DriverLatency);
}
#endif

#if CFG_ENABLE_PER_STA_STATISTICS
void wlanTxLifetimeUpdateStaStats(struct ADAPTER
				  *prAdapter, struct MSDU_INFO *prMsduInfo)
{
	struct STA_RECORD *prStaRec;
	uint32_t u4DeltaTime;
	uint32_t u4DeltaHifTxTime;
	struct PKT_PROFILE *prPktProfile = &prMsduInfo->rPktProfile;
#if 0
	struct QUE_MGT *prQM = &prAdapter->rQM;
	uint32_t u4PktPrintPeriod = 0;
#endif

	prStaRec = cnmGetStaRecByIndex(prAdapter,
				       prMsduInfo->ucStaRecIndex);

	if (prStaRec) {
		u4DeltaTime = (uint32_t) (prPktProfile->rHifTxDoneTimestamp -
				prPktProfile->rHardXmitArrivalTimestamp);
		u4DeltaHifTxTime = (uint32_t) (
				prPktProfile->rHifTxDoneTimestamp -
				prPktProfile->rDequeueTimestamp);

		/* Update StaRec statistics */
		prStaRec->u4TotalTxPktsNumber++;
		prStaRec->u4TotalTxPktsTime += u4DeltaTime;
		prStaRec->u4TotalTxPktsHifTxTime += u4DeltaHifTxTime;

		if (u4DeltaTime > prStaRec->u4MaxTxPktsTime)
			prStaRec->u4MaxTxPktsTime = u4DeltaTime;

		if (u4DeltaHifTxTime > prStaRec->u4MaxTxPktsHifTime)
			prStaRec->u4MaxTxPktsHifTime = u4DeltaHifTxTime;

		if (u4DeltaTime >= NIC_TX_TIME_THRESHOLD)
			prStaRec->u4ThresholdCounter++;

#if 0
		if (u4PktPrintPeriod &&
		    (prStaRec->u4TotalTxPktsNumber >= u4PktPrintPeriod)) {
			DBGLOG(TX, INFO, "[%u]N[%u]A[%u]M[%u]T[%u]E[%4u]\n",
			       prStaRec->ucIndex,
			       prStaRec->u4TotalTxPktsNumber,
			       prStaRec->u4TotalTxPktsTime,
			       prStaRec->u4MaxTxPktsTime,
			       prStaRec->u4ThresholdCounter,
			       prQM->au4QmTcResourceEmptyCounter[
				prStaRec->ucBssIndex][TC2_INDEX]);

			prStaRec->u4TotalTxPktsNumber = 0;
			prStaRec->u4TotalTxPktsTime = 0;
			prStaRec->u4MaxTxPktsTime = 0;
			prStaRec->u4ThresholdCounter = 0;
			prQM->au4QmTcResourceEmptyCounter[
				prStaRec->ucBssIndex][TC2_INDEX] = 0;
		}
#endif
	}
}
#endif

u_int8_t wlanTxLifetimeIsProfilingEnabled(
	struct ADAPTER *prAdapter)
{
	u_int8_t fgEnabled = TRUE;
#if CFG_SUPPORT_WFD
	struct WFD_CFG_SETTINGS *prWfdCfgSettings =
		(struct WFD_CFG_SETTINGS *) NULL;

	prWfdCfgSettings =
		&prAdapter->rWifiVar.rWfdConfigureSettings;

	if (prWfdCfgSettings->ucWfdEnable > 0)
		fgEnabled = TRUE;
#endif

	return fgEnabled;
}

u_int8_t wlanTxLifetimeIsTargetMsdu(struct ADAPTER
				    *prAdapter, struct MSDU_INFO *prMsduInfo)
{
	u_int8_t fgResult = TRUE;

#if 0
	switch (prMsduInfo->ucTID) {
	/* BK */
	case 1:
	case 2:

	/* BE */
	case 0:
	case 3:
		fgResult = FALSE;
		break;
	/* VI */
	case 4:
	case 5:

	/* VO */
	case 6:
	case 7:
		fgResult = TRUE;
		break;
	default:
		break;
	}
#endif
	return fgResult;
}

void wlanTxLifetimeTagPacket(struct ADAPTER *prAdapter,
			     struct MSDU_INFO *prMsduInfo,
			     enum ENUM_TX_PROFILING_TAG eTag)
{
	struct PKT_PROFILE *prPktProfile = &prMsduInfo->rPktProfile;
#if CFG_SUPPORT_TX_LATENCY_STATS
	struct WIFI_VAR *prWifiVar = &prAdapter->rWifiVar;
#endif

	if (!wlanTxLifetimeIsProfilingEnabled(prAdapter))
		return;

	switch (eTag) {
	case TX_PROF_TAG_OS_TO_DRV:
		/* arrival time is tagged in wlanProcessTxFrame */
		break;

	case TX_PROF_TAG_DRV_ENQUE:
		/* Reset packet profile */
		prPktProfile->fgIsValid = FALSE;
		if (wlanTxLifetimeIsTargetMsdu(prAdapter, prMsduInfo)) {
			/* Enable packet lifetime profiling */
			prPktProfile->fgIsValid = TRUE;

			/* Packet arrival time at kernel Hard Xmit */
			prPktProfile->rHardXmitArrivalTimestamp =
				GLUE_GET_PKT_ARRIVAL_TIME(prMsduInfo->prPacket);

			/* Packet enqueue time */
			prPktProfile->rEnqueueTimestamp = (OS_SYSTIME)
							  kalGetTimeTick();
#if CFG_SUPPORT_TX_LATENCY_STATS
			prPktProfile->u8XmitArrival =
				GLUE_GET_PKT_XTIME(prMsduInfo->prPacket);
			prPktProfile->u8EnqTime = StatsEnvTimeGet();
#endif
		}
		break;

	case TX_PROF_TAG_DRV_DEQUE:
		if (prPktProfile->fgIsValid) {
			prPktProfile->rDequeueTimestamp = (OS_SYSTIME)
							  kalGetTimeTick();
#if CFG_SUPPORT_TX_LATENCY_STATS
			prPktProfile->u8DeqTime = StatsEnvTimeGet();
#endif
		}
		break;

	case TX_PROF_TAG_DRV_TX_DONE:
		if (prPktProfile->fgIsValid) {
			prPktProfile->rHifTxDoneTimestamp = (OS_SYSTIME)
							    kalGetTimeTick();
#if CFG_SUPPORT_TX_LATENCY_STATS
			prPktProfile->u8HifTxTime = StatsEnvTimeGet();
			if (prWifiVar->fgPacketLatencyLog)
				DBGLOG(TX, INFO,
					"Latency(us) HIF_D:%lu, DEQ_D:%lu, ENQ_D:%lu A:%llu BSSIDX:WIDX:PID[%u:%u:%u] IPID:0x%04x SeqNo:%d\n",
					NSEC_TO_USEC((uint32_t)(
					prPktProfile->u8HifTxTime -
					prPktProfile->u8XmitArrival)),
					NSEC_TO_USEC((uint32_t)(
					prPktProfile->u8DeqTime -
					prPktProfile->u8XmitArrival)),
					NSEC_TO_USEC((uint32_t)(
					prPktProfile->u8EnqTime -
					prPktProfile->u8XmitArrival)),
					prPktProfile->u8XmitArrival,
					prMsduInfo->ucBssIndex,
					prMsduInfo->ucWlanIndex,
					prMsduInfo->ucPID,
					GLUE_GET_PKT_IP_ID(
					prMsduInfo->prPacket),
					GLUE_GET_PKT_SEQ_NO(
					prMsduInfo->prPacket));

			halAddDriverLatencyCount(prAdapter,
				prMsduInfo->ucBssIndex,
				((uint32_t)(prPktProfile->u8HifTxTime -
				prPktProfile->u8XmitArrival)) /
				USEC_PER_SEC);
#endif

#if CFG_ENABLE_PER_STA_STATISTICS
			wlanTxLifetimeUpdateStaStats(prAdapter, prMsduInfo);
#endif

#if CFG_SUPPORT_TDLS_P2P_AUTO
			TdlsP2pAuto(
				prAdapter,
				prMsduInfo->ucBssIndex,
				prMsduInfo->u2FrameLength,
				0,
				prMsduInfo->aucEthDestAddr);
#endif
		}
		break;
	default:
		break;
	}
}

void wlanTxProfilingTagPacket(struct ADAPTER *prAdapter,
			      void *prPacket,
			      enum ENUM_TX_PROFILING_TAG eTag)
{
	if (!prPacket)
		return;

#if CFG_MET_PACKET_TRACE_SUPPORT
	kalMetTagPacket(prAdapter->prGlueInfo, prPacket, eTag);
#endif

	switch (eTag) {
	case TX_PROF_TAG_OS_TO_DRV:
		kalTraceEvent("Xmit ipid=0x%04x seq=%d",
			GLUE_GET_PKT_IP_ID(prPacket),
			GLUE_GET_PKT_SEQ_NO(prPacket));
		DBGLOG(TX, TEMP, "Xmit ipid=%d seq=%d\n",
			GLUE_GET_PKT_IP_ID(prPacket),
			GLUE_GET_PKT_SEQ_NO(prPacket));
		break;
	case TX_PROF_TAG_DRV_ENQUE:
		kalTraceEvent("Enq ipid=0x%04x seq=%d",
			GLUE_GET_PKT_IP_ID(prPacket),
			GLUE_GET_PKT_SEQ_NO(prPacket));
		DBGLOG(TX, TEMP, "Enq ipid=%d seq=%d\n",
			GLUE_GET_PKT_IP_ID(prPacket),
			GLUE_GET_PKT_SEQ_NO(prPacket));
		break;
	case TX_PROF_TAG_DRV_FREE:
		kalTraceEvent("Cmpl ipid=0x%04x seq=%d",
			GLUE_GET_PKT_IP_ID(prPacket),
			GLUE_GET_PKT_SEQ_NO(prPacket));
		DBGLOG(TX, TEMP, "Cmpl ipid=%d seq=%d\n",
			GLUE_GET_PKT_IP_ID(prPacket),
			GLUE_GET_PKT_SEQ_NO(prPacket));
		break;
	default:
		break;
	}
}

void wlanTxProfilingTagMsdu(struct ADAPTER *prAdapter,
			    struct MSDU_INFO *prMsduInfo,
			    enum ENUM_TX_PROFILING_TAG eTag)
{
	wlanTxLifetimeTagPacket(prAdapter, prMsduInfo, eTag);

	if (prMsduInfo->eSrc == TX_PACKET_OS) {
		/* only profile packets from OS */
		wlanTxProfilingTagPacket(prAdapter, prMsduInfo->prPacket, eTag);
	}
}

void wlanUpdateTxStatistics(struct ADAPTER *prAdapter,
			    struct MSDU_INFO *prMsduInfo,
			    u_int8_t fgTxDrop)
{
	struct STA_RECORD *prStaRec;
	struct BSS_INFO *prBssInfo;
	enum ENUM_WMM_ACI eAci = WMM_AC_BE_INDEX;
	struct QUE_MGT *prQM = &prAdapter->rQM;
	OS_SYSTIME rCurTime;
	struct WIFI_WMM_AC_STAT *prAcStats;

	eAci = aucTid2ACI[prMsduInfo->ucUserPriority];

	prStaRec = cnmGetStaRecByIndex(prAdapter,
				       prMsduInfo->ucStaRecIndex);

	if (eAci < WMM_AC_INDEX_NUM) {
		if (prStaRec) {
			if (fgTxDrop)
				prStaRec->arLinkStatistics[eAci].u4TxDropMsdu++;
			else
				prStaRec->arLinkStatistics[eAci].u4TxMsdu++;
		} else {
			prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
						  prMsduInfo->ucBssIndex);

			if (prBssInfo) {
				prAcStats = &prBssInfo->arLinkStatistics[eAci];
				if (fgTxDrop)
					prAcStats->u4TxDropMsdu++;
				else
					prAcStats->u4TxMsdu++;
			}
		}
	}

#if CFG_AP_80211KVR_INTERFACE
	prStaRec = cnmGetStaRecByAddress(prAdapter,
				prMsduInfo->ucBssIndex,
				prMsduInfo->aucEthDestAddr);
	if (prStaRec && !fgTxDrop)
		prStaRec->u8TotalTxBytes += prMsduInfo->u2FrameLength;
#endif

	/* Trigger FW stats log every 20s */
	rCurTime = (OS_SYSTIME) kalGetTimeTick();

	DBGLOG(INIT, LOUD, "CUR[%u] LAST[%u] TO[%u]\n", rCurTime,
	       prQM->rLastTxPktDumpTime,
	       CHECK_FOR_TIMEOUT(rCurTime, prQM->rLastTxPktDumpTime,
				 MSEC_TO_SYSTIME(
				 prAdapter->rWifiVar.u4StatsLogTimeout)));

	if (CHECK_FOR_TIMEOUT(rCurTime, prQM->rLastTxPktDumpTime,
			      MSEC_TO_SYSTIME(
			      prAdapter->rWifiVar.u4StatsLogTimeout))) {

		wlanDumpAllBssStatistics(prAdapter);

		prQM->rLastTxPktDumpTime = rCurTime;
	}
}

void wlanUpdateRxStatistics(struct ADAPTER *prAdapter,
			    struct SW_RFB *prSwRfb)
{
	struct STA_RECORD *prStaRec;
	enum ENUM_WMM_ACI eAci = WMM_AC_BE_INDEX;

	eAci = aucTid2ACI[prSwRfb->ucTid];

	prStaRec = cnmGetStaRecByIndex(prAdapter,
				       prSwRfb->ucStaRecIdx);
	if (prStaRec && eAci < WMM_AC_INDEX_NUM)
		prStaRec->arLinkStatistics[eAci].u4RxMsdu++;
}

void wlanReportTxDelayOverLimit(struct ADAPTER *prAdapter,
		enum ENUM_TX_OVER_LIMIT_DELAY_TYPE type, uint32_t delay)
{
#if CFG_SUPPORT_TX_LATENCY_STATS
	char uevent[64];

	if (!prAdapter->rTxDelayOverLimitStats.fgTxDelayOverLimitReportEnabled)
		return;

	DBGLOG(HAL, INFO, "Send uevent abnormaltrx=DIR:TX,event:Ab%sDelay:%u",
			type == DRIVER_DELAY ? "Driver" : "Mac", delay);
	kalSnprintf(uevent, sizeof(uevent),
			"abnormaltrx=DIR:TX,event:Ab%sDelay:%u",
			type == DRIVER_DELAY ? "Driver" : "Mac", delay);
	kalSendUevent(uevent);
#endif
}

uint32_t
wlanPktTxDone(struct ADAPTER *prAdapter,
	      struct MSDU_INFO *prMsduInfo,
	      enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	OS_SYSTIME rCurrent = kalGetTimeTick();
	struct PKT_PROFILE *prPktProfile = &prMsduInfo->rPktProfile;
	struct EVENT_TX_DONE *prTxDone = prMsduInfo->prTxDone;
#if CFG_SUPPORT_TX_MGMT_USE_DATAQ
	struct WLAN_MAC_HEADER *prWlanHeader = NULL;
	void *pvPacket = NULL;
	uint8_t *pucBuf = NULL;
	uint32_t u4PacketLen = 0;
	u_int8_t fgIsSuccess = FALSE;
#endif

	if (prMsduInfo->ucPktType >= ENUM_PKT_FLAG_NUM)
		prMsduInfo->ucPktType = 0;

	if (prPktProfile->fgIsValid &&
		((prMsduInfo->ucPktType == ENUM_PKT_ARP) ||
		(prMsduInfo->ucPktType == ENUM_PKT_DHCP))) {
		if (rCurrent - prPktProfile->rHardXmitArrivalTimestamp > 2000) {
			DBGLOG(TX, INFO,
				"valid %d; ArriveDrv %u, Enq %u, Deq %u, LeaveDrv %u, TxDone %u\n",
				prPktProfile->fgIsValid,
				prPktProfile->rHardXmitArrivalTimestamp,
				prPktProfile->rEnqueueTimestamp,
				prPktProfile->rDequeueTimestamp,
				prPktProfile->rHifTxDoneTimestamp, rCurrent);

			if (prMsduInfo->ucPktType == ENUM_PKT_ARP)
				prAdapter->prGlueInfo->fgTxDoneDelayIsARP =
									TRUE;
			prAdapter->prGlueInfo->u4ArriveDrvTick =
				prPktProfile->rHardXmitArrivalTimestamp;
			prAdapter->prGlueInfo->u4EnQueTick =
				prPktProfile->rEnqueueTimestamp;
			prAdapter->prGlueInfo->u4DeQueTick =
				prPktProfile->rDequeueTimestamp;
			prAdapter->prGlueInfo->u4LeaveDrvTick =
				prPktProfile->rHifTxDoneTimestamp;
			prAdapter->prGlueInfo->u4CurrTick = rCurrent;
			prAdapter->prGlueInfo->u8CurrTime = kalGetTimeTickNs();
		}
	}

#if CFG_SUPPORT_MLR
	if (MLR_CHECK_IF_ENABLE_DEBUG(prAdapter))
		DBGLOG(TX, INFO,
			"TX DONE, Type[%s] Tag[0x%08x] WIDX:PID[%u:%u] SN[%d] Status[%u], SeqNo: %d\n",
			TXS_PACKET_TYPE[prMsduInfo->ucPktType],
			prMsduInfo->u4TxDoneTag,
			prMsduInfo->ucWlanIndex,
			prMsduInfo->ucPID,
			prTxDone ? prTxDone->u2SequenceNumber : -1,
			rTxDoneStatus,
			prMsduInfo->ucTxSeqNum);
	else
#endif
		DBGLOG_LIMITED(TX, INFO,
			"TX DONE, Type[%s] Tag[0x%08x] WIDX:PID[%u:%u] SN[%d] Status[%u], SeqNo: %d\n",
			TXS_PACKET_TYPE[prMsduInfo->ucPktType],
			prMsduInfo->u4TxDoneTag,
			prMsduInfo->ucWlanIndex,
			prMsduInfo->ucPID,
			prTxDone ? prTxDone->u2SequenceNumber : -1,
			rTxDoneStatus,
			prMsduInfo->ucTxSeqNum);

	if (prMsduInfo->ucPktType == ENUM_PKT_1X)
		p2pRoleFsmNotifyEapolTxStatus(prAdapter,
				prMsduInfo->ucBssIndex,
				prMsduInfo->eEapolKeyType,
				rTxDoneStatus);

#if CFG_SUPPORT_TDLS
	if (prMsduInfo->ucPktType == ENUM_PKT_TDLS)
		TdlsHandleTxDoneStatus(prAdapter, rTxDoneStatus);
#endif /* CFG_SUPPORT_TDLS */

#if CFG_SUPPORT_TX_MGMT_USE_DATAQ
	if (prMsduInfo->ucPktType == ENUM_PKT_802_11_MGMT) {

		u4PacketLen = kalQueryPacketLength(prMsduInfo->prPacket);
		kalGetPacketBufHeadManipulate(prMsduInfo->prPacket, &pucBuf,
			u4PacketLen - prMsduInfo->u2FrameLength);
		prWlanHeader = (struct WLAN_MAC_HEADER *)pucBuf;

		if (prMsduInfo->u4Option & MSDU_OPT_PROTECTED_FRAME)
			prWlanHeader->u2FrameCtrl &=
				~MASK_FC_PROTECTED_FRAME;
		fgIsSuccess = (rTxDoneStatus == TX_RESULT_SUCCESS) ?
				TRUE : FALSE;
		kalIndicateMgmtTxStatus(prAdapter->prGlueInfo,
					prMsduInfo->u8Cookie,
					fgIsSuccess,
					pucBuf,
					(uint32_t)
					prMsduInfo->u2FrameLength,
					prMsduInfo->ucBssIndex);
	}
#endif
	return WLAN_STATUS_SUCCESS;
}
#if (CFG_CE_ASSERT_DUMP == 1)
void wlanCorDumpTimerInit(struct ADAPTER *prAdapter)
{
	cnmTimerInitTimer(prAdapter,
			  &prAdapter->rN9CorDumpTimer,
			  (PFN_MGMT_TIMEOUT_FUNC) wlanN9CorDumpTimeOut,
			  (uintptr_t) NULL);
}

void wlanCorDumpTimerReset(struct ADAPTER *prAdapter)
{

	if (prAdapter->fgN9AssertDumpOngoing) {
		cnmTimerStopTimer(prAdapter,
				  &prAdapter->rN9CorDumpTimer);
		cnmTimerStartTimer(prAdapter,
				   &prAdapter->rN9CorDumpTimer, 5000);
	} else {
		DBGLOG(INIT, INFO,
		       "Cr4, N9 CorDump Is not ongoing, ignore timer reset\n");
	}
}

void wlanN9CorDumpTimeOut(struct ADAPTER *prAdapter,
			  uintptr_t ulParamPtr)
{
	/* Trigger RESET */
	GL_DEFAULT_RESET_TRIGGER(prAdapter, RST_FW_ASSERT_TIMEOUT);
}

#endif

u_int8_t
wlanGetWlanIdxByAddress(struct ADAPTER *prAdapter,
			uint8_t *pucAddr, uint8_t *pucIndex)
{
	uint8_t ucStaRecIdx;
	struct STA_RECORD *prTempStaRec;

	for (ucStaRecIdx = 0; ucStaRecIdx < CFG_STA_REC_NUM;
	     ucStaRecIdx++) {
		prTempStaRec = &(prAdapter->arStaRec[ucStaRecIdx]);
		if (pucAddr) {
			if (prTempStaRec->fgIsInUse &&
			    EQUAL_MAC_ADDR(prTempStaRec->aucMacAddr,
			    pucAddr)) {
				*pucIndex = prTempStaRec->ucWlanIndex;
				return TRUE;
			}
		} else {
			if (prTempStaRec->fgIsInUse
			    && prTempStaRec->ucStaState == STA_STATE_3) {
				*pucIndex = prTempStaRec->ucWlanIndex;
				return TRUE;
			}
		}
	}
	return FALSE;
}


uint8_t *
wlanGetStaAddrByWlanIdx(struct ADAPTER *prAdapter,
			uint8_t ucIndex)
{
	struct WLAN_TABLE *prWtbl;

	ASSERT(prAdapter);
	prWtbl = prAdapter->rWifiVar.arWtbl;
	if (ucIndex < WTBL_SIZE) {
		if (prWtbl[ucIndex].ucUsed && prWtbl[ucIndex].ucPairwise)
			return &prWtbl[ucIndex].aucMacAddr[0];
	}
	return NULL;
}

uint32_t
wlanGetStaIdxByWlanIdx(struct ADAPTER *prAdapter,
		       uint8_t ucIndex, uint8_t *pucStaIdx)
{
	struct WLAN_TABLE *prWtbl;

	ASSERT(prAdapter);
	prWtbl = prAdapter->rWifiVar.arWtbl;

	if (ucIndex < WTBL_SIZE) {
		if (prWtbl[ucIndex].ucUsed && prWtbl[ucIndex].ucPairwise) {
			if (prWtbl[ucIndex].ucStaIndex >= CFG_STA_REC_NUM)
				return WLAN_STATUS_FAILURE;

			*pucStaIdx = prWtbl[ucIndex].ucStaIndex;
			return WLAN_STATUS_SUCCESS;
		}
	}
	return WLAN_STATUS_FAILURE;
}
/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to query LTE safe channels.
 *
 * \param[in]  pvAdapter        Pointer to the Adapter structure.
 * \param[out] pvQueryBuffer    A pointer to the buffer that holds the result of
 *                              the query.
 * \param[in]  u4QueryBufferLen The length of the query buffer.
 * \param[out] pu4QueryInfoLen  If the call is successful, returns the number of
 *                              bytes written into the query buffer. If the call
 *                              failed due to invalid length of the query
 *                              buffer, returns the amount of storage needed.
 *
 * \retval WLAN_STATUS_PENDING
 * \retval WLAN_STATUS_FAILURE
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanQueryLteSafeChannel(struct ADAPTER *prAdapter,
		uint8_t ucRoleIndex)
{
	uint32_t rResult = WLAN_STATUS_FAILURE;
	struct CMD_GET_LTE_SAFE_CHN rQuery_LTE_SAFE_CHN = {0};
	struct PARAM_GET_CHN_INFO *prQueryLteChn;

	DBGLOG(P2P, TRACE, "[ACS] Get safe LTE Channels\n");

	do {
		if (!prAdapter)
			break;

		prQueryLteChn = kalMemAlloc(sizeof(struct PARAM_GET_CHN_INFO),
				VIR_MEM_TYPE);
		if (!prQueryLteChn)
			break;

		kalMemZero(prQueryLteChn, sizeof(struct PARAM_GET_CHN_INFO));
		prQueryLteChn->ucRoleIndex = ucRoleIndex;

		/* Get LTE safe channel list */
		wlanSendSetQueryCmd(prAdapter,
			CMD_ID_GET_LTE_CHN,
			FALSE,
			TRUE,
			FALSE, /* Query ID */
			nicCmdEventQueryLteSafeChn, /* The handler to receive
						     * firmware notification
						     */
			nicOidCmdTimeoutCommon,
			sizeof(struct CMD_GET_LTE_SAFE_CHN),
			(uint8_t *)&rQuery_LTE_SAFE_CHN,
			prQueryLteChn,
			0);
		rResult = WLAN_STATUS_SUCCESS;
	} while (FALSE);

	return rResult;
}				/* wlanoidQueryLteSafeChannel */

/*----------------------------------------------------------------------------*/
/*!
 * \brief Add dirtiness to neighbor channels of a BSS to estimate channel
 *        quality.
 *
 * \param[in]  prAdapter        Pointer to the Adapter structure.
 * \param[in]  prBssDesc        Pointer to the BSS description.
 * \param[in]  u4Dirtiness      Expected dirtiness value.
 * \param[in]  ucCentralChannel Central channel of the given BSS.
 * \param[in]  ucCoveredRange   With ucCoveredRange and ucCentralChannel,
 *                              all the affected channels can be enumerated.
 */
/*----------------------------------------------------------------------------*/
static void
wlanAddDirtinessToAffectedChannels(struct ADAPTER *prAdapter,
				   struct BSS_DESC *prBssDesc,
				   uint32_t u4Dirtiness,
				   uint8_t ucCentralChannel,
				   uint8_t ucCoveredRange)
{
	uint8_t ucIdx, ucStart, ucEnd;
	u_int8_t bIsABand = FALSE;
	uint8_t ucLeftNeighborChannel, ucRightNeighborChannel,
		ucLeftNeighborChannel2 = 0, ucRightNeighborChannel2 = 0,
		ucLeftestCoveredChannel, ucRightestCoveredChannel;
	struct PARAM_GET_CHN_INFO *prGetChnLoad = &
			(prAdapter->rWifiVar.rChnLoadInfo);

	ucLeftestCoveredChannel = ucCentralChannel > ucCoveredRange
				  ?
				  ucCentralChannel - ucCoveredRange : 1;

	ucLeftNeighborChannel = ucLeftestCoveredChannel ?
				ucLeftestCoveredChannel - 1 : 0;

	if (prBssDesc->eBand == BAND_5G
#if (CFG_SUPPORT_WIFI_6G == 1)
		|| prBssDesc->eBand == BAND_6G
#endif
	) {
		bIsABand = TRUE;
	}

	/* align leftest covered ch and left neighbor ch to valid 5g ch */
	if (bIsABand) {
		ucLeftestCoveredChannel += 2;
		ucLeftNeighborChannel -= 1;
	} else {
		/* we select the nearest 2 ch to the leftest covered ch as left
		 * neighbor chs
		 */
		ucLeftNeighborChannel2 = ucLeftNeighborChannel > 1 ?
						ucLeftNeighborChannel - 1 : 0;
	}

	/* handle corner cases of 5g ch*/
	if (prBssDesc->eBand == BAND_5G &&
			ucLeftestCoveredChannel > 14 &&
			ucLeftestCoveredChannel <= 36) {
		ucLeftestCoveredChannel = 36;
		ucLeftNeighborChannel = 0;
	} else if (prBssDesc->eBand == BAND_5G &&
			ucLeftestCoveredChannel > 64 &&
			ucLeftestCoveredChannel <= 100) {
		ucLeftestCoveredChannel = 100;
		ucLeftNeighborChannel = 0;
	} else if (prBssDesc->eBand == BAND_5G &&
			ucLeftestCoveredChannel > 144 &&
			ucLeftestCoveredChannel <= 149) {
		ucLeftestCoveredChannel = 149;
		ucLeftNeighborChannel = 0;
	}
#if (CFG_SUPPORT_WIFI_6G == 1)
	else if (prBssDesc->eBand == BAND_6G &&
			ucLeftestCoveredChannel < 5) {
		ucLeftestCoveredChannel = 5;
		ucLeftNeighborChannel = 0;
	} else if (prBssDesc->eBand == BAND_6G &&
			ucLeftestCoveredChannel > 229) {
		ucLeftestCoveredChannel = 229;
		ucLeftNeighborChannel = 0;
	}
#endif

	/*
	 * because ch 14 is 12MHz away to ch13, we must shift the leftest
	 * covered ch and left neighbor ch when central ch is ch 14
	 */
	if (prBssDesc->eBand == BAND_2G4 &&
		ucCentralChannel == 14) {
		ucLeftestCoveredChannel = 13;
		ucLeftNeighborChannel = 12;
		ucLeftNeighborChannel2 = 11;
	}

	ucRightestCoveredChannel = ucCentralChannel +
				   ucCoveredRange;
	ucRightNeighborChannel = ucRightestCoveredChannel + 1;

	/* align rightest covered ch and right neighbor ch to valid 5g ch */
	if (bIsABand) {
		ucRightestCoveredChannel -= 2;
		ucRightNeighborChannel += 1;
	} else {
		/* we select the nearest 2 ch to the rightest covered ch as
		 * right neighbor ch
		 */
		ucRightNeighborChannel2 = ucRightNeighborChannel < 13 ?
						ucRightNeighborChannel + 1 : 0;
	}

	/* handle corner cases */
	if (prBssDesc->eBand == BAND_5G &&
			ucRightestCoveredChannel >= 14 &&
			ucRightestCoveredChannel < 36) {
		if (ucRightestCoveredChannel == 14) {
			ucRightestCoveredChannel = 13;
			ucRightNeighborChannel = 14;
		} else {
			ucRightestCoveredChannel = 14;
			ucRightNeighborChannel = 0;
		}

		ucRightNeighborChannel2 = 0;
	} else if (prBssDesc->eBand == BAND_5G &&
			ucRightestCoveredChannel >= 64 &&
			ucRightestCoveredChannel < 100) {
		ucRightestCoveredChannel = 64;
		ucRightNeighborChannel = 0;
	} else if (prBssDesc->eBand == BAND_5G &&
			ucRightestCoveredChannel >= 144 &&
			ucRightestCoveredChannel < 149) {
		ucRightestCoveredChannel = 144;
		ucRightNeighborChannel = 0;
	} else if (prBssDesc->eBand == BAND_5G &&
			ucRightestCoveredChannel >= 165) {
		ucRightestCoveredChannel = 165;
		ucRightNeighborChannel = 0;
	}
#if (CFG_SUPPORT_WIFI_6G == 1)
	else if (prBssDesc->eBand == BAND_6G &&
			ucRightestCoveredChannel < 5) {
		ucRightestCoveredChannel = 5;
		ucRightNeighborChannel = 0;
	} else if (prBssDesc->eBand == BAND_6G &&
			ucRightestCoveredChannel > 229) {
		ucRightestCoveredChannel = 229;
		ucRightNeighborChannel = 0;
	}
#endif

	log_dbg(SCN, TEMP, "central ch %u\n", ucCentralChannel);

	ucStart = wlanGetChannelIndex(prBssDesc->eBand,
				ucLeftestCoveredChannel);
	ucEnd = wlanGetChannelIndex(prBssDesc->eBand,
				ucRightestCoveredChannel);
	if (ucStart >= MAX_CHN_NUM || ucEnd >= MAX_CHN_NUM) {
		DBGLOG(SCN, ERROR, "Invalid ch idx of start %u, or end %u\n",
			ucStart, ucEnd);
		return;
	}

	for (ucIdx = ucStart; ucIdx <= ucEnd; ucIdx++) {
		prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness +=
			u4Dirtiness;
		log_dbg(SCN, TEMP, "Add dirtiness %d, to covered ch %d\n",
		       u4Dirtiness,
		       prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
	}

	if (ucLeftNeighborChannel != 0) {
		ucIdx = wlanGetChannelIndex(prBssDesc->eBand,
					ucLeftNeighborChannel);

		if (ucIdx < MAX_CHN_NUM) {
			prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness +=
				(u4Dirtiness >> 1);
			log_dbg(SCN, TEMP,
				"Add dirtiness %d, to neighbor ch %d\n",
				u4Dirtiness >> 1,
				prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
		}
	}

	if (ucRightNeighborChannel != 0) {
		ucIdx = wlanGetChannelIndex(prBssDesc->eBand,
					ucRightNeighborChannel);
		if (ucIdx < MAX_CHN_NUM) {
			prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness +=
				(u4Dirtiness >> 1);
			log_dbg(SCN, TEMP,
				"Add dirtiness %d, to neighbor ch %d\n",
				u4Dirtiness >> 1,
				prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
		}
	}

	if (bIsABand)
		return;

	/* Only necesaary for 2.5G */
	if (ucLeftNeighborChannel2 != 0) {
		ucIdx = wlanGetChannelIndex(prBssDesc->eBand,
					ucLeftNeighborChannel2);
		if (ucIdx < MAX_CHN_NUM) {
			prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness +=
				(u4Dirtiness >> 1);
			log_dbg(SCN, TEMP,
				"Add dirtiness %d, to neighbor ch %d\n",
				u4Dirtiness >> 1,
				prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
		}
	}

	if (ucRightNeighborChannel2 != 0) {
		ucIdx = wlanGetChannelIndex(prBssDesc->eBand,
					ucRightNeighborChannel2);
		if (ucIdx < MAX_CHN_NUM) {
			prGetChnLoad->rEachChnLoad[ucIdx].u4Dirtiness +=
				(u4Dirtiness >> 1);
			log_dbg(SCN, TEMP,
				"Add dirtiness %d, to neighbor ch %d\n",
				u4Dirtiness >> 1,
				prGetChnLoad->rEachChnLoad[ucIdx].ucChannel);
		}
	}

}

/*----------------------------------------------------------------------------*/
/*!
 * \brief For a scanned BSS, add dirtiness to the channels 1)around its primary
 *        channels and 2) in its working BW to represent the quality degrade.
 *
 * \param[in]  prAdapter        Pointer to the Adapter structure.
 * \param[in]  prBssDesc        Pointer to the BSS description.
 * \param[in]  u4Dirtiness      Expected dirtiness value.
 * \param[in]  bIsIndexOne      True means index 1, False means index 2.
 */
/*----------------------------------------------------------------------------*/
static void
wlanCalculateChannelDirtiness(struct ADAPTER *prAdapter,
			      struct BSS_DESC *prBssDesc, uint32_t u4Dirtiness,
			      u_int8_t bIsIndexOne)
{
	uint8_t ucCoveredRange = 0, ucCentralChannel = 0,
		ucCentralChannel2 = 0;

	if (bIsIndexOne) {
		DBGLOG(SCN, TEMP, "Process dirtiness index 1\n");
		ucCentralChannel = prBssDesc->ucChannelNum;
		ucCoveredRange = 2;
	} else {
		DBGLOG(SCN, TEMP, "Process dirtiness index 2, ");
		switch (prBssDesc->eChannelWidth) {
		case CW_20_40MHZ:
			if (prBssDesc->eSco == CHNL_EXT_SCA) {
				DBGLOG(SCN, TEMP, "BW40\n");
				ucCentralChannel = prBssDesc->ucChannelNum + 2;
				ucCoveredRange = 4;
			} else if (prBssDesc->eSco == CHNL_EXT_SCB) {
				DBGLOG(SCN, TEMP, "BW40\n");
				ucCentralChannel = prBssDesc->ucChannelNum - 2;
				ucCoveredRange = 4;
			} else {
				DBGLOG(SCN, TEMP, "BW20\n");
				ucCentralChannel = prBssDesc->ucChannelNum;
				ucCoveredRange = 2;
			}
			break;
		case CW_80MHZ:
			DBGLOG(SCN, TEMP, "BW80\n");
			ucCentralChannel = prBssDesc->ucCenterFreqS1;
			ucCoveredRange = 8;
			break;
		case CW_160MHZ:
			DBGLOG(SCN, TEMP, "BW160\n");
			ucCentralChannel = prBssDesc->ucCenterFreqS1;
			ucCoveredRange = 16;
			break;
		case CW_80P80MHZ:
			DBGLOG(SCN, TEMP, "BW8080\n");
			ucCentralChannel = prBssDesc->ucCenterFreqS1;
			ucCentralChannel2 = prBssDesc->ucCenterFreqS2;
			ucCoveredRange = 8;
			break;
		default:
			ucCentralChannel = prBssDesc->ucChannelNum;
			ucCoveredRange = 2;
			break;
		};
	}

	wlanAddDirtinessToAffectedChannels(prAdapter, prBssDesc,
					   u4Dirtiness,
					   ucCentralChannel, ucCoveredRange);

	/* 80 + 80 secondary 80 case */
	if (bIsIndexOne || ucCentralChannel2 == 0)
		return;

	wlanAddDirtinessToAffectedChannels(prAdapter, prBssDesc,
					   u4Dirtiness,
					   ucCentralChannel2, ucCoveredRange);
}

void
wlanInitChnLoadInfoChannelList(struct ADAPTER *prAdapter)
{
	uint8_t ucIdx = 0;
	struct PARAM_GET_CHN_INFO *prGetChnLoad = &
			(prAdapter->rWifiVar.rChnLoadInfo);

	for (ucIdx = 0; ucIdx < MAX_CHN_NUM; ucIdx++) {
		prGetChnLoad->rEachChnLoad[ucIdx].eBand =
			wlanGetChannelBandFromIndex(ucIdx);
		prGetChnLoad->rEachChnLoad[ucIdx].ucChannel =
			wlanGetChannelNumFromIndex(ucIdx);
	}
}

uint32_t
wlanCalculateAllChannelDirtiness(struct ADAPTER
				 *prAdapter)
{
	uint32_t rResult = WLAN_STATUS_SUCCESS;
	int32_t i4Rssi = 0;
	struct BSS_DESC *prBssDesc = NULL;
	uint32_t u4Dirtiness = 0;
	struct LINK *prBSSDescList =
				&(prAdapter->rWifiVar.rScanInfo.rBSSDescList);

	LINK_FOR_EACH_ENTRY(prBssDesc, prBSSDescList, rLinkEntry,
			    struct BSS_DESC) {
		i4Rssi = RCPI_TO_dBm(prBssDesc->ucRCPI);

		if (i4Rssi >= ACS_AP_RSSI_LEVEL_HIGH)
			u4Dirtiness = ACS_DIRTINESS_LEVEL_HIGH;
		else if (i4Rssi >= ACS_AP_RSSI_LEVEL_LOW)
			u4Dirtiness = ACS_DIRTINESS_LEVEL_MID;
		else
			u4Dirtiness = ACS_DIRTINESS_LEVEL_LOW;

		DBGLOG(SCN, TEMP, "Found an AP(%s), primary ch %d\n",
		       prBssDesc->aucSSID, prBssDesc->ucChannelNum);

		/* dirtiness index1 */
		wlanCalculateChannelDirtiness(prAdapter, prBssDesc,
					      u4Dirtiness, TRUE);

		/* dirtiness index2 */
		wlanCalculateChannelDirtiness(prAdapter, prBssDesc,
					      u4Dirtiness >> 1, FALSE);
	}

	return rResult;
}

uint8_t
wlanGetChannelIndex(enum ENUM_BAND eBand, uint8_t channel)
{
	uint8_t ucIdx = MAX_CHN_NUM - 1;

	if (eBand == BAND_2G4)
		ucIdx = channel - 1;
	else if (eBand == BAND_5G && channel >= 36 && channel <= 64)
		ucIdx = 14 + (channel - 36) / 4;
	else if (eBand == BAND_5G && channel >= 100 && channel <= 144)
		ucIdx = 14 + 8 + (channel - 100) / 4;
	else if (eBand == BAND_5G && channel >= 149 && channel <= 165)
		ucIdx = 14 + 8 + 12 + (channel - 149) / 4;
#if (CFG_SUPPORT_WIFI_6G == 1)
	else if (eBand == BAND_6G && channel >= 1 && channel <= 233)
		ucIdx = 14 + 8 + 12 + 5 + (channel - 1) / 4;
#endif

	return ucIdx;
}

/*---------------------------------------------------------------------*/
/*!
 * \brief Get ch index by the given ch num; the reverse function of
 *        wlanGetChannelIndex
 *
 * \param[in]    ucIdx                 Channel index
 * \param[out]   ucChannel             Channel number
 */
/*---------------------------------------------------------------------*/

uint8_t
wlanGetChannelNumFromIndex(uint8_t ucIdx)
{
	uint8_t ucChannel = 0;

#if (CFG_SUPPORT_WIFI_6G == 1)
	if (ucIdx >= 39)
		ucChannel = ((ucIdx - 39) << 2) + 1;
	else
#endif
	if (ucIdx >= 34)
		ucChannel = ((ucIdx - 34) << 2) + 149;
	else if (ucIdx >= 22)
		ucChannel = ((ucIdx - 22) << 2) + 100;
	else if (ucIdx >= 14)
		ucChannel = ((ucIdx - 14) << 2) + 36;
	else
		ucChannel = ucIdx + 1;

	return ucChannel;
}

enum ENUM_BAND
wlanGetChannelBandFromIndex(uint8_t ucIdx)
{
	enum ENUM_BAND eBand = BAND_NULL;

#if (CFG_SUPPORT_WIFI_6G == 1)
	if (ucIdx >= 39)
		eBand = BAND_6G;
	else
#endif
	if (ucIdx >= 14)
		eBand = BAND_5G;
	else
		eBand = BAND_2G4;

	return eBand;
}

void
wlanSortChannel(struct ADAPTER *prAdapter,
		enum ENUM_CHNL_SORT_POLICY ucSortType)
{
	struct PARAM_GET_CHN_INFO *prChnLoadInfo = &
			(prAdapter->rWifiVar.rChnLoadInfo);
	int8_t ucIdx = 0, ucRoot = 0, ucChild = 0;
#if (CFG_SUPPORT_P2PGO_ACS == 1)
	uint8_t i = 0, ucBandIdx = 0, ucNumOfChannel = 0, uc2gChNum = 0;
	struct RF_CHANNEL_INFO aucChannelList[MAX_PER_BAND_CHN_NUM] = { { 0 } };
#endif
	struct PARAM_CHN_RANK_INFO rChnRankInfo;
	/* prepare unsorted ch rank list */
#if (CFG_SUPPORT_P2PGO_ACS == 1)
	if (ucSortType == CHNL_SORT_POLICY_BY_CH_DOMAIN) {
		for (ucBandIdx = BAND_2G4; ucBandIdx < BAND_NUM; ucBandIdx++) {
			rlmDomainGetChnlList(prAdapter, ucBandIdx,
				TRUE, MAX_PER_BAND_CHN_NUM,
				&ucNumOfChannel, aucChannelList);

			DBGLOG(SCN, LOUD, "[ACS]Band=%d, Channel Number=%d\n",
				ucBandIdx,
				ucNumOfChannel);

			for (i = 0; i < ucNumOfChannel; i++) {
				ucIdx = wlanGetChannelIndex(
					aucChannelList[i].eBand,
					aucChannelList[i].ucChannelNum);
				prChnLoadInfo->
					rChnRankList[uc2gChNum+i].eBand =
					prChnLoadInfo->rEachChnLoad[ucIdx].
						eBand;
				prChnLoadInfo->
					rChnRankList[uc2gChNum+i].ucChannel =
					prChnLoadInfo->rEachChnLoad[ucIdx].
						ucChannel;
				prChnLoadInfo->
					rChnRankList[uc2gChNum+i].u4Dirtiness =
					prChnLoadInfo->rEachChnLoad[ucIdx].
						u4Dirtiness;

				DBGLOG(SCN, LOUD, "[ACS]Ch[%d],cIdx[%d]\n",
					aucChannelList[i].ucChannelNum,
					uc2gChNum+i);
				DBGLOG(SCN, LOUD, "[ACS]ChR[%d],eCh[%d]\n",
					prChnLoadInfo->
						rChnRankList[uc2gChNum+i].
						ucChannel,
					prChnLoadInfo->rEachChnLoad[ucIdx].
						ucChannel);
			}
			uc2gChNum = uc2gChNum + ucNumOfChannel;
		}

		/*Set the reset idx to invalid value*/
		for (i = uc2gChNum; i < MAX_CHN_NUM; i++) {
			prChnLoadInfo->rChnRankList[i].u4Dirtiness = 0xFFFFFFFF;
			prChnLoadInfo->rChnRankList[i].ucChannel = 0xFF;

			DBGLOG(SCN, LOUD, "uc2gChNum=%d,[ACS]Chn=%d,D=0x%x\n",
				i,
				prChnLoadInfo->rChnRankList[i].ucChannel,
				prChnLoadInfo->rChnRankList[i].u4Dirtiness);
		}
	} else
#endif
	{
		for (ucIdx = 0; ucIdx < MAX_CHN_NUM; ++ucIdx) {
			prChnLoadInfo->rChnRankList[ucIdx].eBand =
				prChnLoadInfo->rEachChnLoad[ucIdx].eBand;
			prChnLoadInfo->rChnRankList[ucIdx].ucChannel =
				prChnLoadInfo->rEachChnLoad[ucIdx].ucChannel;
			prChnLoadInfo->rChnRankList[ucIdx].u4Dirtiness =
				prChnLoadInfo->rEachChnLoad[ucIdx].u4Dirtiness;

		}
	}
	/* heapify ch rank list */
	for (ucIdx = MAX_CHN_NUM / 2 - 1; ucIdx >= 0; --ucIdx) {
		for (ucRoot = ucIdx; ucRoot * 2 + 1 < MAX_CHN_NUM;
				ucRoot = ucChild) {
			ucChild = ucRoot * 2 + 1;
			/* Coverity check*/
			if (ucChild < 0 || ucChild >= MAX_CHN_NUM ||
			    ucRoot < 0 || ucRoot >= MAX_CHN_NUM)
				break;
			if (ucChild < MAX_CHN_NUM - 1 && prChnLoadInfo->
			    rChnRankList[ucChild + 1].u4Dirtiness >
			    prChnLoadInfo->rChnRankList[ucChild].u4Dirtiness)
				ucChild += 1;
			if (prChnLoadInfo->rChnRankList[ucChild].u4Dirtiness <=
			    prChnLoadInfo->rChnRankList[ucRoot].u4Dirtiness)
				break;
			DBGLOG(SCN, LOUD, "[ACS]root Chn=%d,D=0x%x\n",
					   prChnLoadInfo->
					   rChnRankList[ucRoot].ucChannel,
					   prChnLoadInfo->
					   rChnRankList[ucRoot].u4Dirtiness);
			DBGLOG(SCN, LOUD, "[ACS]child Chn=%d,D=0x%x\n",
					   prChnLoadInfo->
					   rChnRankList[ucChild].ucChannel,
					   prChnLoadInfo->
					   rChnRankList[ucChild].u4Dirtiness);
			kalMemCopy(&rChnRankInfo,
				&(prChnLoadInfo->rChnRankList[ucChild]),
				sizeof(struct PARAM_CHN_RANK_INFO));
			kalMemCopy(&prChnLoadInfo->rChnRankList[ucChild],
				&prChnLoadInfo->rChnRankList[ucRoot],
				sizeof(struct PARAM_CHN_RANK_INFO));
			kalMemCopy(&prChnLoadInfo->rChnRankList[ucRoot],
				&rChnRankInfo,
				sizeof(struct PARAM_CHN_RANK_INFO));
			DBGLOG(SCN, LOUD,
				"[ACS]After root uChn=%d,D=0x%x\n",
					   prChnLoadInfo->
					   rChnRankList[ucRoot].ucChannel,
					   prChnLoadInfo->
					   rChnRankList[ucRoot].u4Dirtiness);
			DBGLOG(SCN, LOUD,
				"[ACS]AFter child Chn=%d,D=0x%x\n",
					   prChnLoadInfo->
					   rChnRankList[ucChild].ucChannel,
					   prChnLoadInfo->
					   rChnRankList[ucChild].u4Dirtiness);
			}
	}
	/* sort ch rank list */
	for (ucIdx = MAX_CHN_NUM - 1; ucIdx > 0; ucIdx--) {
		rChnRankInfo = prChnLoadInfo->rChnRankList[0];
		prChnLoadInfo->rChnRankList[0] =
			prChnLoadInfo->rChnRankList[ucIdx];
		prChnLoadInfo->rChnRankList[ucIdx] = rChnRankInfo;
		for (ucRoot = 0; ucRoot * 2 + 1 < ucIdx; ucRoot = ucChild) {
			ucChild = ucRoot * 2 + 1;
			/* Coverity check*/
			if (ucChild < 0 || ucChild >= MAX_CHN_NUM ||
			    ucRoot < 0 || ucRoot >= MAX_CHN_NUM)
				break;
			if (ucChild < ucIdx - 1 && prChnLoadInfo->
			    rChnRankList[ucChild + 1].u4Dirtiness >
			    prChnLoadInfo->rChnRankList[ucChild].u4Dirtiness)
				ucChild += 1;
			if (prChnLoadInfo->rChnRankList[ucChild].u4Dirtiness <=
			    prChnLoadInfo->rChnRankList[ucRoot].u4Dirtiness)
				break;
			DBGLOG(SCN, LOUD,
				"[ACS]root ChNum=%d D=0x%x",
					   prChnLoadInfo->
					   rChnRankList[ucRoot].ucChannel,
					   prChnLoadInfo->
					   rChnRankList[ucRoot].u4Dirtiness);
			DBGLOG(SCN, LOUD, "[ACS]child ChNum=%d D=0x%x\n",
					   prChnLoadInfo->
					   rChnRankList[ucChild].ucChannel,
					   prChnLoadInfo->
					   rChnRankList[ucChild].u4Dirtiness);
			kalMemCopy(&rChnRankInfo,
				&(prChnLoadInfo->rChnRankList[ucChild]),
				sizeof(struct PARAM_CHN_RANK_INFO));
			kalMemCopy(&prChnLoadInfo->rChnRankList[ucChild],
				&prChnLoadInfo->rChnRankList[ucRoot],
				sizeof(struct PARAM_CHN_RANK_INFO));
			kalMemCopy(&prChnLoadInfo->rChnRankList[ucRoot],
				&rChnRankInfo,
				sizeof(struct PARAM_CHN_RANK_INFO));
			DBGLOG(SCN, LOUD,
				"[ACS]New root ChNum=%d D=0x%x",
					   prChnLoadInfo->
					   rChnRankList[ucRoot].ucChannel,
					   prChnLoadInfo->
					   rChnRankList[ucRoot].u4Dirtiness);

			DBGLOG(SCN, LOUD,
				"[ACS]New child ChNum=%d D=0x%x",
					   prChnLoadInfo->
					   rChnRankList[ucChild].ucChannel,
					   prChnLoadInfo->
					   rChnRankList[ucChild].u4Dirtiness);
		}
	}

	for (ucIdx = 0; ucIdx < MAX_CHN_NUM; ++ucIdx) {
		DBGLOG(SCN, LOUD, "[ACS]band=%d,channel=%d,dirtiness=0x%x\n",
			prChnLoadInfo->rChnRankList[ucIdx].eBand,
			prChnLoadInfo->rChnRankList[ucIdx].ucChannel,
			prChnLoadInfo->rChnRankList[ucIdx].u4Dirtiness);
	}

}

#if ((CFG_SISO_SW_DEVELOP == 1) || (CFG_SUPPORT_SPE_IDX_CONTROL == 1))
uint8_t
wlanGetAntPathType(struct ADAPTER *prAdapter,
		   enum ENUM_WF_PATH_FAVOR_T eWfPathFavor)
{
	uint8_t ucFianlWfPathType = eWfPathFavor;
#if (CFG_SUPPORT_SPE_IDX_CONTROL == 1)
	uint8_t ucNss = prAdapter->rWifiVar.ucNSS;
	uint8_t ucSpeIdxCtrl = prAdapter->rWifiVar.ucSpeIdxCtrl;

	if (ucNss <= 2) {
		if (ucSpeIdxCtrl == 0)
			ucFianlWfPathType = ENUM_WF_0_ONE_STREAM_PATH_FAVOR;
		else if (ucSpeIdxCtrl == 1)
			ucFianlWfPathType = ENUM_WF_1_ONE_STREAM_PATH_FAVOR;
		else if (ucSpeIdxCtrl == 2) {
			if (ucNss > 1)
				ucFianlWfPathType =
					ENUM_WF_0_1_DUP_STREAM_PATH_FAVOR;
			else
				ucFianlWfPathType = ENUM_WF_NON_FAVOR;
		} else
			ucFianlWfPathType = ENUM_WF_NON_FAVOR;
	}
#endif
	return ucFianlWfPathType;
}

uint8_t
wlanAntPathFavorSelect(struct ADAPTER *prAdapter,
		       enum ENUM_WF_PATH_FAVOR_T eWfPathFavor)
{
	uint8_t ucRetValSpeIdx = 0x18;
#if (CFG_SUPPORT_SPE_IDX_CONTROL == 1)
	uint8_t ucNss = prAdapter->rWifiVar.ucNSS;

	if (ucNss <= 2) {
		if ((eWfPathFavor == ENUM_WF_NON_FAVOR) ||
			(eWfPathFavor == ENUM_WF_0_ONE_STREAM_PATH_FAVOR) ||
			(eWfPathFavor == ENUM_WF_0_1_TWO_STREAM_PATH_FAVOR))
			ucRetValSpeIdx = ANTENNA_WF0;
		else if (eWfPathFavor == ENUM_WF_0_1_DUP_STREAM_PATH_FAVOR)
			ucRetValSpeIdx = 0x18;
		else if (eWfPathFavor == ENUM_WF_1_ONE_STREAM_PATH_FAVOR)
			ucRetValSpeIdx = ANTENNA_WF1;
		else
			ucRetValSpeIdx = ANTENNA_WF0;
	}
#endif
	return ucRetValSpeIdx;
}
#endif

uint8_t
wlanGetSpeIdx(struct ADAPTER *prAdapter,
	      uint8_t ucBssIndex,
	      enum ENUM_WF_PATH_FAVOR_T eWfPathFavor)
{
	uint8_t ucRetValSpeIdx = 0;
#if ((CFG_SISO_SW_DEVELOP == 1) || (CFG_SUPPORT_SPE_IDX_CONTROL == 1))
	struct BSS_INFO *prBssInfo;
	enum ENUM_BAND eBand = BAND_NULL;

	if (ucBssIndex > prAdapter->ucHwBssIdNum) {
		DBGLOG(SW4, INFO, "Invalid BssInfo index[%u], skip dump!\n",
		       ucBssIndex);
		return ucRetValSpeIdx;
	}
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (!prBssInfo) {
		DBGLOG(SW4, ERROR,
			"Invalid bss idx: %d\n",
			ucBssIndex);
		return ucRetValSpeIdx;
	}
	/*
	 * if DBDC enable return 0, else depend 2.4G/5G & support WF path
	 * retrun accurate value
	 */
	if (prAdapter->rWifiVar.eDbdcMode == ENUM_DBDC_MODE_STATIC ||
	    !prAdapter->rWifiVar.fgDbDcModeEn) {
		if (prBssInfo->fgIsGranted)
			eBand = prBssInfo->eBandGranted;
		else
			eBand = prBssInfo->eBand;

		if (eBand == BAND_2G4) {
			if (IS_WIFI_2G4_SISO(prAdapter)) {
				if (IS_WIFI_2G4_WF0_SUPPORT(prAdapter))
					ucRetValSpeIdx = ANTENNA_WF0;
				else
					ucRetValSpeIdx = ANTENNA_WF1;
			} else {
				if (IS_WIFI_SMART_GEAR_SUPPORT_WF0_SISO(
				    prAdapter))
					ucRetValSpeIdx = ANTENNA_WF0;
				else if (IS_WIFI_SMART_GEAR_SUPPORT_WF1_SISO(
				    prAdapter))
					ucRetValSpeIdx = ANTENNA_WF1;
				else
					ucRetValSpeIdx = wlanAntPathFavorSelect(
						prAdapter, eWfPathFavor);
			}
		} else if (eBand == BAND_5G) {
			if (IS_WIFI_5G_SISO(prAdapter)) {
				if (IS_WIFI_5G_WF0_SUPPORT(prAdapter))
					ucRetValSpeIdx = ANTENNA_WF0;
				else
					ucRetValSpeIdx = ANTENNA_WF1;
			} else {
				if (IS_WIFI_SMART_GEAR_SUPPORT_WF0_SISO(
				    prAdapter))
					ucRetValSpeIdx = ANTENNA_WF0;
				else if (IS_WIFI_SMART_GEAR_SUPPORT_WF1_SISO(
				    prAdapter))
					ucRetValSpeIdx = ANTENNA_WF1;
				else
					ucRetValSpeIdx = wlanAntPathFavorSelect(
						prAdapter, eWfPathFavor);
			}
#if (CFG_SUPPORT_WIFI_6G == 1)
		} else if (eBand == BAND_6G) {
			if (IS_WIFI_6G_SISO(prAdapter)) {
				if (IS_WIFI_6G_WF0_SUPPORT(prAdapter))
					ucRetValSpeIdx = ANTENNA_WF0;
				else
					ucRetValSpeIdx = ANTENNA_WF1;
			} else {
				if (IS_WIFI_SMART_GEAR_SUPPORT_WF0_SISO(
				    prAdapter))
					ucRetValSpeIdx = ANTENNA_WF0;
				else if (IS_WIFI_SMART_GEAR_SUPPORT_WF1_SISO(
				    prAdapter))
					ucRetValSpeIdx = ANTENNA_WF1;
				else
					ucRetValSpeIdx = wlanAntPathFavorSelect(
						prAdapter, eWfPathFavor);
			}
#endif
		} else
			ucRetValSpeIdx = wlanAntPathFavorSelect(prAdapter,
				eWfPathFavor);
	}
	DBGLOG(INIT, TRACE,
		"SpeIdx:%d,D:%d,G=%d,RfBand=%d,Bss=%d,HwBand=%d,BkHwBand=%d\n",
		ucRetValSpeIdx, prAdapter->rWifiVar.fgDbDcModeEn,
		prBssInfo->fgIsGranted, eBand, ucBssIndex,
		prBssInfo->eHwBandIdx, prBssInfo->eBackupHwBandIdx);
#endif
	return ucRetValSpeIdx;
}

uint8_t
wlanGetSupportNss(struct ADAPTER *prAdapter,
		  uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;
#if CFG_SUPPORT_IOT_AP_BLACKLIST
	struct BSS_DESC *prBssDesc;
#endif

	uint8_t ucRetValNss = prAdapter->rWifiVar.ucNSS;
#if CFG_SISO_SW_DEVELOP
	enum ENUM_BAND eBand = BAND_NULL;
#endif
#if (CFG_SUPPORT_POWER_THROTTLING == 1 && CFG_SUPPORT_CNM_POWER_CTRL == 1)
	if (prAdapter->fgPowerForceOneNss) {
		DBGLOG(INIT, TRACE, "Force 1 Nss\n",
		       ucBssIndex);
		return 1;
	}
#endif

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (!prBssInfo) {
		DBGLOG(CNM, ERROR,
			"Invalid bss idx: %d\n",
			ucBssIndex);
		return 1;
	}

	if (IS_BSS_APGO(prBssInfo)) {
		if (p2pFuncIsAPMode(
			prAdapter->rWifiVar.prP2PConnSettings
			[prBssInfo->u4PrivateData])) {
			if (prBssInfo->eBand == BAND_2G4)
				ucRetValNss = prAdapter->rWifiVar.ucAp2gNSS;
			else if (prBssInfo->eBand == BAND_5G)
				ucRetValNss = prAdapter->rWifiVar.ucAp5gNSS;
#if (CFG_SUPPORT_WIFI_6G == 1)
			else if (prBssInfo->eBand == BAND_6G)
				ucRetValNss = prAdapter->rWifiVar.ucAp6gNSS;
#endif
		} else {
			if (prBssInfo->eBand == BAND_2G4)
				ucRetValNss = prAdapter->rWifiVar.ucGo2gNSS;
			else if (prBssInfo->eBand == BAND_5G)
				ucRetValNss = prAdapter->rWifiVar.ucGo5gNSS;
#if (CFG_SUPPORT_WIFI_6G == 1)
			else if (prBssInfo->eBand == BAND_6G)
				ucRetValNss = prAdapter->rWifiVar.ucGo6gNSS;
#endif
		}
	}
#if CFG_SUPPORT_IOT_AP_BLACKLIST
	else if (IS_BSS_AIS(prBssInfo)) {
		struct AIS_FSM_INFO *prAisFsmInfo =
			aisGetAisFsmInfo(prAdapter, ucBssIndex);

		if (prAisFsmInfo) {
			prBssDesc = aisGetTargetBssDesc(prAdapter, ucBssIndex);
			if (prBssDesc != NULL &&
			    bssGetIotApAction(prAdapter, prBssDesc) ==
					WLAN_IOT_AP_DBDC_1SS) {
				DBGLOG(SW4, INFO, "Use 1x1 due to DBDC blk\n");
				ucRetValNss = 1;
			} else if (prAdapter->rWifiVar.fgSta1NSS) {
				DBGLOG(SW4, INFO, "Use 1x1 due to FWK cmd\n");
				ucRetValNss = 1;
			}
		}
	}
#endif

#if (CFG_SUPPORT_DBDC_DOWNGRADE_NSS == 1)
	if (IS_BSS_P2P(prBssInfo) &&
		(p2pFuncIsDualAPMode(prAdapter) ||
		p2pFuncIsDualGOMode(prAdapter))) {
		DBGLOG(SW4, LOUD, "Use 1x1 due to dual ap/go mode\n");
		ucRetValNss = 1;
	}
#endif

	if (ucRetValNss > prAdapter->rWifiVar.ucNSS)
		ucRetValNss = prAdapter->rWifiVar.ucNSS;

#if CFG_SISO_SW_DEVELOP
	if (ucBssIndex > prAdapter->ucHwBssIdNum) {
		DBGLOG(SW4, INFO, "Invalid BssInfo index[%u], skip dump!\n",
		       ucBssIndex);
		return ucRetValNss;
	}
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	/*
	 * depend 2.4G/5G support SISO/MIMO
	 * retrun accurate value
	 */
	if (prBssInfo->fgIsGranted)
		eBand = prBssInfo->eBandGranted;
	else
		eBand = prBssInfo->eBand;

	if ((eBand == BAND_2G4) && IS_WIFI_2G4_SISO(prAdapter))
		ucRetValNss = 1;
	else if ((eBand == BAND_5G) && IS_WIFI_5G_SISO(prAdapter))
		ucRetValNss = 1;
#if (CFG_SUPPORT_WIFI_6G == 1)
	else if ((eBand == BAND_6G) && IS_WIFI_6G_SISO(prAdapter))
		ucRetValNss = 1;
#endif
	DBGLOG(INIT, TRACE, "Nss=%d,G=%d,B=%d,Bss=%d\n",
	       ucRetValNss, prBssInfo->fgIsGranted, eBand, ucBssIndex);
#endif

	return ucRetValNss;
}

#if CFG_SUPPORT_LOWLATENCY_MODE
/*----------------------------------------------------------------------------*/
/*!
 * \brief This is a private routine, which is used to initialize the variables
 *        for low latency mode.
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \retval WLAN_STATUS_SUCCESS: Success
 * \retval WLAN_STATUS_FAILURE: Failed
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanAdapterStartForLowLatency(struct ADAPTER *prAdapter)
{
	uint32_t u4Status = WLAN_STATUS_SUCCESS;

	/* Default disable low latency mode */
	prAdapter->fgEnLowLatencyMode = FALSE;

	/* Default enable scan */
	prAdapter->fgEnCfg80211Scan = TRUE;

	/* Default disable duplicate detect */
	prAdapter->fgEnTxDupDetect = FALSE;
	prAdapter->fgTxDupCertificate = FALSE;

	return u4Status;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This is a private routine, which is used to initialize the variables
 *        for low latency mode.
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \retval WLAN_STATUS_SUCCESS: Success
 * \retval WLAN_STATUS_FAILURE: Failed
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanProbeSuccessForLowLatency(struct ADAPTER *prAdapter)
{
	DBGLOG(INIT, INFO, "LowLatency(ProbeOn)\n");

	/* Reset certificate flag and query capability from firmware */
	prAdapter->fgTxDupCertificate = FALSE;

	wlanSetLowLatencyCommand(prAdapter,
			FALSE,
			prAdapter->fgEnTxDupDetect,
			TRUE);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This is a private routine, which is used to initialize the variables
 *        for low latency mode.
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 *
 * \retval WLAN_STATUS_SUCCESS: Success
 * \retval WLAN_STATUS_FAILURE: Failed
 */
/*----------------------------------------------------------------------------*/
uint32_t
wlanConnectedForLowLatency(struct ADAPTER *prAdapter, uint8_t ucBssIndex)
{
	uint32_t u4Events = 0;

	/* Query setting from wifi adaptor module */
#if CFG_MTK_ANDROID_WMT
	u4Events = get_low_latency_mode();
#endif

	/* Set low latency mode */
	DBGLOG(AIS, INFO, "LowLatency(Connected) event:0x%x\n", u4Events);
	wlanSetLowLatencyMode(prAdapter, u4Events, ucBssIndex);

	return WLAN_STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set enable/disable low latency mode to FW
 *
 * \param[in]  prAdapter       A pointer to the Adapter structure.
 *
 * \retval WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanSetLowLatencyCommand(
	struct ADAPTER *prAdapter,
	u_int8_t fgEnLowLatencyMode,
	u_int8_t fgEnTxDupDetect,
	u_int8_t fgTxDupCertQuery)
{
	struct CMD_LOW_LATENCY_MODE_HEADER rModeHeader = {0};

	rModeHeader.ucVersion = LOW_LATENCY_MODE_CMD_V2;
	rModeHeader.ucType = 0;
	rModeHeader.ucMagicCode = LOW_LATENCY_MODE_MAGIC_CODE;
	rModeHeader.ucBufferLen = sizeof(struct LOW_LATENCY_MODE_SETTING);
	rModeHeader.rSetting.fgEnable = fgEnLowLatencyMode;
	rModeHeader.rSetting.fgTxDupDetect = fgEnTxDupDetect;
#if CONFIG_MTK_MD_SUPPORT
	rModeHeader.rSetting.fgTxDupCertQuery = fgTxDupCertQuery;
#else
	rModeHeader.rSetting.fgTxDupCertQuery = FALSE;
#endif
	DBGLOG(AIS, INFO,
		"Send Dpp Cmd to FW:en=%d, dup_Det=%d, CertQuery=%d\n",
		fgEnLowLatencyMode, fgEnTxDupDetect, fgTxDupCertQuery);

	wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
			    CMD_ID_SET_LOW_LATENCY_MODE,	/* ucCID */
			    TRUE,	/* fgSetQuery */
			    FALSE,	/* fgNeedResp */
			    FALSE,	/* fgIsOid */
			    NULL,	/* pfCmdDoneHandler */
			    NULL,	/* pfCmdTimeoutHandler */
			    sizeof(struct CMD_LOW_LATENCY_MODE_HEADER),
			    (uint8_t *)&rModeHeader,	/* pucInfoBuffer */
			    NULL,	/* pvSetQueryBuffer */
			    0	/* u4SetQueryBufferLen */
		);

	return WLAN_STATUS_SUCCESS;
}


/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to enable/disable low latency mode
 * \param[in]  prAdapter       A pointer to the Adapter structure.
 * \param[in]  pvSetBuffer     A pointer to the buffer that holds the
 *                             OID-specific data to be set.
 * \param[in]  u4SetBufferLen  The number of bytes the set buffer.
 * \param[out] pu4SetInfoLen   Points to the number of bytes it read or is
 *                             needed
 * \retval WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanSetLowLatencyMode(
	struct ADAPTER *prAdapter,
	uint32_t u4Events, uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo;
	u_int8_t fgEnMode = FALSE; /* Low Latency Mode */
	u_int8_t fgEnScan = FALSE; /* Scan management */
	u_int8_t fgEnPM = TRUE; /* Power management */
	u_int8_t fgEnTxDupDetect = FALSE; /* Tx Dup Detect */
	u_int8_t fgEnRoaming = TRUE; /* Roaming management */
	struct PARAM_POWER_MODE_ rPowerMode;
	struct WIFI_VAR *prWifiVar = NULL;
	char arCmd[64]; /* Roaming command buffer */

	DEBUGFUNC("wlanSetLowLatencyMode");

	ASSERT(prAdapter);

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (!prBssInfo) {
		DBGLOG(SW4, INFO, "Invalid BssInfo index[%u]\n", ucBssIndex);
		return WLAN_STATUS_INVALID_DATA;
	}

	/* Initialize */
	prWifiVar = &prAdapter->rWifiVar;

#define LOW_LATENCY_LOG_TEMPLATE \
	"LowLatency(gaming/DPP) event - gas:0x%x, net:0x%x, " \
	"whitelist:0x%x, txdup:0x%x, disable roaming:0x%x, CAM:0x%x, " \
	"scan=%u, reorder=%u, power=%u, cmd data=%u\n"

	DBGLOG(OID, INFO, LOW_LATENCY_LOG_TEMPLATE,
		(u4Events & GED_EVENT_GAS),
		(u4Events & GED_EVENT_NETWORK),
		(u4Events & GED_EVENT_DOPT_WIFI_SCAN),
		(u4Events & GED_EVENT_TX_DUP_DETECT),
		(u4Events & GED_EVENT_DISABLE_ROAMING),
		(u4Events & GED_EVENT_CAM_MODE),
		(uint32_t)prWifiVar->ucLowLatencyModeScan,
		(uint32_t)prWifiVar->ucLowLatencyModeReOrder,
		(uint32_t)prWifiVar->ucLowLatencyModePower,
		(uint32_t)prWifiVar->ucLowLatencyCmdData);

	rPowerMode.ucBssIdx = ucBssIndex;

	/* Enable/disable low latency mode decision:
	 *
	 * Enable if it's GAS and network event
	 * and the Glue media state is connected.
	 */
	if ((u4Events & GED_EVENT_GAS) != 0
		&& (u4Events & GED_EVENT_NETWORK) != 0
		&& MEDIA_STATE_CONNECTED
			== kalGetMediaStateIndicated(prAdapter->prGlueInfo,
			ucBssIndex))
		fgEnMode = TRUE; /* It will enable low latency mode */

#if CFG_FAST_PATH_SUPPORT
	if (fgEnMode != prAdapter->fgEnLowLatencyMode) {
		if (!fgEnMode && (MEDIA_STATE_CONNECTED
			== kalGetMediaStateIndicated(prAdapter->prGlueInfo,
			ucBssIndex)))
			mscsDeactivate(prAdapter,
				aisGetStaRecOfAP(prAdapter, AIS_DEFAULT_INDEX));
	}
#endif

	/* Enable/disable scan management decision:
	 *
	 * Enable if it will enable low latency mode.
	 * Or, enable if it is a white list event.
	 */
	if (fgEnMode != TRUE || (u4Events & GED_EVENT_DOPT_WIFI_SCAN) != 0)
		fgEnScan = TRUE; /* It will enable scan management */

	/* Enable/disable power management decision:
	 *
	 * Enable constantly awake mode (equivalent to disable power management)
	 * if user manually set in low latency mode
	 */
	if (fgEnMode == TRUE && (u4Events & GED_EVENT_CAM_MODE) != 0)
		fgEnPM = FALSE;

	/* Enable/disable roaming decision:
	 *
	 * Disable roaming if user manually set in low latency mode
	 */
	if (fgEnMode == TRUE && (u4Events & GED_EVENT_DISABLE_ROAMING) != 0)
		fgEnRoaming = FALSE; /* It will disable roaming */

	/* Debug log for the actions
	 * log when low latency mode, scan setting changes
	 * or PS, roaming is manually disable under low latency mode
	 */
	if (fgEnMode != prAdapter->fgEnLowLatencyMode
		|| fgEnScan != prAdapter->fgEnCfg80211Scan
		|| (fgEnMode && !fgEnPM)
		|| (fgEnMode && !fgEnRoaming)) {
		DBGLOG(OID, INFO,
			"LowLatency(gaming/DPP) change (m:%d,s:%d,PM:%d,r:%d))\n",
			fgEnMode, fgEnScan, fgEnPM, fgEnRoaming);
	}

	/* Scan management:
	 *
	 * Disable/enable scan
	 */
	if ((prWifiVar->ucLowLatencyModeScan == FEATURE_ENABLED) &&
	    (fgEnScan != prAdapter->fgEnCfg80211Scan))
		prAdapter->fgEnCfg80211Scan = fgEnScan;

	if ((prWifiVar->ucLowLatencyModeReOrder == FEATURE_ENABLED) &&
	    (fgEnMode != prAdapter->fgEnLowLatencyMode)) {
		/* Queue management:
		 *
		 * Change QM RX BA timeout if the gaming mode state changed
		 */
		if (fgEnMode) {
			prAdapter->u4QmRxBaMissTimeout
				= prWifiVar->u4BaShortMissTimeoutMs;
		} else {
			prAdapter->u4QmRxBaMissTimeout
				= prWifiVar->u4BaMissTimeoutMs;
		}
	}

	/* Power management:
	 *
	 * Set power saving mode profile to FW
	 *
	 * Do if 1. the power saving caller including GPU
	 * and 2. it will disable low latency mode.
	 * Or, do if 1. the power saving caller is not including GPU
	 * and 2. it will enable low latency mode.
	 */
	if (prWifiVar->ucLowLatencyModePower == FEATURE_ENABLED) {
		if (fgEnPM == FALSE)
			rPowerMode.ePowerMode = Param_PowerModeCAM;
		else
			rPowerMode.ePowerMode = Param_PowerModeFast_PSP;

		nicConfigPowerSaveProfile(prAdapter, rPowerMode.ucBssIdx,
			rPowerMode.ePowerMode, FALSE, PS_CALLER_GPU);

		#if CFG_SUPPORT_SMART_GEAR
		wlandioSetSGStatus(prAdapter,
			fgEnMode, 0x00, 0x00);
		DBGLOG(OID, INFO,
			"[SG] SmartGear (%d) for gaming mode\n",
			fgEnMode);
		#endif

	}

	/* Roaming management:
	 *
	 * Disable/enable roaming
	 */
	kalSnprintf(arCmd, sizeof(arCmd), "RoamingEnable %d", fgEnRoaming?1:0);
	wlanChipCommand(prAdapter, &arCmd[0], sizeof(arCmd));

	if (fgEnMode != prAdapter->fgEnLowLatencyMode)
		prAdapter->fgEnLowLatencyMode = fgEnMode;

	DBGLOG(OID, INFO,
		"LowLatency(gaming) fgEnMode=[%d]\n", fgEnMode);

	/* Force RTS to protect game packet */
	wlanSetForceRTS(prAdapter, fgEnMode);

	/* Tx Duplicate Detect management:
	 *
	 * Disable/enable tx duplicate detect
	 */
	if ((u4Events & GED_EVENT_TX_DUP_DETECT) != 0)
		fgEnTxDupDetect = TRUE;

	DBGLOG(OID, INFO,
		"EnTxDupDetect: Orig:[%d], New:[%d], CmdData:[%d], Cert:[%d]\n",
		prAdapter->fgEnTxDupDetect,
		fgEnTxDupDetect,
		prAdapter->rWifiVar.ucLowLatencyCmdData,
		prAdapter->fgTxDupCertificate);

	if (prAdapter->fgEnTxDupDetect != fgEnTxDupDetect) {

		prAdapter->fgEnTxDupDetect = fgEnTxDupDetect;

#if CFG_TCP_IP_CHKSUM_OFFLOAD
		if (prAdapter->fgIsSupportCsumOffload) {
			if (prAdapter->fgEnTxDupDetect) {
				/* Disable csum offload for command data */
				kalConfigChksumOffload(
					prAdapter->prGlueInfo, FALSE);
			} else {
				/* Enable csum offload for cut-through data */
				kalConfigChksumOffload(
					prAdapter->prGlueInfo, TRUE);
			}
		}
#endif
		/* Send command to firmware */
		wlanSetLowLatencyCommand(prAdapter,
				prAdapter->fgEnLowLatencyMode,
				prAdapter->fgEnTxDupDetect,
				FALSE);
	}

	return WLAN_STATUS_SUCCESS;
}

#endif /* CFG_SUPPORT_LOWLATENCY_MODE */

#if CFG_SUPPORT_EASY_DEBUG

void wlanCfgFwSetParam(uint8_t *fwBuffer, char *cmdStr, char *value, int num,
		       int type)
{
	struct CMD_FORMAT_V1 *cmd = (struct CMD_FORMAT_V1 *)fwBuffer + num;

	kalMemSet(cmd, 0, sizeof(struct CMD_FORMAT_V1));
	cmd->itemType = type;

	cmd->itemStringLength = strlen(cmdStr);
	if (cmd->itemStringLength > MAX_CMD_NAME_MAX_LENGTH)
		cmd->itemStringLength = MAX_CMD_NAME_MAX_LENGTH;

	/* here will not ensure the end will be '\0' */
	kalMemCopy(cmd->itemString, cmdStr, cmd->itemStringLength);

	cmd->itemValueLength = strlen(value);
	if (cmd->itemValueLength > MAX_CMD_VALUE_MAX_LENGTH)
		cmd->itemValueLength = MAX_CMD_VALUE_MAX_LENGTH;

	/* here will not ensure the end will be '\0' */
	kalMemCopy(cmd->itemValue, value, cmd->itemValueLength);
}

uint32_t wlanCfgSetGetFw(struct ADAPTER *prAdapter, const char *fwBuffer,
			 int cmdNum, enum CMD_TYPE cmdType)
{
	struct CMD_HEADER *pcmdV1Header = NULL;

	pcmdV1Header = (struct CMD_HEADER *)
			kalMemAlloc(sizeof(struct CMD_HEADER), VIR_MEM_TYPE);

	if (pcmdV1Header == NULL)
		return WLAN_STATUS_FAILURE;

	kalMemSet(pcmdV1Header->buffer, 0, MAX_CMD_BUFFER_LENGTH);
	pcmdV1Header->cmdType = cmdType;
	pcmdV1Header->cmdVersion = CMD_VER_1_EXT;
	pcmdV1Header->itemNum = cmdNum;
	pcmdV1Header->cmdBufferLen = cmdNum * sizeof(struct CMD_FORMAT_V1);
	kalMemCopy(pcmdV1Header->buffer, fwBuffer, pcmdV1Header->cmdBufferLen);

	wlanSendSetQueryCmd(prAdapter, CMD_ID_GET_SET_CUSTOMER_CFG,
				TRUE, FALSE, FALSE,
				NULL, NULL,
				sizeof(struct CMD_HEADER),
				(uint8_t *) pcmdV1Header,
				NULL, 0);
	kalMemFree(pcmdV1Header, VIR_MEM_TYPE, sizeof(struct CMD_HEADER));
	return WLAN_STATUS_SUCCESS;
}

uint32_t wlanFwCfgParse(struct ADAPTER *prAdapter, uint8_t *pucConfigBuf)
{
	/* here return a list should be better */
	char *saveptr1, *saveptr2;
	char *cfgItems = pucConfigBuf;
	uint8_t cmdNum = 0;

	uint8_t *cmdBuffer = kalMemAlloc(MAX_CMD_BUFFER_LENGTH, VIR_MEM_TYPE);

	if (cmdBuffer == 0) {
		DBGLOG(INIT, INFO, "omega, cmd buffer return fail!");
		return WLAN_STATUS_FAILURE;
	}
	kalMemSet(cmdBuffer, 0, MAX_CMD_BUFFER_LENGTH);

	while (1) {
		char *keyStr = NULL;
		char *valueStr = NULL;
		char *cfgEntry = kalStrtokR(cfgItems, "\n\r", &saveptr1);

		if (!cfgEntry) {
			if (cmdNum)
				wlanCfgSetGetFw(prAdapter, cmdBuffer, cmdNum,
						CMD_TYPE_SET);

			if (cmdBuffer)
				kalMemFree(cmdBuffer, VIR_MEM_TYPE,
					   MAX_CMD_BUFFER_LENGTH);

			return WLAN_STATUS_SUCCESS;
		}
		cfgItems = NULL;

		keyStr = kalStrtokR(cfgEntry, " \t", &saveptr2);
		valueStr = kalStrtokR(NULL, "\0", &saveptr2);

		/* maybe a blank line, but with some tab or whitespace */
		if (!keyStr)
			continue;

		/* here take '#' at the beginning of line as comment */
		if (keyStr[0] == '#')
			continue;

		/* remove the \t " " at the beginning of the valueStr */
		while (valueStr && (*valueStr == '\t' || *valueStr == ' '))
			valueStr++;

		if (keyStr && valueStr) {
			wlanCfgFwSetParam(cmdBuffer, keyStr, valueStr, cmdNum,
					  1);
			cmdNum++;
			if (cmdNum == MAX_CMD_ITEM_MAX) {
				wlanCfgSetGetFw(prAdapter, cmdBuffer,
						MAX_CMD_ITEM_MAX, CMD_TYPE_SET);
				kalMemSet(cmdBuffer, 0, MAX_CMD_BUFFER_LENGTH);
				cmdNum = 0;
			}
		} else {
			/* here will not to try send the cmd has been parsed,
			 * but not sent yet
			 */
			if (cmdBuffer)
				kalMemFree(cmdBuffer, VIR_MEM_TYPE,
					   MAX_CMD_BUFFER_LENGTH);
			return WLAN_STATUS_FAILURE;
		}
	}
}
#endif /* CFG_SUPPORT_EASY_DEBUG */

void wlanReleasePendingCmdById(struct ADAPTER *prAdapter, uint8_t ucCid)
{
	struct QUE *prCmdQue;
	struct QUE rTempCmdQue;
	struct QUE *prTempCmdQue = &rTempCmdQue;
	struct QUE_ENTRY *prQueueEntry = (struct QUE_ENTRY *) NULL;
	struct CMD_INFO *prCmdInfo = (struct CMD_INFO *) NULL;

	KAL_SPIN_LOCK_DECLARATION();

	ASSERT(prAdapter);
	DBGLOG(OID, INFO, "Remove pending Cmd: CID %d\n", ucCid);

	/* 1: Clear Pending OID in prAdapter->rPendingCmdQueue */
	KAL_ACQUIRE_PENDING_CMD_LOCK(prAdapter);

	prCmdQue = &prAdapter->rPendingCmdQueue;
	QUEUE_MOVE_ALL(prTempCmdQue, prCmdQue);

	QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry, struct QUE_ENTRY *);
	while (prQueueEntry) {
		prCmdInfo = (struct CMD_INFO *) prQueueEntry;
		if (prCmdInfo->ucCID != ucCid) {
			QUEUE_INSERT_TAIL(prCmdQue, prQueueEntry);
			continue;
		}

		if (prCmdInfo->pfCmdTimeoutHandler) {
			prCmdInfo->pfCmdTimeoutHandler(prAdapter, prCmdInfo);
		} else if (prCmdInfo->fgIsOid) {
			kalOidComplete(prAdapter->prGlueInfo,
				       prCmdInfo, 0,
				       WLAN_STATUS_FAILURE);
		}

		cmdBufFreeCmdInfo(prAdapter, prCmdInfo);
		QUEUE_REMOVE_HEAD(prTempCmdQue, prQueueEntry,
				  struct QUE_ENTRY *);
	}

	KAL_RELEASE_PENDING_CMD_LOCK(prAdapter);
}

/* Translate Decimals string to Hex
** The result will be put in a 2bytes variable.
** Integer part will occupy the left most 3 bits, and decimal part is in the
** left 13 bits
** Integer part can be parsed by kstrtou16, decimal part should be translated by
** mutiplying
** 16 and then pick integer part.
** For example
*/
uint32_t wlanDecimalStr2Hexadecimals(uint8_t *pucDecimalStr, uint16_t *pu2Out)
{
	uint8_t aucDecimalStr[32] = {0};
	uint8_t *pucDecimalPart = NULL;
	uint8_t *tmp = NULL;
	uint32_t u4Result = 0;
	uint32_t u4Ret = 0;
	uint32_t u4Degree = 0;
	uint32_t u4Remain = 0;
	uint8_t ucAccuracy = 4; /* Hex decimals accuarcy is 4 bytes */
	uint32_t u4Base = 1;

	if (!pu2Out || !pucDecimalStr)
		return 1;

	while (*pucDecimalStr == '0')
		pucDecimalStr++;
	kalStrnCpy(aucDecimalStr, pucDecimalStr, sizeof(aucDecimalStr) - 1);
	aucDecimalStr[31] = 0;
	pucDecimalPart = strchr(aucDecimalStr, '.');
	if (!pucDecimalPart) {
		DBGLOG(INIT, INFO, "No decimal part, ori str %s\n",
		       pucDecimalStr);
		goto integer_part;
	}
	*pucDecimalPart++ = 0;
	/* get decimal degree */
	tmp = pucDecimalPart + strlen(pucDecimalPart);
	do {
		if (tmp == pucDecimalPart) {
			DBGLOG(INIT, INFO,
			       "Decimal part are all 0, ori str %s\n",
			       pucDecimalStr);
			goto integer_part;
		}
		tmp--;
	} while (*tmp == '0');

	*(++tmp) = 0;
	u4Degree = (uint32_t)(tmp - pucDecimalPart);
	/* if decimal part is not 0, translate it to hexadecimal decimals */
	/* Power(10, degree) */
	for (; u4Remain < u4Degree; u4Remain++)
		u4Base *= 10;

	while (*pucDecimalPart == '0')
		pucDecimalPart++;

	u4Ret = kalkStrtou32(pucDecimalPart, 0, &u4Remain);
	if (u4Ret) {
		DBGLOG(INIT, ERROR, "Parse decimal str %s error, degree %u\n",
			   pucDecimalPart, u4Degree);
		return u4Ret;
	}

	do {
		u4Remain *= 16;
		u4Result |= (u4Remain / u4Base) << ((ucAccuracy-1) * 4);
		u4Remain %= u4Base;
		ucAccuracy--;
	} while (u4Remain && ucAccuracy > 0);
	/* Each Hex Decimal byte was left shift more than 3 bits, so need
	** right shift 3 bits at last
	** For example, mmmnnnnnnnnnnnnn.
	** mmm is integer part, n represents decimals part.
	** the left most 4 n are shift 9 bits. But in for loop, we shift 12 bits
	**/
	u4Result >>= 3;
	u4Remain = 0;

integer_part:
	u4Ret = kalkStrtou32(aucDecimalStr, 0, &u4Remain);
	u4Result |= u4Remain << 13;

	if (u4Ret)
		DBGLOG(INIT, ERROR, "Parse integer str %s error\n",
		       aucDecimalStr);
	else {
		*pu2Out = u4Result & 0xffff;
		DBGLOG(INIT, TRACE, "Result 0x%04x\n", *pu2Out);
	}
	return u4Ret;
}

uint64_t wlanGetSupportedFeatureSet(struct GLUE_INFO *prGlueInfo)
{
	uint64_t u8FeatureSet = WIFI_HAL_FEATURE_SET;
	struct REG_INFO *prRegInfo;

	prRegInfo = &(prGlueInfo->rRegInfo);
	if ((prRegInfo != NULL) && (prRegInfo->ucSupport5GBand))
		u8FeatureSet |= WIFI_FEATURE_INFRA_5G;

#if CFG_SUPPORT_LLS
	if (!kalIsHalted() && prGlueInfo->prAdapter &&
	    prGlueInfo->prAdapter->pucLinkStatsSrcBufferAddr)
		u8FeatureSet |= WIFI_FEATURE_LINK_LAYER_STATS;
#endif

#if CFG_SUPPORT_TDLS_OFFCHANNEL
	u8FeatureSet |= WIFI_FEATURE_TDLS_OFFCHANNEL;
#endif

	return u8FeatureSet;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is a wrapper to send eapol offload (rekey) command
* with PN sync consideration
*
* @param prGlueInfo  Pointer of prGlueInfo Data Structure
*
* @return VOID
*/
/*----------------------------------------------------------------------------*/
uint32_t
wlanSuspendRekeyOffload(struct GLUE_INFO *prGlueInfo, uint8_t ucRekeyMode)
{
	uint32_t u4BufLen;
	struct PARAM_GTK_REKEY_DATA *prGtkData;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4Rslt = WLAN_STATUS_FAILURE;
	struct GL_WPA_INFO *prWpaInfo;
	struct BSS_INFO *prBssInfo = NULL;

	if (!prGlueInfo)
		return WLAN_STATUS_NOT_ACCEPTED;

	prBssInfo = aisGetConnectedBssInfo(
		prGlueInfo->prAdapter);

	/* prAisBssInfo exist only when connect, skip if disconnect */
	if (!prBssInfo)
		return u4Rslt;

	prGtkData =
		(struct PARAM_GTK_REKEY_DATA *) kalMemAlloc(sizeof(
				struct PARAM_GTK_REKEY_DATA), VIR_MEM_TYPE);

	if (!prGtkData)
		return WLAN_STATUS_SUCCESS;

	kalMemZero(prGtkData, sizeof(struct PARAM_GTK_REKEY_DATA));

	prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
		prBssInfo->ucBssIndex);

	DBGLOG(RSN, INFO, "GTK Rekey ucRekeyMode = %d, BssIndex = %d\n",
		ucRekeyMode, prBssInfo->ucBssIndex);

	/* if enable, FW rekey offload. if disable, rekey back to supplicant */
	prGtkData->ucRekeyMode = ucRekeyMode;

	if (ucRekeyMode == GTK_REKEY_CMD_MODE_OFFLOAD_ON) {
		DBGLOG(RSN, INFO, "kek\n");
		DBGLOG_MEM8(RSN, INFO, (uint8_t *)prWpaInfo->aucKek,
			NL80211_KEK_LEN);
		DBGLOG(RSN, INFO, "kck\n");
		DBGLOG_MEM8(RSN, INFO, (uint8_t *)prWpaInfo->aucKck,
			NL80211_KCK_LEN);
		DBGLOG(RSN, INFO, "replay count\n");
		DBGLOG_MEM8(RSN, INFO,
			(uint8_t *)prWpaInfo->aucReplayCtr,
			NL80211_REPLAY_CTR_LEN);

		kalMemCopy(prGtkData->aucKek, prWpaInfo->aucKek,
			NL80211_KEK_LEN);
		kalMemCopy(prGtkData->aucKck, prWpaInfo->aucKck,
			NL80211_KCK_LEN);
		kalMemCopy(prGtkData->aucReplayCtr,
			prWpaInfo->aucReplayCtr,
			NL80211_REPLAY_CTR_LEN);

		prGtkData->ucBssIndex =	prBssInfo->ucBssIndex;

		prGtkData->u4Proto = NL80211_WPA_VERSION_2;
		if (prWpaInfo->u4WpaVersion == IW_AUTH_WPA_VERSION_WPA)
			prGtkData->u4Proto = NL80211_WPA_VERSION_1;

		if (GET_SELECTOR_TYPE(prBssInfo->
			u4RsnSelectedPairwiseCipher) == CIPHER_SUITE_TKIP)
			prGtkData->u4PairwiseCipher = BIT(3);
		else if (GET_SELECTOR_TYPE(prBssInfo->
			u4RsnSelectedPairwiseCipher) == CIPHER_SUITE_CCMP)
			prGtkData->u4PairwiseCipher = BIT(4);
		else {
			kalMemFree(prGtkData, VIR_MEM_TYPE,
				   sizeof(struct PARAM_GTK_REKEY_DATA));
			return 0;
		}

		if (GET_SELECTOR_TYPE(prBssInfo->
			u4RsnSelectedGroupCipher) == CIPHER_SUITE_TKIP)
			prGtkData->u4GroupCipher = BIT(3);
		else if (GET_SELECTOR_TYPE(prBssInfo->
			u4RsnSelectedGroupCipher) == CIPHER_SUITE_CCMP)
			prGtkData->u4GroupCipher = BIT(4);
		else {
			kalMemFree(prGtkData, VIR_MEM_TYPE,
				   sizeof(struct PARAM_GTK_REKEY_DATA));
			return 0;
		}

		prGtkData->u4KeyMgmt = prBssInfo->u4RsnSelectedAKMSuite;
		prGtkData->u4MgmtGroupCipher = 0;
	}

	if (ucRekeyMode == GTK_REKEY_CMD_MODE_OFLOAD_OFF) {
		/* inform FW disable EAPOL offload */
		prGtkData->ucBssIndex =	prBssInfo->ucBssIndex;
		DBGLOG(RSN, INFO, "Disable EAPOL offload\n");
	}

	rStatus = kalIoctl(prGlueInfo,
				wlanoidSetGtkRekeyData,
				prGtkData, sizeof(struct PARAM_GTK_REKEY_DATA),
				&u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		DBGLOG(INIT, ERROR, "Suspend rekey data err:%x\n", rStatus);
	else
		u4Rslt = WLAN_STATUS_SUCCESS;

	kalMemFree(prGtkData, VIR_MEM_TYPE,
		sizeof(struct PARAM_GTK_REKEY_DATA));

	return u4Rslt;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is a wrapper to send power-saving mode command
*        when AIS enter wow, and send WOW command
*        Also let GC/GO/AP enter deactivate state to enter TOP sleep
*
* @param prGlueInfo                     Pointer of prGlueInfo Data Structure
*
* @return VOID
*/
/*----------------------------------------------------------------------------*/
void wlanSuspendPmHandle(struct GLUE_INFO *prGlueInfo)
{
	uint8_t idx, i;
	struct STA_RECORD *prStaRec;
	struct RX_BA_ENTRY *prRxBaEntry;
	enum PARAM_POWER_MODE ePwrMode;
	struct BSS_INFO *prAisBssInfo = NULL;
	struct WIFI_VAR *prWifiVar = NULL;
	uint8_t ucKekZeroCnt = 0;
	uint8_t ucKckZeroCnt = 0;
	uint8_t ucGtkOffload = TRUE;
	struct GL_WPA_INFO *prWpaInfo;

	if (prGlueInfo == NULL || prGlueInfo->prAdapter == NULL)
		return;

	prAisBssInfo = aisGetConnectedBssInfo(
		prGlueInfo->prAdapter);
	prWifiVar = &prGlueInfo->prAdapter->rWifiVar;

	/* process if AIS is connected */
	if (prAisBssInfo) {
		prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
					  prAisBssInfo->ucBssIndex);

		/* if EapolOffload is 0 => rekey when suspend wow */
		if (prWifiVar->ucEapolSuspendOffload) {

			/*
			 * check if KCK, KEK not sync from supplicant.
			 * if no these info updated from supplicant,
			 * disable GTK offload feature.
			 */
			for (i = 0; i < NL80211_KEK_LEN; i++) {
				if (prWpaInfo->aucKek[i] == 0x00)
					ucKekZeroCnt++;
			}

			for (i = 0; i < NL80211_KCK_LEN; i++) {
				if (prWpaInfo->aucKck[i] == 0x00)
					ucKckZeroCnt++;
			}

			if ((ucKekZeroCnt == NL80211_KCK_LEN) ||
			    (ucKckZeroCnt == NL80211_KCK_LEN)) {
				DBGLOG(RSN, INFO,
				       "no offload, no KCK/KEK from cfg\n");

				ucGtkOffload = FALSE;
			}

			if (ucGtkOffload)
				wlanSuspendRekeyOffload(prGlueInfo,
						 GTK_REKEY_CMD_MODE_OFFLOAD_ON);

			DBGLOG(HAL, STATE, "Suspend rekey offload\n");
		}
	}

#if CFG_WOW_SUPPORT
	/* 1) wifi cfg "Wow" is true              */
	/* 2) wow is enable                       */
	/* 3) WIFI connected => execute WOW flow  */
	/* 4) WIFI disconnected                   */
	/*      => is schedlued scanning &        */
	/*          schedlued scan wakeup enabled */
	/*      => execute WOW flow               */

	if (IS_FEATURE_ENABLED(prWifiVar->ucWow)) {
		/* Pending Timer related to CNM need to check and
		 * perform corresponding timeout handler. Without it,
		 * Might happen CNM abnormal after resume or during suspend.
		 */
		cnmStopPendingJoinTimerForSuspend(prGlueInfo->prAdapter);

		if (IS_FEATURE_ENABLED(prGlueInfo->prAdapter->rWowCtrl.fgWowEnable) ||
			IS_FEATURE_ENABLED(prWifiVar->ucAdvPws)) {
			if (prAisBssInfo) {
				/* AIS bss enter wow power mode, default fast pws */
				ePwrMode = Param_PowerModeFast_PSP;
				idx = prAisBssInfo->ucBssIndex;
				nicConfigPowerSaveProfile(prGlueInfo->prAdapter, idx,
					ePwrMode, FALSE, PS_CALLER_WOW);
				DBGLOG(HAL, STATE, "Wow AIS_idx:%d, pwr mode:%d\n",
					idx, ePwrMode);

				DBGLOG(HAL, EVENT, "enter WOW flow\n");
				kalWowProcess(prGlueInfo, TRUE);
			} else if ((prWifiVar->rScanInfo.fgSchedScanning) &&
				(prGlueInfo->prAdapter->rWifiVar.ucWowDetectType
				& WOWLAN_DETECT_TYPE_SCHD_SCAN_SSID_HIT)) {
				DBGLOG(HAL, EVENT,
					"Sched Scanning, enter WOW flow\n");
				kalWowProcess(prGlueInfo, TRUE);
			}
		}
	}
#endif

	/* After resuming, WinStart will unsync with AP's SN.
	 * Set fgFirstSnToWinStart for all valid BA entry before suspend.
	 */
	for (idx = 0; idx < CFG_STA_REC_NUM; idx++) {
		prStaRec = cnmGetStaRecByIndex(prGlueInfo->prAdapter, idx);
		if (!prStaRec)
			continue;

		for (i = 0; i < CFG_RX_MAX_BA_TID_NUM; i++) {
			prRxBaEntry = prStaRec->aprRxReorderParamRefTbl[i];
			if (!prRxBaEntry || !(prRxBaEntry->fgIsValid))
				continue;

			prRxBaEntry->fgFirstSnToWinStart = TRUE;
		}
	}

}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to restore power-saving mode command when leave wow
*        But ignore GC/GO/AP role
*
* @param prGlueInfo                     Pointer of prGlueInfo Data Structure
*
* @return VOID
*/
/*----------------------------------------------------------------------------*/
void wlanResumePmHandle(struct GLUE_INFO *prGlueInfo)
{
#if CFG_WOW_SUPPORT
	struct BSS_INFO *prAisBssInfo = NULL;
	struct WIFI_VAR *prWifiVar = NULL;
	uint8_t ucKekZeroCnt = 0;
	uint8_t ucKckZeroCnt = 0;
	uint8_t ucGtkOffload = TRUE;
	uint8_t i = 0;
	struct GL_WPA_INFO *prWpaInfo;
#if CFG_SUPPORT_REPLAY_DETECTION
	uint8_t ucKeyIdx = 0;
	struct GL_DETECT_REPLAY_INFO *prDetRplyInfo = NULL;
#endif

	if (prGlueInfo == NULL || prGlueInfo->prAdapter == NULL)
		return;

	prAisBssInfo = aisGetConnectedBssInfo(
		prGlueInfo->prAdapter);

	prWifiVar = &prGlueInfo->prAdapter->rWifiVar;

	/* process if AIS is connected */
	if (prAisBssInfo) {
		prWpaInfo = aisGetWpaInfo(prGlueInfo->prAdapter,
					  prAisBssInfo->ucBssIndex);

		/* if cfg EAPOL offload disble, we disable offload
		 * when leave wow
		 */
		if (prWifiVar->ucEapolSuspendOffload) {

			/*
			 * check if KCK, KEK not sync from supplicant.
			 * if no these info updated from supplicant,
			 *disable GTK offload feature.
			 */
			for (i = 0; i < NL80211_KEK_LEN; i++) {
				if (prWpaInfo->aucKek[i] == 0x00)
					ucKekZeroCnt++;
			}

			for (i = 0; i < NL80211_KCK_LEN; i++) {
				if (prWpaInfo->aucKck[i] == 0x00)
					ucKckZeroCnt++;
			}

			if ((ucKekZeroCnt == NL80211_KCK_LEN) ||
			    (ucKckZeroCnt == NL80211_KCK_LEN)) {

				DBGLOG(RSN, INFO,
				       "no offload, no KCK/KEK from cfg\n");

				ucGtkOffload = FALSE;
			}

#if CFG_SUPPORT_REPLAY_DETECTION
			prDetRplyInfo = aisGetDetRplyInfo(prGlueInfo->prAdapter,
						      prAisBssInfo->ucBssIndex);

			/* Reset BC/MC KeyRSC to prevent incorrect replay
			 * detect
			 */
			for (ucKeyIdx = 0; ucKeyIdx < 4; ucKeyIdx++) {
				kalMemZero(
				   prDetRplyInfo->arReplayPNInfo[ucKeyIdx].auPN,
				   NL80211_KEYRSC_LEN);
			}

#endif

			if (ucGtkOffload) {
				wlanSuspendRekeyOffload(prGlueInfo,
						 GTK_REKEY_CMD_MODE_OFLOAD_OFF);

				DBGLOG(HAL, STATE,
				       "Resume rekey offload disable\n");
			}
		}
	}

	if (IS_FEATURE_ENABLED(prWifiVar->ucWow) &&
		(IS_FEATURE_ENABLED(prGlueInfo->prAdapter->rWowCtrl.fgWowEnable) ||
		IS_FEATURE_ENABLED(prWifiVar->ucAdvPws))) {
		if (prAisBssInfo) {
			DBGLOG(HAL, EVENT, "leave WOW flow\n");
			kalWowProcess(prGlueInfo, FALSE);

			/* Restore AIS pws when leave wow, ignore ePwrMode */
			nicConfigPowerSaveProfile(prGlueInfo->prAdapter,
				prAisBssInfo->ucBssIndex,
				Param_PowerModeCAM,
				FALSE, PS_CALLER_WOW);
		} else if ((prWifiVar->rScanInfo.fgSchedScanning) &&
			(prGlueInfo->prAdapter->rWifiVar.ucWowDetectType
			& WOWLAN_DETECT_TYPE_SCHD_SCAN_SSID_HIT)) {
			DBGLOG(HAL, EVENT,
				"Sched Scanning, leave WOW flow\n");
			kalWowProcess(prGlueInfo, FALSE);
		}
	}
#endif
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is to wake up WiFi
*
* @param prAdapter      Pointer to the Adapter structure.
*
* @return WLAN_STATUS_SUCCESS
*         WLAN_STATUS_FAILURE
*/
/*----------------------------------------------------------------------------*/
uint32_t wlanWakeUpWiFi(struct ADAPTER *prAdapter)
{
	u_int8_t fgReady;
	struct mt66xx_chip_info *prChipInfo;

	if (!prAdapter)
		return WLAN_STATUS_FAILURE;
	prChipInfo = prAdapter->chip_info;

	HAL_WIFI_FUNC_READY_CHECK(prAdapter, prChipInfo->sw_ready_bits,
			&fgReady);

	if (fgReady) {
#if defined(_HIF_USB)
		DBGLOG(INIT, INFO,
			"Wi-Fi is already ON!, turn off before FW DL!\n");
		if (wlanPowerOffWifi(prAdapter) != WLAN_STATUS_SUCCESS)
			return WLAN_STATUS_FAILURE;
#else
		DBGLOG(INIT, INFO, "Wi-Fi is already ON!\n");
#endif
	}

	nicpmWakeUpWiFi(prAdapter);
	HAL_HIF_INIT(prAdapter);

	return WLAN_STATUS_SUCCESS;
}

static uint32_t wlanHwRateOfdmNum(uint16_t ofdm_idx)
{
	switch (ofdm_idx) {
	case 11: /* 6M */
		return g_rOfdmDataRateMappingTable.rate[0];
	case 15: /* 9M */
		return g_rOfdmDataRateMappingTable.rate[1];
	case 10: /* 12M */
		return g_rOfdmDataRateMappingTable.rate[2];
	case 14: /* 18M */
		return g_rOfdmDataRateMappingTable.rate[3];
	case 9: /* 24M */
		return g_rOfdmDataRateMappingTable.rate[4];
	case 13: /* 36M */
		return g_rOfdmDataRateMappingTable.rate[5];
	case 8: /* 48M */
		return g_rOfdmDataRateMappingTable.rate[6];
	case 12: /* 54M */
		return g_rOfdmDataRateMappingTable.rate[7];
	default:
		return 0;
	}
}

/**
 * wlanQueryRateByTable() - get phy rate by parameters in the unit of 0.1M
 * @mode: TX_RATE_MODE_CCK (0),
 *	  TX_RATE_MODE_OFDM (1),
 *	  TX_RATE_MODE_HTMIX (2), TX_RATE_MODE_HTGF (3),
 *	  TX_RATE_MODE_VHT (4),
 *	  TX_RATE_MODE_HE_SU (8), TX_RATE_MODE_HE_ER (9)
 * @rate: MCS index, [0, ..]
 * @bw: bandwidth, 0 for 11n, [0, 3] for 11ac, 11ax for 20, 40, 80, 160
 * @gi: GI, [0, 1] for 11n, 11ac, 1 as short GI
 *	    [0, 2] for 11ax, 0 as shortest, 2 as longest
 * @nsts: NSTS, [1, 3]
 * @pu4CurRate: returning current phy rate by given parameters
 * @pu4MaxRate: returning max phy rate (max MCS)
 *
 * Return:
 *	0: Success
 *	-1: Failure
 */
int wlanQueryRateByTable(uint32_t txmode, uint32_t rate,
			uint32_t frmode, uint32_t gi, uint32_t nsts,
			uint32_t *pu4CurRate, uint32_t *pu4MaxRate)
{
	uint32_t u4CurRate = 0, u4MaxRate = 0;
	uint8_t ucMaxSize = 0;

	if (txmode == TX_RATE_MODE_CCK) { /* 11B */
		ucMaxSize = ARRAY_SIZE(g_rCckDataRateMappingTable.rate);
		/* short preamble */
		if (rate >= 5 && rate <= 7)
			rate -= 4;
		if (rate >= ucMaxSize) {
			DBGLOG_LIMITED(SW4, ERROR, "rate error for CCK: %u\n",
					rate);
			return -1;
		}
		u4CurRate = g_rCckDataRateMappingTable.rate[rate];
		u4MaxRate = g_rCckDataRateMappingTable
			.rate[MCS_IDX_MAX_RATE_CCK];
	} else if (txmode == TX_RATE_MODE_OFDM) { /* 11G */
		u4CurRate = wlanHwRateOfdmNum(rate);
		if (u4CurRate == 0) {
			DBGLOG_LIMITED(SW4, ERROR, "rate error for OFDM\n");
			return -1;
		}
		u4MaxRate = g_rOfdmDataRateMappingTable
			.rate[MCS_IDX_MAX_RATE_OFDM];
	} else if (txmode == TX_RATE_MODE_HTMIX ||
		   txmode == TX_RATE_MODE_HTGF) { /* 11N */
		if (nsts == 0 || nsts >= 4) {
			DBGLOG_LIMITED(SW4, ERROR, "nsts error: %u\n", nsts);
			return -1;
		}

		ucMaxSize = 8;
		if (rate > 23) {
			DBGLOG_LIMITED(SW4, ERROR, "rate error for 11N: %u\n",
					rate);
			return -1;
		}
		rate %= ucMaxSize;

		if (frmode > 1) {
			DBGLOG_LIMITED(SW4, ERROR,
			       "frmode error for 11N: %u\n", frmode);
			return -1;
		}
		u4CurRate = g_rDataRateMappingTable.nsts[nsts - 1].bw[frmode]
				.sgi[gi].rate[rate];
		u4MaxRate = g_rDataRateMappingTable.nsts[nsts - 1].bw[frmode]
				.sgi[gi].rate[MCS_IDX_MAX_RATE_HT];
	} else if (txmode == TX_RATE_MODE_VHT) { /* 11AC */
		if (nsts == 0 || nsts >= 4) {
			DBGLOG_LIMITED(SW4, ERROR, "nsts error: %u\n", nsts);
			return -1;
		}

		if (frmode > 3) {
			DBGLOG_LIMITED(SW4, ERROR,
			       "frmode error for 11AC: %u\n", frmode);
			return -1;
		}

		ucMaxSize = ARRAY_SIZE(g_rDataRateMappingTable.nsts[nsts - 1]
				.bw[frmode].sgi[gi].rate);
		if (rate >= ucMaxSize) {
			DBGLOG_LIMITED(SW4, ERROR, "rate error for 11AC: %u\n",
					rate);
			return -1;
		}

		u4CurRate = g_rDataRateMappingTable.nsts[nsts - 1]
				.bw[frmode].sgi[gi].rate[rate];
		u4MaxRate = g_rDataRateMappingTable.nsts[nsts - 1].bw[frmode]
				.sgi[gi].rate[MCS_IDX_MAX_RATE_VHT];
	} else if (txmode == TX_RATE_MODE_HE_SU ||
		   txmode == TX_RATE_MODE_HE_ER ||
		   txmode == TX_RATE_MODE_HE_MU) { /* AX */
		uint8_t dcm = 0, ru106 = 0;

		if (nsts == 0 || nsts >= 5) {
			DBGLOG_LIMITED(SW4, ERROR, "nsts error: %u\n", nsts);
			return -1;
		}
		if (frmode > 3) {
			DBGLOG_LIMITED(SW4, ERROR,
			       "frmode error for 11AX: %u\n", frmode);
			return -1;
		}

		/* bit 4: dcm, bit 5: RU106 */
		dcm = rate & BIT(4);
		ru106 = rate & BIT(5);
		rate = rate & BITS(0, 3);

		ucMaxSize = ARRAY_SIZE(g_rAxDataRateMappingTable.nsts[nsts - 1]
				.bw[frmode].gi[gi].rate);
		if (rate >= ucMaxSize) {
			DBGLOG_LIMITED(SW4, ERROR, "rate error for 11AX: %u\n",
					rate);
			return -1;
		}

		u4CurRate = g_rAxDataRateMappingTable.nsts[nsts - 1]
				.bw[frmode].gi[gi].rate[rate];
		u4MaxRate = g_rAxDataRateMappingTable.nsts[nsts - 1]
				.bw[frmode].gi[gi].rate[MCS_IDX_MAX_RATE_HE];

		if (dcm || ru106) {
			u4CurRate = u4CurRate >> 1;
			u4MaxRate = u4MaxRate >> 1;
		}
	} else if (txmode == TX_RATE_MODE_EHT_ER ||
		   txmode == TX_RATE_MODE_EHT_TRIG ||
		   txmode == TX_RATE_MODE_EHT_MU) { /* BE */
		uint8_t ru106 = 0;

		if (nsts == 0 || nsts >= 5) {
			DBGLOG_LIMITED(SW4, ERROR, "nsts error: %u\n", nsts);
			return -1;
		}
		if (frmode > 5) {
			DBGLOG_LIMITED(SW4, ERROR,
			       "frmode error for 11BE: %u\n", frmode);
			return -1;
		}
		if (frmode == 5) /* Both 4, 5 are 320MHz, look up by index 4 */
			frmode--;

		/* DCM = MCS15, bit 5: RU106 */
		rate = rate & BITS(0, 3);
		ru106 = rate & BIT(5);

		ucMaxSize = ARRAY_SIZE(g_rAxDataRateMappingTable.nsts[nsts - 1]
				.bw[frmode].gi[gi].rate);
		if (rate >= ucMaxSize) {
			DBGLOG(SW4, ERROR, "rate error for 11BE: %u\n", rate);
			return -1;
		}

		u4CurRate = g_rAxDataRateMappingTable.nsts[nsts - 1]
				.bw[frmode].gi[gi].rate[rate];
		u4MaxRate = g_rAxDataRateMappingTable.nsts[nsts - 1]
				.bw[frmode].gi[gi].rate[MCS_IDX_MAX_RATE_EHT];

		if (ru106) {
			u4CurRate = u4CurRate >> 1;
			u4MaxRate = u4MaxRate >> 1;
		}
	} else {
		DBGLOG_LIMITED(SW4, ERROR,
				"Unknown rate for [%d,%d,%d,%d,%d]\n",
				txmode, nsts, frmode, gi, rate);
		return -1;
	}

	if (pu4CurRate)
		*pu4CurRate = u4CurRate;
	if (pu4MaxRate)
		*pu4MaxRate = u4MaxRate;
	return 0;
}

#if CFG_REPORT_MAX_TX_RATE
int wlanGetMaxTxRate(struct ADAPTER *prAdapter,
		 void *prBssPtr, struct STA_RECORD *prStaRec,
		 uint32_t *pu4CurRate, uint32_t *pu4MaxRate)
{
	struct BSS_INFO *prBssInfo;
	uint8_t ucPhyType, ucTxMode = 0, ucMcsIdx = 0, ucSgi = 0;
	uint8_t ucBw = 0, ucAPBwPermitted = 0, ucNss = 0, ucApNss = 0;
	struct BSS_DESC *prBssDesc = NULL;

	*pu4CurRate = 0;
	*pu4MaxRate = 0;
	prBssInfo = (struct BSS_INFO *) prBssPtr;

	/* get tx mode and MCS index */
	ucPhyType = prBssInfo->ucPhyTypeSet;

	DBGLOG(SW4, TRACE,
		   "ucPhyType: 0x%x\n",
		   ucPhyType);

#if (CFG_SUPPORT_802_11BE == 1)
	if (ucPhyType & PHY_TYPE_SET_802_11BE)
		ucTxMode = TX_RATE_MODE_EHT_MU;
	else
#endif
#if (CFG_SUPPORT_802_11AX == 1)
	if (ucPhyType & PHY_TYPE_SET_802_11AX)
		ucTxMode = TX_RATE_MODE_HE_SU;
	else
#endif
	if (ucPhyType & PHY_TYPE_SET_802_11AC)
		ucTxMode = TX_RATE_MODE_VHT;
	else if (ucPhyType & PHY_TYPE_SET_802_11N)
		ucTxMode = TX_RATE_MODE_HTMIX;
	else if (ucPhyType & PHY_TYPE_SET_802_11G) {
		ucTxMode = TX_RATE_MODE_OFDM;
		ucMcsIdx = 12;
	} else if (ucPhyType & PHY_TYPE_SET_802_11A) {
		ucTxMode = TX_RATE_MODE_OFDM;
		ucMcsIdx = 12;
	} else if (ucPhyType & PHY_TYPE_SET_802_11B)
		ucTxMode = TX_RATE_MODE_CCK;
	else {
		DBGLOG(SW4, ERROR,
		       "unknown wifi type, prBssInfo->ucPhyTypeSet: %u\n",
		       ucPhyType);
		goto errhandle;
	}

	/* get bandwidth */
	ucBw = cnmGetBssMaxBw(prAdapter, prBssInfo->ucBssIndex);
	ucAPBwPermitted = rlmGetBssOpBwByVhtAndHtOpInfo(prBssInfo);
	if (ucAPBwPermitted < ucBw)
		ucBw = ucAPBwPermitted;

	/* Get Short GI Tx capability for HT/VHT, check from HT or VHT
	 * capability BW SGI bit. Refer to 802.11 Short GI operation
	 */
	if (ucTxMode == TX_RATE_MODE_HTMIX || ucTxMode == TX_RATE_MODE_HTGF
	    || ucTxMode == TX_RATE_MODE_VHT) {
		if (prStaRec->u2HtCapInfo & HT_CAP_INFO_SHORT_GI_20M) {
			DBGLOG(RLM, TRACE, "HT_CAP_INFO_SHORT_GI_20M\n");
			ucSgi = 1;
		}
		if (prStaRec->u2HtCapInfo & HT_CAP_INFO_SHORT_GI_40M) {
			DBGLOG(RLM, TRACE, "HT_CAP_INFO_SHORT_GI_40M\n");
			ucSgi = 1;
		}

#if CFG_SUPPORT_802_11AC
		if (prStaRec->u4VhtCapInfo & VHT_CAP_INFO_SHORT_GI_80) {
			DBGLOG(RLM, TRACE, "VHT_CAP_INFO_SHORT_GI_80\n");
			ucSgi = 1;
		}
		if (prStaRec->u4VhtCapInfo & VHT_CAP_INFO_SHORT_GI_160_80P80) {
			DBGLOG(RLM, TRACE, "VHT_CAP_INFO_SHORT_GI_160_80P80\n");
			ucSgi = 1;
		}
#endif
	}

	if (ucTxMode == TX_RATE_MODE_HE_SU) {
		DBGLOG(SW4, TRACE, "TX_RATE_MODE_HE_SU\n");
		ucSgi = 0;
	}

	/* get antenna number */
	prBssDesc = aisGetTargetBssDesc(prAdapter, prBssInfo->ucBssIndex);
	ucNss = wlanGetSupportNss(prAdapter, prBssInfo->ucBssIndex);
	if (prBssDesc) {
		ucApNss = bssGetRxNss(prBssDesc);
		if (ucApNss > 0 && ucApNss < ucNss)
			ucNss = ucApNss;
	}
	if ((ucNss < 1) || (ucNss > 3)) {
		DBGLOG(RLM, ERROR, "error ucNss: %u\n", ucNss);
		goto errhandle;
	}

	DBGLOG(SW4, TRACE,
	       "txmode=[%u], mcs idx=[%u], bandwidth=[%u], sgi=[%u], nsts=[%u]\n",
	       ucTxMode, ucMcsIdx, ucBw, ucSgi, ucNss
	);

	if (wlanQueryRateByTable(ucTxMode, ucMcsIdx, ucBw, ucSgi,
				ucNss, pu4CurRate, pu4MaxRate) < 0)
		goto errhandle;

	DBGLOG(SW4, TRACE,
	       "*pu4CurRate=[%u], *pu4MaxRate=[%u]\n",
	       *pu4CurRate, *pu4MaxRate);

	return 0;

errhandle:
	DBGLOG(SW4, ERROR,
	       "txmode=[%u], mcs idx=[%u], bandwidth=[%u], sgi=[%u], nsts=[%u]\n",
	       ucTxMode, ucMcsIdx, ucBw, ucSgi, ucNss);
	return -1;
}
#endif /* CFG_REPORT_MAX_TX_RATE */

/**
 * wlanGetRxRate - Get RX rate from information in RXV
 */
static int wlanGetRxRate(struct GLUE_INFO *prGlueInfo, uint32_t *prRxV,
		uint32_t *pu4CurRate, uint32_t *pu4MaxRate,
		struct RxRateInfo *prRxRateInfo)
{
	struct ADAPTER *prAdapter;
	struct RxRateInfo rRxRateInfo = {0};
	int32_t rv;
#if CFG_SUPPORT_LINK_QUALITY_MONITOR
	struct CHIP_DBG_OPS *prChipDbg;
#endif

	if (pu4CurRate)
		*pu4CurRate = 0;
	if (pu4MaxRate)
		*pu4MaxRate = 0;
	if (prRxRateInfo)
		*prRxRateInfo = (const struct RxRateInfo){0};
	prAdapter = prGlueInfo->prAdapter;

#if CFG_SUPPORT_LINK_QUALITY_MONITOR
	prChipDbg = prAdapter->chip_info->prDebugOps;

	if (prChipDbg && prChipDbg->get_rx_rate_info) {
		rv = prChipDbg->get_rx_rate_info(prRxV, &rRxRateInfo);

		if (rv < 0)
			goto errhandle;
	}
#else
	goto errhandle;
#endif

	if (prRxRateInfo)
		*prRxRateInfo = rRxRateInfo;

	rv = wlanQueryRateByTable(rRxRateInfo.u4Mode, rRxRateInfo.u4Rate,
				rRxRateInfo.u4Bw, rRxRateInfo.u4Gi,
				rRxRateInfo.u4Nss, pu4CurRate, pu4MaxRate);
	if (rv < 0)
		goto errhandle;

	return 0;

errhandle:
	DBGLOG(SW4, TRACE,
		"rxmode=[%u], rate=[%u], frmode=[%u], sgi=[%u], nss=[%u]\n",
		rRxRateInfo.u4Mode, rRxRateInfo.u4Rate, rRxRateInfo.u4Bw,
		rRxRateInfo.u4Gi, rRxRateInfo.u4Nss);
	return -1;
}

/**
 * wlanGetRxRateByBssid() - Get the RX rate in last cached RXV data
 *			    (unit: 0.1Mbps).
 * @prGlueInfo: Pointer to GLUE info
 * @ucBssIdx: BSS index to query by parsing RX rate from RXV saved in SatRec.
 * @pu4CurRate: Returning current rate for given RX rate parameters if non-NULL
 *              pointer is passed-in
 * @pu4MaxRate: Returning max supported rate for given RX parameters with max
 *              MCS index if non-NULL pointer is passed-in
 * @prRxRateInfo: Pointer to structure returning RxRateInfo.
 *		If caller want to get RxRateInfo parameters, call with a valid
 *		pointer to returning structure.
 *		  u4Mode: Returning RX mode
 *		  u4Nss: Returning NSS [1..]
 *		  u4Bw: Returning bandwidth
 *		  u4Gi: Returning guard interval
 *		  u4Rate: Returning MCS index
 *		Check wlanQueryRateByTable() for returning values.
 *
 * This function gets RX rate from the last cached RX rate from RXV saved in
 *     prAdapter->arStaRec[i].u4RxV[*].
 *     The u4RxV were saved earlier on calling
 *     asicConnac2xRxProcessRxvforMSP() in nicRxIndicatePackets() or on calling
 *     nicRxProcessRxReport() on receiving RX_PKT_TYPE_RX_REPORT.
 *
 *     The two functions filling u4RxV are mutual exclusive.
 *     In the chip supporting getting the rate from RX report,
 *     CONNAC2X_RXV_FROM_RX_RPT(prAdapter) returns TRUE, the values will not be
 *     filled in asicConnac2xRxProcessRxvforMSP().
 *
 * Return:0 on success, -1 on failure.
 *	The caller shall pass valid pointers in the arguments of interested
 *	results.
 */
int wlanGetRxRateByBssid(struct GLUE_INFO *prGlueInfo, uint8_t ucBssIdx,
		uint32_t *pu4CurRate, uint32_t *pu4MaxRate,
		struct RxRateInfo *prRxRateInfo)
{
	struct ADAPTER *prAdapter;
	struct STA_RECORD *prStaRec;
	uint32_t *prRxV = NULL; /* pointer to stored RxV */
	uint8_t ucWlanIdx;
	uint8_t ucStaIdx;

	prAdapter = prGlueInfo->prAdapter;

	if (!IS_BSS_INDEX_AIS(prAdapter, ucBssIdx))
		return -1;

	prStaRec = aisGetStaRecOfAP(prAdapter, ucBssIdx);
	if (prStaRec) {
		ucWlanIdx = prStaRec->ucWlanIndex;
	} else {
		DBGLOG(SW4, ERROR, "prStaRecOfAP is null\n");
		return -1;
	}

	if (wlanGetStaIdxByWlanIdx(prAdapter, ucWlanIdx, &ucStaIdx) ==
		WLAN_STATUS_SUCCESS) {
		prRxV = prAdapter->arStaRec[ucStaIdx].au4RxV;
	} else {
		DBGLOG_LIMITED(SW4, ERROR, "wlanGetStaIdxByWlanIdx fail\n");
		return -1;
	}

	return wlanGetRxRate(prGlueInfo, prRxV,
			pu4CurRate, pu4MaxRate, prRxRateInfo);
}

#if CFG_SUPPORT_LINK_QUALITY_MONITOR
uint32_t wlanLinkQualityMonitor(struct GLUE_INFO *prGlueInfo, bool bFgIsOid)
{
	struct ADAPTER *prAdapter;
	struct WIFI_LINK_QUALITY_INFO *prLinkQualityInfo = NULL;
#if (CFG_SUPPORT_STATS_ONE_CMD == 0)
	struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics;
#else
	uint32_t u4QueryInfoLen;
#endif
	struct PARAM_802_11_STATISTICS_STRUCT *prStat;
	uint8_t arBssid[PARAM_MAC_ADDR_LEN];
	uint32_t u4Status = WLAN_STATUS_FAILURE;
	uint8_t ucBssIndex;
	struct BSS_INFO *prBssInfo;

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(SW4, ERROR, "prAdapter is null\n");
		return u4Status;
	}

	ucBssIndex = aisGetDefaultLinkBssIndex(prAdapter);
	if (kalGetMediaStateIndicated(prGlueInfo, ucBssIndex) !=
	    MEDIA_STATE_CONNECTED) {
		/* not connected */
		DBGLOG(SW4, ERROR, "not yet connected\n");
		return u4Status;
	}

	/* Completely record the Link quality and store the current time */
	prAdapter->u4LastLinkQuality = kalGetTimeTick();
	DBGLOG(NIC, TRACE, "LastLinkQuality:%u\n",
			prAdapter->u4LastLinkQuality);

	kalMemZero(arBssid, MAC_ADDR_LEN);
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);
	if (!prBssInfo)
		return u4Status;
	COPY_MAC_ADDR(arBssid, prBssInfo->aucBSSID);

	prStat = &(prAdapter->rStat);

	kalMemZero(prStat, sizeof(struct PARAM_802_11_STATISTICS_STRUCT));

#if (CFG_SUPPORT_STATS_ONE_CMD == 1)
	u4Status = wlanQueryStatsOneCmd(prAdapter,
				NULL,
				0,
				&u4QueryInfoLen,
				FALSE);
	DBGLOG(REQ, TRACE,
			"u4Status=%u", u4Status);
#else
	prQueryStaStatistics = &(prAdapter->rQueryStaStatistics);
	COPY_MAC_ADDR(prQueryStaStatistics->aucMacAddr, arBssid);
	prQueryStaStatistics->ucReadClear = TRUE;
	DBGLOG(REQ, TRACE, "Call prQueryStaStatistics=%p, u4BufLen=%p",
			prQueryStaStatistics, &prAdapter->u4BufLen);
	u4Status = wlanQueryStaStatistics(prAdapter,
				prQueryStaStatistics,
				sizeof(struct PARAM_GET_STA_STATISTICS),
				&(prAdapter->u4BufLen),
				FALSE);
	DBGLOG(REQ, TRACE,
			"u4Status=%u, prQueryStaStatistics=%p, u4BufLen=%p",
			u4Status, prQueryStaStatistics,
			&prAdapter->u4BufLen);

	u4Status = wlanQueryStatistics(prAdapter,
				prStat,
				sizeof(struct PARAM_802_11_STATISTICS_STRUCT),
				&(prAdapter->u4BufLen),
				FALSE);

#endif

	if (bFgIsOid == FALSE)
		u4Status = WLAN_STATUS_SUCCESS;

	if ((bFgIsOid == TRUE) || (prAdapter->u4LastLinkQuality <= 0))
		return u4Status;

	prLinkQualityInfo = &(prAdapter->rLinkQualityInfo);

#if (CFG_SUPPORT_DATA_STALL && CFG_SUPPORT_LINK_QUALITY_MONITOR)
	wlanCustomMonitorFunction(prAdapter, prLinkQualityInfo, ucBssIndex);
#endif

	DBGLOG(SW4, INFO,
	       "Link Quality: Tx(rate:%u, total:%lu, retry:%lu, fail:%lu, RTS fail:%lu, ACK fail:%lu), Rx(rate:%u, total:%lu, dup:%u, error:%lu), PER(%u), Congestion(idle slot:%lu, diff:%lu, AwakeDur:%u)\n",
	       prLinkQualityInfo->u4CurTxRate, /* current tx link speed */
	       prLinkQualityInfo->u8TxTotalCount, /* tx total packages */
	       prLinkQualityInfo->u8TxRetryCount, /* tx retry count */
	       prLinkQualityInfo->u8TxFailCount, /* tx fail count */
	       prLinkQualityInfo->u8TxRtsFailCount, /* tx RTS fail count */
	       prLinkQualityInfo->u8TxAckFailCount, /* tx ACK fail count */
	       prLinkQualityInfo->u4CurRxRate, /* current rx link speed */
	       prLinkQualityInfo->u8RxTotalCount, /* rx total packages */
	       prLinkQualityInfo->u4RxDupCount, /* rx duplicate package count */
	       prLinkQualityInfo->u8RxErrCount, /* rx fcs fail count */
	       prLinkQualityInfo->u4CurTxPer, /* current Tx PER */
	       /* congestion stats */
	       prLinkQualityInfo->u8IdleSlotCount, /* idle slot */
	       prLinkQualityInfo->u8DiffIdleSlotCount, /* idle slot diff */
	       prLinkQualityInfo->u4HwMacAwakeDuration
	);

	return u4Status;
}

void wlanFinishCollectingLinkQuality(struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter;
	struct WIFI_LINK_QUALITY_INFO *prLinkQualityInfo = NULL;
	uint32_t u4CurRxRate, u4MaxRxRate;
	uint64_t u8TxFailCntDif, u8TxTotalCntDif;

	prAdapter = prGlueInfo->prAdapter;
	if (prAdapter == NULL) {
		DBGLOG(SW4, ERROR, "prAdapter is null\n");
		return;
	}

	/* prepare to set/get statistics from BSSInfo's rLinkQualityInfo */
	prLinkQualityInfo = &(prAdapter->rLinkQualityInfo);

	/* Calculate current tx PER */
	u8TxTotalCntDif = (prLinkQualityInfo->u8TxTotalCount >
			   prLinkQualityInfo->u8LastTxTotalCount) ?
			  (prLinkQualityInfo->u8TxTotalCount -
			   prLinkQualityInfo->u8LastTxTotalCount) : 0;
	u8TxFailCntDif = (prLinkQualityInfo->u8TxFailCount >
			  prLinkQualityInfo->u8LastTxFailCount) ?
			 (prLinkQualityInfo->u8TxFailCount -
			  prLinkQualityInfo->u8LastTxFailCount) : 0;
	if (u8TxTotalCntDif >= u8TxFailCntDif)
		prLinkQualityInfo->u4CurTxPer = (u8TxTotalCntDif == 0) ? 0 :
			kal_div64_u64(u8TxFailCntDif * 100, u8TxTotalCntDif);
	else
		prLinkQualityInfo->u4CurTxPer = 0;

	/* Calculate idle slot diff */
	if (prLinkQualityInfo->u8IdleSlotCount <
			prLinkQualityInfo->u8LastIdleSlotCount) {
		prLinkQualityInfo->u8DiffIdleSlotCount = 0;
		DBGLOG(NIC, WARN, "idle slot is error\n");
	} else
		prLinkQualityInfo->u8DiffIdleSlotCount =
			prLinkQualityInfo->u8IdleSlotCount -
			prLinkQualityInfo->u8LastIdleSlotCount;

	/* get current rx rate */
	if (wlanGetRxRateByBssid(prGlueInfo,
			aisGetDefaultLinkBssIndex(prAdapter),
			&u4CurRxRate, &u4MaxRxRate, NULL) < 0)
		prLinkQualityInfo->u4CurRxRate = 0;
	else
		prLinkQualityInfo->u4CurRxRate = u4CurRxRate;

	prLinkQualityInfo->u8LastTxTotalCount =
					prLinkQualityInfo->u8TxTotalCount;
	prLinkQualityInfo->u8LastTxFailCount =
					prLinkQualityInfo->u8TxFailCount;
	prLinkQualityInfo->u8LastIdleSlotCount =
					prLinkQualityInfo->u8IdleSlotCount;
}
#endif /* CFG_SUPPORT_LINK_QUALITY_MONITOR */

#if (CFG_SUPPORT_DATA_STALL && CFG_SUPPORT_LINK_QUALITY_MONITOR)
void wlanCustomMonitorFunction(struct ADAPTER *prAdapter,
	 struct WIFI_LINK_QUALITY_INFO *prLinkQualityInfo, uint8_t ucBssIdx)
{
	uint64_t u8TxTotalCntDif;

	u8TxTotalCntDif = (prLinkQualityInfo->u8TxTotalCount >
			   prLinkQualityInfo->u8LastTxTotalCount) ?
			  (prLinkQualityInfo->u8TxTotalCount -
			   prLinkQualityInfo->u8LastTxTotalCount) : 0;

	/* Add custom monitor here */
	if (u8TxTotalCntDif >= prAdapter->rWifiVar.u4TrafficThreshold) {
		if (prLinkQualityInfo->u4CurTxRate <
			prAdapter->rWifiVar.u4TxLowRateThreshole)
			KAL_REPORT_ERROR_EVENT(prAdapter,
				EVENT_TX_LOW_RATE,
				(uint16_t)sizeof(uint32_t),
				ucBssIdx,
				FALSE);
		else if (prLinkQualityInfo->u4CurRxRate <
			prAdapter->rWifiVar.u4RxLowRateThreshole)
			KAL_REPORT_ERROR_EVENT(prAdapter,
				EVENT_RX_LOW_RATE,
				(uint16_t)sizeof(uint32_t),
				ucBssIdx,
				FALSE);
		else if (prLinkQualityInfo->u4CurTxPer >
			prAdapter->rWifiVar.u4PerHighThreshole)
			KAL_REPORT_ERROR_EVENT(prAdapter,
				EVENT_PER_HIGH,
				(uint16_t)sizeof(uint32_t),
				ucBssIdx,
				FALSE);
	}
}
#endif

uint8_t wlanCheckExtCapBit(struct STA_RECORD *prStaRec, uint8_t *pucIE,
		uint8_t ucTargetBit)
{
	uint8_t *pucIeExtCap;
	uint8_t ucTargetByteIndex = (ucTargetBit >> 3);
	uint8_t ucTargetBitInByte = ucTargetBit % 8;

	if ((prStaRec == NULL) || (pucIE == NULL))
		return FALSE;

	if (IE_ID(pucIE) != ELEM_ID_EXTENDED_CAP)
		return FALSE;

	if (IE_LEN(pucIE) < (ucTargetByteIndex + 1))
		return FALSE;

	/* parse */
	pucIeExtCap = pucIE + 2;
	/* shift to the byte we care about */
	pucIeExtCap += ucTargetByteIndex;

	if ((*pucIeExtCap) & BIT(ucTargetBitInByte))
		return TRUE;

	return FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This routine is called to set enable/disable force RTS mode to FW
 *
 * \param[in]  prAdapter       A pointer to the Adapter structure.
 *
 * \retval WLAN_STATUS_SUCCESS
 */
/*----------------------------------------------------------------------------*/
uint32_t wlanSetForceRTS(
	struct ADAPTER *prAdapter,
	u_int8_t fgEnForceRTS)
{
	struct CMD_SET_FORCE_RTS rForceRts = {0};

	rForceRts.ucForceRtsEn = fgEnForceRTS;
	rForceRts.ucRtsPktNum = 0;
	DBGLOG(REQ, INFO, "fgEnForceRTS = %d\n",
			fgEnForceRTS);
	wlanSendSetQueryCmd(prAdapter,	/* prAdapter */
			    CMD_ID_SET_FORCE_RTS,	/* ucCID */
			    TRUE,	/* fgSetQuery */
			    FALSE,	/* fgNeedResp */
			    FALSE,	/* fgIsOid */
			    NULL,	/* pfCmdDoneHandler */
			    NULL,	/* pfCmdTimeoutHandler */
			    sizeof(struct CMD_SET_FORCE_RTS),
			    (uint8_t *)&rForceRts,	/* pucInfoBuffer */
			    NULL,	/* pvSetQueryBuffer */
			    0	/* u4SetQueryBufferLen */
	);

	return WLAN_STATUS_SUCCESS;
}

void
wlanLoadDefaultCustomerSetting(struct ADAPTER *
	prAdapter)
{

	uint8_t ucItemNum, i;

	/* default setting*/
	ucItemNum = (sizeof(g_rDefaulteSetting) / sizeof(
		struct PARAM_CUSTOM_KEY_CFG_STRUCT));

	DBGLOG(INIT, STATE, "Default firmware setting %d item\n",
			ucItemNum);


	for (i = 0; i < ucItemNum; i++) {
		wlanCfgSet(prAdapter,
			g_rDefaulteSetting[i].aucKey,
			g_rDefaulteSetting[i].aucValue,
			g_rDefaulteSetting[i].u4Flag);
		DBGLOG(INIT, TRACE, "%s with %s\n",
			g_rDefaulteSetting[i].aucKey,
			g_rDefaulteSetting[i].aucValue);
	}


#if 1
	/*If need to re-parsing , included wlanInitFeatureOption*/
	wlanInitFeatureOption(prAdapter);
#endif
}
/*wlan on*/
void
wlanResoreEmCfgSetting(struct ADAPTER *
	prAdapter)
{
	uint32_t i;

	for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {

		if (g_rEmCfgBk[i].aucKey[0] == '\0')
			continue;

		wlanCfgSet(prAdapter, g_rEmCfgBk[i].aucKey,
			g_rEmCfgBk[i].aucValue, WLAN_CFG_EM);

		DBGLOG(INIT, STATE,
			   "cfg restore:(%s,%s) op:%d\n",
			   g_rEmCfgBk[i].aucKey,
			   g_rEmCfgBk[i].aucValue,
			   g_rEmCfgBk[i].u4Flag);

	}

}

/*wlan off*/
void
wlanBackupEmCfgSetting(struct ADAPTER *
	prAdapter)
{
	uint32_t i;
	struct WLAN_CFG_ENTRY *prWlanCfgEntry = NULL;

	kalMemZero(&g_rEmCfgBk, sizeof(g_rEmCfgBk));

	for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {
		prWlanCfgEntry =
			wlanCfgGetEntryByIndex(prAdapter, i, WLAN_CFG_EM);

		if ((!prWlanCfgEntry) || (prWlanCfgEntry->aucKey[0] == '\0'))
			break;


		kalStrnCpy(g_rEmCfgBk[i].aucKey, prWlanCfgEntry->aucKey,
			WLAN_CFG_KEY_LEN_MAX - 1);
		prWlanCfgEntry->aucKey[WLAN_CFG_KEY_LEN_MAX - 1] = '\0';

		kalStrnCpy(g_rEmCfgBk[i].aucValue, prWlanCfgEntry->aucValue,
			WLAN_CFG_VALUE_LEN_MAX - 1);
		prWlanCfgEntry->aucValue[WLAN_CFG_VALUE_LEN_MAX - 1] = '\0';


		g_rEmCfgBk[i].u4Flag = WLAN_CFG_EM;

		DBGLOG(INIT, STATE,
			   "cfg backup:(%s,%s) op:%d\n",
			   g_rEmCfgBk[i].aucKey,
			   g_rEmCfgBk[i].aucValue,
			   g_rEmCfgBk[i].u4Flag);

	}


}

void
wlanCleanAllEmCfgSetting(struct ADAPTER *
	prAdapter)
{
	uint32_t i;
	struct WLAN_CFG_ENTRY *prWlanCfgEntry = NULL;

	for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {
		prWlanCfgEntry =
			wlanCfgGetEntryByIndex(prAdapter, i, WLAN_CFG_EM);

		if ((!prWlanCfgEntry) || (prWlanCfgEntry->aucKey[0] == '\0'))
			break;

		DBGLOG(INIT, STATE,
			   "cfg clean:(%s,%s) op:%d\n",
			   prWlanCfgEntry->aucKey,
			   prWlanCfgEntry->aucValue,
			   prWlanCfgEntry->u4Flags);

		kalMemZero(prWlanCfgEntry, sizeof(struct WLAN_CFG_ENTRY));

	}
}

u_int8_t wlanWfdEnabled(struct ADAPTER *prAdapter)
{
#if CFG_SUPPORT_WFD
	if (prAdapter)
		return prAdapter->rWifiVar.rWfdConfigureSettings.ucWfdEnable;
#endif
	return FALSE;
}

int wlanChipConfig(struct ADAPTER *prAdapter,
	char *pcCommand, int i4TotalLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4BytesWritten = 0;
	uint32_t u4BufLen = 0;
	uint32_t u2MsgSize = 0;
	uint32_t u4CmdLen = 0;
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rChipConfigInfo = {0};

	if (prAdapter == NULL) {
		DBGLOG(REQ, ERROR, "prAdapter null");
		return -1;
	}
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

	u4CmdLen = kalStrnLen(pcCommand, i4TotalLen);

	rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_ASCII;
	rChipConfigInfo.u2MsgSize = u4CmdLen;
	kalStrnCpy(rChipConfigInfo.aucCmd, pcCommand,
		   CHIP_CONFIG_RESP_SIZE - 1);
	rChipConfigInfo.aucCmd[CHIP_CONFIG_RESP_SIZE - 1] = '\0';
	rStatus = kalIoctl(prAdapter->prGlueInfo, wlanoidQueryChipConfig,
		&rChipConfigInfo, sizeof(rChipConfigInfo), &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "%s: kalIoctl ret=%d\n", __func__,
		       rStatus);
		return -1;
	}
	rChipConfigInfo.aucCmd[CHIP_CONFIG_RESP_SIZE - 1] = '\0';

	/* Check respType */
	u2MsgSize = rChipConfigInfo.u2MsgSize;
	DBGLOG(REQ, INFO, "%s: RespTyep  %u\n", __func__,
	       rChipConfigInfo.ucRespType);
	DBGLOG(REQ, INFO, "%s: u2MsgSize %u\n", __func__,
	       rChipConfigInfo.u2MsgSize);

	if (rChipConfigInfo.ucRespType != CHIP_CONFIG_TYPE_ASCII) {
		DBGLOG(REQ, WARN, "only return as ASCII");
		return -1;
	}
	if (u2MsgSize > sizeof(rChipConfigInfo.aucCmd)) {
		DBGLOG(REQ, INFO, "%s: u2MsgSize error ret=%u\n",
		       __func__, rChipConfigInfo.u2MsgSize);
		return -1;
	}
	i4BytesWritten = snprintf(pcCommand, i4TotalLen, "%s",
		     rChipConfigInfo.aucCmd);
	return i4BytesWritten;
}

int wlanChipCommand(struct ADAPTER *prAdapter,
	char *pcCommand, int i4TotalLen)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int32_t i4BytesWritten = 0;
	uint32_t u4BufLen = 0;
	uint32_t u4CmdLen = 0;
	struct PARAM_CUSTOM_CHIP_CONFIG_STRUCT rChipConfigInfo = {0};

	if (prAdapter == NULL) {
		DBGLOG(REQ, ERROR, "prAdapter null");
		return -1;
	}
	DBGLOG(REQ, LOUD, "command is %s\n", pcCommand);

	u4CmdLen = kalStrnLen(pcCommand, i4TotalLen);

	rChipConfigInfo.ucType = CHIP_CONFIG_TYPE_WO_RESPONSE;
	rChipConfigInfo.u2MsgSize = u4CmdLen;
	kalStrnCpy(rChipConfigInfo.aucCmd, pcCommand,
		   CHIP_CONFIG_RESP_SIZE - 1);
	rChipConfigInfo.aucCmd[CHIP_CONFIG_RESP_SIZE - 1] = '\0';

	rStatus = wlanSetChipConfig(prAdapter,
				&rChipConfigInfo, sizeof(rChipConfigInfo),
				&u4BufLen, FALSE);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, ERROR, "wlan ret=%d\n", rStatus);
		i4BytesWritten = -1;
	}

	return i4BytesWritten;
}

uint32_t wlanSetRxBaSize(struct GLUE_INFO *prGlueInfo,
	int8_t i4Type, uint16_t u2BaSize)
{
	struct ADAPTER *prAdapter;
	uint32_t i;
	struct STA_RECORD *prStaRec;
	struct CMD_ADDBA_REJECT rAddbaReject;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	prAdapter = prGlueInfo->prAdapter;
#if (CFG_SUPPORT_802_11BE == 1)
	if (i4Type == WLAN_TYPE_EHT)
		prAdapter->rWifiVar.u2RxEhtBaSize = u2BaSize;
	else
#endif
#if (CFG_SUPPORT_802_11AX == 1)
	if (i4Type == WLAN_TYPE_HE)
		prAdapter->rWifiVar.u2RxHeBaSize = u2BaSize;
	else
#endif
	{
		prAdapter->rWifiVar.ucRxHtBaSize = (uint8_t) u2BaSize;
		prAdapter->rWifiVar.ucRxVhtBaSize = (uint8_t) u2BaSize;
	}

	for (i = 0; i < CFG_STA_REC_NUM; i++) {
		prStaRec = &prAdapter->arStaRec[i];
		if (prStaRec->fgIsInUse)
			cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);
	}

	rAddbaReject.fgEnable = FALSE;
	rAddbaReject.fgApply = TRUE;

	rStatus = kalIoctl(prGlueInfo,
		wlanoidSetAddbaReject, &rAddbaReject,
		sizeof(struct CMD_ADDBA_REJECT), &u4BufLen);

	DBGLOG(OID, INFO, "%s i4Type:%d BaSize:%d\n",
		__func__, i4Type, u2BaSize);

	return rStatus;
}

uint32_t wlanSetTxBaSize(struct GLUE_INFO *prGlueInfo,
	int8_t i4Type, uint16_t u2BaSize)
{
	struct ADAPTER *prAdapter;
	uint32_t i;
	struct STA_RECORD *prStaRec;
	struct CMD_TX_AMPDU rTxAmpdu;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;

	prAdapter = prGlueInfo->prAdapter;
#if (CFG_SUPPORT_802_11BE == 1)
	if (i4Type == WLAN_TYPE_EHT)
		prAdapter->rWifiVar.u2TxEhtBaSize = u2BaSize;
	else
#endif
#if (CFG_SUPPORT_802_11AX == 1)
	if (i4Type == WLAN_TYPE_HE)
		prAdapter->rWifiVar.u2TxHeBaSize = u2BaSize;
	else
#endif
		prAdapter->rWifiVar.ucTxBaSize = (uint8_t) u2BaSize;

	for (i = 0; i < CFG_STA_REC_NUM; i++) {
		prStaRec = &prAdapter->arStaRec[i];
		if (prStaRec->fgIsInUse)
			cnmStaSendUpdateCmd(prAdapter, prStaRec, NULL, FALSE);
	}

	rTxAmpdu.fgEnable = TRUE;
	rTxAmpdu.fgApply = TRUE;

	rStatus = kalIoctl(prGlueInfo, wlanoidSetTxAmpdu,
			&rTxAmpdu, sizeof(struct CMD_TX_AMPDU), &u4BufLen);

	DBGLOG(OID, INFO, "%s i4Type:%d BaSize:%d\n",
		__func__, i4Type, u2BaSize);

	return rStatus;
}

void
wlanGetTRXInfo(struct ADAPTER *prAdapter,
	struct TRX_INFO *prTRxInfo)
{
	char arQueryMib[64] = "getMibCount";
	char arQueryTRx[64] = "getTxRxCount";
	uint8_t *pucItem = NULL;
	char *pucSavedPtr = NULL;
	uint32_t u4temp = 0;
	uint32_t index = 0;

	wlanChipConfig(prAdapter, &arQueryMib[0], sizeof(arQueryMib));
	DBGLOG(REQ, INFO, "Mib:%s\n", arQueryMib);
	pucItem = (uint8_t *)kalStrtokR(&arQueryMib[0], " ", &pucSavedPtr);
	while (pucItem) {
		kalkStrtou32(pucItem, 0, &u4temp);
		*(((uint32_t *)prTRxInfo) + index) = u4temp;
		pucItem =
			(uint8_t *)kalStrtokR(NULL, " ", &pucSavedPtr);
		index++;
	}
	DBGLOG(REQ, INFO, "TxFail:%d %d, RxFail:%d %d, Retry: %d %d\n",
		prTRxInfo->u4TxFail[0], prTRxInfo->u4TxFail[1],
		prTRxInfo->u4RxFail[0], prTRxInfo->u4RxFail[1],
		prTRxInfo->u4TxHwRetry[0], prTRxInfo->u4TxHwRetry[1]);

	pucItem = NULL;
	pucSavedPtr = NULL;
	u4temp = 0;
	index = 0;
	wlanChipConfig(prAdapter, &arQueryTRx[0], sizeof(arQueryTRx));
	DBGLOG(REQ, INFO, "TRX:%s\n", arQueryTRx);
	pucItem = kalStrtokR(&arQueryTRx[0], " ", &pucSavedPtr);
	while (pucItem) {
		kalkStrtou32(pucItem, 0, &u4temp);
		if (index % 2 == 0)
			prTRxInfo->u4TxOk[index / 2] = u4temp;
		else
			prTRxInfo->u4RxOk[index / 2] = u4temp;
		pucItem = kalStrtokR(NULL, " ", &pucSavedPtr);
		index++;
	}
	DBGLOG(REQ, INFO, "TxOk:%d %d %d %d, RxOk:%d %d %d %d\n",
		prTRxInfo->u4TxOk[0], prTRxInfo->u4TxOk[1],
		prTRxInfo->u4TxOk[2], prTRxInfo->u4TxOk[3],
		prTRxInfo->u4RxOk[0], prTRxInfo->u4RxOk[1],
		prTRxInfo->u4RxOk[2], prTRxInfo->u4RxOk[3]);
}

void wlanGetChipDbgOps(struct ADAPTER *prAdapter, uint32_t **pu4Handle)
{
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(prAdapter);
	prChipInfo = prAdapter->chip_info;
	ASSERT(prChipInfo);

	*pu4Handle = (uint32_t *)(prChipInfo->prDebugOps);
}

#if (CFG_WOW_SUPPORT == 1)
/*----------------------------------------------------------------------------*/
/*!
 * \brief This is a routine, which is used to release tx cmd after bus suspend
 *
 * \param prAdapter      Pointer of Adapter Data Structure
 */
/*----------------------------------------------------------------------------*/
void wlanReleaseAllTxCmdQueue(struct ADAPTER *prAdapter)
{
	if (prAdapter == NULL)
		return;

	/* dump queue info before release for debug */
	cmdBufDumpCmdQueue(&prAdapter->rPendingCmdQueue,
				   "waiting response CMD queue");
#if CFG_SUPPORT_MULTITHREAD
	cmdBufDumpCmdQueue(&prAdapter->rTxCmdQueue,
				   "Tx CMD queue");
#endif

	DBGLOG(OID, INFO, "Remove all pending Cmd\n");
	/* 1: Clear Pending OID */
	wlanReleasePendingOid(prAdapter, 1);

	/* Release all CMD/MGMT/CmdData frame in command queue */
	kalClearCommandQueue(prAdapter->prGlueInfo, TRUE);

	/* Release all CMD in pending command queue */
	wlanClearPendingCommandQueue(prAdapter);

#if CFG_SUPPORT_MULTITHREAD

	/* Flush all items in queues for multi-thread */
	wlanClearTxCommandQueue(prAdapter);

	wlanClearTxCommandDoneQueue(prAdapter);

#endif
}

/**
 * wlanSetTxDelayOverLimitReport - configure TX delay over limit report
 *
 * @enable: switching the report on/off
 * @isAverage: choice of report types, average or immediate
 * @interval: interval for counting average report (ms)
 * @driver_limit: driver delay limit triggering indication (ms)
 * @mac_limit: MAC delay limit triggering indication (ms)
 */
int wlanSetTxDelayOverLimitReport(struct ADAPTER *prAdapter,
		bool enable, bool isAverage,
		uint32_t interval, uint32_t driver_limit, uint32_t mac_limit)
{
	int32_t rStatus = WLAN_STATUS_CAPS_UNSUPPORTED;
#if CFG_SUPPORT_TX_LATENCY_STATS
	struct TX_DELAY_OVER_LIMIT_REPORT_STATS *prTxDelayOverLimitStats;

	prTxDelayOverLimitStats = &prAdapter->rTxDelayOverLimitStats;
	prTxDelayOverLimitStats->fgTxDelayOverLimitReportEnabled = enable;
	prTxDelayOverLimitStats->eTxDelayOverLimitStatsType =
			isAverage ? REPORT_AVERAGE : REPORT_IMMEDIATE;
	prTxDelayOverLimitStats->fgTxDelayOverLimitReportInterval = interval;
	prTxDelayOverLimitStats->u4DelayLimit[DRIVER_DELAY] = driver_limit;
	prTxDelayOverLimitStats->u4DelayLimit[MAC_DELAY] = mac_limit;

	DBGLOG(INIT, INFO, "en=%u, type=%u, interval=%u, limit=%u/%u",
		prTxDelayOverLimitStats->fgTxDelayOverLimitReportEnabled,
		prTxDelayOverLimitStats->eTxDelayOverLimitStatsType,
		prTxDelayOverLimitStats->fgTxDelayOverLimitReportInterval,
		prTxDelayOverLimitStats->u4DelayLimit[DRIVER_DELAY],
		prTxDelayOverLimitStats->u4DelayLimit[MAC_DELAY]);

	rStatus = WLAN_STATUS_SUCCESS;
#endif
	return rStatus;
}

void wlanSetConnsysFwLog(struct ADAPTER *prAdapter)
{
#ifdef CFG_MTK_CONNSYS_DEDICATED_LOG_PATH
	struct CMD_CONNSYS_FW_LOG rFwLogCmd;
	uint32_t u4BufLen;
#endif
	int32_t u4LogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;

	/* Enable FW log */
	wlanDbgGetGlobalLogLevel(
		ENUM_WIFI_LOG_MODULE_FW, &u4LogLevel);
	if (u4LogLevel > ENUM_WIFI_LOG_LEVEL_DEFAULT)
		wlanDbgSetLogLevel(prAdapter,
			ENUM_WIFI_LOG_LEVEL_VERSION_V1,
			ENUM_WIFI_LOG_MODULE_FW,
			u4LogLevel, TRUE);

#ifdef CFG_MTK_CONNSYS_DEDICATED_LOG_PATH
	kalMemZero(&rFwLogCmd, sizeof(rFwLogCmd));

	rFwLogCmd.fgCmd = (int)FW_LOG_CMD_ON_OFF;
	rFwLogCmd.fgValue = getFWLogOnOff();
	rFwLogCmd.fgEarlySet = TRUE;

	connsysFwLogControl(prAdapter,
		&rFwLogCmd,
		sizeof(struct CMD_CONNSYS_FW_LOG),
		&u4BufLen);

	if (getFWLogLevel() != -1) {
		rFwLogCmd.fgCmd =
			(int)FW_LOG_CMD_SET_LEVEL;
		rFwLogCmd.fgValue = getFWLogLevel();
		rFwLogCmd.fgEarlySet = TRUE;

		connsysFwLogControl(prAdapter,
		&rFwLogCmd,
		sizeof(struct CMD_CONNSYS_FW_LOG),
		&u4BufLen);
	}
#endif
}

void
wlanWaitCfg80211SuspendDone(struct GLUE_INFO *prGlueInfo)
{
	uint8_t u1Count = 0;

	if (prGlueInfo->prAdapter == NULL)
		return;

	while (!(KAL_TEST_BIT(SUSPEND_FLAG_CLEAR_WHEN_RESUME,
		prGlueInfo->prAdapter->ulSuspendFlag))) {
		if (u1Count > HIF_SUSPEND_MAX_WAIT_TIME) {
			DBGLOG(HAL, ERROR, "cfg80211 not suspend\n");
			/* no cfg80211 suspend called, do pre-suspend flow here */
			aisPreSuspendFlow(prGlueInfo->prAdapter);
			p2pRoleProcessPreSuspendFlow(prGlueInfo->prAdapter);
			break;
		}
		kalUsleep_range(5000, 6000);
		u1Count++;
		DBGLOG(HAL, TRACE, "wait cfg80211 suspend %d\n", u1Count);
	}
}
#endif /* #if (CFG_WOW_SUPPORT == 1) */

uint32_t wlanSendFwLogControlCmd(struct ADAPTER *prAdapter,
				uint8_t ucCID,
				PFN_CMD_DONE_HANDLER pfCmdDoneHandler,
				PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
				uint32_t u4SetQueryInfoLen,
				int8_t *pucInfoBuffer)
{
#ifdef CFG_SUPPORT_UNIFIED_COMMAND
	return wlanSendSetQueryCmdHelper(
#else
	return wlanSendSetQueryCmdAdv(
#endif
		prAdapter, ucCID, 0, TRUE, FALSE, FALSE,
		pfCmdDoneHandler, pfCmdTimeoutHandler, u4SetQueryInfoLen,
		pucInfoBuffer, NULL, 0, CMD_SEND_METHOD_REQ_RESOURCE);
}

#if (CFG_SUPPORT_DYNAMIC_EDCCA == 1)
uint32_t wlanSetEd(struct ADAPTER *prAdapter, int32_t i4EdVal2G,
	int32_t i4EdVal5G, uint32_t u4Sel)
{
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo = prAdapter->prGlueInfo;
	struct PARAM_CUSTOM_SW_CTRL_STRUCT rSwCtrlInfo;

	rSwCtrlInfo.u4Id = CMD_SW_DBGCTL_ADVCTL_SET_ID + CMD_ADVCTL_ED_ID;
	rSwCtrlInfo.u4Data = ((i4EdVal2G & 0xFF) |
		((i4EdVal5G & 0xFF)<<16) | (u4Sel << 31));

	DBGLOG(REQ, INFO, "rSwCtrlInfo.u4Data=0x%x,\n", rSwCtrlInfo.u4Data);

	return kalIoctl(prGlueInfo, wlanoidSetSwCtrlWrite, &rSwCtrlInfo,
		sizeof(rSwCtrlInfo), &u4BufLen);
}
#endif

#if (CFG_WIFI_GET_MCS_INFO == 1)
void wlanRxMcsInfoMonitor(struct ADAPTER *prAdapter,
					    uintptr_t ulParamPtr)
{
	static uint8_t ucSmapleCnt;
	uint8_t ucBssIdx = 0;
	struct STA_RECORD *prStaRec;

	ucBssIdx = GET_IOCTL_BSSIDX(prAdapter);
	prStaRec = aisGetTargetStaRec(prAdapter, ucBssIdx);

	if (prStaRec == NULL)
		goto out;

	if (prStaRec->fgIsValid && prStaRec->fgIsInUse) {
		prStaRec->au4RxV0[ucSmapleCnt] = prStaRec->au4RxV[0];
		prStaRec->au4RxV1[ucSmapleCnt] = prStaRec->au4RxV[1];
		prStaRec->au4RxV2[ucSmapleCnt] = prStaRec->au4RxV[2];

		ucSmapleCnt = (ucSmapleCnt + 1) % MCS_INFO_SAMPLE_CNT;
	}

out:
	cnmTimerStartTimer(prAdapter, &prAdapter->rRxMcsInfoTimer,
			   MCS_INFO_SAMPLE_PERIOD);
}
#endif /* CFG_WIFI_GET_MCS_INFO */

uint32_t wlanQueryThermalTemp(struct ADAPTER *ad,
	struct THERMAL_TEMP_DATA *data)
{
	struct GLUE_INFO *glue = ad->prGlueInfo;
	PFN_OID_HANDLER_FUNC handler = NULL;
	uint32_t status = WLAN_STATUS_SUCCESS;
	uint32_t len = 0;

	if (!data)
		return WLAN_STATUS_FAILURE;

	if (data->eType == THERMAL_TEMP_TYPE_ADIE)
		handler = wlanoidQueryThermalAdieTemp;
	else
		handler = wlanoidQueryThermalDdieTemp;

	status = kalIoctl(glue, handler, data, sizeof(*data), &len);

	return status;
}

