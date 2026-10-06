#include "timidplayer.h"

#ifdef _WIN64
#define VKBD_SETWNDPROC(hwnd, proc) ((WNDPROC)SetWindowLongPtr((hwnd), GWLP_WNDPROC, (LONG_PTR)(proc)))
#define VKBD_SETUSERDATA(hwnd, val) SetWindowLongPtr((hwnd), GWLP_USERDATA, (LONG_PTR)(val))
#define VKBD_GETUSERDATA(hwnd) ((int)GetWindowLongPtr((hwnd), GWLP_USERDATA))
#else
#define VKBD_SETWNDPROC(hwnd, proc) ((WNDPROC)SetWindowLong((hwnd), GWL_WNDPROC, (LONG)(proc)))
#define VKBD_SETUSERDATA(hwnd, val) SetWindowLong((hwnd), GWL_USERDATA, (LONG)(val))
#define VKBD_GETUSERDATA(hwnd) ((int)GetWindowLong((hwnd), GWL_USERDATA))
#endif

#define VKBD_KEY_COUNT 24

#define VKBD_KEY_X 8
#define VKBD_KEY_Y 82
#define VKBD_WHITE_W 18
#define VKBD_WHITE_H 70
#define VKBD_BLACK_W 12
#define VKBD_BLACK_H 42

#define VKBD_BASE_MIN 0
#define VKBD_BASE_MAX 104
#define VKBD_BASE_DEFAULT 48
#define VKBD_BASE_STEP 12

#define VKBD_VELOCITY_MIN 1
#define VKBD_VELOCITY_MAX 127
#define VKBD_VELOCITY_DEFAULT 100

#define VKBD_PRESSURE_MIN 0
#define VKBD_PRESSURE_MAX 127
#define VKBD_PRESSURE_DEFAULT 0

static const TCHAR *s_noteNames[12] =
{
	_T("C"), _T("C#"), _T("D"), _T("D#"), _T("E"), _T("F"),
	_T("F#"), _T("G"), _T("G#"), _T("A"), _T("A#"), _T("B")
};

static const int s_whiteOffsets[7] = { 0, 2, 4, 5, 7, 9, 11 };
static const BOOL s_whiteHasBlackAfter[7] = { TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, FALSE };

static void MidiNoteName(int note, TCHAR *buf, int bufChars)
{
	int octave;
	octave = (note / 12) - 1;
	SafeFormat(buf, bufChars, _T("%s%d"), s_noteNames[note % 12], octave);
}

static void PressVirtualKey(int note)
{
	if (note < 0 || note > 127 || g_App->vkbdNoteDown[note] || !g_App->synth)
	{
		return;
	}
	EnterCriticalSection(&g_App->synthCS);
	timid_channel_note_on(g_App->synth, (unsigned char)g_App->vkbdActiveChannel, (unsigned char)note, (unsigned char)g_App->vkbdVelocity);
	LeaveCriticalSection(&g_App->synthCS);
	g_App->vkbdNoteDown[note] = TRUE;
	g_App->vkbdPressureNote = note;
}

static void ReleaseVirtualKey(int note)
{
	if (note < 0 || note > 127 || !g_App->vkbdNoteDown[note])
	{
		return;
	}
	if (g_App->synth)
	{
		EnterCriticalSection(&g_App->synthCS);
		timid_channel_note_off(g_App->synth, (unsigned char)g_App->vkbdActiveChannel, (unsigned char)note);
		LeaveCriticalSection(&g_App->synthCS);
	}
	g_App->vkbdNoteDown[note] = FALSE;
	if (g_App->vkbdPressureNote == note)
	{
		g_App->vkbdPressureNote = -1;
	}
}

static void ReleaseAllHeldNotes(void)
{
	int note;
	for (note = 0; note < 128; note++)
	{
		if (g_App->vkbdNoteDown[note])
		{
			ReleaseVirtualKey(note);
		}
	}
}

static void PressKeyVisual(HWND hWnd, int note)
{
	SendMessage(hWnd, BM_SETSTATE, TRUE, 0);
	PressVirtualKey(note);
}

