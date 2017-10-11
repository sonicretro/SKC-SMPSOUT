// SMPSOUT.cpp : Defines the exported functions for the DLL application.
//

#ifdef _MSC_VER
#include <tchar.h>
#else
#define _T(x) L ## x
#endif
#include <windows.h>

extern "C"
{
// defines required for SMPSPlay files:
//  DISABLE_DLOAD_FILE
//  DISABLE_DEBUG_MSGS
//  DISABLE_NECPCM
#include <common_def.h>
#include "../SMPSPlay/src/Engine/smps.h"
#include "../SMPSPlay/src/Engine/smps_commands.h"
#include "../SMPSPlay/src/Sound.h"
#include "../SMPSPlay/src/loader_data.h"
#include "../SMPSPlay/src/loader_smps.h"
}

#include <sstream>
#if ! defined(_MSC_VER) || _MSC_VER >= 1600
#include "IniFile.hpp"
#define INISUPPORT 1
#endif
#include <fstream>
#include <vector>
#include <ctime>
#include "resource.h"
#include "resource.gen.h"
#include "songinfo.h"
#include "musicid.gen.h"
using namespace std;

#if defined(_MSC_VER) && _MSC_VER >= 1400
#define get_time_int(x)	_time32(x)	// use _time32 for VC2005 and later
#else
#define get_time_int(x)	time(x)
#endif


HMODULE moduleHandle;
HWND gameWindow;
extern "C"
{
	//extern UINT32 SampleRate;	// from Sound.c
	extern UINT16 FrameDivider;
	//extern UINT32 SmplsPerFrame;
	extern INT32 OutputVolume;
	extern AUDIO_CFG AudioCfg;

	__declspec(dllexport) BOOL PlaySega();
	__declspec(dllexport) BOOL StopSega();

	void NotifySongStopped(void);
}

enum MusicID2 {
	MusicID_ByCharacter = -5,
	MusicID_ByZone,
	MusicID_MIDI,
	MusicID_Random,
	MusicID_Default,
	MusicID_Midboss = MusicID_SKMidboss,
	MusicID_Unused = MusicID_S3Midboss,
	MusicID_HiddenPalace = MusicID_SKCredits + 1,
	MusicID_SuperSonic,
	MusicID_Ending,
	MusicID_DataSelect,
	MusicID_SpecialStageResult,
	MusicID_BlueSphereResult,
	MusicID_BlueSphereTitle,
	MusicID_BlueSphereDifficulty,
	MusicID_TimeAttackRecords,
	TrackCount
};

struct dacentry { int resid; unsigned char rate; };

dacentry DACFiles[] = {
	{ IDR_DAC_81, 0x04 },
	{ IDR_DAC_82, 0x0E },
	{ IDR_DAC_82, 0x14 },
	{ IDR_DAC_82, 0x1A },
	{ IDR_DAC_82, 0x20 },
	{ IDR_DAC_86, 0x04 },
	{ IDR_DAC_87, 0x04 },
	{ IDR_DAC_88, 0x06 },
	{ IDR_DAC_89, 0x0A },
	{ IDR_DAC_8A, 0x14 },
	{ IDR_DAC_8A, 0x1B },
	{ IDR_DAC_8C, 0x08 },
	{ IDR_DAC_8D, 0x0B },
	{ IDR_DAC_8D, 0x11 },
	{ IDR_DAC_8F, 0x08 },
	{ IDR_DAC_90, 0x03 },
	{ IDR_DAC_90, 0x07 },
	{ IDR_DAC_90, 0x0A },
	{ IDR_DAC_90, 0x0E },
	{ IDR_DAC_94, 0x06 },
	{ IDR_DAC_94, 0x0A },
	{ IDR_DAC_94, 0x0D },
	{ IDR_DAC_94, 0x12 },
	{ IDR_DAC_98, 0x0B },
	{ IDR_DAC_98, 0x13 },
	{ IDR_DAC_98, 0x16 },
	{ IDR_DAC_9B, 0x0C },
	{ IDR_DAC_9C, 0x0A },
	{ IDR_DAC_9D, 0x18 },
	{ IDR_DAC_9E, 0x18 },
	{ IDR_DAC_9F, 0x0C },
	{ IDR_DAC_A0, 0x0C },
	{ IDR_DAC_A1, 0x0A },
	{ IDR_DAC_A2, 0x0A },
	{ IDR_DAC_A3, 0x18 },
	{ IDR_DAC_A4, 0x18 },
	{ IDR_DAC_A5, 0x0C },
	{ IDR_DAC_A6, 0x09 },
	{ IDR_DAC_A7, 0x18 },
	{ IDR_DAC_A8, 0x18 },
	{ IDR_DAC_A9, 0x0C },
	{ IDR_DAC_AA, 0x0A },
	{ IDR_DAC_AB, 0x0D },
	{ IDR_DAC_AC, 0x06 },
	{ IDR_DAC_AD, 0x10 },
	{ IDR_DAC_AD, 0x18 },
	{ IDR_DAC_AF, 0x09 },
	{ IDR_DAC_AF, 0x12 },
	{ IDR_DAC_B1, 0x18 },
	{ IDR_DAC_B2, 0x16 },
	{ IDR_DAC_B2, 0x20 },
	{ IDR_DAC_B4, 0x0C },
	{ IDR_DAC_B5, 0x0C },
	{ IDR_DAC_B6, 0x0C },
	{ IDR_DAC_B7, 0x18 },
	{ IDR_DAC_B8, 0x0C },
	{ IDR_DAC_B8, 0x0C },
	{ IDR_DAC_BA, 0x18 },
	{ IDR_DAC_BB, 0x18 },
	{ IDR_DAC_BC, 0x18 },
	{ IDR_DAC_BD, 0x0C },
	{ IDR_DAC_BE, 0x0C },
	{ IDR_DAC_BF, 0x1C },
	{ IDR_DAC_C0, 0x0B },
	{ IDR_DAC_B4, 0x0F },
	{ IDR_DAC_B4, 0x11 },
	{ IDR_DAC_B4, 0x12 },
	{ IDR_DAC_B4, 0x0B },
	{ IDR_DAC_9F_S3D, 0x01 },
	{ IDR_DAC_A0_S3D, 0x12 }
};
#define S3D_ID_BASE	(0xC5 - 0x81)

UINT8 FMDrumList[] = {
	// 0    1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
	0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x86, 0x87, 0x82, 0x83, 0x82, 0x84, 0x82, 0x85,	// 81..8F
	0x82, 0x83, 0x84, 0x85, 0x82, 0x83, 0x84, 0x85, 0x82, 0x83, 0x84, 0x81, 0x88, 0x86, 0x81, 0x81,	// 90..9F
	0x87, 0x87, 0x87, 0x81, 0x86, 0x00, 0x00, 0x86, 0x86, 0x87, 0x87, 0x81, 0x85, 0x82, 0x83, 0x82,	// A0..AF
	0x84, 0x87, 0x82, 0x84, 0x00, 0x00, 0x87, 0x86, 0x86, 0x86, 0x87, 0x87, 0x81, 0x86, 0x00, 0x81,	// B0..BF
	0x86, 0x00, 0x00, 0x00, 0x00                                                                   	// C0..C4
};

dacentry DACFiles_S2[] = {
	{ IDR_DAC_81_S2, 0x17 },
	{ IDR_DAC_82_S2, 0x01 },
	{ IDR_DAC_83_S2, 0x06 },
	{ IDR_DAC_84_S2, 0x08 },
	{ IDR_DAC_85_S2, 0x1B },
	{ IDR_DAC_86_S2, 0x0A },
	{ IDR_DAC_87_S2, 0x1B }
};

UINT8 S2DrumList[][2] =
{	{0x00, 0x00},
	{0x81, 0x17},
	{0x82, 0x01},
	{0x83, 0x06},
	{0x84, 0x08},
	{0x85, 0x1B},
	{0x86, 0x0A},
	{0x87, 0x1B},
	{0x85, 0x12},
	{0x85, 0x15},
	{0x85, 0x1C},
	{0x85, 0x1D},
	{0x86, 0x02},
	{0x86, 0x05},
	{0x86, 0x08},
	{0x87, 0x08},
	{0x97, 0x0B},
	{0x97, 0x12},
};

static const UINT8 DefDPCMData[] =
{	0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
	0x80, 0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0};

