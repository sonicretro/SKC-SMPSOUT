#include <stddef.h>	// for NULL
#include <stdio.h>
#include <memory.h>

#include "stdtype.h"
#include "../chips/mamedef.h"	// for offs_t
#include "smps_structs.h"
#include "smps_structs_int.h"
#include "smps.h"
#include "smps_int.h"
#include "../Sound.h"
#include "dac.h"

UINT8 ym2612_r(UINT8 ChipID, offs_t offset);
#define ReadFM()				ym2612_r(0x00, 0x00)
#define WriteFMI(Reg, Data)		ym2612_fm_write(0x00, 0x00, Reg, Data)
#define WriteFMII(Reg, Data)	ym2612_fm_write(0x00, 0x01, Reg, Data)
#define WritePSG(Data)			sn76496_psg_write(0x00, Data)

// from smps_extra.c
void StartSignal(void);
void StopSignal(void);
//void LoopStartSignal(void);
//void LoopEndSignal(void);
void Extra_LoopStartCheck(TRK_RAM* Trk);
//void Extra_LoopEndCheck(TRK_RAM* Trk);



// Function Prototypes
// -------------------
INLINE UINT16 ReadBE16(const UINT8* Data);
INLINE UINT16 ReadLE16(const UINT8* Data);
INLINE UINT16 ReadRawPtr(const UINT8* Data, const SMPS_CFG* SmpsCfg);
INLINE UINT16 ReadPtr(const UINT8* Data, const SMPS_CFG* SmpsCfg);
INLINE UINT16 ReadJumpPtr(const UINT8* Data, const UINT16 PtrPos, const SMPS_CFG* SmpsCfg);

// TODO: copy smps_int.h here


#define VALID_FREQ(x)	((x & 0xFF00) != 0x8000)

// Variables
// ---------
SND_RAM SmpsRAM;

//static const UINT8 VolOperators_DEF[] = {0x40, 0x48, 0x44, 0x4C, 0x00};
//static const UINT8 VolOperators_HW[] = {0x40, 0x44, 0x48, 0x4C, 0x00};
static const UINT8 AlgoOutMask[0x08] =
	{0x08, 0x08, 0x08, 0x08, 0x0C, 0x0E, 0x0E, 0x0F};
// Note: bit order is 40 44 48 4C (Bit 0 = 40, Bit 3 = 4C)
static const UINT8 OpList_DEF[] = {0x00, 0x08, 0x04, 0x0C};	// default SMPS operator order
static const UINT8 OpList_HW[]  = {0x00, 0x04, 0x08, 0x0C};	// hardware operator order


INLINE UINT16 ReadBE16(const UINT8* Data)
{
	return (Data[0x00] << 8) | (Data[0x01] << 0);
}

INLINE UINT16 ReadLE16(const UINT8* Data)
{
	return (Data[0x01] << 8) | (Data[0x00] << 0);
}

INLINE UINT16 ReadRawPtr(const UINT8* Data, const SMPS_CFG* SmpsCfg)
{
	if ((SmpsCfg->PtrFmt & PTRFMT_EMASK) == PTRFMT_BE)
		return ReadBE16(Data);
	else
		return ReadLE16(Data);
}

INLINE UINT16 ReadPtr(const UINT8* Data, const SMPS_CFG* SmpsCfg)
{
	return ReadRawPtr(Data, SmpsCfg) - SmpsCfg->SeqBase;
}

INLINE UINT16 ReadJumpPtr(const UINT8* Data, const UINT16 PtrPos, const SMPS_CFG* SmpsCfg)
{
	UINT16 PtrVal;
	UINT8 Offset;
	
	PtrVal = ReadRawPtr(Data, SmpsCfg);
	Offset = SmpsCfg->PtrFmt & PTRFMT_OFSMASK;
	if (! Offset)
	{
		// absolute
		return PtrVal - SmpsCfg->SeqBase;
	}
	else
	{
		// relative
		Offset --;
		return PtrPos + Offset + (INT16)PtrVal;
	}
}

void WriteFMMain(const TRK_RAM* Trk, UINT8 Reg, UINT8 Data)
{
	UINT8 FnlReg;
	
	if (Trk->ChannelMask & 0x80)
		return;	// PSG channel - return
	
	//WriteFMIorII:
	if (Trk->PlaybkFlags & PBKFLG_OVERRIDDEN)
		return;
	
	// The actual driver uses ADD instead of OR, but OR is always safe.
	FnlReg = Reg | (Trk->ChannelMask & 0x03);
	if (! (Trk->ChannelMask & 0x04))
		WriteFMI(FnlReg, Data);
	else
		WriteFMII(FnlReg, Data);
	
	return;
}



void InitDriver(void)
{
	memset(&SmpsRAM, 0x00, sizeof(SND_RAM));
	SmpsRAM.LockTimingMode = 0xFF;
	ym2612_timer_mask(0x00);
	StopAllSound();
	
	return;
}

static void ResetYMTimerA(void)
{
	WriteFMI(0x25, SmpsRAM.TimerAVal & 0x03);
	WriteFMI(0x24, SmpsRAM.TimerAVal >> 2);
	WriteFMI(0x27, SmpsRAM.SpcFM3Mode | 0x1F);
	
	return;
}

static void ResetYMTimerB(void)
{
	WriteFMI(0x26, SmpsRAM.TimerBVal);
	WriteFMI(0x27, SmpsRAM.SpcFM3Mode | 0x2F);
	
	return;
}


extern UINT32 PlayingTimer;
void UpdateAll(UINT8 Event)
{
	UINT8 TimerState;
	
	switch(Event)
	{
	case UPDATEEVT_VINT:
		if (SmpsRAM.TimingMode)
			break;
		
		UpdateMusic();
		UpdateSFX();
		break;
	case UPDATEEVT_TIMER:
		if (! SmpsRAM.TimingMode)
			break;
		
		TimerState = ReadFM();
		if (SmpsRAM.TimingMode & 0x80)
		{
			// The original driver updates SFX first, but it looks cleaner this way.
			if (TimerState & 0x01)
			{
				UpdateMusic();
				ResetYMTimerA();
			}
			if (TimerState & 0x02)
			{
				UpdateSFX();
				ResetYMTimerB();
			}
		}
		else if (SmpsRAM.TimingMode == 0x20)	// special Timing mode not present in the original SMPS Z80
		{
			if (TimerState & 0x01)
			{
				UpdateMusic();
				UpdateSFX();
				ResetYMTimerA();
			}
		}
		else	// Timing Mode 40
		{
			if (TimerState & 0x02)
			{
				UpdateMusic();
				UpdateSFX();
				ResetYMTimerB();
			}
		}
		break;
	}
	
	return;
}

void UpdateMusic(void)
{
	UINT8 CurTrk;
	
	if (PlayingTimer == -1)
		PlayingTimer = 0;
	if (SmpsRAM.MusCfg == NULL)
		return;
	DoPause();
	if (SmpsRAM.PauseMode)
		return;
	
	DoTempo();
	DoFadeOut();
	
	SmpsRAM.TrkMode = TRKMODE_MUSIC;
	for (CurTrk = 0; CurTrk < MUS_TRKCNT; CurTrk ++)
		UpdateTrack(&SmpsRAM.MusicTrks[CurTrk]);
	
	return;
}

void UpdateSFX(void)
{
	UINT8 CurTrk;
	
	SmpsRAM.TrkMode = TRKMODE_SFX;
	for (CurTrk = 0; CurTrk < SFX_TRKCNT; CurTrk ++)
		UpdateTrack(&SmpsRAM.SFXTrks[CurTrk]);
	
	SmpsRAM.TrkMode = TRKMODE_SPCSFX;
	for (CurTrk = 0; CurTrk < SPCSFX_TRKCNT; CurTrk ++)
		UpdateTrack(&SmpsRAM.SpcSFXTrks[CurTrk]);
	
	return;
}


INLINE void UpdateTrack(TRK_RAM* Trk)
{
	if (! (Trk->PlaybkFlags & PBKFLG_ACTIVE))
		return;
	
	if (Trk->ChannelMask & 0x80)
		UpdatePSGTrack(Trk);
	else if (Trk->ChannelMask & 0x10)
	{
		if (Trk->ChannelMask & 0x08)
			UpdatePWMTrack(Trk);
		else
			UpdateDrumTrack(Trk);
	}
	else
		UpdateFMTrack(Trk);
	
	return;
}

