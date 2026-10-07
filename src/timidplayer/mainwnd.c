#include "timidplayer.h"

#define VOLUME_STEP 5
#define TIMER_ID_POSITION 2
#define WM_TRAYICON (WM_APP+7)
#define IDM_ALWAYSONTOP 0x1000

typedef struct {
	int appCmd;
	UINT commandId;
} AppCommandMapping;

static const AppCommandMapping g_AppCommandMap[] =
{
	{ APPCOMMAND_MEDIA_PLAY_PAUSE, ID_PLAYBACK_PLAYPAUSE },
	{ APPCOMMAND_MEDIA_STOP, ID_PLAYBACK_STOP },
	{ APPCOMMAND_MEDIA_NEXTTRACK, ID_PLAYBACK_NEXTTRACK },
	{ APPCOMMAND_MEDIA_PREVIOUSTRACK, ID_PLAYBACK_PREVTRACK }
};

#define NUM_APPCOMMAND_MAPPINGS (sizeof(g_AppCommandMap) / sizeof(g_AppCommandMap[0]))

#define HOTKEY_ID_PLAYPAUSE 1
#define HOTKEY_ID_STOP 2
#define HOTKEY_ID_NEXTTRACK 3
#define HOTKEY_ID_PREVTRACK 4
#define HOTKEY_ID_VOLUMEUP 5
#define HOTKEY_ID_VOLUMEDOWN 6

typedef struct {
	int id;
	UINT vk;
	UINT commandId;
	BOOL bVolumeKey;
} GlobalHotkey;

static const GlobalHotkey g_GlobalHotkeys[] =
{
	{ HOTKEY_ID_PLAYPAUSE, VK_MEDIA_PLAY_PAUSE, ID_PLAYBACK_PLAYPAUSE, FALSE },
	{ HOTKEY_ID_STOP, VK_MEDIA_STOP, ID_PLAYBACK_STOP, FALSE },
	{ HOTKEY_ID_NEXTTRACK, VK_MEDIA_NEXT_TRACK, ID_PLAYBACK_NEXTTRACK, FALSE },
	{ HOTKEY_ID_PREVTRACK, VK_MEDIA_PREV_TRACK, ID_PLAYBACK_PREVTRACK, FALSE },
	{ HOTKEY_ID_VOLUMEUP, VK_VOLUME_UP, ID_VOLUME_UP, TRUE },
	{ HOTKEY_ID_VOLUMEDOWN, VK_VOLUME_DOWN, ID_VOLUME_DOWN, TRUE }
};

#define NUM_GLOBAL_HOTKEYS (sizeof(g_GlobalHotkeys) / sizeof(g_GlobalHotkeys[0]))

static void RegisterGlobalHotkeys(HWND hWnd)
{
	int i;
	for (i = 0; i < (int)NUM_GLOBAL_HOTKEYS; i++)
	{
		if (g_GlobalHotkeys[i].bVolumeKey && !g_App->bInterceptVolumeKeys)
		{
			continue;
		}
		RegisterHotKey(hWnd, g_GlobalHotkeys[i].id, 0, g_GlobalHotkeys[i].vk);
	}
}

static void UnregisterGlobalHotkeys(HWND hWnd)
{
	int i;
	for (i = 0; i < (int)NUM_GLOBAL_HOTKEYS; i++)
	{
		UnregisterHotKey(hWnd, g_GlobalHotkeys[i].id);
	}
}

void UpdateVolumeKeyInterception(void)
{
	if (!g_App->hPlayerWnd)
	{
		return;
	}
	UnregisterGlobalHotkeys(g_App->hPlayerWnd);
	RegisterGlobalHotkeys(g_App->hPlayerWnd);
}

#define TRAY_ICON_UID 1

static void InitTrayIconData(NOTIFYICONDATA *nid, HWND hWnd)
{
	ZeroMemory(nid, sizeof(NOTIFYICONDATA));
	nid->cbSize = sizeof(NOTIFYICONDATA);
	nid->hWnd = hWnd;
	nid->uID = TRAY_ICON_UID;
}

static void AddTrayIcon(HWND hWnd)
{
	NOTIFYICONDATA nid;
	if (g_App->bTrayIconActive)
	{
		return;
	}
	InitTrayIconData(&nid, hWnd);
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_TRAYICON;
	nid.hIcon = LoadIcon(g_App->hInst, MAKEINTRESOURCE(IDI_ICON));
	SafeFormat(nid.szTip, (int)(sizeof(nid.szTip) / sizeof(TCHAR)), _T("%s"), g_App->appTitle);
	if (Shell_NotifyIcon(NIM_ADD, &nid))
	{
		g_App->bTrayIconActive = TRUE;
	}
}

static void RemoveTrayIcon(HWND hWnd)
{
	NOTIFYICONDATA nid;
	if (!g_App->bTrayIconActive)
	{
		return;
	}
	InitTrayIconData(&nid, hWnd);
	Shell_NotifyIcon(NIM_DELETE, &nid);
	g_App->bTrayIconActive = FALSE;
}

void SyncTrayIcon(HWND hWnd)
{
	if (g_App->bRunFromTray)
	{
		AddTrayIcon(hWnd);
	}
	else
	{
		RemoveTrayIcon(hWnd);
		if (!IsWindowVisible(hWnd))
		{
			ShowWindow(hWnd, SW_SHOW);
			SetForegroundWindow(hWnd);
		}
	}
}

static void ShowTrayMenu(HWND hWnd)
{
	HMENU hMenu = CreatePopupMenu();
	TCHAR restoreLabel[32];
	TCHAR exitLabel[32];
	POINT pt;
	if (!hMenu)
	{
		return;
	}
	LoadAppString(g_App->hInst, IDS_TRAYRESTORE, restoreLabel, 32);
	LoadAppString(g_App->hInst, IDS_TRAYEXIT, exitLabel, 32);
	AppendMenu(hMenu, MF_STRING, ID_TRAY_RESTORE, restoreLabel);
	if (g_App->bMidiInEnabled)
	{
		TCHAR panicLabel[32];
		TCHAR resetLabel[32];
		LoadAppString(g_App->hInst, IDS_TRAYPANIC, panicLabel, 32);
		LoadAppString(g_App->hInst, IDS_TRAYRESET, resetLabel, 32);
		AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
		AppendMenu(hMenu, MF_STRING, ID_RESET_PANIC, panicLabel);
		AppendMenu(hMenu, MF_STRING, ID_RESET_RESET, resetLabel);
	}
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(hMenu, MF_STRING, ID_FILE_EXIT, exitLabel);
	GetCursorPos(&pt);
	SetForegroundWindow(hWnd);
	TrackPopupMenu(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
	PostMessage(hWnd, WM_NULL, 0, 0);
	DestroyMenu(hMenu);
}

static BOOL ShowContextMenu(HWND hWnd, LPARAM lParam)
{
	int xPos, yPos;
	HMENU hMenu;
	TCHAR playLabel[64];
	TCHAR removeLabel[64];
	TCHAR statsLabel[64];
	TCHAR convertLabel[64];
	if (!g_App->bFileLoaded)
	{
		return FALSE;
	}
	hMenu = CreatePopupMenu();
	if (!hMenu)
	{
		return FALSE;
	}
	LoadAppString(g_App->hInst, IDS_CONTEXTPLAY, playLabel, 64);
	LoadAppString(g_App->hInst, IDS_CONTEXTREMOVE, removeLabel, 64);
	LoadAppString(g_App->hInst, IDS_CONTEXTSTATS, statsLabel, 64);
	LoadAppString(g_App->hInst, IDS_CONTEXTCONVERT, convertLabel, 64);
	AppendMenu(hMenu, MF_STRING, ID_CONTEXT_PLAY, playLabel);
	AppendMenu(hMenu, MF_STRING, ID_CONTEXT_REMOVE, removeLabel);
	AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(hMenu, MF_STRING, ID_PLAYBACK_STATS, statsLabel);
	AppendMenu(hMenu, MF_STRING, ID_FILE_CONVERTCURRENT, convertLabel);
	xPos = GET_X_LPARAM(lParam);
	yPos = GET_Y_LPARAM(lParam);
	if (xPos == -1 && yPos == -1)
	{
		POINT pt = { 0, 0 };
		ClientToScreen(hWnd, &pt);
		xPos = pt.x;
		yPos = pt.y;
	}
	SetForegroundWindow(hWnd);
	TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, xPos, yPos, 0, hWnd, NULL);
	PostMessage(hWnd, WM_NULL, 0, 0);
	DestroyMenu(hMenu);
	return TRUE;
}