static const UINT8 FMCHN_ORDER[] = {0x16, 0, 1, 2, 4, 5, 6};
static const UINT8 PSGCHN_ORDER[] = {0x80, 0xA0, 0xC0};

static const UINT16 DEF_FMFREQ_VAL[13] =
{0x25E, 0x284, 0x2AB, 0x2D3, 0x2FE, 0x32D, 0x35C, 0x38F, 0x3C5, 0x3FF, 0x43C, 0x47C, 0x4C0};

static const UINT16 DEF_PSGFREQ_68K_VAL[] =
{	0x356, 0x326, 0x2F9, 0x2CE, 0x2A5, 0x280, 0x25C, 0x23A, 0x21A, 0x1FB, 0x1DF, 0x1C4,
	0x1AB, 0x193, 0x17D, 0x167, 0x153, 0x140, 0x12E, 0x11D, 0x10D, 0x0FE, 0x0EF, 0x0E2,
	0x0D6, 0x0C9, 0x0BE, 0x0B4, 0x0A9, 0x0A0, 0x097, 0x08F, 0x087, 0x07F, 0x078, 0x071,
	0x06B, 0x065, 0x05F, 0x05A, 0x055, 0x050, 0x04B, 0x047, 0x043, 0x040, 0x03C, 0x039,
	0x036, 0x033, 0x030, 0x02D, 0x02B, 0x028, 0x026, 0x024, 0x022, 0x020, 0x01F, 0x01D,
	0x01B, 0x01A, 0x018, 0x017, 0x016, 0x015, 0x013, 0x012, 0x011, 0x000};

static const UINT16 DEF_PSGFREQ_Z80_T2_VAL[] =
{	0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3F7, 0x3BE, 0x388,
	0x356, 0x326, 0x2F9, 0x2CE, 0x2A5, 0x280, 0x25C, 0x23A, 0x21A, 0x1FB, 0x1DF, 0x1C4,
	0x1AB, 0x193, 0x17D, 0x167, 0x153, 0x140, 0x12E, 0x11D, 0x10D, 0x0FE, 0x0EF, 0x0E2,
	0x0D6, 0x0C9, 0x0BE, 0x0B4, 0x0A9, 0x0A0, 0x097, 0x08F, 0x087, 0x07F, 0x078, 0x071,
	0x06B, 0x065, 0x05F, 0x05A, 0x055, 0x050, 0x04B, 0x047, 0x043, 0x040, 0x03C, 0x039,
	0x036, 0x033, 0x030, 0x02D, 0x02B, 0x028, 0x026, 0x024, 0x022, 0x020, 0x01F, 0x01D,
	0x01B, 0x01A, 0x018, 0x017, 0x016, 0x015, 0x013, 0x012, 0x011, 0x010, 0x000, 0x000};

static const UINT16 FM3FREQS[] = { 0, 0x132, 0x18E, 0x1E4, 0x234, 0x27E, 0x2C2, 0x2F0 };

static const CMD_FLAGS CMDFLAGS_S12[] = {
	{ CF_PANAFMS, CFS_PAFMS_PAN, 0x02 },
	{ CF_DETUNE, 0x00, 0x02 },
	{ CF_SET_COMM, 0x00, 0x02 },
	{ CF_RETURN, 0x00, 0x01 },
	//{ CF_TRK_END, 0x00, 0x01 },	// actually CF_FADE_IN_SONG, but that's not supported yet
	{ CF_FADE_IN_SONG, 0x00, 0x01 },
	{ CF_TICK_MULT, CFS_TMULT_CUR, 0x02 },
	{ CF_VOLUME, CFS_VOL_NN_FM, 0x02 },
	{ CF_HOLD, 0x00, 0x01 },
	{ CF_NOTE_STOP, CFS_NSTOP_NORMAL, 0x02 },
	{ CF_TRANSPOSE, CFS_TRNSP_ADD, 0x02 },
	{ CF_TEMPO, CFS_TEMPO_SET, 0x02 },
	{ CF_TICK_MULT, CFS_TMULT_ALL, 0x02 },
	{ CF_VOLUME, CFS_VOL_NN_PSG, 0x02 },
	{ CF_IGNORE, 0x00, 0x01 },	// Clear Pushing Flag
	{ CF_IGNORE, 0x00, 0x01 },
	{ CF_INSTRUMENT, CFS_INS_N_FM, 0x82 },
	{ CF_MOD_SETUP, 0x00, 0x05 },
	{ CF_MOD_SET, CFS_MODS_ON, 0x01 },
	{ CF_TRK_END, CFS_TEND_STD, 0x01 },
	{ CF_PSG_NOISE, CFS_PNOIS_SET, 0x02 },
	{ CF_MOD_SET, CFS_MODS_OFF, 0x01 },
	{ CF_INSTRUMENT, CFS_INS_C_PSG, 0x02 },
	{ CF_GOTO, 0x00, 0x03, 0x01 },
	{ CF_LOOP, 0x00, 0x05, 0x03 },
	{ CF_GOSUB, 0x00, 0x03, 0x01 },
	{ CF_SND_OFF, 0x00, 0x01, 0x01 }
};

static const CMD_FLAGS CMDFLAGS[] = {
	{ CF_PANAFMS, CFS_PAFMS_PAN, 0x02 },
	{ CF_DETUNE, 0x00, 0x02 },
	{ CF_FADE_IN_SONG, 0x00, 0x02 },
	//{ CF_SET_COMM, 0x00, 0x02 },
	{ CF_TRK_END, CFS_TEND_MUTE, 0x01 },
	{ CF_VOLUME, CFS_VOL_ABS_S3K, 0x02 },
	{ CF_VOLUME, CFS_VOL_CC_FMP2, 0x03 },
	{ CF_VOLUME, CFS_VOL_CC_FM, 0x02 },
	{ CF_HOLD, 0x00, 0x01 },
	{ CF_NOTE_STOP, CFS_NSTOP_MULT, 0x02 },
	{ CF_IGNORE, 0x00, 0x01 },
	{ CF_PLAY_DAC, 0x00, 0x02 },
	{ CF_LOOP_EXIT, 0x00, 0x04, 0x02 },
	{ CF_VOLUME, CFS_VOL_CC_PSG, 0x02 },
	{ CF_TRANSPOSE, CFS_TRNSP_SET_S3K, 0x02 },
	{ CF_FM_COMMAND, 0x00, 0x03 },
	{ CF_INSTRUMENT, CFS_INS_N_FM, 0x82 },
	{ CF_MOD_SETUP, 0x00, 0x05 },
	{ CF_MOD_ENV, CFS_MENV_FMP, 0x03 },
	{ CF_TRK_END, CFS_TEND_STD, 0x01 },
	{ CF_PSG_NOISE, CFS_PNOIS_SRES, 0x02 },
	{ CF_MOD_ENV, CFS_MENV_GEN, 0x02 },
	{ CF_INSTRUMENT, CFS_INS_C_PSG, 0x02 },
	{ CF_GOTO, 0x00, 0x03, 0x01 },
	{ CF_LOOP, 0x00, 0x05, 0x03 },
	{ CF_GOSUB, 0x00, 0x03, 0x01 },
	{ CF_RETURN, 0x00, 0x01 },
	{ CF_MOD_SET, CFS_MODS_OFF, 0x01 },
	{ CF_TRANSPOSE, CFS_TRNSP_ADD, 0x02 },
	{ CF_IGNORE, 0x00, 0x03, 0x01 },
	{ CF_RAW_FREQ, 0x00, 0x02 },
	{ CF_SPC_FM3, 0x00, 0x05 },
	{ CF_META_CF, 0x00, 0x01 }
};

static const CMD_FLAGS CMDMETAFLAGS[] = {
	{ CF_TEMPO, CFS_TEMPO_SET, 0x02 },
	{ CF_SND_CMD, 0x00, 0x02 },
	{ CF_IGNORE, 0x00, 0x02 },
	{ CF_IGNORE, 0x00, 0x03 },
	{ CF_TICK_MULT, CFS_TMULT_ALL, 0x02 },
	{ CF_SSG_EG, CFS_SEG_NORMAL, 0x05 },
	{ CF_FM_VOLENV, 0x00, 0x03 },
	{ CF_IGNORE, 0x00, 0x01 }
};

