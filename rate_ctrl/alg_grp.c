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


#ifdef NEW_RATE_ADAPT_SUPPORT
#include "rt_config.h"


/*
	MlmeSetMcsGroup - set initial mcsGroup based on supported MCSs
		On exit pEntry->mcsGroup is set to the mcsGroup
*/
void MlmeSetMcsGroup(struct rtmp_adapter*pAd, MAC_TABLE_ENTRY *pEntry)
{
#ifdef DOT11N_SS3_SUPPORT
	if ((pEntry->HTCapability.MCSSet[2] == 0xff) && (pAd->CommonCfg.TxStream == 3))
		pEntry->mcsGroup = 3;
	 else
#endif /* DOT11N_SS3_SUPPORT */
	 if ((pEntry->HTCapability.MCSSet[0] == 0xff) &&
		(pEntry->HTCapability.MCSSet[1] == 0xff) &&
		(pAd->CommonCfg.TxStream > 1) &&
		((pAd->CommonCfg.TxStream == 2) || (pEntry->HTCapability.MCSSet[2] == 0x0)))
		pEntry->mcsGroup = 2;
	else
		pEntry->mcsGroup = 1;

#ifdef DOT11_VHT_AC
	// TODO: shiang-6590, fix me!!
	if (pEntry->SupportRateMode & SUPPORT_VHT_MODE)
	{
		if ((pAd->CommonCfg.TxStream == 2) && (pEntry->SupportVHTMCS[8] == 0x1))
			pEntry->mcsGroup = 2;
		else
			pEntry->mcsGroup = 1;
	}
#endif /* DOT11_VHT_AC */
}


/*
	MlmeSelectUpRate - select UpRate based on MCS group
	returns the UpRate index and updates the MCS group
*/
u8 MlmeSelectUpRate(
	IN struct rtmp_adapter*pAd,
	IN MAC_TABLE_ENTRY *pEntry,
	IN RTMP_RA_GRP_TB *pCurrTxRate)
{
	u8 UpRateIdx = 0;

	while (1)
	{
		if ((pEntry->HTCapability.MCSSet[2] == 0xff) && (pAd->CommonCfg.TxStream == 3))
		{
			switch (pEntry->mcsGroup)
			{
				case 0:/* improvement: use round robin mcs when group == 0 */
					UpRateIdx = pCurrTxRate->upMcs3;
					if (UpRateIdx == pCurrTxRate->ItemNo)
					{
						UpRateIdx = pCurrTxRate->upMcs2;
						if (UpRateIdx == pCurrTxRate->ItemNo)
							UpRateIdx = pCurrTxRate->upMcs1;
					}

					if (MlmeGetTxQuality(pEntry, UpRateIdx) > MlmeGetTxQuality(pEntry, pCurrTxRate->upMcs2) &&
						pCurrTxRate->upMcs2 != pCurrTxRate->ItemNo)
						UpRateIdx = pCurrTxRate->upMcs2;

					if (MlmeGetTxQuality(pEntry, UpRateIdx) > MlmeGetTxQuality(pEntry, pCurrTxRate->upMcs1) &&
						pCurrTxRate->upMcs1 != pCurrTxRate->ItemNo)
						UpRateIdx = pCurrTxRate->upMcs1;
					break;
				case 3:
					UpRateIdx = pCurrTxRate->upMcs3;
					break;
				case 2:
					UpRateIdx = pCurrTxRate->upMcs2;
					break;
				case 1:
					UpRateIdx = pCurrTxRate->upMcs1;
					break;
				default:
					DBGPRINT_RAW(RT_DEBUG_ERROR, ("3ss:wrong mcsGroup value\n"));
					break;
			}

		}
		else if (((pEntry->HTCapability.MCSSet[0] == 0xff) &&
				 (pEntry->HTCapability.MCSSet[1] == 0xff) &&
				 (pAd->CommonCfg.TxStream > 1) &&
				 ((pAd->CommonCfg.TxStream == 2) || (pEntry->HTCapability.MCSSet[2] == 0x0)))
#ifdef DOT11_VHT_AC
				|| (pEntry->pTable == RateTableVht2S)
#endif /* DOT11_VHT_AC */
		)
		{
			switch (pEntry->mcsGroup)
			{
				case 0:
					UpRateIdx = pCurrTxRate->upMcs2;
					if (UpRateIdx == pCurrTxRate->ItemNo)
						UpRateIdx = pCurrTxRate->upMcs1;

					if (MlmeGetTxQuality(pEntry, UpRateIdx) > MlmeGetTxQuality(pEntry, pCurrTxRate->upMcs1) &&
						pCurrTxRate->upMcs1 != pCurrTxRate->ItemNo)
						UpRateIdx = pCurrTxRate->upMcs1;
					break;
				case 2:
					UpRateIdx = pCurrTxRate->upMcs2;
					break;
				case 1:
					UpRateIdx = pCurrTxRate->upMcs1;
					break;
				default:
					DBGPRINT_RAW(RT_DEBUG_TRACE, ("wrong mcsGroup value %d\n", pEntry->mcsGroup));
					break;
			}

		}
		else
		{
			switch (pEntry->mcsGroup)
			{
				case 1:
				case 0:
					UpRateIdx = pCurrTxRate->upMcs1;
					break;
				default:
					DBGPRINT_RAW(RT_DEBUG_TRACE, ("wrong mcsGroup value %d\n", pEntry->mcsGroup));
					break;
			}
		}

		/*  If going up from CCK to MCS32 make sure it's allowed */
		if (PTX_RA_GRP_ENTRY(pEntry->pTable, UpRateIdx)->CurrMCS == 32)
		{
			/*  If not allowed then skip over it */
			BOOLEAN mcs32Supported = 0;
			BOOLEAN mcs0Fallback = 0;

			if ((pEntry->HTCapability.MCSSet[4] & 0x1)
#ifdef DBG_CTRL_SUPPORT
				&& (pAd->CommonCfg.DebugFlags & DBF_ENABLE_HT_DUP)
#endif /* DBG_CTRL_SUPPORT */
			)
				mcs32Supported = 1;

#ifdef DBG_CTRL_SUPPORT
			if ((pAd->CommonCfg.DebugFlags & DBF_DISABLE_20MHZ_MCS0)==0)
				mcs0Fallback = 1;
#endif /* DBG_CTRL_SUPPORT */

			if (pEntry->MaxHTPhyMode.field.BW != BW_40 ||
				pAd->CommonCfg.BBPCurrentBW != BW_40 ||
				(!mcs32Supported && !mcs0Fallback))
			{
				UpRateIdx = PTX_RA_GRP_ENTRY(pEntry->pTable, UpRateIdx)->upMcs1;
				pEntry->mcsGroup = 1;
				break;
			}
		}

		/*  If ShortGI and not allowed then mark it as bad. We'll try another group below */
		if (PTX_RA_GRP_ENTRY(pEntry->pTable, UpRateIdx)->ShortGI &&
			!pEntry->MaxHTPhyMode.field.ShortGI)
		{
			MlmeSetTxQuality(pEntry, UpRateIdx, DRS_TX_QUALITY_WORST_BOUND*2);
		}

		/*  If we reached the end of the group then select the best next time */
		if (UpRateIdx == pEntry->CurrTxRateIndex)
		{
			pEntry->mcsGroup = 0;
			break;
		}

		/*  If the current group has bad TxQuality then try another group */
		if ((MlmeGetTxQuality(pEntry, UpRateIdx) > 0) && (pEntry->mcsGroup > 0))
			pEntry->mcsGroup--;
		else
			break;
	}

	return UpRateIdx;
}