static void UpdateFMTrack(TRK_RAM* Trk)
{
	UINT16 Freq;
	UINT8 FreqUpdate;
	
	Trk->Timeout --;
	if (! Trk->Timeout)
	{
		TrkUpdate_Proc(Trk);
		if (! (Trk->PlaybkFlags & PBKFLG_ACTIVE))
			return;	// return after TRK_END command (the driver POPs some addresses from the stack instead)
		if (Trk->PlaybkFlags & PBKFLG_ATREST)
			return;
		
		PrepareModulat(Trk);
		if (Trk->SmpsCfg->DelayFreq == DLYFREQ_RESET && ! Trk->Frequency)
		{
			Trk->PlaybkFlags |= PBKFLG_ATREST;
			return;
		}
		if (! Trk->SmpsCfg->FMOctWrap)
			Freq = Trk->Frequency + Trk->Detune;
		else
			Freq = DoPitchSlide(Trk);
		
		if (Trk->SmpsCfg->ModAlgo == MODULAT_Z80)
			DoModulation(Trk, &Freq);
		
		SendFMFrequency(Trk, Freq);
		DoNoteOn(Trk);
	}
	else
	{
		DoPanAnimation(Trk, 1);
		ExecLFOModulation(Trk);
		if (Trk->PlaybkFlags & PBKFLG_ATREST)
			return;
		
		DoFMVolEnv(Trk);
		
		if (Trk->NStopTout)
		{
			Trk->NStopTout --;
			if (! Trk->NStopTout)
			{
				DoNoteOff(Trk);
				return;
			}
		}
		
		Freq = DoPitchSlide(Trk);
		if (Trk->PlaybkFlags & PBKFLG_LOCKFREQ)
			return;
		
		FreqUpdate = DoModulation(Trk, &Freq);
		if ((Trk->PlaybkFlags & PBKFLG_PITCHSLIDE) || FreqUpdate == 0x01)
			SendFMFrequency(Trk, Freq);
	}
	
	return;
}

static void UpdatePSGTrack(TRK_RAM* Trk)
{
	UINT16 Freq;
	UINT8 FreqUpdate;
	UINT8 WasNewNote;
	
	Trk->Timeout --;
	if (! Trk->Timeout)
	{
		TrkUpdate_Proc(Trk);
		if (! (Trk->PlaybkFlags & PBKFLG_ACTIVE))
			return;	// return after TRK_END command (the driver POPs some addresses from the stack instead)
		if (Trk->PlaybkFlags & PBKFLG_ATREST)
			return;
		
		PrepareModulat(Trk);
		if (Trk->SmpsCfg->DelayFreq == DLYFREQ_RESET && (Trk->Frequency & 0x8000))
		{
			Trk->PlaybkFlags |= PBKFLG_ATREST;
			return;
		}
		if (! Trk->SmpsCfg->FMOctWrap)
			Freq = Trk->Frequency + Trk->Detune;
		else
			Freq = DoPitchSlide(Trk);
		
		if (Trk->SmpsCfg->ModAlgo == MODULAT_Z80)
			FreqUpdate = DoModulation(Trk, &Freq);
		else
			FreqUpdate = 0x00;
		WasNewNote = 0x01;
	}
	else
	{
		// Note: Trying to return here causes wrong behaviour for unusual cases
		//       like 80 48 E7 0C (from Mega Man Wily Wars - 3E Introduction)
		//if (Trk->PlaybkFlags & PBKFLG_ATREST)
		//	return;
		
		if (Trk->NStopTout)
		{
			Trk->NStopTout --;
			if (! Trk->NStopTout)
			{
				//SetRest:
				Trk->PlaybkFlags |= PBKFLG_ATREST;
				DoNoteOff(Trk);
				return;
			}
		}
		Freq = DoPitchSlide(Trk);
		FreqUpdate = DoModulation(Trk, &Freq);
		WasNewNote = 0x00;
	}
	
	if (Trk->PlaybkFlags & PBKFLG_OVERRIDDEN)
		return;
	
	if (WasNewNote || (Trk->PlaybkFlags & PBKFLG_PITCHSLIDE) || FreqUpdate == 0x01)
		SendPSGFrequency(Trk, Freq);
	
	// Do PSG Volume:
	{
		UINT8 FinalVol;
		UINT8 EnvVol;
		
		FinalVol = Trk->Volume;
		if (Trk->Instrument)
		{
			EnvVol = DoVolumeEnvelope(Trk, Trk->Instrument);
			if (EnvVol & 0x80)
			{
				if (EnvVol == 0x81)	// SMPS Z80 does it for 0x80, too, but that breaks the Note Stop effect
					Trk->PlaybkFlags |= PBKFLG_ATREST;
				return;
			}
			FinalVol += EnvVol;
		}
		
		if (Trk->PlaybkFlags & PBKFLG_ATREST)
			return;
		
		if (FinalVol >= 0x10)
			FinalVol = 0x0F;
		FinalVol |= Trk->ChannelMask | 0x10;
		if (Trk->PlaybkFlags & PBKFLG_SPCMODE)
			FinalVol |= 0x20;
		WritePSG(FinalVol);
	}
	
	return;
}

static void UpdateDrumTrack(TRK_RAM* Trk)
{
	Trk->Timeout --;
	if (! Trk->Timeout)
	{
		const UINT8* Data = Trk->SmpsCfg->SeqData;
		UINT8 Note;
		
		if (Trk->Pos >= Trk->SmpsCfg->SeqLength)
		{
			Trk->PlaybkFlags &= ~PBKFLG_ACTIVE;
			return;
		}
		
		Trk->PlaybkFlags &= ~(PBKFLG_HOLD | PBKFLG_ATREST);
		
		while(Data[Trk->Pos] >= Trk->SmpsCfg->CmdList.FlagBase)
		{
			Extra_LoopStartCheck(Trk);
			cfHandler(Trk, Data[Trk->Pos]);
			if (! (Trk->PlaybkFlags & PBKFLG_ACTIVE))
				return;
		}
		
		if (Data[Trk->Pos] & 0x80)
		{
			Extra_LoopStartCheck(Trk);
			Trk->DAC.Snd = Data[Trk->Pos];
			Trk->Pos ++;
			
			if (Trk->SpcDacMode == DCHNMODE_GAXE3)
			{
				Trk->DAC.Unused = 0x00;
				if (Trk->GA3_DacMode & 0x01)
				{
					Trk->DAC.Unused = Data[Trk->Pos];
					Trk->Pos ++;
				}
				if (! (Trk->GA3_DacMode & 0x02))
					Trk->GA3_DacMode &= ~0x01;
			}
		}
		Note = Trk->DAC.Snd;
		if (! (Trk->PlaybkFlags & PBKFLG_OVERRIDDEN))
		{
			if (! (Trk->PlaybkFlags & PBKFLG_SPCMODE))
			{
				if (Trk->SpcDacMode == DCHNMODE_CYMN)
				{
					SmpsRAM.DacChVol[0x00] = 0x80 | (Trk->Volume & 0x0F);
					RefreshDACVolume(Trk, Trk->SpcDacMode, 0x00, SmpsRAM.DacChVol[0x00]);
					if (Trk->PlaybkFlags & PBKFLG_HOLD)
						Note = 0x00;	// not in the driver, but seems to be intended
				}
				
				if (Note >= 0x80)
					PlayDrumNote(Trk, Note);
				if (Trk->SpcDacMode == DCHNMODE_GAXE3 && Trk->DAC.Unused >= 0x80)
					PlayDrumNote(Trk, Trk->DAC.Unused);
			}
			else
			{
				PlayPS4DrumNote(Trk, Note);
			}
		}
		
		if (! (Data[Trk->Pos] & 0x80))
		{
			//SetDuration:
			Extra_LoopStartCheck(Trk);
			Trk->NoteLen = Data[Trk->Pos] * Trk->TickMult;
			Trk->Pos ++;
		}
		Trk->Timeout = Trk->NoteLen;
		// actually the driver sets more values, but these are never read in UpdateDrumTrack
		if (! (Trk->PlaybkFlags & PBKFLG_HOLD))
		{
			Trk->NStopTout = Trk->NStopInit;
		}
	}
	else
	{
		// Phantasy Star IV - special DAC handling
		if (Trk->NStopTout)
		{
			Trk->NStopTout --;
			if (! Trk->NStopTout)
			{
				if (Trk->PlaybkFlags & PBKFLG_SPCMODE)
				{
					Trk->PlaybkFlags |= PBKFLG_ATREST;
					DoNoteOff(Trk);
					DAC_Stop(0x00);
				}
				return;
			}
		}
	}
	
	return;
}

static void UpdatePWMTrack(TRK_RAM* Trk)
{
	Trk->Timeout --;
	if (! Trk->Timeout)
	{
		const UINT8* Data = Trk->SmpsCfg->SeqData;
		UINT8 Note;
		
		if (Trk->Pos >= Trk->SmpsCfg->SeqLength)
		{
			Trk->PlaybkFlags &= ~PBKFLG_ACTIVE;
			return;
		}
		
		Trk->PlaybkFlags &= ~(PBKFLG_HOLD | PBKFLG_ATREST);
		
		while(Data[Trk->Pos] >= Trk->SmpsCfg->CmdList.FlagBase)
		{
			Extra_LoopStartCheck(Trk);
			cfHandler(Trk, Data[Trk->Pos]);
			if (! (Trk->PlaybkFlags & PBKFLG_ACTIVE))
				return;
		}
		
		if (Data[Trk->Pos] & 0x80)
		{
			Extra_LoopStartCheck(Trk);
			Trk->DAC.Snd = Data[Trk->Pos];
			Trk->Pos ++;
		}
		if (! (Data[Trk->Pos] & 0x80))
		{
			//SetDuration:
			Extra_LoopStartCheck(Trk);
			Trk->NoteLen = Data[Trk->Pos] * Trk->TickMult;
			Trk->Pos ++;
		}
		
		Note = Trk->DAC.Snd;
		if (Note >= 0x81)
		{
			UINT8 VolValue;
			
			VolValue  = 1 + ((Trk->Volume & 0x0F) >> 0);
			VolValue += 1 + ((Trk->Volume & 0xF0) >> 4);
			DAC_SetVolume((Trk->ChannelMask & 0x06) >> 1, VolValue * 0x10);
			DAC_Play((Trk->ChannelMask & 0x06) >> 1, Note - 0x81);
		}
		
		Trk->Timeout = Trk->NoteLen;
		if (! (Trk->PlaybkFlags & PBKFLG_HOLD))
			Trk->NStopTout = Trk->NStopInit;
	}
	else
	{
		// not in the driver, but why not?
		if (Trk->NStopTout)
		{
			Trk->NStopTout --;
			if (! Trk->NStopTout)
			{
				Trk->PlaybkFlags |= PBKFLG_ATREST;
				DoNoteOff(Trk);
				return;
			}
		}
	}
	
	return;
}