static const UINT8 INSOPS_DEFAULT[] =
{	0xB0,
0x30, 0x38, 0x34, 0x3C,
0x50, 0x58, 0x54, 0x5C,
0x60, 0x68, 0x64, 0x6C,
0x70, 0x78, 0x74, 0x7C,
0x80, 0x88, 0x84, 0x8C,
0x40, 0x48, 0x44, 0x4C,
0x00};	// The 0 is required by the SMPS routines to terminate the array.

static const UINT8 INSOPS_HARDWARE[] =
{	0xB0,
0x30, 0x34, 0x38, 0x3C,
0x50, 0x54, 0x58, 0x5C,
0x60, 0x64, 0x68, 0x6C,
0x70, 0x74, 0x78, 0x7C,
0x80, 0x84, 0x88, 0x8C,
0x40, 0x44, 0x48, 0x4C,
0x00};	// The 0 is required by the SMPS routines to terminate the array.

enum DriverMode {
	SmpsDrv_S3K,
	SmpsDrv_S12
};

struct SampleInfo { UINT8 *data; UINT32 length; };

struct DrumInfo {
	unsigned char id;
	bool isRef;
	union {
		struct {
			unsigned char game;
			unsigned char id;
		} ref;
		struct {
			bool isPCM;
			UINT8 *data;
			UINT32 dataSize;
			UINT8 rate;
		} cust;
	};
};

struct DrumSet { unsigned char base; int drumCount; DrumInfo *drums; };

struct SongInfo { UINT8 *data; UINT16 length; UINT16 base; unsigned char mode; DrumSet *drumset; };

vector<SongInfo> songs(SongCount);

vector<const char *> customsongs;

#ifdef _M_IX86
#define DataRef(type,name,addr) type &name = *(type *)addr

DataRef(unsigned int, GameSelection, 0x831188);
DataRef(unsigned char, reg_d0, 0x8549A4);
DataRef(unsigned short, Ending_running_flag, 0x8FFEF72);
DataRef(unsigned char, Game_mode, 0x8FFF600);
DataRef(unsigned char, Super_Tails_flag, 0x8FFF667);
DataRef(unsigned short, Current_zone_and_act, 0x8FFFE10);
DataRef(unsigned char, Super_Sonic_Knux_flag, 0x8FFFE19);
DataRef(unsigned short, Saved_zone_and_act, 0x8FFFE2C);
DataRef(unsigned short, Player_mode, 0x8FFFF08);

#define GameModeID_SegaScreen 0
#define GameModeID_TitleScreen 4
#define GameModeID_Demo 8
#define GameModeID_Level 0xC
#define GameModeID_SegaScreen2 0x10
#define GameModeID_ContinueScreen 0x14
#define GameModeID_SegaScreen3 0x18
#define GameModeID_LevelSelect 0x1C
#define GameModeID_S3Credits 0x20
#define GameModeID_LevelSelect2 0x24
#define GameModeID_LevelSelect3 0x28
#define GameModeID_BlueSpheresTitle 0x2C
#define GameModeID_BlueSpheresDifficulty 0x2C
#define GameModeID_BlueSpheresResults 0x30
#define GameModeID_SpecialStage 0x34
#define GameModeID_CompetitionMenu 0x38
#define GameModeID_CompetitionPlayerSelect 0x3C
#define GameModeID_CompetitionLevelSelect 0x40
#define GameModeID_CompetitionResults 0x44
#define GameModeID_SpecialStageResults 0x48
#define GameModeID_SaveScreen 0x4C
#define GameModeID_TimeAttackRecords 0x50

#define flying_battery_zone 4
#define mushroom_hill_zone 7
#define gumball_bonus 0x13
#define glowing_spheres_bonus 0x14
#define slot_bonus 0x15
#define ending 0x0D01
#define hidden_palace_zone 0x1601
#define hidden_palace_shrine 0x1701
#endif

class MidiInterfaceClass
{
public:
	virtual BOOL initialize(HWND hwnd) = 0; // hwnd = game window
	virtual BOOL load_song(short id, unsigned int bgmmode) = 0; // id = song to be played + 1 (well, +1 compared to the sound test id, it's the ID of the song in the MIDIOUT.DLL's resources); bgmmode = 0 for FM synth, 1 for General MIDI
	virtual BOOL play_song() = 0;
	virtual BOOL stop_song() = 0;
	virtual BOOL pause_song() = 0;
	virtual BOOL unpause_song() = 0;
	virtual BOOL set_song_tempo(unsigned int pct) = 0; // pct = percentage of delay between beats the song should be set to. lower = faster tempo
};

bool EnableSKCHacks = true;
UINT8* SmpsReloadState;
UINT8* SmpsMusicSaveState;
const char *const INISections[] = { "S3K", "S&K", "S3" };

class SMPSInterfaceClass : MidiInterfaceClass
{
	SMPS_CFG smpscfg_3K;
	SMPS_CFG smpscfg_12;
	SMPS_CFG* cursmpscfg;
	SMPS_SET cursmps;
	ENV_LIB VolEnvs_S3;
	ENV_LIB VolEnvs_SK;
	bool fmdrum_on;
#ifdef _M_IX86
	short trackSettings[TrackCount], s3TrackSettings[TrackCount], skTrackSettings[TrackCount];
#endif
	bool trackMIDI;
	MidiInterfaceClass *MIDIFallbackClass;

	INLINE UINT16 ReadBE16(const UINT8* Data)
	{
		return (Data[0x00] << 8) | (Data[0x01] << 0);
	}

	INLINE UINT16 ReadLE16(const UINT8* Data)
	{
		return (Data[0x01] << 8) | (Data[0x00] << 0);
	}

	static void LoadRegisterList(SMPS_CFG* SmpsCfg, UINT8 RegCnt, const UINT8* RegPtr)
	{
		UINT8 CurReg;
		UINT8 RegTL_Idx;

		RegTL_Idx = 0xFF;
		for (CurReg = 0x00; CurReg < RegCnt; CurReg ++)
		{
			if (RegPtr[CurReg] == 0x00 || (RegPtr[CurReg] & 0x03))
				break;
			if ((RegPtr[CurReg] & 0xF0) == 0x40 && RegTL_Idx == 0xFF)
				RegTL_Idx = CurReg;
		}
		RegCnt = CurReg;
		if (! RegCnt)
		{
			SmpsCfg->InsRegCnt = 0x00;
			SmpsCfg->InsRegs = NULL;
			SmpsCfg->InsReg_TL = NULL;
			return;
		}

		SmpsCfg->InsRegCnt = RegCnt;
		SmpsCfg->InsRegs = (UINT8*)RegPtr;
		if (RegTL_Idx == 0xFF)
			SmpsCfg->InsReg_TL = NULL;
		else
			SmpsCfg->InsReg_TL = (UINT8*)&RegPtr[RegTL_Idx];

		return;
	}