/*
	MlmeSelectDownRate - select DownRate.
		pEntry->pTable is assumed to be a pointer to an adaptive rate table with mcsGroup values
		CurrRateIdx - current rate index
		returns the DownRate index. Down Rate = CurrRateIdx if there is no valid Down Rate
*/
u8 MlmeSelectDownRate(
	IN struct rtmp_adapter*pAd,
	IN PMAC_TABLE_ENTRY	pEntry,
	IN u8 CurrRateIdx)
{
	u8 DownRateIdx = PTX_RA_GRP_ENTRY(pEntry->pTable, CurrRateIdx)->downMcs;
	RTMP_RA_GRP_TB *pDownRate;

	/*  Loop until a valid down rate is found */
	while (1) {
		pDownRate = PTX_RA_GRP_ENTRY(pEntry->pTable, DownRateIdx);

		/*  Break out of loop if rate is valid */
		if (pDownRate->Mode==MODE_CCK)
		{
			/*  CCK is valid only if in G band and if not disabled */
			if ((pAd->LatchRfRegs.Channel<=14
#ifdef DBG_CTRL_SUPPORT
				|| (pAd->CommonCfg.DebugFlags & DBF_ENABLE_CCK_5G)
#endif /* DBG_CTRL_SUPPORT */
			     )
#ifdef DBG_CTRL_SUPPORT
				&& ((pAd->CommonCfg.DebugFlags & DBF_DISABLE_CCK)==0)
#endif /* DBG_CTRL_SUPPORT */
			)
				break;
		}
		else if (pDownRate->CurrMCS == MCS_32)
		{
			BOOLEAN valid_mcs32 = FALSE;

			if ((pEntry->MaxHTPhyMode.field.BW == BW_40 && pAd->CommonCfg.BBPCurrentBW == BW_40)
#ifdef DOT11_VHT_AC
				|| (pEntry->MaxHTPhyMode.field.BW == BW_80 && pAd->CommonCfg.BBPCurrentBW == BW_80)
#endif /* DOT11_VHT_AC */
			)
				valid_mcs32 = TRUE;

			/*  If 20MHz MCS0 fallback enabled and in 40MHz then MCS32 is valid and will be mapped to 20MHz MCS0 */
			if (valid_mcs32
#ifdef DBG_CTRL_SUPPORT
				&& ((pAd->CommonCfg.DebugFlags & DBF_DISABLE_20MHZ_MCS0)==0)
#endif /* DBG_CTRL_SUPPORT */
			)
				break;

			/*  MCS32 is valid if enabled and client supports it */
			if (valid_mcs32 && (pEntry->HTCapability.MCSSet[4] & 0x1)
#ifdef DBG_CTRL_SUPPORT
				&& (pAd->CommonCfg.DebugFlags & DBF_ENABLE_HT_DUP)
#endif /* DBG_CTRL_SUPPORT */
			)
				break;
		}
		else
			break;	/*  All other rates are valid */

		/*  Return original rate if we reached the end without finding a valid rate */
		if (DownRateIdx == pDownRate->downMcs)
			return CurrRateIdx;

		/*  Otherwise try the next lower rate */
		DownRateIdx = pDownRate->downMcs;
	}

	return DownRateIdx;
}


/*
	MlmeGetSupportedMcsAdapt - fills in the table of supported MCSs
		pAd - pointer to adapter
		pEntry - MAC Table entry. pEntry->pTable is a rate table with mcsGroup values
		mcs23GI - the MCS23 entry will have this guard interval
		mcs - table of MCS index into the Rate Table. -1 => not supported
*/
void MlmeGetSupportedMcsAdapt(
	IN struct rtmp_adapter *pAd,
	IN PMAC_TABLE_ENTRY pEntry,
	IN u8 mcs23GI,
	OUT CHAR mcs[])
{
	CHAR idx;
	RTMP_RA_GRP_TB *pCurrTxRate;
	u8 *pTable = pEntry->pTable;

	for (idx=0; idx<24; idx++)
		mcs[idx] = -1;

#ifdef DOT11_VHT_AC
	if (pEntry->pTable == RateTableVht2S)
	{
		for (idx = 0; idx < RATE_TABLE_SIZE(pTable); idx++)
		{
			pCurrTxRate = PTX_RA_GRP_ENTRY(pEntry->pTable, idx);
			if ((pCurrTxRate->CurrMCS == MCS_0) && (pCurrTxRate->dataRate == 1) && (mcs[0] == -1))
				mcs[0] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_1 && pCurrTxRate->dataRate == 1)
				mcs[1] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_2 && pCurrTxRate->dataRate == 1)
				mcs[2] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_3 && pCurrTxRate->dataRate == 1)
				mcs[3] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_4 && pCurrTxRate->dataRate == 1)
				mcs[4] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_5 && pCurrTxRate->dataRate == 1)
				mcs[5] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_6 && pCurrTxRate->dataRate == 1)
				mcs[6] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_7 && pCurrTxRate->dataRate == 1)
				mcs[7] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_0 && pCurrTxRate->dataRate == 2)
				mcs[8] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_1 && pCurrTxRate->dataRate == 2)
				mcs[9] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_2 && pCurrTxRate->dataRate == 2)
				mcs[10] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_3 && pCurrTxRate->dataRate == 2)
				mcs[11] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_4 && pCurrTxRate->dataRate == 2)
				mcs[12] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_5 && pCurrTxRate->dataRate == 2)
				mcs[13] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_6 && pCurrTxRate->dataRate == 2)
				mcs[14] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_7 && pCurrTxRate->dataRate == 2)
				mcs[15] = idx;
		}

		return;
	}
	if (pEntry->pTable == RateTableVht1S)
	{
		for (idx = 0; idx < RATE_TABLE_SIZE(pTable); idx++)
		{
			pCurrTxRate = PTX_RA_GRP_ENTRY(pEntry->pTable, idx);
			if ((pCurrTxRate->CurrMCS == MCS_0) && (pCurrTxRate->dataRate == 1) && (mcs[0] == -1))
				mcs[0] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_1 && pCurrTxRate->dataRate == 1)
				mcs[1] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_2 && pCurrTxRate->dataRate == 1)
				mcs[2] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_3 && pCurrTxRate->dataRate == 1)
				mcs[3] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_4 && pCurrTxRate->dataRate == 1)
				mcs[4] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_5 && pCurrTxRate->dataRate == 1)
				mcs[5] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_6 && pCurrTxRate->dataRate == 1)
				mcs[6] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_7 && pCurrTxRate->dataRate == 1)
				mcs[7] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_8 && pCurrTxRate->dataRate == 1)
				mcs[8] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_9 && pCurrTxRate->dataRate == 1)
				mcs[9] = idx;
		}

		return;
	}
	if (pEntry->pTable == RateTableVht1S_MCS7)
	{
		for (idx = 0; idx < RATE_TABLE_SIZE(pTable); idx++)
		{
			pCurrTxRate = PTX_RA_GRP_ENTRY(pEntry->pTable, idx);
			if ((pCurrTxRate->CurrMCS == MCS_0) && (pCurrTxRate->dataRate == 1) && (mcs[0] == -1))
				mcs[0] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_1 && pCurrTxRate->dataRate == 1)
				mcs[1] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_2 && pCurrTxRate->dataRate == 1)
				mcs[2] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_3 && pCurrTxRate->dataRate == 1)
				mcs[3] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_4 && pCurrTxRate->dataRate == 1)
				mcs[4] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_5 && pCurrTxRate->dataRate == 1)
				mcs[5] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_6 && pCurrTxRate->dataRate == 1)
				mcs[6] = idx;
			else if (pCurrTxRate->CurrMCS == MCS_7 && pCurrTxRate->dataRate == 1)
				mcs[7] = idx;
		}

		return;
	}