static void SendFMFrequency(TRK_RAM* Trk, UINT16 Freq)
{
	if (Trk->PlaybkFlags & PBKFLG_OVERRIDDEN)
		return;
	
	if (! ((Trk->PlaybkFlags & PBKFLG_SPCMODE) && Trk->ChannelMask == 0x02))
	{
		WriteFMMain(Trk, 0xA4, Freq >> 8);
		WriteFMMain(Trk, 0xA0, Freq & 0xFF);
	}
	else
	{
		const UINT8 SpcFM3Regs[4] = {0xAD, 0xAE, 0xAC, 0xA6};
		UINT16* SpcFreqPtr;
		UINT16 FinalFrq;
		UINT8 CurFrq;
		
		SpcFreqPtr = GetFM3FreqPtr();
		for (CurFrq = 0; CurFrq < 4; CurFrq ++)
		{
			FinalFrq = Trk->Frequency + SpcFreqPtr[CurFrq];
			WriteFMI(SpcFM3Regs[CurFrq] - 0, FinalFrq >> 8);
			WriteFMI(SpcFM3Regs[CurFrq] - 4, FinalFrq & 0xFF);
		}
	}
	
	return;
}

INLINE UINT16* GetFM3FreqPtr(void)
{
	if (SmpsRAM.TrkMode == TRKMODE_MUSIC)
		return SmpsRAM.FM3Freqs_Mus;
	else if (SmpsRAM.TrkMode == TRKMODE_SPCSFX)
		return SmpsRAM.FM3Freqs_SpcSFX;
	else //if (SmpsRAM.TrkMode == TRKMODE_SSFX)
		return SmpsRAM.FM3Freqs_SFX;
}

static void SendPSGFrequency(TRK_RAM* Trk, UINT16 Freq)
{
	if (Trk->PlaybkFlags & PBKFLG_OVERRIDDEN)
		return;
	
	WritePSG(Trk->ChannelMask | (Freq & 0x0F));
	WritePSG((Freq >> 4) & 0x7F);
	
	return;
}

static void TrkUpdate_Proc(TRK_RAM* Trk)
{
	const UINT8* Data = Trk->SmpsCfg->SeqData;
	UINT8 Note;
	UINT8 ReuseDelay;
	
	if (Trk->Pos >= Trk->SmpsCfg->SeqLength)
	{
		Trk->PlaybkFlags &= ~PBKFLG_ACTIVE;
		return;
	}
	
	Trk->PlaybkFlags &= ~(PBKFLG_HOLD | PBKFLG_ATREST);
	
	while(Data[Trk->Pos] >= /*0xE0*/Trk->SmpsCfg->CmdList.FlagBase)
	{
		Extra_LoopStartCheck(Trk);
		cfHandler(Trk, Data[Trk->Pos]);
		if (! (Trk->PlaybkFlags & PBKFLG_ACTIVE))
			return;
	}
	
	DoNoteOff(Trk);
	DoPanAnimation(Trk, 0);
	InitLFOModulation(Trk);
	
	ReuseDelay = 0x00;
	if (! (Trk->PlaybkFlags & PBKFLG_RAWFREQ))
	{
		Note = Data[Trk->Pos];
		if (Note & 0x80)
		{
			Extra_LoopStartCheck(Trk);
			Trk->Pos ++;
			if (Note == 0x80)
			{
				Trk->PlaybkFlags |= PBKFLG_ATREST;	// inlined SetRest
				if (Trk->SmpsCfg->DelayFreq == DLYFREQ_RESET)
					Trk->Frequency = (Trk->ChannelMask & 0x80) ? 0xFFFF : 0x0000;
			}
			else
			{
				Trk->Frequency = GetNote(Trk, Note);
			}
			
			if (! (Trk->PlaybkFlags & PBKFLG_PITCHSLIDE))
			{
				if (Data[Trk->Pos] & 0x80)
					ReuseDelay = 0x01;
			}
			else
			{
				Trk->Detune = (INT8)Data[Trk->Pos];
				Trk->Pos ++;
			}
		}
	}
	else
	{
		//DoRawFreqMode:
		Extra_LoopStartCheck(Trk);
		Trk->Frequency = ReadLE16(&Data[Trk->Pos]);
		Trk->Pos += 2;
		if (Trk->Frequency)
			Trk->Frequency += Trk->Transpose;
	}
	
	if (! ReuseDelay)
	{
		//SetDuration:
		Extra_LoopStartCheck(Trk);
		Trk->NoteLen = Data[Trk->Pos] * Trk->TickMult;
		Trk->Pos ++;
	}
	
	Trk->Timeout = Trk->NoteLen;
	if (! (Trk->PlaybkFlags & PBKFLG_HOLD) || (Trk->NStopRevMode & 0x80))	// Mode 0x80 == execute always
	{
		if (! Trk->NStopRevMode)
		{
			Trk->NStopTout = Trk->NStopInit;
			// Note: A few earlier SMPS Z80 driver have a bug and do
			//Trk->VolEnvIdx = Trk->NStopInit;
			// right here.
		}
		else
		{
			INT16 NewTout;
			
			NewTout = (INT16)Trk->NoteLen - Trk->NStopInit;
			switch(Trk->NStopRevMode & 0x0F)
			{
			case 0x01:
				if (NewTout <= 0)
					NewTout = 1;	// Chou Yakyuu Miracle Nine - make a lower limit of 1
				break;
			case 0x02:
				if (Data[Trk->Pos] == 0xE7)
					NewTout = 0;	// Ristar - the E7 flag disables the effect temporarily.
				break;
			}
			Trk->NStopTout = (UINT8)NewTout;
		}
	}
	if (! (Trk->PlaybkFlags & PBKFLG_HOLD))
	{
		Trk->ModEnvIdx = 0x00;
		Trk->ModEnvMult = 0x00;
		Trk->ModEnvCache = 0;
		Trk->VolEnvIdx = 0x00;
		Trk->VolEnvCache = 0x00;
	}
	
	return;
}

static UINT16 GetNote(TRK_RAM* Trk, UINT8 NoteCmd)
{
	INT16 Note;
	UINT8 Octave;
	
	Note = NoteCmd - 0x81;
	Note += Trk->Transpose;
	if (Trk->ChannelMask & 0x80)
	{
		if (Note < 0)
			Note = 0;
		else if (Note >= Trk->SmpsCfg->PSGFreqCnt)
			Note = Trk->SmpsCfg->PSGFreqCnt - 1;
		return Trk->SmpsCfg->PSGFreqs[Note];
	}
	else //if (! (Trk->ChannelMask & 0x70))
	{
		// SMPS 68k drivers do (NoteCmd - 0x80) instead, so add 1 again.
		if (Trk->SmpsCfg->FMBaseNote == FMBASEN_B)
			Note ++;
		
		if (Trk->SmpsCfg->FMFreqCnt == 12)
		{
			Note &= 0xFF;	// simulate SMPS Z80 behaviour
			Octave = Trk->SmpsCfg->FMBaseOct + Note / 12;
			Note %= 12;
			return Trk->SmpsCfg->FMFreqs[Note] | (Octave << 11);
		}
		else
		{
			if (Note < 0)
				Note = 0;
			else if (Note >= Trk->SmpsCfg->FMFreqCnt)
				Note = Trk->SmpsCfg->FMFreqCnt - 1;
			return Trk->SmpsCfg->FMFreqs[Note];
		}
	}
}