static UINT MenuEnableFlag(BOOL enabled)
{
	if (enabled)
	{
		return MF_ENABLED;
	}
	return MF_GRAYED;
}

static void SetMenuItemEnabled(HMENU hMenu, UINT id, BOOL enabled)
{
	EnableMenuItem(hMenu, id, MF_BYCOMMAND | MenuEnableFlag(enabled));
}

static void SetMenuItemChecked(HMENU hMenu, UINT id, BOOL checked)
{
	UINT flags;
	if (checked)
	{
		flags = MF_CHECKED;
	}
	else
	{
		flags = MF_UNCHECKED;
	}
	CheckMenuItem(hMenu, id, MF_BYCOMMAND | flags);
}

static void ShowOrHideWindow(HWND hWndCtrl, BOOL show)
{
	int cmd;
	if (show)
	{
		cmd = SW_SHOW;
	}
	else
	{
		cmd = SW_HIDE;
	}
	ShowWindow(hWndCtrl, cmd);
}

void UpdateMenuState(HWND hWnd)
{
	HMENU hMenu = GetMenu(hWnd);
	TCHAR label[64];
	BOOL canSeek = g_App->bFileLoaded && !g_App->bMidiInEnabled;
	BOOL canStop = canSeek && g_App->state != PLAYER_STOPPED;
	UINT playPauseEnable;
	SetMenuItemEnabled(hMenu, ID_FILE_OPEN, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_FILE_ADDFILES, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_FILE_ADDFOLDER, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_FILE_PASTE, g_App->synth != NULL && ClipboardHasPasteableFiles());
	SetMenuItemEnabled(hMenu, ID_FILE_CLEARRECENTFILES, g_App->recentFileCount > 0);
	SetMenuItemEnabled(hMenu, ID_FILE_OPENPLAYLIST, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_FILE_SAVEPLAYLIST, g_App->playlistCount > 0);
	SetMenuItemEnabled(hMenu, ID_FILE_UNLOAD, g_App->bFileLoaded);
	SetMenuItemEnabled(hMenu, ID_FILE_CLEARPLAYLIST, g_App->bFileLoaded || g_App->playlistCount > 0);
	SetMenuItemEnabled(hMenu, ID_FILE_CONVERTCURRENT, g_App->bFileLoaded && !g_App->bQuickConverting && !g_App->bConverting);
	SetMenuItemEnabled(hMenu, ID_FILE_BATCHCONVERT, !g_App->bQuickConverting);
	playPauseEnable = MenuEnableFlag((g_App->bFileLoaded || g_App->playlistCount > 0) && !g_App->bMidiInEnabled);
	EnableMenuItem(hMenu, ID_PLAYBACK_PLAYPAUSE, MF_BYCOMMAND | playPauseEnable);
	SetMenuItemEnabled(hMenu, ID_PLAYBACK_STOP, canStop);
	SetMenuItemEnabled(hMenu, ID_PLAYBACK_NEXTTRACK, !g_App->bMidiInEnabled && g_App->playlistCount > 0 && (g_App->playlistIndex < 0 || g_App->playlistIndex + 1 < g_App->playlistCount));
	SetMenuItemEnabled(hMenu, ID_PLAYBACK_PREVTRACK, !g_App->bMidiInEnabled && g_App->playlistCount > 0 && (g_App->playlistIndex < 0 || g_App->playlistIndex > 0));
	SetMenuItemEnabled(hMenu, ID_PLAYBACK_FIRSTTRACK, !g_App->bMidiInEnabled && g_App->playlistCount > 0 && g_App->playlistIndex != 0);
	SetMenuItemEnabled(hMenu, ID_PLAYBACK_LASTTRACK, !g_App->bMidiInEnabled && g_App->playlistCount > 0 && g_App->playlistIndex != g_App->playlistCount - 1);
	SetMenuItemEnabled(hMenu, ID_SEEK_FORWARD, canSeek);
	SetMenuItemEnabled(hMenu, ID_SEEK_BACK, canSeek);
	EnableWindow(g_App->hSeekBar, canSeek);
	SetMenuItemEnabled(hMenu, ID_PLAYBACK_RESTART, canSeek);
	SetMenuItemEnabled(hMenu, ID_PLAYBACK_JUMPTOEND, canSeek);
	SetMenuItemEnabled(hMenu, ID_PLAYBACK_JUMPTOTIME, canSeek);
	SetMenuItemEnabled(hMenu, ID_PLAYBACK_SLEEPTIMER, !g_App->bMidiInEnabled);
	SetMenuItemEnabled(hMenu, ID_PLAYBACK_SHUFFLE, g_App->playlistCount > 1);
	SetMenuItemEnabled(hMenu, ID_PLAYBACK_MANAGEPLAYLIST, g_App->playlistCount > 1);
	CheckMenuRadioItem(hMenu, ID_PLAYBACK_REPEATOFF, ID_PLAYBACK_REPEATALL, ID_PLAYBACK_REPEATOFF + g_App->repeatMode, MF_BYCOMMAND);
	SetMenuItemEnabled(hMenu, ID_PLAYBACK_STATS, g_App->bFileLoaded);
	SetMenuItemEnabled(hMenu, ID_CONFIGURE_AUDIOOUTPUT, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_CONFIGURE_MIDIINPUT, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_TOOLS_CHANNELMIXER, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_TOOLS_VKBD, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_TOOLS_INJECTMIDI, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_RESET_ALLNOTESOFF, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_RESET_ALLSOUNDSOFF, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_RESET_RESETCONTROLLERS, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_RESET_PANIC, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_RESET_RESET, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_ENGINE_RELOADCONFIG, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_ENGINE_RELOADSMF, canSeek);
	SetMenuItemEnabled(hMenu, ID_ENGINE_FORCEINSTRUMENTLOAD, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_ENGINE_FREEDEFAULTINSTRUMENT, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_ENGINE_UNLOADCONFIG, g_App->synth != NULL);
	SetMenuItemEnabled(hMenu, ID_ENGINE_RESTOREDEFAULTS, g_App->synth != NULL);
	SetMenuItemChecked(hMenu, ID_VIEW_STATUSBAR, g_App->bShowStatusBar);
	SetMenuItemChecked(hMenu, ID_VIEW_SEEKBAR, g_App->bShowSeekBar);
	SetMenuItemChecked(hMenu, ID_VIEW_PLAYLISTVIEW, g_App->bShowPlaylistView);
	if (g_App->state == PLAYER_PLAYING && !g_App->bMidiInActive)
	{
		LoadAppString(g_App->hInst, IDS_PAUSELABEL, label, 64);
	}
	else
	{
		LoadAppString(g_App->hInst, IDS_PLAYLABEL, label, 64);
	}
	ModifyMenu(hMenu, ID_PLAYBACK_PLAYPAUSE, MF_BYCOMMAND | MF_STRING | playPauseEnable, ID_PLAYBACK_PLAYPAUSE, label);
	DrawMenuBar(hWnd);
}

