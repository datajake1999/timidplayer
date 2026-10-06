#include "timidplayer.h"

BOOL IsValidMidiInDeviceId(int deviceId)
{
	return deviceId >= 0 && (UINT)deviceId < midiInGetNumDevs();
}

void OpenMidiInDevice(HWND hWnd)
{
	UINT_PTR deviceId;
	if (g_App->hMidiIn)
	{
		return;
	}
	if (!IsValidMidiInDeviceId(g_App->midiInDeviceId))
	{
		deviceId = (UINT_PTR)MIDI_MAPPER;
	}
	else
	{
		deviceId = (UINT_PTR)g_App->midiInDeviceId;
	}
	if (midiInOpen(&g_App->hMidiIn, deviceId, (DWORD_PTR)hWnd, 0, CALLBACK_WINDOW) != MMSYSERR_NOERROR)
	{
		g_App->hMidiIn = NULL;
		if (hWnd && IsWindowEnabled(hWnd))
		{
			ShowAppStringMessage(hWnd, IDS_MIDIINFAILED, MB_ICONERROR);
		}
		return;
	}
	ZeroMemory(&g_App->midiInSysexHdr, sizeof(MIDIHDR));
	g_App->midiInSysexHdr.lpData = (LPSTR)g_App->midiInSysexBuffer;
	g_App->midiInSysexHdr.dwBufferLength = MIDI_SYSEX_BUFFER_SIZE;
	if (midiInPrepareHeader(g_App->hMidiIn, &g_App->midiInSysexHdr, sizeof(MIDIHDR)) == MMSYSERR_NOERROR)
	{
		midiInAddBuffer(g_App->hMidiIn, &g_App->midiInSysexHdr, sizeof(MIDIHDR));
	}
	if (midiInStart(g_App->hMidiIn) != MMSYSERR_NOERROR)
	{
		CloseMidiInDevice();
		return;
	}
	if (!StartMidiInputStream(hWnd))
	{
		CloseMidiInDevice();
	}
}

void CloseMidiInDevice(void)
{
	if (!g_App->hMidiIn)
	{
		return;
	}
	StopMidiInputStream();
	midiInStop(g_App->hMidiIn);
	midiInReset(g_App->hMidiIn);
	if (g_App->midiInSysexHdr.dwFlags & MHDR_PREPARED)
	{
		midiInUnprepareHeader(g_App->hMidiIn, &g_App->midiInSysexHdr, sizeof(MIDIHDR));
	}
	midiInClose(g_App->hMidiIn);
	g_App->hMidiIn = NULL;
}

static DWORD_PTR RemapMidiInChannel(DWORD_PTR packedMsg)
{
	unsigned char status = (unsigned char)(packedMsg & 0xFF);
	int channel;
	if (status < 0x80 || status >= 0xF0)
	{
		return packedMsg;
	}
	channel = status & 0x0F;
	status = (unsigned char)((status & 0xF0) | g_App->midiInChannelMap[channel]);
	return (packedMsg & ~(DWORD_PTR)0xFF) | status;
}

void HandleMidiInShortMessage(DWORD_PTR packedMsg)
{
	int nextHead;
	DWORD targetFrame;
	if (!g_App->bMidiInActive)
	{
		return;
	}
	packedMsg = RemapMidiInChannel(packedMsg);
	targetFrame = GetMidiInPlaybackFrame();
	EnterCriticalSection(&g_App->synthCS);
	nextHead = (g_App->midiInQueueHead + 1) % MIDIIN_QUEUE_SIZE;
	if (nextHead != g_App->midiInQueueTail)
	{
		g_App->midiInQueue[g_App->midiInQueueHead].msg = (unsigned long)packedMsg;
		g_App->midiInQueue[g_App->midiInQueueHead].frame = targetFrame;
		g_App->midiInQueueHead = nextHead;
	}
	LeaveCriticalSection(&g_App->synthCS);
}

void HandleMidiInLongMessage(MIDIHDR *hdr)
{
	if (!hdr)
	{
		return;
	}
	if (g_App->bMidiInActive && hdr->dwBytesRecorded > 0)
	{
		EnterCriticalSection(&g_App->synthCS);
		timid_write_sysex(g_App->synth, (unsigned char *)hdr->lpData, (long)hdr->dwBytesRecorded);
		LeaveCriticalSection(&g_App->synthCS);
	}
	if (g_App->hMidiIn && (hdr->dwFlags & MHDR_PREPARED))
	{
		midiInAddBuffer(g_App->hMidiIn, hdr, sizeof(MIDIHDR));
	}
}

