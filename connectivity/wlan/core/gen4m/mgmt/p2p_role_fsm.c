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
#include "precomp.h"
#include "p2p_role_state.h"
#include "gl_p2p_os.h"

#if 1
/*lint -save -e64 Type mismatch */
static uint8_t *apucDebugP2pRoleState[P2P_ROLE_STATE_NUM] = {
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_IDLE"),
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_SCAN"),
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_REQING_CHANNEL"),
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_AP_CHNL_DETECTION"),
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_GC_JOIN"),
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_OFF_CHNL_TX"),
#if (CFG_SUPPORT_DFS_MASTER == 1)
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_DFS_CAC"),
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_SWITCH_CHANNEL"),
#endif
	(uint8_t *) DISP_STRING("P2P_ROLE_STATE_WAIT_FOR_NEXT_REQ_CHNL"),
};

uint8_t *
	p2pRoleFsmGetFsmState(
	enum ENUM_P2P_ROLE_STATE eCurrentState) {
	if ((uint32_t)eCurrentState <
		P2P_ROLE_STATE_NUM)
		return apucDebugP2pRoleState[(uint32_t)eCurrentState];

	return (uint8_t *) DISP_STRING("UNKNOWN");
}

/*lint -restore */
#endif /* DBG */

void
p2pRoleFsmStateTransition(struct ADAPTER *prAdapter,
		struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		enum ENUM_P2P_ROLE_STATE eNextState);
static u_int8_t
p2pRoleFsmIsAcsProcessing(struct ADAPTER *prAdapter,
		uint8_t ucRoleIdx);
static void
p2pRoleFsmAbortCurrentAcsReq(struct ADAPTER *prAdapter,
		struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest);

#if (CFG_DBDC_SW_FOR_P2P_LISTEN == 1)
static void
p2pRoleP2pLisStopDbdcDecision(
		struct ADAPTER *prAdapter,
		enum ENUM_P2P_CONNECTION_TYPE eConnRequest);
#endif

u_int8_t p2pRoleFsmNeedMlo(
	struct ADAPTER *prAdapter,
	uint8_t ucRoleIdx)
{
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	u_int8_t fgIsApMode = p2pFuncIsAPMode(
		prAdapter->rWifiVar.prP2PConnSettings[ucRoleIdx]);

	return mldIsMloFeatureEnabled(prAdapter, NETWORK_TYPE_P2P, fgIsApMode);
#else
	return FALSE;
#endif
}

uint8_t p2pRoleFsmInit(struct ADAPTER *prAdapter,
	uint8_t ucRoleIdx)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prP2pChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;
	struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxReqInfo =
		(struct P2P_MGMT_TX_REQ_INFO *) NULL;
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	struct MLD_BSS_INFO *prMldBssInfo = NULL;
#endif

	do {
		ASSERT_BREAK(prAdapter != NULL);

		if (P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx)
			!= NULL) {
			DBGLOG(P2P, ERROR,
				"Error already init for role %d\n", ucRoleIdx);
			break;
		}

		prP2pRoleFsmInfo = kalMemAlloc(
			sizeof(struct P2P_ROLE_FSM_INFO),
			VIR_MEM_TYPE);

		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx) =
			prP2pRoleFsmInfo;

		if (!prP2pRoleFsmInfo) {
			DBGLOG(P2P, ERROR,
				"Error allocating fsm Info Structure\n");
			break;
		}

		kalMemZero(prP2pRoleFsmInfo, sizeof(struct P2P_ROLE_FSM_INFO));

		prP2pRoleFsmInfo->ucRoleIndex = ucRoleIdx;

		prP2pRoleFsmInfo->eCurrentState = P2P_ROLE_STATE_IDLE;

		prP2pRoleFsmInfo->u4P2pPacketFilter =
			PARAM_PACKET_FILTER_SUPPORTED;

		prP2pChnlReqInfo = &prP2pRoleFsmInfo->rChnlReqInfo;
		LINK_INITIALIZE(&(prP2pChnlReqInfo->rP2pChnlReqLink));

		prP2pMgmtTxReqInfo = &prP2pRoleFsmInfo->rMgmtTxInfo;
		LINK_INITIALIZE(&prP2pMgmtTxReqInfo->rTxReqLink);

		cnmTimerInitTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer),
			(PFN_MGMT_TIMEOUT_FUNC) p2pRoleFsmRunEventTimeout,
			(uintptr_t) prP2pRoleFsmInfo);

#if CFG_ENABLE_PER_STA_STATISTICS_LOG
		cnmTimerInitTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pRoleFsmGetStatisticsTimer),
			(PFN_MGMT_TIMEOUT_FUNC) p2pRoleFsmGetStaStatistics,
			(uintptr_t) prP2pRoleFsmInfo);
#endif

#if (CFG_SUPPORT_DFS_MASTER == 1)
		cnmTimerInitTimer(prAdapter,
			&(prP2pRoleFsmInfo->rDfsShutDownTimer),
			(PFN_MGMT_TIMEOUT_FUNC)
			p2pRoleFsmRunEventDfsShutDownTimeout,
			(uintptr_t) prP2pRoleFsmInfo);
#endif

		cnmTimerInitTimer(prAdapter,
			&(prP2pRoleFsmInfo->rWaitNextReqChnlTimer),
			(PFN_MGMT_TIMEOUT_FUNC)
			p2pRoleFsmRunEventWaitNextReqChnlTimeout,
			(uintptr_t) prP2pRoleFsmInfo);

		prP2pBssInfo = cnmGetBssInfoAndInit(prAdapter,
			NETWORK_TYPE_P2P, FALSE);

		if (!prP2pBssInfo) {
			DBGLOG(P2P, ERROR,
				"Error allocating BSS Info Structure\n");
			break;
		}

		BSS_INFO_INIT(prAdapter, prP2pBssInfo);
		prP2pRoleFsmInfo->ucBssIndex = prP2pBssInfo->ucBssIndex;

		/* For state identify, not really used. */
		prP2pBssInfo->eIntendOPMode = OP_MODE_P2P_DEVICE;

		/* glRegisterP2P has setup the mac address */
		/* For wlan0 as AP mode case, this function will be called when
		 * changing interface type. And the MAC Addr overwrite by Role
		 * isn't expected.
		 * Maybe only using ucRoleIdx to calc MAC addr is better than
		 * using Role type.
		 */

		kalSetP2pDevMac(prAdapter->prGlueInfo,
			prP2pBssInfo, ucRoleIdx);

		/* For BSS_INFO back trace to P2P Role & get Role FSM. */
		prP2pBssInfo->u4PrivateData = ucRoleIdx;

		if (p2pFuncIsAPMode(
			prAdapter->rWifiVar.prP2PConnSettings[ucRoleIdx])) {
			prP2pBssInfo->ucConfigAdHocAPMode = AP_MODE_11G;
			prP2pBssInfo->u2HwDefaultFixedRateCode =
				RATE_CCK_1M_LONG;
		} else {
			prP2pBssInfo->ucConfigAdHocAPMode = AP_MODE_11G_P2P;
			prP2pBssInfo->u2HwDefaultFixedRateCode = RATE_OFDM_6M;
		}

		prP2pBssInfo->ucNonHTBasicPhyType = (uint8_t)
			rNonHTApModeAttributes
				[prP2pBssInfo->ucConfigAdHocAPMode]
				.ePhyTypeIndex;

		prP2pBssInfo->u2BSSBasicRateSet =
			rNonHTApModeAttributes
				[prP2pBssInfo->ucConfigAdHocAPMode]
				.u2BSSBasicRateSet;
		prP2pBssInfo->u2OperationalRateSet =
			rNonHTPhyAttributes
				[prP2pBssInfo->ucNonHTBasicPhyType]
				.u2SupportedRateSet;

		rateGetDataRatesFromRateSet(prP2pBssInfo->u2OperationalRateSet,
			prP2pBssInfo->u2BSSBasicRateSet,
			prP2pBssInfo->aucAllSupportedRates,
			&prP2pBssInfo->ucAllSupportedRatesLen);

		prP2pBssInfo->prBeacon = cnmMgtPktAlloc(prAdapter,
			OFFSET_OF(struct WLAN_BEACON_FRAME, aucInfoElem[0])
				+ MAX_IE_LENGTH);

		if (prP2pBssInfo->prBeacon) {
			prP2pBssInfo->prBeacon->eSrc = TX_PACKET_MGMT;
			/* NULL STA_REC */
			prP2pBssInfo->prBeacon->ucStaRecIndex =
				STA_REC_INDEX_BMCAST;
			prP2pBssInfo->prBeacon->ucBssIndex =
				prP2pBssInfo->ucBssIndex;
		} else {
			/* Out of memory. */
			DBGLOG(P2P, ERROR,
				"Error allocating BSS Info Beacon\n");
			cnmFreeBssInfo(prAdapter, prP2pBssInfo);
			if (prP2pRoleFsmInfo)
				kalMemFree(prP2pRoleFsmInfo, VIR_MEM_TYPE,
					sizeof(struct P2P_ROLE_FSM_INFO));
			P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx) =
				NULL;
			return MAX_BSS_INDEX;
		}

		prP2pBssInfo->rPmProfSetupInfo.ucBmpDeliveryAC =
			(uint8_t) prAdapter->u4P2pUapsdAcBmp;
		prP2pBssInfo->rPmProfSetupInfo.ucBmpTriggerAC =
			(uint8_t) prAdapter->u4P2pUapsdAcBmp;
		prP2pBssInfo->rPmProfSetupInfo.ucUapsdSp =
			(uint8_t) prAdapter->u4P2pMaxSpLen;
		prP2pBssInfo->ucPrimaryChannel = P2P_DEFAULT_LISTEN_CHANNEL;
		prP2pBssInfo->eBand = BAND_2G4;
		prP2pBssInfo->eBssSCO = CHNL_EXT_SCN;
		prP2pBssInfo->ucOpRxNss = prP2pBssInfo->ucOpTxNss =
			wlanGetSupportNss(prAdapter, prP2pBssInfo->ucBssIndex);
#if (CFG_HW_WMM_BY_BSS == 0)
		prP2pBssInfo->ucWmmQueSet = (prAdapter->rWifiVar.eDbdcMode ==
			ENUM_DBDC_MODE_DISABLED)
			? DBDC_5G_WMM_INDEX
			: DBDC_2G_WMM_INDEX;
#endif
		if (IS_FEATURE_ENABLED(prAdapter->rWifiVar.ucQoS))
			prP2pBssInfo->fgIsQBSS = TRUE;
		else
			prP2pBssInfo->fgIsQBSS = FALSE;

		cnmTimerInitTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pCsaDoneTimer),
			(PFN_MGMT_TIMEOUT_FUNC) p2pFsmRunEventCsaDoneTimeOut,
			(uintptr_t)prP2pBssInfo);

#if (CFG_SUPPORT_DFS_MASTER == 1)
		p2pFuncRadarInfoInit();
#endif
#if CFG_SUPPORT_802_11W
		kal_init_completion(&prP2pRoleFsmInfo->rDeauthComp);
		prP2pRoleFsmInfo->encryptedDeauthIsInProcess = FALSE;
#endif

		LINK_INITIALIZE(&prP2pBssInfo->rPmkidCache);

#if (CFG_SUPPORT_802_11BE_MLO == 1)
		p2pMldBssInit(prAdapter, prP2pRoleFsmInfo);
		prMldBssInfo = prP2pRoleFsmInfo->prP2pMldBssInfo;
		if (!prMldBssInfo) {
			DBGLOG(P2P, ERROR,
				"Error allocating mld bss\n");
			break;
		}

		if (p2pRoleFsmNeedMlo(prAdapter, ucRoleIdx))
			prP2pBssInfo->ucLinkIndex = ucRoleIdx;

		mldBssRegister(prAdapter, prMldBssInfo, prP2pBssInfo);
#endif
	} while (FALSE);

	if (prP2pBssInfo)
		return prP2pBssInfo->ucBssIndex;
	else
		return prAdapter->ucP2PDevBssIdx;
}				/* p2pFsmInit */

void p2pRoleFsmUninit(struct ADAPTER *prAdapter, uint8_t ucRoleIdx)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		DEBUGFUNC("p2pRoleFsmUninit()");
		DBGLOG(P2P, INFO, "->p2pRoleFsmUninit(%d)\n", ucRoleIdx);

		prP2pRoleFsmInfo =
			P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx);

		ASSERT_BREAK(prP2pRoleFsmInfo != NULL);

		if (!prP2pRoleFsmInfo)
			return;

		prP2pBssInfo =
			prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

		p2pFuncDissolve(prAdapter,
			prP2pBssInfo, TRUE,
			REASON_CODE_DEAUTH_LEAVING_BSS,
			TRUE);

		SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);

		/* Function Dissolve should already enter IDLE state. */
		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_IDLE);

		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);

		p2pFunClearAllTxReq(prAdapter,
				&(prP2pRoleFsmInfo->rMgmtTxInfo));

		/* Clear CmdQue */
		kalClearMgmtFramesByBssIdx(prAdapter->prGlueInfo,
			prP2pBssInfo->ucBssIndex);
		kalClearCmdDataFramesByBssIdx(prAdapter->prGlueInfo,
			prP2pBssInfo->ucBssIndex);
		/* Clear PendingCmdQue */
		wlanReleasePendingCMDbyBssIdx(prAdapter,
			prP2pBssInfo->ucBssIndex);
		/* Clear PendingTxMsdu */
		nicFreePendingTxMsduInfo(prAdapter,
			prP2pBssInfo->ucBssIndex, MSDU_REMOVE_BY_BSS_INDEX);

		/* Indicate PM abort if the BSS is under deauth process */
		if (prP2pBssInfo->eConnectionState == MEDIA_STATE_CONNECTED) {
			nicPmIndicateBssAbort(prAdapter,
				prP2pRoleFsmInfo->ucBssIndex);
		}

		/* Deactivate BSS. */
		nicDeactivateNetwork(prAdapter,
			NETWORK_ID(prP2pBssInfo->ucBssIndex,
			prP2pRoleFsmInfo->ucRoleIndex));
		nicUpdateBss(prAdapter, prP2pBssInfo->ucBssIndex);

		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx) = NULL;

		if (prP2pBssInfo->prBeacon) {
			cnmMgtPktFree(prAdapter, prP2pBssInfo->prBeacon);
			prP2pBssInfo->prBeacon = NULL;
		}

#if (CFG_SUPPORT_802_11BE_MLO == 1)
		mldBssUnregister(prAdapter,
			prP2pRoleFsmInfo->prP2pMldBssInfo,
			prP2pBssInfo);

		p2pMldBssUninit(prAdapter, prP2pRoleFsmInfo);
#endif
		cnmFreeBssInfo(prAdapter, prP2pBssInfo);

		/* ensure the timer be stopped */
		cnmTimerStopTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer));

		cnmTimerStopTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pCsaDoneTimer));

#if CFG_ENABLE_PER_STA_STATISTICS_LOG
		cnmTimerStopTimer(prAdapter,
			&prP2pRoleFsmInfo->rP2pRoleFsmGetStatisticsTimer);
#endif

#if (CFG_SUPPORT_DFS_MASTER == 1)
		cnmTimerStopTimer(prAdapter,
			&(prP2pRoleFsmInfo->rDfsShutDownTimer));
#endif

		if (prP2pRoleFsmInfo)
			kalMemFree(prP2pRoleFsmInfo, VIR_MEM_TYPE,
				sizeof(struct P2P_ROLE_FSM_INFO));

		rsnFlushPmkid(prAdapter, prP2pBssInfo->ucBssIndex);
	} while (FALSE);

	return;
#if 0
	struct P2P_FSM_INFO *prP2pFsmInfo = (struct P2P_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		DEBUGFUNC("p2pFsmUninit()");
		DBGLOG(P2P, INFO, "->p2pFsmUninit()\n");

		prP2pFsmInfo = prAdapter->rWifiVar.prP2pFsmInfo;
		prP2pBssInfo =
			&(prAdapter->rWifiVar
			.arBssInfo[NETWORK_TYPE_P2P_INDEX]);

		p2pFuncSwitchOPMode(prAdapter,
			prP2pBssInfo,
			OP_MODE_P2P_DEVICE,
			TRUE);

		p2pFsmRunEventAbort(prAdapter, prP2pFsmInfo);

		p2pStateAbort_IDLE(prAdapter, prP2pFsmInfo, P2P_STATE_NUM);

		UNSET_NET_ACTIVE(prAdapter, NETWORK_TYPE_P2P_INDEX);

		wlanAcquirePowerControl(prAdapter);

		/* Release all pending CMD queue. */
		DBGLOG(P2P, TRACE,
		       "p2pFsmUninit: wlanProcessCommandQueue, num of element:%d\n",
		       prAdapter->prGlueInfo->rCmdQueue.u4NumElem);
		wlanProcessCommandQueue(prAdapter,
			&prAdapter->prGlueInfo->rCmdQueue);

		wlanReleasePowerControl(prAdapter);

		/* Release pending mgmt frame,
		 * mgmt frame may be pending by CMD without resource.
		 */
		kalClearMgmtFramesByBssIdx(prAdapter->prGlueInfo,
			NETWORK_TYPE_P2P_INDEX);

		/* Clear PendingCmdQue */
		wlanReleasePendingCMDbyBssIdx(prAdapter,
			NETWORK_TYPE_P2P_INDEX);

		if (prP2pBssInfo->prBeacon) {
			cnmMgtPktFree(prAdapter, prP2pBssInfo->prBeacon);
			prP2pBssInfo->prBeacon = NULL;
		}

	} while (FALSE);

	return;
#endif
}				/* p2pRoleFsmUninit */

void
p2pRoleFsmStateTransition(struct ADAPTER *prAdapter,
		struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		enum ENUM_P2P_ROLE_STATE eNextState)
{
	u_int8_t fgIsTransitionOut = (u_int8_t) FALSE;
	struct BSS_INFO *prP2pRoleBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;

	prP2pRoleBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter, prP2pRoleFsmInfo->ucBssIndex);
	prChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);
	if (!prP2pRoleBssInfo || !prChnlReqInfo)
		return;
	do {
		if (!IS_BSS_ACTIVE(prP2pRoleBssInfo)) {
			if (!cnmP2PIsPermitted(prAdapter))
				return;

			if ((eNextState == P2P_ROLE_STATE_IDLE &&
				prP2pRoleFsmInfo->eCurrentState ==
					P2P_ROLE_STATE_IDLE)
#if (CFG_SUPPORT_DFS_MASTER == 1)
				|| (eNextState == P2P_ROLE_STATE_SWITCH_CHANNEL)
#endif

#if CFG_SUPPORT_DBDC
				|| (cnmDBDCIsReqPeivilegeLock() &&
				((eNextState == P2P_ROLE_STATE_REQING_CHANNEL &&
					(prChnlReqInfo->eChnlReqType ==
						CH_REQ_TYPE_GO_START_BSS ||
					prChnlReqInfo->eChnlReqType ==
						CH_REQ_TYPE_JOIN))))
#endif
			) {
				/* Do not activate network ruring DBDC HW
				 * switch. Otherwise, BSS may use incorrect
				 * CR and result in TRx problems.
				 */
				 DBGLOG(P2P, STATE,
					"[P2P_ROLE][%d](Bss%d): Skip activate network [%s]\n",
					prP2pRoleFsmInfo->ucRoleIndex,
					prP2pRoleFsmInfo->ucBssIndex,
					p2pRoleFsmGetFsmState(eNextState));
			} else {
				nicActivateNetwork(prAdapter,
					NETWORK_ID(prP2pRoleBssInfo->ucBssIndex,
					prP2pRoleFsmInfo->ucRoleIndex));
			}
		}

		fgIsTransitionOut = fgIsTransitionOut ? FALSE : TRUE;

		if (!fgIsTransitionOut) {
			DBGLOG(P2P, STATE,
				"[P2P_ROLE][%d]TRANSITION(Bss%d): [%s] -> [%s]\n",
				prP2pRoleFsmInfo->ucRoleIndex,
				prP2pRoleFsmInfo->ucBssIndex,
				p2pRoleFsmGetFsmState
				(prP2pRoleFsmInfo->eCurrentState),
				p2pRoleFsmGetFsmState(eNextState));

			/* Transition into current state. */
			prP2pRoleFsmInfo->eCurrentState = eNextState;
		}

		switch (prP2pRoleFsmInfo->eCurrentState) {
		case P2P_ROLE_STATE_IDLE:
			if (!fgIsTransitionOut)
				p2pRoleStateInit_IDLE(prAdapter,
					prP2pRoleFsmInfo,
					prP2pRoleBssInfo);
			else
				p2pRoleStateAbort_IDLE(prAdapter,
					prP2pRoleFsmInfo,
					&(prP2pRoleFsmInfo->rChnlReqInfo));
			break;
		case P2P_ROLE_STATE_SCAN:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_SCAN(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					&(prP2pRoleFsmInfo->rScanReqInfo));
			} else {
				p2pRoleStateAbort_SCAN(prAdapter,
					prP2pRoleFsmInfo);
			}
			break;
		case P2P_ROLE_STATE_REQING_CHANNEL:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_REQING_CHANNEL(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					prP2pRoleFsmInfo,
					&(prP2pRoleFsmInfo->rChnlReqInfo));
			} else {
				p2pRoleStateAbort_REQING_CHANNEL(prAdapter,
					prP2pRoleBssInfo,
					prP2pRoleFsmInfo, eNextState);
			}
			break;
		case P2P_ROLE_STATE_WAIT_FOR_NEXT_REQ_CHNL:
			if (!fgIsTransitionOut) {
				cnmTimerStartTimer(prAdapter,
				   &prP2pRoleFsmInfo->rWaitNextReqChnlTimer,
				   SEC_TO_MSEC(2));
			}
			break;
		case P2P_ROLE_STATE_AP_CHNL_DETECTION:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_AP_CHNL_DETECTION(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					&(prP2pRoleFsmInfo->rScanReqInfo),
					&(prP2pRoleFsmInfo->rConnReqInfo));
			} else {
				p2pRoleStateAbort_AP_CHNL_DETECTION(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					&(prP2pRoleFsmInfo->rConnReqInfo),
					&(prP2pRoleFsmInfo->rChnlReqInfo),
					&(prP2pRoleFsmInfo->rScanReqInfo),
					eNextState);
			}
			break;
		case P2P_ROLE_STATE_GC_JOIN:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_GC_JOIN(prAdapter,
					prP2pRoleFsmInfo,
					&(prP2pRoleFsmInfo->rChnlReqInfo));
			} else {
				p2pRoleStateAbort_GC_JOIN(prAdapter,
					prP2pRoleFsmInfo,
					&(prP2pRoleFsmInfo->rJoinInfo),
					eNextState);
			}
			break;
		case P2P_ROLE_STATE_OFF_CHNL_TX:
			if (!fgIsTransitionOut) {
				fgIsTransitionOut =
					p2pRoleStateInit_OFF_CHNL_TX(prAdapter,
					prP2pRoleFsmInfo,
					&(prP2pRoleFsmInfo->rChnlReqInfo),
					&(prP2pRoleFsmInfo->rMgmtTxInfo),
					&eNextState);
			} else {
				p2pRoleStateAbort_OFF_CHNL_TX(prAdapter,
					prP2pRoleFsmInfo,
					&(prP2pRoleFsmInfo->rMgmtTxInfo),
					&(prP2pRoleFsmInfo->rChnlReqInfo),
					eNextState);
			}
			break;

#if (CFG_SUPPORT_DFS_MASTER == 1)
		case P2P_ROLE_STATE_DFS_CAC:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_DFS_CAC(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					&(prP2pRoleFsmInfo->rChnlReqInfo));
			} else {
				p2pRoleStateAbort_DFS_CAC(prAdapter,
					prP2pRoleBssInfo,
					prP2pRoleFsmInfo,
					eNextState);
			}
			break;
		case P2P_ROLE_STATE_SWITCH_CHANNEL:
			if (!fgIsTransitionOut) {
				p2pRoleStateInit_SWITCH_CHANNEL(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					&(prP2pRoleFsmInfo->rChnlReqInfo));
			} else {
				p2pRoleStateAbort_SWITCH_CHANNEL(prAdapter,
					prP2pRoleBssInfo,
					prP2pRoleFsmInfo,
					eNextState);
			}
			break;
#endif
		default:
			ASSERT(FALSE);
			break;
		}
	} while (fgIsTransitionOut);

}				/* p2pRoleFsmStateTransition */

void p2pRoleFsmRunEventTimeout(struct ADAPTER *prAdapter,
		uintptr_t ulParamPtr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) ulParamPtr;
	struct P2P_CHNL_REQ_INFO *prP2pChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prP2pConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;
	uint8_t ucBssIndex = 0;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pRoleFsmInfo != NULL));

		switch (prP2pRoleFsmInfo->eCurrentState) {
		case P2P_ROLE_STATE_IDLE:
			prP2pChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);
			if (prP2pChnlReqInfo->fgIsChannelRequested) {
				p2pFuncReleaseCh(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex,
					prP2pChnlReqInfo);
				if (IS_NET_PWR_STATE_IDLE(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex))
					DBGLOG(P2P, ERROR,
						"Power state was reset while request channel\n");
			}

			if (IS_NET_PWR_STATE_IDLE(prAdapter,
				prP2pRoleFsmInfo->ucBssIndex) &&
				IS_NET_ACTIVE(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex)) {
				ucBssIndex = prP2pRoleFsmInfo->ucBssIndex;
				if (prAdapter->aprBssInfo[ucBssIndex]
					->eConnectionState ==
					MEDIA_STATE_CONNECTED) {
					DBGLOG(P2P, TRACE,
						"Under deauth procedure.\n");
					break;
				}
				DBGLOG(P2P, TRACE,
					"Role BSS IDLE, deactive network.\n");
				nicDeactivateNetwork(prAdapter,
					NETWORK_ID(prP2pRoleFsmInfo->ucBssIndex,
					prP2pRoleFsmInfo->ucRoleIndex));
				nicUpdateBss(prAdapter,
					prP2pRoleFsmInfo->ucBssIndex);
			}
			break;
		case P2P_ROLE_STATE_GC_JOIN:
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
			break;
		case P2P_ROLE_STATE_OFF_CHNL_TX:
			p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo,
				P2P_ROLE_STATE_OFF_CHNL_TX);
			break;
