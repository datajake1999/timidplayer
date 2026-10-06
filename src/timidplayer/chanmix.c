#include "timidplayer.h"

#define CHANMIX_TIMER_ID 1
#define CHANMIX_PITCH_CENTER 8192

typedef void (*ChanMixApplyFn)(Timid *tm, unsigned char channel, int value);
typedef int (*ChanMixReadFn)(Timid *tm, int channel);

typedef struct {
	int trackbarId;
	int valueTextId;
	int rangeMin;
	int rangeMax;
	ChanMixApplyFn apply;
	ChanMixReadFn read;
	BOOL signedCentered;
} ChanMixSlider;

static void ApplyVolume(Timid *tm, unsigned char ch, int value)
{
	timid_channel_set_volume(tm, ch, (unsigned char)value);
}

static int ReadVolume(Timid *tm, int ch)
{
	return timid_channel_get_volume(tm, ch);
}

static void ApplyExpression(Timid *tm, unsigned char ch, int value)
{
	timid_channel_set_expression(tm, ch, (unsigned char)value);
}

static int ReadExpression(Timid *tm, int ch)
{
	return timid_channel_get_expression(tm, ch);
}

static void ApplyPan(Timid *tm, unsigned char ch, int value)
{
	timid_channel_set_pan(tm, ch, (unsigned char)value);
}

static int ReadPan(Timid *tm, int ch)
{
	return timid_channel_get_pan(tm, ch);
}

static void ApplyPitchBend(Timid *tm, unsigned char ch, int value)
{
	timid_channel_set_pitch_wheel(tm, ch, (unsigned short)value);
}

static int ReadPitchBend(Timid *tm, int ch)
{
	return timid_channel_get_pitch_wheel(tm, ch);
}

static void ApplyPitchRange(Timid *tm, unsigned char ch, int value)
{
	timid_channel_set_pitch_range(tm, ch, (unsigned char)value);
}

static int ReadPitchRange(Timid *tm, int ch)
{
	return timid_channel_get_pitch_range(tm, ch);
}

static void ApplyModulation(Timid *tm, unsigned char ch, int value)
{
	timid_channel_set_modulation(tm, ch, (unsigned char)value);
}

static int ReadModulation(Timid *tm, int ch)
{
	return timid_channel_get_modulation(tm, ch);
}

static void ApplyReverb(Timid *tm, unsigned char ch, int value)
{
	timid_channel_set_reverb(tm, ch, (unsigned char)value);
}

static int ReadReverb(Timid *tm, int ch)
{
	return timid_channel_get_reverb(tm, ch);
}

static void ApplyChorus(Timid *tm, unsigned char ch, int value)
{
	timid_channel_set_chorus(tm, ch, (unsigned char)value);
}

static int ReadChorus(Timid *tm, int ch)
{
	return timid_channel_get_chorus(tm, ch);
}

static const ChanMixSlider s_sliders[] =
{
	{ IDC_CHANMIX_VOLUME, IDC_CHANMIX_VOLUMETXT, 0, 127, ApplyVolume, ReadVolume, FALSE },
	{ IDC_CHANMIX_EXPRESSION, IDC_CHANMIX_EXPRESSIONTXT, 0, 127, ApplyExpression, ReadExpression, FALSE },
	{ IDC_CHANMIX_PAN, IDC_CHANMIX_PANTXT, 0, 127, ApplyPan, ReadPan, FALSE },
	{ IDC_CHANMIX_PITCHBEND, IDC_CHANMIX_PITCHBENDTXT, 0, 16383, ApplyPitchBend, ReadPitchBend, TRUE },
	{ IDC_CHANMIX_PITCHRANGE, IDC_CHANMIX_PITCHRANGETXT, 0, 24, ApplyPitchRange, ReadPitchRange, FALSE },
	{ IDC_CHANMIX_MODULATION, IDC_CHANMIX_MODULATIONTXT, 0, 127, ApplyModulation, ReadModulation, FALSE },
	{ IDC_CHANMIX_REVERB, IDC_CHANMIX_REVERBTXT, 0, 127, ApplyReverb, ReadReverb, FALSE },
	{ IDC_CHANMIX_CHORUS, IDC_CHANMIX_CHORUSTXT, 0, 127, ApplyChorus, ReadChorus, FALSE }
};

