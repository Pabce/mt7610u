/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2010, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************/


#include "rt_config.h"


#ifdef DOT11_N_SUPPORT


INT ht_mode_adjust(struct rtmp_adapter*pAd, MAC_TABLE_ENTRY *pEntry, HT_CAPABILITY_IE *peer, RT_HT_CAPABILITY *my)
{
	if ((peer->HtCapInfo.GF) && (my->GF))
	{
		pEntry->MaxHTPhyMode.field.MODE = MODE_HTGREENFIELD;
	}
	else
	{
		pEntry->MaxHTPhyMode.field.MODE = MODE_HTMIX;
		pAd->CommonCfg.AddHTInfo.AddHtInfo2.NonGfPresent = 1;
		pAd->MacTab.fAnyStationNonGF = true;
	}

	if ((peer->HtCapInfo.ChannelWidth) && (my->ChannelWidth))
	{
		pEntry->MaxHTPhyMode.field.BW= BW_40;
		pEntry->MaxHTPhyMode.field.ShortGI = ((my->ShortGIfor40) & (peer->HtCapInfo.ShortGIfor40));
	}
	else
	{
		pEntry->MaxHTPhyMode.field.BW = BW_20;
		pEntry->MaxHTPhyMode.field.ShortGI = ((my->ShortGIfor20) & (peer->HtCapInfo.ShortGIfor20));
		pAd->MacTab.fAnyStation20Only = true;
	}

	return true;
}


INT set_ht_fixed_mcs(struct rtmp_adapter*pAd, MAC_TABLE_ENTRY *pEntry, u8 fixed_mcs, u8 mcs_bound)
{
	if (fixed_mcs == 32)
	{
		/* Fix MCS as HT Duplicated Mode */
		pEntry->MaxHTPhyMode.field.BW = 1;
		pEntry->MaxHTPhyMode.field.MODE = MODE_HTMIX;
		pEntry->MaxHTPhyMode.field.STBC = 0;
		pEntry->MaxHTPhyMode.field.ShortGI = 0;
		pEntry->MaxHTPhyMode.field.MCS = 32;
	}
	else if (pEntry->MaxHTPhyMode.field.MCS > mcs_bound)
	{
		/* STA supports fixed MCS */
		pEntry->MaxHTPhyMode.field.MCS = mcs_bound;
	}

	return true;
}


INT get_ht_max_mcs(struct rtmp_adapter*pAd, u8 *desire_mcs, u8 *cap_mcs)
{
	INT i, j;
	u8 bitmask;


	for (i=23; i>=0; i--)
	{
		j = i/8;
		bitmask = (1<<(i-(j*8)));
		if ((desire_mcs[j] & bitmask) && (cap_mcs[j] & bitmask))
		{
			/*pEntry->MaxHTPhyMode.field.MCS = i; */
			/* find it !!*/
			break;
		}
		if (i==0)
			break;
	}

	return i;
}


INT get_ht_cent_ch(struct rtmp_adapter*pAd, u8 *rf_bw, u8 *ext_ch)
{
	if ((pAd->CommonCfg.HtCapability.HtCapInfo.ChannelWidth  == BW_40) &&
		(pAd->CommonCfg.RegTransmitSetting.field.EXTCHA == EXTCHA_ABOVE)
	)
	{
		*rf_bw = BW_40;
		*ext_ch = EXTCHA_ABOVE;
		pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel + 2;
	}
	else if ((pAd->CommonCfg.Channel > 2) &&
			(pAd->CommonCfg.HtCapability.HtCapInfo.ChannelWidth  == BW_40) &&
			(pAd->CommonCfg.RegTransmitSetting.field.EXTCHA == EXTCHA_BELOW))
	{
		*rf_bw = BW_40;
		*ext_ch = EXTCHA_BELOW;
		if (pAd->CommonCfg.Channel == 14)
			pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel - 1;
		else
			pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel - 2;
	} else {
		return false;
	}

	return true;
}