#if (CFG_SUPPORT_DFS_MASTER == 1)
		case P2P_ROLE_STATE_DFS_CAC:
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
			kalP2PCacFinishedUpdate(prAdapter->prGlueInfo,
				prP2pRoleFsmInfo->ucRoleIndex);
			p2pFuncSetDfsState(DFS_STATE_ACTIVE);
			cnmTimerStartTimer(prAdapter,
				&(prP2pRoleFsmInfo->rDfsShutDownTimer),
				5000);
			/* start ap */
			prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);
			p2pRoleFsmRunEventStartAP(prAdapter,
				(struct MSG_HDR *)
				&prP2pConnReqInfo->rMsgStartAp);
			break;
#endif
		default:
			DBGLOG(P2P, ERROR,
			       "Current P2P Role State %d is unexpected for FSM timeout event.\n",
			       prP2pRoleFsmInfo->eCurrentState);
			ASSERT(FALSE);
			break;
		}
	} while (FALSE);
}				/* p2pRoleFsmRunEventTimeout */

static void
p2pRoleFsmDeauthCompleteImpl(struct ADAPTER *prAdapter,
		struct STA_RECORD *prStaRec)
{
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	enum ENUM_PARAM_MEDIA_STATE eOriMediaStatus;
	struct GL_P2P_INFO *prP2PInfo;
	uint16_t u2ReasonCode;
	u_int8_t fgIsLocallyGenerated;

	DBGLOG(P2P, INFO, "Deauth TX Complete!\n");

	if (!prAdapter) {
		DBGLOG(P2P, ERROR, "prAdapter shouldn't be NULL!\n");
		return;
	}

	if (!prStaRec) {
		DBGLOG(P2P, ERROR, "prStaRec shouldn't be NULL!\n");
		return;
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];
	if (!prP2pBssInfo) {
		DBGLOG(P2P, ERROR, "prP2pBssInfo shouldn't be NULL!\n");
		return;
	}

	eOriMediaStatus = prP2pBssInfo->eConnectionState;
	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pBssInfo->u4PrivateData);

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR, "prP2pRoleFsmInfo shouldn't be NULL!\n");
		return;
	}

	prP2PInfo = prAdapter->prGlueInfo->prP2PInfo[
			prP2pRoleFsmInfo->ucRoleIndex];

	if (!prP2PInfo) {
		DBGLOG(P2P, ERROR, "prP2PInfo shouldn't be NULL!\n");
		return;
	}
#if CFG_SUPPORT_802_11W
	/* Notify completion after encrypted deauth frame tx done */
	if (prP2pRoleFsmInfo->encryptedDeauthIsInProcess == TRUE) {
		if (!kal_completion_done(&prP2pRoleFsmInfo->rDeauthComp)) {
			DBGLOG(P2P, TRACE, "Complete rDeauthComp\n");
			complete(&prP2pRoleFsmInfo->rDeauthComp);
		}
	}
	prP2pRoleFsmInfo->encryptedDeauthIsInProcess = FALSE;
#endif

	/*
	 * After EAP exchange, GO/GC will disconnect
	 * and re-connect in short time.
	 * GC's new station record will be removed unexpectedly at GO's side
	 * if new GC's connection happens
	 * when previous GO's disconnection flow is
	 * processing. 4-way handshake will NOT be triggered.
	 */
	if ((prStaRec->eAuthAssocState == AAA_STATE_SEND_AUTH2 ||
			prStaRec->eAuthAssocState == AAA_STATE_SEND_ASSOC2) &&
		(prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) &&
		(p2pFuncIsAPMode(prAdapter->rWifiVar
		.prP2PConnSettings[prP2pBssInfo->u4PrivateData]) == FALSE)) {
		DBGLOG(P2P, WARN,
			"Skip deauth tx done since AAA fsm is in progress.\n");
		return;
	} else if (prStaRec->eAuthAssocState == SAA_STATE_SEND_AUTH1 ||
		prStaRec->eAuthAssocState == SAA_STATE_SEND_ASSOC1) {
		DBGLOG(P2P, WARN,
			"Skip deauth tx done since SAA fsm is in progress.\n");
		return;
	}

#if CFG_AP_80211KVR_INTERFACE
	aaaMulAPAgentStaEventNotify(prStaRec,
		prP2pBssInfo->aucBSSID, FALSE);
#endif

	/* Change station state. */
	cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_1);

	/* Save ReasonCode */
	u2ReasonCode = prStaRec->u2ReasonCode;
	fgIsLocallyGenerated = prStaRec->fgIsLocallyGenerated;

	/* Reset Station Record Status. */
	p2pFuncResetStaRecStatus(prAdapter, prStaRec);

	/* Try to remove StaRec in BSS client list before free it */
	bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec);

	/* STA_RECORD free */
	cnmStaRecFree(prAdapter, prStaRec);

	/* Skip when already disconnected to prevent deAuth done twice */
	if (prP2pBssInfo->eConnectionState == MEDIA_STATE_DISCONNECTED)
		return;

	if ((prP2pBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) ||
		(bssGetClientCount(prAdapter, prP2pBssInfo) == 0)) {
		if (prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT)
			DBGLOG(P2P, TRACE,
				"No More Client, Media Status DISCONNECTED\n");
		else
			DBGLOG(P2P, INFO,
				"Deauth done, Media Status DISCONNECTED, reason=%d\n",
				u2ReasonCode);
		p2pChangeMediaState(prAdapter,
			prP2pBssInfo,
			MEDIA_STATE_DISCONNECTED);
		if (prP2pBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE) {
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
					prP2pRoleFsmInfo->ucRoleIndex,
					NULL, NULL, 0, u2ReasonCode,
					fgIsLocallyGenerated ?
					WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY :
					WLAN_STATUS_MEDIA_DISCONNECT);
		}
		if (prP2PInfo && prP2PInfo->eChnlSwitchPolicy ==
				CHNL_SWITCH_POLICY_DEAUTH) {
			prP2PInfo->eChnlSwitchPolicy = CHNL_SWITCH_POLICY_NONE;
			p2pFunChnlSwitchNotifyDone(prAdapter);
		}
	}

	/* STOP BSS if power is IDLE */
	if (prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) {
		if (IS_NET_PWR_STATE_IDLE(prAdapter,
			prP2pRoleFsmInfo->ucBssIndex)
			&& (bssGetClientCount(prAdapter, prP2pBssInfo) == 0)) {
			/* All Peer disconnected !! Stop BSS now!! */
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
			p2pFuncStopComplete(prAdapter, prP2pBssInfo);
		} else if (eOriMediaStatus != prP2pBssInfo->eConnectionState) {
			/* Update the Media State if necessary */
			nicUpdateBss(prAdapter, prP2pBssInfo->ucBssIndex);
		}
	} else { /* GC : Stop BSS when Deauth done */
		p2pChangeMediaState(prAdapter,
			prP2pBssInfo,
			MEDIA_STATE_DISCONNECTED);
		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_IDLE);
		p2pFuncStopComplete(prAdapter,
			prP2pBssInfo);
	}
}

static void
p2pRoleFsmDeauthComplete(struct ADAPTER *prAdapter,
		struct STA_RECORD *prStaRec)
{
#if (CFG_SUPPORT_802_11BE_MLO == 1)
	struct MLD_STA_RECORD *mld_starec;

	mld_starec = mldStarecGetByStarec(prAdapter, prStaRec);
	if (mld_starec) {
		/* backup mldsta idx firstr because mldstarec is freed
		 * when all starec unregister
		 */
		uint8_t mldsta_idx = mld_starec->ucIdx;
		uint16_t i;
		struct STA_RECORD *starec;

		for (i = 0; i < CFG_STA_REC_NUM; i++) {
			starec = (struct STA_RECORD *) &prAdapter->arStaRec[i];

			if (!starec ||
			    !starec->fgIsInUse ||
			    starec->ucMldStaIndex != mldsta_idx)
				continue;

			DBGLOG(INIT, INFO,
				"\tsta: %d, wid: %d, bss: %d\n",
				starec->ucIndex,
				starec->ucWlanIndex,
				starec->ucBssIndex);

			p2pRoleFsmDeauthCompleteImpl(prAdapter,
				starec);
			cnmTimerStopTimer(prAdapter,
				&(starec->rDeauthTxDoneTimer));
		}
	} else
#endif
		p2pRoleFsmDeauthCompleteImpl(prAdapter, prStaRec);
}

void p2pRoleFsmDeauthTimeout(struct ADAPTER *prAdapter,
		uintptr_t ulParamPtr)
{
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) ulParamPtr;

	p2pRoleFsmDeauthComplete(prAdapter, prStaRec);
}				/* p2pRoleFsmRunEventTimeout */

void p2pRoleFsmRunEventAbort(struct ADAPTER *prAdapter,
		struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo)
{

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pRoleFsmInfo != NULL));

		if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_IDLE) {
			/* Get into IDLE state. */
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
		}

		/* Abort IDLE. */
		p2pRoleStateAbort_IDLE(prAdapter,
			prP2pRoleFsmInfo,
			&(prP2pRoleFsmInfo->rChnlReqInfo));

	} while (FALSE);
}				/* p2pRoleFsmRunEventAbort */

uint32_t
p2pRoleFsmRunEventDeauthTxDone(struct ADAPTER *prAdapter,
		struct MSDU_INFO *prMsduInfo,
		enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prMsduInfo != NULL));

		DBGLOG(P2P, INFO,
			"Deauth TX Done, seq = %d, sta = %d, status = %d\n",
			prMsduInfo->ucTxSeqNum,
			prMsduInfo->ucStaRecIndex,
			rTxDoneStatus);

		prStaRec = cnmGetStaRecByIndex(prAdapter,
			prMsduInfo->ucStaRecIndex);

		if (prStaRec == NULL) {
			DBGLOG(P2P, TRACE,
				"Station Record NULL, Index:%d\n",
				prMsduInfo->ucStaRecIndex);
			break;
		}

		p2pRoleFsmDeauthComplete(prAdapter, prStaRec);
		/* Avoid re-entry */
		cnmTimerStopTimer(prAdapter, &(prStaRec->rDeauthTxDoneTimer));

	} while (FALSE);

	return WLAN_STATUS_SUCCESS;

}				/* p2pRoleFsmRunEventDeauthTxDone */

void p2pRoleFsmRunEventRxDeauthentication(struct ADAPTER *prAdapter,
		struct STA_RECORD *prStaRec,
		struct SW_RFB *prSwRfb)
{
	struct WLAN_DEAUTH_FRAME *prDeauthFrame;
	struct BSS_INFO *prP2pBssInfo;
	uint16_t u2ReasonCode = 0, u2IELength = 0;
	/* flag to send deauth when rx sta disassc/deauth */
	u_int8_t fgSendDeauth = FALSE;

	if (prStaRec->ucStaState == STA_STATE_1)
		return;

	DBGLOG(P2P, INFO, "RX Deauth\n");

	prP2pBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];

	if (authProcessRxDeauthFrame(prSwRfb, prP2pBssInfo->aucBSSID,
				     &u2ReasonCode) != WLAN_STATUS_SUCCESS)
		return;

	prDeauthFrame = prSwRfb->pvHeader;
	u2IELength = prSwRfb->u2PacketLen -
		(WLAN_MAC_HEADER_LEN + REASON_CODE_FIELD_LEN);

	switch (prP2pBssInfo->eCurrentOPMode) {
	case OP_MODE_INFRASTRUCTURE:
		if (prP2pBssInfo->prStaRecOfAP != prStaRec)
			break;

		prP2pBssInfo->prStaRecOfAP = NULL;

		p2pFuncDisconnect(prAdapter,
			prP2pBssInfo,
			prStaRec,
			FALSE,
			u2ReasonCode,
			FALSE);

		SET_NET_PWR_STATE_IDLE(prAdapter,
			prP2pBssInfo->ucBssIndex);

		p2pRoleFsmStateTransition(prAdapter,
			P2P_ROLE_INDEX_2_ROLE_FSM_INFO(
				prAdapter,
				prP2pBssInfo->u4PrivateData),
				P2P_ROLE_STATE_IDLE);

		p2pFuncStopComplete(prAdapter, prP2pBssInfo);

		if (!prSwRfb->fgDriverGen)
			/* Indicate disconnect to Host. */
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
				(uint8_t) prP2pBssInfo->u4PrivateData,
				NULL,
				prDeauthFrame->aucInfoElem,
				u2IELength,
				u2ReasonCode,
				WLAN_STATUS_MEDIA_DISCONNECT);
		break;

	case OP_MODE_ACCESS_POINT:
#if CFG_SUPPORT_802_11W
		/* AP PMF */
		if (!prSwRfb->fgDriverGen) {
			if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
				if (prSwRfb->fgIsCipherMS ||
				    prSwRfb->fgIsCipherLenMS) {
					/* if cipher mismatch,
					 * or incorrect encrypt,
					 * just drop
					 */
					DBGLOG(P2P, ERROR,
						"Rx deauth CM/CLM=1\n");
					return;
				}

				/* 4.3.3.1 send unprotected deauth
				 * reason 6/7
				 */
				DBGLOG(P2P, INFO, "deauth reason=6\n");
				fgSendDeauth = TRUE;
				u2ReasonCode = REASON_CODE_CLASS_2_ERR;
				prStaRec->rPmfCfg.fgRxDeauthResp = TRUE;
			}
		}
#endif

		/* Delete client from client list. */
		if (bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec)) {
			/* Indicate disconnect to Host. */
			p2pFuncDisconnect(prAdapter,
				prP2pBssInfo,
				prStaRec,
				fgSendDeauth,
				u2ReasonCode,
				FALSE);
			/* Deactive BSS
			 * if PWR is IDLE and no peer
			 */
			if (IS_NET_PWR_STATE_IDLE(prAdapter,
				prP2pBssInfo->ucBssIndex) &&
				(bssGetClientCount(prAdapter,
				prP2pBssInfo) == 0)) {
				/* All Peer disconnected !!
				 * Stop BSS now!!
				 */
				p2pFuncStopComplete(prAdapter,
					prP2pBssInfo);
			}
		}
		break;

	case OP_MODE_P2P_DEVICE:
	default:
		/* Findout why someone
		 * sent deauthentication frame to us.
		 */
		ASSERT(FALSE);
		break;
	}
}				/* p2pRoleFsmRunEventRxDeauthentication */

void p2pRoleFsmRunEventRxDisassociation(struct ADAPTER *prAdapter,
		struct STA_RECORD *prStaRec,
		struct SW_RFB *prSwRfb)
{
	struct WLAN_DISASSOC_FRAME *prDisassocFrame;
	struct BSS_INFO *prP2pBssInfo;
	uint16_t u2ReasonCode = 0, u2IELength = 0;
	/* flag to send deauth when rx sta disassc/deauth */
	u_int8_t fgSendDeauth = FALSE;

	if (prStaRec->ucStaState == STA_STATE_1)
		return;

	DBGLOG(P2P, TRACE, "RX Disassoc\n");

	prP2pBssInfo = prAdapter->aprBssInfo[prStaRec->ucBssIndex];

	if (assocProcessRxDisassocFrame(prAdapter, prSwRfb,
					prP2pBssInfo->aucBSSID,
					&u2ReasonCode) != WLAN_STATUS_SUCCESS)
		return;

	prDisassocFrame = prSwRfb->pvHeader;
	u2IELength = prSwRfb->u2PacketLen -
		(WLAN_MAC_HEADER_LEN + REASON_CODE_FIELD_LEN);

	switch (prP2pBssInfo->eCurrentOPMode) {
	case OP_MODE_INFRASTRUCTURE:
		if (prP2pBssInfo->prStaRecOfAP != prStaRec)
			break;

		prP2pBssInfo->prStaRecOfAP = NULL;

		p2pFuncDisconnect(prAdapter,
			prP2pBssInfo,
			prStaRec,
			FALSE,
			prStaRec->u2ReasonCode,
			FALSE);

		SET_NET_PWR_STATE_IDLE(prAdapter,
			prP2pBssInfo->ucBssIndex);

		p2pRoleFsmStateTransition(prAdapter,
			P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
				prP2pBssInfo->u4PrivateData),
				P2P_ROLE_STATE_IDLE);

		p2pFuncStopComplete(prAdapter, prP2pBssInfo);

		if (!prSwRfb->fgDriverGen)
			/* Indicate disconnect to Host. */
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
				(uint8_t) prP2pBssInfo->u4PrivateData,
				NULL,
				prDisassocFrame->aucInfoElem,
				u2IELength,
				u2ReasonCode,
				WLAN_STATUS_MEDIA_DISCONNECT);
		break;

	case OP_MODE_ACCESS_POINT:
#if CFG_SUPPORT_802_11W
		/* AP PMF */
		if (!prSwRfb->fgDriverGen) {
			if (rsnCheckBipKeyInstalled(prAdapter, prStaRec)) {
				if (prSwRfb->fgIsCipherMS ||
				    prSwRfb->fgIsCipherLenMS) {
					/* if cipher mismatch,
					 * or incorrect encrypt, just drop
					 */
					DBGLOG(P2P, ERROR,
						"Rx disassoc CM/CLM=1\n");
					return;
				}

				/* 4.3.3.1 send unprotected deauth
				 * reason 6/7
				 */
				DBGLOG(P2P, INFO, "deauth reason=6\n");
				fgSendDeauth = TRUE;
				u2ReasonCode = REASON_CODE_CLASS_2_ERR;
				prStaRec->rPmfCfg.fgRxDeauthResp = TRUE;
			}
		}
#endif

		/* Delete client from client list. */
		if (bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec)) {
			/* Indicate disconnect to Host. */
			p2pFuncDisconnect(prAdapter,
				prP2pBssInfo,
				prStaRec,
				fgSendDeauth,
				u2ReasonCode,
				FALSE);
			/* Deactive BSS if PWR is IDLE and no peer */
			if (IS_NET_PWR_STATE_IDLE(prAdapter,
				prP2pBssInfo->ucBssIndex) &&
				(bssGetClientCount(prAdapter,
				prP2pBssInfo) == 0)) {
				/* All Peer disconnected !!
				 * Stop BSS now!!
				 */
				p2pFuncStopComplete(prAdapter,
					prP2pBssInfo);
			}
		}
		break;

	case OP_MODE_P2P_DEVICE:
	default:
		ASSERT(FALSE);
		break;
	}
}				/* p2pRoleFsmRunEventRxDisassociation */

void p2pRoleFsmRunEventBeaconTimeout(struct ADAPTER *prAdapter,
		struct BSS_INFO *prP2pBssInfo)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL) && (prP2pBssInfo != NULL));

		prP2pRoleFsmInfo =
			P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
				prP2pBssInfo->u4PrivateData);

		/* Only client mode would have beacon lost event. */
		if (prP2pBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE) {
			DBGLOG(P2P, ERROR,
			       "Error case, P2P BSS %d not INFRA mode but beacon timeout\n",
			       prP2pRoleFsmInfo->ucRoleIndex);
			break;
		}

		DBGLOG(P2P, TRACE,
			"p2pFsmRunEventBeaconTimeout: BSS %d Beacon Timeout\n",
			prP2pRoleFsmInfo->ucRoleIndex);

		if (prP2pBssInfo->eConnectionState
			== MEDIA_STATE_CONNECTED) {

			/* Indicate disconnect to Host. */
			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
					prP2pRoleFsmInfo->ucRoleIndex,
					NULL, NULL, 0,
					REASON_CODE_DISASSOC_LEAVING_BSS,
					WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY);

			if (prP2pBssInfo->prStaRecOfAP != NULL) {
				struct STA_RECORD *prStaRec =
					prP2pBssInfo->prStaRecOfAP;

				prP2pBssInfo->prStaRecOfAP = NULL;

				p2pFuncDisconnect(prAdapter,
					prP2pBssInfo,
					prStaRec, FALSE,
					REASON_CODE_DISASSOC_LEAVING_BSS,
					TRUE);

				SET_NET_PWR_STATE_IDLE(prAdapter,
					prP2pBssInfo->ucBssIndex);

				p2pRoleFsmStateTransition(prAdapter,
					prP2pRoleFsmInfo,
					P2P_ROLE_STATE_IDLE);

				p2pFuncStopComplete(prAdapter, prP2pBssInfo);
			}
		}
	} while (FALSE);
}				/* p2pFsmRunEventBeaconTimeout */

/*================== Message Event ==================*/
void p2pRoleFsmRunEventPreStartAP(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct MSG_P2P_START_AP *prP2pStartAPMsg =
		(struct MSG_P2P_START_AP *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prP2pConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;
	u_int8_t bSkipCac = TRUE;
	enum ENUM_BAND eBand;
	uint8_t ucChannelNum;

	prP2pStartAPMsg = (struct MSG_P2P_START_AP *) prMsgHdr;

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pStartAPMsg->ucRoleIdx);

	DBGLOG(P2P, TRACE,
		"p2pRoleFsmRunEventPreStartAP with Role(%d)\n",
		prP2pStartAPMsg->ucRoleIdx);

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
			"Corresponding P2P Role FSM empty: %d.\n",
			prP2pStartAPMsg->ucRoleIdx);
		cnmMemFree(prAdapter, prMsgHdr);
		return;
	}

	prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);
	prAdapter->rWifiVar
		.prP2pSpecificBssInfo[prP2pStartAPMsg->ucRoleIdx]
		->fgIsRddOpchng = FALSE;

	eBand = prP2pConnReqInfo->rChannelInfo.eBand;
	ucChannelNum = prP2pConnReqInfo->rChannelInfo.ucChannelNum;

	if (p2pFuncIsAPMode(prAdapter->rWifiVar
			.prP2PConnSettings[prP2pStartAPMsg->ucRoleIdx])) {
		if ((eBand == BAND_5G) &&
			rlmDomainIsLegalDfsChannel(
			prAdapter,
			eBand,
			ucChannelNum))
			bSkipCac = FALSE;
		else if ((eBand == BAND_5G) &&
			(prAdapter->rWifiVar.ucAp5gBandwidth >=
			MAX_BW_160MHZ)) {
			uint8_t ucRfBw =
				prAdapter->rWifiVar.ucAp5gBandwidth;

			/* Downgrade */
			if (p2pFuncIsDualAPMode(prAdapter) &&
				(ucRfBw >= MAX_BW_160MHZ))
				ucRfBw = MAX_BW_80MHZ;

			/* Revise to VHT OP BW */
			ucRfBw = rlmGetVhtOpBwByBssOpBw(ucRfBw);
			if (nicGetVhtS1(
				ucChannelNum,
				ucRfBw) &&
				(ucRfBw >= VHT_OP_CHANNEL_WIDTH_160))
				bSkipCac = FALSE;
		}

		/* STA+SAP will follow STA BW */
		if (p2pGetAisBssByBand(prAdapter, BAND_5G))
			bSkipCac = TRUE;
		else if (p2pFuncIsManualCac() &&
			(prAdapter->rWifiVar.u4ByPassCacTime <= 2)) {
			p2pFuncSetDfsState(DFS_STATE_ACTIVE);
			bSkipCac = TRUE;
		}
	}

	if (bSkipCac)
		p2pRoleFsmRunEventStartAP(prAdapter, prMsgHdr);
	else {
		memcpy(&prP2pConnReqInfo->rMsgStartAp,
			prMsgHdr,
			sizeof(struct MSG_P2P_START_AP));
		kalP2pPreStartRdd(prAdapter->prGlueInfo,
			prP2pStartAPMsg->ucRoleIdx,
			ucChannelNum,
			eBand);
	}

	cnmMemFree(prAdapter, prMsgHdr);
}