	static void LoadSettings(UINT8 Mode, SMPS_CFG* SmpsCfg)
	{
		switch(Mode)
		{
		case SmpsDrv_S12:
			SmpsCfg->PtrFmt = PTRFMT_Z80;

			SmpsCfg->InsMode = INSMODE_DEF;
			LoadRegisterList(SmpsCfg, (UINT8)LengthOfArray(INSOPS_DEFAULT), INSOPS_DEFAULT);
			SmpsCfg->FMChnCnt = (UINT8)LengthOfArray(FMCHN_ORDER);
			memcpy(SmpsCfg->FMChnList, FMCHN_ORDER, SmpsCfg->FMChnCnt);
			SmpsCfg->PSGChnCnt = (UINT8)LengthOfArray(PSGCHN_ORDER);
			memcpy(SmpsCfg->PSGChnList, PSGCHN_ORDER, SmpsCfg->PSGChnCnt);
			SmpsCfg->AddChnCnt = 0;

			SmpsCfg->TempoMode = TEMPO_TIMEOUT;
			SmpsCfg->Tempo1Tick = T1TICK_DOTEMPO;	// S1/S2B: T1TICK_DOTEMPO, S2F: T1TICK_NOTEMPO
			SmpsCfg->FMBaseNote = FMBASEN_B;
			SmpsCfg->FMBaseOct = 0;
			SmpsCfg->FMOctWrap = 0;
			SmpsCfg->NoteOnPrevent = NONPREV_REST;
			SmpsCfg->DelayFreq = DLYFREQ_RESET;
			SmpsCfg->FM6DACOff = 0x01;	// improve Special Stage -> Chaos Emerald song change
			SmpsCfg->ModAlgo = MODULAT_68K;
			SmpsCfg->EnvMult = ENVMULT_68K;
			SmpsCfg->VolMode = VOLMODE_ALGO;
			SmpsCfg->DrumChnMode = DCHNMODE_NORMAL;

			SmpsCfg->FMFreqs = (UINT16*)&DEF_FMFREQ_VAL[0];
			SmpsCfg->FMFreqCnt = 12;
			SmpsCfg->PSGFreqs = (UINT16*)DEF_PSGFREQ_68K_VAL;
			SmpsCfg->PSGFreqCnt = (UINT8)LengthOfArray(DEF_PSGFREQ_68K_VAL);
			SmpsCfg->FM3Freqs = (UINT16*)FM3FREQS;
			SmpsCfg->FM3FreqCnt = (UINT8)LengthOfArray(FM3FREQS);

			SmpsCfg->FadeMode = FADEMODE_68K;
			SmpsCfg->FadeOut.Steps = 0x28;
			SmpsCfg->FadeOut.Delay = 3;
			SmpsCfg->FadeOut.AddFM = 1;
			SmpsCfg->FadeOut.AddPSG = 1;
			SmpsCfg->FadeIn.Steps = 0x28;
			SmpsCfg->FadeIn.Delay = 2;
			SmpsCfg->FadeIn.AddFM = 1;
			SmpsCfg->FadeIn.AddPSG = 1;

			SmpsCfg->EnvCmds[0] = ENVCMD_HOLD;

			SmpsCfg->NoteBase = 0x80;
			SmpsCfg->CmdList.CmdData = (CMD_FLAGS*)CMDFLAGS_S12;
			SmpsCfg->CmdList.FlagBase = 0xE0;
			SmpsCfg->CmdList.FlagCount = (UINT16)LengthOfArray(CMDFLAGS);
			SmpsCfg->CmdMetaList.CmdData = NULL;
			SmpsCfg->CmdMetaList.FlagBase = 0x00;
			SmpsCfg->CmdMetaList.FlagCount = 0;
			break;
		case SmpsDrv_S3K:
			SmpsCfg->PtrFmt = PTRFMT_Z80;

			SmpsCfg->InsMode = INSMODE_DEF;
			LoadRegisterList(SmpsCfg, (UINT8)LengthOfArray(INSOPS_DEFAULT), INSOPS_DEFAULT);
			SmpsCfg->FMChnCnt = (UINT8)LengthOfArray(FMCHN_ORDER);
			memcpy(SmpsCfg->FMChnList, FMCHN_ORDER, SmpsCfg->FMChnCnt);
			SmpsCfg->PSGChnCnt = (UINT8)LengthOfArray(PSGCHN_ORDER);
			memcpy(SmpsCfg->PSGChnList, PSGCHN_ORDER, SmpsCfg->PSGChnCnt);
			SmpsCfg->AddChnCnt = 0;

			SmpsCfg->TempoMode = TEMPO_OVERFLOW;
			SmpsCfg->Tempo1Tick = T1TICK_DOTEMPO;
			SmpsCfg->FMBaseNote = FMBASEN_C;
			SmpsCfg->FMBaseOct = 0;
			SmpsCfg->FMOctWrap = 0;
			SmpsCfg->NoteOnPrevent = NONPREV_HOLD;
			SmpsCfg->DelayFreq = DLYFREQ_KEEP;
			SmpsCfg->FM6DACOff = 0x01;	// improve Special Stage -> Chaos Emerald song change
			SmpsCfg->ModAlgo = MODULAT_Z80;
			SmpsCfg->EnvMult = ENVMULT_Z80;
			SmpsCfg->VolMode = VOLMODE_BIT7;
			SmpsCfg->DrumChnMode = DCHNMODE_NORMAL;

			SmpsCfg->FMFreqs = (UINT16*)&DEF_FMFREQ_VAL[1];
			SmpsCfg->FMFreqCnt = 12;
			SmpsCfg->PSGFreqs = (UINT16*)DEF_PSGFREQ_Z80_T2_VAL;
			SmpsCfg->PSGFreqCnt = (UINT8)LengthOfArray(DEF_PSGFREQ_Z80_T2_VAL);
			SmpsCfg->FM3Freqs = (UINT16*)FM3FREQS;
			SmpsCfg->FM3FreqCnt = (UINT8)LengthOfArray(FM3FREQS);

			SmpsCfg->FadeMode = FADEMODE_Z80;
			SmpsCfg->FadeOut.Steps = 0x28;
			SmpsCfg->FadeOut.Delay = 6;
			SmpsCfg->FadeOut.AddFM = 1;
			SmpsCfg->FadeOut.AddPSG = 1;
			SmpsCfg->FadeIn.Steps = 0x40;
			SmpsCfg->FadeIn.Delay = 2;
			SmpsCfg->FadeIn.AddFM = 1;
			SmpsCfg->FadeIn.AddPSG = 1;

			SmpsCfg->EnvCmds[0] = ENVCMD_RESET;
			SmpsCfg->EnvCmds[1] = ENVCMD_HOLD;
			SmpsCfg->EnvCmds[2] = ENVCMD_LOOP;
			SmpsCfg->EnvCmds[3] = ENVCMD_STOP;
			SmpsCfg->EnvCmds[4] = ENVCMD_CHGMULT;

			SmpsCfg->NoteBase = 0x80;
			SmpsCfg->CmdList.CmdData = (CMD_FLAGS*)CMDFLAGS;
			SmpsCfg->CmdList.FlagBase = 0xE0;
			SmpsCfg->CmdList.FlagCount = (UINT16)LengthOfArray(CMDFLAGS);
			SmpsCfg->CmdMetaList.CmdData = (CMD_FLAGS*)CMDMETAFLAGS;
			SmpsCfg->CmdMetaList.FlagBase = 0x00;
			SmpsCfg->CmdMetaList.FlagCount = (UINT16)LengthOfArray(CMDMETAFLAGS);
			break;
		}
	}

#if defined(INISUPPORT) && defined(_M_IX86)
	void ReadSettings(const IniGroup *settings, short *trackSettings)
	{
		for (int i = 0; i < TrackCount; i++)
		{
			string value = settings->getString(TrackOptions[i].name);
			if (value == "ByCharacter")
			{
				trackSettings[i] = MusicID_ByCharacter;
				continue;
			}
			else if (value == "ByZone")
			{
				trackSettings[i] = MusicID_ByZone;
				continue;
			}
			else if (value == "MIDI")
			{
				if (MIDIFallbackClass) // don't use MIDI if fallback DLL wasn't found
					trackSettings[i] = MusicID_MIDI;
				continue;
			}
			else if (value == "Default")
				continue;
			else if (TrackOptions[i].optioncount >= 2)
			{
				if (value == "Random")
				{
					trackSettings[i] = MusicID_Random;
					continue;
				}
				else
				{
					bool found = false;
					for (int j = 0; j < TrackOptions[i].optioncount; j++)
						if (value == TrackOptions[i].options[j].text)
						{
							trackSettings[i] = TrackOptions[i].options[j].id;
							found = true;
							break;
						}
					if (found) continue;
				}
			}
			for (unsigned int j = 0; j < customsongs.size(); j++)
				if (value == customsongs[j])
				{
					trackSettings[i] = SongCount + j;
					break;
				}
		}
	}
#endif

public:
	SMPSInterfaceClass() { }

	void GenerateDACAlgos(DAC_ALGO* DACAlgo, UINT8 Mode, UINT32 DacCycles)
	{
		DACAlgo->BaseCycles = DacCycles;	// BaseCyles overrides BaseRate and Divider
		if (Mode == COMPR_DPCM)
		{
			DACAlgo->LoopCycles = 26;
			DACAlgo->LoopSamples = 2;
		}
		else //if (Mode == COMPR_PCM)
		{
			DACAlgo->LoopCycles = 13;
			DACAlgo->LoopSamples = 1;
		}
		DACAlgo->RateMode = DACRM_DELAY;
		DACAlgo->DefCompr = Mode;
		return;
	}