static const TCHAR *GMInstrumentName(int program)
{
	static const TCHAR *names[128] =
	{
		_T("Acoustic Grand Piano"), _T("Bright Acoustic Piano"), _T("Electric Grand Piano"), _T("Honky-tonk Piano"),
		_T("Electric Piano 1"), _T("Electric Piano 2"), _T("Harpsichord"), _T("Clavinet"),
		_T("Celesta"), _T("Glockenspiel"), _T("Music Box"), _T("Vibraphone"),
		_T("Marimba"), _T("Xylophone"), _T("Tubular Bells"), _T("Dulcimer"),
		_T("Drawbar Organ"), _T("Percussive Organ"), _T("Rock Organ"), _T("Church Organ"),
		_T("Reed Organ"), _T("Accordion"), _T("Harmonica"), _T("Tango Accordion"),
		_T("Acoustic Guitar (nylon)"), _T("Acoustic Guitar (steel)"), _T("Electric Guitar (jazz)"), _T("Electric Guitar (clean)"),
		_T("Electric Guitar (muted)"), _T("Overdriven Guitar"), _T("Distortion Guitar"), _T("Guitar Harmonics"),
		_T("Acoustic Bass"), _T("Electric Bass (finger)"), _T("Electric Bass (pick)"), _T("Fretless Bass"),
		_T("Slap Bass 1"), _T("Slap Bass 2"), _T("Synth Bass 1"), _T("Synth Bass 2"),
		_T("Violin"), _T("Viola"), _T("Cello"), _T("Contrabass"),
		_T("Tremolo Strings"), _T("Pizzicato Strings"), _T("Orchestral Harp"), _T("Timpani"),
		_T("String Ensemble 1"), _T("String Ensemble 2"), _T("Synth Strings 1"), _T("Synth Strings 2"),
		_T("Choir Aahs"), _T("Voice Oohs"), _T("Synth Voice"), _T("Orchestra Hit"),
		_T("Trumpet"), _T("Trombone"), _T("Tuba"), _T("Muted Trumpet"),
		_T("French Horn"), _T("Brass Section"), _T("Synth Brass 1"), _T("Synth Brass 2"),
		_T("Soprano Sax"), _T("Alto Sax"), _T("Tenor Sax"), _T("Baritone Sax"),
		_T("Oboe"), _T("English Horn"), _T("Bassoon"), _T("Clarinet"),
		_T("Piccolo"), _T("Flute"), _T("Recorder"), _T("Pan Flute"),
		_T("Blown Bottle"), _T("Shakuhachi"), _T("Whistle"), _T("Ocarina"),
		_T("Lead 1 (square)"), _T("Lead 2 (sawtooth)"), _T("Lead 3 (calliope)"), _T("Lead 4 (chiff)"),
		_T("Lead 5 (charang)"), _T("Lead 6 (voice)"), _T("Lead 7 (fifths)"), _T("Lead 8 (bass + lead)"),
		_T("Pad 1 (new age)"), _T("Pad 2 (warm)"), _T("Pad 3 (polysynth)"), _T("Pad 4 (choir)"),
		_T("Pad 5 (bowed)"), _T("Pad 6 (metallic)"), _T("Pad 7 (halo)"), _T("Pad 8 (sweep)"),
		_T("FX 1 (rain)"), _T("FX 2 (soundtrack)"), _T("FX 3 (crystal)"), _T("FX 4 (atmosphere)"),
		_T("FX 5 (brightness)"), _T("FX 6 (goblins)"), _T("FX 7 (echoes)"), _T("FX 8 (sci-fi)"),
		_T("Sitar"), _T("Banjo"), _T("Shamisen"), _T("Koto"),
		_T("Kalimba"), _T("Bagpipe"), _T("Fiddle"), _T("Shanai"),
		_T("Tinkle Bell"), _T("Agogo"), _T("Steel Drums"), _T("Woodblock"),
		_T("Taiko Drum"), _T("Melodic Tom"), _T("Synth Drum"), _T("Reverse Cymbal"),
		_T("Guitar Fret Noise"), _T("Breath Noise"), _T("Seashore"), _T("Bird Tweet"),
		_T("Telephone Ring"), _T("Helicopter"), _T("Applause"), _T("Gunshot")
	};
	if (program < 0 || program > 127)
	{
		return _T("");
	}
	return names[program];
}