void p2pRoleFsmRunEventStartAP(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct MSG_P2P_START_AP *prP2pStartAPMsg =
		(struct MSG_P2P_START_AP *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prP2pConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecificBssInfo =
		(struct P2P_SPECIFIC_BSS_INFO *) NULL;

	DBGLOG(P2P, TRACE, "p2pRoleFsmRunEventStartAP\n");

	prP2pStartAPMsg = (struct MSG_P2P_START_AP *) prMsgHdr;

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pStartAPMsg->ucRoleIdx);

	prAdapter->prP2pInfo->eConnState = P2P_CNN_NORMAL;

	DBGLOG(P2P, TRACE,
		"p2pRoleFsmRunEventStartAP with Role(%d)\n",
		prP2pStartAPMsg->ucRoleIdx);


	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
		       "p2pRoleFsmRunEventStartAP: Corresponding P2P Role FSM empty: %d.\n",
		       prP2pStartAPMsg->ucRoleIdx);
		goto error;
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];
	prP2pSpecificBssInfo =
		prAdapter->rWifiVar
			.prP2pSpecificBssInfo[prP2pBssInfo->u4PrivateData];
	prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);

	if (prP2pStartAPMsg->u4BcnInterval) {
		DBGLOG(P2P, TRACE,
			"Beacon interval updated to :%u\n",
			prP2pStartAPMsg->u4BcnInterval);
		prP2pBssInfo->u2BeaconInterval =
			(uint16_t) prP2pStartAPMsg->u4BcnInterval;
	} else if (prP2pBssInfo->u2BeaconInterval == 0) {
		prP2pBssInfo->u2BeaconInterval = DOT11_BEACON_PERIOD_DEFAULT;
	}

	if (prAdapter->rWifiVar.ucGoBcnIntrvl != 0) {
		prP2pBssInfo->u2BeaconInterval =
			prAdapter->rWifiVar.ucGoBcnIntrvl;
		DBGLOG(P2P, TRACE,
			"Updated u2BeaconInterval to :%u by customize.\n",
			prP2pBssInfo->u2BeaconInterval);
	}

	if (prP2pStartAPMsg->u4DtimPeriod) {
		DBGLOG(P2P, TRACE,
			"DTIM interval updated to :%u\n",
			prP2pStartAPMsg->u4DtimPeriod);
		prP2pBssInfo->ucDTIMPeriod =
			(uint8_t) prP2pStartAPMsg->u4DtimPeriod;
	} else if (prP2pBssInfo->ucDTIMPeriod == 0) {
		prP2pBssInfo->ucDTIMPeriod = DOT11_DTIM_PERIOD_DEFAULT;
	}

	if (prP2pStartAPMsg->u2SsidLen != 0) {
		kalMemCopy(prP2pConnReqInfo->rSsidStruct.aucSsid,
			prP2pStartAPMsg->aucSsid,
			prP2pStartAPMsg->u2SsidLen);
		prP2pConnReqInfo->rSsidStruct.ucSsidLen =
		    prP2pSpecificBssInfo->u2GroupSsidLen =
		    prP2pStartAPMsg->u2SsidLen;
		kalMemCopy(prP2pSpecificBssInfo->aucGroupSsid,
			prP2pStartAPMsg->aucSsid,
			prP2pStartAPMsg->u2SsidLen);
	}

	if (p2pFuncIsAPMode(prAdapter->rWifiVar
		.prP2PConnSettings[prP2pStartAPMsg->ucRoleIdx])) {
		prP2pConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_PURE_AP;

		/* Overwrite AP channel */
		if (prAdapter->rWifiVar.ucApChannel &&
			prAdapter->rWifiVar.ucApChnlDefFromCfg) {
			prP2pConnReqInfo->rChannelInfo.ucChannelNum =
				prAdapter->rWifiVar.ucApChannel;

			if (prAdapter->rWifiVar.ucApChannel <= 14)
				prP2pConnReqInfo->rChannelInfo.eBand = BAND_2G4;
			else
				prP2pConnReqInfo->rChannelInfo.eBand = BAND_5G;
		} else if (prAdapter->rWifiVar.u2ApFreq &&
			prAdapter->rWifiVar.ucApChnlDefFromCfg) {
			/* Translate Freq from MHz to channel number. */
			prP2pConnReqInfo->rChannelInfo.ucChannelNum =
			nicFreq2ChannelNum(prAdapter->rWifiVar.u2ApFreq * 1000);

			if (prAdapter->rWifiVar.u2ApFreq >= 2412 &&
				prAdapter->rWifiVar.u2ApFreq <= 2484)
				prP2pConnReqInfo->rChannelInfo.eBand = BAND_2G4;
			else if (prAdapter->rWifiVar.u2ApFreq >= 5180 &&
				prAdapter->rWifiVar.u2ApFreq <= 5900)
				prP2pConnReqInfo->rChannelInfo.eBand = BAND_5G;
#if (CFG_SUPPORT_WIFI_6G == 1)
			else if (prAdapter->rWifiVar.u2ApFreq >= 5955 &&
				prAdapter->rWifiVar.u2ApFreq <= 7115)
				prP2pConnReqInfo->rChannelInfo.eBand = BAND_6G;
#endif
		}
	} else {
		prP2pConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_GO;
#if CFG_P2P_DEFAULT_CLIENT_COUNT
		kalP2PSetMaxClients(prAdapter->prGlueInfo,
			P2P_DEFAULT_CLIENT_COUNT,
			prP2pStartAPMsg->ucRoleIdx);
#endif
	}

	/* Clear current AP's STA_RECORD_T and current AID to prevent
	 * using previous p2p connection state. This is needed because
	 * upper layer may add keys before we start SAP/GO.
	 */
	prP2pBssInfo->prStaRecOfAP = (struct STA_RECORD *) NULL;
	prP2pBssInfo->u2AssocId = 0;

	/* Clear list to ensure no client staRec */
	if (bssGetClientCount(prAdapter, prP2pBssInfo) != 0) {
		DBGLOG(P2P, WARN,
			"Clear list to ensure no empty/client staRec\n");
		bssInitializeClientList(prAdapter, prP2pBssInfo);
	}

	/* The supplicant may start AP
	 * before rP2pRoleFsmTimeoutTimer is time out
	 */
	/* We need to make sure the BSS was deactivated
	 * and all StaRec can be free
	 */
	if (timerPendingTimer(&(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer))) {
		/* call p2pRoleFsmRunEventTimeout()
		 * to deactive BSS and free channel
		 */
		p2pRoleFsmRunEventTimeout(prAdapter,
			(uintptr_t)prP2pRoleFsmInfo);
		cnmTimerStopTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer));
	}

#if (CFG_SUPPORT_DFS_MASTER == 1)
	if (timerPendingTimer(&(prP2pRoleFsmInfo->rDfsShutDownTimer))) {
		DBGLOG(P2P, INFO,
			"p2pRoleFsmRunEventStartAP: Stop DFS shut down timer.\n");
		cnmTimerStopTimer(prAdapter,
			&(prP2pRoleFsmInfo->rDfsShutDownTimer));
	}
#endif

	prP2pBssInfo->eBand = prP2pConnReqInfo->rChannelInfo.eBand;
	p2pGetLinkWmmQueSet(prAdapter, prP2pBssInfo);
#if CFG_SUPPORT_DBDC
#if (CFG_DBDC_SW_FOR_P2P_LISTEN == 1)
	p2pRoleP2pLisStopDbdcDecision(prAdapter,
		prP2pConnReqInfo->eConnRequest);
#endif
	/* DBDC decsion.may change OpNss */
	cnmDbdcPreConnectionEnableDecision(prAdapter,
		prP2pBssInfo->ucBssIndex,
		prP2pConnReqInfo->rChannelInfo.eBand,
		prP2pConnReqInfo->rChannelInfo.ucChannelNum,
		prP2pBssInfo->ucWmmQueSet);
#endif /*CFG_SUPPORT_DBDC*/

	cnmOpModeGetTRxNss(
		prAdapter, prP2pBssInfo->ucBssIndex,
		&prP2pBssInfo->ucOpRxNss, &prP2pBssInfo->ucOpTxNss);
	prP2pBssInfo->eHiddenSsidType = prP2pStartAPMsg->ucHiddenSsidType;

	DBGLOG(P2P, TRACE,
		"p2pRoleFsmRunEventStartAP: start AP CH[%u]",
		prP2pConnReqInfo->rChannelInfo.ucChannelNum);
	DBGLOG(P2P, TRACE, "RxNSS[%u]TxNss[%u]. Hidden[%u]\n",
		prP2pBssInfo->ucOpRxNss, prP2pBssInfo->ucOpTxNss,
		prP2pBssInfo->eHiddenSsidType);

	/* Update channel info first for rnr */
	prP2pBssInfo->ucPrimaryChannel =
		prP2pConnReqInfo->rChannelInfo.ucChannelNum;
	prP2pBssInfo->eBand =
		prP2pConnReqInfo->rChannelInfo.eBand;

	/*
	 * beacon content is related with Nss number ,
	 * need to update because of modification
	 */
	bssUpdateBeaconContent(prAdapter, prP2pBssInfo->ucBssIndex);

	if ((prP2pBssInfo->eCurrentOPMode != OP_MODE_ACCESS_POINT) ||
	    (prP2pBssInfo->eIntendOPMode != OP_MODE_NUM)) {
		/* 1. No switch to AP mode.
		 * 2. Not started yet.
		 */

		if (prP2pRoleFsmInfo->eCurrentState
			!= P2P_ROLE_STATE_AP_CHNL_DETECTION
			&&
		    prP2pRoleFsmInfo->eCurrentState
		    != P2P_ROLE_STATE_IDLE) {
			/* Make sure the state is in IDLE state. */
			p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
		} else if (prP2pRoleFsmInfo->eCurrentState
					== P2P_ROLE_STATE_AP_CHNL_DETECTION) {
			goto error;
		}

		/* Leave IDLE state. */
		SET_NET_PWR_STATE_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex);

		prP2pBssInfo->eIntendOPMode = OP_MODE_ACCESS_POINT;

#if 0
		prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.ucChannelNum = 8;
		prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.eBand = BAND_2G4;
		/* prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.ucBandwidth =
		 * 0;
		 */
		/* prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.eSCO =
		 * CHNL_EXT_SCN;
		 */
#endif

		if (prP2pRoleFsmInfo->rConnReqInfo
			.rChannelInfo.ucChannelNum != 0) {
			DBGLOG(P2P, INFO,
				"Role(%d) StartAP at Band[%u]CH[%u]RxNSS[%u]TxNss[%u]\n",
				prP2pStartAPMsg->ucRoleIdx,
				prP2pRoleFsmInfo->rConnReqInfo
					.rChannelInfo.eBand,
				prP2pRoleFsmInfo->rConnReqInfo
					.rChannelInfo.ucChannelNum,
				prP2pBssInfo->ucOpRxNss,
				prP2pBssInfo->ucOpTxNss);

			p2pRoleStatePrepare_To_REQING_CHANNEL_STATE(
				prAdapter,
				GET_BSS_INFO_BY_INDEX(prAdapter,
				prP2pRoleFsmInfo->ucBssIndex),
				&(prP2pRoleFsmInfo->rConnReqInfo),
				&(prP2pRoleFsmInfo->rChnlReqInfo));

			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_REQING_CHANNEL);
		} else {
			DBGLOG(P2P, INFO,
				"Role(%d) StartAP Scan for working channel\n",
				prP2pStartAPMsg->ucRoleIdx);

			/* For AP/GO mode with specific channel
			 * or non-specific channel.
			 */
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_AP_CHNL_DETECTION);
		}
	}

error:
	return;

}				/* p2pRoleFsmRunEventStartAP */

void p2pRoleFsmDelIfaceDone(
	struct ADAPTER *prAdapter,
	uint8_t ucRoleIdx)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct GL_P2P_INFO *prP2pInfo =
		(struct GL_P2P_INFO *) NULL;

	if (!prAdapter || !prAdapter->prGlueInfo) {
		DBGLOG(P2P, ERROR, "pAd is null.\n");
		return;
	}

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(
		prAdapter, ucRoleIdx);
	prP2pInfo =
		prAdapter->prGlueInfo->prP2PInfo[ucRoleIdx];
	if (!prP2pRoleFsmInfo || !prP2pInfo) {
		DBGLOG(P2P, ERROR, "Fsm is null.\n");
		return;
	} else if (!prP2pInfo->fgDelIface) {
		DBGLOG(P2P, INFO,
			"No need to delete iface done\n");
		return;
	}

	DBGLOG(P2P, INFO,
		"Delete iface for role %d\n", ucRoleIdx);

	prP2pInfo->fgDelIface = FALSE;

	if (p2pGetMode() == RUNNING_P2P_DEV_MODE) {
#if (CFG_SUPPORT_802_11BE_MLO == 1)
		/* don't use u4ConnType to check gc role because
		 * upper layer might change iface fist
		 */
		if (p2pGetGCBssNum(prP2pRoleFsmInfo) > 1)
			p2pLinkUninitGCRole(prAdapter);
		else
#endif
			p2pRoleFsmUninit(prAdapter, ucRoleIdx);
	}
}

void p2pRoleFsmDelIface(
	struct ADAPTER *prAdapter,
	uint8_t ucRoleIdx)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct GLUE_INFO *prGlueInfo = (struct GLUE_INFO *) NULL;
	struct GL_P2P_INFO *prP2pInfo = (struct GL_P2P_INFO *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prConnReqInfo = NULL;
	const uint8_t aucZeroMacAddr[] = NULL_MAC_ADDR;
	uint32_t u4ConnType;

	prGlueInfo = prAdapter->prGlueInfo;
	if (prGlueInfo == NULL) {
		DBGLOG(P2P, ERROR, "prGlueInfo shouldn't be NULL!\n");
		goto error;
	}

	prAdapter = prGlueInfo->prAdapter;
	prP2pInfo = prGlueInfo->prP2PInfo[ucRoleIdx];
	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter, ucRoleIdx);
	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
			"p2pRoleFsmRunEventDelIface: Corresponding P2P Role FSM empty: %d.\n",
			ucRoleIdx);
		goto error;
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];
	prConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);
	u4ConnType = bssInfoConnType(prAdapter, prP2pBssInfo);

	DBGLOG(P2P, INFO,
		"Delete bss%d connType=0x%x, netType=%d, op=%d, intend=%d\n",
		prP2pBssInfo->ucBssIndex, u4ConnType,
		prP2pBssInfo->eNetworkType,
		prP2pBssInfo->eCurrentOPMode,
		prP2pBssInfo->eIntendOPMode);

	/* Case: Delete iface without disconnection */
	prP2pInfo->fgDelIface = TRUE;

	if (!IS_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex) &&
		IS_NET_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex) &&
		prP2pBssInfo->eConnectionState == MEDIA_STATE_CONNECTED) {
		struct STA_RECORD *prStaRec =
			(struct STA_RECORD *) prP2pBssInfo->prStaRecOfAP;

		if (IS_BSS_APGO(prP2pBssInfo)) {
			struct MSG_P2P_STOP_AP *prP2pStopApMsg =
				(struct MSG_P2P_STOP_AP *) NULL;

			prP2pStopApMsg = cnmMemAlloc(prAdapter,
				RAM_TYPE_MSG, sizeof(struct MSG_P2P_STOP_AP));
			if (prP2pStopApMsg) {
				prP2pStopApMsg->ucRoleIdx = ucRoleIdx;
				DBGLOG(P2P, INFO,
					"GO: Delete before disconnection\n");
				p2pRoleFsmRunEventStopAP(prAdapter,
					(struct MSG_HDR *) prP2pStopApMsg);
			}
		} else if (prStaRec && prStaRec->fgIsInUse) {
			p2pFuncDisconnect(prAdapter,
				prP2pBssInfo,
				prStaRec,
				TRUE,
				REASON_CODE_DEAUTH_LEAVING_BSS,
				TRUE);

			cnmTimerStopTimer(prAdapter,
				&(prStaRec->rDeauthTxDoneTimer));
			cnmTimerInitTimer(prAdapter,
				&(prStaRec->rDeauthTxDoneTimer),
				(PFN_MGMT_TIMEOUT_FUNC)
					p2pRoleFsmDeauthTimeout,
				(uintptr_t) prStaRec);
			cnmTimerStartTimer(prAdapter,
				&(prStaRec->rDeauthTxDoneTimer),
				P2P_DEAUTH_TIMEOUT_TIME_MS);
			SET_NET_PWR_STATE_IDLE(prAdapter,
				prP2pBssInfo->ucBssIndex);
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
			DBGLOG(P2P, INFO,
				"GC: Delete before disconnection\n");
		}
	}

	/* The state is in disconnecting and can not change any BSS status */
	if (IS_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex) &&
		IS_NET_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex) &&
		prP2pBssInfo->eConnectionState == MEDIA_STATE_CONNECTED) {
		DBGLOG(P2P, INFO, "under deauth procedure, Quit.\n");
	} else {
		/*p2pFuncDissolve(prAdapter,
		 * prP2pBssInfo, TRUE,
		 * REASON_CODE_DEAUTH_LEAVING_BSS);
		 */

		SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);

		/* Function Dissolve should already enter IDLE state. */
		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_IDLE);

		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);

		/* Clear CmdQue */
		kalClearMgmtFramesByBssIdx(prAdapter->prGlueInfo,
			prP2pBssInfo->ucBssIndex);
		kalClearCmdDataFramesByBssIdx(prAdapter->prGlueInfo,
			prP2pBssInfo->ucBssIndex);
		/* Clear PendingCmdQue */
		wlanReleasePendingCMDbyBssIdx(prAdapter,
			prP2pBssInfo->ucBssIndex);
		/* Clear PendingTxMsdu */
		nicFreePendingTxMsduInfo(prAdapter,
			prP2pBssInfo->ucBssIndex, MSDU_REMOVE_BY_BSS_INDEX);

		/* Deactivate BSS. */
		nicDeactivateNetwork(prAdapter,
			NETWORK_ID(prP2pRoleFsmInfo->ucBssIndex,
			prP2pRoleFsmInfo->ucRoleIndex));
		nicUpdateBss(prAdapter, prP2pRoleFsmInfo->ucBssIndex);
		prP2pBssInfo->eCurrentOPMode = OP_MODE_INFRASTRUCTURE;
		p2pFuncInitConnectionSettings(prAdapter,
			prAdapter->rWifiVar.prP2PConnSettings[ucRoleIdx],
			FALSE);

		if (!EQUAL_MAC_ADDR(aucZeroMacAddr, prConnReqInfo->aucBssid))
			kalP2pUnlinkBss(prAdapter->prGlueInfo,
					prConnReqInfo->aucBssid);

		p2pRoleFsmDelIfaceDone(prAdapter, ucRoleIdx);
	}

error:

	DBGLOG(P2P, LOUD, "Finish del iface, Quit.\n");
}

void p2pRoleFsmRunEventDelIface(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct MSG_P2P_DEL_IFACE *prP2pDelIfaceMsg =
		(struct MSG_P2P_DEL_IFACE *) prMsgHdr;

	p2pRoleFsmDelIface(prAdapter,
		prP2pDelIfaceMsg->ucRoleIdx);

	cnmMemFree(prAdapter, prMsgHdr);
}


void p2pRoleFsmRunEventStopAP(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_STOP_AP *prP2pStopApMsg;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct STA_RECORD *prCurrStaRec;
	struct LINK *prClientList;

	prP2pStopApMsg = (struct MSG_P2P_STOP_AP *) prMsgHdr;

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
		prP2pStopApMsg->ucRoleIdx);

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
		       "p2pRoleFsmRunEventStopAP: Corresponding P2P Role FSM empty: %d.\n",
		       prP2pStopApMsg->ucRoleIdx);
		goto error;
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

	if (!prP2pBssInfo) {
		DBGLOG(P2P, ERROR,
			"prP2pBssInfo of prP2pRoleFsmInfo->ucBssIndex %d is NULL!\n",
			prP2pRoleFsmInfo->ucBssIndex);
		goto error;
	}

#if (CFG_SUPPORT_DFS_MASTER == 1)
	if (prP2pBssInfo->eBand != BAND_5G)
		goto SKIP_END_RDD;

	p2pFuncSetDfsState(DFS_STATE_INACTIVE);
	p2pFuncStopRdd(prAdapter, prP2pBssInfo->ucBssIndex);
#endif

SKIP_END_RDD:

	rsnFlushPmkid(prAdapter, prP2pBssInfo->ucBssIndex);

	kalP2PResetBlackList(prAdapter->prGlueInfo,
		prP2pStopApMsg->ucRoleIdx);

	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_REQING_CHANNEL) {
		p2pFuncStopGO(prAdapter, prP2pBssInfo);

		/* Start all Deauth done timer for all client */
		prClientList = &prP2pBssInfo->rStaRecOfClientList;

		LINK_FOR_EACH_ENTRY(prCurrStaRec,
			prClientList, rLinkEntry, struct STA_RECORD) {
			if (!prCurrStaRec || !prCurrStaRec->fgIsInUse)
				break;
			/* Do not restart timer if the timer is pending, */
			/* (start in p2pRoleFsmRunEventConnectionAbort()) */
			if (!timerPendingTimer(
				&(prCurrStaRec->rDeauthTxDoneTimer))) {
				cnmTimerInitTimer(prAdapter,
					&(prCurrStaRec->rDeauthTxDoneTimer),
					(PFN_MGMT_TIMEOUT_FUNC)
					p2pRoleFsmDeauthTimeout,
					(uintptr_t) prCurrStaRec);

				cnmTimerStartTimer(prAdapter,
					&(prCurrStaRec->rDeauthTxDoneTimer),
					P2P_DEAUTH_TIMEOUT_TIME_MS);
			}
		}
	}

	SET_NET_PWR_STATE_IDLE(prAdapter, prP2pBssInfo->ucBssIndex);

	/* Postpone entering idle state if sending deauth frames. This is to
	 * prevent releasing channel too early and cause unnecessary cnm
	 * time when starting and stopping AP quickly.
	 */
	if (IS_NET_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex) &&
		prP2pBssInfo->eConnectionState == MEDIA_STATE_CONNECTED) {
		DBGLOG(P2P, INFO,
			"postpone entering idle state for deauth process\n");
	} else {
		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_IDLE);
	}

error:
	cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pRoleFsmRunEventStopAP */

#if (CFG_SUPPORT_DFS_MASTER == 1)
void p2pRoleFsmRunEventDfsCac(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct MSG_P2P_DFS_CAC *prP2pDfsCacMsg =
		(struct MSG_P2P_DFS_CAC *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prP2pConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	enum ENUM_CHANNEL_WIDTH rChannelWidth;

	DBGLOG(P2P, INFO, "p2pRoleFsmRunEventDfsCac\n");

	prP2pDfsCacMsg = (struct MSG_P2P_DFS_CAC *) prMsgHdr;

	rChannelWidth = prP2pDfsCacMsg->eChannelWidth;

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pDfsCacMsg->ucRoleIdx);

	DBGLOG(P2P, INFO,
		"p2pRoleFsmRunEventDfsCac with Role(%d)\n",
		prP2pDfsCacMsg->ucRoleIdx);

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
		       "p2pRoleFsmRunEventDfsCac: Corresponding P2P Role FSM empty: %d.\n",
		       prP2pDfsCacMsg->ucRoleIdx);
		goto error;
	}

	if (timerPendingTimer(&(prP2pRoleFsmInfo->rDfsShutDownTimer))) {
		DBGLOG(P2P, INFO,
			"p2pRoleFsmRunEventDfsCac: Stop DFS shut down timer.\n");
		cnmTimerStopTimer(prAdapter,
			&(prP2pRoleFsmInfo->rDfsShutDownTimer));
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

	prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);

	if (p2pFuncIsAPMode(prAdapter->rWifiVar
		.prP2PConnSettings[prP2pDfsCacMsg->ucRoleIdx]))
		prP2pConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_PURE_AP;
	else
		prP2pConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_GO;

	prP2pBssInfo->eBand = prP2pConnReqInfo->rChannelInfo.eBand;
	p2pGetLinkWmmQueSet(prAdapter, prP2pBssInfo);

#if CFG_SUPPORT_DBDC
#if (CFG_DBDC_SW_FOR_P2P_LISTEN == 1)
	p2pRoleP2pLisStopDbdcDecision(prAdapter,
		prP2pConnReqInfo->eConnRequest);
#endif
	/* DBDC decsion.may change OpNss */
	cnmDbdcPreConnectionEnableDecision(prAdapter,
		prP2pBssInfo->ucBssIndex,
		prP2pConnReqInfo->rChannelInfo.eBand,
		prP2pConnReqInfo->rChannelInfo.ucChannelNum,
		prP2pBssInfo->ucWmmQueSet
	);
#endif /*CFG_SUPPORT_DBDC*/

	cnmOpModeGetTRxNss(
		prAdapter, prP2pBssInfo->ucBssIndex,
		&prP2pBssInfo->ucOpRxNss, &prP2pBssInfo->ucOpTxNss);

	DBGLOG(P2P, INFO,
		"p2pRoleFsmRunEventDfsCac: CH[%u]RxNSS[%u]TxNss[%u].\n",
		prP2pConnReqInfo->rChannelInfo.ucChannelNum,
		prP2pBssInfo->ucOpRxNss,
		prP2pBssInfo->ucOpTxNss);

	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_IDLE) {
		/* Make sure the state is in IDLE state. */
		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
	}

	/* Leave IDLE state. */
	SET_NET_PWR_STATE_ACTIVE(prAdapter, prP2pBssInfo->ucBssIndex);

	prP2pBssInfo->eIntendOPMode = OP_MODE_ACCESS_POINT;
	prP2pBssInfo->fgIsDfsActive = TRUE;

	if (prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.ucChannelNum != 0) {
		DBGLOG(P2P, INFO, "Role(%d) Set channel at CH(%d)\n",
			prP2pDfsCacMsg->ucRoleIdx,
			prP2pRoleFsmInfo->rConnReqInfo
				.rChannelInfo.ucChannelNum);

		kalP2PCacStartedUpdate(prAdapter->prGlueInfo,
			prP2pDfsCacMsg->ucRoleIdx);

		p2pRoleStatePrepare_To_DFS_CAC_STATE(prAdapter,
				GET_BSS_INFO_BY_INDEX(prAdapter,
				prP2pRoleFsmInfo->ucBssIndex),
				rChannelWidth,
				&(prP2pRoleFsmInfo->rConnReqInfo),
				&(prP2pRoleFsmInfo->rChnlReqInfo));
		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_DFS_CAC);
	} else {
		DBGLOG(P2P, ERROR,
			"prP2pRoleFsmInfo->rConnReqInfo.rChannelInfo.ucChannelNum shouldn't be 0\n");
	}

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/*p2pRoleFsmRunEventDfsCac*/

