#ifndef __SMPS_COMMANDS_H__
#define __SMPS_COMMANDS_H__

// SMPS Command Constants
// ======================

#define CF_IGNORE				0x00
// General Flags
// -------------
#define CF_PANAFMS				0x01	// E0
	#define CFS_PAFMS_PAN			0x00	// E0
	#define CFS_PAFMS_AMS			0x01	// EA (early SMPS Z80)
	#define CFS_PAFMS_FMS			0x02	// EB (early SMPS Z80)
	#define CFS_PAFMS_PAN_C			0x10	// FD (preSMPS 68k)
	#define CFS_PAFMS_PAN_L			0x11	// FF (preSMPS 68k)
	#define CFS_PAFMS_PAN_R			0x12	// FE (preSMPS 68k)
#define CF_DETUNE				0x02	// E1
#define CF_SET_COMM				0x03	// E2 (set Communication Byte)
#define CF_VOLUME				0x04	// E6/EC
// Note: I probably need:
//		68k volume: no FM/PSG check, no clipping (00..02)
//		Z80/1 volume: FM/PSG check, no clipping (10..12)
//		Z80/2 volume: FM/PSG check, volume is clipped to chip range (20..23)
// Command Syntax: CFS_VOL_ab_...
//	a - FM/PSG check, N for No checking, C for Checking
//	b - volume clipping, N for No clipping, C for Clipping
	#define CFS_VOL_NN_FM			0x00	// E6 [SMPS 68k]
	#define CFS_VOL_NN_PSG			0x01	// EC [SMPS 68k]
	#define CFS_VOL_NN_FMP			0x02	// E5 [SMPS 68k] (FM+PSG)
	#define CFS_VOL_NN_FMP1			0x03	// E6 [early SMPS Z80]
	#define CFS_VOL_CN_FM			0x10	// E6 [SMPS Z80 Type 1/2]
	#define CFS_VOL_CN_PSG			0x11	// EC [SMPS Z80 Type 1]
	#define CFS_VOL_CN_FMP			0x12	// E5 [SMPS Z80] (FM+PSG)
	#define CFS_VOL_CC_FM			0x20	// E6 [Sonic 3K]
	#define CFS_VOL_CC_PSG			0x21	// EC [SMPS Z80 Type 2]
	#define CFS_VOL_CC_FMP			0x22	// E5 (FM+PSG)
	#define CFS_VOL_CC_FMP2			0x23	// E5 [S3K] (FM+PSG, broken)
	#define CFS_VOL_ABS				0x40	// F0 [preSMPS] (absolute Volume)
	#define CFS_VOL_ABS_S3K			0x80	// E4 [S3K] (absolute Volume, 00..7F min...max scale for FM and PSG)
	#define CFS_VOL_ABS_HF			0x81	// FF 07 [Hybrid Front] (absolute Volume + global)
	#define CFS_VOL_ABS_HF2			0x82	// E5 [Hybrid Front] (broken, like FF 07 with first parameter ignored)
	#define CFS_VOL_ABS_TMP			0x83	// E4 [Tempo 32x]
	#define CFS_VOL_SPC_TMP			0x84	// E6 [Tempo 32x] (change FM volume, set PSG volume)
#define CF_HOLD					0x05	// E7 (known as "no attack")
#define CF_NOTE_STOP			0x06	// E8
	#define CFS_NSTOP_NORMAL		0x00	// SMPS 68k
	#define CFS_NSTOP_MULT			0x01	// SMPS Z80 (value gets multiplied with the track's Tick Multiplier)
#define CF_TRANSPOSE			0x07	// FB
	#define CFS_TRNSP_ADD			0x00
	#define CFS_TRNSP_SET			0x01
	#define CFS_TRNSP_SET_S3K		0x02	// ED set Transpose to (Param - 0x40) [Sonic 3K]
	#define CFS_TRNSP_ADD_ALL		0x08	// FF 05 (Sorcerian)
	#define CFS_TRNSP_GADD			0x10	// FC add global value to transpose [Tempo 32x]
	#define CFS_TRNSP_GSET			0x11	// FE set transpose to global value [Tempo 32x]
	#define CFS_TRNSP_RAND			0x80	// E9 random transpose [Tempo 32x]
#define CF_INSTRUMENT			0x08	// EF/F5
	#define CFS_INS_FM				0x00	// EF
	#define CFS_INS_PSG				0x01	// F5
	#define CFS_INS_IMASK			0x0F
	#define CFS_INS_N				0x00	// no channel check
	#define CFS_INS_C				0x10	// with channel check
	#define CFS_INS_CMASK			0x10
	#define CFS_INS_N_FM			(CFS_INS_N | CFS_INS_FM)
	#define CFS_INS_N_PSG			(CFS_INS_N | CFS_INS_PSG)
	#define CFS_INS_C_FM			(CFS_INS_C | CFS_INS_FM)
	#define CFS_INS_C_PSG			(CFS_INS_C | CFS_INS_PSG)
#define CF_PSG_NOISE			0x09	// F3 (set PSG Noise)
	#define CFS_PNOIS_SET			0x00	// F3 [SMPS 68k]
	#define CFS_PNOIS_SET2			0x01	// F3 [early SMPS Z80]
	#define CFS_PNOIS_SRES			0x02	// F3 [SMPS Z80]
#define CF_FM_COMMAND			0x0A	// ED/EE [SMPS Z80/68k]
	#define CFS_FMW_CHN				0x00	// ED
	#define CFS_FMW_FM1				0x01	// EE
#define CF_SND_CMD				0x0F	// EB [SMPS 68k] / FF 01 [SMPS Z80] (Play Sound ID)
#define CF_PLAY_DAC				0x10	// EA [SMPS Z80]
#define CF_PLAY_PWM				0x11	// FD [Tempo 32x]