static void PopulateInstrumentCombo(HWND hCombo, BOOL isDrum)
{
	int i;
	TCHAR buf[40];
	for (i = 0; i < 128; i++)
	{
		if (isDrum)
		{
			SafeFormat(buf, 40, _T("Drum Kit %d"), i);
			SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)buf);
		}
		else
		{
			SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)GMInstrumentName(i));
		}
	}
}

static void PopulateBankCombo(HWND hCombo)
{
	int i;
	TCHAR buf[8];
	for (i = 0; i < 128; i++)
	{
		SafeFormat(buf, 8, _T("%d"), i);
		SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)buf);
	}
}

static void HandleComboNotify(WPARAM wParam, LPARAM lParam, int ch, BOOL *openFlag, void (*setter)(Timid *, unsigned char, unsigned char))
{
	int sel;
	if (HIWORD(wParam) == CBN_DROPDOWN)
	{
		*openFlag = TRUE;
	}
	else if (HIWORD(wParam) == CBN_CLOSEUP)
	{
		*openFlag = FALSE;
	}
	else if (HIWORD(wParam) == CBN_SELCHANGE)
	{
		sel = (int)SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
		if (sel >= 0 && g_App->synth)
		{
			EnterCriticalSection(&g_App->synthCS);
			setter(g_App->synth, (unsigned char)ch, (unsigned char)sel);
			LeaveCriticalSection(&g_App->synthCS);
		}
	}
}

static void ApplyDrumState(HWND hWnd, BOOL isDrum)
{
	HWND hInstr;
	g_App->chanMixIsDrum = isDrum;
	hInstr = GetDlgItem(hWnd, IDC_CHANMIX_INSTR);
	SendMessage(hInstr, CB_RESETCONTENT, 0, 0);
	PopulateInstrumentCombo(hInstr, g_App->chanMixIsDrum);
	EnableWindow(GetDlgItem(hWnd, IDC_CHANMIX_BANK), !g_App->chanMixIsDrum);
}

static void SetCheckState(HWND hWnd, int id, BOOL checked)
{
	if (checked)
	{
		CheckDlgButton(hWnd, id, BST_CHECKED);
	}
	else
	{
		CheckDlgButton(hWnd, id, BST_UNCHECKED);
	}
}

static void UpdateSliderDisplay(HWND hWnd, unsigned int i, int rawValue)
{
	int display;
	if (s_sliders[i].signedCentered)
	{
		display = rawValue - CHANMIX_PITCH_CENTER;
	}
	else
	{
		display = rawValue;
	}
	SetDlgItemIntText(hWnd, s_sliders[i].valueTextId, display);
}

static void InitChannelMixer(HWND hWnd)
{
	int i;
	PopulateChannelCombo(GetDlgItem(hWnd, IDC_CHANMIX_CHANNEL));
	SendDlgItemMessage(hWnd, IDC_CHANMIX_CHANNEL, CB_SETCURSEL, 0, 0);
	g_App->chanMixActiveChannel = 0;
	ApplyDrumState(hWnd, (BOOL)timid_get_drum_channel_enabled(g_App->synth, g_App->chanMixActiveChannel));
	PopulateBankCombo(GetDlgItem(hWnd, IDC_CHANMIX_BANK));
	for (i = 0; i < (int)CHANMIX_SLIDER_COUNT; i++)
	{
		SendDlgItemMessage(hWnd, s_sliders[i].trackbarId, TBM_SETRANGE, TRUE, MAKELONG(s_sliders[i].rangeMin, s_sliders[i].rangeMax));
		g_App->chanMixDragging[i] = FALSE;
	}
}