static void DoPanAnimation(TRK_RAM* Trk, UINT8 Continue)
{
	const PAN_ANI_LIB* PAniLib = &Trk->SmpsCfg->PanAnims;
	UINT16 DataPtr;
	UINT8 PanData;
	
	// Continue == 0 - Start Pan Animation
	// Continue == 1 - Execute Pan Animation
	if (! Trk->PanAni.Type)
		return;	// Pan Animation disabled
	if (Trk->PanAni.Anim >= PAniLib->AniCount)
		return;
	
	// StartPanAnim:
	//	1	(return if Hold-Bit set), DoPanAnim
	//	2	Reset Index, DoPanAnim
	//	3	Reset Index, DoPanAnim
	// ExecutePanAnim:
	//	1	-- (return)
	//	2	DoPanAnim
	//	3	DoPanAnim
	if (! Continue)
	{
		//StartPanAnim:
		if (Trk->PanAni.Type == 0x01)
		{
			if (Trk->PlaybkFlags & PBKFLG_HOLD)
				return;
		}
		else //if (Trk->PanAni.Type >= 0x02)
		{
			Trk->PanAni.AniIdx = 0x00;
			if (Trk->SmpsCfg->ModAlgo == MODULAT_68K)
				Trk->PanAni.Timeout = 1;
		}
	}
	else
	{
		//ExecPanAnim:
		if (Trk->PanAni.Type == 0x01)
			return;
	}
	
	Trk->PanAni.Timeout --;
	if (Trk->PanAni.Timeout)
		return;
	
	Trk->PanAni.Timeout = Trk->PanAni.ToutInit;
	
	//PanData = AniData[Trk->PanAni.AniIdx];
	DataPtr = PAniLib->AniList[Trk->PanAni.Anim] - PAniLib->AniBase;
	if (DataPtr >= PAniLib->DataLen)
		return;
	DataPtr += Trk->PanAni.AniIdx;	// add Animation Index
	if (DataPtr >= PAniLib->DataLen)
		DataPtr = PAniLib->DataLen - 1;	// prevent reading beyond EOF
	PanData = PAniLib->Data[DataPtr];
	
	Trk->PanAni.AniIdx ++;
	if (Trk->PanAni.AniIdx == Trk->PanAni.AniLen)
	{
		if (Trk->PanAni.Type == 2)
			Trk->PanAni.AniIdx --;
		else
			Trk->PanAni.AniIdx = 0;
	}
	
	// inlined cfE0_Pan:
	Trk->PanAFMS &= 0x3F;
	Trk->PanAFMS |= PanData;
	WriteFMMain(Trk, 0xB4, Trk->PanAFMS);
	
	return;
}

static void InitLFOModulation(TRK_RAM* Trk)
{
	LFO_MOD* LFOMod = &Trk->LFOMod;
	
	if (Trk->PlaybkFlags & (PBKFLG_HOLD | PBKFLG_OVERRIDDEN))
		return;
	if (SmpsRAM.FadeOut.Steps)
		return;
	if (! Trk->LFOMod.MaxFMS)
		return;
	
	LFOMod->Delay = LFOMod->DelayInit;
	LFOMod->Timeout = LFOMod->ToutInit;
	LFOMod->CurFMS = Trk->PanAFMS & 0xF8;
	WriteFMMain(Trk, 0xB4, LFOMod->CurFMS);
	
	return;
}

static void ExecLFOModulation(TRK_RAM* Trk)
{
	LFO_MOD* LFOMod = &Trk->LFOMod;
	
	if (SmpsRAM.FadeOut.Steps)
		return;
	if (! LFOMod->MaxFMS)
		return;
	
	LFOMod->Delay --;
	if (LFOMod->Delay)
		return;
	LFOMod->Delay = 1;
	
	if ((LFOMod->CurFMS & 0x07) == LFOMod->MaxFMS)
		return;
	
	LFOMod->Timeout --;
	if (LFOMod->Timeout)
		return;
	LFOMod->Timeout = LFOMod->ToutInit;
	
	LFOMod->CurFMS ++;
	WriteFMMain(Trk, 0xB4, LFOMod->CurFMS);
	
	return;
}

static void DoFMVolEnv(TRK_RAM* Trk)
{
	UINT8 EnvVol;
	//const UINT8* OpPtr = (Trk->SmpsCfg->InsMode & 0x01) ? VolOperators_HW : VolOperators_DEF;
	const UINT8* OpPtr = Trk->SmpsCfg->InsReg_TL;
	const UINT8* VolPtr = Trk->VolOpPtr;
	UINT8 AlgoMask;
	UINT8 CurOp;
	UINT8 CurTL;
	
	if (! Trk->FMVolEnv.VolEnv || (Trk->FMVolEnv.VolEnv & 0x80))	// VolEnv 80+ is SSG-EG
		return;
	
	EnvVol = DoVolumeEnvelope(Trk, Trk->FMVolEnv.VolEnv);
	if (EnvVol & 0x80)
		return;
	if (OpPtr == NULL || VolPtr == NULL)
		return;
	
	AlgoMask = Trk->FMVolEnv.OpMask;
	for (CurOp = 0x00; CurOp < 0x04; CurOp ++, AlgoMask >>= 1)
	{
		if (AlgoMask & 0x01)
		{
			CurTL = VolPtr[CurOp] + EnvVol;
			CurTL &= 0x7F;
			WriteFMMain(Trk, OpPtr[CurOp], CurTL);
		}
	}
	
	return;
}

static void PrepareModulat(TRK_RAM* Trk)
{
	const UINT8* Data = Trk->SmpsCfg->SeqData;
	const UINT8* ModData;
	
	if (Trk->PlaybkFlags & PBKFLG_HOLD)
		return;
	if (! (Trk->ModEnv & 0x80))
	{
		Trk->CstMod.Freq = 0;
		return;	// no Custom Modulation
	}
	
	ModData = &Data[Trk->CstMod.DataPtr];
	Trk->CstMod.Delay = ModData[0x00];
	Trk->CstMod.Rate = ModData[0x01];
	Trk->CstMod.Delta = ModData[0x02];
	Trk->CstMod.RemSteps = ModData[0x03] / 2;
	Trk->CstMod.Freq = 0;
	
	return;
}

static UINT8 DoModulation(TRK_RAM* Trk, UINT16* Freq)
{
	INT16 EnvFreq;
	INT16 CstFreq;
	INT16 FreqChange;
	UINT8 Executed;
	
	if (! Trk->ModEnv)
		return 0x00;
	if (Trk->ModEnv > 0x80 && Trk->SmpsCfg->ModAlgo == MODULAT_Z80)
	{
		printf("Warning: Modulation Type %02X on Channel %02X, Pos 0x%04X!\n", Trk->ModEnv, Trk->ChannelMask, Trk->Pos);
		Trk->ModEnv &= 0x80;
	}
	
	// Note: SMPS 68k can do Modulation Envelope and Custom Modulation simultaneously.
	//       SMPS Z80 can only do one of them at the same time, since they share some memory.
	
	EnvFreq = DoModulatEnvelope(Trk, Trk->ModEnv & 0x7F);
	CstFreq = DoCustomModulation(Trk);
	
	Executed = 0x00;
	FreqChange = 0;
	if (VALID_FREQ(EnvFreq))
	{
		Executed |= 0x01;
		FreqChange += EnvFreq;
	}
	else if ((EnvFreq & 0xFF) == 0x01)
	{
		Executed |= 0x10;
		FreqChange += Trk->ModEnvCache;	// hold at current level
	}
	if (VALID_FREQ(CstFreq))
	{
		Executed |= 0x02;
		FreqChange += CstFreq;
	}
	else if ((CstFreq & 0xFF) == 0x01)
	{
		Executed |= 0x20;
		FreqChange += Trk->CstMod.Freq;	// hold at current level
	}
	
	*Freq += FreqChange;
	if (Executed & 0x0F)
		return 0x01;	// has updated
	else if (Executed & 0xF0)
		return 0x02;	// in effect, but not changed
	else
		return 0x00;	// nothing done
}

static INT16 DoCustomModulation(TRK_RAM* Trk)
{
	const UINT8* Data = Trk->SmpsCfg->SeqData;
	const UINT8* ModData = &Data[Trk->CstMod.DataPtr];
	
	if (! (Trk->ModEnv & 0x80))
		return 0x8000;
	
	if (Trk->SmpsCfg->ModAlgo == MODULAT_68K)
	{
		if (Trk->CstMod.Delay)
		{
			Trk->CstMod.Delay --;
			return 0x8001;
		}
		
		Trk->CstMod.Rate --;
		if (Trk->CstMod.Rate)
			return 0x8001;
		Trk->CstMod.Rate = ModData[0x01];
		
		if (! Trk->CstMod.RemSteps)
		{
			Trk->CstMod.RemSteps = ModData[0x03];
			Trk->CstMod.Delta = -Trk->CstMod.Delta;
			return 0x8001;
		}
		
		Trk->CstMod.RemSteps --;
		Trk->CstMod.Freq += Trk->CstMod.Delta;
		return Trk->CstMod.Freq;
	}
	else if (Trk->SmpsCfg->ModAlgo == MODULAT_Z80)
	{
		INT16 NewFreq;
		
		Trk->CstMod.Delay --;
		if (Trk->CstMod.Delay)
			return 0x8001;
		Trk->CstMod.Delay ++;
		
		//NewFreq = 0x8000;
		NewFreq = Trk->CstMod.Freq;
		
		Trk->CstMod.Rate --;
		if (! Trk->CstMod.Rate)
		{
			Trk->CstMod.Rate = ModData[0x01];
			Trk->CstMod.Freq += Trk->CstMod.Delta;
			NewFreq = Trk->CstMod.Freq;
		}
		
		Trk->CstMod.RemSteps --;
		if (! Trk->CstMod.RemSteps)
		{
			Trk->CstMod.RemSteps = ModData[0x03];
			Trk->CstMod.Delta = -Trk->CstMod.Delta;
		}
		return NewFreq;
	}
	
	return 0x80FF;
}