u8 get_cent_ch_by_htinfo(
	struct rtmp_adapter*pAd,
	ADD_HT_INFO_IE *ht_op,
	HT_CAPABILITY_IE *ht_cap)
{
	u8 cent_ch;

	if ((ht_op->ControlChan > 2)&&
		(ht_op->AddHtInfo.ExtChanOffset == EXTCHA_BELOW) &&
		(ht_cap->HtCapInfo.ChannelWidth == BW_40))
		cent_ch = ht_op->ControlChan - 2;
	else if ((ht_op->AddHtInfo.ExtChanOffset == EXTCHA_ABOVE) &&
		(ht_cap->HtCapInfo.ChannelWidth == BW_40))
		cent_ch = ht_op->ControlChan + 2;
	else
		cent_ch = ht_op->ControlChan;

	return cent_ch;
}


/*
	========================================================================
	Routine Description:
		Caller ensures we has 802.11n support.
		Calls at setting HT from AP/STASetinformation

	Arguments:
		pAd - Pointer to our adapter
		phymode  -

	========================================================================
*/
void RTMPSetHT(
	IN struct rtmp_adapter*pAd,
	IN OID_SET_HT_PHYMODE *pHTPhyMode)
{
	u8 RxStream = pAd->CommonCfg.RxStream;
	INT bw;
	RT_HT_CAPABILITY *rt_ht_cap = &pAd->CommonCfg.DesiredHtPhy;
	HT_CAPABILITY_IE *ht_cap= &pAd->CommonCfg.HtCapability;


	DBGPRINT(RT_DEBUG_TRACE, ("RTMPSetHT : HT_mode(%d), ExtOffset(%d), MCS(%d), BW(%d), STBC(%d), SHORTGI(%d)\n",
										pHTPhyMode->HtMode, pHTPhyMode->ExtOffset,
										pHTPhyMode->MCS, pHTPhyMode->BW,
										pHTPhyMode->STBC, pHTPhyMode->SHORTGI));

	/* Don't zero supportedHyPhy structure.*/
	memset(ht_cap, 0, sizeof(HT_CAPABILITY_IE));
	memset(&pAd->CommonCfg.AddHTInfo, 0, sizeof(pAd->CommonCfg.AddHTInfo));
	memset(&pAd->CommonCfg.NewExtChanOffset, 0, sizeof(pAd->CommonCfg.NewExtChanOffset));
	memset(rt_ht_cap, 0, sizeof(RT_HT_CAPABILITY));

   	if (pAd->CommonCfg.bRdg)
	{
		ht_cap->ExtHtCapInfo.PlusHTC = 1;
		ht_cap->ExtHtCapInfo.RDGSupport = 1;
	}
	else
	{
		ht_cap->ExtHtCapInfo.PlusHTC = 0;
		ht_cap->ExtHtCapInfo.RDGSupport = 0;
	}


	if (RxStream == 1)
	{
		ht_cap->HtCapParm.MaxRAmpduFactor = 2;
		rt_ht_cap->MaxRAmpduFactor = 2;
	}
	else
	{
		ht_cap->HtCapParm.MaxRAmpduFactor = 3;
		rt_ht_cap->MaxRAmpduFactor = 3;
	}

	DBGPRINT(RT_DEBUG_TRACE, ("RTMPSetHT : RxBAWinLimit = %d\n", pAd->CommonCfg.BACapability.field.RxBAWinLimit));

	/* Mimo power save, A-MSDU size, */
	rt_ht_cap->AmsduEnable = (USHORT)pAd->CommonCfg.BACapability.field.AmsduEnable;
	rt_ht_cap->AmsduSize = (u8)pAd->CommonCfg.BACapability.field.AmsduSize;
	rt_ht_cap->MimoPs = (u8)pAd->CommonCfg.BACapability.field.MMPSmode;
	rt_ht_cap->MpduDensity = (u8)pAd->CommonCfg.BACapability.field.MpduDensity;

	ht_cap->HtCapInfo.AMsduSize = (USHORT)pAd->CommonCfg.BACapability.field.AmsduSize;
	ht_cap->HtCapInfo.MimoPs = (USHORT)pAd->CommonCfg.BACapability.field.MMPSmode;
	ht_cap->HtCapParm.MpduDensity = (u8)pAd->CommonCfg.BACapability.field.MpduDensity;

	DBGPRINT(RT_DEBUG_TRACE, ("RTMPSetHT : AMsduSize = %d, MimoPs = %d, MpduDensity = %d, MaxRAmpduFactor = %d\n",
													rt_ht_cap->AmsduSize,
													rt_ht_cap->MimoPs,
													rt_ht_cap->MpduDensity,
													rt_ht_cap->MaxRAmpduFactor));

	if(pHTPhyMode->HtMode == HTMODE_GF)
	{
		ht_cap->HtCapInfo.GF = 1;
		rt_ht_cap->GF = 1;
	}
	else
		rt_ht_cap->GF = 0;

	/* Decide Rx MCSSet*/
	switch (RxStream)
	{
		case 3:
			ht_cap->MCSSet[2] =  0xff;
		case 2:
			ht_cap->MCSSet[1] =  0xff;
		case 1:
		default:
			ht_cap->MCSSet[0] =  0xff;
			break;
	}

	if (pAd->CommonCfg.bForty_Mhz_Intolerant && (pHTPhyMode->BW == BW_40))
	{
		pHTPhyMode->BW = BW_20;
		ht_cap->HtCapInfo.Forty_Mhz_Intolerant = 1;
	}

	// TODO: shiang-6590, how about the "bw" when channel 14 for JP region???
	bw = BW_20;
	if(pHTPhyMode->BW == BW_40)
	{
		ht_cap->MCSSet[4] = 0x1; /* MCS 32*/
		ht_cap->HtCapInfo.ChannelWidth = 1;
		if (pAd->CommonCfg.Channel <= 14)
			ht_cap->HtCapInfo.CCKmodein40 = 1;

		rt_ht_cap->ChannelWidth = 1;
		pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth = 1;
		pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset = (pHTPhyMode->ExtOffset == EXTCHA_BELOW)? (EXTCHA_BELOW): EXTCHA_ABOVE;
		/* Set Regsiter for extension channel position.*/
		mt7610u_mac_set_ctrlch(pAd, pHTPhyMode->ExtOffset);

		/* Turn on BBP 40MHz mode now only as AP . */
		/* Sta can turn on BBP 40MHz after connection with 40MHz AP. Sta only broadcast 40MHz capability before connection.*/
		if ((pAd->OpMode == OPMODE_AP) || INFRA_ON(pAd) || ADHOC_ON(pAd)
		)
		{
			mt7610u_bbp_set_ctrlch(pAd, pHTPhyMode->ExtOffset);
				bw = BW_40;
		}
	}
	else
	{
		ht_cap->HtCapInfo.ChannelWidth = 0;
		rt_ht_cap->ChannelWidth = 0;
		pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth = 0;
		pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset = EXTCHA_NONE;
		pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel;
		/* Turn on BBP 20MHz mode by request here.*/
		bw = BW_20;
	}

	if (pHTPhyMode->BW == BW_40 &&
		pAd->CommonCfg.vht_bw == VHT_BW_80 &&
		pAd->CommonCfg.vht_cent_ch)
		bw = BW_80;

	mt7610u_bbp_set_bw(pAd, bw);


	if(pHTPhyMode->STBC == STBC_USE)
	{
		if (pAd->Antenna.field.TxPath >= 2)
		{
			ht_cap->HtCapInfo.TxSTBC = 1;
			rt_ht_cap->TxSTBC = 1;
		}
		else
		{
			ht_cap->HtCapInfo.TxSTBC = 0;
			rt_ht_cap->TxSTBC = 0;
		}

		/*
			RxSTBC
				0: not support,
				1: support for 1SS
				2: support for 1SS, 2SS
				3: support for 1SS, 2SS, 3SS
		*/
		if (pAd->Antenna.field.RxPath >= 1)
		{
			ht_cap->HtCapInfo.RxSTBC = 1;
			rt_ht_cap->RxSTBC = 1;
		}
		else
		{
			ht_cap->HtCapInfo.RxSTBC = 0;
			rt_ht_cap->RxSTBC = 0;
		}
	}
	else
	{
		rt_ht_cap->TxSTBC = 0;
		rt_ht_cap->RxSTBC = 0;
	}

	if(pHTPhyMode->SHORTGI == GI_400)
	{
		ht_cap->HtCapInfo.ShortGIfor20 = 1;
		ht_cap->HtCapInfo.ShortGIfor40 = 1;
		rt_ht_cap->ShortGIfor20 = 1;
		rt_ht_cap->ShortGIfor40 = 1;
	}
	else
	{
		ht_cap->HtCapInfo.ShortGIfor20 = 0;
		ht_cap->HtCapInfo.ShortGIfor40 = 0;
		rt_ht_cap->ShortGIfor20 = 0;
		rt_ht_cap->ShortGIfor40 = 0;
	}

	/* We support link adaptation for unsolicit MCS feedback, set to 2.*/
	pAd->CommonCfg.AddHTInfo.ControlChan = pAd->CommonCfg.Channel;
	/* 1, the extension channel above the control channel. */

	/* EDCA parameters used for AP's own transmission*/
	if (pAd->CommonCfg.APEdcaParm.bValid == false)
	{
		pAd->CommonCfg.APEdcaParm.bValid = true;
		pAd->CommonCfg.APEdcaParm.Aifsn[0] = 3;
		pAd->CommonCfg.APEdcaParm.Aifsn[1] = 7;
		pAd->CommonCfg.APEdcaParm.Aifsn[2] = 1;
		pAd->CommonCfg.APEdcaParm.Aifsn[3] = 1;

		pAd->CommonCfg.APEdcaParm.Cwmin[0] = 4;
		pAd->CommonCfg.APEdcaParm.Cwmin[1] = 4;
		pAd->CommonCfg.APEdcaParm.Cwmin[2] = 3;
		pAd->CommonCfg.APEdcaParm.Cwmin[3] = 2;

		pAd->CommonCfg.APEdcaParm.Cwmax[0] = 6;
		pAd->CommonCfg.APEdcaParm.Cwmax[1] = 10;
		pAd->CommonCfg.APEdcaParm.Cwmax[2] = 4;
		pAd->CommonCfg.APEdcaParm.Cwmax[3] = 3;

		pAd->CommonCfg.APEdcaParm.Txop[0]  = 0;
		pAd->CommonCfg.APEdcaParm.Txop[1]  = 0;
		pAd->CommonCfg.APEdcaParm.Txop[2]  = 94;
		pAd->CommonCfg.APEdcaParm.Txop[3]  = 47;
	}
	AsicSetEdcaParm(pAd, &pAd->CommonCfg.APEdcaParm);

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{

		RTMPSetIndividualHT(pAd, 0);
	}
#endif /* CONFIG_STA_SUPPORT */

}