static void RefreshChannelMixer(HWND hWnd)
{
	Timid *tm;
	int program;
	int bank;
	int quiet;
	int sustain;
	int mono;
	int isDrum;
	unsigned int i;
	int value;
	tm = g_App->synth;
	if (!tm)
	{
		return;
	}
	EnterCriticalSection(&g_App->synthCS);
	program = timid_channel_get_program(tm, g_App->chanMixActiveChannel);
	bank = timid_channel_get_bank(tm, g_App->chanMixActiveChannel);
	quiet = timid_get_quiet_channel_enabled(tm, g_App->chanMixActiveChannel);
	sustain = timid_channel_get_sustain(tm, g_App->chanMixActiveChannel);
	mono = timid_channel_get_mono(tm, g_App->chanMixActiveChannel);
	isDrum = timid_get_drum_channel_enabled(tm, g_App->chanMixActiveChannel);
	LeaveCriticalSection(&g_App->synthCS);
	if ((BOOL)isDrum != g_App->chanMixIsDrum)
	{
		ApplyDrumState(hWnd, (BOOL)isDrum);
	}
	if (!g_App->chanMixInstrComboOpen)
	{
		SendDlgItemMessage(hWnd, IDC_CHANMIX_INSTR, CB_SETCURSEL, (WPARAM)ClampInt(program, 0, 127), 0);
	}
	if (!g_App->chanMixIsDrum && !g_App->chanMixBankComboOpen)
	{
		SendDlgItemMessage(hWnd, IDC_CHANMIX_BANK, CB_SETCURSEL, (WPARAM)ClampInt(bank, 0, 127), 0);
	}
	SetCheckState(hWnd, IDC_CHANMIX_DRUM, g_App->chanMixIsDrum);
	SetCheckState(hWnd, IDC_CHANMIX_MUTE, quiet);
	SetCheckState(hWnd, IDC_CHANMIX_SUSTAIN, sustain >= 64);
	SetCheckState(hWnd, IDC_CHANMIX_MONO, mono);
	{
		int values[CHANMIX_SLIDER_COUNT];
		EnterCriticalSection(&g_App->synthCS);
		for (i = 0; i < CHANMIX_SLIDER_COUNT; i++)
		{
			if (!g_App->chanMixDragging[i])
			{
				values[i] = s_sliders[i].read(tm, g_App->chanMixActiveChannel);
			}
		}
		LeaveCriticalSection(&g_App->synthCS);
		for (i = 0; i < CHANMIX_SLIDER_COUNT; i++)
		{
			if (g_App->chanMixDragging[i])
			{
				continue;
			}
			value = ClampInt(values[i], s_sliders[i].rangeMin, s_sliders[i].rangeMax);
			SendDlgItemMessage(hWnd, s_sliders[i].trackbarId, TBM_SETPOS, TRUE, value);
			UpdateSliderDisplay(hWnd, i, value);
		}
	}
}

static void SwitchActiveChannel(HWND hWnd, int newChannel)
{
	BOOL newIsDrum;
	g_App->chanMixActiveChannel = newChannel;
	newIsDrum = (BOOL)timid_get_drum_channel_enabled(g_App->synth, g_App->chanMixActiveChannel);
	if (newIsDrum != g_App->chanMixIsDrum)
	{
		ApplyDrumState(hWnd, newIsDrum);
	}
	RefreshChannelMixer(hWnd);
}

static void RunChannelAction(void (*fn)(Timid *, unsigned char))
{
	EnterCriticalSection(&g_App->synthCS);
	fn(g_App->synth, (unsigned char)g_App->chanMixActiveChannel);
	LeaveCriticalSection(&g_App->synthCS);
}