static INT16 DoModulatEnvelope(TRK_RAM* Trk, UINT8 EnvID)
{
	const ENV_LIB* ModEnvLib = &Trk->SmpsCfg->ModEnvs;
	INT8 EnvVal;
	INT16 Multiplier;
	
	if (! EnvID)
		return 0x8000;
	EnvID --;
	if (EnvID >= ModEnvLib->EnvCount)
		return 0x8000;
	
	EnvVal = (INT8)DoEnvelope(&ModEnvLib->EnvData[EnvID], Trk->SmpsCfg->EnvCmds,
								&Trk->ModEnvIdx, &Trk->ModEnvMult);
	if (EnvVal & 0x80)
	{
		switch(Trk->SmpsCfg->EnvCmds[EnvVal & 0x7F])
		{
		case ENVCMD_HOLD:	// 81 - hold at current level
			return 0x8001;
		case ENVCMD_STOP:	// 83 - stop
			DoNoteOff(Trk);
			return 0x80FF;
		}
	}
	
	// preSMPS 68k formula: (Rent-A-Hero)
	//	if (EnvMult != 0)
	//		FreqDelta = (INT8)EnvVal * (UINT8)EnvMult
	//	else
	//		FreqDelta = (INT8)EnvVal
	//	Note: Whether EnvMult is signed or unsigned depends on how the register D3 was set before the
	//	      sound driver was called. Usually it is set to 0 though.
	//	      In Phantasy Star II, a multiplication with 0 is possible, EnvMult is ensured to be unsigned
	//	      and an invalid sign extention causes negative EnvVal to be always -1.
	
	// SMPS 68k formula:
	//	FreqDelta = (INT8)EnvVal * (INT8)EnvMult
	//	Note: EnvMult is 0 by default, nullifying the envelope until the first Change Multiplier command.
	
	// SMPS Z80 formula: (also preSMPS Z80, Space Harrier II)
	//	FreqDelta = (INT8)EnvVal * (UNT8)(EnvMult + 1)
	
	switch(Trk->SmpsCfg->EnvMult)
	{
	case ENVMULT_PRE:
		Multiplier = Trk->ModEnvMult ? Trk->ModEnvMult : 1;
		break;
	case ENVMULT_68K:
		Multiplier = (INT8)Trk->ModEnvMult;
		break;
	case ENVMULT_Z80:
		Multiplier = Trk->ModEnvMult + 1;
		break;
	}
	
	Trk->ModEnvCache = EnvVal * Multiplier;
	return Trk->ModEnvCache;
}

static UINT8 DoVolumeEnvelope(TRK_RAM* Trk, UINT8 EnvID)
{
	const ENV_LIB* VolEnvLib = &Trk->SmpsCfg->VolEnvs;
	UINT8 EnvVal;
	
	if (! EnvID)
		return 0x80;
	EnvID --;
	if (EnvID >= VolEnvLib->EnvCount)
		return 0x80;
	
	EnvVal = DoEnvelope(&VolEnvLib->EnvData[EnvID], Trk->SmpsCfg->EnvCmds, &Trk->VolEnvIdx, NULL);
	if (EnvVal & 0x80)
	{
		switch(Trk->SmpsCfg->EnvCmds[EnvVal & 0x7F])
		{
		case ENVCMD_HOLD:	// 81 - hold at current level
			return 0x80;
		case ENVCMD_STOP:	// 83 - stop
			DoNoteOff(Trk);
			return 0x81;
		}
	}
	
	Trk->VolEnvCache = EnvVal;
	return Trk->VolEnvCache;
}

static UINT8 DoEnvelope(const ENV_DATA* EnvData, const UINT8* EnvCmds, UINT8* EnvIdx, UINT8* EnvMult)
{
	UINT8 Data;
	UINT8 Finished;
	
	if (*EnvIdx >= EnvData->Len)
	{
		printf("Warning: invalid Envelope Index 0x%02X (Env. Length %02X)\n", *EnvIdx, EnvData->Len);
		*EnvIdx = 0x00;
	}
	
	Finished = 0x00;
	do
	{
		Data = EnvData->Data[*EnvIdx];
		(*EnvIdx) ++;
		if (Data < 0x80)
			break;
		
		switch(EnvCmds[Data & 0x7F])
		{
		case ENVCMD_DATA:
			Finished = 0x01;
			break;
		case ENVCMD_RESET:	// 80 - reset Envelope
			*EnvIdx = 0x00;
			break;
		case ENVCMD_HOLD:	// 81 - hold at current level
			(*EnvIdx) --;
			Finished = 0x01;
			break;
		case ENVCMD_LOOP:	// 82 xx - loop back to index xx
			*EnvIdx = EnvData->Data[*EnvIdx];
			break;
		case ENVCMD_STOP:	// 83 - stop
			(*EnvIdx) --;
			Finished = 0x01;
			break;
		case ENVCMD_CHGMULT:	// 84 xx - change Multiplier
			if (EnvMult != NULL)
				*EnvMult += EnvData->Data[*EnvIdx];
			(*EnvIdx) ++;
			break;
		}
	} while(! Finished);
	
	return Data;
}


void DoNoteOn(TRK_RAM* Trk)
{
	UINT8 Flags;
	
	if (! Trk->Frequency)
		return;
	
	// SMPS 68k and Z80 both return when bit 1 or 2 is set. (AND 06h)
	// But for SMPS Z80, bit 1 is PBKFLG_HOLD, for SMPS 68k it is PBKFLG_ATREST.
	Flags = PBKFLG_OVERRIDDEN;
	if (Trk->SmpsCfg->NoteOnPrevent == NONPREV_HOLD)
		Flags |= PBKFLG_HOLD;	// SMPS Z80
	else //if (Trk->SmpsCfg->NoteOnPrevent == NONPREV_REST)
		Flags |= PBKFLG_ATREST;	// SMPS 68k
	if (Trk->PlaybkFlags & Flags)
		return;
	
	if (Trk->ChannelMask & 0x80)	// not in SMPS Z80 code
		return;
	
	// [not in driver] turn DAC off when playing a note on FM6
	if (Trk->SmpsCfg->FM6DACOff && Trk->ChannelMask == 0x06 && (SmpsRAM.DacState & 0x80))
		SetDACState(0x00);
	
	WriteFMI(0x28, Trk->ChannelMask | 0xF0);
	
	return;
}

void DoNoteOff(TRK_RAM* Trk)
{
	if (Trk->PlaybkFlags & (PBKFLG_HOLD | PBKFLG_OVERRIDDEN))
		return;
	if (Trk->ChannelMask & 0x10)	// skip Drum Tracks
		return;
	
	if (Trk->ChannelMask & 0x80)
	{
		WritePSG(Trk->ChannelMask | 0x10 | 0x0F);
		if (Trk->PlaybkFlags & PBKFLG_SPCMODE)
			WritePSG(Trk->ChannelMask | 0x30 | 0x0F);	// mute Noise Channel
	}
	else
	{
		WriteFMI(0x28, Trk->ChannelMask);
	}
	
	return;
}

static UINT16 DoPitchSlide(TRK_RAM* Trk)
{
	UINT16 NewFreq;
	
	NewFreq = Trk->Frequency + Trk->Detune;
	// Note: SMPS Z80 does the pitch wrap even if no Pitch Slide is active,
	//       but this breaks SMPS 68k songs.
	if (! (Trk->ChannelMask & 0x80))	// FM channels only
	{
		// make it slide smoothly through the octaves
		UINT16 BaseFreq;
		UINT16 OctFreq;	// frequency within octave
		
		BaseFreq = Trk->SmpsCfg->FMFreqs[0];
		OctFreq = NewFreq & 0x7FF;
		if (OctFreq < BaseFreq)
			NewFreq -= (0x7FF - BaseFreq);
		else if (OctFreq > BaseFreq * 2)
			NewFreq += (0x800 - BaseFreq);
		
		/* Original formula:
	SMPS Z80 (Type 1/2):
		if (OctFreq <= 0x283)		// 0x284-1
			NewFreq -= 0x57B;		// 0x800-1 - 0x284
		else if (OctFreq > 0x508)	// 0x284*2
			NewFreq += 0x57C;		// 0x800 - 0x284
		// A few game have different numbers:
	Ghostbusters:
		if (OctFreq <= 0x284)
			NewFreq -= 0x57C;
		else if (OctFreq > 0x508)
			NewFreq += 0x57C;
	Arnold Palmer Tournament Golf (early SMPS):
		if (OctFreq <= 0x27E)
			NewFreq -= 0x580;
		else if (OctFreq > 0x4FE)
			NewFreq += 0x580;
	*/
	}
	
	if (Trk->PlaybkFlags & PBKFLG_PITCHSLIDE)
		Trk->Frequency = NewFreq;
	
	return NewFreq;
}

void SendFMIns(TRK_RAM* Trk, const UINT8* InsData)
{
	const UINT8* OpPtr = Trk->SmpsCfg->InsRegs;
	const UINT8* InsPtr = InsData;
	UINT8 HadB4;
	
	if (OpPtr == NULL || InsData == NULL)
		return;
	
	HadB4 = 0x00;
	while(*OpPtr)
	{
		if (*OpPtr == 0xB0)
			Trk->FMAlgo = *InsPtr;
		else if (*OpPtr == 0xB4)
		{
			Trk->PanAFMS = *InsPtr;
			HadB4 = 0x01;
		}
		else if (*OpPtr == 0x40)
			Trk->VolOpPtr = InsPtr;
		
		if ((*OpPtr & 0xF0) != 0x40)	// exclude the TL operators - RefreshVolume will do them
		{
			//WriteInsReg:
			WriteFMMain(Trk, *OpPtr, *InsPtr);
		}
		*OpPtr ++;	*InsPtr ++;
	}
	if (! HadB4)	// if it was in the list already, skip it
		WriteFMMain(Trk, 0xB4, Trk->PanAFMS);
	RefreshVolume(Trk);
	
	return;
}