// Modulation Flags
// ----------------
#define CF_MOD_SETUP			0x20	// F0
#define CF_MOD_SET				0x21	// F1/F4 [SMPS 68k]
	#define CFS_MODS_ON				0x00	// F1/FC (SMPS 68k)
	#define CFS_MODS_OFF			0x01	// F4/FD (SMPS 68k)
#define CF_MOD_ENV				0x22	// F1/F4 [SMPS 68k/Z80] (set Modulation Type)
	#define CFS_MENV_GEN			0x00	// F4
	#define CFS_MENV_FMP			0x01	// F1 (separate params for FM and PSG tracks)
	#define CFS_MENV_GEN2			0x02	// F1 (broken, always uses second param) [Tempo 32x]
#define CF_FM_VOLENV			0x23	// FF 06 [S3K] (enable FM volume envelope)
#define CF_LFO_MOD				0x24	// E2 [Ghostbusters]

// Effect Flags
// ------------
#define CF_PAN_ANIM				0x28	// E4 [SMPS 68k/Z80] (Pan Animation)
#define CF_SET_LFO				0x29	// E9 [SMPS 68k/Z80] (set LFO Data)
	#define CFS_LFO_NORMAL			0x00
	#define CFS_LFO_AMSEN			0x01	// can force AMS Enable bits
#define CF_SET_LFO_SPD			0x2A	// E9 [preSMPS] (set LFO Speed)
#define CF_PITCH_SLIDE			0x2B	// FC [SMPS Z80] (enable Portamento)
#define CF_RAW_FREQ				0x2C	// FD [SMPS Z80] (Raw Frequency Mode)
#define CF_SPC_FM3				0x2D	// FE [SMPS 68k/Z80] (enable Special FM3 Mode)
#define CF_SSG_EG				0x2E	// FF 05 [SMPS Z80] (set SSG-EG)
	#define CFS_SEG_NORMAL			0x00	// SMPS Z80
	#define CFS_SEG_FULLATK			0x01	// SMPS 68k

// Tempo Flags
// -----------
#define CF_TEMPO				0x40	// EA/FF 01
	#define CFS_TEMPO_SET			0x00	// EA [SMPS 68k/SMPS Z80]
	#define CFS_TEMPO_ADD			0x01	// E4 [preSMPS] / E1 [Ghostbusters]
#define CF_TICK_MULT			0x41	// FA/FC
	#define CFS_TMULT_CUR			0x00	// FA (current track) [Sonic 1: E5]
	#define CFS_TMULT_ALL			0x01	// FC/FF 02 (all tracks) [Sonic 1: EB]
#define CF_TIMING				0x42	// EA/EB [SMPS Z80 Type 1, YM2612 Timer]
	#define CFS_TIME_SET			0x00	// EA (set timing)
	#define CFS_TIME_ADD			0x01	// EB (add to timing)
	#define CFS_TIME_SPC			0x10	// EB (add or set timing, modify Timing Mode) [some SMPS Z80 Type 1]
#define CF_TIMING_MODE			0x43	// FF 00 [SMPS Z80 Type 1, Timing Mode]

// Jump and Control Flags
// ----------------------
#define CF_GOTO					0x70	// F6
#define CF_LOOP					0x71	// F7
#define CF_GOSUB				0x72	// F8
#define CF_RETURN				0x73	// F9 [Sonic 1: E3]
#define CF_LOOP_EXIT			0x74	// EB [SMPS Z80]
#define CF_COND_JUMP			0x75	// FF 07 [Ristar]
	#define CFS_CJMP_NZ				0x00
	#define CFS_CJMP_Z				0x01
	#define CFS_CJMP_EQ				0x10
	#define CFS_CJMP_RESET			0x80
	#define CFS_CJMP_NZ_RESET		(CFS_CJMP_RESET | CFS_CJMP_NZ)
	#define CFS_CJMP_Z_RESET		(CFS_CJMP_RESET | CFS_CJMP_Z)
#define CF_META_CF				0x7E	// FF
#define CF_TRK_END				0x7F	// F2/E3
	#define CFS_TEND_STD			0x00	// F2
	#define CFS_TEND_MUTE			0x01	// E3 [all but Sonic 1]

// Special Game-Specific Flags
// ---------------------------
#define CF_FADE_IN				0x80	// E4 [Sonic 1] / E2 [S3K]
#define CF_SND_OFF				0x81	// F9 [Sonic 1]
#define CF_NOTE_STOP_REV		0x83	// FF 06 [Ristar] (Reversed Note Stop)
	#define CFS_NSREV_CYMN			0x00
	#define CFS_NSREV_RST			0x01

#define CF_DAC_PS4				0x90
	#define CFS_PS4_VOLCTRL			0x00
	#define CFS_PS4_VOLUME			0x01
	#define CFS_PS4_SET_SND			0x02
	#define CFS_PS4_LOOP			0x03
	#define CFS_PS4_REVERSE			0x04
	#define CFS_PS4_TRKMODE			0x0F

#define CF_DAC_CYMN				0x91
	#define CFS_CYMN_CHG_CH		0x00
	#define CFS_CYMN_SET_CH		0x10

#define CF_DAC_GAXE3			0x92
	#define CFS_GA3_2NOTE_TEMP			0x00
	#define CFS_GA3_2NOTE_PERM			0x01
	#define CFS_GA3_2NOTE_OFF			0x02


//#define CF_			0x00
#define CFSMS_VOL_FX			0xC0
#define CFSMS_FREQ_ENV			0xC1
#define CFSMS_VOL_ENV			0xC2
#define CFSMS_VOL_QUICK			0xC3
#define CFSCD_PAN				0xD0
//#define CFSCD_				0xD1


#endif // __SMPS_COMMANDS_H__
