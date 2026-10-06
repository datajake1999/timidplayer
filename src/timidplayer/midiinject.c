#include "timidplayer.h"

#define INJECTMIDI_DATA_MIN 0
#define INJECTMIDI_DATA_MAX 127

typedef struct {
	UINT nameId;
	unsigned char status;
} InjectMidiEventType;

static const InjectMidiEventType s_eventTypes[] =
{
	{ IDS_INJECT_NOTEOFF, 0x80 },
	{ IDS_INJECT_NOTEON, 0x90 },
	{ IDS_INJECT_POLYAFTERTOUCH, 0xA0 },
	{ IDS_INJECT_CONTROLCHANGE, 0xB0 },
	{ IDS_INJECT_PROGRAMCHANGE, 0xC0 },
	{ IDS_INJECT_PITCHBEND, 0xE0 }
};

#define INJECTMIDI_EVENTTYPE_COUNT (sizeof(s_eventTypes) / sizeof(s_eventTypes[0]))

static void InitInjectMidiControls(HWND hWnd)
{
	int i;
	TCHAR eventName[32];
	PopulateChannelCombo(GetDlgItem(hWnd, IDC_INJECT_CHANNEL));
	SendDlgItemMessage(hWnd, IDC_INJECT_CHANNEL, CB_SETCURSEL, 0, 0);
	for (i = 0; i < INJECTMIDI_EVENTTYPE_COUNT; i++)
	{
		LoadAppString(g_App->hInst, s_eventTypes[i].nameId, eventName, 32);
		SendDlgItemMessage(hWnd, IDC_INJECT_EVENTTYPE, CB_ADDSTRING, 0, (LPARAM)eventName);
	}
	SendDlgItemMessage(hWnd, IDC_INJECT_EVENTTYPE, CB_SETCURSEL, 0, 0);
	SendDlgItemMessage(hWnd, IDC_INJECT_DATA1S, UDM_SETRANGE32, INJECTMIDI_DATA_MIN, INJECTMIDI_DATA_MAX);
	SendDlgItemMessage(hWnd, IDC_INJECT_DATA1S, UDM_SETPOS32, 0, 0);
	SendDlgItemMessage(hWnd, IDC_INJECT_DATA2S, UDM_SETRANGE32, INJECTMIDI_DATA_MIN, INJECTMIDI_DATA_MAX);
	SendDlgItemMessage(hWnd, IDC_INJECT_DATA2S, UDM_SETPOS32, 0, 0);
}

static void InjectMidiMessage(HWND hWnd)
{
	int channel;
	int eventIndex;
	unsigned char byte1, byte2, byte3;
	if (!g_App->synth)
	{
		return;
	}
	channel = (int)SendDlgItemMessage(hWnd, IDC_INJECT_CHANNEL, CB_GETCURSEL, 0, 0);
	if (channel < 0)
	{
		channel = 0;
	}
	eventIndex = (int)SendDlgItemMessage(hWnd, IDC_INJECT_EVENTTYPE, CB_GETCURSEL, 0, 0);
	if (eventIndex < 0)
	{
		eventIndex = 0;
	}
	byte1 = (unsigned char)(s_eventTypes[eventIndex].status | channel);
	byte2 = (unsigned char)ClampInt((int)SendDlgItemMessage(hWnd, IDC_INJECT_DATA1S, UDM_GETPOS32, 0, 0), INJECTMIDI_DATA_MIN, INJECTMIDI_DATA_MAX);
	byte3 = (unsigned char)ClampInt((int)SendDlgItemMessage(hWnd, IDC_INJECT_DATA2S, UDM_GETPOS32, 0, 0), INJECTMIDI_DATA_MIN, INJECTMIDI_DATA_MAX);
	EnterCriticalSection(&g_App->synthCS);
	timid_write_midi(g_App->synth, byte1, byte2, byte3);
	LeaveCriticalSection(&g_App->synthCS);
}

BOOL WINAPI InjectMidiDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	switch (message)
	{
	case WM_INITDIALOG:
		InitInjectMidiControls(hWnd);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_INJECT_SEND:
			InjectMidiMessage(hWnd);
			return TRUE;
		case IDOK:
		case IDCANCEL:
			EndDialog(hWnd, TRUE);
			return TRUE;
		}
	}
	return FALSE;
}