	void GenerateDACDrv(DAC_CFG* DACDrv, UINT16 smplCount, WORD startID, UINT32 dacCycles,
						unsigned int smplTblSize, const dacentry* smplTblList)
	{
		unsigned int i;
		DAC_SETTINGS* DACCfg;
		HRSRC hres;
		
		memset(DACDrv, 0x00, sizeof(DAC_CFG));
		
		DACDrv->SmplAlloc = smplCount;
		DACDrv->Smpls = new DAC_SAMPLE[DACDrv->SmplAlloc];
		memset(DACDrv->Smpls, 0x00, DACDrv->SmplAlloc * sizeof(DAC_SAMPLE));
		for (i = 0; i < smplCount; i++)
		{
			hres = FindResource(moduleHandle, MAKEINTRESOURCE(startID + i), _T("DAC"));
			DACDrv->Smpls[i].Compr = COMPR_DPCM;
			DACDrv->Smpls[i].DPCMArr = (UINT8*)DefDPCMData;
			DACDrv->Smpls[i].Size = SizeofResource(moduleHandle, hres);
			DACDrv->Smpls[i].Data = (UINT8*)LockResource(LoadResource(moduleHandle, hres));
		}
		DACDrv->SmplCount = smplCount;
		
		DACCfg = &DACDrv->Cfg;
		DACCfg->AlgoAlloc = 1;
		DACCfg->Algos = new DAC_ALGO[DACCfg->AlgoAlloc];
		memset(DACCfg->Algos, 0x00, DACCfg->AlgoAlloc * sizeof(DAC_ALGO));
		GenerateDACAlgos(&DACCfg->Algos[0], COMPR_DPCM, 297);
		DACCfg->AlgoCount = 1;
		
		DACCfg->SmplMode = DACSM_NORMAL;
		DACCfg->Channels = 1;
		DACCfg->VolDiv = 1;
		
		DACDrv->TblAlloc = (UINT16)smplTblSize;
		DACDrv->SmplTbl = new DAC_TABLE[DACDrv->TblAlloc];
		memset(DACDrv->SmplTbl, 0x00, DACDrv->TblAlloc * sizeof(DAC_TABLE));
		for (i = 0; i < smplTblSize; i++)
		{
			DACDrv->SmplTbl[i].Sample = smplTblList[i].resid - startID;
			DACDrv->SmplTbl[i].Rate = smplTblList[i].rate;
		}
		DACDrv->TblCount = (UINT16)smplTblSize;
		
		DACDrv->BankCount = 0;
		DACDrv->BankAlloc = 0;
		DACDrv->BankTbl = NULL;
		
		return;
	}
	
