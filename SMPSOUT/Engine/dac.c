#include <stddef.h>	// for NULL
#include "stdtype.h"
#include "dac.h"
#include "smps.h"
#include "../chips/mamedef.h"
#include "../Sound.h"
#include "../chips/2612intf.h"

#define CLOCK_Z80	3579545

typedef struct _dac_state
{
	const DAC_SAMPLE* DACSmplPtr;
	const DAC_TABLE* DACTblPtr;
	const UINT8* DPCMData;
	const UINT8* SmplData;
	UINT32 SmplLen;
	UINT32 FreqForce;	// FreqForce and RateForce force override the current playback speed.
	UINT32 RateForce;	// The priority order (highest to lowest) is: FreqForce, RateForce, OverriddenRate, Rate
	UINT32 Pos;
	UINT32 PosFract;	// 16.16 fixed point (upper 16 bits are used during calculation)
	UINT32 DeltaFract;	// 16.16 fixed point Position Step
	UINT16 Volume;
	INT16 OutSmpl;		// needs to be 16-bit to allow volumes > 100%
	UINT8 Compr;
	UINT8 PbFlags;
	UINT8 DPCMState;
	UINT8 DPCMNibble;
} DAC_STATE;


// Function Prototypes
//void SetDACDriver(DAC_CFG* DACSet);
static UINT32 CalcDACFreq(UINT32 Rate);
static UINT32 CalcDACDelta_Hz(UINT32 FreqHz);
static UINT32 CalcDACDelta_Rate(UINT32 Rate);
//void UpdateDAC(UINT32 Samples);
//void DAC_Reset(void);
//void DAC_ResetOverride(void);
//void DAC_SetFeature(UINT8 Chn, UINT8 DacFlag, UINT8 Set);
//void DAC_SetRateOverride(UINT16 SmplID, UINT32 Rate);
//void DAC_SetVolume(UINT8 Chn, UINT16 Volume);
//void DAC_Stop(UINT8 Chn);
//void DAC_Play(UINT8 Chn, UINT16 SmplID);
//void DAC_SetRate(UINT8 Chn, UINT32 Rate, UINT8 MidNote);
//void DAC_SetFrequency(UINT8 Chn, UINT32 Freq, UINT8 MidNote);


DAC_CFG* DACDrv = NULL;

extern UINT32 SampleRate;

#define MAX_DAC_CHNS	4
static DAC_STATE DACChnState[MAX_DAC_CHNS];

void SetDACDriver(DAC_CFG* DACSet)
{
	DACDrv = DACSet;
	
	if (! DACDrv->Cfg.Channels || DACDrv->Cfg.Channels > MAX_DAC_CHNS)
		DACDrv->Cfg.Channels = MAX_DAC_CHNS;
	if (! DACDrv->Cfg.VolDiv)
		DACDrv->Cfg.VolDiv = 1;
	
	return;
}

static UINT32 CalcDACFreq(UINT32 Rate)
{
	UINT32 Numerator;
	UINT32 Divisor;
	
	switch(DACDrv->Cfg.RateMode)
	{
	case DACRM_DELAY:
		if (DACDrv->Cfg.BaseCycles)
		{
			Numerator = CLOCK_Z80 * DACDrv->Cfg.LoopSamples;
			Divisor = DACDrv->Cfg.BaseCycles + DACDrv->Cfg.LoopCycles * (Rate - 1);
		}
		else
		{
			Numerator = DACDrv->Cfg.BaseRate * 100;
			Divisor = DACDrv->Cfg.Divider + Rate * 100;
		}
		break;
	case DACRM_OVERFLOW:
	case DACRM_NOVERFLOW:
		if (Rate == DACRM_NOVERFLOW)
			Rate = 0x100 - Rate;
		if (DACDrv->Cfg.BaseCycles)
		{
			Numerator = CLOCK_Z80 * DACDrv->Cfg.LoopSamples * Rate;
			Divisor = DACDrv->Cfg.BaseCycles * 0x100;
		}
		else
		{
			Numerator = DACDrv->Cfg.BaseRate * Rate;
			Divisor = DACDrv->Cfg.Divider / 100;
		}
		break;
	default:
		Numerator = 0;
		Divisor = 1;
		break;
	}
	
	return (Numerator + Divisor / 2) / Divisor;
}