#endif /* DOT11_VHT_AC */

	/*  check the existence and index of each needed MCS */
	for (idx = 0; idx < RATE_TABLE_SIZE(pTable); idx++)
	{
		pCurrTxRate = PTX_RA_GRP_ENTRY(pEntry->pTable, idx);

		if ((pCurrTxRate->CurrMCS >= 8 && pAd->CommonCfg.TxStream < 2) ||
			(pCurrTxRate->CurrMCS >= 16 && pAd->CommonCfg.TxStream < 3))
			continue;

		/*  Rate Table may contain CCK and MCS rates. Give HT/Legacy priority over CCK */
		if (pCurrTxRate->CurrMCS==MCS_0 && (mcs[0]==-1 || pCurrTxRate->Mode!=MODE_CCK))
			mcs[0] = idx;
		else if (pCurrTxRate->CurrMCS==MCS_1 && (mcs[1]==-1 || pCurrTxRate->Mode!=MODE_CCK))
			mcs[1] = idx;
		else if (pCurrTxRate->CurrMCS==MCS_2 && (mcs[2]==-1 || pCurrTxRate->Mode!=MODE_CCK))
			mcs[2] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_3)
			mcs[3] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_4)
			mcs[4] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_5)
			mcs[5] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_6)
			mcs[6] = idx;
		else if ((pCurrTxRate->CurrMCS == MCS_7) && (pCurrTxRate->ShortGI == GI_800))
			mcs[7] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_8)
			mcs[8] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_9)
			mcs[9] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_10)
			mcs[10] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_11)
			mcs[11] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_12)
			mcs[12] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_13)
			mcs[13] = idx;
		else if ((pCurrTxRate->CurrMCS == MCS_14) && (pCurrTxRate->ShortGI == GI_800))
			mcs[14] = idx;
		else if ((pCurrTxRate->CurrMCS == MCS_15) && (pCurrTxRate->ShortGI == GI_800))
			mcs[15] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_16)
			mcs[16] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_17)
			mcs[17] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_18)
			mcs[18] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_19)
			mcs[19] = idx;
		else if (pCurrTxRate->CurrMCS == MCS_20)
			mcs[20] = idx;
		else if ((pCurrTxRate->CurrMCS == MCS_21) && (pCurrTxRate->ShortGI == GI_800))
			mcs[21] = idx;
		else if ((pCurrTxRate->CurrMCS == MCS_22) && (pCurrTxRate->ShortGI == GI_800))
			mcs[22] = idx;
		else if ((pCurrTxRate->CurrMCS == MCS_23) && (pCurrTxRate->ShortGI == mcs23GI))
			mcs[23] = idx;
	}

#ifdef DBG_CTRL_SUPPORT
	/*  Debug Option: Disable highest MCSs when picking initial MCS based on RSSI */
	if (pAd->CommonCfg.DebugFlags & DBF_INIT_MCS_DIS1)
		mcs[23] = mcs[15] = mcs[7] = mcs[22] = mcs[14] = mcs[6] = 0;
#endif /* DBG_CTRL_SUPPORT */

}


/*
	MlmeSelectTxRateAdapt - select the MCS based on the RSSI and the available MCSs
		pAd - pointer to adapter
		pEntry - pointer to MAC table entry
		mcs - table of MCS index into the Rate Table. -1 => not supported
		Rssi - the Rssi value
		RssiOffset - offset to apply to the Rssi
*/
u8 MlmeSelectTxRateAdapt(
	IN struct rtmp_adapter *pAd,
	IN PMAC_TABLE_ENTRY	pEntry,
	IN CHAR		mcs[],
	IN CHAR		Rssi,
	IN CHAR		RssiOffset)
{
	u8 TxRateIdx = 0;
	u8 *pTable = pEntry->pTable;

#ifdef DBG_CTRL_SUPPORT
	/*  Debug option: Add 6 dB of margin */
	if (pAd->CommonCfg.DebugFlags & DBF_INIT_MCS_MARGIN)
		RssiOffset += 6;
#endif /* DBG_CTRL_SUPPORT */

#ifdef DOT11_N_SUPPORT
#ifdef DOT11_VHT_AC
	if (pTable == RateTableVht1S || pTable == RateTableVht2S || pTable == RateTableVht1S_MCS7)
	{
		if (pTable == RateTableVht2S)
		{
			DBGPRINT_RAW(RT_DEBUG_INFO | DBG_FUNC_RA, ("%s: GRP: 2*2, RssiOffset=%d\n", __FUNCTION__, RssiOffset));

			/* 2x2 peer device (Adhoc, DLS or AP) */
			if (mcs[15] && (Rssi > (-69 + RssiOffset)))
				TxRateIdx = mcs[15];
			else if (mcs[14] && (Rssi > (-71 + RssiOffset)))
				TxRateIdx = mcs[14];
			else if (mcs[13] && (Rssi > (-74 + RssiOffset)))
				TxRateIdx = mcs[13];
			else if (mcs[12] && (Rssi > (-76 + RssiOffset)))
				TxRateIdx = mcs[12];
			else if (mcs[11] && (Rssi > (-80 + RssiOffset)))
				TxRateIdx = mcs[11];
			else if (mcs[10] && (Rssi > (-82 + RssiOffset)))
				TxRateIdx = mcs[10];
			else if (mcs[9] && (Rssi > (-87 + RssiOffset)))
				TxRateIdx = mcs[9];
			else
				TxRateIdx = mcs[0];

			pEntry->mcsGroup = 2;
		}
		else if (pTable == RateTableVht1S)
		{
			DBGPRINT_RAW(RT_DEBUG_INFO | DBG_FUNC_RA, ("%s: GRP: 1*1, RssiOffset=%d\n", __FUNCTION__, RssiOffset));

			/* 1x1 peer device (Adhoc, DLS or AP) */
			if (mcs[9] && (Rssi > (-67 + RssiOffset)))
				TxRateIdx = mcs[9];
			else if (mcs[8] && (Rssi > (-69 + RssiOffset)))
				TxRateIdx = mcs[8];
			else if (mcs[7] && (Rssi > (-71 + RssiOffset)))
				TxRateIdx = mcs[7];
			else if (mcs[6] && (Rssi > (-73 + RssiOffset)))
				TxRateIdx = mcs[6];
			else if (mcs[5] && (Rssi > (-76 + RssiOffset)))
				TxRateIdx = mcs[5];
			else if (mcs[4] && (Rssi > (-78 + RssiOffset)))
				TxRateIdx = mcs[4];
			else if (mcs[3] && (Rssi > (-82 + RssiOffset)))
				TxRateIdx = mcs[3];
			else if (mcs[2] && (Rssi > (-84 + RssiOffset)))
				TxRateIdx = mcs[2];
			else if (mcs[1] && (Rssi > (-89 + RssiOffset)))
				TxRateIdx = mcs[1];
			else
				TxRateIdx = mcs[0];

			pEntry->mcsGroup = 1;
		}
		else
		{
			DBGPRINT_RAW(RT_DEBUG_INFO | DBG_FUNC_RA, ("%s: GRP: 1*1, RssiOffset=%d\n", __FUNCTION__, RssiOffset));

			/* 1x1 peer device (Adhoc, DLS or AP) */
			if (mcs[7] && (Rssi > (-71 + RssiOffset)))
				TxRateIdx = mcs[7];
			else if (mcs[6] && (Rssi > (-73 + RssiOffset)))
				TxRateIdx = mcs[6];
			else if (mcs[5] && (Rssi > (-76 + RssiOffset)))
				TxRateIdx = mcs[5];
			else if (mcs[4] && (Rssi > (-78 + RssiOffset)))
				TxRateIdx = mcs[4];
			else if (mcs[3] && (Rssi > (-82 + RssiOffset)))
				TxRateIdx = mcs[3];
			else if (mcs[2] && (Rssi > (-84 + RssiOffset)))
				TxRateIdx = mcs[2];
			else if (mcs[1] && (Rssi > (-89 + RssiOffset)))
				TxRateIdx = mcs[1];
			else
				TxRateIdx = mcs[0];

			pEntry->mcsGroup = 1;
		}
	}
	else
#endif /* DOT11_VHT_AC */
	 if (ADAPT_RATE_TABLE(pTable) ||
		 (pTable == RateSwitchTable11BGN3S) ||
		 (pTable == RateSwitchTable11BGN3SForABand))
	{/*  N mode with 3 stream */
		if ((pEntry->HTCapability.MCSSet[2] == 0xff) && (pAd->CommonCfg.TxStream == 3))
		{
			if (mcs[23]>=0 && (Rssi > (-72+RssiOffset)))
				TxRateIdx = mcs[23];
			else if (mcs[22]>=0 && (Rssi > (-74+RssiOffset)))
				TxRateIdx = mcs[22];
			else if (mcs[21]>=0 && (Rssi > (-77+RssiOffset)))
				TxRateIdx = mcs[21];
			else if (mcs[20]>=0 && (Rssi > (-79+RssiOffset)))
				TxRateIdx = mcs[20];
			else if (mcs[11]>=0 && (Rssi > (-81+RssiOffset)))
				TxRateIdx = mcs[11];
			else if (mcs[10]>=0 && (Rssi > (-83+RssiOffset)))
				TxRateIdx = mcs[10];
			else if (mcs[2]>=0 && (Rssi > (-86+RssiOffset)))
				TxRateIdx = mcs[2];
			else if (mcs[1]>=0 && (Rssi > (-88+RssiOffset)))
				TxRateIdx = mcs[1];
			else
				TxRateIdx = mcs[0];

			pEntry->mcsGroup = 3;
		}
		else if ((pEntry->HTCapability.MCSSet[0] == 0xff) &&
				(pEntry->HTCapability.MCSSet[1] == 0xff) &&
				(pAd->CommonCfg.TxStream > 1) &&
				((pAd->CommonCfg.TxStream == 2) || (pEntry->HTCapability.MCSSet[2] == 0x0)))
		{
			if (mcs[15]>=0 && (Rssi > (-72+RssiOffset)))
				TxRateIdx = mcs[15];
			else if (mcs[14]>=0 && (Rssi > (-74+RssiOffset)))
				TxRateIdx = mcs[14];
			else if (mcs[13]>=0 && (Rssi > (-77+RssiOffset)))
				TxRateIdx = mcs[13];
			else if (mcs[12]>=0 && (Rssi > (-79+RssiOffset)))
				TxRateIdx = mcs[12];
			else if (mcs[11]>=0 && (Rssi > (-81+RssiOffset)))
				TxRateIdx = mcs[11];
			else if (mcs[10]>=0 && (Rssi > (-83+RssiOffset)))
				TxRateIdx = mcs[10];
			else if (mcs[2]>=0 && (Rssi > (-86+RssiOffset)))
				TxRateIdx = mcs[2];
			else if (mcs[1]>=0 && (Rssi > (-88+RssiOffset)))
				TxRateIdx = mcs[1];
			else
				TxRateIdx = mcs[0];

			pEntry->mcsGroup = 2;
		}
		else
		{
			if (mcs[7]>=0 && (Rssi > (-72+RssiOffset)))
				TxRateIdx = mcs[7];
			else if (mcs[6]>=0 && (Rssi > (-74+RssiOffset)))
				TxRateIdx = mcs[6];
			else if (mcs[5]>=0 && (Rssi > (-77+RssiOffset)))
				TxRateIdx = mcs[5];
			else if (mcs[4]>=0 && (Rssi > (-79+RssiOffset)))
				TxRateIdx = mcs[4];
			else if (mcs[3]>=0 && (Rssi > (-81+RssiOffset)))
				TxRateIdx = mcs[3];
			else if (mcs[2]>=0 && (Rssi > (-83+RssiOffset)))
				TxRateIdx = mcs[2];
			else if (mcs[1]>=0 && (Rssi > (-86+RssiOffset)))
				TxRateIdx = mcs[1];
			else
				TxRateIdx = mcs[0];

			pEntry->mcsGroup = 1;
		}
	}
	else if ((pTable == RateSwitchTable11BGN2S) ||
		(pTable == RateSwitchTable11BGN2SForABand) ||
		(pTable == RateSwitchTable11N2S) ||
		(pTable == RateSwitchTable11N2SForABand))
	{/*  N mode with 2 stream */
		if (mcs[15]>=0 && (Rssi >= (-70+RssiOffset)))
			TxRateIdx = mcs[15];
		else if (mcs[14]>=0 && (Rssi >= (-72+RssiOffset)))
			TxRateIdx = mcs[14];
		else if (mcs[13]>=0 && (Rssi >= (-76+RssiOffset)))
			TxRateIdx = mcs[13];
		else if (mcs[12]>=0 && (Rssi >= (-78+RssiOffset)))
			TxRateIdx = mcs[12];
		else if (mcs[4]>=0 && (Rssi >= (-82+RssiOffset)))
			TxRateIdx = mcs[4];
		else if (mcs[3]>=0 && (Rssi >= (-84+RssiOffset)))
			TxRateIdx = mcs[3];
		else if (mcs[2]>=0 && (Rssi >= (-86+RssiOffset)))
			TxRateIdx = mcs[2];
		else if (mcs[1]>=0 && (Rssi >= (-88+RssiOffset)))
			TxRateIdx = mcs[1];
		else
			TxRateIdx = mcs[0];
	}
	else if ((pTable == RateSwitchTable11BGN1S) ||
			 (pTable == RateSwitchTable11N1S) ||
			 (pTable == RateSwitchTable11N1SForABand))
	{/*  N mode with 1 stream */
		if (mcs[7]>=0 && (Rssi > (-72+RssiOffset)))
			TxRateIdx = mcs[7];
		else if (mcs[6]>=0 && (Rssi > (-74+RssiOffset)))
			TxRateIdx = mcs[6];
		else if (mcs[5]>=0 && (Rssi > (-77+RssiOffset)))
			TxRateIdx = mcs[5];
		else if (mcs[4]>=0 && (Rssi > (-79+RssiOffset)))
			TxRateIdx = mcs[4];
		else if (mcs[3]>=0 && (Rssi > (-81+RssiOffset)))
			TxRateIdx = mcs[3];
		else if (mcs[2]>=0 && (Rssi > (-83+RssiOffset)))
			TxRateIdx = mcs[2];
		else if (mcs[1]>=0 && (Rssi > (-86+RssiOffset)))
			TxRateIdx = mcs[1];
		else
			TxRateIdx = mcs[0];
	}
	else
#endif /*  DOT11_N_SUPPORT */
	{/*  Legacy mode */
		if (mcs[7]>=0 && (Rssi > -70))
			TxRateIdx = mcs[7];
		else if (mcs[6]>=0 && (Rssi > -74))
			TxRateIdx = mcs[6];
		else if (mcs[5]>=0 && (Rssi > -78))
			TxRateIdx = mcs[5];
		else if (mcs[4]>=0 && (Rssi > -82))
			TxRateIdx = mcs[4];
		else if (mcs[4] == -1)	/*  for B-only mode */
			TxRateIdx = mcs[3];
		else if (mcs[3]>=0 && (Rssi > -85))
			TxRateIdx = mcs[3];
		else if (mcs[2]>=0 && (Rssi > -87))
			TxRateIdx = mcs[2];
		else if (mcs[1]>=0 && (Rssi > -90))
			TxRateIdx = mcs[1];
		else
			TxRateIdx = mcs[0];
	}

	return TxRateIdx;
}