static void ReleaseKeyVisual(HWND hWnd, int note)
{
	SendMessage(hWnd, BM_SETSTATE, FALSE, 0);
	ReleaseVirtualKey(note);
}

static void MoveKeyFocus(HWND hWnd, int note, int delta)
{
	int target;
	HWND hNeighbor;
	target = note + delta;
	if (target < g_App->vkbdBaseNote || target > g_App->vkbdBaseNote + VKBD_KEY_COUNT - 1)
	{
		return;
	}
	hNeighbor = GetDlgItem(GetParent(hWnd), IDC_VKBD_KEYBASE + (target - g_App->vkbdBaseNote));
	if (hNeighbor)
	{
		SetFocus(hNeighbor);
	}
}

static LRESULT CALLBACK KeyButtonProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int note;
	switch (message)
	{
	case WM_GETDLGCODE:
		return CallWindowProc(g_App->vkbdDefButtonProc, hWnd, message, wParam, lParam) | DLGC_WANTARROWS;
	case WM_LBUTTONDOWN:
		note = VKBD_GETUSERDATA(hWnd);
		SetCapture(hWnd);
		PressKeyVisual(hWnd, note);
		return 0;
	case WM_LBUTTONUP:
		note = VKBD_GETUSERDATA(hWnd);
		if (GetCapture() == hWnd)
		{
			ReleaseCapture();
		}
		ReleaseKeyVisual(hWnd, note);
		return 0;
	case WM_CAPTURECHANGED:
		if ((HWND)lParam != hWnd)
		{
			note = VKBD_GETUSERDATA(hWnd);
			ReleaseKeyVisual(hWnd, note);
		}
		break;
	case WM_KEYDOWN:
		note = VKBD_GETUSERDATA(hWnd);
		switch (wParam)
		{
		case VK_SPACE:
			PressKeyVisual(hWnd, note);
			return 0;
		case VK_LEFT:
		case VK_UP:
			MoveKeyFocus(hWnd, note, -1);
			return 0;
		case VK_RIGHT:
		case VK_DOWN:
			MoveKeyFocus(hWnd, note, 1);
			return 0;
		}
		break;
	case WM_KEYUP:
		if (wParam == VK_SPACE)
		{
			note = VKBD_GETUSERDATA(hWnd);
			ReleaseKeyVisual(hWnd, note);
			return 0;
		}
		break;
	case WM_KILLFOCUS:
		note = VKBD_GETUSERDATA(hWnd);
		if (g_App->vkbdNoteDown[note])
		{
			ReleaseKeyVisual(hWnd, note);
		}
		break;
	}
	return CallWindowProc(g_App->vkbdDefButtonProc, hWnd, message, wParam, lParam);
}