static DWORD ComputePlaylistFingerprint(void)
{
	DWORD hash = 5381;
	int i;
	const TCHAR *p;
	hash = hash * 33 + (DWORD)g_App->playlistCount;
	hash = hash * 33 + (DWORD)(g_App->playlistIndex + 1);
	for (i = 0; i < g_App->playlistCount; i++)
	{
		for (p = g_App->playlist[i].path; *p; p++)
		{
			hash = hash * 33 + (DWORD)(*p);
		}
		hash = hash * 33 + 1;
	}
	return hash;
}

static void SyncMainPlaylistView(void)
{
	DWORD fp;
	int i, curSel, topIndex;
	fp = ComputePlaylistFingerprint();
	if (fp == g_App->lastPlaylistFingerprint)
	{
		return;
	}
	g_App->lastPlaylistFingerprint = fp;
	if (g_App->hPlaylistMgrWnd)
	{
		HWND hMgrList = GetDlgItem(g_App->hPlaylistMgrWnd, IDC_PLMGRLIST);
		int mgrCount;
		int *mgrItems = GetSortedSelectedListViewItems(hMgrList, &mgrCount, FALSE);
		PopulatePlaylistListBox(g_App->hPlaylistMgrWnd);
		if (mgrItems)
		{
			ReselectMovedListViewItems(hMgrList, mgrItems, mgrCount, g_App->plMgrFilterCount);
		}
		else if (g_App->plMgrFilterCount > 0)
		{
			ListView_SetItemState(hMgrList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		}
	}
	if (!g_App->hPlaylistView)
	{
		return;
	}
	curSel = GetListViewCurSel(g_App->hPlaylistView);
	topIndex = ListView_GetTopIndex(g_App->hPlaylistView);
	SendMessage(g_App->hPlaylistView, WM_SETREDRAW, FALSE, 0);
	ListView_DeleteAllItems(g_App->hPlaylistView);
	for (i = 0; i < g_App->playlistCount; i++)
	{
		TCHAR buf[MAX_PATH + 4];
		const TCHAR *fname = GetBaseName(g_App->playlist[i].path);
		if (i == g_App->playlistIndex)
		{
			SafeFormat(buf, MAX_PATH + 4, _T("> %s"), fname);
		}
		else
		{
			SafeFormat(buf, MAX_PATH + 4, _T("%s"), fname);
		}
		InsertListViewItem(g_App->hPlaylistView, i, buf);
	}
	if (g_App->bAutoscrollPlaylist && g_App->playlistIndex >= 0 && g_App->playlistIndex < g_App->playlistCount)
	{
		SetListViewCurSel(g_App->hPlaylistView, g_App->playlistIndex);
	}
	else if (curSel >= 0 && curSel < g_App->playlistCount)
	{
		ListView_SetItemState(g_App->hPlaylistView, curSel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		ListView_EnsureVisible(g_App->hPlaylistView, topIndex, FALSE);
	}
	else
	{
		if (g_App->playlistCount > 0)
		{
			ListView_SetItemState(g_App->hPlaylistView, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		}
		ListView_EnsureVisible(g_App->hPlaylistView, topIndex, FALSE);
	}
	SendMessage(g_App->hPlaylistView, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(g_App->hPlaylistView, NULL, TRUE);
}

static void CaptureCpuLoadSnapshot(LARGE_INTEGER *start, LARGE_INTEGER *end, LONGLONG *samples)
{
	*start = g_App->cpuLoadLastStart;
	*end = g_App->cpuLoadLastEnd;
	*samples = g_App->cpuLoadLastSamples;
}

void UpdateStatusBar(void)
{
	TCHAR posBuf[32], durBuf[32], voicesBuf[32], cpuBuf[32];
	TCHAR part0[160];
	TCHAR stateTextBuf[32];
	int curMs = 0, durMs = 0, activeVoices = 0, maxVoices = 0;
	LARGE_INTEGER cpuStart, cpuEnd;
	LONGLONG cpuSamples = 0;
	cpuStart.QuadPart = 0;
	cpuEnd.QuadPart = 0;
	if (g_App->bSeekBarDragging && GetCapture() != g_App->hSeekBar)
	{
		g_App->bSeekBarDragging = FALSE;
	}
	if (g_App->bFileLoaded || g_App->bMidiInActive)
	{
		EnterCriticalSection(&g_App->synthCS);
		if (g_App->bFileLoaded)
		{
			curMs = timid_get_current_time(g_App->synth);
			durMs = timid_get_duration(g_App->synth);
		}
		activeVoices = timid_get_active_voices(g_App->synth);
		maxVoices = timid_get_max_voices(g_App->synth);
		CaptureCpuLoadSnapshot(&cpuStart, &cpuEnd, &cpuSamples);
		LeaveCriticalSection(&g_App->synthCS);
	}
	if (!g_App->bQuickConverting)
	{
		TCHAR fmt[64];
		if (g_App->bMidiInActive)
		{
			LoadAppString(g_App->hInst, IDS_STATEMIDIIN, stateTextBuf, 32);
		}
		else
		{
			switch (g_App->state)
			{
			case PLAYER_PLAYING:
				LoadAppString(g_App->hInst, IDS_STATEPLAYING, stateTextBuf, 32);
				break;
			case PLAYER_PAUSED:
				LoadAppString(g_App->hInst, IDS_STATEPAUSED, stateTextBuf, 32);
				break;
			default:
				if (g_App->bFileLoaded)
				{
					LoadAppString(g_App->hInst, IDS_STATESTOPPED, stateTextBuf, 32);
				}
				else
				{
					LoadAppString(g_App->hInst, IDS_STATENOFILE, stateTextBuf, 32);
				}
				break;
			}
		}
		if (g_App->bFileLoaded)
		{
			FormatTime(curMs, posBuf, 32);
			FormatTime(durMs, durBuf, 32);
			if (g_App->playlistCount > 1)
			{
				LoadAppString(g_App->hInst, IDS_STATUSTRACKFMT, fmt, 64);
				SafeFormat(part0, 160, fmt, stateTextBuf, posBuf, durBuf, g_App->playlistIndex+1, g_App->playlistCount);
			}
			else
			{
				LoadAppString(g_App->hInst, IDS_STATUSFMT, fmt, 64);
				SafeFormat(part0, 160, fmt, stateTextBuf, posBuf, durBuf);
			}
		}
		else
		{
			SafeFormat(part0, 160, _T("%s"), stateTextBuf);
		}
		SendMessage(g_App->hStatusBar, SB_SETTEXT, 0, (LPARAM)part0);
	}
	if (g_App->bFileLoaded && g_App->title[0])
	{
		SendMessage(g_App->hStatusBar, SB_SETTEXT, 1, (LPARAM)g_App->title);
	}
	else
	{
		SendMessage(g_App->hStatusBar, SB_SETTEXT, 1, (LPARAM)_T(""));
	}
	if (g_App->bFileLoaded && g_App->copyright[0])
	{
		SendMessage(g_App->hStatusBar, SB_SETTEXT, 2, (LPARAM)g_App->copyright);
	}
	else
	{
		SendMessage(g_App->hStatusBar, SB_SETTEXT, 2, (LPARAM)_T(""));
	}
	if (g_App->bQuickConverting)
	{
		SendMessage(g_App->hStatusBar, SB_SETTEXT, 3, (LPARAM)_T(""));
		SendMessage(g_App->hStatusBar, SB_SETTEXT, 4, (LPARAM)_T(""));
	}
	else
	{
		TCHAR fmt2[64];
		if (g_App->bFileLoaded || g_App->bMidiInActive)
		{
			LoadAppString(g_App->hInst, IDS_VOICESFMT, fmt2, 64);
			SafeFormat(voicesBuf, 32, fmt2, activeVoices, maxVoices, g_App->volume);
		}
		else
		{
			LoadAppString(g_App->hInst, IDS_VOLFMT, fmt2, 64);
			SafeFormat(voicesBuf, 32, fmt2, g_App->volume);
		}
		SendMessage(g_App->hStatusBar, SB_SETTEXT, 3, (LPARAM)voicesBuf);
		LoadAppString(g_App->hInst, IDS_CPUFMT, fmt2, 64);
		SafeFormat(cpuBuf, 32, fmt2, CalculateCpuLoad(cpuStart, cpuEnd, cpuSamples));
		SendMessage(g_App->hStatusBar, SB_SETTEXT, 4, (LPARAM)cpuBuf);
	}
	if (g_App->bFileLoaded)
	{
		int rangeMax;
		SendMessage(g_App->hSeekBar, TBM_SETRANGEMIN, FALSE, 0);
		if (durMs > 0)
		{
			rangeMax = durMs;
		}
		else
		{
			rangeMax = 0;
		}
		SendMessage(g_App->hSeekBar, TBM_SETRANGEMAX, TRUE, rangeMax);
		if (!g_App->bSeekBarDragging)
		{
			SendMessage(g_App->hSeekBar, TBM_SETPOS, TRUE, curMs);
		}
	}
	else
	{
		SendMessage(g_App->hSeekBar, TBM_SETRANGEMIN, FALSE, 0);
		SendMessage(g_App->hSeekBar, TBM_SETRANGEMAX, FALSE, 0);
		SendMessage(g_App->hSeekBar, TBM_SETPOS, TRUE, 0);
	}
	SyncMainPlaylistView();
}

void UpdateWindowTitle(HWND hWnd)
{
	TCHAR title[MAX_PATH + 64];
	TCHAR label[MAX_PATH];
	TCHAR fmt[64];
	if (!g_App->bShowFilenameInTitle)
	{
		SetWindowText(hWnd, g_App->appTitle);
		return;
	}
	if (g_App->bFileLoaded && g_App->loadedFilePath[0])
	{
		const TCHAR *fname = GetBaseName(g_App->loadedFilePath);
		SafeFormat(label, MAX_PATH, _T("%s"), fname);
	}
	else
	{
		LoadAppString(g_App->hInst, IDS_NOFILELOADED, label, MAX_PATH);
	}
	LoadAppString(g_App->hInst, IDS_WINDOWTITLEFMT, fmt, 64);
	SafeFormat(title, MAX_PATH + 64, fmt, g_App->appTitle, label);
	SetWindowText(hWnd, title);
}

static BOOL BlockCloseIfBusy(HWND hWnd, const BOOL *busyFlag, UINT msgId)
{
	if (!*busyFlag)
	{
		return FALSE;
	}
	ShowAppStringMessage(hWnd, msgId, MB_ICONWARNING);
	return *busyFlag;
}

static void StopPlaybackAndSync(HWND hWnd)
{
	StopPlayback();
	CancelSleepTimer();
	ResumeMidiInputIfIdle();
	RefreshPlayerUI(hWnd);
}

static void LayoutStatusBar(HWND hWnd)
{
	RECT rc;
	int parts[5];
	GetClientRect(hWnd, &rc);
	parts[0] = rc.right * 25 / 100;
	parts[1] = rc.right * 48 / 100;
	parts[2] = rc.right * 68 / 100;
	parts[3] = rc.right * 85 / 100;
	parts[4] = -1;
	SendMessage(g_App->hStatusBar, SB_SETPARTS, 5, (LPARAM)parts);
}

static int GetStatusBarHeightIfShown(void)
{
	RECT rcStatus;
	if (g_App->bShowStatusBar && GetWindowRect(g_App->hStatusBar, &rcStatus))
	{
		return rcStatus.bottom - rcStatus.top;
	}
	return 0;
}

static void LayoutSeekBar(HWND hWnd)
{
	RECT rcClient;
	int sbHeight, seekHeight, seekTop, width;
	GetClientRect(hWnd, &rcClient);
	sbHeight = GetStatusBarHeightIfShown();
	seekHeight = 20;
	seekTop = rcClient.bottom - sbHeight - seekHeight - 4;
	if (seekTop < 0)
	{
		seekTop = 0;
	}
	width = rcClient.right - 16;
	if (width < 0)
	{
		width = 0;
	}
	MoveWindow(g_App->hSeekBar, 8, seekTop, width, seekHeight, TRUE);
}

static void LayoutPlaylistView(HWND hWnd)
{
	RECT rcClient, rcSeek;
	int bottom, labelHeight, listTop, listHeight, width;
	GetClientRect(hWnd, &rcClient);
	width = rcClient.right - 16;
	if (width < 0)
	{
		width = 0;
	}
	if (g_App->bShowSeekBar && GetWindowRect(g_App->hSeekBar, &rcSeek))
	{
		MapWindowPoints(NULL, hWnd, (POINT *)&rcSeek, 2);
		bottom = rcSeek.top - 4;
	}
	else
	{
		bottom = rcClient.bottom - GetStatusBarHeightIfShown() - 4;
	}
	if (bottom < 8)
	{
		bottom = 8;
	}
	labelHeight = 14;
	MoveWindow(g_App->hPlaylistLabel, 8, 8, width, labelHeight, TRUE);
	listTop = 8 + labelHeight + 2;
	if (listTop > bottom)
	{
		listTop = bottom;
	}
	listHeight = bottom - listTop;
	if (listHeight < 0)
	{
		listHeight = 0;
	}
	MoveWindow(g_App->hPlaylistView, 8, listTop, width, listHeight, TRUE);
	ListView_SetColumnWidth(g_App->hPlaylistView, 0, width);
}

static void ApplyViewVisibility(HWND hWnd)
{
	ShowOrHideWindow(g_App->hStatusBar, g_App->bShowStatusBar);
	ShowOrHideWindow(g_App->hSeekBar, g_App->bShowSeekBar);
	ShowOrHideWindow(g_App->hPlaylistLabel, g_App->bShowPlaylistView);
	ShowOrHideWindow(g_App->hPlaylistView, g_App->bShowPlaylistView);
	LayoutStatusBar(hWnd);
	LayoutSeekBar(hWnd);
	LayoutPlaylistView(hWnd);
}

static void ToggleViewFlag(HWND hWnd, BOOL *flag)
{
	*flag = !*flag;
	ApplyViewVisibility(hWnd);
	UpdateMenuState(hWnd);
	SaveViewSettings();
}

static void ShowAboutBox(HWND hWnd)
{
	TCHAR caption[MAX_PATH];
	TCHAR text[MAX_PATH*2];
	LoadAppString(g_App->hInst, IDS_ABOUTCAP, caption, MAX_PATH);
	LoadAppString(g_App->hInst, IDS_ABOUTTXT, text, MAX_PATH*2);
	ShowMessageBox(hWnd, text, caption, MB_ICONINFORMATION);
}

static void AddStatRow(HWND hList, int index, UINT fmtId, ...)
{
	TCHAR fmt[64];
	TCHAR buf[300];
	va_list args;
	LoadAppString(g_App->hInst, fmtId, fmt, 64);
	va_start(args, fmtId);
	_vsntprintf(buf, 300, fmt, args);
	va_end(args);
	buf[299] = _T('\0');
	InsertListViewItem(hList, index, buf);
}

static void RefreshStatsDialog(HWND hWnd)
{
	HWND hList = GetDlgItem(hWnd, IDC_STATSLIST);
	TCHAR noneBuf[32];
	TCHAR durBuf[32], curBuf[32];
	TCHAR fileBuf[MAX_PATH];
	int curSel;
	int events = 0, lost = 0, cut = 0, activeVoices = 0, maxVoices = 0;
	int bitrate = 0, durMs = 0, curMs = 0;
	const TCHAR *fileText;
	const TCHAR *titleText;
	const TCHAR *copyrightText;
	LARGE_INTEGER cpuStart, cpuEnd;
	LONGLONG cpuSamples = 0;
	cpuStart.QuadPart = 0;
	cpuEnd.QuadPart = 0;
	fileBuf[0] = _T('\0');
	if (g_App->bFileLoaded && g_App->synth)
	{
		char smfNameAnsi[MAX_PATH];
		ZeroMemory(smfNameAnsi, sizeof(smfNameAnsi));
		EnterCriticalSection(&g_App->synthCS);
		if (timid_get_smf_name(g_App->synth, smfNameAnsi, MAX_PATH))
		{
			AnsiToTChar(smfNameAnsi, fileBuf, MAX_PATH);
		}
		events = timid_get_event_count(g_App->synth);
		durMs = timid_get_duration(g_App->synth);
		curMs = timid_get_current_time(g_App->synth);
		if (durMs > 0)
		{
			bitrate = timid_get_bitrate(g_App->synth);
		}
		activeVoices = timid_get_active_voices(g_App->synth);
		maxVoices = timid_get_max_voices(g_App->synth);
		lost = timid_get_lost_notes(g_App->synth);
		cut = timid_get_cut_notes(g_App->synth);
		CaptureCpuLoadSnapshot(&cpuStart, &cpuEnd, &cpuSamples);
		LeaveCriticalSection(&g_App->synthCS);
	}
	FormatTimeHMS(durMs, durBuf, 32);
	FormatTimeHMS(curMs, curBuf, 32);
	curSel = GetListViewCurSel(hList);
	ListView_DeleteAllItems(hList);
	LoadAppString(g_App->hInst, IDS_STATSNONE, noneBuf, 32);
	if (fileBuf[0])
	{
		fileText = fileBuf;
	}
	else
	{
		fileText = noneBuf;
	}
	AddStatRow(hList, 0, IDS_STATSFILE, fileText);
	if (g_App->title[0])
	{
		titleText = g_App->title;
	}
	else
	{
		titleText = noneBuf;
	}
	AddStatRow(hList, 1, IDS_STATSTITLE, titleText);
	if (g_App->copyright[0])
	{
		copyrightText = g_App->copyright;
	}
	else
	{
		copyrightText = noneBuf;
	}
	AddStatRow(hList, 2, IDS_STATSCOPYRIGHT, copyrightText);
	AddStatRow(hList, 3, IDS_STATSEVENTS, events);
	AddStatRow(hList, 4, IDS_STATSDURATION, durBuf);
	AddStatRow(hList, 5, IDS_STATSPOSITION, curBuf);
	AddStatRow(hList, 6, IDS_STATSBITRATE, bitrate);
	AddStatRow(hList, 7, IDS_STATSVOICES, activeVoices, maxVoices);
	AddStatRow(hList, 8, IDS_STATSLOST, lost);
	AddStatRow(hList, 9, IDS_STATSCUT, cut);
	AddStatRow(hList, 10, IDS_STATSCPU, CalculateCpuLoad(cpuStart, cpuEnd, cpuSamples));
	if (curSel < 0)
	{
		curSel = 0;
	}
	ListView_SetItemState(hList, curSel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

static BOOL WINAPI StatsDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	switch (message)
	{
	case WM_INITDIALOG:
		InitSingleColumnListView(GetDlgItem(hWnd, IDC_STATSLIST));
		RefreshStatsDialog(hWnd);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_STATSREFRESH:
			RefreshStatsDialog(hWnd);
			return TRUE;
		case IDOK:
		case IDCANCEL:
			EndDialog(hWnd, TRUE);
			return TRUE;
		}
	}
	return FALSE;
}

LRESULT CALLBACK PlayerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		{
			TCHAR playlistLabel[32];
			LoadAppString(g_App->hInst, IDS_PLAYLISTLABEL, playlistLabel, 32);
			g_App->hStatusBar = CreateWindow(STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_STATUSBAR, g_App->hInst, NULL);
			g_App->hSeekBar = CreateWindow(TRACKBAR_CLASS, NULL, WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_TOOLTIPS, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_SEEKBAR, g_App->hInst, NULL);
			g_App->hPlaylistLabel = CreateWindow(_T("STATIC"), playlistLabel, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_STATIC, g_App->hInst, NULL);
			g_App->hPlaylistView = CreateWindow(WC_LISTVIEW, NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | WS_BORDER | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SHOWSELALWAYS | LVS_SINGLESEL, 0, 0, 0, 0, hWnd, (HMENU)(INT_PTR)IDC_PLAYLISTVIEW, g_App->hInst, NULL);
			if (!g_App->hStatusBar || !g_App->hSeekBar || !g_App->hPlaylistLabel || !g_App->hPlaylistView)
			{
				ShowAppStringMessage(hWnd, IDS_CTRLFAILED, MB_ICONERROR);
			}
			else
			{
				InitSingleColumnListView(g_App->hPlaylistView);
			}
			ApplyViewVisibility(hWnd);
			DragAcceptFiles(hWnd, TRUE);
			{
				HMENU hFileMenu = GetSubMenu(GetMenu(hWnd), 0);
				int menuCount = GetMenuItemCount(hFileMenu);
				int mi;
				g_App->hRecentMenu = NULL;
				for (mi = 0; mi < menuCount; mi++)
				{
					HMENU hSub = GetSubMenu(hFileMenu, mi);
					if (hSub && GetMenuItemID(hSub, 0) == ID_FILE_RECENT_EMPTY)
					{
						g_App->hRecentMenu = hSub;
						break;
					}
				}
			}
			RebuildRecentFilesMenu();
			{
				HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
				if (hSysMenu)
				{
					UINT sysCheck;
					TCHAR alwaysOnTopLabel[32];
					if (g_App->bAlwaysOnTop)
					{
						sysCheck = MF_CHECKED;
					}
					else
					{
						sysCheck = MF_UNCHECKED;
					}
					LoadAppString(g_App->hInst, IDS_ALWAYSONTOPMENU, alwaysOnTopLabel, 32);
					AppendMenu(hSysMenu, MF_SEPARATOR, 0, NULL);
					AppendMenu(hSysMenu, MF_STRING | sysCheck, IDM_ALWAYSONTOP, alwaysOnTopLabel);
				}
			}
			if (g_App->bAlwaysOnTop)
			{
				SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			}
			if (g_App->bRunFromTray)
			{
				AddTrayIcon(hWnd);
			}
			InitializeCriticalSection(&g_App->synthCS);
			if (!RefreshSynth())
			{
				ShowAppStringMessage(hWnd, IDS_SYNTHINITFAILED, MB_ICONERROR);
			}
			LoadMidiInputSettings();
			if (g_App->bMidiInEnabled)
			{
				OpenMidiInDevice(hWnd);
			}
			SetTimer(hWnd, TIMER_ID_POSITION, 500, NULL);
			RegisterGlobalHotkeys(hWnd);
			RefreshPlayerUI(hWnd);
		}
		return 0;
	case WM_SIZE:
		SendMessage(g_App->hStatusBar, WM_SIZE, 0, 0);
		LayoutStatusBar(hWnd);
		LayoutSeekBar(hWnd);
		LayoutPlaylistView(hWnd);
		return 0;
	case WM_INITMENUPOPUP:
		UpdateMenuState(hWnd);
		return 0;
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			g_App->hLastFocus = GetFocus();
		}
		else
		{
			if (g_App->hLastFocus && IsWindow(g_App->hLastFocus) && IsChild(hWnd, g_App->hLastFocus))
			{
				SetFocus(g_App->hLastFocus);
			}
			g_App->hLastFocus = NULL;
		}
		return 0;
	case WM_TIMER:
		if (wParam == TIMER_ID_POSITION)
		{
			if (!IsIconic(hWnd))
			{
				UpdateStatusBar();
			}
		}
		else if (wParam == TIMER_ID_SLEEP)
		{
			g_App->sleepTimerMinutes = 0;
			StopPlaybackAndSync(hWnd);
		}
		return 0;
	case WM_HSCROLL:
		if ((HWND)lParam == g_App->hSeekBar)
		{
			int code = LOWORD(wParam);
			if (code == TB_THUMBTRACK)
			{
				g_App->bSeekBarDragging = TRUE;
			}
			else
			{
				int pos = (int)SendMessage(g_App->hSeekBar, TBM_GETPOS, 0, 0);
				g_App->bSeekBarDragging = FALSE;
				SeekAbsolute(pos);
			}
			return 0;
		}
		break;
	case WM_DROPFILES:
		DropFiles(hWnd, (HDROP)wParam);
		return 0;
	case WM_COPYDATA:
		{
			COPYDATASTRUCT *pcds = (COPYDATASTRUCT *)lParam;
			if (pcds && pcds->dwData == COPYDATA_OPENPATH && pcds->lpData && pcds->cbData >= sizeof(TCHAR))
			{
				size_t availChars = pcds->cbData / sizeof(TCHAR);
				TCHAR *cmdBuf = (TCHAR *)malloc(sizeof(TCHAR) * (availChars + 1));
				if (cmdBuf)
				{
					CopyMemory(cmdBuf, pcds->lpData, availChars * sizeof(TCHAR));
					cmdBuf[availChars] = _T('\0');
					if (_tcsicmp(cmdBuf, CMDLINE_BATCH_SWITCH) == 0)
					{
						OpenCommandLineBatch(hWnd, cmdBuf);
					}
					else
					{
						OpenCommandLinePath(hWnd, cmdBuf);
					}
					free(cmdBuf);
				}
			}
		}
		return TRUE;
	case WM_NOTIFY:
		{
			LPNMHDR pnmh = (LPNMHDR)lParam;
			if (pnmh->idFrom == IDC_PLAYLISTVIEW && pnmh->code == NM_DBLCLK)
			{
				int idx = GetListViewCurSel(g_App->hPlaylistView);
				if (idx >= 0 && idx < g_App->playlistCount)
				{
					PlayPlaylistEntryAt(hWnd, idx);
				}
			}
		}
		return 0;
	case WM_ENABLE:
		if (wParam && g_App->pDeferredQuickConvertResult)
		{
			QuickConvertResult *deferred = g_App->pDeferredQuickConvertResult;
			g_App->pDeferredQuickConvertResult = NULL;
			ShowQuickConvertResult(hWnd, deferred);
			free(deferred);
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_FILE_OPEN:
			OpenMidiFilesDialog(hWnd);
			return 0;
		case ID_FILE_ADDFILES:
			AddMidiFilesDialog(hWnd);
			return 0;
		case ID_FILE_ADDFOLDER:
			AddFolderDialog(hWnd);
			return 0;
		case ID_FILE_PASTE:
			PasteFilesFromClipboard(hWnd);
			return 0;
		case ID_FILE_RECENT1:
		case ID_FILE_RECENT2:
		case ID_FILE_RECENT3:
		case ID_FILE_RECENT4:
		case ID_FILE_RECENT5:
		case ID_FILE_RECENT6:
		case ID_FILE_RECENT7:
		case ID_FILE_RECENT8:
			{
				int idx = LOWORD(wParam) - ID_FILE_RECENT1;
				if (idx >= 0 && idx < g_App->recentFileCount)
				{
					TCHAR recentPath[MAX_PATH];
					SafeFormat(recentPath, MAX_PATH, _T("%s"), g_App->recentFiles[idx]);
					if (!SeedSinglePlaylistAndPlay(hWnd, recentPath) && !g_App->bMidiInEnabled)
					{
						RemoveRecentFile(recentPath);
					}
				}
			}
			return 0;
		case ID_FILE_CLEARRECENTFILES:
			ClearRecentFiles();
			RebuildRecentFilesMenu();
			return 0;
		case ID_FILE_OPENPLAYLIST:
			OpenM3UPlaylistDialog(hWnd);
			return 0;
		case ID_FILE_SAVEPLAYLIST:
			SaveM3UPlaylistDialog(hWnd);
			return 0;
		case ID_FILE_UNLOAD:
			UnloadCurrentFile(hWnd);
			return 0;
		case ID_FILE_CLEARPLAYLIST:
			ClearPlaylist(hWnd);
			return 0;
		case ID_FILE_CONVERTCURRENT:
			ConvertCurrentToWav(hWnd);
			return 0;
		case ID_FILE_BATCHCONVERT:
			if (g_App->bQuickConverting)
			{
				return 0;
			}
			if (g_App->playlistCount > 1 && PlaylistHasNewBatchFiles())
			{
				int answer = ShowFormattedAppMessage(hWnd, IDS_ADDQUEUETOBATCH, MB_ICONQUESTION | MB_YESNOCANCEL, g_App->playlistCount);
				if (answer == IDYES)
				{
					AddPlaylistToBatch();
				}
				else if (answer == IDCANCEL)
				{
					return 0;
				}
			}
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_BATCH), hWnd, (DLGPROC)BatchDialogProc);
			return 0;
		case ID_FILE_EXIT:
			g_App->bExitRequested = TRUE;
			SendMessage(hWnd, WM_CLOSE, 0, 0);
			return 0;
		case ID_PLAYBACK_PLAYPAUSE:
			TogglePlayPause(hWnd);
			return 0;
		case ID_PLAYBACK_STOP:
			StopPlaybackAndSync(hWnd);
			return 0;
		case ID_PLAYBACK_NEXTTRACK:
			NextTrack(hWnd);
			return 0;
		case ID_PLAYBACK_PREVTRACK:
			PreviousTrack(hWnd);
			return 0;
		case ID_PLAYBACK_FIRSTTRACK:
			FirstTrack(hWnd);
			return 0;
		case ID_PLAYBACK_LASTTRACK:
			LastTrack(hWnd);
			return 0;
		case ID_SEEK_FORWARD:
			SeekRelative(SEEK_STEP_MS);
			return 0;
		case ID_SEEK_BACK:
			SeekRelative(-SEEK_STEP_MS);
			return 0;
		case ID_PLAYBACK_RESTART:
			RestartTrack();
			return 0;
		case ID_PLAYBACK_JUMPTOEND:
			JumpToTrackEnd();
			return 0;
		case ID_PLAYBACK_JUMPTOTIME:
			if (!g_App->bFileLoaded)
			{
				return 0;
			}
			g_App->jumpTargetMs = -1;
			if (ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_JUMPTIME), hWnd, (DLGPROC)JumpTimeDialogProc) && g_App->jumpTargetMs >= 0)
			{
				SeekAbsolute(g_App->jumpTargetMs);
			}
			return 0;
		case ID_PLAYBACK_SLEEPTIMER:
			if (g_App->bMidiInEnabled)
			{
				return 0;
			}
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_SLEEPTIMER), hWnd, (DLGPROC)SleepTimerDialogProc);
			return 0;
		case ID_PLAYBACK_SHUFFLE:
			ShufflePlaylist(hWnd);
			return 0;
		case ID_PLAYBACK_MANAGEPLAYLIST:
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_PLAYLISTMGR), hWnd, (DLGPROC)PlaylistMgrDialogProc);
			return 0;
		case ID_PLAYBACK_REPEATOFF:
			g_App->repeatMode = REPEAT_OFF;
			UpdateMenuState(hWnd);
			return 0;
		case ID_PLAYBACK_REPEATONE:
			g_App->repeatMode = REPEAT_ONE;
			UpdateMenuState(hWnd);
			return 0;
		case ID_PLAYBACK_REPEATALL:
			g_App->repeatMode = REPEAT_ALL;
			UpdateMenuState(hWnd);
			return 0;
		case ID_PLAYBACK_STATS:
			if (!g_App->bFileLoaded)
			{
				return 0;
			}
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_STATS), hWnd, (DLGPROC)StatsDialogProc);
			return 0;
		case ID_VOLUME_UP:
			ChangeVolume(VOLUME_STEP);
			return 0;
		case ID_VOLUME_DOWN:
			ChangeVolume(-VOLUME_STEP);
			return 0;
		case ID_CONFIGURE_TIMIDITY:
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_CONFIG), hWnd, (DLGPROC)ConfigDialogProc);
			return 0;
		case ID_CONFIGURE_AUDIOOUTPUT:
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_AUDIOOUTPUT), hWnd, (DLGPROC)AudioOutputDialogProc);
			return 0;
		case ID_CONFIGURE_PLAYEROPTIONS:
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_PLAYEROPTIONS), hWnd, (DLGPROC)PlayerOptionsDialogProc);
			return 0;
		case ID_CONFIGURE_MIDIINPUT:
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_MIDIINPUT), hWnd, (DLGPROC)MidiInputDialogProc);
			return 0;
		case ID_CONFIGURE_EXPORTSETTINGS:
			{
				TCHAR path[MAX_PATH];
				ZeroMemory(path, sizeof(path));
				if (!PromptFileName(hWnd, TRUE, path, MAX_PATH, IDS_SETTINGSFLT, IDS_SETTINGSSAVECAP, _T("tpset"), NULL, OFN_OVERWRITEPROMPT))
				{
					return 0;
				}
				if (ExportPlayerSettings(path))
				{
					ShowAppStringMessage(hWnd, IDS_SETTINGSEXPORTED, MB_ICONINFORMATION);
				}
				else
				{
					ShowAppStringMessage(hWnd, IDS_EXPORTSETTINGSFAILED, MB_ICONERROR);
				}
			}
			return 0;
		case ID_CONFIGURE_IMPORTSETTINGS:
			{
				TCHAR path[MAX_PATH];
				ZeroMemory(path, sizeof(path));
				if (!PromptFileName(hWnd, FALSE, path, MAX_PATH, IDS_SETTINGSFLT, IDS_SETTINGSOPENCAP, _T("tpset"), NULL, OFN_FILEMUSTEXIST))
				{
					return 0;
				}
				if (ConfirmAppString(hWnd, IDS_CONFIRMIMPORTSETTINGS))
				{
					if (ImportPlayerSettings(path))
					{
						ShowAppStringMessage(hWnd, IDS_SETTINGSIMPORTED, MB_ICONINFORMATION);
					}
					else
					{
						ShowAppStringMessage(hWnd, IDS_IMPORTSETTINGSFAILED, MB_ICONERROR);
					}
				}
			}
			return 0;
		case ID_CONFIGURE_RESTOREDEFAULTS:
			if (ConfirmAppString(hWnd, IDS_CONFIRMRESTOREDEFAULTS))
			{
				if (DeletePlayerSettings())
				{
					ShowAppStringMessage(hWnd, IDS_SETTINGSRESTORED, MB_ICONINFORMATION);
				}
				else
				{
					ShowAppStringMessage(hWnd, IDS_DELETESETTINGSFAILED, MB_ICONERROR);
				}
			}
			return 0;
		case ID_TOOLS_CHANNELMIXER:
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_CHANMIX), hWnd, (DLGPROC)ChannelMixerDialogProc);
			return 0;
		case ID_TOOLS_VKBD:
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_VKBD), hWnd, (DLGPROC)VirtualKeyboardDialogProc);
			return 0;
		case ID_TOOLS_INJECTMIDI:
			ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_INJECTMIDI), hWnd, (DLGPROC)InjectMidiDialogProc);
			return 0;
		case ID_RESET_ALLNOTESOFF:
			HandleMidiInReset(MIDIIN_RESET_ALLNOTESOFF);
			return 0;
		case ID_RESET_ALLSOUNDSOFF:
			HandleMidiInReset(MIDIIN_RESET_ALLSOUNDSOFF);
			return 0;
		case ID_RESET_RESETCONTROLLERS:
			HandleMidiInReset(MIDIIN_RESET_CONTROLLERS);
			return 0;
		case ID_RESET_PANIC:
			HandleMidiInReset(MIDIIN_RESET_PANIC);
			return 0;
		case ID_RESET_RESET:
			HandleMidiInReset(MIDIIN_RESET_FULL);
			return 0;
		case ID_ENGINE_RELOADCONFIG:
			ReloadEngineConfig();
			return 0;
		case ID_ENGINE_RELOADSMF:
			ReloadCurrentFile(hWnd);
			return 0;
		case ID_ENGINE_FORCEINSTRUMENTLOAD:
			ForceInstrumentLoad();
			return 0;
		case ID_ENGINE_FREEDEFAULTINSTRUMENT:
			FreeDefaultInstrument();
			return 0;
		case ID_ENGINE_UNLOADCONFIG:
			UnloadEngineConfig();
			return 0;
		case ID_ENGINE_RESTOREDEFAULTS:
			RestoreEngineDefaults();
			return 0;
		case ID_VIEW_STATUSBAR:
			ToggleViewFlag(hWnd, &g_App->bShowStatusBar);
			return 0;
		case ID_VIEW_SEEKBAR:
			ToggleViewFlag(hWnd, &g_App->bShowSeekBar);
			return 0;
		case ID_VIEW_PLAYLISTVIEW:
			ToggleViewFlag(hWnd, &g_App->bShowPlaylistView);
			return 0;
		case ID_HELP_ABOUT:
			ShowAboutBox(hWnd);
			return 0;
		case ID_TRAY_RESTORE:
			ShowWindow(hWnd, SW_RESTORE);
			SetForegroundWindow(hWnd);
			return 0;
		case ID_CONTEXT_PLAY:
			{
				int idx = g_App->playlistIndex;
				if (idx >= 0 && idx < g_App->playlistCount)
				{
					PlayPlaylistEntryAt(hWnd, idx);
				}
			}
			return 0;
		case ID_CONTEXT_REMOVE:
			{
				int idx = g_App->playlistIndex;
				if (idx >= 0 && idx < g_App->playlistCount)
				{
					BOOL bRemove = TRUE;
					if (g_App->bConfirmPlaylistDelete)
					{
						bRemove = (ShowFormattedAppMessage(hWnd, IDS_CONFIRMDELETEPLAYLISTITEM, MB_ICONQUESTION | MB_YESNO, GetBaseName(g_App->playlist[idx].path)) == IDYES);
					}
					if (bRemove)
					{
						RemovePlaylistItemAt(hWnd, idx);
					}
				}
			}
			return 0;
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xFFF0) == IDM_ALWAYSONTOP)
		{
			HWND insertAfter;
			UINT check;
			g_App->bAlwaysOnTop = !g_App->bAlwaysOnTop;
			if (g_App->bAlwaysOnTop)
			{
				insertAfter = HWND_TOPMOST;
				check = MF_CHECKED;
			}
			else
			{
				insertAfter = HWND_NOTOPMOST;
				check = MF_UNCHECKED;
			}
			SetWindowPos(hWnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
			CheckMenuItem(GetSystemMenu(hWnd, FALSE), IDM_ALWAYSONTOP, MF_BYCOMMAND | check);
			SaveAlwaysOnTopSetting();
			return 0;
		}
		if (g_App->bRunFromTray && (wParam & 0xFFF0) == SC_MINIMIZE)
		{
			ShowWindow(hWnd, SW_HIDE);
			return 0;
		}
		break;
	case WM_APPCOMMAND:
		{
			int ai;
			int appCmd = GET_APPCOMMAND_LPARAM(lParam);
			for (ai = 0; ai < (int)NUM_APPCOMMAND_MAPPINGS; ai++)
			{
				if (appCmd == g_AppCommandMap[ai].appCmd)
				{
					SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(g_AppCommandMap[ai].commandId, 0), 0);
					return TRUE;
				}
			}
		}
		break;
	case WM_HOTKEY:
		{
			int hi;
			for (hi = 0; hi < (int)NUM_GLOBAL_HOTKEYS; hi++)
			{
				if ((int)wParam == g_GlobalHotkeys[hi].id)
				{
					SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(g_GlobalHotkeys[hi].commandId, 0), 0);
					break;
				}
			}
		}
		return 0;
	case WM_TRAYICON:
		switch (lParam)
		{
		case WM_LBUTTONDBLCLK:
			ShowWindow(hWnd, SW_RESTORE);
			SetForegroundWindow(hWnd);
			return 0;
		case WM_RBUTTONUP:
			ShowTrayMenu(hWnd);
			return 0;
		}
		return 0;
	case WM_CONTEXTMENU:
		if (ShowContextMenu(hWnd, lParam))
		{
			return 0;
		}
		break;
	case MM_MIM_DATA:
		HandleMidiInShortMessage((DWORD_PTR)lParam);
		return 0;
	case MM_MIM_LONGDATA:
		HandleMidiInLongMessage((MIDIHDR *)lParam);
		return 0;
	case WM_PLAYBACK_ENDED:
		if ((DWORD)wParam != g_App->playbackGeneration)
		{
			return 0;
		}
		if (g_App->repeatMode != REPEAT_ONE && g_App->playlistIndex >= 0 && g_App->playlistIndex + 1 < g_App->playlistCount)
		{
			NextTrack(hWnd);
		}
		else if (g_App->repeatMode == REPEAT_ONE && g_App->playlistIndex >= 0 && g_App->playlistIndex < g_App->playlistCount)
		{
			LoadAndPlayFile(hWnd, g_App->playlist[g_App->playlistIndex].path);
		}
		else if (g_App->repeatMode == REPEAT_ALL && g_App->playlistCount > 0)
		{
			g_App->playlistIndex = 0;
			LoadAndPlayFile(hWnd, g_App->playlist[g_App->playlistIndex].path);
		}
		else
		{
			StopPlaybackAndSync(hWnd);
		}
		return 0;
	case WM_QUICKCONVERT_PROGRESS:
		{
			TCHAR fmt[64];
			TCHAR text[64];
			LoadAppString(g_App->hInst, IDS_CONVERTINGWAVFMT, fmt, 64);
			SafeFormat(text, 64, fmt, (int)wParam);
			SendMessage(g_App->hStatusBar, SB_SETTEXT, 0, (LPARAM)text);
		}
		return 0;
	case WM_QUICKCONVERT_DONE:
		{
			QuickConvertResult *result = (QuickConvertResult *)lParam;
			CloseHandleAndClear(&g_App->hQuickConvertThread);
			g_App->bQuickConverting = FALSE;
			if (result)
			{
				if (IsWindowEnabled(hWnd))
				{
					ShowQuickConvertResult(hWnd, result);
					free(result);
				}
				else
				{
					if (g_App->pDeferredQuickConvertResult)
					{
						free(g_App->pDeferredQuickConvertResult);
					}
					g_App->pDeferredQuickConvertResult = result;
				}
			}
			RefreshPlayerUI(hWnd);
		}
		return 0;
	case WM_CLOSE:
		if (g_App->bRunFromTray && !g_App->bExitRequested)
		{
			ShowWindow(hWnd, SW_HIDE);
			return 0;
		}
		if (BlockCloseIfBusy(hWnd, &g_App->bQuickConverting, IDS_QUICKCONVERTINPROGRESS))
		{
			return 0;
		}
		if (BlockCloseIfBusy(hWnd, &g_App->bConverting, IDS_BATCHCONVERTINPROGRESS))
		{
			return 0;
		}
		if (g_App->bSettingsRestored && g_App->bLoadPreviousPlaylist && g_App->playlistCount > 0 && !g_App->bPlaylistSavedManually)
		{
			if (ConfirmAppString(hWnd, IDS_CONFIRMSAVEPLAYLISTONEXIT))
			{
				SaveM3UPlaylistDialog(hWnd);
			}
		}
		DestroyWindow(hWnd);
		return 0;
	case WM_DESTROY:
		UnregisterGlobalHotkeys(hWnd);
		RemoveTrayIcon(hWnd);
		CloseMidiInDevice();
		KillTimer(hWnd, TIMER_ID_POSITION);
		KillTimer(hWnd, TIMER_ID_SLEEP);
		if (g_App->pDeferredQuickConvertResult)
		{
			free(g_App->pDeferredQuickConvertResult);
			g_App->pDeferredQuickConvertResult = NULL;
		}
		SaveAudioOutputSettings();
		SaveSession();
		StopPlayback();
		g_App->bFileLoaded = FALSE;
		EnterCriticalSection(&g_App->synthCS);
		if (g_App->synth)
		{
			timid_close(g_App->synth);
			g_App->synth = NULL;
		}
		LeaveCriticalSection(&g_App->synthCS);
		DeleteCriticalSection(&g_App->synthCS);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}