/*
	MlmeRAEstimateThroughput - estimate Throughput based on PER and PHY rate
		pEntry - the MAC table entry for this STA
		pCurrTxRate - pointer to Rate table entry for rate
		TxErrorRatio - the PER
*/
static ULONG MlmeRAEstimateThroughput(
	IN struct rtmp_adapter*pAd,
	IN MAC_TABLE_ENTRY *pEntry,
	IN RTMP_RA_GRP_TB *pCurrTxRate,
	IN ULONG TxErrorRatio)
{
	ULONG estTP = (100-TxErrorRatio)*pCurrTxRate->dataRate;

	/*  Adjust rates for MCS32-40MHz mapped to MCS0-20MHz and for non-CCK 40MHz */
	if (pCurrTxRate->CurrMCS == MCS_32)
	{
#ifdef DBG_CTRL_SUPPORT
		if ((pAd->CommonCfg.DebugFlags & DBF_DISABLE_20MHZ_MCS0)==0)
			estTP /= 2;
#endif /* DBG_CTRL_SUPPORT */
	}
	else if ((pCurrTxRate->Mode==MODE_HTMIX) || (pCurrTxRate->Mode==MODE_HTGREENFIELD))
	{
		if (pEntry->MaxHTPhyMode.field.BW==BW_40
#ifdef DBG_CTRL_SUPPORT
			|| (pAd->CommonCfg.DebugFlags & DBF_FORCE_40MHZ)
#endif /* DBG_CTRL_SUPPORT */
		)
			estTP *= 2;
	}

	return estTP;
}

/*
	MlmeRAHybridRule - decide whether to keep the new rate or use old rate
		pEntry - the MAC table entry for this STA
		pCurrTxRate - pointer to Rate table entry for new up rate
		NewTxOkCount - normalized count of Tx packets for new up rate
		TxErrorRatio - the PER
	returns
		TRUE if old rate should be used
*/
BOOLEAN MlmeRAHybridRule(
	IN struct rtmp_adapter *	pAd,
	IN PMAC_TABLE_ENTRY	pEntry,
	IN RTMP_RA_GRP_TB *pCurrTxRate,
	IN ULONG			NewTxOkCount,
	IN ULONG			TxErrorRatio)
{
	ULONG newTP, oldTP;

	if (100*NewTxOkCount < pAd->CommonCfg.TrainUpLowThrd*pEntry->LastTxOkCount)
		return TRUE;

	if (100*NewTxOkCount > pAd->CommonCfg.TrainUpHighThrd*pEntry->LastTxOkCount)
		return FALSE;

	newTP = MlmeRAEstimateThroughput(pAd, pEntry, pCurrTxRate, TxErrorRatio);
	oldTP = MlmeRAEstimateThroughput(pAd, pEntry, PTX_RA_GRP_ENTRY(pEntry->pTable, pEntry->lastRateIdx), pEntry->LastTxPER);

	return (oldTP > newTP);
}