/*
	========================================================================
	Routine Description:
		Caller ensures we has 802.11n support.
		Calls at setting HT from AP/STASetinformation

	Arguments:
		pAd - Pointer to our adapter
		phymode  -

	========================================================================
*/
void RTMPSetIndividualHT(
	IN struct rtmp_adapter*pAd,
	IN u8 apidx)
{
	RT_PHY_INFO *pDesired_ht_phy = NULL;
	u8 TxStream = pAd->CommonCfg.TxStream;
	u8 DesiredMcs = MCS_AUTO;
	u8 encrypt_mode = Ndis802_11EncryptionDisabled;

	do
	{



#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		{
			pDesired_ht_phy = &pAd->StaCfg.DesiredHtPhyInfo;
			DesiredMcs = pAd->StaCfg.DesiredTransmitSetting.field.MCS;
			encrypt_mode = pAd->StaCfg.WepStatus;
			break;
		}
#endif /* CONFIG_STA_SUPPORT */
	} while (false);

	if (pDesired_ht_phy == NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("RTMPSetIndividualHT: invalid apidx(%d)\n", apidx));
		return;
	}
	memset(pDesired_ht_phy, 0, sizeof(RT_PHY_INFO));

	DBGPRINT(RT_DEBUG_TRACE, ("RTMPSetIndividualHT : Desired MCS = %d\n", DesiredMcs));
	/* Check the validity of MCS */
	if ((TxStream == 1) && ((DesiredMcs >= MCS_8) && (DesiredMcs <= MCS_15)))
	{
		DBGPRINT(RT_DEBUG_WARN, ("RTMPSetIndividualHT: MCS(%d) is invalid in 1S, reset it as MCS_7\n", DesiredMcs));
		DesiredMcs = MCS_7;
	}

	if ((pAd->CommonCfg.DesiredHtPhy.ChannelWidth == BW_20) && (DesiredMcs == MCS_32))
	{
		DBGPRINT(RT_DEBUG_WARN, ("RTMPSetIndividualHT: MCS_32 is only supported in 40-MHz, reset it as MCS_0\n"));
		DesiredMcs = MCS_0;
	}