void HandleMidiInReset(MidiInResetType type)
{
	if (!g_App->synth)
	{
		return;
	}
	EnterCriticalSection(&g_App->synthCS);
	switch (type)
	{
	case MIDIIN_RESET_ALLNOTESOFF:
		timid_all_notes_off(g_App->synth);
		break;
	case MIDIIN_RESET_ALLSOUNDSOFF:
		timid_all_sounds_off(g_App->synth);
		break;
	case MIDIIN_RESET_CONTROLLERS:
		timid_reset_controllers(g_App->synth);
		break;
	case MIDIIN_RESET_PANIC:
		timid_panic(g_App->synth);
		break;
	case MIDIIN_RESET_FULL:
		timid_reset(g_App->synth);
		break;
	}
	LeaveCriticalSection(&g_App->synthCS);
}

static BOOL GetMidiInDeviceName(UINT index, TCHAR *nameOut, int nameOutChars)
{
	MIDIINCAPS caps;
	if (midiInGetDevCaps(index, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
	{
		return FALSE;
	}
	SafeFormat(nameOut, nameOutChars, _T("%s"), caps.szPname);
	return TRUE;
}

static void InitChannelMapCombos(HWND hWnd)
{
	int ch;
	for (ch = 0; ch < 16; ch++)
	{
		HWND hCombo = GetDlgItem(hWnd, IDC_MIDIIN_CHANMAP0 + ch);
		PopulateChannelCombo(hCombo);
		SendMessage(hCombo, CB_SETCURSEL, (WPARAM)g_App->midiInChannelMap[ch], 0);
	}
}

BOOL WINAPI MidiInputDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	switch (message)
	{
	case WM_INITDIALOG:
		if (g_App->bMidiInEnabled)
		{
			CheckDlgButton(hWnd, IDC_MIDIIN_ENABLE, BST_CHECKED);
		}
		PopulateDeviceCombo(GetDlgItem(hWnd, IDC_MIDIIN_DEVICE), midiInGetNumDevs(), GetMidiInDeviceName, IDS_MIDIINMAPPER, g_App->midiInDeviceId);
		EnableWindow(GetDlgItem(hWnd, IDC_MIDIIN_DEVICE), g_App->bMidiInEnabled);
		InitChannelMapCombos(hWnd);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_MIDIIN_ENABLE:
			if (HIWORD(wParam) == BN_CLICKED)
			{
				EnableWindow(GetDlgItem(hWnd, IDC_MIDIIN_DEVICE), IsDlgButtonChecked(hWnd, IDC_MIDIIN_ENABLE) == BST_CHECKED);
			}
			return TRUE;
		case IDOK:
			{
				int sel = (int)SendDlgItemMessage(hWnd, IDC_MIDIIN_DEVICE, CB_GETCURSEL, 0, 0);
				int newDeviceId = ComboSelToDeviceId(sel, MIDI_MAPPER);
				int ch;
				BOOL newEnabled = (IsDlgButtonChecked(hWnd, IDC_MIDIIN_ENABLE) == BST_CHECKED);
				BOOL settingsChanged;
				settingsChanged = (newEnabled != g_App->bMidiInEnabled);
				if (newEnabled && newDeviceId != g_App->midiInDeviceId)
				{
					settingsChanged = TRUE;
				}
				g_App->bMidiInEnabled = newEnabled;
				g_App->midiInDeviceId = newDeviceId;
				for (ch = 0; ch < 16; ch++)
				{
					int mapSel = (int)SendDlgItemMessage(hWnd, IDC_MIDIIN_CHANMAP0 + ch, CB_GETCURSEL, 0, 0);
					if (mapSel < 0)
					{
						mapSel = ch;
					}
					g_App->midiInChannelMap[ch] = mapSel;
				}
				SaveMidiInputSettings();
				EndDialog(hWnd, TRUE);
				if (settingsChanged)
				{
					CloseMidiInDevice();
					if (g_App->bMidiInEnabled)
					{
						CancelSleepTimer();
						UnloadCurrentFile(g_App->hPlayerWnd);
						OpenMidiInDevice(g_App->hPlayerWnd);
					}
					RefreshPlayerUI(g_App->hPlayerWnd);
				}
			}
			return TRUE;
		case IDCANCEL:
			EndDialog(hWnd, FALSE);
			return TRUE;
		}
	}
	return FALSE;
}
