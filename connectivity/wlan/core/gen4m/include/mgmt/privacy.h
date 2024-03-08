/******************************************************************************
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
 *****************************************************************************/
/*
 * Id: include/mgmt/privacy.h#1
 */

/*! \file   privacy.h
 *  \brief This file contains the function declaration for privacy.c.
 */


#ifndef _PRIVACY_H
#define _PRIVACY_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* 0: unicast, 1-3: GTK, 4-5: IGTK*/
#define MAX_KEY_NUM                             6
#define WEP_40_LEN                              5
#define WEP_104_LEN                             13
#define WEP_128_LEN                             16
#define LEGACY_KEY_MAX_LEN                      16
#define CCMP_KEY_LEN                            16
#define TKIP_KEY_LEN                            32
#define MAX_KEY_LEN                             32
#define MIC_RX_KEY_OFFSET                       16
#define MIC_TX_KEY_OFFSET                       24
#define MIC_KEY_LEN                             8

#define WEP_KEY_ID_FIELD      BITS(0, 29)
#define KEY_ID_FIELD          BITS(0, 7)

#define IS_TRANSMIT_KEY       BIT(31)
#define IS_UNICAST_KEY        BIT(30)
#define IS_AUTHENTICATOR      BIT(28)

/* WTBL cipher selector, sync with HAL RX from hal_hw_def_rom.h */
#define CIPHER_SUITE_NONE               0
#define CIPHER_SUITE_WEP40              1
#define CIPHER_SUITE_TKIP               2
#define CIPHER_SUITE_TKIP_WO_MIC        3
#define CIPHER_SUITE_CCMP               4
#define CIPHER_SUITE_WEP104             5
#define CIPHER_SUITE_BIP                6
#define CIPHER_SUITE_BIP_CMAC_128       CIPHER_SUITE_BIP
#define CIPHER_SUITE_WEP128             7
#define CIPHER_SUITE_WPI                8
#define CIPHER_SUITE_CCMP_W_CCX         9 /* CCMP-128 for DFP or CCX MFP */
#define CIPHER_SUITE_CCMP_256           10
#define CIPHER_SUITE_GCMP_128           11
#define CIPHER_SUITE_GCMP_256           12
#define CIPHER_SUITE_GCM_WPI_128        13
#define CIPHER_SUITE_BIP_CMAC_256       14
#define CIPHER_SUITE_BCN_PROT_CMAC_128  15
#define CIPHER_SUITE_BCN_PROT_CMAC_256  16
#define CIPHER_SUITE_BIP_GMAC_256       17

/* Todo:: Move to register */
#if defined(MT6630)
#define WTBL_RESERVED_ENTRY             255
#else
#define WTBL_RESERVED_ENTRY             255
#endif
/* Todo:: By chip capability */
/* Max wlan table size, the max+1 used for probe request,... mgmt frame */
/*sending use basic rate and no security */
#define WTBL_SIZE                       32

#define WTBL_ALLOC_FAIL                 WTBL_RESERVED_ENTRY
#define WTBL_DEFAULT_ENTRY              0

/*******************************************************************************
 *                         D A T A   T Y P E S
 *******************************************************************************
 */

struct IEEE_802_1X_HDR {
	uint8_t ucVersion;
	uint8_t ucType;
	uint16_t u2Length;
	/* followed by length octets of data */
};

struct EAPOL_KEY {
	uint8_t ucType;
	/* Note: key_info, key_length, and key_data_length are unaligned */
	uint8_t aucKeyInfo[2];	/* big endian */
	uint8_t aucKeyLength[2];	/* big endian */
	uint8_t aucReplayCounter[8];
	uint8_t aucKeyNonce[16];
	uint8_t aucKeyIv[16];
	uint8_t aucKeyRsc[8];
	uint8_t aucKeyId[8];	/* Reserved in IEEE 802.11i/RSN */
	uint8_t aucKeyMic[16];
	uint8_t aucKeyDataLength[2];	/* big endian */
	/* followed by key_data_length bytes of key_data */
};

/* WPA2 PMKID candicate structure */
struct PMKID_CANDICATE {
	uint8_t aucBssid[MAC_ADDR_LEN];
	uint32_t u4PreAuthFlags;
};

#if 0
/* WPA2 PMKID cache structure */
struct PMKID_ENTRY {
	struct PARAM_BSSID_INFO rBssidInfo;
	u_int8_t fgPmkidExist;
};
#endif

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
/* TRUE if CM error || Unprotected UC frame */
#define IS_INCORRECT_SEC_RX_FRAME(_prSwRfb, _aucDestAddr, u2FrameCtrl) \
		(_prSwRfb->fgIsCipherMS || \
		(IS_UCAST_MAC_ADDR(_aucDestAddr) && \
			!(u2FrameCtrl & MASK_FC_PROTECTED_FRAME)))

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