/*
	MlmeNewRateAdapt - perform Rate Adaptation based on PER using New RA algorithm
		pEntry - the MAC table entry for this STA
		UpRateIdx, DownRateIdx - UpRate and DownRate index
		TrainUp, TrainDown - TrainUp and Train Down threhsolds
		TxErrorRatio - the PER

		On exit:
			pEntry->LastSecTxRateChangeAction = RATE_UP or RATE_DOWN if there was a change
			pEntry->CurrTxRateIndex = new rate index
			pEntry->TxQuality is updated
*/
void MlmeNewRateAdapt(
	IN struct rtmp_adapter *	pAd,
	IN PMAC_TABLE_ENTRY	pEntry,
	IN u8 		UpRateIdx,
	IN u8 		DownRateIdx,
	IN ULONG			TrainUp,
	IN ULONG			TrainDown,
	IN ULONG			TxErrorRatio)
{
	USHORT		phyRateLimit20 = 0;
	BOOLEAN		bTrainUp = FALSE;
	u8 *pTable = pEntry->pTable;
	u8 CurrRateIdx = pEntry->CurrTxRateIndex;
	RTMP_RA_GRP_TB *pCurrTxRate = PTX_RA_GRP_ENTRY(pTable, CurrRateIdx);

	pEntry->CurrTxRateStableTime++;

	pEntry->LastSecTxRateChangeAction = RATE_NO_CHANGE;


	if (TxErrorRatio >= TrainDown)
	{

		/*  Downgrade TX quality if PER >= Rate-Down threshold */
		MlmeSetTxQuality(pEntry, CurrRateIdx, DRS_TX_QUALITY_WORST_BOUND);

		if (CurrRateIdx != DownRateIdx)
		{
			pEntry->CurrTxRateIndex = DownRateIdx;
			pEntry->LastSecTxRateChangeAction = RATE_DOWN;
		}
	}
	else
	{
		RTMP_RA_GRP_TB *pUpRate = PTX_RA_GRP_ENTRY(pTable, UpRateIdx);

		/*  Upgrade TX quality if PER <= Rate-Up threshold */
		if (TxErrorRatio <= TrainUp)
		{
			bTrainUp = TRUE;
			MlmeDecTxQuality(pEntry, CurrRateIdx);  /*  quality very good in CurrRate */

			if (pEntry->TxRateUpPenalty) /* always == 0, always go to else */
				pEntry->TxRateUpPenalty --;
			else
			{
				/*
					Decrement the TxQuality of the UpRate and all of the MCS groups.
					Note that UpRate may mot equal one of the MCS groups if MlmeSelectUpRate
					skipped over a rate that is not valid for this configuration.
				*/
				MlmeDecTxQuality(pEntry, UpRateIdx);

				if (pCurrTxRate->upMcs3!=CurrRateIdx &&
					pCurrTxRate->upMcs3!=UpRateIdx)
					MlmeDecTxQuality(pEntry, pCurrTxRate->upMcs3);

				if (pCurrTxRate->upMcs2!=CurrRateIdx &&
						pCurrTxRate->upMcs2!=UpRateIdx &&
						pCurrTxRate->upMcs2!=pCurrTxRate->upMcs3)
					MlmeDecTxQuality(pEntry, pCurrTxRate->upMcs2);

				if (pCurrTxRate->upMcs1!=CurrRateIdx &&
						pCurrTxRate->upMcs1!=UpRateIdx &&
						pCurrTxRate->upMcs1!=pCurrTxRate->upMcs3 &&
						pCurrTxRate->upMcs1!=pCurrTxRate->upMcs2)
					MlmeDecTxQuality(pEntry, pCurrTxRate->upMcs1);
			}
		}
		else if (pEntry->mcsGroup > 0) /* even if TxErrorRatio > TrainUp */
		{
			/*  Moderate PER but some groups are not tried */
			bTrainUp = TRUE;

			/* TxQuality[CurrRateIdx] must be decremented so that mcs won't decrease wrongly */
			MlmeDecTxQuality(pEntry, CurrRateIdx);  /*  quality very good in CurrRate */
			MlmeDecTxQuality(pEntry, UpRateIdx);    /*  may improve next UP rate's quality */
		}

		/*  Don't try up rate if it's greater than the limit */
		if ((phyRateLimit20 != 0) && (pUpRate->dataRate >= phyRateLimit20))
			return;

		/*  If UpRate is good then train up in current BF state */
		if ((CurrRateIdx != UpRateIdx) && (MlmeGetTxQuality(pEntry, UpRateIdx) <= 0) && bTrainUp)
		{
			pEntry->CurrTxRateIndex = UpRateIdx;
			pEntry->LastSecTxRateChangeAction = RATE_UP;
		}
	}

	/*  Handle the rate change */
	if ((pEntry->LastSecTxRateChangeAction != RATE_NO_CHANGE)
#ifdef DBG_CTRL_SUPPORT
		|| (pAd->CommonCfg.DebugFlags & DBF_FORCE_QUICK_DRS)
#endif /* DBG_CTRL_SUPPORT */
	)
	{
		if (pEntry->LastSecTxRateChangeAction!=RATE_NO_CHANGE)
		{
			DBGPRINT_RAW(RT_DEBUG_INFO,("DRS: %sTX rate from %d to %d \n",
				pEntry->LastSecTxRateChangeAction==RATE_UP? "++": "--", CurrRateIdx, pEntry->CurrTxRateIndex));
		}

		pEntry->CurrTxRateStableTime = 0;
		pEntry->TxRateUpPenalty = 0;

		/*  Save last rate information */
		pEntry->lastRateIdx = CurrRateIdx;

		/*  Update TxQuality */
		if (pEntry->LastSecTxRateChangeAction == RATE_DOWN)
		{
			MlmeSetTxQuality(pEntry, pEntry->CurrTxRateIndex, 0);
			pEntry->PER[pEntry->CurrTxRateIndex] = 0;
		}

		/*  Set timer for check in 100 msec */
#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		{
			if (!pAd->StaCfg.StaQuickResponeForRateUpTimerRunning)
			{
				RTMPSetTimer(&pAd->StaCfg.StaQuickResponeForRateUpTimer, pAd->ra_fast_interval);
				pAd->StaCfg.StaQuickResponeForRateUpTimerRunning = TRUE;
			}
		}
#endif /*  CONFIG_STA_SUPPORT */

		/*  Update PHY rate */
		MlmeNewTxRate(pAd, pEntry);
	}
}