	BOOL initialize(HWND hwnd)
	{
		HRSRC hres;
		UINT8* dataPtr;
		UINT32 dataSize;
		unsigned int i;

		ZeroMemory(&smpscfg_3K, sizeof(smpscfg_3K));
		ZeroMemory(&smpscfg_12, sizeof(smpscfg_12));
		cursmpscfg = NULL;
		fmdrum_on = false;

		for (i = 0; i < SongCount; i++)
		{
			hres = FindResource(moduleHandle, MAKEINTRESOURCE(IDR_MUSIC_1 + i), _T("MUSIC"));
			songs[i].mode = MusicFiles[i].mode;
			songs[i].base = MusicFiles[i].base;
			songs[i].length = (UINT16)SizeofResource(moduleHandle, hres);
			songs[i].data = (UINT8*)LockResource(LoadResource(moduleHandle, hres));
		}

#ifdef INISUPPORT
		IniFile custsongs(_T("songs_cust.ini"));
		unordered_map<string, DrumSet *> drumsets;
		unordered_map <string, SampleInfo> samples;
		
		for (auto iter = custsongs.begin(); iter != custsongs.end(); iter++)
		{
			if (iter->first.empty()) continue;
			IniGroup *group = iter->second;
			SongInfo si = { };
			string str = group->getString("Type");
			if (str == "S1")
				si.mode = TrackMode_S1;
			else if (str == "S2")
				si.mode = TrackMode_S2;
			else if (str == "S2B")
				si.mode = TrackMode_S2B;
			else if (str == "S3D")
				si.mode = TrackMode_S3D;
			else if (str == "S3")
				si.mode = TrackMode_S3;
			si.base = group->getHexInt("Offset");
			FILE *fi;
			fopen_s(&fi, group->getString("File").c_str(), "rb");
			if (fi != nullptr)
			{
				fseek(fi, 0, SEEK_END);
				si.length = (UINT16)ftell(fi);
				fseek(fi, 0, SEEK_SET);
				si.data = new UINT8[si.length];
				fread(si.data, 1, si.length, fi);
				fclose(fi);
				if (group->hasKey("CustomDrums"))
				{
					string drumsetfile = group->getString("CustomDrums");
					auto it2 = drumsets.find(drumsetfile);
					if (it2 == drumsets.cend())
					{
						IniFile drumini(drumsetfile);
						DrumSet *drumset = new DrumSet;
						string str = drumini.getString("", "BaseSet");
						if (str == "S1")
							drumset->base = TrackMode_S1;
						else if (str == "S2")
							drumset->base = TrackMode_S2;
						else if (str == "S2B")
							drumset->base = TrackMode_S2B;
						else if (str == "S3D")
							drumset->base = TrackMode_S3D;
						else if (str == "S3")
							drumset->base = TrackMode_S3;
						else if (str == "S&K")
							drumset->base = TrackMode_SK;
						else
							drumset->base = -1;
						vector<DrumInfo> drums;
						for (auto it3 = drumini.begin(); iter != drumini.end(); it3++)
						{
							if (it3->first.empty()) continue;
							IniGroup *g2 = it3->second;
							DrumInfo di = {};
							di.id = (UINT8)strtol(it3->first.c_str(), nullptr, 16);
							di.isRef = g2->hasKey("DrumRef");
							if (di.isRef)
							{
								string str = g2->getString("DrumRef");
								size_t i = str.find(' ');
								di.ref.id = (UINT8)strtol(str.substr(i + 1).c_str(), nullptr, 16);
								str = str.substr(0, i);
								if (str == "S1")
									di.ref.game = TrackMode_S1;
								else if (str == "S2")
									di.ref.game = TrackMode_S2;
								else if (str == "S2B")
									di.ref.game = TrackMode_S2B;
								else if (str == "S3D")
									di.ref.game = TrackMode_S3D;
								else if (str == "S3")
									di.ref.game = TrackMode_S3;
								else
									di.ref.game = TrackMode_SK;
							}
							else
							{
								di.cust.isPCM = g2->getBool("PCM");
								di.cust.rate = (UINT8)g2->getHexInt("Rate");
								string str = g2->getString("File");
								auto it4 = samples.find(str);
								if (it4 == samples.cend())
								{
									fopen_s(&fi, str.c_str(), "rb");
									if (fi != nullptr)
									{
										fseek(fi, 0, SEEK_END);
										di.cust.dataSize = (UINT32)ftell(fi);
										fseek(fi, 0, SEEK_SET);
										di.cust.data = new UINT8[di.cust.dataSize];
										fread(di.cust.data, 1, di.cust.dataSize, fi);
										fclose(fi);
										samples[str] = { di.cust.data, di.cust.dataSize };
									}
									else
										continue;
								}
								else
								{
									di.cust.data = it4->second.data;
									di.cust.dataSize = it4->second.length;
								}
							}
							drums.push_back(di);
						}
						drumset->drumCount = drums.size();
						drumset->drums = new DrumInfo[drums.size()];
						memcpy(drumset->drums, drums.data(), sizeof(DrumInfo) * drums.size());
						drumsets[drumsetfile] = drumset;
						si.drumset = drumset;
					}
					else
						si.drumset = it2->second;
				}
				songs.push_back(si);
				char *buf = new char[iter->first.length() + 1];
				strncpy_s(buf, iter->first.length() + 1, iter->first.c_str(), iter->first.length());
				buf[iter->first.length()] = 0;
				customsongs.push_back(buf);
			}
		}
#endif

#ifdef _M_IX86
		if (EnableSKCHacks)
		{
			HMODULE midimodule = LoadLibrary(_T("MIDIOUTY.DLL"));
			if (!midimodule)
				midimodule = LoadLibrary(_T("MIDIOUT.DLL")); // in case this DLL is SMPSOUT.DLL, MIDIOUT.DLL will be the original
			if (midimodule && midimodule != moduleHandle) // don't want to recursively call ourself
			{
				MIDIFallbackClass = ((MidiInterfaceClass *(*)())GetProcAddress(midimodule, "GetMidiInterface"))();
				MIDIFallbackClass->initialize(hwnd);
			}

			gameWindow = hwnd;

			memset(&trackSettings, MusicID_Default, sizeof(short) * TrackCount);

#ifdef INISUPPORT
			IniFile settings(_T("SMPSOUT.ini"));
			fmdrum_on = settings.getBool("", "FMDrums");

			const IniGroup *group = settings.getGroup("");
			if (group != nullptr)
				ReadSettings(group, trackSettings);

			memcpy(s3TrackSettings, trackSettings, sizeof(short) * TrackCount);
			memcpy(skTrackSettings, trackSettings, sizeof(short) * TrackCount);

			group = settings.getGroup(INISections[GameSelection]);
			if (group != nullptr)
				ReadSettings(group, trackSettings);

			group = settings.getGroup("S3");
			if (group != nullptr)
				ReadSettings(group, s3TrackSettings);
			if (s3TrackSettings[MusicID_Midboss] == MusicID_Default)
				s3TrackSettings[MusicID_Midboss] = MusicID_S3Midboss;
			if (s3TrackSettings[MusicID_Continue] == MusicID_Default)
				s3TrackSettings[MusicID_Continue] = MusicID_S3Continue;

			group = settings.getGroup("S&K");
			if (group != nullptr)
				ReadSettings(group, skTrackSettings);

#else
			fmdrum_on = true;
#endif
			if (trackSettings[MusicID_HiddenPalace] == MusicID_Default)
				trackSettings[MusicID_HiddenPalace] = MusicID_LavaReef2;
			if (trackSettings[MusicID_Ending] == MusicID_Default)
				trackSettings[MusicID_Ending] = MusicID_SkySanctuary;
			if (trackSettings[MusicID_BlueSphereTitle] == MusicID_Default)
				trackSettings[MusicID_BlueSphereTitle] = GameSelection == 2 ? MusicID_S3Continue : MusicID_Continue;
			if (trackSettings[MusicID_BlueSphereDifficulty] == MusicID_Default)
				trackSettings[MusicID_BlueSphereDifficulty] = MusicID_SKInvincibility;
			if (trackSettings[MusicID_TimeAttackRecords] == MusicID_Default)
				trackSettings[MusicID_TimeAttackRecords] = GameSelection == 2 ? MusicID_S3Continue : MusicID_Continue;
			if (GameSelection == 2)
			{
				if (trackSettings[MusicID_Midboss] == MusicID_Default)
					trackSettings[MusicID_Midboss] = MusicID_S3Midboss;
				if (trackSettings[MusicID_Continue] == MusicID_Default)
					trackSettings[MusicID_Continue] = MusicID_S3Continue;
			}
		}
#endif
		LoadSettings(SmpsDrv_S3K, &smpscfg_3K);
		LoadSettings(SmpsDrv_S12, &smpscfg_12);

		ZeroMemory(&smpscfg_3K.DrumLib, sizeof(smpscfg_3K.DrumLib));
		smpscfg_3K.DrumLib.DrumCount = 0x60;
		smpscfg_3K.DrumLib.DrumData = new DRUM_DATA[smpscfg_3K.DrumLib.DrumCount];
		ZeroMemory(smpscfg_3K.DrumLib.DrumData, sizeof(DRUM_DATA) * smpscfg_3K.DrumLib.DrumCount);

		smpscfg_3K.DrumLib.DrumData[0].Type = DRMTYPE_NONE;
		for (i = 1; i < smpscfg_3K.DrumLib.DrumCount; i++)
		{
			smpscfg_3K.DrumLib.DrumData[i].Type = DRMTYPE_DAC;
			smpscfg_3K.DrumLib.DrumData[i].DrumID = i - 1;	// set DAC sample
		}

		ZeroMemory(&smpscfg_12.DrumLib, sizeof(smpscfg_12.DrumLib));
		smpscfg_12.DrumLib.DrumCount = (UINT8)LengthOfArray(S2DrumList);
		smpscfg_12.DrumLib.DrumData = new DRUM_DATA[smpscfg_12.DrumLib.DrumCount];
		ZeroMemory(smpscfg_12.DrumLib.DrumData, sizeof(DRUM_DATA) * smpscfg_12.DrumLib.DrumCount);

		smpscfg_12.DrumLib.DrumData[0].Type = DRMTYPE_NONE;
		for (i = 1; i < smpscfg_12.DrumLib.DrumCount; i ++)
		{
			if (! S2DrumList[i][0])
			{
				smpscfg_12.DrumLib.DrumData[i].Type = DRMTYPE_NONE;
			}
			else
			{
				smpscfg_12.DrumLib.DrumData[i].Type = DRMTYPE_DAC;
				smpscfg_12.DrumLib.DrumData[i].DrumID = S2DrumList[i][0] - 0x81;	// set DAC sample
				smpscfg_12.DrumLib.DrumData[i].PitchOvr = S2DrumList[i][1];
			}
		}

		hres = FindResource(moduleHandle, MAKEINTRESOURCE(IDR_MISC_MODULAT), _T("MISC"));
		dataSize = SizeofResource(moduleHandle, hres);
		dataPtr = (UINT8*)LockResource(LoadResource(moduleHandle, hres));
		LoadEnvelopeData_Mem(dataSize, dataPtr, &smpscfg_3K.ModEnvs);

		hres = FindResource(moduleHandle, MAKEINTRESOURCE(IDR_MISC_PSG_S3), _T("MISC"));
		dataSize = SizeofResource(moduleHandle, hres);
		dataPtr = (UINT8*)LockResource(LoadResource(moduleHandle, hres));
		LoadEnvelopeData_Mem(dataSize, dataPtr, &VolEnvs_S3);

		hres = FindResource(moduleHandle, MAKEINTRESOURCE(IDR_MISC_PSG_SK), _T("MISC"));
		dataSize = SizeofResource(moduleHandle, hres);
		dataPtr = (UINT8*)LockResource(LoadResource(moduleHandle, hres));
		LoadEnvelopeData_Mem(dataSize, dataPtr, &VolEnvs_SK);

		hres = FindResource(moduleHandle, MAKEINTRESOURCE(IDR_MISC_PSG_S2), _T("MISC"));
		dataSize = SizeofResource(moduleHandle, hres);
		dataPtr = (UINT8*)LockResource(LoadResource(moduleHandle, hres));
		LoadEnvelopeData_Mem(dataSize, dataPtr, &smpscfg_12.VolEnvs);

		hres = FindResource(moduleHandle, MAKEINTRESOURCE(IDR_MISC_FM_DRUMS), _T("MISC"));
		dataSize = SizeofResource(moduleHandle, hres);
		dataPtr = (UINT8*)LockResource(LoadResource(moduleHandle, hres));
		LoadDrumTracks_Mem(dataSize, dataPtr, &smpscfg_3K.FMDrums, 0x01);	// 0x01 - FM drums

		smpscfg_3K.VolEnvs = VolEnvs_SK;

		GenerateDACDrv(&smpscfg_3K.DACDrv, (IDR_DAC_A0_S3D - IDR_DAC_81) + 1, IDR_DAC_81, 297,
						(unsigned int)LengthOfArray(DACFiles), DACFiles);

		GenerateDACDrv(&smpscfg_12.DACDrv, (IDR_DAC_87_S2 - IDR_DAC_81_S2) + 1, IDR_DAC_81_S2, 288,
						(unsigned int)LengthOfArray(DACFiles_S2), DACFiles_S2);

		hres = FindResource(moduleHandle, MAKEINTRESOURCE(IDR_MISC_INSSET), _T("MISC"));
		LoadGlobalInstrumentLib_Mem((UINT16)SizeofResource(moduleHandle, hres),
									(UINT8 *)LockResource(LoadResource(moduleHandle, hres)), &smpscfg_3K);
		/*smpscfg_3K.GblIns.alloc = 0x00;
		smpscfg_3K.GblIns.Len = (UINT16)SizeofResource(moduleHandle, hres);
		smpscfg_3K.GblIns.Data = (UINT8 *)LockResource(LoadResource(moduleHandle, hres));

		UINT16 InsCount = smpscfg_3K.GblIns.Len / smpscfg_3K.InsRegCnt;
		smpscfg_3K.GblInsLib.InsCount = InsCount;
		smpscfg_3K.GblInsLib.InsPtrs = NULL;
		if (InsCount)
		{
			smpscfg_3K.GblInsLib.InsPtrs = new UINT8*[InsCount];
			UINT16 CurPos = 0x0000;
			for (UINT16 CurIns = 0x00; CurIns < InsCount; CurIns ++, CurPos += smpscfg_3K.InsRegCnt)
				smpscfg_3K.GblInsLib.InsPtrs[CurIns] = &smpscfg_3K.GblIns.Data[CurPos];
		}*/
		smpscfg_3K.GblInsBase = 0x17D8;

		smpscfg_12.GblIns.alloc = 0x00;
		smpscfg_12.GblIns.Len = 0x0000;
		smpscfg_12.GblIns.Data = NULL;
		smpscfg_12.GblInsLib.InsCount = 0;
		smpscfg_12.GblInsLib.InsPtrs = NULL;

		DAC_Reset();
		InitAudioOutput();
		{
			const char* const AudAPIList[] = {"WASAPI", "XAudio2", "DirectSound", "WinMM"};
			unsigned int curADrv;
			UINT8 retVal;
			
			for (curADrv = 0; curADrv < 4; curADrv ++)
			{
				retVal = QueryDeviceParams(AudAPIList[curADrv], &AudioCfg);
				if (retVal)
					continue;
				AudioCfg.AudAPIName = (char*)AudAPIList[curADrv];
				AudioCfg.AudioBufs = 5;
				AudioCfg.AudioBufSize = (curADrv < 3) ? 4 : 8;	// WinMM: use 8+ buffers
				AudioCfg.Volume = 1.0f;
				
				retVal = StartAudioOutput();
				if (! retVal)
					break;
			}
		}
		InitDriver();
		SMPSExtra_SetCallbacks(SMPSCB_STOP, &NotifySongStopped);
		SmpsReloadState = SmpsGetVariable(SMPSVAR_RESTORE_REQ);
		SmpsMusicSaveState = SmpsGetVariable(SMPSVAR_MUSSTATE_USE);

#ifdef _M_IX86
		if (EnableSKCHacks)
		{
			timeBeginPeriod(2);

			srand(get_time_int(NULL));
		}
#endif

		return TRUE;
	}