void RefreshVolume(TRK_RAM* Trk)
{
	//const UINT8* OpPtr = (Trk->SmpsCfg->InsMode & 0x01) ? VolOperators_HW : VolOperators_DEF;
	const UINT8* OpPtr = Trk->SmpsCfg->InsReg_TL;
	const UINT8* VolPtr = Trk->VolOpPtr;
	UINT8 AlgoMask;
	UINT8 CurOp;
	UINT8 CurTL;
	UINT8 IsOutputOp;
	
	if (OpPtr == NULL || VolPtr == NULL)
		return;
	if (Trk->ChannelMask & 0x10)
		return;	// don't refresh on DAC/Drum tracks
	
	AlgoMask = AlgoOutMask[Trk->FMAlgo & 0x07];
	for (CurOp = 0x00; CurOp < 0x04; CurOp ++)
	{
		CurTL = VolPtr[CurOp];
		
		if (Trk->SmpsCfg->VolMode & VOLMODE_BIT7)
		{
			IsOutputOp = (CurTL & 0x80);
		}
		else // VOLMODE_ALGO
		{
			IsOutputOp = (OpPtr[CurOp] & 0x0C) >> 2;	// 40/44/48/4C -> Bit 0/1/2/3
			IsOutputOp = AlgoMask & (1 << IsOutputOp);
		}
		if (IsOutputOp)
		{
			if (Trk->SmpsCfg->VolMode & VOLMODE_SETVOL)
				CurTL = Trk->Volume;
			else
				CurTL += Trk->Volume;
		}
		CurTL &= 0x7F;
		
		WriteFMMain(Trk, OpPtr[CurOp], CurTL);
	}
	
	return;
}

void SendSSGEG(TRK_RAM* Trk, const UINT8* Data, UINT8 ForceMaxAtk)
{
	const UINT8* OpPtr;
	UINT8 CurOp;
	
	if ((Trk->SmpsCfg->InsMode & 0x01) == INSMODE_DEF)
		OpPtr = OpList_DEF;
	else //if ((Trk->SmpsCfg->InsMode & 0x01) == INSMODE_HW)
		OpPtr = OpList_HW;
	
	for (CurOp = 0x00; CurOp < 0x04; CurOp ++)
	{
		WriteFMMain(Trk, 0x90 | OpPtr[CurOp], Data[CurOp]);
		if (ForceMaxAtk)
			WriteFMMain(Trk, 0x50 | OpPtr[CurOp], 0x1F);
	}
	
	return;
}



//static const UINT8 FMChnOrder[7] = {0x16, 0x00, 0x01, 0x02, 0x04, 0x05, 0x06};
//static const UINT8 FMChnOrder[7] = {0x12, 0x00, 0x01, 0x04, 0x05, 0x06, 0x02};
static const UINT8 PSGChnOrder[3] = {0x80, 0xA0, 0xC0};

void PlayMusic(SMPS_CFG* SmpsFileConfig)
{
	UINT8 FMChnCount;
	UINT8* FMChnOrder;
	const SMPS_CFG_INIT* InitCfg;
	const UINT8* Data;
	UINT16 CurPos;
	UINT8 FMChnCnt;
	UINT8 PSGChnCnt;
	UINT8 TickMult;
	UINT8 TrkBase;
	UINT8 CurTrk;
	UINT8 TrkID;
	TRK_RAM* TempTrk;
	
	//StopAllSound();	// in the driver, but I can do that in a better way
	
	StopSignal();
	for (CurTrk = 0; CurTrk < MUS_TRKCNT; CurTrk ++)
	{
		TempTrk = &SmpsRAM.MusicTrks[CurTrk];
		TempTrk->PlaybkFlags &= ~PBKFLG_HOLD;
		DoNoteOff(TempTrk);
		DisableSSGEG(TempTrk);
		TempTrk->PlaybkFlags = 0x00;
		TempTrk->SmpsCfg = NULL;
	}
	ResetSpcFM3Mode();
	SmpsRAM.FadeOut.Steps = 0x00;
	
	InitCfg = &SmpsFileConfig->InitCfg;
	if (SmpsRAM.LockTimingMode == 0xFF)
	{
		SmpsRAM.LockTimingMode = InitCfg->Timing_Lock;
		SmpsRAM.TimingMode = InitCfg->Timing_DefMode;
		SmpsRAM.TimerAVal = InitCfg->Timing_TimerA;
		SmpsRAM.TimerBVal = InitCfg->Timing_TimerB;
	}
	else
	{
		SmpsRAM.LockTimingMode = InitCfg->Timing_Lock;
		if (InitCfg->Timing_DefMode != 0xFF)
			SmpsRAM.TimingMode = InitCfg->Timing_DefMode;
		if (InitCfg->Timing_TimerA)
			SmpsRAM.TimerAVal = InitCfg->Timing_TimerA;
		if (InitCfg->Timing_TimerB)
			SmpsRAM.TimerBVal = InitCfg->Timing_TimerB;
	}
	if (SmpsRAM.TimingMode == 0x00)
		ym2612_timer_mask(0x00);	// no YM2612 Timer
	else if (SmpsRAM.TimingMode == 0x20)
		ym2612_timer_mask(0x01);	// YM2612 Timer A
	else if (SmpsRAM.TimingMode == 0x40)
		ym2612_timer_mask(0x02);	// YM2612 Timer B
	else //if (SmpsRAM.TimingMode == 0x80)
		ym2612_timer_mask(0x03);	// YM2612 Timer A and B
	SetDACDriver(&SmpsFileConfig->DACDrv);
	DAC_ResetOverride();
	
	SmpsRAM.MusCfg = SmpsFileConfig;
	Data = SmpsFileConfig->SeqData;
	CurPos = 0x00;
	
//	for (CurTrk = 0; CurTrk < 6; CurTrk ++)
//		FMChnOrder[1 + CurTrk] = SmpsFileConfig->FMChnList[CurTrk];
//	FMChnOrder[0] = 0x10 | FMChnOrder[6];
	FMChnCount = SmpsFileConfig->FMChnCnt;
	FMChnOrder = SmpsFileConfig->FMChnList;
	
	//InsLibPtr = ReadPtr(&Data[CurPos + 0x00], SmpsFileConfig);
	FMChnCnt = Data[CurPos + 0x02];
	PSGChnCnt = Data[CurPos + 0x03];
	if (FMChnCnt > 7 || PSGChnCnt > 3)
		return;	// invalid file
	TickMult = Data[CurPos + 0x04];
	SmpsRAM.TempoCntr = Data[CurPos + 0x05];
	SmpsRAM.TempoInit = Data[CurPos + 0x05];
	if (SmpsFileConfig->TempoMode == TEMPO_TIMEOUT)
		SmpsRAM.TempoCntr ++;	// DoTempo is called before PlayMusic, so simulate that behaviour
	CurPos += 0x06;
	
	StartSignal();
	
	TrkBase = 0x00;
	TrkID = TRACK_MUS_DRUM;
	for (CurTrk = 0; CurTrk < FMChnCnt; CurTrk ++, CurPos += 0x04, TrkID ++)
	{
		// skip Drum tracks unless Channel Bits are 0x10-0x17
		while(TrkID < TRACK_MUS_FM1 && ! (FMChnOrder[TrkID] & 0x10))
			TrkID ++;
		if (TrkID >= TRACK_MUS_PSG1 || CurTrk >= FMChnCnt)
			continue;
		
		TempTrk = &SmpsRAM.MusicTrks[TrkID];
		memset(TempTrk, 0x00, sizeof(TRK_RAM));
		TempTrk->SmpsCfg = SmpsRAM.MusCfg;
		TempTrk->PlaybkFlags = PBKFLG_ACTIVE;
		TempTrk->ChannelMask = FMChnOrder[CurTrk];
		TempTrk->TickMult = TickMult;
		TempTrk->Pos = ReadPtr(&Data[CurPos + 0x00], SmpsFileConfig);
		TempTrk->Transpose = Data[CurPos + 0x02];
		TempTrk->Volume = Data[CurPos + 0x03];
		//FinishFMTrkInit:
		TempTrk->ModEnv = 0x00;
		TempTrk->Instrument = 0x00;
		//FinishTrkInit:
		TempTrk->StackPtr = TRK_STACK_SIZE;
		TempTrk->PanAFMS = 0xC0;
		TempTrk->Timeout = 0x01;
		if (SmpsFileConfig->LoopPtrs != NULL)
			TempTrk->LoopOfs = SmpsFileConfig->LoopPtrs[TrkBase + CurTrk];
		else
			TempTrk->LoopOfs = 0x0000;
		
		if ((TempTrk->ChannelMask & 0xF8) == 0x10)
			TempTrk->SpcDacMode = SmpsFileConfig->DrumChnMode;
		
		if (TrkID == TRACK_MUS_DRUM)
			WriteFMMain(TempTrk, 0xB4, 0xC0);	// force Pan bits to LR
		
		if (TempTrk->Pos >= SmpsFileConfig->SeqLength)
			TempTrk->PlaybkFlags &= ~PBKFLG_ACTIVE;
	}
	TrkBase += CurTrk;
	
	TrkID = TRACK_MUS_PSG1;
	for (CurTrk = 0; CurTrk < PSGChnCnt; CurTrk ++, CurPos += 0x06, TrkID ++)
	{
		if (TrkID >= TRACK_MUS_PWM1 || CurTrk >= 0x03)
			continue;
		
		TempTrk = &SmpsRAM.MusicTrks[TrkID];
		memset(TempTrk, 0x00, sizeof(TRK_RAM));
		TempTrk->SmpsCfg = SmpsRAM.MusCfg;
		TempTrk->PlaybkFlags = 0x80;
		TempTrk->ChannelMask = PSGChnOrder[CurTrk];
		TempTrk->TickMult = TickMult;
		TempTrk->Pos = ReadPtr(&Data[CurPos + 0x00], SmpsFileConfig);
		TempTrk->Transpose = Data[CurPos + 0x02];
		TempTrk->Volume = Data[CurPos + 0x03];
		TempTrk->ModEnv = Data[CurPos + 0x04];
		TempTrk->Instrument = Data[CurPos + 0x05];
		//FinishTrkInit:
		TempTrk->StackPtr = TRK_STACK_SIZE;
		TempTrk->PanAFMS = 0xC0;
		TempTrk->Timeout = 0x01;
		if (SmpsFileConfig->LoopPtrs != NULL)
			TempTrk->LoopOfs = SmpsFileConfig->LoopPtrs[TrkBase + CurTrk];
		else
			TempTrk->LoopOfs = 0x0000;
		
		if (TempTrk->Pos >= SmpsFileConfig->SeqLength)
			TempTrk->PlaybkFlags &= ~PBKFLG_ACTIVE;
	}
	TrkBase += CurTrk;
	
	TrkID = TRACK_MUS_PWM1;
	for (CurTrk = 0; CurTrk < SmpsFileConfig->AddChnCnt; CurTrk ++, CurPos += 0x04, TrkID ++)
	{
		if (TrkID >= MUS_TRKCNT || CurTrk >= SmpsFileConfig->AddChnCnt)
			continue;
		
		TempTrk = &SmpsRAM.MusicTrks[TrkID];
		memset(TempTrk, 0x00, sizeof(TRK_RAM));
		TempTrk->SmpsCfg = SmpsRAM.MusCfg;
		TempTrk->PlaybkFlags = PBKFLG_ACTIVE;
		TempTrk->ChannelMask = SmpsFileConfig->AddChnList[CurTrk];
		TempTrk->TickMult = TickMult;
		TempTrk->Pos = ReadPtr(&Data[CurPos + 0x00], SmpsFileConfig);
		TempTrk->Transpose = Data[CurPos + 0x02];
		TempTrk->Volume = Data[CurPos + 0x03];
		//FinishFMTrkInit:
		TempTrk->ModEnv = 0x00;
		TempTrk->Instrument = 0x00;
		//FinishTrkInit:
		TempTrk->StackPtr = TRK_STACK_SIZE;
		TempTrk->PanAFMS = 0xC0;
		TempTrk->Timeout = 0x01;
		if (SmpsFileConfig->LoopPtrs != NULL)
			TempTrk->LoopOfs = SmpsFileConfig->LoopPtrs[TrkBase + CurTrk];
		else
			TempTrk->LoopOfs = 0x0000;
		
		if ((TempTrk->ChannelMask & 0xF8) == 0x10)
			TempTrk->SpcDacMode = SmpsFileConfig->DrumChnMode;
		
		SmpsRAM.MusicTrks[TRACK_MUS_FM6].PlaybkFlags = 0x00;
		
		if (TempTrk->Pos >= SmpsFileConfig->SeqLength)
			TempTrk->PlaybkFlags &= ~PBKFLG_ACTIVE;
	}
	
	//SetSFXOverrideBits();
	
	return;
}