#ifdef CONFIG_STA_SUPPORT
void StaQuickResponeForRateUpExecAdapt(
	IN struct rtmp_adapter *pAd,
	IN ULONG i,
	IN CHAR  Rssi)
{
	u8 *				pTable;
	u8 				CurrRateIdx;
	ULONG					TxTotalCnt;
	ULONG					TxErrorRatio = 0;
	PMAC_TABLE_ENTRY		pEntry;
	RTMP_RA_GRP_TB *pCurrTxRate;
	u8 				TrainUp, TrainDown;
	CHAR					ratio;
	ULONG					TxSuccess, TxRetransmit, TxFailCount;
	ULONG					OneSecTxNoRetryOKRationCount;
	BOOLEAN					rateChanged;


	pEntry = &pAd->MacTab.Content[i];
	pTable = pEntry->pTable;

	if (pAd->MacTab.Size == 1)
	{
		TX_STA_CNT1_STRUC		StaTx1;
		TX_STA_CNT0_STRUC		TxStaCnt0;

		/* Update statistic counter */
		NicGetTxRawCounters(pAd, &TxStaCnt0, &StaTx1);

		TxRetransmit = StaTx1.field.TxRetransmit;
		TxSuccess = StaTx1.field.TxSuccess;
		TxFailCount = TxStaCnt0.field.TxFailCount;
	}
	else
	{
		TxRetransmit = pEntry->OneSecTxRetryOkCount;
		TxSuccess = pEntry->OneSecTxNoRetryOkCount;
		TxFailCount = pEntry->OneSecTxFailCount;
	}

	TxTotalCnt = TxRetransmit + TxSuccess + TxFailCount;
	if (TxTotalCnt)
		TxErrorRatio = ((TxRetransmit + TxFailCount) * 100) / TxTotalCnt;

#ifdef MFB_SUPPORT
	if (pEntry->fLastChangeAccordingMfb == TRUE)
	{
		pEntry->fLastChangeAccordingMfb = FALSE;
		pEntry->LastSecTxRateChangeAction = RATE_NO_CHANGE;
		DBGPRINT(RT_DEBUG_INFO | DBG_FUNC_RA,("DRS: MCS is according to MFB, and ignore tuning this sec \n"));

		/* reset all OneSecTx counters */
		RESET_ONE_SEC_TX_CNT(pEntry);
		return;
	}
#endif	/* MFB_SUPPORT */

	/* Remember the current rate */
	CurrRateIdx = pEntry->CurrTxRateIndex;
	pCurrTxRate = PTX_RA_GRP_ENTRY(pTable, CurrRateIdx);

#ifdef DOT11_N_SUPPORT
	if ((Rssi > -65) && (pCurrTxRate->Mode >= MODE_HTMIX) && pEntry->perThrdAdj == 1)
	{
		TrainUp		= (pCurrTxRate->TrainUp + (pCurrTxRate->TrainUp >> 1));
		TrainDown	= (pCurrTxRate->TrainDown + (pCurrTxRate->TrainDown >> 1));
	}
	else
#endif /* DOT11_N_SUPPORT */
	{
		TrainUp		= pCurrTxRate->TrainUp;
		TrainDown	= pCurrTxRate->TrainDown;
	}


#ifdef DBG_CTRL_SUPPORT
	/* Debug option: Concise RA log */
	if ((pAd->CommonCfg.DebugFlags & DBF_SHOW_RA_LOG) || (pAd->CommonCfg.DebugFlags & DBF_DBQ_RA_LOG))
		MlmeRALog(pAd, pEntry, RAL_QUICK_DRS, TxErrorRatio, TxTotalCnt);
#endif /* DBG_CTRL_SUPPORT */

	/*
		CASE 1. when TX samples are fewer than 15, then decide TX rate solely on RSSI
		     (criteria copied from RT2500 for Netopia case)
	*/
	if (TxTotalCnt <= 12)
	{
		/* Go back to the original rate */
		MlmeRestoreLastRate(pEntry);
		DBGPRINT(RT_DEBUG_INFO | DBG_FUNC_RA,("   QuickDRS: TxTotalCnt <= 12, back to original rate \n"));

		MlmeNewTxRate(pAd, pEntry);

		// TODO: should we reset all OneSecTx counters?
		/* RESET_ONE_SEC_TX_CNT(pEntry); */
		return;
	}

	/*
		Compare throughput.
		LastTxCount is based on a time interval of 500 msec or "500-pAd->ra_fast_interval" ms.
	*/
	if ((pEntry->LastTimeTxRateChangeAction == RATE_NO_CHANGE)
#ifdef DBG_CTRL_SUPPORT
		&& (pAd->CommonCfg.DebugFlags & DBF_FORCE_QUICK_DRS)==0
#endif /* DBG_CTRL_SUPPORT */
	)
		ratio = RA_INTERVAL/pAd->ra_fast_interval;
	else
		ratio = (RA_INTERVAL-pAd->ra_fast_interval)/pAd->ra_fast_interval;

	OneSecTxNoRetryOKRationCount = (TxSuccess * ratio);

	/* Downgrade TX quality if PER >= Rate-Down threshold */
	if (TxErrorRatio >= TrainDown)
	{
		MlmeSetTxQuality(pEntry, CurrRateIdx, DRS_TX_QUALITY_WORST_BOUND); /* the only situation when pEntry->TxQuality[CurrRateIdx] = DRS_TX_QUALITY_WORST_BOUND but no rate change */
	}

	pEntry->PER[CurrRateIdx] = (u8)TxErrorRatio;

	/* Perform DRS - consider TxRate Down first, then rate up. */
	if (pEntry->LastSecTxRateChangeAction == RATE_UP)
	{
		BOOLEAN useOldRate;

		// TODO: gaa - Finalize the decision criterion
		/*
			0=>Throughput. Use New Rate if New TP is better than Old TP
			1=>PER. Use New Rate if New PER is less than the TrainDown PER threshold
			2=>Hybrid. Use rate with best TP if difference > 10%. Otherwise use rate with Best Estimated TP
			3=>Hybrid with check that PER<TrainDown Threshold
		*/
		if (pAd->CommonCfg.TrainUpRule == 0)
		{
			useOldRate = (pEntry->LastTxOkCount + 2) >= OneSecTxNoRetryOKRationCount;
		}
		else if (pAd->CommonCfg.TrainUpRule==2 && Rssi<=pAd->CommonCfg.TrainUpRuleRSSI)
		{
			useOldRate = MlmeRAHybridRule(pAd, pEntry, pCurrTxRate, OneSecTxNoRetryOKRationCount, TxErrorRatio);
		}
		else if (pAd->CommonCfg.TrainUpRule==3 && Rssi<=pAd->CommonCfg.TrainUpRuleRSSI)
		{
			useOldRate = (TxErrorRatio >= TrainDown) ||
						 MlmeRAHybridRule(pAd, pEntry, pCurrTxRate, OneSecTxNoRetryOKRationCount, TxErrorRatio);
		}
		else
			useOldRate = TxErrorRatio >= TrainDown;
		if (useOldRate)
		{
			/* If PER>50% or TP<lastTP/2 then double the TxQuality delay */
			if ((TxErrorRatio > 50) || (OneSecTxNoRetryOKRationCount < pEntry->LastTxOkCount/2))
				MlmeSetTxQuality(pEntry, CurrRateIdx, DRS_TX_QUALITY_WORST_BOUND*2);
			else
				MlmeSetTxQuality(pEntry, CurrRateIdx, DRS_TX_QUALITY_WORST_BOUND);

			MlmeRestoreLastRate(pEntry);
			DBGPRINT(RT_DEBUG_INFO | DBG_FUNC_RA,("   QuickDRS: (Up) bad tx ok count (L:%ld, C:%ld)\n", pEntry->LastTxOkCount, OneSecTxNoRetryOKRationCount));
		}
		else
		{
			RTMP_RA_GRP_TB *pLastTxRate = PTX_RA_GRP_ENTRY(pTable, pEntry->lastRateIdx);

			/* Clear the history if we changed the MCS and PHY Rate */
			if ((pCurrTxRate->CurrMCS != pLastTxRate->CurrMCS) &&
				(pCurrTxRate->dataRate != pLastTxRate->dataRate))
				MlmeClearTxQuality(pEntry);

			if (pEntry->mcsGroup == 0)
				MlmeSetMcsGroup(pAd, pEntry);
			DBGPRINT(RT_DEBUG_INFO | DBG_FUNC_RA,("   QuickDRS: (Up) keep rate-up (L:%ld, C:%ld)\n", pEntry->LastTxOkCount, OneSecTxNoRetryOKRationCount));
		}
	}
	else if (pEntry->LastSecTxRateChangeAction == RATE_DOWN)
	{
		if ((TxErrorRatio >= 50) || (TxErrorRatio >= TrainDown))
		{
			MlmeSetMcsGroup(pAd, pEntry);
			DBGPRINT(RT_DEBUG_INFO | DBG_FUNC_RA,("   QuickDRS: (Down) direct train down (TxErrorRatio >= TrainDown)\n"));
		}
		else if ((pEntry->LastTxOkCount + 2) >= OneSecTxNoRetryOKRationCount)
		{
			MlmeRestoreLastRate(pEntry);
			DBGPRINT(RT_DEBUG_INFO | DBG_FUNC_RA,("   QuickDRS: (Down) bad tx ok count (L:%ld, C:%ld)\n", pEntry->LastTxOkCount, OneSecTxNoRetryOKRationCount));
		}
		else
		{
			MlmeSetMcsGroup(pAd, pEntry);
			DBGPRINT(RT_DEBUG_INFO | DBG_FUNC_RA,("   QuickDRS: (Down) keep rate-down (L:%ld, C:%ld)\n", pEntry->LastTxOkCount, OneSecTxNoRetryOKRationCount));
		}
	}

	/* See if we reverted to the old rate */
	rateChanged = (pEntry->CurrTxRateIndex != CurrRateIdx);

	/* Update mcsGroup */
	if (pEntry->LastSecTxRateChangeAction == RATE_UP)
	{
		u8 UpRateIdx;

		/* If RATE_UP failed look for the next group with valid mcs */
		if (pEntry->CurrTxRateIndex != CurrRateIdx && pEntry->mcsGroup > 0)
		{
			pEntry->mcsGroup--;
			pCurrTxRate = PTX_RA_GRP_ENTRY(pTable, pEntry->lastRateIdx);
		}

		switch (pEntry->mcsGroup)
		{
			case 3:
				UpRateIdx = pCurrTxRate->upMcs3;
				break;
			case 2:
				UpRateIdx = pCurrTxRate->upMcs2;
				break;
			case 1:
				UpRateIdx = pCurrTxRate->upMcs1;
				break;
			default:
				UpRateIdx = CurrRateIdx;
				break;
		}

		if (UpRateIdx == pEntry->CurrTxRateIndex)
			pEntry->mcsGroup = 0;
	}

	/* Handle change back to old rate */
	if (rateChanged)
	{
		/* Clear Old Rate's history */
		MlmeSetTxQuality(pEntry, pEntry->CurrTxRateIndex, 0);
		pEntry->TxRateUpPenalty = 0;/*redundant */
		pEntry->PER[pEntry->CurrTxRateIndex] = 0;/*redundant */

		/* Set new Tx rate */
		MlmeNewTxRate(pAd, pEntry);
	}

	// TODO: should we reset all OneSecTx counters?
	/* RESET_ONE_SEC_TX_CNT(pEntry); */
}