	BOOL load_song(short id, unsigned int bgmmode)
	{
		if (trackMIDI)
			MIDIFallbackClass->stop_song();
		//else
		//	ThreadSync(1);
		int newid = id;
#ifdef _M_IX86
		if (EnableSKCHacks)
		{
			--newid;
			switch (newid)
			{
			case MusicID_LavaReef2:
				if (Current_zone_and_act == hidden_palace_zone
					|| Current_zone_and_act == hidden_palace_shrine)
					newid = MusicID_HiddenPalace;
				break;
			case MusicID_SkySanctuary:
				if (Current_zone_and_act == ending || Ending_running_flag)
					newid = MusicID_Ending;
				break;
			case MusicID_Continue:
				if (Game_mode == GameModeID_BlueSpheresTitle)
					newid = MusicID_BlueSphereTitle;
				else if (Game_mode == GameModeID_TimeAttackRecords)
					newid = MusicID_TimeAttackRecords;
				break;
			case MusicID_ActClear:
				if (Game_mode == GameModeID_SpecialStageResults)
					newid = MusicID_SpecialStageResult;
				else if (Game_mode == GameModeID_BlueSpheresResults)
					newid = MusicID_BlueSphereResult;
				break;
			case MusicID_S3Invincibility:
			case MusicID_SKInvincibility:
				if (Game_mode == GameModeID_BlueSpheresDifficulty)
					newid = MusicID_BlueSphereDifficulty;
				else if (Super_Sonic_Knux_flag || Super_Tails_flag)
					newid = MusicID_SuperSonic;
				break;
			case MusicID_LevelSelect:
				if (Game_mode == GameModeID_SaveScreen)
					newid = MusicID_DataSelect;
				break;
			}
			short *settings = trackSettings;
			if (!GameSelection)
			{
				if (settings[newid] == MusicID_ByCharacter)
					if (Player_mode == 3)
						settings = skTrackSettings;
					else
						settings = s3TrackSettings;
				else if (!GameSelection && settings[newid] == MusicID_ByZone)
				{
					unsigned char level = Current_zone_and_act >> 8;
					switch (level)
					{
					case gumball_bonus:
					case glowing_spheres_bonus:
					case slot_bonus:
						level = Saved_zone_and_act >> 8;
						break;
					}
					if (level == flying_battery_zone || level >= mushroom_hill_zone)
						settings = skTrackSettings;
					else
						settings = s3TrackSettings;
				}
			}
			switch (newid)
			{
			case MusicID_SuperSonic:
			case MusicID_DataSelect:
			case MusicID_SpecialStageResult:
			case MusicID_BlueSphereResult:
				if (settings[newid] == MusicID_Default)
					newid = id - 1;
				break;
			}
			short set = settings[newid];
			if (MIDIFallbackClass && set == MusicID_MIDI)
			{
				trackMIDI = true;
				return MIDIFallbackClass->load_song(id, bgmmode);
			}
			else if (set == MusicID_Random)
			{
				const tracknameoptions *opt = &TrackOptions[newid];
				newid = opt->options[rand() % opt->optioncount].id;
			}
			else if (set != MusicID_Default)
				newid = set;
		}
#endif
		trackMIDI = false;
		if ((size_t)newid >= songs.size())
			return FALSE;
		const SongInfo *song = &songs[newid];
		switch (song->mode)
		{
		case TrackMode_S1:
		case TrackMode_S2B:
		case TrackMode_S2:
			cursmpscfg = &smpscfg_12;
			switch (song->mode)
			{
			case TrackMode_S1:
				cursmpscfg->PtrFmt = PTRFMT_68K;
				cursmpscfg->InsMode = INSMODE_DEF;
				LoadRegisterList(cursmpscfg, (UINT8)LengthOfArray(INSOPS_DEFAULT), INSOPS_DEFAULT);
				cursmpscfg->TempoMode = TEMPO_TIMEOUT;
				break;
			case TrackMode_S2B:
				cursmpscfg->PtrFmt = PTRFMT_Z80;
				cursmpscfg->InsMode = INSMODE_HW;
				LoadRegisterList(cursmpscfg, (UINT8)LengthOfArray(INSOPS_HARDWARE), INSOPS_HARDWARE);
				cursmpscfg->TempoMode = TEMPO_TIMEOUT;
				break;
			case TrackMode_S2:
				cursmpscfg->PtrFmt = PTRFMT_Z80;
				cursmpscfg->InsMode = INSMODE_HW;
				LoadRegisterList(cursmpscfg, (UINT8)LengthOfArray(INSOPS_HARDWARE), INSOPS_HARDWARE);
				cursmpscfg->TempoMode = TEMPO_OVERFLOW2;
				break;
			}
			break;
		//default:
		case TrackMode_SK:
		case TrackMode_S3:
		case TrackMode_S3D:
			DRUM_LIB* drumLib;
			
			cursmpscfg = &smpscfg_3K;
			drumLib = &cursmpscfg->DrumLib;
			if (song->mode == TrackMode_S3)
			{
				cursmpscfg->VolEnvs = VolEnvs_S3;
				cursmpscfg->DACDrv.SmplTbl[0xB2-0x81].Sample = IDR_DAC_B2_S3 - IDR_DAC_81;
				cursmpscfg->DACDrv.SmplTbl[0xB3-0x81].Sample = IDR_DAC_B2_S3 - IDR_DAC_81;
			}
			else
			{
				cursmpscfg->VolEnvs = VolEnvs_SK;
				cursmpscfg->DACDrv.SmplTbl[0xB2-0x81].Sample = IDR_DAC_B2 - IDR_DAC_81;
				cursmpscfg->DACDrv.SmplTbl[0xB3-0x81].Sample = IDR_DAC_B2 - IDR_DAC_81;
			}

			UINT8 i;
			if (! bgmmode && fmdrum_on)
			{
				// Sonic 3/K/3D - FM drums
				for (i = 1; i <= LengthOfArray(FMDrumList); i++)
				{
					if (FMDrumList[i - 1])
					{
						drumLib->DrumData[i].Type = DRMTYPE_FM;
						drumLib->DrumData[i].DrumID = FMDrumList[i - 1] - 0x81;
					}
				}
				if (song->mode == TrackMode_S3D)
				{
					drumLib->DrumData[0x9F-0x80].Type = DRMTYPE_FM;
					drumLib->DrumData[0x9F-0x80].DrumID = 0x86 - 0x81;
					drumLib->DrumData[0xA0-0x80].Type = DRMTYPE_DAC;
					drumLib->DrumData[0xA0-0x80].DrumID = S3D_ID_BASE + (0xA0-0x9F);
				}
			}
			else if (song->mode == TrackMode_S3D)
			{
				// Sonic 3D DAC drums
				for (i = 1; i < 0x1F; i++)
				{
					drumLib->DrumData[i].Type = DRMTYPE_DAC;
					drumLib->DrumData[i].DrumID = i - 1;	// set DAC sample
				}
				for (; i <= 0x20; i++)
				{
					drumLib->DrumData[i].Type = DRMTYPE_DAC;
					drumLib->DrumData[i].DrumID = S3D_ID_BASE + (i - 0x1F);	// S3D sounds 9F/A0 are C5/C6 here.
				}
				for (; i < drumLib->DrumCount; i++)
					drumLib->DrumData[i].Type = DRMTYPE_NONE;
			}
			else
			{
				// Sonic 3/K DAC drums
				for (i = 1; i < drumLib->DrumCount; i++)
				{
					drumLib->DrumData[i].Type = DRMTYPE_DAC;
					drumLib->DrumData[i].DrumID = i - 1;	// set DAC sample
				}
			}
			break;
		}	// end switch (song->mode)

		memset(&cursmps, 0x00, sizeof(SMPS_SET));
		cursmps.Cfg = cursmpscfg;
		cursmps.SeqBase = song->base;
		cursmps.Seq.alloc = 0x00;
		cursmps.Seq.Len = song->length;
		cursmps.Seq.Data = song->data;
		PreparseSMPSFile(&cursmps);
		return TRUE;
	}
	
