#include "timidplayer.h"

static const TCHAR ReverbPresetNames[][32] =
{
	_T("Generic"),
	_T("Padded Cell"),
	_T("Room"),
	_T("Bathroom"),
	_T("Living Room"),
	_T("Stone Room"),
	_T("Auditorium"),
	_T("Concert Hall"),
	_T("Cave"),
	_T("Arena"),
	_T("Hangar"),
	_T("Carpeted Hallway"),
	_T("Hallway"),
	_T("Stone Corridor"),
	_T("Alley"),
	_T("Forest"),
	_T("City"),
	_T("Mountains"),
	_T("Quarry"),
	_T("Plain"),
	_T("Parking Lot"),
	_T("Sewer Pipe"),
	_T("Underwater"),
	_T("Drugged"),
	_T("Dizzy"),
	_T("Psychotic"),
	_T("Castle Small Room"),
	_T("Castle Short Passage"),
	_T("Castle Medium Room"),
	_T("Castle Large Room"),
	_T("Castle Long Passage"),
	_T("Castle Hall"),
	_T("Castle Cupboard"),
	_T("Castle Courtyard"),
	_T("Castle Alcove"),
	_T("Factory Small Room"),
	_T("Factory Short Passage"),
	_T("Factory Medium Room"),
	_T("Factory Large Room"),
	_T("Factory Long Passage"),
	_T("Factory Hall"),
	_T("Factory Cupboard"),
	_T("Factory Courtyard"),
	_T("Factory Alcove"),
	_T("Ice Palace Small Room"),
	_T("Ice Palace Short Passage"),
	_T("Ice Palace Medium Room"),
	_T("Ice Palace Large Room"),
	_T("Ice Palace Long Passage"),
	_T("Ice Palace Hall"),
	_T("Ice Palace Cupboard"),
	_T("Ice Palace Courtyard"),
	_T("Ice Palace Alcove"),
	_T("Space Station Small Room"),
	_T("Space Station Short Passage"),
	_T("Space Station Medium Room"),
	_T("Space Station Large Room"),
	_T("Space Station Long Passage"),
	_T("Space Station Hall"),
	_T("Space Station Cupboard"),
	_T("Space Station Alcove"),
	_T("Wooden Small Room"),
	_T("Wooden Short Passage"),
	_T("Wooden Medium Room"),
	_T("Wooden Large Room"),
	_T("Wooden Long Passage"),
	_T("Wooden Hall"),
	_T("Wooden Cupboard"),
	_T("Wooden Courtyard"),
	_T("Wooden Alcove"),
	_T("Sport Empty Stadium"),
	_T("Sport Squash Court"),
	_T("Sport Small Swimming Pool"),
	_T("Sport Large Swimming Pool"),
	_T("Sport Gymnasium"),
	_T("Sport Full Stadium"),
	_T("Sport Stadium Tannoy"),
	_T("Prefab Workshop"),
	_T("Prefab School Room"),
	_T("Prefab Practise Room"),
	_T("Prefab Outhouse"),
	_T("Prefab Caravan"),
	_T("Dome Tomb"),
	_T("Pipe Small"),
	_T("Dome Saint Pauls"),
	_T("Pipe Long Thin"),
	_T("Pipe Large"),
	_T("Pipe Resonant"),
	_T("Outdoors Backyard"),
	_T("Outdoors Rolling Plains"),
	_T("Outdoors Deep Canyon"),
	_T("Outdoors Creek"),
	_T("Outdoors Valley"),
	_T("Mood Heaven"),
	_T("Mood Hell"),
	_T("Mood Memory"),
	_T("Driving Commentator"),
	_T("Driving Pit Garage"),
	_T("Driving In-Car Racer"),
	_T("Driving In-Car Sports"),
	_T("Driving In-Car Luxury"),
	_T("Driving Full Grandstand"),
	_T("Driving Empty Grandstand"),
	_T("Driving Tunnel"),
	_T("City Streets"),
	_T("City Subway"),
	_T("City Museum"),
	_T("City Library"),
	_T("City Underpass"),
	_T("City Abandoned"),
	_T("Dusty Room"),
	_T("Chapel"),
	_T("Small Water Room")
};