#ifdef CONFIG_STA_SUPPORT
	if ((pAd->OpMode == OPMODE_STA) && (pAd->StaCfg.BssType == BSS_INFRA) && (apidx == MIN_NET_DEVICE_FOR_MBSSID))
		;
	else
#endif /* CONFIG_STA_SUPPORT */
	/*
		WFA recommend to restrict the encryption type in 11n-HT mode.
	 	So, the WEP and TKIP are not allowed in HT rate.
	*/
	if (pAd->CommonCfg.HT_DisallowTKIP && IS_INVALID_HT_SECURITY(encrypt_mode))
	{
		DBGPRINT(RT_DEBUG_WARN, ("%s : Use legacy rate in WEP/TKIP encryption mode (apidx=%d)\n",
									__FUNCTION__, apidx));
		return;
	}

	if (pAd->CommonCfg.HT_Disable)
	{
#ifdef CONFIG_STA_SUPPORT
		pAd->StaCfg.bAdhocN = false;
#endif /* CONFIG_STA_SUPPORT */
		DBGPRINT(RT_DEBUG_TRACE, ("%s : HT is disabled\n", __FUNCTION__));
		return;
	}

	pDesired_ht_phy->bHtEnable = true;

	/* Decide desired Tx MCS*/
	switch (TxStream)
	{
		case 1:
			if (DesiredMcs == MCS_AUTO)
				pDesired_ht_phy->MCSSet[0]= 0xff;
			else if (DesiredMcs <= MCS_7)
				pDesired_ht_phy->MCSSet[0]= 1<<DesiredMcs;
			break;

		case 2:
			if (DesiredMcs == MCS_AUTO)
			{
				pDesired_ht_phy->MCSSet[0]= 0xff;
				pDesired_ht_phy->MCSSet[1]= 0xff;
			}
			else if (DesiredMcs <= MCS_15)
			{
				ULONG mode;

				mode = DesiredMcs / 8;
				if (mode < 2)
					pDesired_ht_phy->MCSSet[mode] = (1 << (DesiredMcs - mode * 8));
			}
			break;

		case 3:
			if (DesiredMcs == MCS_AUTO)
			{
				/* MCS0 ~ MCS23, 3 bytes */
				pDesired_ht_phy->MCSSet[0]= 0xff;
				pDesired_ht_phy->MCSSet[1]= 0xff;
				pDesired_ht_phy->MCSSet[2]= 0xff;
			}
			else if (DesiredMcs <= MCS_23)
			{
				ULONG mode;

				mode = DesiredMcs / 8;
				if (mode < 3)
					pDesired_ht_phy->MCSSet[mode] = (1 << (DesiredMcs - mode * 8));
			}
			break;
	}

	if(pAd->CommonCfg.DesiredHtPhy.ChannelWidth == BW_40)
	{
		if (DesiredMcs == MCS_AUTO || DesiredMcs == MCS_32)
			pDesired_ht_phy->MCSSet[4] = 0x1;
	}

	/* update HT Rate setting */
	if (pAd->OpMode == OPMODE_STA)
	{
			MlmeUpdateHtTxRates(pAd, BSS0);
	}
	else
		MlmeUpdateHtTxRates(pAd, apidx);

	if (WMODE_CAP_AC(pAd->CommonCfg.PhyMode)) {
		pDesired_ht_phy->bVhtEnable = true;
		rtmp_set_vht(pAd, pDesired_ht_phy);
	}
}