void MlmeDynamicTxRateSwitchingAdapt(
    IN struct rtmp_adapter *pAd,
	IN ULONG i,
	IN ULONG TxSuccess,
	IN ULONG TxRetransmit,
	IN ULONG TxFailCount)
{
	u8 *		  pTable;
	u8 		  UpRateIdx, DownRateIdx, CurrRateIdx;
	ULONG			  TxTotalCnt;
	ULONG			  TxErrorRatio = 0;
	MAC_TABLE_ENTRY	  *pEntry;
	RTMP_RA_GRP_TB *pCurrTxRate;
	u8 		  TrainUp, TrainDown;
	CHAR			  Rssi;

	pEntry = &pAd->MacTab.Content[i];
	pTable = pEntry->pTable;

	if ((pAd->MacTab.Size == 1) || (IS_ENTRY_DLS(pEntry)))
	{
		Rssi = RTMPAvgRssi(pAd, &pAd->StaCfg.RssiSample);

		/* Update statistic counter */
		TxTotalCnt = TxRetransmit + TxSuccess + TxFailCount;

		if (TxTotalCnt)
			TxErrorRatio = ((TxRetransmit + TxFailCount) * 100) / TxTotalCnt;
	}
	else
	{
		Rssi = RTMPAvgRssi(pAd, &pEntry->RssiSample);

		TxSuccess = pEntry->OneSecTxNoRetryOkCount;
		TxTotalCnt = pEntry->OneSecTxNoRetryOkCount +
					 pEntry->OneSecTxRetryOkCount +
					 pEntry->OneSecTxFailCount;

		if (TxTotalCnt)
			TxErrorRatio = ((pEntry->OneSecTxRetryOkCount + pEntry->OneSecTxFailCount) * 100) / TxTotalCnt;

#ifdef FIFO_EXT_SUPPORT
		if (pEntry->Aid >= 1 && pEntry->Aid <= 8)
		{
			WCID_TX_CNT_STRUC wcidTxCnt;
			UINT32 regAddr, offset;
			ULONG HwTxCnt, HwErrRatio = 0;

			regAddr = WCID_TX_CNT_0 + (pEntry->Aid - 1) * 4;
			RTMP_IO_READ32(pAd, regAddr, &wcidTxCnt.word);

			HwTxCnt = wcidTxCnt.field.succCnt + wcidTxCnt.field.reTryCnt;
			if (HwTxCnt)
				HwErrRatio = (wcidTxCnt.field.reTryCnt * 100) / HwTxCnt;

			DBGPRINT(RT_DEBUG_TRACE ,("%s():TxErrRatio(Aid:%d, MCS:%d, Hw:0x%x-0x%x, Sw:0x%x-%x)\n",
					__FUNCTION__, pEntry->Aid, pEntry->HTPhyMode.field.MCS,
					HwTxCnt, HwErrRatio, TxTotalCnt, TxErrorRatio));

			TxSuccess = wcidTxCnt.field.succCnt;
			TxRetransmit = wcidTxCnt.field.reTryCnt;
			TxErrorRatio = HwErrRatio;
			TxTotalCnt = HwTxCnt;
		}
#endif /* FIFO_EXT_SUPPORT */
	}

	/* Save LastTxOkCount, LastTxPER and last MCS action for StaQuickResponeForRateUpExec */
	pEntry->LastTxOkCount = TxSuccess;
	pEntry->LastTxPER = (u8)TxErrorRatio;
	pEntry->LastTimeTxRateChangeAction = pEntry->LastSecTxRateChangeAction;

	/* decide the next upgrade rate and downgrade rate, if any */
	CurrRateIdx = pEntry->CurrTxRateIndex;
	pCurrTxRate = PTX_RA_GRP_ENTRY(pTable, CurrRateIdx);
	UpRateIdx = MlmeSelectUpRate(pAd, pEntry, pCurrTxRate);
	DownRateIdx = MlmeSelectDownRate(pAd, pEntry, CurrRateIdx);

#ifdef DOT11_N_SUPPORT
	/*
		when Rssi > -65, there is a lot of interference usually. therefore, the algorithm tends to choose the mcs lower than the optimal one.
		by increasing the thresholds, the chosen mcs will be closer to the optimal mcs
	*/
	if ((Rssi > -65) && (pCurrTxRate->Mode >= MODE_HTMIX) && pEntry->perThrdAdj == 1)
	{
		TrainUp		= (pCurrTxRate->TrainUp + (pCurrTxRate->TrainUp >> 1));
		TrainDown	= (pCurrTxRate->TrainDown + (pCurrTxRate->TrainDown >> 1));
	}
	else
#endif /* DOT11_N_SUPPORT */
	{
		TrainUp		= pCurrTxRate->TrainUp;
		TrainDown	= pCurrTxRate->TrainDown;
	}


#ifdef DBG_CTRL_SUPPORT
	/* Debug option: Concise RA log */
	if ((pAd->CommonCfg.DebugFlags & DBF_SHOW_RA_LOG) || (pAd->CommonCfg.DebugFlags & DBF_DBQ_RA_LOG))
		MlmeRALog(pAd, pEntry, RAL_NEW_DRS, TxErrorRatio, TxTotalCnt);
#endif /* DBG_CTRL_SUPPORT */

#ifdef MFB_SUPPORT
	if (pEntry->fLastChangeAccordingMfb == TRUE)
	{
		pEntry->fLastChangeAccordingMfb = FALSE;
		pEntry->LastSecTxRateChangeAction = RATE_NO_CHANGE;
		DBGPRINT_RAW(RT_DEBUG_TRACE,("DRS: MCS is according to MFB, and ignore tuning this sec \n"));

		/* reset all OneSecTx counters */
		RESET_ONE_SEC_TX_CNT(pEntry);

		return;
	}
#endif	/* MFB_SUPPORT */


	/*
		CASE 1. when TX samples are fewer than 15, then decide TX rate solely on RSSI
		     (criteria copied from RT2500 for Netopia case)
	*/
	if (TxTotalCnt <= 15)
	{
		pEntry->lowTrafficCount++;

		if (pEntry->lowTrafficCount >= pAd->CommonCfg.lowTrafficThrd)
		{
			u8 TxRateIdx;
			CHAR	mcs[24];
			CHAR	RssiOffset = 0;

			pEntry->lowTrafficCount = 0;

			/* Check existence and get index of each MCS */
			MlmeGetSupportedMcsAdapt(pAd, pEntry, GI_800, mcs);

			if (pAd->LatchRfRegs.Channel <= 14)
			{
				if (pAd->NicConfig2.field.ExternalLNAForG)
				{
					RssiOffset = 2;
				}
				else if (ADAPT_RATE_TABLE(pTable))
				{
					RssiOffset = 0;
				}
				else
				{
					RssiOffset = 5;
				}
			}
			else
			{
				if (pAd->NicConfig2.field.ExternalLNAForA)
				{
					RssiOffset = 5;
				}
				else if (ADAPT_RATE_TABLE(pTable))
				{
					RssiOffset = 2;
				}
				else
				{
					RssiOffset = 8;
				}
			}

			/* Select the Tx rate based on the RSSI */
			TxRateIdx = MlmeSelectTxRateAdapt(pAd, pEntry, mcs, Rssi, RssiOffset);

			/* if (TxRateIdx != pEntry->CurrTxRateIndex) */
			{
				pEntry->lastRateIdx = pEntry->CurrTxRateIndex;
				MlmeSetMcsGroup(pAd, pEntry);

				pEntry->CurrTxRateIndex = TxRateIdx;
				MlmeNewTxRate(pAd, pEntry);
				if (!pEntry->fLastSecAccordingRSSI)
				{
					DBGPRINT_RAW(RT_DEBUG_INFO,("DRS: TxTotalCnt <= 15, switch to MCS%d according to RSSI (%d), RssiOffset=%d\n", pEntry->HTPhyMode.field.MCS, Rssi, RssiOffset));
				}
			}

			MlmeClearAllTxQuality(pEntry);	/* clear all history */
			pEntry->fLastSecAccordingRSSI = TRUE;
		}

		/* reset all OneSecTx counters */
		RESET_ONE_SEC_TX_CNT(pEntry);

		return;
	}

	pEntry->lowTrafficCount = 0;

	if (pEntry->fLastSecAccordingRSSI == TRUE)
	{
		pEntry->fLastSecAccordingRSSI = FALSE;
		pEntry->LastSecTxRateChangeAction = RATE_NO_CHANGE;
		/* DBGPRINT_RAW(RT_DEBUG_TRACE,("DRS: MCS is according to RSSI, and ignore tuning this sec \n")); */

		/* reset all OneSecTx counters */
		RESET_ONE_SEC_TX_CNT(pEntry);

		return;
	}

	pEntry->PER[CurrRateIdx] = (u8)TxErrorRatio;

	/* Select rate based on PER */
	MlmeNewRateAdapt(pAd, pEntry, UpRateIdx, DownRateIdx, TrainUp, TrainDown, TxErrorRatio);

#ifdef DOT11N_SS3_SUPPORT
	/* Turn off RDG when 3SS and rx count > tx count*5 */
	MlmeCheckRDG(pAd, pEntry);
#endif /* DOT11N_SS3_SUPPORT */

	/* reset all OneSecTx counters */
	RESET_ONE_SEC_TX_CNT(pEntry);

}
#endif /* CONFIG_STA_SUPPORT */