	BOOL play_song()
	{
		if (trackMIDI)
		{
			ThreadSync(1);
			StopAllSound();	// make sure to clean SMPS memory (might prevent 1up bugs)
			ThreadSync(0);
			return MIDIFallbackClass->play_song();
		}
		if (cursmpscfg == NULL)
			return TRUE;
		
		ThreadSync(1);
		
		//SmplsPerFrame = SampleRate / FrameDivider;
		FrameDivider = 60;
		PlayMusic(&cursmps);
		
		ThreadSync(0);
		PauseStream(0);
		return TRUE;
	}

	BOOL stop_song()
	{
#ifdef _M_IX86
		if (EnableSKCHacks)
		{
			if (reg_d0 == 0xFF)
				return PlaySega();
			else if (reg_d0 == 0xFE)
				return StopSega();
		}
#endif
		if (trackMIDI)
			return MIDIFallbackClass->stop_song();
#ifdef _M_IX86
		if (EnableSKCHacks && reg_d0 > 0)
		{
			//if (reg_d0 == 0x2A)
			//	return TRUE;	// silently skip stopping before playing 1-up tune
			if (reg_d0 < 0xE0)
				return TRUE;	// don't stop if we're going to play a new song anyway
			if (reg_d0 == 0xE1)
			{
				FadeOutMusic();
				return TRUE;
			}
		}
#endif
		ThreadSync(1);
		StopAllSound();
		ThreadSync(0);
		return TRUE;
	}

	BOOL pause_song()
	{
		if (trackMIDI)
			return MIDIFallbackClass->pause_song();
		PauseStream(1);
		return TRUE;
	}

	BOOL unpause_song()
	{
		if (trackMIDI)
			return MIDIFallbackClass->unpause_song();
		PauseStream(0);
		return TRUE;
	}

	BOOL set_song_tempo(unsigned int pct)
	{
		if (trackMIDI)
			return MIDIFallbackClass->set_song_tempo(pct);
		//SmplsPerFrame = (SampleRate * pct) / (FrameDivider * 100);
		FrameDivider = 60 * 100 / pct;
		return TRUE;
	}

	~SMPSInterfaceClass()
	{
		SMPSExtra_SetCallbacks(SMPSCB_OFF, NULL);	// doing callbacks now can cause crashes
		ThreadSync(1);
		StopAllSound();
		
		DeinitDriver();
		//StopAudioOutput();	// It's already too late to do this.
		//DeinitAudioOutput();
		{
			delete [] smpscfg_3K.DrumLib.DrumData;
			delete [] smpscfg_12.DrumLib.DrumData;
			FreeEnvelopeData(&smpscfg_3K.ModEnvs);
			FreeEnvelopeData(&smpscfg_12.ModEnvs);
			FreeEnvelopeData(&VolEnvs_S3);
			FreeEnvelopeData(&VolEnvs_SK);
			FreeEnvelopeData(&smpscfg_12.VolEnvs);
			//FreeDACData(&smpscfg_3K.DACDrv);
			{
				delete [] smpscfg_3K.DACDrv.Smpls;
				delete [] smpscfg_3K.DACDrv.Cfg.Algos;
				delete [] smpscfg_3K.DACDrv.SmplTbl;
			}
			//FreeDACData(&smpscfg_12.DACDrv);
			{
				delete [] smpscfg_12.DACDrv.Smpls;
				delete [] smpscfg_12.DACDrv.Cfg.Algos;
				delete [] smpscfg_12.DACDrv.SmplTbl;
			}
			FreeDrumTracks(&smpscfg_3K.FMDrums);
			FreeDrumTracks(&smpscfg_3K.PSGDrums);
			FreeGlobalInstrumentLib(&smpscfg_3K);
			FreeGlobalInstrumentLib(&smpscfg_12);
		}
#if _M_IX86
		if (EnableSKCHacks)
			timeEndPeriod(2);
#endif
	}
};

SMPSInterfaceClass midiInterface;
void (*SongStoppedCallback)() = NULL;

extern "C"
{
	__declspec(dllexport) SMPSInterfaceClass *GetMidiInterface()
	{
		return &midiInterface;
	}

	__declspec(dllexport) BOOL InitializeDriver()
	{
#ifdef _M_IX86
		EnableSKCHacks = false;
#endif
		return midiInterface.initialize(NULL);
	}

	__declspec(dllexport) void RegisterSongStoppedCallback(void (*callback)())
	{
		SongStoppedCallback = callback;
	}

	__declspec(dllexport) BOOL PlaySong(short song)
	{
		if (!midiInterface.load_song(song, 0))
			return FALSE;
		return midiInterface.play_song();
	}

	__declspec(dllexport) BOOL StopSong()
	{
		return midiInterface.stop_song();
	}

	__declspec(dllexport) void FadeOutSong()
	{
		FadeOutMusic();
	}

	__declspec(dllexport) BOOL PauseSong()
	{
		return midiInterface.pause_song();
	}

	__declspec(dllexport) BOOL ResumeSong()
	{
		return midiInterface.unpause_song();
	}

	__declspec(dllexport) BOOL SetSongTempo(unsigned int pct)
	{
		return midiInterface.set_song_tempo(pct);
	}

	__declspec(dllexport) const char **GetCustomSongs(unsigned int &count)
	{
		count = (unsigned int)customsongs.size();
		return &customsongs[0];
	}

	__declspec(dllexport) BOOL PlaySega()
	{
		return PlaySound(MAKEINTRESOURCE(IDR_WAVE_SEGA), moduleHandle, SND_RESOURCE | SND_ASYNC);
	}

	__declspec(dllexport) BOOL StopSega()
	{
		return PlaySound(NULL, NULL, SND_ASYNC);
	}

	__declspec(dllexport) void SetVolume(double volume)
	{
		OutputVolume = (INT32)(volume * 0x100 + 0.5);
	}

	__declspec(dllexport) void SetWaveLogPath(const char *logfile)
	{
		if (AudioCfg.WaveLogPath)
		{
			free(AudioCfg.WaveLogPath);
			AudioCfg.WaveLogPath = NULL;
		}
		if (logfile)
		{
			AudioCfg.WaveLogPath = _strdup(logfile);
			AudioCfg.LogWave = 1;
		}
		else
		{
			AudioCfg.LogWave = 0;
		}
	}

	__declspec(dllexport) void RegisterSongLoopCallback(SMPS_CB_SIGNAL func)
	{
		SMPSExtra_SetCallbacks(SMPSCB_LOOP, func);
	}

	void NotifySongStopped(void)
	{
#ifdef _M_IX86
		// The SMPS driver restores the previous song automatically if:
		// 1. the "reload save state" flag is set
		// 2. there is a save state to load
		if (EnableSKCHacks && ! (*SmpsReloadState == 0x01 && *SmpsMusicSaveState))
		{
			PostMessageA(gameWindow, 0x464u, 0, 0);
			return;
		}
#endif
		if (SongStoppedCallback != NULL)
			SongStoppedCallback();
	}
}