#define NUM_REVERB_PRESETS (sizeof(ReverbPresetNames) / sizeof(ReverbPresetNames[0]))

static BOOL BrowseForConfigPath(HWND hWnd, int controlId, const TCHAR *currentPath, UINT filterId, UINT captionId, const TCHAR *ext)
{
	TCHAR filename[MAX_PATH];
	TCHAR directory[MAX_PATH];
	if (!hWnd)
	{
		return FALSE;
	}
	GetDirectoryPart(currentPath, directory, MAX_PATH);
	GetDlgItemText(hWnd, controlId, filename, MAX_PATH);
	if (PromptFileName(hWnd, FALSE, filename, MAX_PATH, filterId, captionId, ext, directory, OFN_FILEMUSTEXIST))
	{
		SetDlgItemText(hWnd, controlId, filename);
	}
	return TRUE;
}

static BOOL SetConfigFile(HWND hWnd)
{
	return BrowseForConfigPath(hWnd, IDC_CFG, g_App->configCfg.szConfigFile, IDS_CFGFLT, IDS_CFGCAP, _T("cfg"));
}

static BOOL SetDefaultInstrument(HWND hWnd)
{
	return BrowseForConfigPath(hWnd, IDC_DEFINST, g_App->configCfg.szDefaultInstrument, IDS_INSTFLT, IDS_INSTCAP, _T("pat"));
}

static BOOL HandleChannelDialog(HWND hWnd, UINT message, WPARAM wParam, UINT baseId, int *channels)
{
	UINT i;
	switch (message)
	{
	case WM_INITDIALOG:
		for (i = 0; i < 16; i++)
		{
			if (*channels & (1<<i))
			{
				CheckDlgButton(hWnd, baseId+i, BST_CHECKED);
			}
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			*channels = 0;
			for (i = 0; i < 16; i++)
			{
				if (IsDlgButtonChecked(hWnd, baseId+i))
				{
					*channels |= (1<<i);
				}
			}
			EndDialog(hWnd, TRUE);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWnd, FALSE);
			return TRUE;
		}
	}
	return FALSE;
}

static BOOL WINAPI ConfigDrumDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	return HandleChannelDialog(hWnd, message, wParam, IDC_DCHAN01, &g_App->configCfg.nDrumChannels);
}

static BOOL WINAPI ConfigQuietDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	return HandleChannelDialog(hWnd, message, wParam, IDC_QCHAN01, &g_App->configCfg.nQuietChannels);
}