void PlaySFX(SMPS_CFG* SmpsFileConfig, UINT8 SpecialSFX)
{
	const UINT8* Data;
	UINT16 CurPos;
	UINT8 ChnCnt;
	UINT8 TickMult;
	UINT8 CurTrk;
	UINT8 SFXTrkID;
	UINT8 SpcSFXTrkID;
	UINT8 MusTrkID;
	TRK_RAM* SFXTrk;
	TRK_RAM* MusTrk;
	
	Data = SmpsFileConfig->SeqData;
	CurPos = 0x00;
	
	//InsLibPtr = ReadPtr(&Data[CurPos + 0x00], SmpsFileConfig);
	TickMult = Data[CurPos + 0x02];
	ChnCnt = Data[CurPos + 0x03];
	CurPos += 0x04;
	
	for (CurTrk = 0; CurTrk < ChnCnt; CurTrk ++, CurPos += 0x06)
	{
		GetSFXChnPtrs(Data[CurPos + 0x01], &MusTrkID, &SpcSFXTrkID, &SFXTrkID);
		
		if (SpecialSFX)
		{
			if (SpcSFXTrkID == 0xFF)
				continue;
			SFXTrk = &SmpsRAM.SpcSFXTrks[SpcSFXTrkID];
		}
		else
		{
			if (SFXTrkID == 0xFF)
				continue;
			SFXTrk = &SmpsRAM.SFXTrks[SFXTrkID];
		}
		if (MusTrkID != 0xFF)
		{
			MusTrk = &SmpsRAM.MusicTrks[MusTrkID];
			MusTrk->PlaybkFlags |= PBKFLG_OVERRIDDEN;
		}
		
		memset(SFXTrk, 0x00, sizeof(TRK_RAM));
		SFXTrk->SmpsCfg = SmpsFileConfig;
		SFXTrk->PlaybkFlags = Data[CurPos + 0x00];
		SFXTrk->ChannelMask = Data[CurPos + 0x01];
		if (SFXTrk->ChannelMask == 0x02)
			ResetSpcFM3Mode();
		SFXTrk->TickMult = TickMult;
		SFXTrk->Pos = ReadPtr(&Data[CurPos + 0x00], SmpsFileConfig);
		SFXTrk->Transpose = Data[CurPos + 0x02];
		SFXTrk->Volume = Data[CurPos + 0x03];
		//FinishFMTrkInit:
		SFXTrk->ModEnv = 0x00;
		SFXTrk->Instrument = 0x00;
		//FinishTrkInit:
		SFXTrk->StackPtr = TRK_STACK_SIZE;
		SFXTrk->PanAFMS = 0xC0;
		SFXTrk->Timeout = 0x01;
		
		if (SFXTrk->Pos >= SmpsFileConfig->SeqLength)
			SFXTrk->PlaybkFlags &= ~PBKFLG_ACTIVE;
		
		if (SpcSFXTrkID != 0xFF && SFXTrkID != 0xFF)
		{
			if (SmpsRAM.SFXTrks[SFXTrkID].PlaybkFlags & PBKFLG_ACTIVE)
				SmpsRAM.SpcSFXTrks[SpcSFXTrkID].PlaybkFlags |= PBKFLG_OVERRIDDEN;
		}
		
		//SFXTrk->InsPtr = InsLibPtr;
		DoNoteOff(SFXTrk);
		DisableSSGEG(SFXTrk);
	}
	
	
	return;
}

void GetSFXChnPtrs(UINT8 ChannelMask, UINT8* MusicTrk, UINT8* SFXTrk, UINT8* SpcSFXTrk)
{
	if (MusicTrk != NULL)
		*MusicTrk = GetChannelTrack(ChannelMask, MUS_TRKCNT, SmpsRAM.MusicTrks);
	
	if (SFXTrk != NULL)
		*SFXTrk = GetChannelTrack(ChannelMask, SFX_TRKCNT, SmpsRAM.SFXTrks);
	
	if (SpcSFXTrk != NULL)
		*SpcSFXTrk = GetChannelTrack(ChannelMask, SPCSFX_TRKCNT, SmpsRAM.SpcSFXTrks);
	
	return;
}

UINT8 GetChannelTrack(UINT8 ChannelMask, UINT8 TrkCount, const TRK_RAM* Tracks)
{
	UINT8 CurTrk;
	
	for (CurTrk = 0; CurTrk < TrkCount; CurTrk ++)
	{
		if (Tracks[CurTrk].ChannelMask == ChannelMask)
			return CurTrk;
	}
	
	return 0xFF;
}


static void DoPause(void)
{
	if (! SmpsRAM.PauseMode)
		return;	// 00 - not paused
	
	if (! (SmpsRAM.PauseMode & 0x80))
	{
		if (SmpsRAM.PauseMode == 0x01)
		{
			SmpsRAM.PauseMode = 0x02;
			SilenceAll();
		}
	}
	else
	{
		//UnpauseMusic:
		UINT8 CurTrk;
		TRK_RAM* TempTrk;
		
		if (SmpsRAM.FadeOut.Steps)
		{
			StopAllSound();
			return;
		}
		
		for (CurTrk = 0; CurTrk < MUS_TRKCNT; CurTrk ++)
		{
			TempTrk = &SmpsRAM.MusicTrks[CurTrk];
			if (TempTrk->PlaybkFlags & PBKFLG_ACTIVE)
			{
				if (! (TempTrk->ChannelMask & 0x80))
					WriteFMMain(TempTrk, 0xB4, TempTrk->PanAFMS);
			}
		}
		
		for (CurTrk = 0; CurTrk < SPCSFX_TRKCNT; CurTrk ++)
		{
			TempTrk = &SmpsRAM.SpcSFXTrks[CurTrk];
			if (TempTrk->PlaybkFlags & PBKFLG_ACTIVE)
			{
				if (! (TempTrk->ChannelMask & 0x80))
					WriteFMMain(TempTrk, 0xB4, TempTrk->PanAFMS);
			}
		}
	}
	
	return;
}