void p2pRoleFsmRunEventRadarDet(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_RADAR_DETECT *prMsgP2pRddDetMsg;
	struct P2P_CONNECTION_REQ_INFO *prP2pConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;


	DBGLOG(P2P, INFO, "p2pRoleFsmRunEventRadarDet\n");

	prMsgP2pRddDetMsg = (struct MSG_P2P_RADAR_DETECT *) prMsgHdr;

	prP2pBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter,
			prMsgP2pRddDetMsg->ucBssIndex);

	if (!prP2pBssInfo)
		goto error;

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pBssInfo->u4PrivateData);

	if (!prP2pRoleFsmInfo)
		goto error;

	DBGLOG(P2P, INFO,
		"p2pRoleFsmRunEventRadarDet with Role(%d)\n",
		prP2pRoleFsmInfo->ucRoleIndex);

	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_DFS_CAC &&
		prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_IDLE) {
		DBGLOG(P2P, ERROR,
			"Wrong prP2pRoleFsmInfo->eCurrentState \"%s\"!",
			(prP2pRoleFsmInfo->eCurrentState < P2P_ROLE_STATE_NUM
			? (const char *)
			p2pRoleFsmGetFsmState(prP2pRoleFsmInfo->eCurrentState)
			: ""));
		goto error;
	}

	if (p2pFuncGetRadarDetectMode()) {
		DBGLOG(P2P, INFO,
			"p2pRoleFsmRunEventRadarDet: Ignore radar event\n");
		if (prP2pRoleFsmInfo->eCurrentState == P2P_ROLE_STATE_DFS_CAC)
			p2pFuncSetDfsState(DFS_STATE_CHECKING);
		else
			p2pFuncSetDfsState(DFS_STATE_ACTIVE);
	} else {
		uint8_t ucNumOfChannel;
		uint8_t ch_idx = 0;
		uint8_t ucChannelNum = 36;
		uint8_t ucRoleIndex = prP2pBssInfo->u4PrivateData;
		struct RF_CHANNEL_INFO aucChannelList
			[MAX_5G_BAND_CHN_NUM] = {0};
		struct RF_CHANNEL_INFO aucChannelListRdd
			[MAX_5G_BAND_CHN_NUM] = {0};

		if (prP2pRoleFsmInfo->eCurrentState == P2P_ROLE_STATE_DFS_CAC) {
			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
			SET_NET_PWR_STATE_IDLE(
				prAdapter,
				prP2pBssInfo->ucBssIndex)
		}

		kalP2PRddDetectUpdate(prAdapter->prGlueInfo,
			prP2pRoleFsmInfo->ucRoleIndex);

		cnmTimerStartTimer(prAdapter,
			&(prP2pRoleFsmInfo->rDfsShutDownTimer),
			5000);

		/* Get random ch */
		rlmDomainGetChnlList(prAdapter,
			BAND_5G,
			TRUE,
			MAX_5G_BAND_CHN_NUM,
			&ucNumOfChannel,
			aucChannelList);

		prP2pConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);

		if (prAdapter->rWifiVar.prP2pSpecificBssInfo[ucRoleIndex]
			->fgIsRddOpchng == TRUE) {
			ucChannelNum = prAdapter->rWifiVar
				.prP2pSpecificBssInfo[ucRoleIndex]
				->ucRddCh;
		} else {
			p2pFuncChannelListFiltering(prAdapter,
				prP2pConnReqInfo->rChannelInfo.ucChannelNum,
				prP2pBssInfo->ucVhtChannelWidth,
				ucNumOfChannel,
				aucChannelList,
				&ucNumOfChannel,
				aucChannelListRdd);

			ch_idx = kalRandomNumber() % ucNumOfChannel;

			if (ch_idx < MAX_5G_BAND_CHN_NUM)
				ucChannelNum = aucChannelListRdd[ch_idx]
					.ucChannelNum;
		}
		prP2pBssInfo->eCurrentOPMode = OP_MODE_ACCESS_POINT;
		prP2pConnReqInfo->rChannelInfo.ucChannelNum = ucChannelNum;
		if (ucChannelNum == 165) {
			prAdapter->rWifiVar
				.prP2pSpecificBssInfo[ucRoleIndex]
				->ucRddBw = VHT_OP_CHANNEL_WIDTH_20_40;
		} else {
			prAdapter->rWifiVar
				.prP2pSpecificBssInfo[ucRoleIndex]
				->ucRddBw = VHT_OP_CHANNEL_WIDTH_80;
		}
		/* Use rConnReqInfo bw */

		if (IS_NET_PWR_STATE_ACTIVE(
			prAdapter,
			prP2pBssInfo->ucBssIndex)) {

			cnmSapChannelSwitchReq(prAdapter,
				&prP2pConnReqInfo->rChannelInfo,
				prP2pBssInfo->u4PrivateData);
			kalP2PTxCarrierOn(prAdapter->prGlueInfo,
					prP2pBssInfo);
		} else {
			p2pRoleFsmRunEventStartAP(prAdapter,
				(struct MSG_HDR *)
				&prP2pConnReqInfo->rMsgStartAp);
		}
	}

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/*p2pRoleFsmRunEventRadarDet*/

void p2pRoleFsmRunEventSetNewChannel(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_SET_NEW_CHANNEL *prMsgP2pSetNewChannelMsg;
	struct RF_CHANNEL_INFO *prRfChannelInfo;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo;


	DBGLOG(P2P, INFO, "p2pRoleFsmRunEventSetNewChannel\n");

	prMsgP2pSetNewChannelMsg = (struct MSG_P2P_SET_NEW_CHANNEL *) prMsgHdr;

	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prMsgP2pSetNewChannelMsg->ucBssIndex);

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prMsgP2pSetNewChannelMsg->ucRoleIdx);

	if (prP2pBssInfo &&
		kalP2pIsStoppingAp(prAdapter,
		prP2pBssInfo)) {
		DBGLOG(P2P, ERROR,
			"BSS %d is disabled.\n",
			prP2pBssInfo->ucBssIndex);
		goto error;
	}

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
			"Corresponding P2P Role FSM empty: %d.\n",
			prMsgP2pSetNewChannelMsg->ucRoleIdx);
		goto error;
	}

	prRfChannelInfo = &prMsgP2pSetNewChannelMsg->rRfChannelInfo;
	prChnlReqInfo = &prP2pRoleFsmInfo->rChnlReqInfo;

	prChnlReqInfo->ucReqChnlNum =
		prRfChannelInfo->ucChannelNum;
	prChnlReqInfo->eBand =
		prRfChannelInfo->eBand;
	prChnlReqInfo->eChannelWidth =
		(enum ENUM_CHANNEL_WIDTH)
		rlmGetVhtOpBwByBssOpBw(prRfChannelInfo->ucChnlBw);
	prChnlReqInfo->eChnlSco =
		rlmGetScoByChnInfo(prAdapter, prRfChannelInfo);
	prChnlReqInfo->ucCenterFreqS1 = nicGetS1(
		prChnlReqInfo->eBand,
		prChnlReqInfo->ucReqChnlNum,
		prChnlReqInfo->eChannelWidth);
	prChnlReqInfo->ucCenterFreqS2 = 0;
	prChnlReqInfo->u4MaxInterval = P2P_AP_CHNL_HOLD_TIME_CSA_MS;
	prChnlReqInfo->eChnlReqType = CH_REQ_TYPE_GO_START_BSS;

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/*p2pRoleFsmRunEventSetNewChannel*/

void p2pRoleFsmRunEventCsaDone(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_CSA_DONE *prMsgP2pCsaDoneMsg;
	struct BSS_INFO *prAisBssInfo;
	struct GL_P2P_INFO *prP2PInfo = (struct GL_P2P_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;

	DBGLOG(P2P, TRACE, "p2pRoleFsmRunEventCsaDone\n");

	prMsgP2pCsaDoneMsg = (struct MSG_P2P_CSA_DONE *) prMsgHdr;

	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prMsgP2pCsaDoneMsg->ucBssIndex);
	prAisBssInfo = aisGetConnectedBssInfo(prAdapter);

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pBssInfo->u4PrivateData);
	prP2PInfo = prAdapter->prGlueInfo->prP2PInfo[
			prP2pRoleFsmInfo->ucRoleIndex];
	prChnlReqInfo = &prP2pRoleFsmInfo->rChnlReqInfo;

	if (prP2PInfo)
		prP2PInfo->eChnlSwitchPolicy = CHNL_SWITCH_POLICY_NONE;

	/* SAP */
	if (p2pFuncIsAPMode(prAdapter->rWifiVar
		.prP2PConnSettings[prP2pBssInfo->u4PrivateData])) {
		if (prAdapter->rWifiVar.eDbdcMode != ENUM_DBDC_MODE_DISABLED &&
			prP2pBssInfo->eBand != prChnlReqInfo->eBand) {
			/* Indicate PM abort to sync BSS state with FW */
			nicPmIndicateBssAbort(prAdapter,
				prP2pBssInfo->ucBssIndex);

			nicDeactivateNetwork(prAdapter,
				NETWORK_ID(prP2pBssInfo->ucBssIndex,
				prP2pRoleFsmInfo->ucRoleIndex));
			nicUpdateBss(prAdapter,
				prP2pBssInfo->ucBssIndex);
			p2pChangeMediaState(prAdapter, prP2pBssInfo,
				MEDIA_STATE_DISCONNECTED);
			nicUpdateBssEx(prAdapter,
				prP2pBssInfo->ucBssIndex,
				FALSE);


#if CFG_SUPPORT_DBDC
			cnmDbdcPreConnectionEnableDecision(prAdapter,
				prP2pBssInfo->ucBssIndex,
				prChnlReqInfo->eBand,
				prChnlReqInfo->ucReqChnlNum,
				prP2pBssInfo->ucWmmQueSet);
#endif /*CFG_SUPPORT_DBDC*/

			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_SWITCH_CHANNEL);
		} else {
			/* SAP: Skip channel request/abort for
			 * STA+SAP/MCC concurrent cases.
			 */
#if !CFG_P2P_FORCE_ROC_CSA
			if (prAisBssInfo &&
				(prAisBssInfo->ucPrimaryChannel !=
				prP2pBssInfo->ucPrimaryChannel) &&
				(prAisBssInfo->eConnectionState ==
				MEDIA_STATE_CONNECTED)) {
				p2pFuncDfsSwitchCh(prAdapter,
					prP2pBssInfo,
					prChnlReqInfo);
			} else
#endif
				p2pRoleFsmStateTransition(prAdapter,
					prP2pRoleFsmInfo,
					P2P_ROLE_STATE_SWITCH_CHANNEL);
		}
	} else { /* GO */
		DBGLOG(P2P, INFO, "GO CSA done: %s band\n",
			prP2pBssInfo->eBand == prChnlReqInfo->eBand ?
				"same" : "cross");

		if (prAdapter->rWifiVar.eDbdcMode != ENUM_DBDC_MODE_DISABLED &&
			cnmGet80211Band(prP2pBssInfo->eBand) !=
				cnmGet80211Band(prChnlReqInfo->eBand)) {

			/* Indicate PM abort to sync BSS state with FW */
			nicPmIndicateBssAbort(prAdapter,
				prP2pBssInfo->ucBssIndex);

			/* Update BSS with temp. disconnect state to FW */
			nicDeactivateNetworkEx(prAdapter,
				NETWORK_ID(prP2pRoleFsmInfo->ucBssIndex,
				prP2pRoleFsmInfo->ucRoleIndex),
				FALSE);
			p2pChangeMediaState(prAdapter, prP2pBssInfo,
				MEDIA_STATE_DISCONNECTED);
			nicUpdateBssEx(prAdapter,
				prP2pBssInfo->ucBssIndex,
				FALSE);

#if CFG_SUPPORT_DBDC
			cnmDbdcPreConnectionEnableDecision(prAdapter,
				prP2pBssInfo->ucBssIndex,
				prChnlReqInfo->eBand,
				prChnlReqInfo->ucReqChnlNum,
				prP2pBssInfo->ucWmmQueSet);
#endif /*CFG_SUPPORT_DBDC*/
		}

		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_SWITCH_CHANNEL);
	}
	cnmTimerStopTimer(prAdapter, &prP2pRoleFsmInfo->rP2pCsaDoneTimer);

	cnmMemFree(prAdapter, prMsgHdr);
}				/*p2pRoleFsmRunEventCsaDone*/

void p2pRoleFsmRunEventDfsShutDownTimeout(struct ADAPTER *prAdapter,
		uintptr_t ulParamPtr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) ulParamPtr;

	DBGLOG(P2P, INFO,
		"p2pRoleFsmRunEventDfsShutDownTimeout: DFS shut down.\n");

	p2pFuncSetDfsState(DFS_STATE_INACTIVE);
	p2pFuncStopRdd(prAdapter, prP2pRoleFsmInfo->ucBssIndex);

}				/* p2pRoleFsmRunEventDfsShutDownTimeout */

#endif

void p2pRoleFsmRunEventWaitNextReqChnlTimeout(struct ADAPTER *prAdapter,
		uintptr_t ulParamPtr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) ulParamPtr;

	DBGLOG(P2P, INFO, "timeout\n");

	p2pRoleFsmStateTransition(prAdapter,
		prP2pRoleFsmInfo,
		P2P_ROLE_STATE_REQING_CHANNEL);
} /* p2pRoleFsmRunEventWaitNextReqChnlTimeout */

void
p2pRoleFsmScanTargetBss(struct ADAPTER *prAdapter,
		struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		uint8_t ucChannelNum,
		enum ENUM_BAND eBand,
		struct P2P_SSID_STRUCT *prSsid)
{
	/* Update scan parameter... to scan target device. */
	struct P2P_SCAN_REQ_INFO *prScanReqInfo =
			&(prP2pRoleFsmInfo->rScanReqInfo);

	prScanReqInfo->ucNumChannelList = 1;
	prScanReqInfo->eScanType = SCAN_TYPE_ACTIVE_SCAN;
	prScanReqInfo->eChannelSet = SCAN_CHANNEL_SPECIFIED;
	prScanReqInfo->arScanChannelList[0].ucChannelNum = ucChannelNum;
	prScanReqInfo->arScanChannelList[0].eBand = eBand;
	prScanReqInfo->ucSsidNum = 1;
	kalMemCopy(&(prScanReqInfo->arSsidStruct[0]), prSsid,
			sizeof(struct P2P_SSID_STRUCT));
	/* Prevent other P2P ID in IE. */
	prScanReqInfo->u4BufLength = 0;
	prScanReqInfo->fgIsAbort = TRUE;

	p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_SCAN);
}

void p2pRoleFsmRunEventConnectionRequest(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *bss = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct MSG_P2P_CONNECTION_REQUEST *prP2pConnReqMsg =
		(struct MSG_P2P_CONNECTION_REQUEST *) NULL;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;
	struct P2P_JOIN_INFO *prJoinInfo = (struct P2P_JOIN_INFO *) NULL;
	struct BSS_DESC_SET set;
	uint8_t i;

	prP2pConnReqMsg = (struct MSG_P2P_CONNECTION_REQUEST *) prMsgHdr;

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pConnReqMsg->ucRoleIdx);

	prAdapter->prP2pInfo->eConnState = P2P_CNN_NORMAL;


	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
		       "Corresponding P2P Role FSM empty: %d.\n",
		       prP2pConnReqMsg->ucRoleIdx);
		goto error;
	}

	bss = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

	if (!bss) {
		DBGLOG(P2P, ERROR,
			"prP2pRoleFsmInfo->ucBssIndex %d of prAdapter->aprBssInfo is NULL!\n",
			prP2pRoleFsmInfo->ucBssIndex);
		goto error;
	}

	prConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);
	prJoinInfo = &(prP2pRoleFsmInfo->rJoinInfo);

	DBGLOG(P2P, TRACE, "p2pFsmRunEventConnectionRequest\n");

	if (bss->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		goto error;

	/* In case the network is already activated, we need to re-activate
	 * the network. Otherwise, the connection may be failed in dbdc cases.
	 */
	if (IS_NET_ACTIVE(prAdapter, bss->ucBssIndex)) {
		p2pDeactivateAllLink(prAdapter,
			prP2pRoleFsmInfo,
			TRUE);
	}

	SET_NET_PWR_STATE_ACTIVE(prAdapter, bss->ucBssIndex);

	/* In P2P GC case, the interval of
	 * two ASSOC flow could be very short,
	 * we must start to connect directly before Deauth done
	 */
	prStaRec = bss->prStaRecOfAP;
	if (prStaRec) {
		if (timerPendingTimer(&prStaRec->rDeauthTxDoneTimer)) {
			cnmTimerStopTimer(prAdapter,
				&(prStaRec->rDeauthTxDoneTimer));
			/* Force to stop */
			p2pRoleFsmDeauthComplete(prAdapter, prStaRec);
		}
	}
	/* Reset intended OP mode */
	bss->eIntendOPMode = OP_MODE_P2P_DEVICE;
	/* Make sure the state is in IDLE state. */
	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_IDLE)
		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
	/* Update connection request information. */
	prConnReqInfo->eConnRequest = P2P_CONNECTION_TYPE_GC;
	COPY_MAC_ADDR(prConnReqInfo->aucBssid,
		prP2pConnReqMsg->aucBssid);
	COPY_MAC_ADDR(bss->aucOwnMacAddr,
		prP2pConnReqMsg->aucSrcMacAddr);
	kalMemCopy(&(prConnReqInfo->rSsidStruct),
		&(prP2pConnReqMsg->rSsid),
		sizeof(struct P2P_SSID_STRUCT));
	kalMemCopy(prConnReqInfo->aucIEBuf,
		prP2pConnReqMsg->aucIEBuf,
		prP2pConnReqMsg->u4IELen);
	prConnReqInfo->u4BufLength = prP2pConnReqMsg->u4IELen;

	switch (prP2pConnReqMsg->eAuthMode) {
	case AUTH_MODE_SHARED:
		prJoinInfo->ucAvailableAuthTypes =
			(uint8_t) AUTH_TYPE_SHARED_KEY;
		break;
	case AUTH_MODE_WPA3_SAE:
		prJoinInfo->ucAvailableAuthTypes =
			(uint8_t) AUTH_TYPE_SAE;
		break;
	case AUTH_MODE_OPEN:
	default:
		prJoinInfo->ucAvailableAuthTypes =
			(uint8_t) AUTH_TYPE_OPEN_SYSTEM;
		break;
	}

	/* Find BSS Descriptor first. */
	prJoinInfo->prTargetBssDesc =
		scanP2pSearchDesc(prAdapter, prConnReqInfo,
		&set);
	p2pFillLinkBssDesc(prAdapter,
		prP2pRoleFsmInfo, &set);

#if (CFG_SUPPORT_802_11BE_MLO == 1)
	if (set.ucLinkNum > 1)
		p2pLinkInitGCRole(prAdapter);
#endif

	if (prJoinInfo->prTargetBssDesc == NULL) {
		p2pRoleFsmScanTargetBss(prAdapter,
				prP2pRoleFsmInfo,
				prP2pConnReqMsg->rChannelInfo.ucChannelNum,
				prP2pConnReqMsg->rChannelInfo.eBand,
				&(prP2pConnReqMsg->rSsid));
	} else {
		for (i = 0; i < MLD_LINK_MAX; i++) {
			struct BSS_INFO *prP2pBssInfo =
				p2pGetLinkBssInfo(prAdapter,
				prP2pRoleFsmInfo, i);
			struct BSS_DESC *prBssDesc =
				p2pGetLinkBssDesc(prP2pRoleFsmInfo, i);
			struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
				p2pGetChnlReqInfo(prAdapter,
				prP2pRoleFsmInfo, i);
			uint8_t ucRfBw;

			if (!prP2pBssInfo || !prBssDesc)
				continue;

#if (CFG_DBDC_SW_FOR_P2P_LISTEN == 1)
			p2pRoleP2pLisStopDbdcDecision(prAdapter,
				prConnReqInfo->eConnRequest);
#endif

			prChnlReqInfo->u8Cookie = 0;
			prChnlReqInfo->ucReqChnlNum =
				prBssDesc->ucChannelNum;
			prChnlReqInfo->eBand =
				prBssDesc->eBand;
			prChnlReqInfo->eChnlSco =
				prBssDesc->eSco;
			prChnlReqInfo->u4MaxInterval =
				AIS_JOIN_CH_REQUEST_INTERVAL;
			prChnlReqInfo->eChnlReqType =
				CH_REQ_TYPE_JOIN;

			rlmReviseMaxBw(prAdapter,
				prP2pBssInfo->ucBssIndex,
				&prChnlReqInfo->eChnlSco,
				(enum ENUM_CHANNEL_WIDTH *)
				&prChnlReqInfo->eChannelWidth,
				&prChnlReqInfo->ucCenterFreqS1,
				&prChnlReqInfo->ucReqChnlNum);

			prP2pBssInfo->eBand = prChnlReqInfo->eBand;
			p2pGetLinkWmmQueSet(prAdapter, prP2pBssInfo);

#if CFG_SUPPORT_DBDC
			/* DBDC decsion.may change OpNss */
			cnmDbdcPreConnectionEnableDecision(
			prAdapter,
				prP2pBssInfo->ucBssIndex,
				prChnlReqInfo->eBand,
				prChnlReqInfo->ucReqChnlNum,
				prP2pBssInfo->ucWmmQueSet);
#endif /* CFG_SUPPORT_DBDC */

			cnmOpModeGetTRxNss(
				prAdapter, prP2pBssInfo->ucBssIndex,
				&prP2pBssInfo->ucOpRxNss,
				&prP2pBssInfo->ucOpTxNss);

			DBGLOG(P2P, INFO,
			   "start GC CH[%u]RxNSS[%u]TxNss[%u]\n",
			   prChnlReqInfo->ucReqChnlNum,
			   prP2pBssInfo->ucOpRxNss,
			   prP2pBssInfo->ucOpTxNss);

			/* Decide RF BW by own OP and Peer OP BW */
#if CFG_SUPPORT_DBDC
			ucRfBw = cnmGetDbdcBwCapability(prAdapter,
				prP2pBssInfo->ucBssIndex);
#else
			ucRfBw = cnmGetBssMaxBw(prAdapter,
				prP2pBssInfo->ucBssIndex);
#endif
			/* Revise to VHT OP BW */
			ucRfBw = rlmGetVhtOpBwByBssOpBw(ucRfBw);
			if (ucRfBw > prBssDesc->eChannelWidth)
				ucRfBw = prBssDesc->eChannelWidth;

			prChnlReqInfo->eChannelWidth = ucRfBw;
			/* TODO: BW80+80 support */
			prChnlReqInfo->ucCenterFreqS1 = nicGetS1(
				prChnlReqInfo->eBand,
				prChnlReqInfo->ucReqChnlNum,
				prChnlReqInfo->eChannelWidth);
			prChnlReqInfo->ucCenterFreqS2 = 0;
		}

		p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_REQING_CHANNEL);
	}

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventConnectionRequest */

void p2pRoleFsmRunEventConnectionAbort(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct MSG_P2P_CONNECTION_ABORT *prDisconnMsg =
		(struct MSG_P2P_CONNECTION_ABORT *) NULL;
	struct STA_RECORD *prStaRec = (struct STA_RECORD *) NULL;


	prDisconnMsg = (struct MSG_P2P_CONNECTION_ABORT *) prMsgHdr;

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prDisconnMsg->ucRoleIdx);

	DBGLOG(P2P, TRACE,
		"p2pFsmRunEventConnectionAbort: Connection Abort.\n");

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
		       "p2pRoleFsmRunEventConnectionAbort: Corresponding P2P Role FSM empty: %d.\n",
		       prDisconnMsg->ucRoleIdx);
		goto error;
	}

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex];

	if (!prP2pBssInfo) {
		DBGLOG(P2P, ERROR,
		       "prAdapter->aprBssInfo[prP2pRoleFsmInfo->ucBssIndex(%d)] is NULL!",
		       prP2pRoleFsmInfo->ucBssIndex);
		goto error;
	}

#if CFG_SUPPORT_TDLS_P2P
	prStaRec = cnmGetStaRecByAddress(
		prAdapter,
		prP2pBssInfo->ucBssIndex,
		prDisconnMsg->aucTargetID);
	/*
	 * Do nothing as TDLS disable operation will free this STA REC
	 * when this is TDLS peer.
	 * This operation will go through when as GO/HP.
	 */
	if (prStaRec != NULL &&
		IS_DLS_STA(prStaRec)) {
		DBGLOG(P2P, INFO,
			"[TDLS] Remove [" MACSTR "], do nothing.\n",
			MAC2STR(prDisconnMsg->aucTargetID));
		/* cnmStaRecFree(prAdapter, prStaRec); */
		goto error;
	}