static UINT32 CalcDACDelta_Hz(UINT32 FreqHz)	// returns 16.16 fixed point delta
{
	UINT32 Numerator;
	
	Numerator = FreqHz << 16;
	return (Numerator + SampleRate / 2) / SampleRate;
}

static UINT32 CalcDACDelta_Rate(UINT32 Rate)	// returns 16.16 fixed point delta
{
	UINT64 Numerator;
	UINT64 Divisor;
	
	switch(DACDrv->Cfg.RateMode)
	{
	case DACRM_DELAY:
		if (DACDrv->Cfg.BaseCycles)
		{
			Numerator = CLOCK_Z80 * DACDrv->Cfg.LoopSamples;
			Divisor = DACDrv->Cfg.BaseCycles + DACDrv->Cfg.LoopCycles * (Rate - 1);
		}
		else
		{
			Numerator = DACDrv->Cfg.BaseRate * 100;
			Divisor = DACDrv->Cfg.Divider + Rate * 100;
		}
		break;
	case DACRM_OVERFLOW:
	case DACRM_NOVERFLOW:
		if (Rate == DACRM_NOVERFLOW)
			Rate = 0x100 - Rate;
		if (DACDrv->Cfg.BaseCycles)
		{
			Numerator = CLOCK_Z80 * DACDrv->Cfg.LoopSamples * Rate;
			Divisor = DACDrv->Cfg.BaseCycles * 0x100;
		}
		else
		{
			Numerator = DACDrv->Cfg.BaseRate * Rate * 100;
			Divisor = DACDrv->Cfg.Divider;
		}
		break;
	default:
		Numerator = 0;
		Divisor = 1;
		break;
	}
	
	Numerator <<= 16;
	Divisor *= SampleRate;
	return (UINT32)((Numerator + Divisor / 2) / Divisor);
}

void UpdateDAC(UINT32 Samples)
{
	DAC_STATE* ChnState;
	UINT8 CurChn;
	INT16 OutSmpl;
	UINT8 FnlSmpl;
	UINT16 ProcessedSmpls;	// if 0, nothing is sent to the YM2612
	UINT8 DacStopSignal;
	
	if (DACDrv == NULL)
		return;
	
	ym2612_w(0x00, 0x00, 0x2A);
	
	while(Samples)
	{
		ProcessedSmpls = 0;
		DacStopSignal = 0;
		OutSmpl = 0x0000;
		for (CurChn = 0; CurChn < DACDrv->Cfg.Channels; CurChn ++)
		{
			ChnState = &DACChnState[CurChn];
			if (ChnState->DACSmplPtr == NULL)
				continue;
			
			ChnState->PosFract += ChnState->DeltaFract;
			while(ChnState->PosFract >= 0x10000)
			{
				ChnState->PosFract -= 0x10000;
				if (ChnState->Compr == COMPR_PCM)
				{
					ChnState->OutSmpl = ChnState->SmplData[ChnState->Pos] - 0x80;
					if (ChnState->PbFlags & DACFLAG_REVERSE)
						ChnState->Pos --;
					else
						ChnState->Pos ++;
					ChnState->SmplLen --;
				}
				else if (ChnState->Compr == COMPR_DPCM)
				{
					UINT8 NibbleData;
					
					NibbleData = ChnState->SmplData[ChnState->Pos];
					if (! ChnState->DPCMNibble)
					{
						NibbleData >>= 4;
					}
					else
					{
						//NibbleData >>= 0;
						if (ChnState->PbFlags & DACFLAG_REVERSE)
							ChnState->Pos --;
						else
							ChnState->Pos ++;
						ChnState->SmplLen --;
					}
					NibbleData &= 0x0F;
					ChnState->DPCMNibble ^= 0x01;
					
					if (ChnState->PbFlags & DACFLAG_REVERSE)
						ChnState->DPCMState -= ChnState->DPCMData[NibbleData];
					else
						ChnState->DPCMState += ChnState->DPCMData[NibbleData];
					ChnState->OutSmpl = ChnState->DPCMState - 0x80;
				}
				
				if (ChnState->Volume != 0x100)
					ChnState->OutSmpl = (ChnState->OutSmpl * ChnState->Volume) >> 8;
				
				ProcessedSmpls ++;
				
				if (! ChnState->SmplLen)
				{
					if ((ChnState->PbFlags & DACFLAG_LOOP) || (ChnState->DACSmplPtr->Flags & DACFLAG_LOOP))
					{
						ChnState->SmplLen = ChnState->DACSmplPtr->Size;
						if (ChnState->PbFlags & DACFLAG_REVERSE)
							ChnState->Pos = ChnState->SmplLen - 1;
						else
							ChnState->Pos = 0x00;
						ChnState->DPCMState = 0x80;
					}
					else
					{
						ChnState->DACSmplPtr = NULL;
						DacStopSignal = 1;
						break;
					}
				}
			}
			OutSmpl += ChnState->OutSmpl;
		}	// end for (CurChn)
		
		if (ProcessedSmpls)
		{
			OutSmpl /= DACDrv->Cfg.VolDiv;
			if (OutSmpl < -0x80)
				OutSmpl = -0x80;
			else if (OutSmpl > 0x7F)
				OutSmpl = 0x7F;
			FnlSmpl = (UINT8)(0x80 + OutSmpl);
			ym2612_w(0x00, 0x01, FnlSmpl);	// write directly to chip, skipping VGM logging
		}
		Samples --;
	}
	
	if (DacStopSignal)
	{
		ProcessedSmpls = 0;
		for (CurChn = 0; CurChn < DACDrv->Cfg.Channels; CurChn ++)
		{
			ChnState = &DACChnState[CurChn];
			if (ChnState->DACSmplPtr != NULL)
				ProcessedSmpls ++;
		}
		if (! ProcessedSmpls)
			SetDACState(0x00);	// also does WriteFMI(0x2B, 0x00);
	}
	
	return;
}