BOOL WINAPI ChannelMixerDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int id;
	unsigned int i;
	HWND hCtrl;
	int pos;
	int sel;
	BOOL checked;
	BOOL found;
	switch (message)
	{
	case WM_INITDIALOG:
		InitChannelMixer(hWnd);
		RefreshChannelMixer(hWnd);
		SetTimer(hWnd, CHANMIX_TIMER_ID, 200, NULL);
		return TRUE;
	case WM_TIMER:
		if (wParam == CHANMIX_TIMER_ID)
		{
			RefreshChannelMixer(hWnd);
		}
		return TRUE;
	case WM_HSCROLL:
		hCtrl = (HWND)lParam;
		if (!hCtrl || !g_App->synth)
		{
			return TRUE;
		}
		id = GetDlgCtrlID(hCtrl);
		found = FALSE;
		for (i = 0; i < CHANMIX_SLIDER_COUNT && !found; i++)
		{
			if (id == s_sliders[i].trackbarId)
			{
				found = TRUE;
				pos = (int)SendMessage(hCtrl, TBM_GETPOS, 0, 0);
				EnterCriticalSection(&g_App->synthCS);
				s_sliders[i].apply(g_App->synth, (unsigned char)g_App->chanMixActiveChannel, pos);
				LeaveCriticalSection(&g_App->synthCS);
				UpdateSliderDisplay(hWnd, i, pos);
				g_App->chanMixDragging[i] = (LOWORD(wParam) == TB_THUMBTRACK);
			}
		}
		return TRUE;
	case WM_COMMAND:
		id = LOWORD(wParam);
		switch (id)
		{
		case IDOK:
		case IDCANCEL:
			EndDialog(hWnd, TRUE);
			return TRUE;
		case IDC_CHANMIX_CHANNEL:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				sel = (int)SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
				if (sel >= 0)
				{
					SwitchActiveChannel(hWnd, sel);
				}
			}
			return TRUE;
		case IDC_CHANMIX_INSTR:
			HandleComboNotify(wParam, lParam, g_App->chanMixActiveChannel, &g_App->chanMixInstrComboOpen, timid_channel_set_program);
			return TRUE;
		case IDC_CHANMIX_BANK:
			HandleComboNotify(wParam, lParam, g_App->chanMixActiveChannel, &g_App->chanMixBankComboOpen, timid_channel_set_bank);
			return TRUE;
		}
		if (HIWORD(wParam) != BN_CLICKED || !g_App->synth)
		{
			return TRUE;
		}
		checked = (IsDlgButtonChecked(hWnd, id) == BST_CHECKED);
		switch (id)
		{
		case IDC_CHANMIX_MUTE:
			EnterCriticalSection(&g_App->synthCS);
			timid_set_quiet_channel(g_App->synth, g_App->chanMixActiveChannel, checked);
			LeaveCriticalSection(&g_App->synthCS);
			return TRUE;
		case IDC_CHANMIX_DRUM:
			EnterCriticalSection(&g_App->synthCS);
			timid_set_drum_channel(g_App->synth, g_App->chanMixActiveChannel, checked);
			LeaveCriticalSection(&g_App->synthCS);
			ApplyDrumState(hWnd, checked);
			return TRUE;
		case IDC_CHANMIX_SUSTAIN:
			EnterCriticalSection(&g_App->synthCS);
			if (checked)
			{
				timid_channel_set_sustain(g_App->synth, (unsigned char)g_App->chanMixActiveChannel, 127);
			}
			else
			{
				timid_channel_set_sustain(g_App->synth, (unsigned char)g_App->chanMixActiveChannel, 0);
			}
			LeaveCriticalSection(&g_App->synthCS);
			return TRUE;
		case IDC_CHANMIX_MONO:
			EnterCriticalSection(&g_App->synthCS);
			if (checked)
			{
				timid_channel_mono_mode(g_App->synth, (unsigned char)g_App->chanMixActiveChannel);
			}
			else
			{
				timid_channel_poly_mode(g_App->synth, (unsigned char)g_App->chanMixActiveChannel);
			}
			LeaveCriticalSection(&g_App->synthCS);
			return TRUE;
		case IDC_CHANMIX_NOTESOFF:
			RunChannelAction(timid_channel_all_notes_off);
			return TRUE;
		case IDC_CHANMIX_SOUNDSOFF:
			RunChannelAction(timid_channel_all_sounds_off);
			return TRUE;
		case IDC_CHANMIX_RESETCTRL:
			RunChannelAction(timid_channel_reset_controllers);
			return TRUE;
		}
		break;
	case WM_DESTROY:
		KillTimer(hWnd, CHANMIX_TIMER_ID);
		return TRUE;
	}
	return FALSE;
}