#endif /* CFG_SUPPORT_TDLS_P2P */

	switch (prP2pBssInfo->eCurrentOPMode) {
	case OP_MODE_INFRASTRUCTURE:
		{
			uint8_t aucBCBSSID[] = BC_BSSID;

			if (!prP2pBssInfo->prStaRecOfAP) {
				struct P2P_JOIN_INFO *prJoinInfo;

				DBGLOG(P2P, INFO, "GO's StaRec is NULL\n");
				/* Receive disconnection request during GC join.
				  * Abort GC join to prevent STA record leak.
				  */
				prJoinInfo = &(prP2pRoleFsmInfo->rJoinInfo);
				if (prJoinInfo->prTargetStaRec) {
					p2pFuncDisconnect(prAdapter,
						prP2pBssInfo,
						prJoinInfo->prTargetStaRec,
						FALSE,
						prDisconnMsg->u2ReasonCode,
						TRUE);

					p2pRoleFsmStateTransition(prAdapter,
						prP2pRoleFsmInfo,
						P2P_ROLE_STATE_IDLE);
				}
				break;
			}
			if (UNEQUAL_MAC_ADDR(
					prP2pBssInfo->prStaRecOfAP->aucMacAddr,
					prDisconnMsg->aucTargetID) &&
			    UNEQUAL_MAC_ADDR(prDisconnMsg->aucTargetID,
				    aucBCBSSID)) {
				DBGLOG(P2P, INFO,
				"Unequal MAC ADDR [" MACSTR ":" MACSTR "]\n",
				MAC2STR(
					prP2pBssInfo->prStaRecOfAP->aucMacAddr),
				MAC2STR(prDisconnMsg->aucTargetID));
				break;
			}

			if (IS_NET_PWR_STATE_IDLE(prAdapter,
				prP2pBssInfo->ucBssIndex) &&
				IS_NET_ACTIVE(prAdapter,
					prP2pBssInfo->ucBssIndex) &&
				prP2pBssInfo->eConnectionState ==
					MEDIA_STATE_CONNECTED) {
				DBGLOG(P2P, INFO,
					"under deauth, ignore disconnection procedure\n");
				break;
			}

			prStaRec = prP2pBssInfo->prStaRecOfAP;

			if (prStaRec && prStaRec->fgIsInUse) {
				p2pFuncDisconnect(prAdapter, prP2pBssInfo,
					prStaRec,
					prDisconnMsg->fgSendDeauth,
					prDisconnMsg->u2ReasonCode,
					TRUE);

				cnmTimerStopTimer(prAdapter,
					&(prStaRec->rDeauthTxDoneTimer));

				cnmTimerInitTimer(prAdapter,
					&(prStaRec->rDeauthTxDoneTimer),
					(PFN_MGMT_TIMEOUT_FUNC)
						p2pRoleFsmDeauthTimeout,
					(uintptr_t) prStaRec);

				cnmTimerStartTimer(prAdapter,
					&(prStaRec->rDeauthTxDoneTimer),
					P2P_DEAUTH_TIMEOUT_TIME_MS);
			}

			SET_NET_PWR_STATE_IDLE(prAdapter,
				prP2pBssInfo->ucBssIndex);

			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
		}
		break;
	case OP_MODE_ACCESS_POINT:
		{
			/* Search specific client device, and disconnect. */
			/* 1. Send deauthentication frame. */
			/* 2. Indication: Device disconnect. */
			struct STA_RECORD *prCurrStaRec =
				(struct STA_RECORD *) NULL;

			DBGLOG(P2P, INFO,
				"Disconnecting with Target ID: " MACSTR "\n",
				MAC2STR(prDisconnMsg->aucTargetID));

			prCurrStaRec = bssGetClientByMac(prAdapter,
				prP2pBssInfo,
				prDisconnMsg->aucTargetID);

			if (prCurrStaRec && prCurrStaRec->fgIsInUse) {
				DBGLOG(P2P, TRACE,
					"Disconnecting: " MACSTR "\n",
					MAC2STR(prCurrStaRec->aucMacAddr));

				if ((prP2pBssInfo->u4RsnSelectedAKMSuite ==
					RSN_AKM_SUITE_OWE)) {
					DBGLOG(P2P, INFO,
						"[OWE] Ignore deauth in %d\n",
						prCurrStaRec->eAuthAssocState);
					break;
				}

				/* Glue layer indication. */
				/* kalP2PGOStationUpdate(prAdapter->prGlueInfo,
				 * prCurrStaRec, FALSE);
				 */

				/* Send deauth & do indication. */
				p2pFuncDisconnect(prAdapter,
					prP2pBssInfo,
					prCurrStaRec,
					prDisconnMsg->fgSendDeauth,
					prDisconnMsg->u2ReasonCode,
					TRUE);

				cnmTimerStopTimer(prAdapter,
					&(prCurrStaRec->rDeauthTxDoneTimer));

				cnmTimerInitTimer(prAdapter,
					&(prCurrStaRec->rDeauthTxDoneTimer),
					(PFN_MGMT_TIMEOUT_FUNC)
					p2pRoleFsmDeauthTimeout,
					(uintptr_t) prCurrStaRec);

				cnmTimerStartTimer(prAdapter,
					&(prCurrStaRec->rDeauthTxDoneTimer),
					P2P_DEAUTH_TIMEOUT_TIME_MS);
#if CFG_SUPPORT_802_11W
			} else if (prP2pBssInfo
				->u4RsnSelectedAKMSuite ==
				RSN_AKM_SUITE_SAE) {
				if (!kal_completion_done(
					&prP2pRoleFsmInfo->rDeauthComp)) {
					DBGLOG(P2P, TRACE,
						"Complete rDeauthComp\n");
					complete(&prP2pRoleFsmInfo
						->rDeauthComp);
				}
				prP2pRoleFsmInfo
					->encryptedDeauthIsInProcess = FALSE;
#endif
			}
#if 0
			LINK_FOR_EACH(prLinkEntry, prStaRecOfClientList) {
				prCurrStaRec = LINK_ENTRY(prLinkEntry,
					struct STA_RECORD, rLinkEntry);

				ASSERT(prCurrStaRec);

				if (EQUAL_MAC_ADDR(prCurrStaRec->aucMacAddr,
					prDisconnMsg->aucTargetID)) {

					DBGLOG(P2P, TRACE,
					"Disconnecting: " MACSTR "\n",
					MAC2STR(
					prCurrStaRec->aucMacAddr));

					/* Remove STA from client list. */
					LINK_REMOVE_KNOWN_ENTRY(
						prStaRecOfClientList,
						&prCurrStaRec->rLinkEntry);

					/* Glue layer indication. */
					/* kalP2PGOStationUpdate(
					 * prAdapter->prGlueInfo,
					 * prCurrStaRec, FALSE);
					 */

					/* Send deauth & do indication. */
					p2pFuncDisconnect(prAdapter,
						prP2pBssInfo,
						prCurrStaRec,
						prDisconnMsg->fgSendDeauth,
						prDisconnMsg->u2ReasonCode);

					/* prTargetStaRec = prCurrStaRec; */

					break;
				}
			}
#endif

		}
		break;
	case OP_MODE_P2P_DEVICE:
	default:
		ASSERT(FALSE);
		break;
	}

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventConnectionAbort */

void p2pRoleFsmUpdateBssInfoForJOIN(struct ADAPTER *prAdapter,
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
	struct SW_RFB *prAssocRspSwRfb,
	struct STA_RECORD *prSetupStaRec)
{
	uint8_t i;

	for (i = 0; i < MLD_LINK_MAX; i++) {
		struct BSS_INFO *prP2pLinkBssInfo =
			p2pGetLinkBssInfo(prAdapter,
			prP2pRoleFsmInfo, i);
		struct BSS_DESC *prTargetBssDesc =
			p2pGetLinkBssDesc(prP2pRoleFsmInfo, i);
		struct STA_RECORD *prStaRec =
			p2pGetLinkStaRec(prP2pRoleFsmInfo, i);

		if (!prP2pLinkBssInfo ||
			!prTargetBssDesc ||
			!prStaRec)
			continue;

		/* TODO: mlo, get AID from assoc resp frame */
		prStaRec->u2AssocId = prSetupStaRec->u2AssocId;

		/* 4 <1.1> Change
		 * FW's Media State immediately.
		 */
		p2pChangeMediaState(prAdapter,
			prP2pLinkBssInfo,
			MEDIA_STATE_CONNECTED);

		/* 4 <1.2> Deactivate previous AP's STA_RECORD_T
		 * in Driver if have.
		 */
		if ((prP2pLinkBssInfo->prStaRecOfAP)
			&& (prP2pLinkBssInfo->prStaRecOfAP
			!= prStaRec)) {
			cnmStaRecChangeState(prAdapter,
				prP2pLinkBssInfo->prStaRecOfAP,
				STA_STATE_1);

			cnmStaRecFree(prAdapter,
				prP2pLinkBssInfo->prStaRecOfAP);

			prP2pLinkBssInfo->prStaRecOfAP = NULL;
		}
		/* 4 <1.3> Update BSS_INFO_T */
		if (prAssocRspSwRfb) {
#if (CFG_SUPPORT_802_11BE_MLO == 1)
			if (prStaRec == prSetupStaRec) {
				p2pFuncUpdateBssInfoForJOIN(
				    prAdapter,
				    prTargetBssDesc,
				    prStaRec,
				    prP2pLinkBssInfo,
				    prAssocRspSwRfb);

			} else {
				struct SW_RFB *prSwRfb =
					mldDupAssocSwRfb(
					prAdapter,
					prAssocRspSwRfb,
					prStaRec);

				if (prSwRfb) {
					p2pFuncUpdateBssInfoForJOIN(
					    prAdapter,
					    prTargetBssDesc,
					    prStaRec,
					    prP2pLinkBssInfo,
					    prSwRfb);
					nicRxReturnRFB(prAdapter, prSwRfb);
				}
			}
#else
			p2pFuncUpdateBssInfoForJOIN(prAdapter,
			    prTargetBssDesc,
			    prStaRec,
			    prP2pLinkBssInfo,
			    prAssocRspSwRfb);

#endif

		} else {
			DBGLOG(P2P, INFO,
				"prAssocRspSwRfb is NULL!\n");
		}

		/* 4 <1.4> Activate current AP's STA_RECORD_T
		 * in Driver.
		 */
		cnmStaRecChangeState(prAdapter,
			prStaRec, STA_STATE_3);

#if CFG_SUPPORT_TDLS_P2P_AUTO
		/* fire the update jiffies */
		prP2pLinkBssInfo->ulLastUpdate = kalGetJiffies();
#endif
#if CFG_SUPPORT_P2P_RSSI_QUERY
		/* <1.5> Update RSSI if necessary */
		nicUpdateRSSI(prAdapter,
			prP2pLinkBssInfo->ucBssIndex,
			(int8_t)(RCPI_TO_dBm(prStaRec->ucRCPI)),
			0);
#endif

		/* 4 <1.6> Indicate Connected Event to
		 * Host immediately.
		 * Require BSSID, Association ID,
		 * Beacon Interval..
		 * from AIS_BSS_INFO_T
		 * p2pIndicationOfMediaStateToHost(prAdapter,
		 * MEDIA_STATE_CONNECTED,
		 * prStaRec->aucMacAddr);
		 */
		if (prTargetBssDesc)
			scanReportBss2Cfg80211(prAdapter,
				BSS_TYPE_P2P_DEVICE,
				prTargetBssDesc);
	}
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief    This function is called when JOIN complete message event
 *             is received from SAA.
 *
 * \param[in] prAdapter  Pointer of ADAPTER_T
 *
 * \return none
 */
/*----------------------------------------------------------------------------*/
void p2pRoleFsmRunEventJoinComplete(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_JOIN_INFO *prJoinInfo =
		(struct P2P_JOIN_INFO *) NULL;
	struct MSG_SAA_FSM_COMP *prJoinCompMsg =
		(struct MSG_SAA_FSM_COMP *) NULL;
	struct SW_RFB *prAssocRspSwRfb = (struct SW_RFB *) NULL;
	struct STA_RECORD *prSetupStaRec = (struct STA_RECORD *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;

	prJoinCompMsg = (struct MSG_SAA_FSM_COMP *) prMsgHdr;
	prSetupStaRec = prJoinCompMsg->prStaRec;
	prAssocRspSwRfb = prJoinCompMsg->prSwRfb;

	ASSERT(prSetupStaRec);
	if (!prSetupStaRec) {
		DBGLOG(P2P, ERROR, "prJoinCompMsg->prStaRec is NULL!\n");
		goto error;
	}

	DBGLOG(P2P, INFO,
		"P2P BSS %d [" MACSTR "], Join Complete, status: %d\n",
		prSetupStaRec->ucBssIndex,
		MAC2STR(prSetupStaRec->aucMacAddr),
		prJoinCompMsg->rJoinStatus);

	ASSERT(prSetupStaRec->ucBssIndex < prAdapter->ucP2PDevBssIdx);
	if (!(prSetupStaRec->ucBssIndex < prAdapter->ucP2PDevBssIdx)) {
		DBGLOG(P2P, ERROR,
			"prStaRec->ucBssIndex %d should < prAdapter->ucP2PDevBssIdx(%d)!\n",
			prSetupStaRec->ucBssIndex, prAdapter->ucP2PDevBssIdx);
		goto error;
	}

	prP2pBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter,
			prSetupStaRec->ucBssIndex);
	if (!prP2pBssInfo) {
		DBGLOG(P2P, ERROR, "prP2pBssInfo is NULL!\n");
		goto error;
	}

	if (prP2pBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE) {
		DBGLOG(P2P, ERROR,
			"prP2pBssInfo->eCurrentOPMode %d != OP_MODE_INFRASTRUCTURE(%d)!\n",
			prP2pBssInfo->eCurrentOPMode,
			OP_MODE_INFRASTRUCTURE);
		goto error;
	}

	ASSERT(prP2pBssInfo->u4PrivateData < BSS_P2P_NUM);
	if (!(prP2pBssInfo->u4PrivateData < BSS_P2P_NUM)) {
		DBGLOG(P2P, ERROR,
			"prP2pBssInfo->u4PrivateData %d should < BSS_P2P_NUM(%d)!\n",
			prP2pBssInfo->u4PrivateData,
			BSS_P2P_NUM);
		goto error;
	}

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pBssInfo->u4PrivateData);

	prJoinInfo = &(prP2pRoleFsmInfo->rJoinInfo);

	if (!prJoinInfo->prTargetStaRec) {
		DBGLOG(P2P, ERROR, "prJoinInfo->prTargetStaRec is NULL!\n");
		goto error;
	}

	/* Check SEQ NUM */
	if (prJoinCompMsg->ucSeqNum == prJoinInfo->ucSeqNumOfReqMsg) {
		ASSERT(prSetupStaRec == prJoinInfo->prTargetStaRec);

		if (prJoinCompMsg->rJoinStatus == WLAN_STATUS_SUCCESS) {
			prJoinInfo->fgIsJoinSuccess = TRUE;
			p2pRoleFsmUpdateBssInfoForJOIN(prAdapter,
				prP2pRoleFsmInfo,
				prAssocRspSwRfb,
				prSetupStaRec);

			kalP2PGCIndicateConnectionStatus(prAdapter->prGlueInfo,
				prP2pRoleFsmInfo->ucRoleIndex,
				&prP2pRoleFsmInfo->rConnReqInfo,
				prJoinInfo->aucIEBuf,
				prJoinInfo->u4BufLength,
				prSetupStaRec->u2StatusCode,
				WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY);
		} else {
			/* Join Fail */
			/* 4 <2.1> Redo JOIN process
			 * with other Auth Type if possible
			 */
			if (p2pFuncRetryJOIN(prAdapter,
				prSetupStaRec, prJoinInfo) == FALSE) {

				struct BSS_DESC *prBssDesc;

				prBssDesc = prJoinInfo->prTargetBssDesc;

				if (!prBssDesc) {
					DBGLOG(P2P, WARN,
						"prTargetBssDesc is NULL! Skip retry join\n");
					goto error;
				}

				p2pTargetBssDescResetConnecting(prAdapter,
					prP2pRoleFsmInfo);

				/* Increase Failure Count */
				prSetupStaRec->ucJoinFailureCount++;

				if (prSetupStaRec->ucJoinFailureCount >=
						P2P_SAA_RETRY_COUNT) {
#define DISCONNECT_LOCALLY WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY
					/* Join failed after retries */
					kalP2PGCIndicateConnectionStatus(
						prAdapter->prGlueInfo,
						prP2pRoleFsmInfo->ucRoleIndex,
						&prP2pRoleFsmInfo->rConnReqInfo,
						prJoinInfo->aucIEBuf,
						prJoinInfo->u4BufLength,
						prSetupStaRec->u2StatusCode,
						DISCONNECT_LOCALLY);

					/* Reset p2p state */
					prJoinInfo->prTargetBssDesc = NULL;

					SET_NET_PWR_STATE_IDLE(prAdapter,
						prP2pBssInfo->ucBssIndex);

					p2pRoleFsmStateTransition(prAdapter,
						prP2pRoleFsmInfo,
						P2P_ROLE_STATE_IDLE);

					p2pFuncStopComplete(prAdapter,
						prP2pBssInfo);
				}
			}
		}
	}

	if (prP2pRoleFsmInfo->eCurrentState == P2P_ROLE_STATE_GC_JOIN) {
		if (prP2pBssInfo->eConnectionState ==
				MEDIA_STATE_CONNECTED) {
			/* do nothing & wait for timeout or EAPOL 4/4 TX done */
		} else {
			struct BSS_DESC *prBssDesc;
			struct P2P_SSID_STRUCT rSsid = {0};

			prBssDesc = prJoinInfo->prTargetBssDesc;

			if (prBssDesc) {
				COPY_SSID(rSsid.aucSsid,
					rSsid.ucSsidLen,
					prBssDesc->aucSSID,
					prBssDesc->ucSSIDLen);
				p2pRoleFsmScanTargetBss(prAdapter,
					prP2pRoleFsmInfo,
					prBssDesc->ucChannelNum,
					prBssDesc->eBand,
					&rSsid);
			}
		}
	}

error:
	if (prAssocRspSwRfb)
		nicRxReturnRFB(prAdapter, prAssocRspSwRfb);

	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventJoinComplete */

void p2pRoleFsmRunEventScanRequest(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct MSG_P2P_SCAN_REQUEST *prP2pScanReqMsg =
		(struct MSG_P2P_SCAN_REQUEST *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_SCAN_REQ_INFO *prScanReqInfo =
		(struct P2P_SCAN_REQ_INFO *) NULL;
	uint32_t u4ChnlListSize = 0;
	struct P2P_SSID_STRUCT *prP2pSsidStruct =
		(struct P2P_SSID_STRUCT *) NULL;
	struct BSS_INFO *prP2pBssInfo = NULL;


	prP2pScanReqMsg = (struct MSG_P2P_SCAN_REQUEST *) prMsgHdr;

	prP2pBssInfo = prAdapter->aprBssInfo[prP2pScanReqMsg->ucBssIdx];

	prP2pRoleFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pBssInfo->u4PrivateData);

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR, "prP2pRoleFsmInfo is NULL!");
		goto error;
	}

	prP2pScanReqMsg = (struct MSG_P2P_SCAN_REQUEST *) prMsgHdr;
	prScanReqInfo = &(prP2pRoleFsmInfo->rScanReqInfo);

	DBGLOG(P2P, TRACE, "p2pDevFsmRunEventScanRequest\n");

	/* Do we need to be in IDLE state? */
	/* p2pDevFsmRunEventAbort(prAdapter, prP2pDevFsmInfo); */

	prScanReqInfo->fgIsAbort = TRUE;
	prScanReqInfo->eScanType = prP2pScanReqMsg->eScanType;

	if (prP2pScanReqMsg->u4NumChannel) {
		prScanReqInfo->eChannelSet = SCAN_CHANNEL_SPECIFIED;

		/* Channel List */
		prScanReqInfo->ucNumChannelList = prP2pScanReqMsg->u4NumChannel;
		DBGLOG(P2P, TRACE,
			"Scan Request Channel List Number: %d\n",
			prScanReqInfo->ucNumChannelList);
		if (prScanReqInfo->ucNumChannelList
			> MAXIMUM_OPERATION_CHANNEL_LIST) {
			DBGLOG(P2P, TRACE,
				"Channel List Number Overloaded: %d, change to: %d\n",
				prScanReqInfo->ucNumChannelList,
				MAXIMUM_OPERATION_CHANNEL_LIST);
			prScanReqInfo->ucNumChannelList =
				MAXIMUM_OPERATION_CHANNEL_LIST;
		}

		u4ChnlListSize =
			sizeof(struct RF_CHANNEL_INFO)
			* prScanReqInfo->ucNumChannelList;
		kalMemCopy(prScanReqInfo->arScanChannelList,
			   prP2pScanReqMsg->arChannelListInfo, u4ChnlListSize);
	} else {
		/* If channel number is ZERO.
		 * It means do a FULL channel scan.
		 */
		prScanReqInfo->eChannelSet = SCAN_CHANNEL_FULL;
	}

	/* SSID */
	prP2pSsidStruct = prP2pScanReqMsg->prSSID;
	for (prScanReqInfo->ucSsidNum = 0;
	     prScanReqInfo->ucSsidNum < prP2pScanReqMsg->i4SsidNum;
		 prScanReqInfo->ucSsidNum++) {

		kalMemCopy(
			prScanReqInfo->arSsidStruct[prScanReqInfo->ucSsidNum]
				.aucSsid,
			prP2pSsidStruct->aucSsid, prP2pSsidStruct->ucSsidLen);

		prScanReqInfo->arSsidStruct[prScanReqInfo->ucSsidNum]
				.ucSsidLen =
			prP2pSsidStruct->ucSsidLen;

		prP2pSsidStruct++;
	}

	/* IE Buffer */
	kalMemCopy(prScanReqInfo->aucIEBuf,
		prP2pScanReqMsg->pucIEBuf,
		prP2pScanReqMsg->u4IELen);

	prScanReqInfo->u4BufLength = prP2pScanReqMsg->u4IELen;
	prScanReqInfo->eScanReason = prP2pScanReqMsg->eScanReason;

	/* bssid */
	COPY_MAC_ADDR(prScanReqInfo->aucBSSID, prP2pScanReqMsg->aucBSSID);

	p2pRoleFsmStateTransition(prAdapter,
		prP2pRoleFsmInfo,
		P2P_ROLE_STATE_SCAN);

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pDevFsmRunEventScanRequest */

void
p2pRoleFsmRunEventScanDone(struct ADAPTER *prAdapter,
	struct MSG_HDR *prMsgHdr,
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo)
{
	struct MSG_SCN_SCAN_DONE *prScanDoneMsg =
		(struct MSG_SCN_SCAN_DONE *) prMsgHdr;
	struct P2P_SCAN_REQ_INFO *prScanReqInfo =
		(struct P2P_SCAN_REQ_INFO *) NULL;
	enum ENUM_P2P_ROLE_STATE eNextState = P2P_ROLE_STATE_NUM;
	struct P2P_CONNECTION_REQ_INFO *prConnReqInfo =
		&(prP2pRoleFsmInfo->rConnReqInfo);
	struct P2P_JOIN_INFO *prP2pJoinInfo =
		&(prP2pRoleFsmInfo->rJoinInfo);
	struct P2P_SCAN_REQ_INFO *prScanInfo;

	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, TRACE, "prP2pRoleFsmInfo is NULL\n");
		goto error;
	}

	DBGLOG(P2P, TRACE, "P2P Role Scan Done Event\n");

	prScanReqInfo = &(prP2pRoleFsmInfo->rScanReqInfo);
	prScanDoneMsg = (struct MSG_SCN_SCAN_DONE *) prMsgHdr;
	prScanInfo = &prP2pRoleFsmInfo->rScanReqInfo;

	if (prScanDoneMsg->ucSeqNum != prScanReqInfo->ucSeqNumOfScnMsg) {
		/* Scan Done message sequence number mismatch.
		 * Ignore this event. (P2P FSM issue two scan events.)
		 */
		/* The scan request has been cancelled.
		 * Ignore this message. It is possible.
		 */
		DBGLOG(P2P, TRACE,
		       "P2P Role Scan Don SeqNum Received:%d <-> P2P Role Fsm SCAN Seq Issued:%d\n",
		       prScanDoneMsg->ucSeqNum,
		       prScanReqInfo->ucSeqNumOfScnMsg);

		goto error;
	}

	switch (prP2pRoleFsmInfo->eCurrentState) {
	case P2P_ROLE_STATE_SCAN:
		prScanReqInfo->fgIsAbort = FALSE;

		if (prConnReqInfo->eConnRequest == P2P_CONNECTION_TYPE_GC) {

			prP2pJoinInfo->prTargetBssDesc =
				p2pFuncKeepOnConnection(prAdapter,
					prAdapter->aprBssInfo
						[prP2pRoleFsmInfo->ucBssIndex],
					prConnReqInfo,
					prP2pRoleFsmInfo,
					&prP2pRoleFsmInfo->rScanReqInfo);
			if ((prP2pJoinInfo->prTargetBssDesc) == NULL) {
				eNextState = P2P_ROLE_STATE_SCAN;
			} else {
				uint8_t i;

				for (i = 0; i < MLD_LINK_MAX; i++) {
					struct BSS_INFO *prP2pBssInfo =
						p2pGetLinkBssInfo(
						prAdapter,
						prP2pRoleFsmInfo, i);
					struct P2P_CHNL_REQ_INFO *
						prChnlReqInfo =
						p2pGetChnlReqInfo(prAdapter,
						prP2pRoleFsmInfo, i);

					if (!prP2pBssInfo || !prChnlReqInfo)
						continue;

					prP2pBssInfo->eBand =
						prChnlReqInfo->eBand;
					p2pGetLinkWmmQueSet(
						prAdapter, prP2pBssInfo);
#if CFG_SUPPORT_DBDC
#if (CFG_DBDC_SW_FOR_P2P_LISTEN == 1)
					p2pRoleP2pLisStopDbdcDecision(prAdapter,
						prConnReqInfo->eConnRequest);
#endif
					/* DBDC decsion.may change OpNss */
					cnmDbdcPreConnectionEnableDecision(
						prAdapter,
						prP2pBssInfo->ucBssIndex,
						prChnlReqInfo->eBand,
						prChnlReqInfo->ucReqChnlNum,
						prP2pBssInfo->ucWmmQueSet);
#endif /* CFG_SUPPORT_DBDC */

					cnmOpModeGetTRxNss(
						prAdapter,
						prP2pBssInfo->ucBssIndex,
						&prP2pBssInfo->ucOpRxNss,
						&prP2pBssInfo->ucOpTxNss);

					DBGLOG(P2P, INFO,
						"start GC CH[%u]RxNSS[%u]TxNss[%u]\n",
						prChnlReqInfo->ucReqChnlNum,
						prP2pBssInfo->ucOpRxNss,
						prP2pBssInfo->ucOpTxNss);
				}
				/* For GC join. */
				eNextState = P2P_ROLE_STATE_REQING_CHANNEL;
			}
		} else if (prScanInfo->eScanReason == SCAN_REASON_ACS) {
			struct P2P_ACS_REQ_INFO *prAcsReqInfo;

			prAcsReqInfo = &prP2pRoleFsmInfo->rAcsReqInfo;
			prScanInfo->eScanReason = SCAN_REASON_UNKNOWN;
			p2pFunCalAcsChnScores(prAdapter);
			if (wlanQueryLteSafeChannel(prAdapter,
					prP2pRoleFsmInfo->ucRoleIndex) ==
					WLAN_STATUS_SUCCESS) {
				/* do nothing & wait for FW event */
			} else {
				DBGLOG(P2P, WARN, "query safe chn fail.\n");
				p2pFunProcessAcsReport(prAdapter,
						prP2pRoleFsmInfo->ucRoleIndex,
						NULL,
						prAcsReqInfo);
			}
			eNextState = P2P_ROLE_STATE_IDLE;
		} else {
			eNextState = P2P_ROLE_STATE_IDLE;
		}
		break;
	case P2P_ROLE_STATE_AP_CHNL_DETECTION:
		eNextState = P2P_ROLE_STATE_REQING_CHANNEL;
		break;
	default:
		/* Unexpected channel scan done event
		 * without being chanceled.
		 */
		DBGLOG(P2P, WARN,
			"Unexpected scan done for state [%s].\n",
			p2pRoleFsmGetFsmState(prP2pRoleFsmInfo->eCurrentState));
		prScanReqInfo->fgIsScanRequest = FALSE;
		goto error;
	}

	prScanReqInfo->fgIsScanRequest = FALSE;

	p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo, eNextState);