void DAC_Reset(void)
{
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < MAX_DAC_CHNS; CurChn ++)
	{
		DACChnState[CurChn].DACSmplPtr = NULL;
		DACChnState[CurChn].PbFlags = 0x00;
		DACChnState[CurChn].Volume = 0x100;
	}
	
	DAC_ResetOverride();
	
	return;
}

void DAC_ResetOverride(void)
{
	UINT16 CurSmpl;
	UINT8 CurChn;
	
	if (DACDrv != NULL)
	{
		for (CurSmpl = 0; CurSmpl < DACDrv->SmplCount; CurSmpl ++)
			DACDrv->SmplTbl[CurSmpl].OverriddenRate = 0x00;
	}
	
	for (CurChn = 0; CurChn < MAX_DAC_CHNS; CurChn ++)
	{
		DACChnState[CurChn].FreqForce = 0;
		DACChnState[CurChn].RateForce = 0;
	}
	
	return;
}

void DAC_SetFeature(UINT8 Chn, UINT8 DacFlag, UINT8 Set)
{
	if (Chn >= MAX_DAC_CHNS)
		return;
	if (DACDrv != NULL && Chn >= DACDrv->Cfg.Channels)
		return;
	
	if (Set)
		DACChnState[Chn].PbFlags |= DacFlag;
	else
		DACChnState[Chn].PbFlags &= ~DacFlag;
	
	return;
}

void DAC_SetRateOverride(UINT16 SmplID, UINT32 Rate)
{
	if (DACDrv == NULL)
		return;
	
	if (SmplID >= DACDrv->TblCount)
		return;
	
	DACDrv->SmplTbl[SmplID].OverriddenRate = Rate;
	
	return;
}

void DAC_SetVolume(UINT8 Chn, UINT16 Volume)
{
	if (Chn >= MAX_DAC_CHNS)
		return;
	if (DACDrv != NULL && Chn >= DACDrv->Cfg.Channels)
		return;
	
	DACChnState[Chn].Volume = Volume;
	
	return;
}

void DAC_Stop(UINT8 Chn)
{
	DAC_STATE* DACChn;
	UINT8 CurChn;
	
	if (Chn >= MAX_DAC_CHNS)
		return;
	
	DACChn = &DACChnState[Chn];
	if (DACChn->DACSmplPtr == NULL)
		return;
	
	DACChn->DACSmplPtr = NULL;
	
	for (CurChn = 0; CurChn < MAX_DAC_CHNS; CurChn ++)
	{
		if (DACChnState[CurChn].DACSmplPtr != NULL)
			return;
	}
	SetDACState(0x00);	// also does WriteFMI(0x2B, 0x00);
	
	return;
}