/*
	========================================================================
	Routine Description:
		Clear the desire HT info per interface

	Arguments:

	========================================================================
*/
void RTMPDisableDesiredHtInfo(
	IN	struct rtmp_adapter *	pAd)
{


#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		memset(&pAd->StaCfg.DesiredHtPhyInfo, 0, sizeof(RT_PHY_INFO));
	}
#endif /* CONFIG_STA_SUPPORT */

}


INT	SetCommonHT(struct rtmp_adapter*pAd)
{
	OID_SET_HT_PHYMODE SetHT;

	if (!WMODE_CAP_N(pAd->CommonCfg.PhyMode))
	{
		/* Clear previous HT information */
		RTMPDisableDesiredHtInfo(pAd);
		return false;
	}

	SetCommonVHT(pAd);

	SetHT.PhyMode = (RT_802_11_PHY_MODE)pAd->CommonCfg.PhyMode;
	SetHT.TransmitNo = ((u8)pAd->Antenna.field.TxPath);
	SetHT.HtMode = (u8)pAd->CommonCfg.RegTransmitSetting.field.HTMODE;
	SetHT.ExtOffset = (u8)pAd->CommonCfg.RegTransmitSetting.field.EXTCHA;
	SetHT.MCS = MCS_AUTO;
	SetHT.BW = (u8)pAd->CommonCfg.RegTransmitSetting.field.BW;
	SetHT.STBC = (u8)pAd->CommonCfg.RegTransmitSetting.field.STBC;
	SetHT.SHORTGI = (u8)pAd->CommonCfg.RegTransmitSetting.field.ShortGI;

	RTMPSetHT(pAd, &SetHT);

	if(pAd->CommonCfg.bBssCoexEnable && pAd->CommonCfg.Bss2040NeedFallBack)
	{
		pAd->CommonCfg.AddHTInfo.AddHtInfo.RecomWidth = 0;
		pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset = 0;
		pAd->CommonCfg.LastBSSCoexist2040.field.BSS20WidthReq = 1;
		pAd->CommonCfg.Bss2040CoexistFlag |= BSS_2040_COEXIST_INFO_SYNC;
		pAd->CommonCfg.Bss2040NeedFallBack = 1;
	}

	return true;
}