static void DoTempo(void)
{
	UINT16 NewTempoVal;
	UINT8 CurTrk;
	
	switch(SmpsRAM.MusCfg->TempoMode)
	{
	case TEMPO_TIMEOUT:
		// Note: (pre-)SMPS 68k checks TempoInit, SMPS Z80 checks TempoCntr
		if (! SmpsRAM.TempoInit)
			return;	// Tempo 00 - never delayed
		
		SmpsRAM.TempoCntr --;
		if (SmpsRAM.TempoCntr)
			return;
		// reached 00 - delay tracks
		SmpsRAM.TempoCntr = SmpsRAM.TempoInit;
		break;
	case TEMPO_OVERFLOW:
		NewTempoVal = SmpsRAM.TempoCntr + SmpsRAM.TempoInit;
		SmpsRAM.TempoCntr = (UINT8)NewTempoVal;
		if (NewTempoVal < 0x100)
			return;
		// calculation overflowed - delay tracks
		break;
	case TEMPO_OVERFLOW2:
		NewTempoVal = SmpsRAM.TempoCntr + SmpsRAM.TempoInit;
		SmpsRAM.TempoCntr = (UINT8)NewTempoVal;
		if (NewTempoVal >= 0x100)
			return;
		// calculation didn't overflow - delay tracks
		break;
	case TEMPO_TOUT_OFLW:
		if (! (SmpsRAM.TempoInit & 0x80))
		{
			// Tempo 00..7F - Timeout
			if (! SmpsRAM.TempoInit)
				return;	// Tempo 00 - never delayed
			
			SmpsRAM.TempoCntr --;
			if (SmpsRAM.TempoCntr)
				return;
			// reached 00 - delay tracks
			SmpsRAM.TempoCntr = SmpsRAM.TempoInit;
		}
		else
		{
			// Tempo 80..FF - Overflow
			NewTempoVal = SmpsRAM.TempoCntr + (SmpsRAM.TempoInit & 0x7F);
			SmpsRAM.TempoCntr = (UINT8)NewTempoVal;
			if (NewTempoVal < 0x100)
				return;
			// calculation overflowed - delay tracks
		}
		break;
	}
	
	// Delay all tracks by 1 frame.
	for (CurTrk = 0; CurTrk < MUS_TRKCNT; CurTrk ++)
		SmpsRAM.MusicTrks[CurTrk].Timeout ++;
	
	return;
}

void FadeOutMusic(void)
{
	FADE_INF* Fade = &SmpsRAM.FadeOut;
	
	Fade->Steps = 0x28 | 0x80;
	Fade->DlyInit = 0x06;
	Fade->DlyCntr = Fade->DlyInit;
	
	return;
}

static void DoFadeOut(void)
{
	FADE_INF* Fade = &SmpsRAM.FadeOut;
	UINT8 CurTrk;
	TRK_RAM* TempTrk;
	UINT8 FinalVol;
	
	if (! Fade->Steps)
		return;	// Fading disabled - return
	
	if (Fade->Steps & 0x80)
	{
		// TODO: Maybe a loop checking for Channel Bits 0x80 and 0x10 would be a better solution.
		//call StopDrumPSG:
		SmpsRAM.MusicTrks[TRACK_MUS_DRUM].PlaybkFlags = 0x00;
		SmpsRAM.MusicTrks[TRACK_MUS_DAC2].PlaybkFlags = 0x00;
		//SmpsRAM.MusicTrks[TRACK_MUS_FM6].PlaybkFlags = 0x00;	// for SMPS Z80 with FM drums only
#if 0
		SmpsRAM.MusicTrks[TRACK_MUS_PSG3].PlaybkFlags = 0x00;
		SmpsRAM.MusicTrks[TRACK_MUS_PSG1].PlaybkFlags = 0x00;
		SmpsRAM.MusicTrks[TRACK_MUS_PSG2].PlaybkFlags = 0x00;
		SilencePSG();
#endif
		
		Fade->Steps &= ~0x80;	// Note: The driver clears this bit even if it wasn't set.
	}
	
	Fade->DlyCntr --;
	if (Fade->DlyCntr)
		return;
	// Timeout expired
	
	//ApplyFading:
	Fade->DlyCntr = Fade->DlyInit;
	Fade->Steps --;
	if (! Fade->Steps)
	{
		StopAllSound();
		return;
	}
	
	for (CurTrk = 0; CurTrk < MUS_TRKCNT; CurTrk ++)
	{
		TempTrk = &SmpsRAM.MusicTrks[CurTrk];
		
#if 0
		TempTrk->Volume ++;
		if (TempTrk->Volume & 0x80)
			TempTrk->Volume --;	// prevent overflow
#endif
		// This gets more complicated for the few idiotic homebrew SMPS files
		// that use negative volumes.
		FinalVol = TempTrk->Volume;
		TempTrk->Volume ++;
		if ((TempTrk->Volume & 0x80) && ! (FinalVol & 0x80))
			TempTrk->Volume --;	// prevent overflow
		
		if (TempTrk->PlaybkFlags & PBKFLG_ACTIVE)
		{
			if (! (TempTrk->PlaybkFlags & PBKFLG_OVERRIDDEN))
			{
				if (TempTrk->ChannelMask & 0x80)
				{
					if (TempTrk->PlaybkFlags & PBKFLG_ATREST)
						continue;
					
					FinalVol = TempTrk->Volume + TempTrk->VolEnvCache;
					if (FinalVol >= 0x10)
						FinalVol = 0x0F;
					FinalVol |= TempTrk->ChannelMask | 0x10;
					if (TempTrk->PlaybkFlags & PBKFLG_SPCMODE)
						FinalVol |= 0x20;
					WritePSG(FinalVol);
				}
				else if (! (TempTrk->ChannelMask & 0xF0))
				{
					RefreshVolume(TempTrk);
				}
			}
		}
	}
	
	return;
}


void StopAllSound(void)
{
	TRK_RAM TempTrack;
	UINT8 CurChn;
	UINT8 CurTrk;
	
	// TODO: Clear all memory?
	for (CurTrk = 0; CurTrk < MUS_TRKCNT; CurTrk ++)
		SmpsRAM.MusicTrks[CurTrk].PlaybkFlags = 0x00;
	for (CurTrk = 0; CurTrk < SFX_TRKCNT; CurTrk ++)
		SmpsRAM.SFXTrks[CurTrk].PlaybkFlags = 0x00;
	for (CurTrk = 0; CurTrk < SPCSFX_TRKCNT; CurTrk ++)
		SmpsRAM.SpcSFXTrks[CurTrk].PlaybkFlags = 0x00;
	
	for (CurChn = 0; CurChn < 8; CurChn ++)
		DAC_Stop(CurChn);
	for (CurChn = 0; CurChn < 7; CurChn ++)
	{
		if ((CurChn & 0x03) == 0x03)
			continue;
		TempTrack.ChannelMask = CurChn;
		SilenceFMChn(&TempTrack);
		DisableSSGEG(&TempTrack);
	}
	
	SmpsRAM.FadeOut.Steps = 0x00;
	SilencePSG();
	ResetSpcFM3Mode();
	DAC_ResetOverride();
	
	StopSignal();
	
	return;
}

void ResetSpcFM3Mode(void)
{
	SmpsRAM.SpcFM3Mode = 0x0F;
	WriteFMI(0x27, SmpsRAM.SpcFM3Mode);
	
	return;
}

void DisableSSGEG(TRK_RAM* Trk)
{
	UINT8 CurOp;
	
	for (CurOp = 0x00; CurOp < 0x10; CurOp += 0x04)
		WriteFMMain(Trk, 0x90 | CurOp, 0x00);
	
	return;
}

void SilenceFMChn(TRK_RAM* Trk)
{
	UINT8 CurOp;
	
	//SetMaxRelRate:
	for (CurOp = 0x00; CurOp < 0x10; CurOp += 0x04)
		WriteFMMain(Trk, 0x80 | CurOp, 0xFF);
	
	for (CurOp = 0x00; CurOp < 0x10; CurOp += 0x04)
		WriteFMMain(Trk, 0x40 | CurOp, 0x7F);
	
	WriteFMI(0x28, Trk->ChannelMask);
	
	return;
}

static void SilenceAll(void)
{
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 3; CurChn ++)
		WriteFMI(0xB4 | CurChn, 0x00);
	for (CurChn = 0; CurChn < 3; CurChn ++)
		WriteFMII(0xB4 | CurChn, 0x00);
	
	for (CurChn = 0; CurChn < 7; CurChn ++)
		WriteFMI(0x28, CurChn);
	
	SilencePSG();
	
	return;
}

static void SilencePSG(void)
{
	UINT8 CurChn;
	UINT8 PSGVal;
	
	PSGVal = 0x9F;
	for (CurChn = 0x00; CurChn < 0x04; CurChn ++, PSGVal += 0x20)
		WritePSG(PSGVal);
	
	return;
}

void SetDACState(UINT8 DacOn)
{
	SmpsRAM.DacState = DacOn;
	WriteFMI(0x2B, DacOn);
	
	return;
}