static HWND CreateOneKey(HWND hWnd, HFONT hFont, HWND hInsertAfter, int note, int x, int y, int w, int h, BOOL tabStop)
{
	TCHAR label[8];
	HWND hKey;
	DWORD style;
	RECT rc;
	rc.left = x;
	rc.top = y;
	rc.right = x + w;
	rc.bottom = y + h;
	MapDialogRect(hWnd, &rc);
	MidiNoteName(note, label, 8);
	style = WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON;
	if (tabStop)
	{
		style |= WS_TABSTOP;
	}
	hKey = CreateWindow(_T("BUTTON"), label, style, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hWnd, (HMENU)(INT_PTR)(IDC_VKBD_KEYBASE + (note - g_App->vkbdBaseNote)), g_App->hInst, NULL);
	if (!hKey)
	{
		return hInsertAfter;
	}
	SetWindowPos(hKey, hInsertAfter, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
	SendMessage(hKey, WM_SETFONT, (WPARAM)hFont, TRUE);
	VKBD_SETUSERDATA(hKey, note);
	g_App->vkbdDefButtonProc = VKBD_SETWNDPROC(hKey, KeyButtonProc);
	return hKey;
}

static void CreateKeyboardKeys(HWND hWnd)
{
	HFONT hFont;
	HWND hAfter;
	int octave, degree, note, whiteSlot, blackX;
	hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
	hAfter = GetDlgItem(hWnd, IDC_VKBD_PRESSURETXT);
	whiteSlot = 0;
	for (octave = 0; octave < 2; octave++)
	{
		for (degree = 0; degree < 7; degree++)
		{
			note = g_App->vkbdBaseNote + octave * 12 + s_whiteOffsets[degree];
			hAfter = CreateOneKey(hWnd, hFont, hAfter, note, VKBD_KEY_X + whiteSlot * VKBD_WHITE_W, VKBD_KEY_Y, VKBD_WHITE_W, VKBD_WHITE_H, (octave == 0 && degree == 0));
			whiteSlot++;
		}
	}
	whiteSlot = 0;
	for (octave = 0; octave < 2; octave++)
	{
		for (degree = 0; degree < 7; degree++)
		{
			if (s_whiteHasBlackAfter[degree])
			{
				note = g_App->vkbdBaseNote + octave * 12 + s_whiteOffsets[degree] + 1;
				blackX = VKBD_KEY_X + (whiteSlot + 1) * VKBD_WHITE_W - VKBD_BLACK_W / 2;
				hAfter = CreateOneKey(hWnd, hFont, hAfter, note, blackX, VKBD_KEY_Y, VKBD_BLACK_W, VKBD_BLACK_H, FALSE);
			}
			whiteSlot++;
		}
	}
}

static void DestroyKeyboardKeys(HWND hWnd)
{
	int i;
	HWND hKey;
	for (i = 0; i < VKBD_KEY_COUNT; i++)
	{
		hKey = GetDlgItem(hWnd, IDC_VKBD_KEYBASE + i);
		if (hKey)
		{
			DestroyWindow(hKey);
		}
	}
}

static void RefreshRangeLabel(HWND hWnd)
{
	TCHAR lowName[8], highName[8];
	TCHAR range[32];
	MidiNoteName(g_App->vkbdBaseNote, lowName, 8);
	MidiNoteName(g_App->vkbdBaseNote + VKBD_KEY_COUNT - 1, highName, 8);
	SafeFormat(range, 32, _T("%s - %s"), lowName, highName);
	SetDlgItemText(hWnd, IDC_VKBD_RANGE, range);
	EnableWindow(GetDlgItem(hWnd, IDC_VKBD_OCTDOWN), g_App->vkbdBaseNote > VKBD_BASE_MIN);
	EnableWindow(GetDlgItem(hWnd, IDC_VKBD_OCTUP), g_App->vkbdBaseNote < VKBD_BASE_MAX);
}

static void ShiftOctave(HWND hWnd, int delta)
{
	int newBase;
	newBase = ClampInt(g_App->vkbdBaseNote + delta, VKBD_BASE_MIN, VKBD_BASE_MAX);
	if (newBase == g_App->vkbdBaseNote)
	{
		return;
	}
	ReleaseAllHeldNotes();
	DestroyKeyboardKeys(hWnd);
	g_App->vkbdBaseNote = newBase;
	CreateKeyboardKeys(hWnd);
	RefreshRangeLabel(hWnd);
}

static void InitTrackbarValue(HWND hWnd, int trackId, int minVal, int maxVal, int value, int textId)
{
	SendDlgItemMessage(hWnd, trackId, TBM_SETRANGE, TRUE, MAKELONG(minVal, maxVal));
	SendDlgItemMessage(hWnd, trackId, TBM_SETPOS, TRUE, value);
	SetDlgItemIntText(hWnd, textId, value);
}

static int UpdateHScrollValue(HWND hWnd, HWND hTrack, int minVal, int maxVal, int textId)
{
	int pos;
	pos = (int)SendMessage(hTrack, TBM_GETPOS, 0, 0);
	pos = ClampInt(pos, minVal, maxVal);
	SetDlgItemIntText(hWnd, textId, pos);
	return pos;
}

BOOL WINAPI VirtualKeyboardDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int id;
	BOOL checked;
	switch (message)
	{
	case WM_INITDIALOG:
		g_App->vkbdActiveChannel = 0;
		g_App->vkbdVelocity = VKBD_VELOCITY_DEFAULT;
		g_App->vkbdPressure = VKBD_PRESSURE_DEFAULT;
		g_App->vkbdPressureNote = -1;
		g_App->vkbdBaseNote = VKBD_BASE_DEFAULT;
		ZeroMemory(g_App->vkbdNoteDown, sizeof(g_App->vkbdNoteDown));
		PopulateChannelCombo(GetDlgItem(hWnd, IDC_VKBD_CHANNEL));
		SendDlgItemMessage(hWnd, IDC_VKBD_CHANNEL, CB_SETCURSEL, 0, 0);
		InitTrackbarValue(hWnd, IDC_VKBD_VELOCITY, VKBD_VELOCITY_MIN, VKBD_VELOCITY_MAX, g_App->vkbdVelocity, IDC_VKBD_VELOCITYTXT);
		InitTrackbarValue(hWnd, IDC_VKBD_PRESSURE, VKBD_PRESSURE_MIN, VKBD_PRESSURE_MAX, g_App->vkbdPressure, IDC_VKBD_PRESSURETXT);
		CreateKeyboardKeys(hWnd);
		RefreshRangeLabel(hWnd);
		return TRUE;
	case WM_HSCROLL:
		id = GetDlgCtrlID((HWND)lParam);
		if (id == IDC_VKBD_VELOCITY)
		{
			g_App->vkbdVelocity = UpdateHScrollValue(hWnd, (HWND)lParam, VKBD_VELOCITY_MIN, VKBD_VELOCITY_MAX, IDC_VKBD_VELOCITYTXT);
		}
		else if (id == IDC_VKBD_PRESSURE)
		{
			g_App->vkbdPressure = UpdateHScrollValue(hWnd, (HWND)lParam, VKBD_PRESSURE_MIN, VKBD_PRESSURE_MAX, IDC_VKBD_PRESSURETXT);
			if (g_App->vkbdPressureNote >= 0 && g_App->synth)
			{
				EnterCriticalSection(&g_App->synthCS);
				timid_channel_key_pressure(g_App->synth, (unsigned char)g_App->vkbdActiveChannel, (unsigned char)g_App->vkbdPressureNote, (unsigned char)g_App->vkbdPressure);
				LeaveCriticalSection(&g_App->synthCS);
			}
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_VKBD_OCTDOWN:
			ShiftOctave(hWnd, -VKBD_BASE_STEP);
			return TRUE;
		case IDC_VKBD_OCTUP:
			ShiftOctave(hWnd, VKBD_BASE_STEP);
			return TRUE;
		case IDC_VKBD_CHANNEL:
			if (HIWORD(wParam) == CBN_SELCHANGE)
			{
				ReleaseAllHeldNotes();
				g_App->vkbdActiveChannel = (int)SendDlgItemMessage(hWnd, IDC_VKBD_CHANNEL, CB_GETCURSEL, 0, 0);
				if (g_App->vkbdActiveChannel < 0)
				{
					g_App->vkbdActiveChannel = 0;
				}
			}
			return TRUE;
		case IDC_VKBD_SUSTAIN:
			if (HIWORD(wParam) == BN_CLICKED && g_App->synth)
			{
				checked = (IsDlgButtonChecked(hWnd, IDC_VKBD_SUSTAIN) == BST_CHECKED);
				EnterCriticalSection(&g_App->synthCS);
				if (checked)
				{
					timid_channel_control_change(g_App->synth, (unsigned char)g_App->vkbdActiveChannel, 64, 127);
				}
				else
				{
					timid_channel_control_change(g_App->synth, (unsigned char)g_App->vkbdActiveChannel, 64, 0);
				}
				LeaveCriticalSection(&g_App->synthCS);
			}
			return TRUE;
		case IDOK:
		case IDCANCEL:
			ReleaseAllHeldNotes();
			EndDialog(hWnd, TRUE);
			return TRUE;
		}
		break;
	case WM_DESTROY:
		ReleaseAllHeldNotes();
		return TRUE;
	}
	return FALSE;
}