BOOL WINAPI ConfigDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	UINT i;
	(void)lParam;
	switch (message)
	{
	case WM_INITDIALOG:
		InitDriverConfigDefaults(&g_App->configCfg);
		ReadRegistry(&g_App->configCfg);
		if (g_App->synth)
		{
			char szAnsi[MAX_PATH];
			int j;
			ZeroMemory(szAnsi, sizeof(szAnsi));
			EnterCriticalSection(&g_App->synthCS);
			g_App->configCfg.nSampleRate = timid_get_sample_rate(g_App->synth);
			g_App->configCfg.nControlRate = timid_get_control_rate(g_App->synth);
			g_App->configCfg.nVoices = timid_get_max_voices(g_App->synth);
			g_App->configCfg.nAmp = timid_get_amplification(g_App->synth);
			g_App->configCfg.fAdjustPanning = (BOOL)timid_get_immediate_panning(g_App->synth);
			g_App->configCfg.fMono = (BOOL)timid_get_mono(g_App->synth);
			g_App->configCfg.fAntialiasing = (BOOL)timid_get_antialiasing(g_App->synth);
			g_App->configCfg.fPreResample = (BOOL)timid_get_pre_resample(g_App->synth);
			g_App->configCfg.fFastDecay = (BOOL)timid_get_fast_decay(g_App->synth);
			g_App->configCfg.fDynamicLoad = (BOOL)timid_get_dynamic_instrument_load(g_App->synth);
			g_App->configCfg.nDefaultProgram = timid_get_default_program(g_App->synth);
			g_App->configCfg.fReverbEnabled = (BOOL)timid_get_reverb_enabled(g_App->synth);
			g_App->configCfg.fReverbOnly = (BOOL)timid_get_reverb_only(g_App->synth);
			g_App->configCfg.nReverbLevel = timid_get_reverb_level(g_App->synth);
			g_App->configCfg.nReverbPreset = timid_get_reverb_preset(g_App->synth);
			g_App->configCfg.fChorusEnabled = (BOOL)timid_get_chorus_enabled(g_App->synth);
			g_App->configCfg.nChorusDepth = timid_get_chorus_depth(g_App->synth);
			g_App->configCfg.fDitherEnabled = (BOOL)timid_get_dither_enabled(g_App->synth);
			if (timid_get_config_name(g_App->synth, szAnsi, MAX_PATH))
			{
				AnsiToTChar(szAnsi, g_App->configCfg.szConfigFile, MAX_PATH);
			}
			g_App->configCfg.nDrumChannels = 0;
			g_App->configCfg.nQuietChannels = 0;
			for (j = 0; j < 16; j++)
			{
				if (timid_get_drum_channel_enabled(g_App->synth, j))
				{
					g_App->configCfg.nDrumChannels |= (1<<j);
				}
				if (timid_get_quiet_channel_enabled(g_App->synth, j))
				{
					g_App->configCfg.nQuietChannels |= (1<<j);
				}
			}
			LeaveCriticalSection(&g_App->synthCS);
		}
		SendDlgItemMessage(hWnd, IDC_CTRATES, UDM_SETRANGE32, g_App->configCfg.nSampleRate/MAX_CONTROL_RATIO, g_App->configCfg.nSampleRate);
		SendDlgItemMessage(hWnd, IDC_VOICESS, UDM_SETRANGE32, 1, MAX_VOICES);
		SendDlgItemMessage(hWnd, IDC_AMPS, UDM_SETRANGE32, 0, MAX_AMPLIFICATION);
		SendDlgItemMessage(hWnd, IDC_DEFPROGS, UDM_SETRANGE32, 0, 127);
		SendDlgItemMessage(hWnd, IDC_REVERBLEVEL, TBM_SETRANGE, 0, MAKELONG(0, 100));
		SendDlgItemMessage(hWnd, IDC_REVERBLEVEL, TBM_SETPAGESIZE, 0, 10);
		SendDlgItemMessage(hWnd, IDC_CHORUSDEPTH, TBM_SETRANGE, 0, MAKELONG(0, 100));
		SendDlgItemMessage(hWnd, IDC_CHORUSDEPTH, TBM_SETPAGESIZE, 0, 10);
		for (i = 0; i < NUM_REVERB_PRESETS; i++)
		{
			SendDlgItemMessage(hWnd, IDC_REVERBPRESET, CB_ADDSTRING, 0, (LPARAM)ReverbPresetNames[i]);
		}
		SetDlgItemText(hWnd, IDC_CFG, g_App->configCfg.szConfigFile);
		SetDlgItemInt(hWnd, IDC_SAMPRATE, g_App->configCfg.nSampleRate, FALSE);
		SendDlgItemMessage(hWnd, IDC_CTRATES, UDM_SETPOS32, 0, g_App->configCfg.nControlRate);
		SendDlgItemMessage(hWnd, IDC_VOICESS, UDM_SETPOS32, 0, g_App->configCfg.nVoices);
		SendDlgItemMessage(hWnd, IDC_AMPS, UDM_SETPOS32, 0, g_App->configCfg.nAmp);
		if (g_App->configCfg.fAdjustPanning)
		{
			CheckDlgButton(hWnd, IDC_PAN, BST_CHECKED);
		}
		if (g_App->configCfg.fMono)
		{
			CheckDlgButton(hWnd, IDC_MONO, BST_CHECKED);
		}
		if (g_App->configCfg.f8Bit)
		{
			CheckDlgButton(hWnd, IDC_8BIT, BST_CHECKED);
		}
		if (g_App->configCfg.fAntialiasing)
		{
			CheckDlgButton(hWnd, IDC_ANTI, BST_CHECKED);
		}
		if (g_App->configCfg.fPreResample)
		{
			CheckDlgButton(hWnd, IDC_PRERES, BST_CHECKED);
		}
		if (g_App->configCfg.fFastDecay)
		{
			CheckDlgButton(hWnd, IDC_FASTDEC, BST_CHECKED);
		}
		if (g_App->configCfg.fDynamicLoad)
		{
			CheckDlgButton(hWnd, IDC_DYNALOAD, BST_CHECKED);
		}
		SetDlgItemText(hWnd, IDC_DEFINST, g_App->configCfg.szDefaultInstrument);
		SendDlgItemMessage(hWnd, IDC_DEFPROGS, UDM_SETPOS32, 0, g_App->configCfg.nDefaultProgram);
		if (g_App->configCfg.fReverbEnabled)
		{
			CheckDlgButton(hWnd, IDC_REVERBENABLED, BST_CHECKED);
		}
		if (g_App->configCfg.fReverbOnly)
		{
			CheckDlgButton(hWnd, IDC_REVERBONLY, BST_CHECKED);
		}
		SendDlgItemMessage(hWnd, IDC_REVERBLEVEL, TBM_SETPOS, TRUE, (LPARAM)g_App->configCfg.nReverbLevel);
		SendDlgItemMessage(hWnd, IDC_REVERBPRESET, CB_SETCURSEL, g_App->configCfg.nReverbPreset, 0);
		if (g_App->configCfg.fChorusEnabled)
		{
			CheckDlgButton(hWnd, IDC_CHORUSENABLED, BST_CHECKED);
		}
		SendDlgItemMessage(hWnd, IDC_CHORUSDEPTH, TBM_SETPOS, TRUE, (LPARAM)g_App->configCfg.nChorusDepth);
		if (g_App->configCfg.fDitherEnabled)
		{
			CheckDlgButton(hWnd, IDC_DITHERENABLED, BST_CHECKED);
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_CFGB:
			return SetConfigFile(hWnd);
		case IDC_SAMPRATE:
			switch (HIWORD(wParam))
			{
			case EN_KILLFOCUS:
				{
					int rate;
					int ctrlPos;
					ClampDlgItemInt(hWnd, IDC_SAMPRATE, MIN_OUTPUT_RATE, MAX_OUTPUT_RATE);
					rate = GetDlgItemInt(hWnd, IDC_SAMPRATE, NULL, FALSE);
					SendDlgItemMessage(hWnd, IDC_CTRATES, UDM_SETRANGE32, rate/MAX_CONTROL_RATIO, rate);
					ctrlPos = (int)SendDlgItemMessage(hWnd, IDC_CTRATES, UDM_GETPOS32, 0, 0);
					if (ctrlPos > rate)
					{
						SendDlgItemMessage(hWnd, IDC_CTRATES, UDM_SETPOS32, 0, rate);
					}
					else if (ctrlPos < rate/MAX_CONTROL_RATIO)
					{
						SendDlgItemMessage(hWnd, IDC_CTRATES, UDM_SETPOS32, 0, rate/MAX_CONTROL_RATIO);
					}
				}
				return TRUE;
			default:
				return FALSE;
			}
		case IDC_DEFINSTB:
			return SetDefaultInstrument(hWnd);
		case IDC_DRUMCHANNELS:
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_DRUMDLG), hWnd, (DLGPROC)ConfigDrumDialogProc);
			return TRUE;
		case IDC_QUIETCHANNELS:
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_QUIETDLG), hWnd, (DLGPROC)ConfigQuietDialogProc);
			return TRUE;
		case IDOK:
		case IDC_APPLY:
			GetDlgItemText(hWnd, IDC_CFG, g_App->configCfg.szConfigFile, MAX_PATH);
			g_App->configCfg.nSampleRate = ClampInt(GetDlgItemInt(hWnd, IDC_SAMPRATE, NULL, FALSE), MIN_OUTPUT_RATE, MAX_OUTPUT_RATE);
			g_App->configCfg.nControlRate = ClampInt((int)SendDlgItemMessage(hWnd, IDC_CTRATES, UDM_GETPOS32, 0, 0), g_App->configCfg.nSampleRate/MAX_CONTROL_RATIO, g_App->configCfg.nSampleRate);
			g_App->configCfg.nVoices = ClampInt((int)SendDlgItemMessage(hWnd, IDC_VOICESS, UDM_GETPOS32, 0, 0), 1, MAX_VOICES);
			g_App->configCfg.nAmp = ClampInt((int)SendDlgItemMessage(hWnd, IDC_AMPS, UDM_GETPOS32, 0, 0), 0, MAX_AMPLIFICATION);
			g_App->configCfg.fAdjustPanning = (IsDlgButtonChecked(hWnd, IDC_PAN) == BST_CHECKED);
			g_App->configCfg.fMono = (IsDlgButtonChecked(hWnd, IDC_MONO) == BST_CHECKED);
			g_App->configCfg.f8Bit = (IsDlgButtonChecked(hWnd, IDC_8BIT) == BST_CHECKED);
			g_App->configCfg.fAntialiasing = (IsDlgButtonChecked(hWnd, IDC_ANTI) == BST_CHECKED);
			g_App->configCfg.fPreResample = (IsDlgButtonChecked(hWnd, IDC_PRERES) == BST_CHECKED);
			g_App->configCfg.fFastDecay = (IsDlgButtonChecked(hWnd, IDC_FASTDEC) == BST_CHECKED);
			g_App->configCfg.fDynamicLoad = (IsDlgButtonChecked(hWnd, IDC_DYNALOAD) == BST_CHECKED);
			GetDlgItemText(hWnd, IDC_DEFINST, g_App->configCfg.szDefaultInstrument, MAX_PATH);
			g_App->configCfg.nDefaultProgram = ClampInt((int)SendDlgItemMessage(hWnd, IDC_DEFPROGS, UDM_GETPOS32, 0, 0), 0, 127);
			g_App->configCfg.fReverbEnabled = (IsDlgButtonChecked(hWnd, IDC_REVERBENABLED) == BST_CHECKED);
			g_App->configCfg.fReverbOnly = (IsDlgButtonChecked(hWnd, IDC_REVERBONLY) == BST_CHECKED);
			g_App->configCfg.nReverbLevel = (int)SendDlgItemMessage(hWnd, IDC_REVERBLEVEL, TBM_GETPOS, 0, 0);
			g_App->configCfg.nReverbPreset = (int)SendDlgItemMessage(hWnd, IDC_REVERBPRESET, CB_GETCURSEL, 0, 0);
			g_App->configCfg.fChorusEnabled = (IsDlgButtonChecked(hWnd, IDC_CHORUSENABLED) == BST_CHECKED);
			g_App->configCfg.nChorusDepth = (int)SendDlgItemMessage(hWnd, IDC_CHORUSDEPTH, TBM_GETPOS, 0, 0);
			g_App->configCfg.fDitherEnabled = (IsDlgButtonChecked(hWnd, IDC_DITHERENABLED) == BST_CHECKED);
			WriteRegistry(&g_App->configCfg);
			g_App->bConfigDirty = TRUE;
			RefreshSynth();
			UpdateMenuState(g_App->hPlayerWnd);
			if (LOWORD(wParam) == IDOK)
			{
				EndDialog(hWnd, TRUE);
			}
			return TRUE;
		case IDCANCEL:
			EndDialog(hWnd, FALSE);
			return TRUE;
		}
	}
	return FALSE;
}