/*
	========================================================================
	Routine Description:
		Update HT IE from our capability.

	Arguments:
		Send all HT IE in beacon/probe rsp/assoc rsp/action frame.


	========================================================================
*/
void RTMPUpdateHTIE(
	IN RT_HT_CAPABILITY	*pRtHt,
	IN u8 *pMcsSet,
	OUT HT_CAPABILITY_IE *pHtCapability,
	OUT ADD_HT_INFO_IE *pAddHtInfo)
{
	memset(pHtCapability, 0, sizeof(HT_CAPABILITY_IE));
	memset(pAddHtInfo, 0, sizeof(ADD_HT_INFO_IE));

		pHtCapability->HtCapInfo.ChannelWidth = pRtHt->ChannelWidth;
		pHtCapability->HtCapInfo.MimoPs = pRtHt->MimoPs;
		pHtCapability->HtCapInfo.GF = pRtHt->GF;
		pHtCapability->HtCapInfo.ShortGIfor20 = pRtHt->ShortGIfor20;
		pHtCapability->HtCapInfo.ShortGIfor40 = pRtHt->ShortGIfor40;
		pHtCapability->HtCapInfo.TxSTBC = pRtHt->TxSTBC;
		pHtCapability->HtCapInfo.RxSTBC = pRtHt->RxSTBC;
		pHtCapability->HtCapInfo.AMsduSize = pRtHt->AmsduSize;
		pHtCapability->HtCapParm.MaxRAmpduFactor = pRtHt->MaxRAmpduFactor;
		pHtCapability->HtCapParm.MpduDensity = pRtHt->MpduDensity;

		pAddHtInfo->AddHtInfo.ExtChanOffset = pRtHt->ExtChanOffset ;
		pAddHtInfo->AddHtInfo.RecomWidth = pRtHt->RecomWidth;
		pAddHtInfo->AddHtInfo2.OperaionMode = pRtHt->OperaionMode;
		pAddHtInfo->AddHtInfo2.NonGfPresent = pRtHt->NonGfPresent;
		memmove(pAddHtInfo->MCSSet, /*pRtHt->MCSSet*/pMcsSet, 4); /* rt2860 only support MCS max=32, no need to copy all 16 uchar.*/

        DBGPRINT(RT_DEBUG_TRACE,("RTMPUpdateHTIE <== \n"));
}

#endif /* DOT11_N_SUPPORT */