error:
	cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pRoleFsmRunEventScanDone */

void
p2pRoleFsmRunEventChnlGrant(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr,
		struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo)
{
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
		(struct P2P_CHNL_REQ_INFO *) NULL;
	struct MSG_CH_GRANT *prMsgChGrant = (struct MSG_CH_GRANT *) NULL;
#if (CFG_SUPPORT_DFS_MASTER == 1)
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	uint32_t u4CacTimeMs;
	uint8_t ucVhtChannelWidthAfterCsa = VHT_OP_CHANNEL_WIDTH_20_40;
#endif
	uint8_t ucTokenID = 0;


	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR, "prP2pRoleFsmInfo is NULL!\n");
		goto error;
	}

	DBGLOG(P2P, TRACE, "P2P Run Event Role Channel Grant\n");

	prMsgChGrant = (struct MSG_CH_GRANT *) prMsgHdr;
	ucTokenID = prMsgChGrant->ucTokenID;
	prChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);

#if (CFG_SUPPORT_DFS_MASTER == 1)
	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, prMsgChGrant->ucBssIndex);
	if (!prBssInfo)
		return;
#endif
	if (prChnlReqInfo->u4MaxInterval != prMsgChGrant->u4GrantInterval) {
		DBGLOG(P2P, WARN,
			"P2P Role:%d Request Channel Interval:%d, Grant Interval:%d\n",
			prP2pRoleFsmInfo->ucRoleIndex,
			prChnlReqInfo->u4MaxInterval,
			prMsgChGrant->u4GrantInterval);
		prChnlReqInfo->u4MaxInterval = prMsgChGrant->u4GrantInterval;
	}

	if (ucTokenID == prChnlReqInfo->ucSeqNumOfChReq) {
		enum ENUM_P2P_ROLE_STATE eNextState = P2P_ROLE_STATE_NUM;

		switch (prP2pRoleFsmInfo->eCurrentState) {
		case P2P_ROLE_STATE_REQING_CHANNEL:
			switch (prChnlReqInfo->eChnlReqType) {
			case CH_REQ_TYPE_JOIN:
				eNextState = P2P_ROLE_STATE_GC_JOIN;
				break;
			case CH_REQ_TYPE_GO_START_BSS:
				eNextState = P2P_ROLE_STATE_IDLE;
				break;
			case CH_REQ_TYPE_OFFCHNL_TX:
				eNextState = P2P_ROLE_STATE_OFF_CHNL_TX;
				break;
			default:
				DBGLOG(P2P, WARN,
				       "p2pRoleFsmRunEventChnlGrant: Invalid Channel Request Type:%d\n",
				       prChnlReqInfo->eChnlReqType);
				GL_DEFAULT_RESET_TRIGGER(prAdapter,
					       RST_P2P_CHNL_GRANT_INVALID_TYPE);
				break;
			}

			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo, eNextState);
			break;

#if (CFG_SUPPORT_DFS_MASTER == 1)
		case P2P_ROLE_STATE_DFS_CAC:
			rlmDomainSetDfsDbdcBand(prMsgChGrant->eDBDCBand);

			if (prMsgChGrant->ucBssIndex < (MAX_BSSID_NUM + 1))
				p2pFuncStartRdd(prAdapter,
					prMsgChGrant->ucBssIndex);

			if (p2pFuncCheckWeatherRadarBand(prChnlReqInfo))
				u4CacTimeMs =
					P2P_AP_CAC_WEATHER_CHNL_HOLD_TIME_MS;
			else
				u4CacTimeMs =
					prP2pRoleFsmInfo->rChnlReqInfo
						.u4MaxInterval;

			if (p2pFuncIsManualCac())
				u4CacTimeMs = p2pFuncGetDriverCacTime() * 1000;
			else
				p2pFuncSetDriverCacTime(u4CacTimeMs/1000);

			cnmTimerStartTimer(prAdapter,
				&(prP2pRoleFsmInfo->rP2pRoleFsmTimeoutTimer),
				u4CacTimeMs);

			p2pFuncRecordCacStartBootTime();
			p2pFuncSetRadarDetectMode(0);
			p2pFuncSetDfsState(DFS_STATE_CHECKING);

			DBGLOG(P2P, INFO,
				"p2pRoleFsmRunEventChnlGrant: CAC time = %ds\n",
				u4CacTimeMs/1000);
			break;
		case P2P_ROLE_STATE_SWITCH_CHANNEL:
			prBssInfo->fgIsSwitchingChnl = FALSE;

			/* Restore connection state only for P2P CSA */
			if (!p2pFuncIsAPMode(prAdapter->rWifiVar.
				prP2PConnSettings[prBssInfo->u4PrivateData])) {
				p2pChangeMediaState(prAdapter, prBssInfo,
					MEDIA_STATE_CONNECTED);
			}

			/* GC */
			if (prBssInfo->eIftype == IFTYPE_P2P_CLIENT) {
				/* Renew NSS */
				cnmOpModeGetTRxNss(prAdapter,
					prBssInfo->ucBssIndex,
					&prBssInfo->ucOpRxNss,
					&prBssInfo->ucOpTxNss);

				nicUpdateBss(prAdapter, prBssInfo->ucBssIndex);

				p2pFuncCsaUpdateGcStaRec(prBssInfo);

				/* Indicate op mode change to update BW/NSS.
				 * Note that we have to temporarily set VHT
				 * channel width to the one before CSA.
				 * Otherwise, op mode change will not work.
				 */
				ucVhtChannelWidthAfterCsa =
					prBssInfo->ucVhtChannelWidth;
				prBssInfo->ucVhtChannelWidth =
					prBssInfo->ucVhtChannelWidthBeforeCsa;

				rlmChangeOperationMode(
					prAdapter, prBssInfo->ucBssIndex,
					rlmGetBssOpBwByOwnAndPeerCapability(
						prAdapter, prBssInfo),
					prBssInfo->ucOpRxNss,
					prBssInfo->ucOpTxNss,
					TRUE,
					rlmDummyChangeOpHandler);

				/* Restore VHT channel width after CSA */
				prBssInfo->ucVhtChannelWidth =
					ucVhtChannelWidthAfterCsa;

				/* Indicate channel switch to kernel */
				prAdapter->prGlueInfo->
					prP2PInfo[prBssInfo->u4PrivateData]->
					fgChannelSwitchReq = TRUE;
				kalP2pIndicateChnlSwitch(prAdapter,
					prBssInfo);
			} else { /* GO */
				p2pFuncDfsSwitchCh(prAdapter,
					prBssInfo,
					&prP2pRoleFsmInfo->rChnlReqInfo);
			}

			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
			break;
#endif
		case P2P_ROLE_STATE_OFF_CHNL_TX:
			if (prMsgChGrant->eReqType == CH_REQ_TYPE_OFFCHNL_TX) {
				p2pRoleFsmStateTransition(prAdapter,
					prP2pRoleFsmInfo,
					P2P_ROLE_STATE_OFF_CHNL_TX);
			} else {
				p2pRoleFsmStateTransition(prAdapter,
					prP2pRoleFsmInfo,
					P2P_ROLE_STATE_IDLE);
			}
			break;
		default:
			/* Channel is granted under unexpected state.
			 * Driver should cancel channel privileagea
			 * before leaving the states.
			 */
			if (IS_BSS_ACTIVE(
				prAdapter->aprBssInfo
					[prP2pRoleFsmInfo->ucBssIndex])) {
				DBGLOG(P2P, WARN,
				       "p2pRoleFsmRunEventChnlGrant: Invalid CurrentState:%d\n",
				       prP2pRoleFsmInfo->eCurrentState);
				GL_DEFAULT_RESET_TRIGGER(prAdapter,
					      RST_P2P_CHNL_GRANT_INVALID_STATE);
			}
			break;
		}
	} else {
		DBGLOG(P2P, ERROR,
			"p2pRoleFsmRunEventChnlGrant: Token mismatch, Chreq: %d, ChGrant: %d\n",
			prChnlReqInfo->ucSeqNumOfChReq, ucTokenID);
	}

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventChnlGrant */

/* ////////////////////////////////////// */
void p2pRoleFsmRunEventDissolve(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	/* TODO: */

	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventDissolve */

/*----------------------------------------------------------------------------*/
/*!
 * @	This routine update the current MAC table based on the current ACL.
 *	If ACL change causing an associated STA become un-authorized. This STA
 *	will be kicked out immediately.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] ucBssIdx            Bss index.
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void p2pRoleUpdateACLEntry(struct ADAPTER *prAdapter, uint8_t ucBssIdx)
{
	u_int8_t bMatchACL = FALSE;
	int32_t i = 0;
	struct LINK *prClientList;
	struct STA_RECORD *prCurrStaRec, *prNextStaRec;
	struct BSS_INFO *prP2pBssInfo;

	ASSERT(prAdapter);

	if ((!prAdapter) || (ucBssIdx > prAdapter->ucHwBssIdNum))
		return;

	DBGLOG(P2P, TRACE, "Update ACL Entry ucBssIdx = %d\n", ucBssIdx);
	prP2pBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	/* ACL is disabled. Do nothing about the MAC table. */
	if (prP2pBssInfo->rACL.ePolicy == PARAM_CUSTOM_ACL_POLICY_DISABLE)
		return;

	prClientList = &prP2pBssInfo->rStaRecOfClientList;

	LINK_FOR_EACH_ENTRY_SAFE(prCurrStaRec,
		prNextStaRec, prClientList, rLinkEntry, struct STA_RECORD) {
		if (!prCurrStaRec)
			break;
		bMatchACL = FALSE;
		for (i = 0; i < prP2pBssInfo->rACL.u4Num; i++) {
			if (EQUAL_MAC_ADDR(prCurrStaRec->aucMacAddr,
				prP2pBssInfo->rACL.rEntry[i].aucAddr)) {
				bMatchACL = TRUE;
				break;
			}
		}

		if (((!bMatchACL) &&
			(prP2pBssInfo->rACL.ePolicy
				== PARAM_CUSTOM_ACL_POLICY_ACCEPT))
			|| ((bMatchACL) &&
			(prP2pBssInfo->rACL.ePolicy
				== PARAM_CUSTOM_ACL_POLICY_DENY))) {
			struct MSG_P2P_CONNECTION_ABORT *prDisconnectMsg =
				(struct MSG_P2P_CONNECTION_ABORT *) NULL;

			DBGLOG(P2P, TRACE,
				"ucBssIdx=%d, ACL Policy=%d\n",
				ucBssIdx, prP2pBssInfo->rACL.ePolicy);

			prDisconnectMsg =
				(struct MSG_P2P_CONNECTION_ABORT *)
				cnmMemAlloc(prAdapter, RAM_TYPE_MSG,
				sizeof(struct MSG_P2P_CONNECTION_ABORT));
			if (prDisconnectMsg == NULL)
				return;
			prDisconnectMsg->rMsgHdr.eMsgId
				= MID_MNY_P2P_CONNECTION_ABORT;
			prDisconnectMsg->ucRoleIdx
				=  (uint8_t) prP2pBssInfo->u4PrivateData;
			COPY_MAC_ADDR(prDisconnectMsg->aucTargetID,
				prCurrStaRec->aucMacAddr);
			prDisconnectMsg->u2ReasonCode
				= STATUS_CODE_REQ_DECLINED;
			prDisconnectMsg->fgSendDeauth = TRUE;
			mboxSendMsg(prAdapter,
				MBOX_ID_0,
				(struct MSG_HDR *) prDisconnectMsg,
				MSG_SEND_METHOD_BUF);
		}
	}
} /* p2pRoleUpdateACLEntry */

/*----------------------------------------------------------------------------*/
/*!
 * @ Check if the specified STA pass the Access Control List inspection.
 *	If fails to pass the checking,
 *  then no authentication or association is allowed.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] pMacAddr           Pointer to the mac address.
 * @param[in] ucBssIdx            Bss index.
 *
 * @return TRUE - pass ACL inspection, FALSE - ACL inspection fail
 */
/*----------------------------------------------------------------------------*/
u_int8_t p2pRoleProcessACLInspection(struct ADAPTER *prAdapter,
		uint8_t *pMacAddr,
		uint8_t ucBssIdx)
{
	u_int8_t bPassACL = TRUE;
	int32_t i = 0;
	struct BSS_INFO *prP2pBssInfo;

	ASSERT(prAdapter);

	if ((!prAdapter) || (!pMacAddr) || (ucBssIdx > prAdapter->ucHwBssIdNum))
		return FALSE;

	prP2pBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	if (prP2pBssInfo->rACL.ePolicy == PARAM_CUSTOM_ACL_POLICY_DISABLE)
		return TRUE;

	if (prP2pBssInfo->rACL.ePolicy == PARAM_CUSTOM_ACL_POLICY_ACCEPT)
		bPassACL = FALSE;
	else
		bPassACL = TRUE;

	for (i = 0; i < prP2pBssInfo->rACL.u4Num; i++) {
		if (EQUAL_MAC_ADDR(pMacAddr,
			prP2pBssInfo->rACL.rEntry[i].aucAddr)) {
			bPassACL = !bPassACL;
			break;
		}
	}

	if (bPassACL == FALSE)
		DBGLOG(P2P, WARN,
		"this mac [" MACSTR "] is fail to pass ACL inspection.\n",
		MAC2STR(pMacAddr));

	return bPassACL;
} /* p2pRoleProcessACLInspection */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will indicate the Event
 *           of Successful Completion of AAA Module.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] prStaRec           Pointer to the STA_RECORD_T
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
uint32_t
p2pRoleFsmRunEventAAACompleteImpl(struct ADAPTER *prAdapter,
		struct STA_RECORD *prStaRec,
		struct BSS_INFO *prP2pBssInfo)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	enum ENUM_PARAM_MEDIA_STATE eOriMediaState;

	do {
		ASSERT_BREAK((prAdapter != NULL)
			&& (prStaRec != NULL) && (prP2pBssInfo != NULL));

		eOriMediaState = prP2pBssInfo->eConnectionState;

		bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec);

		if (prP2pBssInfo->rStaRecOfClientList.u4NumElem
			>= P2P_MAXIMUM_CLIENT_COUNT
			|| !p2pRoleProcessACLInspection(prAdapter,
					prStaRec->aucMacAddr,
					prP2pBssInfo->ucBssIndex)
#if CFG_SUPPORT_HOTSPOT_WPS_MANAGER
			|| kalP2PMaxClients(prAdapter->prGlueInfo,
				prP2pBssInfo->rStaRecOfClientList.u4NumElem,
				(uint8_t) prP2pBssInfo->u4PrivateData)
#endif
		) {
			rStatus = WLAN_STATUS_RESOURCES;
			break;
		}

		bssAddClient(prAdapter, prP2pBssInfo, prStaRec);

		prStaRec->u2AssocId = bssAssignAssocID(prStaRec);

		cnmStaRecChangeState(prAdapter, prStaRec, STA_STATE_3);

		p2pChangeMediaState(prAdapter,
			prP2pBssInfo,
			MEDIA_STATE_CONNECTED);

		/* Update Connected state to FW. */
		if (eOriMediaState != prP2pBssInfo->eConnectionState)
			nicUpdateBss(prAdapter, prP2pBssInfo->ucBssIndex);

	} while (FALSE);

	return rStatus;
}				/* p2pRunEventAAAComplete */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will indicate the Event
 *           of Successful Completion of AAA Module.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] prStaRec           Pointer to the STA_RECORD_T
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
uint32_t
p2pRoleFsmRunEventAAASuccessImpl(struct ADAPTER *prAdapter,
		struct STA_RECORD *prStaRec,
		struct BSS_INFO *prP2pBssInfo)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;

	do {
		ASSERT_BREAK((prAdapter != NULL)
			&& (prStaRec != NULL) && (prP2pBssInfo != NULL));

		if ((prP2pBssInfo->eNetworkType != NETWORK_TYPE_P2P)
			|| (prP2pBssInfo->u4PrivateData >= BSS_P2P_NUM)) {
			ASSERT(FALSE);
			rStatus = WLAN_STATUS_INVALID_DATA;
			break;
		}

		ASSERT(prP2pBssInfo->ucBssIndex < prAdapter->ucP2PDevBssIdx);

		prP2pRoleFsmInfo =
			P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
				prP2pBssInfo->u4PrivateData);

		if (prP2pBssInfo->u4RsnSelectedAKMSuite ==
			RSN_AKM_SUITE_OWE) {
			DBGLOG(P2P, INFO,
				"[OWE] Bypass new_sta\n");
			break;
		}

		/* Glue layer indication. */
		kalP2PGOStationUpdate(prAdapter->prGlueInfo,
			prP2pRoleFsmInfo->ucRoleIndex, prStaRec, TRUE);

	} while (FALSE);

	return rStatus;
}				/* p2pRoleFsmRunEventAAASuccess */

/*----------------------------------------------------------------------------*/
/*!
 * @brief This function will indicate the Event
 *           of Tx Fail of AAA Module.
 *
 * @param[in] prAdapter          Pointer to the Adapter structure.
 * @param[in] prStaRec           Pointer to the STA_RECORD_T
 *
 * @return (none)
 */
/*----------------------------------------------------------------------------*/
void p2pRoleFsmRunEventAAATxFailImpl(struct ADAPTER *prAdapter,
		struct STA_RECORD *prStaRec,
		struct BSS_INFO *prP2pBssInfo)
{
	ASSERT(prAdapter);
	ASSERT(prStaRec);

	bssRemoveClient(prAdapter, prP2pBssInfo, prStaRec);

	p2pFuncDisconnect(prAdapter,
		prP2pBssInfo, prStaRec, FALSE,
		prStaRec->eAuthAssocState == AAA_STATE_SEND_AUTH2
		? STATUS_CODE_AUTH_TIMEOUT
		: STATUS_CODE_ASSOC_TIMEOUT,
		TRUE);
}				/* p2pRoleFsmRunEventAAATxFail */

void p2pRoleFsmRunEventSwitchOPMode(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_SWITCH_OP_MODE *prSwitchOpMode =
		(struct MSG_P2P_SWITCH_OP_MODE *) prMsgHdr;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;

	ASSERT(prSwitchOpMode->ucRoleIdx < BSS_P2P_NUM);
	if (!(prSwitchOpMode->ucRoleIdx < BSS_P2P_NUM)) {
		DBGLOG(P2P, ERROR,
			"prSwitchOpMode->ucRoleIdx %d should < BSS_P2P_NUM(%d)\n",
			prSwitchOpMode->ucRoleIdx, BSS_P2P_NUM);
		goto error;
	}

	prP2pRoleFsmInfo =
		prAdapter->rWifiVar
			.aprP2pRoleFsmInfo[prSwitchOpMode->ucRoleIdx];
	if (!prP2pRoleFsmInfo) {
		DBGLOG(P2P, ERROR,
			"Null fsm Info Structure (%d)\n",
			prSwitchOpMode->ucRoleIdx);
		goto error;
	}

	prConnReqInfo = &(prP2pRoleFsmInfo->rConnReqInfo);

	ASSERT(prP2pRoleFsmInfo->ucBssIndex < prAdapter->ucP2PDevBssIdx);
	if (!(prP2pRoleFsmInfo->ucBssIndex < prAdapter->ucP2PDevBssIdx)) {
		DBGLOG(P2P, ERROR,
			"prP2pRoleFsmInfo->ucBssIndex %d should < prAdapter->ucP2PDevBssIdx(%d)\n",
			prP2pRoleFsmInfo->ucBssIndex,
			prAdapter->ucP2PDevBssIdx);
		goto error;
	}

	if (!(prSwitchOpMode->eOpMode < OP_MODE_NUM)) {
		DBGLOG(P2P, ERROR,
			"prSwitchOpMode->eOpMode %d should < OP_MODE_NUM(%d)\n",
			prSwitchOpMode->eOpMode, OP_MODE_NUM);
		goto error;
	}

	prP2pBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter,
			prP2pRoleFsmInfo->ucBssIndex);
	if (!prP2pBssInfo) {
		DBGLOG(P2P, ERROR,
			"prP2pBssInfo is NULL");
		goto error;
	}

	/* P2P Device / GC. */
	p2pFuncSwitchOPMode(prAdapter,
		prP2pBssInfo,
		prSwitchOpMode->eOpMode,
		TRUE);

	if (prP2pBssInfo->eIftype == IFTYPE_P2P_CLIENT &&
		prSwitchOpMode->eIftype == IFTYPE_STATION) {
		kalP2pUnlinkBss(prAdapter->prGlueInfo,
			prConnReqInfo->aucBssid);
	}
	prP2pBssInfo->eIftype = prSwitchOpMode->eIftype;

	if (prP2pBssInfo->eIftype != IFTYPE_AP)
		p2pFuncInitConnectionSettings(prAdapter,
		prAdapter->rWifiVar.prP2PConnSettings
		[prSwitchOpMode->ucRoleIdx], FALSE);

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventSwitchOPMode */

/* /////////////////////////////// TO BE REFINE //////////////////////////// */

void p2pRoleFsmRunEventBeaconUpdate(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct P2P_ROLE_FSM_INFO *prRoleP2pFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) NULL;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_P2P_BEACON_UPDATE *prBcnUpdateMsg =
		(struct MSG_P2P_BEACON_UPDATE *) NULL;
	struct P2P_BEACON_UPDATE_INFO *prBcnUpdateInfo =
		(struct P2P_BEACON_UPDATE_INFO *) NULL;
	struct P2P_CONNECTION_REQ_INFO *prP2pConnReqInfo =
		(struct P2P_CONNECTION_REQ_INFO *) NULL;


	DBGLOG(P2P, TRACE, "p2pRoleFsmRunEventBeaconUpdate\n");

	prBcnUpdateMsg = (struct MSG_P2P_BEACON_UPDATE *) prMsgHdr;
	if (!prBcnUpdateMsg || prBcnUpdateMsg->ucRoleIndex >= BSS_P2P_NUM)
		goto error;

	prRoleP2pFsmInfo =
		P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prBcnUpdateMsg->ucRoleIndex);
	if (!prRoleP2pFsmInfo)
		goto error;

	prP2pBssInfo =
		GET_BSS_INFO_BY_INDEX(prAdapter,
			prRoleP2pFsmInfo->ucBssIndex);
	if (!prP2pBssInfo || !prP2pBssInfo->prBeacon)
		goto error;

	prP2pConnReqInfo = &(prRoleP2pFsmInfo->rConnReqInfo);

	prP2pBssInfo->fgIsWepCipherGroup = prBcnUpdateMsg->fgIsWepCipher;

	prBcnUpdateInfo = &(prRoleP2pFsmInfo->rBeaconUpdateInfo);

	DBGLOG(P2P, TRACE, "Dump beacon content from supplicant.\n");
	if (aucDebugModule[DBG_P2P_IDX] & DBG_CLASS_TRACE) {
		dumpMemory8((uint8_t *) prBcnUpdateMsg->pucBcnBody,
			(uint32_t) prBcnUpdateMsg->u4BcnBodyLen);
	}

	p2pFuncBeaconUpdate(prAdapter,
		prP2pBssInfo,
		prBcnUpdateInfo,
		prBcnUpdateMsg->pucBcnHdr,
		prBcnUpdateMsg->u4BcnHdrLen,
		prBcnUpdateMsg->pucBcnBody,
		prBcnUpdateMsg->u4BcnBodyLen);

	if (prBcnUpdateMsg->pucAssocRespIE != NULL
		&& prBcnUpdateMsg->u4AssocRespLen > 0) {
		DBGLOG(P2P, TRACE,
			"Copy extra IEs for assoc resp (Length= %d)\n",
			prBcnUpdateMsg->u4AssocRespLen);
		DBGLOG_MEM8(P2P, TRACE,
			prBcnUpdateMsg->pucAssocRespIE,
			prBcnUpdateMsg->u4AssocRespLen);

		if (p2pFuncAssocRespUpdate(prAdapter,
			prP2pBssInfo,
			prBcnUpdateMsg->pucAssocRespIE,
			prBcnUpdateMsg->u4AssocRespLen) == WLAN_STATUS_FAILURE)
			DBGLOG(P2P, ERROR,
				"Nss%d, Update extra IEs for asso resp fail!\n",
				wlanGetSupportNss(prAdapter,
				prP2pBssInfo->ucBssIndex));
	}

	if ((prP2pBssInfo->eCurrentOPMode == OP_MODE_ACCESS_POINT) &&
		(prP2pBssInfo->eIntendOPMode == OP_MODE_NUM)) {

		/* AP is created, Beacon Update. */
		/* nicPmIndicateBssAbort(prAdapter, NETWORK_TYPE_P2P_INDEX); */


		DBGLOG(P2P, TRACE,
			"p2pRoleFsmRunEventBeaconUpdate with Bssidex(%d)\n",
			prRoleP2pFsmInfo->ucBssIndex);

		bssUpdateBeaconContent(prAdapter, prRoleP2pFsmInfo->ucBssIndex);

#if (CFG_SUPPORT_WIFI_6G == 1)
		if (prP2pBssInfo->eBand == BAND_6G) {
			/* Update unsolicited probe response as beacon */
			bssUpdateBeaconContentEx(prAdapter,
				prP2pBssInfo->ucBssIndex,
				IE_UPD_METHOD_UNSOL_PROBE_RSP);
		}
#endif

#if CFG_SUPPORT_P2P_GO_OFFLOAD_PROBE_RSP
		if (p2pFuncProbeRespUpdate(prAdapter,
			prP2pBssInfo,
			prP2pBssInfo->prBeacon->prPacket,
			prP2pBssInfo->prBeacon->u2FrameLength,
			IE_UPD_METHOD_UPDATE_PROBE_RSP) ==
				WLAN_STATUS_FAILURE) {
			DBGLOG(P2P, ERROR,
				"Update probe resp IEs fail!\n");
		}
#endif

		/* nicPmIndicateBssCreated(prAdapter,
		 * NETWORK_TYPE_P2P_INDEX);
		 */
	}