/*
	Set_RateTable_Proc - Display or replace byte for item in RateSwitchTableAdapt11N3S
		usage: iwpriv ra0 set RateTable=<item>[:<offset>:<value>]
*/
INT Set_RateTable_Proc(struct rtmp_adapter*pAd, char *arg)
{
	u8 *pTable, TableSize, InitTxRateIdx;
	int i;
	MAC_TABLE_ENTRY *pEntry;
	int itemNo, rtIndex, value;
	u8 *pRateEntry;

	/* Find first Associated STA in MAC table */
	for (i=1; i<MAX_LEN_OF_MAC_TABLE; i++)
	{
		pEntry = &pAd->MacTab.Content[i];
		if (IS_ENTRY_CLIENT(pEntry) && pEntry->Sst==SST_ASSOC)
			break;
	}

	if (i==MAX_LEN_OF_MAC_TABLE)
	{
	    DBGPRINT(RT_DEBUG_ERROR, ("Set_RateTable_Proc: Empty MAC Table\n"));
		return FALSE;
	}

	/* Get peer's rate table */
	MlmeSelectTxRateTable(pAd, pEntry, &pTable, &TableSize, &InitTxRateIdx);

	/* Get rate index */
	itemNo = simple_strtol(arg, &arg, 10);
	if (itemNo<0 || itemNo>=RATE_TABLE_SIZE(pTable))
		return FALSE;

#ifdef NEW_RATE_ADAPT_SUPPORT
	if (ADAPT_RATE_TABLE(pTable))
		pRateEntry = (u8 *)PTX_RA_GRP_ENTRY(pTable, itemNo);
	else
#endif /* NEW_RATE_ADAPT_SUPPORT */
		pRateEntry = (u8 *)PTX_RA_LEGACY_ENTRY(pTable, itemNo);

	/* If no addtional parameters then print the entry */
	if (*arg != ':') {
		DBGPRINT(RT_DEBUG_OFF, ("Set_RateTable_Proc::%d\n", itemNo));
	}
	else {
		/* Otherwise get the offset and the replace byte */
		while (*arg<'0' || *arg>'9')
			arg++;
		rtIndex = simple_strtol(arg, &arg, 10);
		if (rtIndex<0 || rtIndex>9)
			return FALSE;

		if (*arg!=':')
			return FALSE;
		while (*arg<'0' || *arg>'9')
			arg++;
		value = simple_strtol(arg, &arg, 10);
		pRateEntry[rtIndex] = value;
		DBGPRINT(RT_DEBUG_OFF, ("Set_RateTable_Proc::%d:%d:%d\n", itemNo, rtIndex, value));
	}

    DBGPRINT(RT_DEBUG_OFF, ("%d, 0x%02x, %d, %d, %d, %d, %d, %d, %d, %d\n",
		pRateEntry[0], pRateEntry[1], pRateEntry[2], pRateEntry[3], pRateEntry[4],
		pRateEntry[5], pRateEntry[6], pRateEntry[7], pRateEntry[8], pRateEntry[9]));

	return TRUE;
}


INT	Set_PerThrdAdj_Proc(
	IN	struct rtmp_adapter *pAd,
	IN	char *		arg)
{
	u8 i;
	for (i=0; i<MAX_LEN_OF_MAC_TABLE; i++){
		pAd->MacTab.Content[i].perThrdAdj = simple_strtol(arg, 0, 10);
	}
	return TRUE;
}

/* Set_LowTrafficThrd_Proc - set threshold for reverting to default MCS based on RSSI */
INT	Set_LowTrafficThrd_Proc(
	IN	struct rtmp_adapter *pAd,
	IN	char *		arg)
{
	pAd->CommonCfg.lowTrafficThrd = simple_strtol(arg, 0, 10);

	return TRUE;
}

/* Set_TrainUpRule_Proc - set rule for Quick DRS train up */
INT	Set_TrainUpRule_Proc(
	IN	struct rtmp_adapter *pAd,
	IN	char *		arg)
{
	pAd->CommonCfg.TrainUpRule = simple_strtol(arg, 0, 10);

	return TRUE;
}

/* Set_TrainUpRuleRSSI_Proc - set RSSI threshold for Quick DRS Hybrid train up */
INT	Set_TrainUpRuleRSSI_Proc(
	IN	struct rtmp_adapter *pAd,
	IN	char *		arg)
{
	pAd->CommonCfg.TrainUpRuleRSSI = simple_strtol(arg, 0, 10);

	return TRUE;
}

/* Set_TrainUpLowThrd_Proc - set low threshold for Quick DRS Hybrid train up */
INT	Set_TrainUpLowThrd_Proc(
	IN	struct rtmp_adapter *pAd,
	IN	char *		arg)
{
	pAd->CommonCfg.TrainUpLowThrd = simple_strtol(arg, 0, 10);

	return TRUE;
}

/* Set_TrainUpHighThrd_Proc - set high threshold for Quick DRS Hybrid train up */
INT	Set_TrainUpHighThrd_Proc(
	IN	struct rtmp_adapter *pAd,
	IN	char *		arg)
{
	pAd->CommonCfg.TrainUpHighThrd = simple_strtol(arg, 0, 10);

	return TRUE;
}

#endif /* NEW_RATE_ADAPT_SUPPORT */