void DAC_Play(UINT8 Chn, UINT16 SmplID)
{
	UINT32 Rate;
	DAC_TABLE* TempEntry;
	DAC_SAMPLE* TempSmpl;
	DAC_STATE* DACChn;
	UINT32 FreqHz;
	
	if (Chn >= MAX_DAC_CHNS || DACDrv == NULL)
		return;
	if (Chn >= DACDrv->Cfg.Channels)
		return;
	
	if (SmplID == 0xFFFF)
	{
		DAC_Stop(Chn);
		return;
	}
	
	if (SmplID >= DACDrv->TblCount)
		return;
	TempEntry = &DACDrv->SmplTbl[SmplID];
	if (TempEntry->Sample == 0xFFFF || TempEntry->Sample >= DACDrv->SmplCount)
		return;
	
	TempSmpl = &DACDrv->Smpls[TempEntry->Sample];
	if (! TempSmpl->Size)
	{
		DAC_Stop(Chn);
		return;
	}
	DACChn = &DACChnState[Chn];
	
	DACChn->DACSmplPtr = TempSmpl;
	DACChn->DACTblPtr = TempEntry;
	DACChn->DPCMData = TempSmpl->DPCMArr;
	DACChn->SmplData = TempSmpl->Data;
	DACChn->Compr = TempSmpl->Compr;
	DACChn->DPCMState = 0x80;
	DACChn->DPCMNibble = 0x00;
	DACChn->OutSmpl = 0x0000;
	
	DACChn->SmplLen = TempSmpl->Size;
	if (DACChn->PbFlags & DACFLAG_REVERSE)
		DACChn->Pos = DACChn->SmplLen - 1;
	else
		DACChn->Pos = 0x00;
	DACChn->PosFract = 0x00;
	
	if (DACChn->FreqForce)
	{
		FreqHz = DACChn->FreqForce;
		DACChn->DeltaFract = CalcDACDelta_Hz(FreqHz);
	}
	else
	{
		if (DACChn->RateForce)
			Rate = DACChn->RateForce;
		else if (TempEntry->OverriddenRate)
			Rate = TempEntry->OverriddenRate;
		else
			Rate = TempEntry->Rate;
		FreqHz = CalcDACFreq(Rate);	// for VGM logging
		DACChn->DeltaFract = CalcDACDelta_Rate(Rate);
	}
	
	SetDACState(0x80);	// also does WriteFMI(0x2B, 0x00);
	if (TempEntry->Pan)
		ym2612_fm_write(0x00, 0x01, 0xB6, TempEntry->Pan);
	return;
}

void DAC_SetRate(UINT8 Chn, UINT32 Rate, UINT8 MidNote)
{
	// Note: MidNote is here for optimization.
	//       setting it to 0 skips the rate recalculation, since that's done when playing the next sound anyway
	DAC_STATE* DACChn;
	UINT32 FreqHz;
	
	if (Chn >= MAX_DAC_CHNS || DACDrv == NULL)
		return;
	
	DACChn = &DACChnState[Chn];
	
	DACChn->RateForce = Rate;
	if (DACChn->FreqForce || DACChn->DACSmplPtr == NULL || ! MidNote)
		return;
	
	if (! Rate && DACChn->DACTblPtr != NULL)
	{
		if (DACChn->DACTblPtr->OverriddenRate)
			Rate = DACChn->DACTblPtr->OverriddenRate;
		else
			Rate = DACChn->DACTblPtr->Rate;
	}
	FreqHz = CalcDACFreq(Rate);
	DACChn->DeltaFract = CalcDACDelta_Rate(Rate);
	
	return;
}

void DAC_SetFrequency(UINT8 Chn, UINT32 Freq, UINT8 MidNote)
{
	// Note: MidNote is here for optimization. (see DAC_SetRate)
	DAC_STATE* DACChn;
	
	if (Chn >= MAX_DAC_CHNS || DACDrv == NULL)
		return;
	
	DACChn = &DACChnState[Chn];
	
	DACChn->FreqForce = Freq;
	if (DACChn->DACSmplPtr != NULL || ! MidNote)
		return;
	
	if (! Freq)
	{
		DAC_SetRate(Chn, DACChn->RateForce, MidNote);
	}
	else
	{
		DACChn->DeltaFract = CalcDACDelta_Hz(Freq);
	}
	
	return;
}