error:
	cnmMemFree(prAdapter, prMsgHdr);

}				/* p2pRoleFsmRunEventBeaconUpdate */

void
p2pProcessEvent_UpdateNOAParam(struct ADAPTER *prAdapter,
		uint8_t ucBssIdx,
		struct EVENT_UPDATE_NOA_PARAMS *prEventUpdateNoaParam)
{
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_SPECIFIC_BSS_INFO *prP2pSpecificBssInfo;
	uint32_t i;
	u_int8_t fgNoaAttrExisted = FALSE;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIdx);
	if (!prBssInfo)
		return;
	prP2pSpecificBssInfo =
		prAdapter->rWifiVar
			.prP2pSpecificBssInfo[prBssInfo->u4PrivateData];

	prP2pSpecificBssInfo->fgEnableOppPS =
		prEventUpdateNoaParam->ucEnableOppPS;
	prP2pSpecificBssInfo->u2CTWindow = prEventUpdateNoaParam->u2CTWindow;
	prP2pSpecificBssInfo->ucNoAIndex = prEventUpdateNoaParam->ucNoAIndex;
	prP2pSpecificBssInfo->ucNoATimingCount =
		prEventUpdateNoaParam->ucNoATimingCount;

	fgNoaAttrExisted |= prP2pSpecificBssInfo->fgEnableOppPS;

	ASSERT(prP2pSpecificBssInfo->ucNoATimingCount <= P2P_MAXIMUM_NOA_COUNT);

	for (i = 0; i < prP2pSpecificBssInfo->ucNoATimingCount; i++) {
		/* in used */
		prP2pSpecificBssInfo->arNoATiming[i].fgIsInUse =
			prEventUpdateNoaParam->arEventNoaTiming[i].ucIsInUse;
		/* count */
		prP2pSpecificBssInfo->arNoATiming[i].ucCount =
			prEventUpdateNoaParam->arEventNoaTiming[i].ucCount;
		/* duration */
		prP2pSpecificBssInfo->arNoATiming[i].u4Duration =
			prEventUpdateNoaParam->arEventNoaTiming[i].u4Duration;
		/* interval */
		prP2pSpecificBssInfo->arNoATiming[i].u4Interval =
			prEventUpdateNoaParam->arEventNoaTiming[i].u4Interval;
		/* start time */
		prP2pSpecificBssInfo->arNoATiming[i].u4StartTime =
		    prEventUpdateNoaParam->arEventNoaTiming[i].u4StartTime;

		fgNoaAttrExisted |=
			prP2pSpecificBssInfo->arNoATiming[i].fgIsInUse;
	}

	prP2pSpecificBssInfo->fgIsNoaAttrExisted = fgNoaAttrExisted;

	DBGLOG(P2P, INFO, "Update NoA param, count=%d, ucBssIdx=%d\n",
		prEventUpdateNoaParam->ucNoATimingCount,
		ucBssIdx);

	/* update beacon content by the change */
	bssUpdateBeaconContent(prAdapter, ucBssIdx);
}				/* p2pProcessEvent_UpdateNOAParam */

#if CFG_ENABLE_PER_STA_STATISTICS_LOG
void
p2pRoleFsmGetStaStatistics(struct ADAPTER *prAdapter,
		uintptr_t ulParamPtr)
{
	uint32_t u4BufLen;
	struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
		(struct P2P_ROLE_FSM_INFO *) ulParamPtr;
	struct BSS_INFO *prP2pBssInfo = (struct BSS_INFO *) NULL;

	if ((!prAdapter) || (!prP2pRoleFsmInfo)) {
		DBGLOG(P2P, ERROR, "prAdapter=NULL || prP2pRoleFsmInfo=NULL\n");
		return;
	}

	prP2pBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
		prP2pRoleFsmInfo->ucBssIndex);
	if (!prP2pBssInfo) {
		DBGLOG(P2P, ERROR, "prP2pBssInfo=NULL\n");
		return;
	}

	prQueryStaStatistics =
		prAdapter->rWifiVar.prP2pQueryStaStatistics
		[prP2pRoleFsmInfo->ucRoleIndex];
	if (!prQueryStaStatistics) {
		DBGLOG(P2P, ERROR, "prQueryStaStatistics=NULL\n");
		return;
	}

	if (prP2pBssInfo->eConnectionState == MEDIA_STATE_CONNECTED) {
		if ((prP2pBssInfo->eCurrentOPMode
			!= OP_MODE_INFRASTRUCTURE) &&
			(prP2pBssInfo->eCurrentOPMode
			!= OP_MODE_ACCESS_POINT)) {
			DBGLOG(P2P, ERROR, "Invalid OPMode=%d\n",
				prP2pBssInfo->eCurrentOPMode);
			return;
		}
		if (prP2pBssInfo->eCurrentOPMode == OP_MODE_INFRASTRUCTURE
			&& prP2pBssInfo->prStaRecOfAP) {
			COPY_MAC_ADDR(
				prQueryStaStatistics->aucMacAddr,
				prP2pBssInfo->prStaRecOfAP->aucMacAddr);
		} else if (prP2pBssInfo->eCurrentOPMode
			== OP_MODE_ACCESS_POINT) {
			struct STA_RECORD *prCurrStaRec;
			struct LINK *prClientList =
				&prP2pBssInfo->rStaRecOfClientList;
			if (!prClientList) {
				DBGLOG(P2P, ERROR, "prClientList=NULL\n");
				return;
			}
			LINK_FOR_EACH_ENTRY(prCurrStaRec,
				prClientList, rLinkEntry, struct STA_RECORD) {
				if (!prCurrStaRec)
					break;
				COPY_MAC_ADDR(
					prQueryStaStatistics->aucMacAddr,
					prCurrStaRec->aucMacAddr);
					/* break for LINK_FOR_EACH_ENTRY */
					break;
			}
		}

		prQueryStaStatistics->ucReadClear = TRUE;
		DBGLOG(REQ, TRACE, "Call: prQueryStaStatistics=%p, u4BufLen=%p",
				prQueryStaStatistics, &u4BufLen);
		wlanQueryStaStatistics(prAdapter,
			prQueryStaStatistics,
			sizeof(struct PARAM_GET_STA_STATISTICS),
			&u4BufLen,
			FALSE);
		DBGLOG(REQ, TRACE, "ret prQueryStaStatistics=%p, &u4BufLen=%p",
				prQueryStaStatistics, &u4BufLen);

	}
#if CFG_SUPPORT_WFD
	/* Make sure WFD is still enabled */
	if (prAdapter->rWifiVar.rWfdConfigureSettings.ucWfdEnable &&
		!prAdapter->prGlueInfo->fgIsInSuspendMode) {
		cnmTimerStartTimer(prAdapter,
			&(prP2pRoleFsmInfo->rP2pRoleFsmGetStatisticsTimer),
			P2P_ROLE_GET_STATISTICS_TIME);
	}
#endif
}
#endif

void p2pRoleFsmNotifyEapolTxStatus(struct ADAPTER *prAdapter,
		uint8_t ucBssIndex,
		enum ENUM_EAPOL_KEY_TYPE_T rEapolKeyType,
		enum ENUM_TX_RESULT_CODE rTxDoneStatus)
{
#if 0 /* finish GC join process when dhcp is done */
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
			(struct P2P_ROLE_FSM_INFO *) NULL;

	if (prAdapter == NULL)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (prBssInfo == NULL || prBssInfo->eNetworkType != NETWORK_TYPE_P2P)
		return;

	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prBssInfo->u4PrivateData);

	if (prP2pRoleFsmInfo == NULL)
		return;

	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		return;
	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_GC_JOIN)
		return;

	if (rEapolKeyType == EAPOL_KEY_4_OF_4 &&
			rTxDoneStatus == TX_RESULT_SUCCESS) {
		/* Finish GC connection process. */
		p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
	}
#endif
}

void p2pRoleFsmNotifyDhcpDone(struct ADAPTER *prAdapter,
		uint8_t ucBssIndex)
{
	struct BSS_INFO *prBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
			(struct P2P_ROLE_FSM_INFO *) NULL;

	if (prAdapter == NULL)
		return;

	prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucBssIndex);

	if (prBssInfo == NULL || prBssInfo->eNetworkType != NETWORK_TYPE_P2P)
		return;

	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prBssInfo->u4PrivateData);

	if (prP2pRoleFsmInfo == NULL)
		return;

	if (prBssInfo->eCurrentOPMode != OP_MODE_INFRASTRUCTURE)
		return;
	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_GC_JOIN)
		return;

	/* Finish GC connection process. */
	p2pRoleFsmStateTransition(prAdapter,
			prP2pRoleFsmInfo,
			P2P_ROLE_STATE_IDLE);
}

static u_int8_t
p2pRoleNeedOffchnlTx(struct ADAPTER *prAdapter,
		struct BSS_INFO *prBssInfo,
		struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg)
{
	u_int8_t fgNeedOffchnlTx = FALSE;
	struct WLAN_MAC_HEADER *prWlanHdr = (struct WLAN_MAC_HEADER *) NULL;

	if (prAdapter == NULL || prBssInfo == NULL || prMgmtTxMsg == NULL)
		return FALSE;

	fgNeedOffchnlTx = prMgmtTxMsg->fgIsOffChannel;

	if (fgNeedOffchnlTx) {
		prWlanHdr = (struct WLAN_MAC_HEADER *)
			((uintptr_t) prMgmtTxMsg->prMgmtMsduInfo->prPacket +
					MAC_TX_RESERVED_FIELD);
		if (prMgmtTxMsg->rChannelInfo.ucChannelNum ==
				prBssInfo->ucPrimaryChannel)
			fgNeedOffchnlTx = FALSE;
		else if ((prWlanHdr->u2FrameCtrl & MASK_FRAME_TYPE) ==
				MAC_FRAME_PROBE_RSP)
			fgNeedOffchnlTx = FALSE;
	}

	return fgNeedOffchnlTx;
}				/* p2pRoleNeedOffchnlTx */


static void
p2pRoleAdjustChnlTime(struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg,
	struct P2P_OFF_CHNL_TX_REQ_INFO *prOffChnlTxReq)
{
	if (prMgmtTxMsg == NULL || prOffChnlTxReq == NULL)
		return;

	if (!prMgmtTxMsg->fgIsOffChannel)
		return;

	if (prMgmtTxMsg->u4Duration < MIN_TX_DURATION_TIME_MS) {
		/*
		 * The wait time requested from Supplicant is too short
		 * to TX a frame, eg. nego.conf.. Overwrite the wait time
		 * as driver's min TX time.
		 */
		DBGLOG(P2P, INFO, "Overwrite channel duration from %d to %d\n",
				prMgmtTxMsg->u4Duration,
				MIN_TX_DURATION_TIME_MS);
		prOffChnlTxReq->u4Duration = MIN_TX_DURATION_TIME_MS;
	} else {
		prOffChnlTxReq->u4Duration = prMgmtTxMsg->u4Duration;
	}
}				/* p2pRoleAdjustChnlTime */

static void
p2pRoleChnlReqByOffChnl(struct ADAPTER *prAdapter,
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
	struct P2P_OFF_CHNL_TX_REQ_INFO *prOffChnlTxReq)
{
	struct P2P_CHNL_REQ_INFO *prP2pChnlReqInfo =
			(struct P2P_CHNL_REQ_INFO *) NULL;

	if (prAdapter == NULL || prP2pRoleFsmInfo == NULL ||
			prOffChnlTxReq == NULL)
		return;

	prP2pChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);

	prP2pChnlReqInfo->u8Cookie = prOffChnlTxReq->u8Cookie;
	prP2pChnlReqInfo->eChnlReqType = CH_REQ_TYPE_OFFCHNL_TX;
	prP2pChnlReqInfo->eBand = prOffChnlTxReq->rChannelInfo.eBand;
	prP2pChnlReqInfo->ucReqChnlNum =
			prOffChnlTxReq->rChannelInfo.ucChannelNum;
	prP2pChnlReqInfo->eChnlSco = prOffChnlTxReq->eChnlExt;
	prP2pChnlReqInfo->u4MaxInterval = prOffChnlTxReq->u4Duration;
	prP2pChnlReqInfo->eChannelWidth = prOffChnlTxReq->rChannelInfo.ucChnlBw;
	prP2pChnlReqInfo->ucCenterFreqS1 =
			prOffChnlTxReq->rChannelInfo.u4CenterFreq1;
	prP2pChnlReqInfo->ucCenterFreqS2 =
			prOffChnlTxReq->rChannelInfo.u4CenterFreq2;

	p2pRoleFsmStateTransition(prAdapter, prP2pRoleFsmInfo,
			P2P_ROLE_STATE_REQING_CHANNEL);
}				/* p2pRoleChnlReqByOffChnl */

static u_int8_t
p2pRoleAddTxReq2Queue(struct ADAPTER *prAdapter,
		struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxReqInfo,
		struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg,
		struct P2P_OFF_CHNL_TX_REQ_INFO **pprOffChnlTxReq)
{
	struct P2P_OFF_CHNL_TX_REQ_INFO *prTmpOffChnlTxReq =
			(struct P2P_OFF_CHNL_TX_REQ_INFO *) NULL;

	prTmpOffChnlTxReq = cnmMemAlloc(prAdapter,
			RAM_TYPE_MSG,
			sizeof(struct P2P_OFF_CHNL_TX_REQ_INFO));

	if (prTmpOffChnlTxReq == NULL) {
		DBGLOG(P2P, ERROR,
				"Allocate TX request buffer fails.\n");
		return FALSE;
	}

	prTmpOffChnlTxReq->ucBssIndex = prMgmtTxMsg->ucBssIdx;
	prTmpOffChnlTxReq->u8Cookie = prMgmtTxMsg->u8Cookie;
	prTmpOffChnlTxReq->prMgmtTxMsdu = prMgmtTxMsg->prMgmtMsduInfo;
	prTmpOffChnlTxReq->fgNoneCckRate = prMgmtTxMsg->fgNoneCckRate;
	kalMemCopy(&prTmpOffChnlTxReq->rChannelInfo,
			&prMgmtTxMsg->rChannelInfo,
			sizeof(struct RF_CHANNEL_INFO));
	prTmpOffChnlTxReq->eChnlExt = prMgmtTxMsg->eChnlExt;
	prTmpOffChnlTxReq->fgIsWaitRsp = prMgmtTxMsg->fgIsWaitRsp;

	p2pRoleAdjustChnlTime(prMgmtTxMsg, prTmpOffChnlTxReq);

	LINK_INSERT_TAIL(&prP2pMgmtTxReqInfo->rTxReqLink,
			&prTmpOffChnlTxReq->rLinkEntry);

	*pprOffChnlTxReq = prTmpOffChnlTxReq;

	return TRUE;
}

static void
p2pRoleAbortChlReqIfNeed(struct ADAPTER *prAdapter,
		struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo,
		struct P2P_CHNL_REQ_INFO *prChnlReqInfo)
{
	if (prAdapter == NULL || prP2pRoleFsmInfo == NULL ||
			prChnlReqInfo == NULL)
		return;

	/* Cancel ongoing channel request whose type is not offchannel-tx*/
	if (prP2pRoleFsmInfo->eCurrentState != P2P_ROLE_STATE_REQING_CHANNEL ||
			prChnlReqInfo->eChnlReqType == CH_REQ_TYPE_OFFCHNL_TX)
		return;

	p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
}

static void
p2pRoleHandleOffchnlTxReq(struct ADAPTER *prAdapter,
		struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg)
{
	struct BSS_INFO *prP2pRoleBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
			(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxReqInfo =
			(struct P2P_MGMT_TX_REQ_INFO *) NULL;
	struct P2P_OFF_CHNL_TX_REQ_INFO *prOffChnlTxReq =
			(struct P2P_OFF_CHNL_TX_REQ_INFO *) NULL;
	struct P2P_CHNL_REQ_INFO *prChnlReqInfo =
			(struct P2P_CHNL_REQ_INFO *) NULL;

	if (prAdapter == NULL || prMgmtTxMsg == NULL)
		return;

	prP2pRoleBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
			prMgmtTxMsg->ucBssIdx);
	prP2pRoleFsmInfo = prP2pRoleBssInfo != NULL ?
			P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pRoleBssInfo->u4PrivateData) : NULL;

	if (prP2pRoleFsmInfo == NULL)
		return;

	prP2pMgmtTxReqInfo = &(prP2pRoleFsmInfo->rMgmtTxInfo);
	prChnlReqInfo = &(prP2pRoleFsmInfo->rChnlReqInfo);

	if (prP2pMgmtTxReqInfo == NULL || prChnlReqInfo == NULL)

	p2pRoleAbortChlReqIfNeed(prAdapter, prP2pRoleFsmInfo, prChnlReqInfo);

	if (p2pRoleAddTxReq2Queue(prAdapter, prP2pMgmtTxReqInfo,
			prMgmtTxMsg, &prOffChnlTxReq) == FALSE)
		goto error;

	switch (prP2pRoleFsmInfo->eCurrentState) {
	case P2P_ROLE_STATE_IDLE:
		p2pRoleChnlReqByOffChnl(prAdapter, prP2pRoleFsmInfo,
				prOffChnlTxReq);
		break;
	case P2P_ROLE_STATE_REQING_CHANNEL:
		if (prChnlReqInfo->eChnlReqType != CH_REQ_TYPE_OFFCHNL_TX) {
			DBGLOG(P2P, WARN,
				"channel already requested by others\n");
			goto error;
		}
		break;
	case P2P_ROLE_STATE_OFF_CHNL_TX:
		if (p2pFuncCheckOnRocChnl(&(prMgmtTxMsg->rChannelInfo),
				prChnlReqInfo) &&
				prP2pMgmtTxReqInfo->rTxReqLink.u4NumElem == 1) {
			p2pRoleFsmStateTransition(prAdapter,
					prP2pRoleFsmInfo,
					P2P_ROLE_STATE_OFF_CHNL_TX);
		} else {
			log_dbg(P2P, INFO, "tx ch: %d, current ch: %d, requested: %d, tx link num: %d",
				prMgmtTxMsg->rChannelInfo.ucChannelNum,
				prChnlReqInfo->ucReqChnlNum,
				prChnlReqInfo->fgIsChannelRequested,
				prP2pMgmtTxReqInfo->rTxReqLink.u4NumElem);
		}
		break;
	default:
		DBGLOG(P2P, WARN, "Unknown state (%s) for offchannel-tx.\n",
				p2pRoleFsmGetFsmState(
				prP2pRoleFsmInfo->eCurrentState));
		goto error;
	}

	return;

error:
	LINK_REMOVE_KNOWN_ENTRY(
			&(prP2pMgmtTxReqInfo->rTxReqLink),
			&prOffChnlTxReq->rLinkEntry);
	cnmMgtPktFree(prAdapter, prOffChnlTxReq->prMgmtTxMsdu);
	cnmMemFree(prAdapter, prOffChnlTxReq);
}				/* p2pRoleHandleOffchnlTxReq */

void p2pRoleFsmRunEventMgmtTx(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *prP2pRoleBssInfo = (struct BSS_INFO *) NULL;
	struct MSG_MGMT_TX_REQUEST *prMgmtTxMsg =
		(struct MSG_MGMT_TX_REQUEST *) NULL;
	u_int8_t fgNeedOffchnlTx;

	if (prAdapter == NULL || prMsgHdr == NULL)
		goto error;

	prMgmtTxMsg = (struct MSG_MGMT_TX_REQUEST *) prMsgHdr;
	prP2pRoleBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
			prMgmtTxMsg->ucBssIdx);

	fgNeedOffchnlTx = p2pRoleNeedOffchnlTx(prAdapter, prP2pRoleBssInfo,
			prMgmtTxMsg);
	DBGLOG(P2P, INFO, "fgNeedOffchnlTx: %d\n", fgNeedOffchnlTx);

	if (!fgNeedOffchnlTx)
		p2pFuncTxMgmtFrame(prAdapter,
				prMgmtTxMsg->ucBssIdx,
				prMgmtTxMsg->prMgmtMsduInfo,
				prMgmtTxMsg->fgNoneCckRate);
	else
		p2pRoleHandleOffchnlTxReq(prAdapter, prMgmtTxMsg);

error:
	cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventMgmtTx */

void p2pRoleFsmRunEventTxCancelWait(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct BSS_INFO *prP2pRoleBssInfo = (struct BSS_INFO *) NULL;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo =
			(struct P2P_ROLE_FSM_INFO *) NULL;
	struct P2P_MGMT_TX_REQ_INFO *prP2pMgmtTxInfo =
			(struct P2P_MGMT_TX_REQ_INFO *) NULL;
	struct MSG_CANCEL_TX_WAIT_REQUEST *prCancelTxWaitMsg =
			(struct MSG_CANCEL_TX_WAIT_REQUEST *) NULL;
	struct P2P_OFF_CHNL_TX_REQ_INFO *prOffChnlTxPkt =
			(struct P2P_OFF_CHNL_TX_REQ_INFO *) NULL;
	u_int8_t fgIsCookieFound = FALSE;

	if (prAdapter == NULL || prMsgHdr == NULL)
		goto exit;

	prCancelTxWaitMsg = (struct MSG_CANCEL_TX_WAIT_REQUEST *) prMsgHdr;
	prP2pRoleBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter,
			prCancelTxWaitMsg->ucBssIdx);
	if (!prP2pRoleBssInfo)
		goto exit;
	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pRoleBssInfo->u4PrivateData);
	prP2pMgmtTxInfo = prP2pRoleFsmInfo != NULL ?
			&(prP2pRoleFsmInfo->rMgmtTxInfo) : NULL;

	if (prCancelTxWaitMsg == NULL || prP2pRoleFsmInfo == NULL ||
			prP2pMgmtTxInfo == NULL || prP2pMgmtTxInfo == NULL)
		goto exit;

	LINK_FOR_EACH_ENTRY(prOffChnlTxPkt,
			&(prP2pMgmtTxInfo->rTxReqLink),
			rLinkEntry,
			struct P2P_OFF_CHNL_TX_REQ_INFO) {
		if (!prOffChnlTxPkt)
			break;
		if (prOffChnlTxPkt->u8Cookie == prCancelTxWaitMsg->u8Cookie) {
			fgIsCookieFound = TRUE;
			break;
		}
	}

	if (fgIsCookieFound || prP2pRoleFsmInfo->eCurrentState ==
			P2P_ROLE_STATE_OFF_CHNL_TX) {
		p2pFunClearAllTxReq(prAdapter,
				&(prP2pRoleFsmInfo->rMgmtTxInfo));
		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
	}

exit:
	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}				/* p2pRoleFsmRunEventTxCancelWait */

static void initAcsParams(struct ADAPTER *prAdapter,
		struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest,
		struct P2P_ACS_REQ_INFO *prAcsReqInfo)
{
	struct RF_CHANNEL_INFO *prRfChannelInfo;
	uint8_t i;

	if (!prAdapter || !prMsgAcsRequest || !prAcsReqInfo)
		return;

	kalMemSet(prAcsReqInfo, 0, sizeof(struct P2P_ACS_REQ_INFO));
	prAcsReqInfo->fgIsProcessing = TRUE;
	prAcsReqInfo->ucRoleIdx = prMsgAcsRequest->ucRoleIdx;
	prAcsReqInfo->fgIsHtEnable = prMsgAcsRequest->fgIsHtEnable;
	prAcsReqInfo->fgIsHt40Enable = prMsgAcsRequest->fgIsHt40Enable;
	prAcsReqInfo->fgIsVhtEnable = prMsgAcsRequest->fgIsVhtEnable;
	prAcsReqInfo->eChnlBw = prMsgAcsRequest->eChnlBw;
	prAcsReqInfo->eHwMode = prMsgAcsRequest->eHwMode;

	if (prAcsReqInfo->eChnlBw == MAX_BW_UNKNOWN) {
		if (prAcsReqInfo->fgIsHtEnable &&
				prAcsReqInfo->fgIsHt40Enable) {
			prAcsReqInfo->eChnlBw = MAX_BW_40MHZ;
		} else {
			prAcsReqInfo->eChnlBw = MAX_BW_20MHZ;
		}
	}
	if (!prAcsReqInfo->fgIsVhtEnable &&
			(prAcsReqInfo->eChnlBw >= MAX_BW_80MHZ)) {
		if (prAcsReqInfo->fgIsHtEnable &&
				prAcsReqInfo->fgIsHt40Enable) {
			prAcsReqInfo->eChnlBw = MAX_BW_40MHZ;
		} else {
			prAcsReqInfo->eChnlBw = MAX_BW_20MHZ;
		}
	}

	DBGLOG(P2P, INFO, "idx=%d, ht=%d, ht40=%d, vht=%d, bw=%d, m=%d, c=%d",
			prMsgAcsRequest->ucRoleIdx,
			prMsgAcsRequest->fgIsHtEnable,
			prMsgAcsRequest->fgIsHt40Enable,
			prMsgAcsRequest->fgIsVhtEnable,
			prMsgAcsRequest->eChnlBw,
			prMsgAcsRequest->eHwMode,
			prMsgAcsRequest->u4NumChannel);
	if (prMsgAcsRequest->u4NumChannel) {
		for (i = 0; i < prMsgAcsRequest->u4NumChannel; i++) {
			prRfChannelInfo =
				&(prMsgAcsRequest->arChannelListInfo[i]);
			DBGLOG(REQ, TRACE, "[%d] band=%d, ch=%d\n", i,
				prRfChannelInfo->eBand,
				prRfChannelInfo->ucChannelNum);
			prAcsReqInfo->ucBand |= BIT(prRfChannelInfo->eBand);
			prRfChannelInfo++;
		}
	}
}

static void indicateAcsResultByAisCh(struct ADAPTER *prAdapter,
		struct P2P_ACS_REQ_INFO *prAcsReqInfo,
		struct BSS_INFO *prAisBssInfo)
{
	if (!prAdapter || !prAcsReqInfo)
		return;