void secInit(struct ADAPTER *prAdapter,
	     uint8_t ucBssIndex);

void secSetPortBlocked(struct ADAPTER *prAdapter,
		       struct STA_RECORD *prSta, u_int8_t fgPort);

u_int8_t secCheckClassError(struct ADAPTER *prAdapter,
			    struct SW_RFB *prSwRfb,
			    struct STA_RECORD *prStaRec);

u_int8_t secTxPortControlCheck(struct ADAPTER *prAdapter,
			       struct MSDU_INFO *prMsduInfo,
			       struct STA_RECORD *prStaRec);

u_int8_t secRxPortControlCheck(struct ADAPTER *prAdapter,
			       struct SW_RFB *prSWRfb);

void secSetCipherSuite(struct ADAPTER *prAdapter,
		       uint32_t u4CipherSuitesFlags,
		       uint8_t ucBssIndex);

u_int8_t secIsProtectedFrame(struct ADAPTER *prAdapter,
			     struct MSDU_INFO *prMsdu,
			     struct STA_RECORD *prStaRec);

u_int8_t secRsnKeyHandshakeEnabled(struct ADAPTER
				   *prAdapter);

uint8_t secGetBmcWlanIndex(struct ADAPTER *prAdapter,
			   enum ENUM_NETWORK_TYPE eNetType,
			   struct STA_RECORD *prStaRec);

u_int8_t secTransmitKeyExist(struct ADAPTER *prAdapter,
			     struct STA_RECORD *prSta);

u_int8_t secEnabledInAis(struct ADAPTER *prAdapter,
		uint8_t ucBssIndex);

u_int8_t secPrivacySeekForEntry(struct ADAPTER
				*prAdapter, struct STA_RECORD *prSta);

void secPrivacyFreeForEntry(struct ADAPTER *prAdapter,
			    uint8_t ucEntry);

void secPrivacyFreeSta(struct ADAPTER *prAdapter,
		       struct STA_RECORD *prStaRec);

void secRemoveBssBcEntry(struct ADAPTER *prAdapter,
			 struct BSS_INFO *prBssInfo, u_int8_t fgRoam);

uint8_t
secPrivacySeekForBcEntry(struct ADAPTER *prAdapter,
			 uint8_t ucBssIndex,
			 uint8_t *pucAddr, uint8_t ucStaIdx,
			 uint8_t ucAlg, uint8_t ucKeyId);

uint8_t secGetStaIdxByWlanIdx(struct ADAPTER *prAdapter,
			      uint8_t ucWlanIdx);

uint8_t secGetWlanIdxByStaIdx(struct ADAPTER *prAdapter,
				uint8_t ucStaIndex);

uint8_t secGetBssIdxByWlanIdx(struct ADAPTER *prAdapter,
			      uint8_t ucWlanIdx);

uint8_t secGetBssIdxByRfb(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb);

struct BSS_INFO *secGetBssByRfb(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb);

uint8_t secLookupStaRecIndexFromTA(struct ADAPTER
				   *prAdapter, uint8_t *pucMacAddress);

void secPrivacyDumpWTBL(struct ADAPTER *prAdapter);

uint8_t secCheckWTBLwlanIdxInUseByOther(struct ADAPTER *prAdapter,
			uint8_t ucWlanIdx, uint8_t ucBssIndex);

u_int8_t secCheckWTBLAssign(struct ADAPTER *prAdapter);

u_int8_t secIsProtectedBss(struct ADAPTER *prAdapter,
			   struct BSS_INFO *prBssInfo);

u_int8_t secIsRobustActionFrame(struct ADAPTER *prAdapter,
			   void *prPacket);

u_int8_t secIsRobustMgmtFrame(struct ADAPTER *prAdapter,
			void *prPacket);

u_int8_t secIsWepBss(struct ADAPTER *prAdapter,
			struct BSS_INFO *prBssInfo);

u_int8_t tkipMicDecapsulate(struct SW_RFB *prSwRfb,
			    uint8_t *pucMicKey);
u_int8_t tkipMicDecapsulateInRxHdrTransMode(
	struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb,
	uint8_t *pucMicKey);

void secPostUpdateAddr(struct ADAPTER *prAdapter,
		       struct BSS_INFO *prBssInfo);

enum ENUM_EAPOL_KEY_TYPE_T secGetEapolKeyType(
	uint8_t *pucPacket);

void secHandleNoWtbl(struct ADAPTER *prAdapter,
	struct SW_RFB *prSwRfb);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

#endif /* _PRIVACY_H */