	prAcsReqInfo->ucPrimaryCh = prAisBssInfo->ucPrimaryChannel;
	if (prAisBssInfo->ucVhtChannelWidth == VHT_OP_CHANNEL_WIDTH_20_40) {
		if (prAisBssInfo->eBssSCO == CHNL_EXT_SCN)
			prAcsReqInfo->eChnlBw = MAX_BW_20MHZ;
		else
			prAcsReqInfo->eChnlBw = MAX_BW_40MHZ;
	} else if (prAisBssInfo->ucVhtChannelWidth >= VHT_OP_CHANNEL_WIDTH_80) {
		prAcsReqInfo->eChnlBw = MAX_BW_80MHZ;
	} else {
		prAcsReqInfo->eChnlBw = MAX_BW_20MHZ;
	}

	prAcsReqInfo->eBand = prAisBssInfo->eBand;
	if (prAcsReqInfo->eBand == BAND_2G4)
		prAcsReqInfo->eHwMode = P2P_VENDOR_ACS_HW_MODE_11G;
	else
		prAcsReqInfo->eHwMode = P2P_VENDOR_ACS_HW_MODE_11A;

	prAcsReqInfo->fgIsAis = TRUE;

	if ((prAdapter->rWifiVar.fgSapChannelSwitchPolicy >=
		P2P_CHANNEL_SWITCH_POLICY_SKIP_DFS) &&
		(prAcsReqInfo->eBand == BAND_5G) &&
		(rlmDomainIsLegalDfsChannel(
		prAdapter,
		prAcsReqInfo->eBand,
		prAcsReqInfo->ucPrimaryCh) ||
		(prAcsReqInfo->eChnlBw >= MAX_BW_160MHZ))) {
		DBGLOG(P2P, INFO,
			"[SKIP] StaCH(%d), Band(%d)\n",
			prAcsReqInfo->ucPrimaryCh,
			prAcsReqInfo->eBand);
		prAcsReqInfo->ucPrimaryCh = AP_DEFAULT_CHANNEL_5G;
	} else if (rlmDomainIsLegalDfsChannel(prAdapter,
		prAcsReqInfo->eBand,
		prAcsReqInfo->ucPrimaryCh)) {
		prAcsReqInfo->eChnlBw = MAX_BW_20MHZ;
		DBGLOG(P2P, INFO, "report 20mhz bw due to dfs\n",
			prAcsReqInfo->eBand,
			prAcsReqInfo->ucPrimaryCh);
	}

	p2pFunIndicateAcsResult(prAdapter->prGlueInfo,
			prAcsReqInfo);
}

static void trimAcsScanList(struct ADAPTER *prAdapter,
		struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest,
		struct P2P_ACS_REQ_INFO *prAcsReqInfo,
		uint8_t ucDesiredBand)
{
	uint32_t u4NumChannel = 0;
	uint8_t i;
	struct RF_CHANNEL_INFO *prRfChannelInfo1;
	struct RF_CHANNEL_INFO *prRfChannelInfo2;

	if (!prAdapter || !prAcsReqInfo)
		return;

	for (i = 0; i < prMsgAcsRequest->u4NumChannel; i++) {
		prRfChannelInfo1 =
				&(prMsgAcsRequest->arChannelListInfo[i]);
		if ((ucDesiredBand & BIT(prRfChannelInfo1->eBand)) == 0)
			continue;
#if (CFG_SUPPORT_WIFI_6G == 1)
		/* Keep only PSC channels */
		if (prRfChannelInfo1->eBand == BAND_6G &&
			!(prRfChannelInfo1->ucChannelNum >= 5 &&
			  prRfChannelInfo1->ucChannelNum <= 225 &&
			  ((prRfChannelInfo1->ucChannelNum - 5) % 16 == 0)))
			continue;
#endif
#if (CFG_SUPPORT_AVOID_DESENSE == 1)
		if (IS_CHANNEL_IN_DESENSE_RANGE(prAdapter,
			prRfChannelInfo1->ucChannelNum,
			prRfChannelInfo1->eBand))
			continue;
#endif
		DBGLOG(P2P, INFO, "acs trim scan list, [%d]=%d %d\n",
				u4NumChannel,
				prRfChannelInfo1->eBand,
				prRfChannelInfo1->ucChannelNum);
		prRfChannelInfo2 = &(prMsgAcsRequest->arChannelListInfo[
				u4NumChannel]);
		prRfChannelInfo2->eBand = prRfChannelInfo1->eBand;
		prRfChannelInfo2->u4CenterFreq1 =
				prRfChannelInfo1->u4CenterFreq1;
		prRfChannelInfo2->u4CenterFreq2 =
				prRfChannelInfo1->u4CenterFreq2;
		prRfChannelInfo2->u2PriChnlFreq =
				prRfChannelInfo1->u2PriChnlFreq;
		prRfChannelInfo2->ucChnlBw = prRfChannelInfo1->ucChnlBw;
		prRfChannelInfo2->ucChannelNum =
				prRfChannelInfo1->ucChannelNum;
		u4NumChannel++;
		prRfChannelInfo1++;
	}
	prMsgAcsRequest->u4NumChannel = u4NumChannel;
}

static void initAcsChnlMask(struct ADAPTER *prAdapter,
		struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest,
		struct P2P_ACS_REQ_INFO *prAcsReqInfo)
{
	uint8_t i;
	struct RF_CHANNEL_INFO *prRfChannelInfo;

	prAcsReqInfo->u4LteSafeChnMask_2G = 0;
	prAcsReqInfo->u4LteSafeChnMask_5G_1 = 0;
	prAcsReqInfo->u4LteSafeChnMask_5G_2 = 0;
	prAcsReqInfo->u4LteSafeChnMask_6G = 0;

	for (i = 0; i < prMsgAcsRequest->u4NumChannel; i++) {
		prRfChannelInfo = &(prMsgAcsRequest->arChannelListInfo[i]);
		if (prRfChannelInfo->eBand == BAND_2G4) {
			prAcsReqInfo->u4LteSafeChnMask_2G |= BIT(
				prRfChannelInfo->ucChannelNum);
		} else if (prRfChannelInfo->eBand == BAND_5G &&
				prRfChannelInfo->ucChannelNum >= 36 &&
				prRfChannelInfo->ucChannelNum <= 144) {
			prAcsReqInfo->u4LteSafeChnMask_5G_1 |= BIT(
				(prRfChannelInfo->ucChannelNum - 36) / 4);
		} else if (prRfChannelInfo->eBand == BAND_5G &&
				prRfChannelInfo->ucChannelNum >= 149 &&
				prRfChannelInfo->ucChannelNum <= 181) {
			prAcsReqInfo->u4LteSafeChnMask_5G_2 |= BIT(
				(prRfChannelInfo->ucChannelNum - 149) / 4);
		}
#if (CFG_SUPPORT_WIFI_6G == 1)
		else if (prRfChannelInfo->eBand == BAND_6G &&
				prRfChannelInfo->ucChannelNum >= 7 &&
				prRfChannelInfo->ucChannelNum <= 215) {
			prAcsReqInfo->u4LteSafeChnMask_6G |= BIT(
				(prRfChannelInfo->ucChannelNum - 7) / 16);
		}
#endif
	}

	DBGLOG(P2P, INFO, "acs chnl mask=[0x%08x][0x%08x][0x%08x][0x%08x]\n",
			prAcsReqInfo->u4LteSafeChnMask_2G,
			prAcsReqInfo->u4LteSafeChnMask_5G_1,
			prAcsReqInfo->u4LteSafeChnMask_5G_2,
			prAcsReqInfo->u4LteSafeChnMask_6G);
}

u_int8_t indicateApAcsOverwrite(
	struct ADAPTER *prAdapter,
	struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest,
	struct P2P_ACS_REQ_INFO *prAcsReqInfo)
{
	struct RF_CHANNEL_INFO *prRfChannelInfo;
	enum ENUM_BAND eBand = BAND_NULL;
	enum ENUM_MAX_BANDWIDTH_SETTING eChnlBw;
	u_int8_t bOverwrite = FALSE;
	uint8_t ucPrimaryCh = 0;
	int32_t i;

	if (!prAdapter || !prAcsReqInfo || !prMsgAcsRequest)
		return FALSE;

	for (i = prMsgAcsRequest->u4NumChannel - 1; i >= 0; i--) {
		prRfChannelInfo =
			&(prMsgAcsRequest->arChannelListInfo[i]);
		if (prRfChannelInfo->eBand != BAND_NULL) {
			eBand = prRfChannelInfo->eBand;
			break;
		}
	}

	if (eBand == BAND_NULL)
		return FALSE;

	if (eBand == BAND_2G4) {
		if (prAdapter->rWifiVar.ucApAcsChannel[0]) {
			ucPrimaryCh = prAdapter->rWifiVar.ucApAcsChannel[0];
			eChnlBw = prAdapter->rWifiVar.ucAp2gBandwidth;
		} else if (p2pFuncIsDualAPMode(prAdapter)) {
			struct BSS_INFO *bss =
				aisGetConnectedBssInfo(prAdapter);

			if (!bss)
				ucPrimaryCh = AP_DEFAULT_CHANNEL_2G;
			else if (bss->eBand == BAND_2G4)
				ucPrimaryCh = bss->ucPrimaryChannel;
			eChnlBw = prAdapter->rWifiVar.ucAp2gBandwidth;
		}
	} else if ((eBand == BAND_5G) &&
		prAdapter->rWifiVar.ucApAcsChannel[1]) {
		ucPrimaryCh = prAdapter->rWifiVar.ucApAcsChannel[1];
		eChnlBw = prAdapter->rWifiVar.ucAp5gBandwidth;
	}
#if (CFG_SUPPORT_WIFI_6G == 1)
	else if ((eBand == BAND_6G) &&
		prAdapter->rWifiVar.ucApAcsChannel[2]) {
		ucPrimaryCh = prAdapter->rWifiVar.ucApAcsChannel[2];
		eChnlBw = prAdapter->rWifiVar.ucAp6gBandwidth;
	}
#endif

	if (ucPrimaryCh) {
		prAcsReqInfo->ucPrimaryCh = ucPrimaryCh;
		prAcsReqInfo->eBand = eBand;
		if (eBand == BAND_2G4)
			prAcsReqInfo->eHwMode = P2P_VENDOR_ACS_HW_MODE_11G;
		else
			prAcsReqInfo->eHwMode = P2P_VENDOR_ACS_HW_MODE_11A;
		if (eChnlBw
			> prAdapter->rWifiVar.ucApBandwidth)
			eChnlBw = prAdapter->rWifiVar.ucApBandwidth;
		/* prAcsReqInfo->eChnlBw = eChnlBw;*/
		p2pFunIndicateAcsResult(prAdapter->prGlueInfo,
			prAcsReqInfo);
		bOverwrite = TRUE;
	}

	return bOverwrite;
}

void p2pRoleFsmRunEventAcs(struct ADAPTER *prAdapter,
		struct MSG_HDR *prMsgHdr)
{
	struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest;
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo;
	struct MSG_P2P_SCAN_REQUEST *prP2pScanReqMsg;
	struct P2P_ACS_REQ_INFO *prAcsReqInfo;
	uint32_t u4MsgSize = 0;

	if (!prAdapter || !prMsgHdr)
		return;

	prMsgAcsRequest = (struct MSG_P2P_ACS_REQUEST *) prMsgHdr;
	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prMsgAcsRequest->ucRoleIdx);
	prP2pRoleFsmInfo->fgIsChannelSelectByAcs = TRUE;
	prAcsReqInfo = &prP2pRoleFsmInfo->rAcsReqInfo;

	p2pRoleFsmAbortCurrentAcsReq(prAdapter, prMsgAcsRequest);

	initAcsParams(prAdapter, prMsgAcsRequest, prAcsReqInfo);

	if (indicateApAcsOverwrite(prAdapter,
		prMsgAcsRequest, prAcsReqInfo)) {
		goto exit;
	}

	if (prAdapter->rWifiVar.eDbdcMode == ENUM_DBDC_MODE_DISABLED) {
		struct BSS_INFO *prAisBssInfo;

		DBGLOG(P2P, INFO, "Report SCC channel\n");

		prAisBssInfo = aisGetConnectedBssInfo(prAdapter);
		if (prAisBssInfo &&
			prAisBssInfo->eBand == BAND_2G4 &&
			prAcsReqInfo->eHwMode == P2P_VENDOR_ACS_HW_MODE_11G) {
			/* Force SCC, indicate channel directly */
			indicateAcsResultByAisCh(prAdapter, prAcsReqInfo,
			prAisBssInfo);
			goto exit;
		} else if (prAisBssInfo &&
			prAisBssInfo->eBand == BAND_5G &&
			prAcsReqInfo->eHwMode == P2P_VENDOR_ACS_HW_MODE_11A) {
			/* Force SCC, indicate channel directly */
			indicateAcsResultByAisCh(prAdapter, prAcsReqInfo,
			prAisBssInfo);
			goto exit;
#if CFG_HOTSPOT_SUPPORT_FORCE_ACS_SCC
		} else if (prAisBssInfo &&
			prAcsReqInfo->eHwMode == P2P_VENDOR_ACS_HW_MODE_11ANY) {
			/* Force SCC, indicate channel directly */
			indicateAcsResultByAisCh(prAdapter, prAcsReqInfo,
			prAisBssInfo);
			goto exit;
#endif
		}
	}


	if (prAcsReqInfo->eHwMode == P2P_VENDOR_ACS_HW_MODE_11ANY) {
		struct BSS_INFO *prAisBssInfo;
		prAisBssInfo = aisGetDefaultLinkBssInfo(prAdapter);
		if (prAisBssInfo->eConnectionState == MEDIA_STATE_CONNECTED &&
			(!p2pFuncIsDualAPMode(prAdapter) ||
			(p2pFuncIsDualAPMode(prAdapter) &&
			prAisBssInfo->eBand > BAND_2G4))) {
			/* Force SCC, indicate channel directly */
			indicateAcsResultByAisCh(prAdapter, prAcsReqInfo,
				prAisBssInfo);
			goto exit;
#if (CFG_SUPPORT_WIFI_6G == 1)
		} else if (prAdapter->fgIsHwSupport6G) {
			/* Trim 5G + 6G PSC channels */
			trimAcsScanList(prAdapter, prMsgAcsRequest,
				prAcsReqInfo, BIT(BAND_6G) | BIT(BAND_5G));
			prAcsReqInfo->eHwMode = P2P_VENDOR_ACS_HW_MODE_11A;
#endif
		} else if (prAdapter->fgEnable5GBand) {
			/* Trim 5G channels */
			trimAcsScanList(prAdapter, prMsgAcsRequest,
				prAcsReqInfo, BIT(BAND_5G));
			prAcsReqInfo->eHwMode = P2P_VENDOR_ACS_HW_MODE_11A;
		} else {
			trimAcsScanList(prAdapter, prMsgAcsRequest,
				prAcsReqInfo, BIT(BAND_2G4));
			prAcsReqInfo->eHwMode = P2P_VENDOR_ACS_HW_MODE_11G;
		}
	} else if (prAcsReqInfo->eHwMode == P2P_VENDOR_ACS_HW_MODE_11A) {
#if (CFG_SUPPORT_WIFI_6G == 1)
		if (prAdapter->fgIsHwSupport6G) {
			/* Trim 5G + 6G PSC channels */
			trimAcsScanList(prAdapter, prMsgAcsRequest,
				prAcsReqInfo, BIT(BAND_6G) | BIT(BAND_5G));
		} else
#endif
		if (prAdapter->fgEnable5GBand) {
			/* Trim 5G channels */
			trimAcsScanList(prAdapter, prMsgAcsRequest,
				prAcsReqInfo, BIT(BAND_5G));
		}
	}

	initAcsChnlMask(prAdapter, prMsgAcsRequest, prAcsReqInfo);

	u4MsgSize = sizeof(struct MSG_P2P_SCAN_REQUEST) + (
			prMsgAcsRequest->u4NumChannel *
				sizeof(struct RF_CHANNEL_INFO));

	prP2pScanReqMsg = cnmMemAlloc(prAdapter, RAM_TYPE_MSG, u4MsgSize);
	if (prP2pScanReqMsg == NULL) {
		DBGLOG(P2P, ERROR, "alloc scan req. fail\n");
		return;
	}
	kalMemSet(prP2pScanReqMsg, 0, u4MsgSize);
	prP2pScanReqMsg->eScanType = SCAN_TYPE_ACTIVE_SCAN;
	prP2pScanReqMsg->ucBssIdx = prP2pRoleFsmInfo->ucBssIndex;
	prP2pScanReqMsg->i4SsidNum = 0;
	prP2pScanReqMsg->u4NumChannel = prMsgAcsRequest->u4NumChannel;
	prP2pScanReqMsg->u4IELen = 0;
	prP2pScanReqMsg->eScanReason = SCAN_REASON_ACS;
	kalMemCopy(&(prP2pScanReqMsg->arChannelListInfo),
			&(prMsgAcsRequest->arChannelListInfo),
			(prMsgAcsRequest->u4NumChannel *
				sizeof(struct RF_CHANNEL_INFO)));
	p2pRoleFsmRunEventScanRequest(prAdapter,
			(struct MSG_HDR *) prP2pScanReqMsg);

exit:
	if (prMsgHdr)
		cnmMemFree(prAdapter, prMsgHdr);
}

static u_int8_t
p2pRoleFsmIsAcsProcessing(struct ADAPTER *prAdapter,
		uint8_t ucRoleIdx)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo;
	struct P2P_ACS_REQ_INFO *prAcsReqInfo;

	if (!prAdapter)
		return FALSE;

	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			ucRoleIdx);
	if (!prP2pRoleFsmInfo)
		return FALSE;

	prAcsReqInfo = &prP2pRoleFsmInfo->rAcsReqInfo;
	if (!prAcsReqInfo)
		return FALSE;

	return prAcsReqInfo->fgIsProcessing;
}

static void
p2pRoleFsmAbortCurrentAcsReq(struct ADAPTER *prAdapter,
		struct MSG_P2P_ACS_REQUEST *prMsgAcsRequest)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo = NULL;
	struct P2P_SCAN_REQ_INFO *prScanReqInfo = NULL;

	if (!prAdapter || !prMsgAcsRequest)
		return;

	prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prMsgAcsRequest->ucRoleIdx);
	prScanReqInfo = &(prP2pRoleFsmInfo->rScanReqInfo);

	if (!p2pRoleFsmIsAcsProcessing(prAdapter, prMsgAcsRequest->ucRoleIdx))
		return;

	if (prP2pRoleFsmInfo->eCurrentState == P2P_ROLE_STATE_SCAN &&
			prScanReqInfo->eScanReason == SCAN_REASON_ACS) {
		DBGLOG(P2P, INFO, "Cancel current ACS scan.\n");
		p2pRoleFsmRunEventAbort(prAdapter, prP2pRoleFsmInfo);
	}

	if (prScanReqInfo && prScanReqInfo->fgIsScanRequest) {
		DBGLOG(P2P, ERROR, "Clear old scan req.\n");
		prScanReqInfo->fgIsScanRequest = FALSE;
	}
}

void p2pRoleFsmRunEventScanAbort(struct ADAPTER *prAdapter,
		uint8_t ucBssIdx)
{
	struct P2P_ROLE_FSM_INFO *prP2pRoleFsmInfo = NULL;
	struct BSS_INFO *prP2pBssInfo = NULL;

	do {
		ASSERT_BREAK(prAdapter != NULL);

		DBGLOG(P2P, TRACE, "p2pRoleFsmRunEventScanAbort\n");

		prP2pBssInfo = prAdapter->aprBssInfo[ucBssIdx];
		prP2pRoleFsmInfo = P2P_ROLE_INDEX_2_ROLE_FSM_INFO(prAdapter,
			prP2pBssInfo->u4PrivateData);

		if (prP2pRoleFsmInfo->eCurrentState == P2P_ROLE_STATE_SCAN) {
			struct P2P_SCAN_REQ_INFO *prScanReqInfo =
				&(prP2pRoleFsmInfo->rScanReqInfo);

			prScanReqInfo->fgIsAbort = TRUE;

			p2pRoleFsmStateTransition(prAdapter,
				prP2pRoleFsmInfo,
				P2P_ROLE_STATE_IDLE);
		}
	} while (FALSE);
}

#if (CFG_WOW_SUPPORT == 1)
/*----------------------------------------------------------------------------*/
/*!
* @brief This function will clean p2p connection before suspend.
*
* @param[in] prAdapter          Pointer to the Adapter structure.
*
* @return (none)
*/
/*----------------------------------------------------------------------------*/
void p2pRoleProcessPreSuspendFlow(struct ADAPTER *prAdapter)
{
	uint8_t ucIdx;
	struct BSS_INFO *prBssInfo;
	uint32_t u4ClientCount = 0;
	struct LINK *prClientList;
	struct STA_RECORD *prCurrStaRec, *prStaRecNext;
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
		(struct P2P_DEV_FSM_INFO *) NULL;
	enum ENUM_OP_MODE eOPMode;

	if (prAdapter == NULL)
		return;

	/* This should be cover by USB TX/RX check condition */
	/*
	* if (!wlanGetHifState(prAdapter->prGlueInfo))
	* return;
	*/

	for (ucIdx = 0; ucIdx < MAX_BSS_INDEX; ucIdx++) {
		prBssInfo = GET_BSS_INFO_BY_INDEX(prAdapter, ucIdx);
		if (!prBssInfo)
			continue;

		/* Skip AIS BSS */
		if (IS_BSS_AIS(prBssInfo))
			continue;

		if (!IS_BSS_ACTIVE(prBssInfo))
			continue;

		/* Non-P2P network type */
		if (!IS_BSS_P2P(prBssInfo)) {
			DBGLOG(P2P, STATE, "[Suspend] eNetworkType %d.\n",
				prBssInfo->eNetworkType);
			nicPmIndicateBssAbort(prAdapter, ucIdx);
			nicDeactivateNetwork(prAdapter, ucIdx);
			nicUpdateBss(prAdapter, ucIdx);
			continue;
		}
		/* P2P network type */
		eOPMode = prBssInfo->eCurrentOPMode;

		/* Deactive GO/AP bss to let TOP sleep */
		if (eOPMode == OP_MODE_ACCESS_POINT) {
			/* Force to deactivate Network of GO case */
			u4ClientCount = bssGetClientCount(prAdapter, prBssInfo);
			if (u4ClientCount != 0) {
				prClientList = &prBssInfo->rStaRecOfClientList;
				LINK_FOR_EACH_ENTRY_SAFE(prCurrStaRec,
					prStaRecNext, prClientList, rLinkEntry,
					struct STA_RECORD) {
					p2pFuncDisconnect(prAdapter,
						prBssInfo,
						prCurrStaRec,
						FALSE,
						REASON_CODE_DEAUTH_LEAVING_BSS,
						TRUE);
				}
			}

			DBGLOG(P2P, STATE, "Susp Force Deactive GO\n");
			p2pChangeMediaState(prAdapter, prBssInfo,
				MEDIA_STATE_DISCONNECTED);
			p2pFuncStopComplete(prAdapter, prBssInfo);
		} else if (eOPMode == OP_MODE_INFRASTRUCTURE) {
			/* Deactive GC bss to let TOP sleep */
			if (prBssInfo->prStaRecOfAP == NULL)
				continue;

			/* Force to deactivate Network of GC case */
			DBGLOG(P2P, STATE, "Susp Force Deactive GC\n");

			kalP2PGCIndicateConnectionStatus(
				prAdapter->prGlueInfo,
				(uint8_t) prBssInfo->u4PrivateData,
				NULL, NULL, 0,
				REASON_CODE_DEAUTH_LEAVING_BSS,
				WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY);

			p2pFuncDisconnect(prAdapter, prBssInfo,
				prBssInfo->prStaRecOfAP, FALSE,
				REASON_CODE_DEAUTH_LEAVING_BSS, TRUE);
			p2pFuncStopComplete(prAdapter, prBssInfo);
		}
	}

	prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;

	if (prP2pDevFsmInfo) {
		DBGLOG(P2P, STATE, "Force P2P to IDLE state when suspend\n");
		cnmTimerStopTimer(prAdapter,
			&(prP2pDevFsmInfo->rP2pFsmTimeoutTimer));

		/* Abort device FSM */
		p2pDevFsmStateTransition(prAdapter, prP2pDevFsmInfo,
			P2P_DEV_STATE_IDLE);
	}
}
#endif

#if (CFG_DBDC_SW_FOR_P2P_LISTEN == 1)
/* This function is used to disable dbdc when the dbdc is enabled for
 * p2p listen and the p2p related bss is going to be created.
 * Must disable Dbdc first and enable Dbdc again, otherwise dbdc A+A mode
 * info will not update to FW.
 */
static void
p2pRoleP2pLisStopDbdcDecision(
	struct ADAPTER *prAdapter,
	enum ENUM_P2P_CONNECTION_TYPE eConnRequest)
{
	struct P2P_DEV_FSM_INFO *prP2pDevFsmInfo =
	(struct P2P_DEV_FSM_INFO *) NULL;

	prP2pDevFsmInfo = prAdapter->rWifiVar.prP2pDevFsmInfo;

	if (prP2pDevFsmInfo
		&& prAdapter->rWifiVar.ucDbdcP2pLisEn) {

		DBGLOG(P2P, INFO,
				"IsP2pListenDbdcEn %u eConnRequest %u\n"
				, cnmDbdcIsP2pListenDbdcEn(), eConnRequest);

		if (eConnRequest == P2P_CONNECTION_TYPE_GC
			|| eConnRequest == P2P_CONNECTION_TYPE_GO) {

			cnmTimerStopTimer(prAdapter,
				&(prP2pDevFsmInfo->rP2pListenDbdcTimer));
			prP2pDevFsmInfo->fgIsP2pListening = FALSE;
		}

		if (cnmDbdcIsP2pListenDbdcEn())
			cnmDbdcRuntimeCheckDecision(prAdapter,
				prAdapter->ucP2PDevBssIdx, TRUE);
	}
}
#endif
