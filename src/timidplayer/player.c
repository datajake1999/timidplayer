#include "timidplayer.h"

#define SLEEPTIMER_MAX_MINUTES 1440

#ifdef UNICODE
#define CF_TCHARTEXT CF_UNICODETEXT
#else
#define CF_TCHARTEXT CF_TEXT
#endif

void StartPendingSleepTimer(void)
{
	if (g_App->sleepTimerMinutes > 0 && !g_App->bSleepTimerRunning)
	{
		DWORD durationMs;
		if (g_App->sleepTimerRemainingMs > 0)
		{
			durationMs = g_App->sleepTimerRemainingMs;
		}
		else
		{
			durationMs = (DWORD)g_App->sleepTimerMinutes * 60000;
		}
		SetTimer(g_App->hPlayerWnd, TIMER_ID_SLEEP, durationMs, NULL);
		g_App->sleepTimerEndTick = GetTickCount() + durationMs;
		g_App->bSleepTimerRunning = TRUE;
		g_App->sleepTimerRemainingMs = 0;
	}
}

void CancelSleepTimer(void)
{
	if (g_App->bSleepTimerRunning)
	{
		KillTimer(g_App->hPlayerWnd, TIMER_ID_SLEEP);
		g_App->bSleepTimerRunning = FALSE;
	}
	g_App->sleepTimerRemainingMs = 0;
}

static DWORD GetSleepTimerRemainingMs(void)
{
	DWORD now = GetTickCount();
	LONG remainSigned = (LONG)(g_App->sleepTimerEndTick - now);
	if (remainSigned > 0)
	{
		return (DWORD)remainSigned;
	}
	return 0;
}

static void PauseSleepTimer(void)
{
	if (g_App->bSleepTimerRunning)
	{
		DWORD remain = GetSleepTimerRemainingMs();
		KillTimer(g_App->hPlayerWnd, TIMER_ID_SLEEP);
		g_App->bSleepTimerRunning = FALSE;
		if (remain > 0)
		{
			g_App->sleepTimerRemainingMs = remain;
		}
		else
		{
			g_App->sleepTimerRemainingMs = 1;
		}
	}
}

BOOL WINAPI JumpTimeDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	switch (message)
	{
	case WM_INITDIALOG:
		{
			int curMs = 0;
			TCHAR buf[16];
			if (g_App->bFileLoaded)
			{
				EnterCriticalSection(&g_App->synthCS);
				curMs = timid_get_current_time(g_App->synth);
				LeaveCriticalSection(&g_App->synthCS);
			}
			FormatTimeHMS(curMs, buf, 16);
			SetDlgItemText(hWnd, IDC_JUMPTIME, buf);
			SendDlgItemMessage(hWnd, IDC_JUMPTIME, EM_SETSEL, 0, -1);
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				TCHAR buf[32];
				int h, m, s;
				double totalMs;
				GetDlgItemText(hWnd, IDC_JUMPTIME, buf, 32);
				if (_stscanf(buf, _T("%9d:%9d:%9d"), &h, &m, &s) != 3 || h < 0 || m < 0 || s < 0)
				{
					ShowAppStringMessage(hWnd, IDS_JUMPTIMEFORMAT, MB_ICONWARNING);
					return TRUE;
				}
				totalMs = (((double)h * 3600.0) + ((double)m * 60.0) + (double)s) * 1000.0;
				if (totalMs > (double)INT_MAX)
				{
					totalMs = (double)INT_MAX;
				}
				g_App->jumpTargetMs = (int)totalMs;
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

BOOL WINAPI SleepTimerDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	switch (message)
	{
	case WM_INITDIALOG:
		{
			TCHAR buf[16];
			int mins = 0;
			if (g_App->bSleepTimerRunning)
			{
				DWORD remain = GetSleepTimerRemainingMs();
				mins = (int)((remain + 59999) / 60000);
			}
			else if (g_App->sleepTimerMinutes > 0)
			{
				mins = g_App->sleepTimerMinutes;
			}
			SafeFormat(buf, 16, _T("%d"), mins);
			SetDlgItemText(hWnd, IDC_SLEEPMINUTES, buf);
			SendDlgItemMessage(hWnd, IDC_SLEEPMINUTES, EM_SETSEL, 0, -1);
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			{
				BOOL translated = FALSE;
				int mins = (int)GetDlgItemInt(hWnd, IDC_SLEEPMINUTES, &translated, FALSE);
				if (!translated)
				{
					ShowAppStringMessage(hWnd, IDS_SLEEPMINUTESFORMAT, MB_ICONWARNING);
					return TRUE;
				}
				if (mins < 0 || mins > SLEEPTIMER_MAX_MINUTES)
				{
					mins = SLEEPTIMER_MAX_MINUTES;
				}
				if (mins == 0)
				{
					CancelSleepTimer();
					g_App->sleepTimerMinutes = 0;
				}
				else
				{
					g_App->sleepTimerMinutes = mins;
					if (g_App->state == PLAYER_PLAYING)
					{
						SetTimer(g_App->hPlayerWnd, TIMER_ID_SLEEP, (UINT)mins * 60000, NULL);
						g_App->sleepTimerEndTick = GetTickCount() + (DWORD)mins * 60000;
						g_App->bSleepTimerRunning = TRUE;
					}
					else
					{
						CancelSleepTimer();
					}
				}
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

BOOL WINAPI PlayerOptionsDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	switch (message)
	{
	case WM_INITDIALOG:
		if (g_App->bLoadPreviousPlaylist)
		{
			CheckDlgButton(hWnd, IDC_OPT_LOADPLAYLIST, BST_CHECKED);
		}
		RefreshLoadFilesIntoRunningInstanceSetting();
		if (g_App->bLoadFilesIntoRunningInstance)
		{
			CheckDlgButton(hWnd, IDC_OPT_LOADINTORUNNING, BST_CHECKED);
		}
		if (g_App->bShowFilenameInTitle)
		{
			CheckDlgButton(hWnd, IDC_OPT_SHOWFILENAMEINTITLE, BST_CHECKED);
		}
		if (g_App->bPreventDuplicatePlaylistItems)
		{
			CheckDlgButton(hWnd, IDC_OPT_PREVENTDUPLICATES, BST_CHECKED);
		}
		if (g_App->bSaveTrack)
		{
			CheckDlgButton(hWnd, IDC_OPT_SAVETRACK, BST_CHECKED);
		}
		if (g_App->bSavePosition)
		{
			CheckDlgButton(hWnd, IDC_OPT_SAVEPOSITION, BST_CHECKED);
		}
		if (g_App->bAutoStartPlayback)
		{
			CheckDlgButton(hWnd, IDC_OPT_AUTOSTART, BST_CHECKED);
		}
		if (g_App->bRemoveOnUnload)
		{
			CheckDlgButton(hWnd, IDC_OPT_REMOVEONUNLOAD, BST_CHECKED);
		}
		if (g_App->bConfirmPlaylistDelete)
		{
			CheckDlgButton(hWnd, IDC_OPT_CONFIRMDELETE, BST_CHECKED);
		}
		if (g_App->bPreventSeekTrackChange)
		{
			CheckDlgButton(hWnd, IDC_OPT_PREVENTSEEKCHANGE, BST_CHECKED);
		}
		if (g_App->bAutoscrollPlaylist)
		{
			CheckDlgButton(hWnd, IDC_OPT_AUTOSCROLLPLAYLIST, BST_CHECKED);
		}
		if (g_App->bRunFromTray)
		{
			CheckDlgButton(hWnd, IDC_OPT_RUNFROMTRAY, BST_CHECKED);
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			g_App->bLoadPreviousPlaylist = (IsDlgButtonChecked(hWnd, IDC_OPT_LOADPLAYLIST) == BST_CHECKED);
			g_App->bLoadFilesIntoRunningInstance = (IsDlgButtonChecked(hWnd, IDC_OPT_LOADINTORUNNING) == BST_CHECKED);
			g_App->bShowFilenameInTitle = (IsDlgButtonChecked(hWnd, IDC_OPT_SHOWFILENAMEINTITLE) == BST_CHECKED);
			g_App->bPreventDuplicatePlaylistItems = (IsDlgButtonChecked(hWnd, IDC_OPT_PREVENTDUPLICATES) == BST_CHECKED);
			g_App->bSaveTrack = (IsDlgButtonChecked(hWnd, IDC_OPT_SAVETRACK) == BST_CHECKED);
			g_App->bSavePosition = (IsDlgButtonChecked(hWnd, IDC_OPT_SAVEPOSITION) == BST_CHECKED);
			g_App->bAutoStartPlayback = (IsDlgButtonChecked(hWnd, IDC_OPT_AUTOSTART) == BST_CHECKED);
			g_App->bRemoveOnUnload = (IsDlgButtonChecked(hWnd, IDC_OPT_REMOVEONUNLOAD) == BST_CHECKED);
			g_App->bConfirmPlaylistDelete = (IsDlgButtonChecked(hWnd, IDC_OPT_CONFIRMDELETE) == BST_CHECKED);
			g_App->bPreventSeekTrackChange = (IsDlgButtonChecked(hWnd, IDC_OPT_PREVENTSEEKCHANGE) == BST_CHECKED);
			g_App->bAutoscrollPlaylist = (IsDlgButtonChecked(hWnd, IDC_OPT_AUTOSCROLLPLAYLIST) == BST_CHECKED);
			g_App->bRunFromTray = (IsDlgButtonChecked(hWnd, IDC_OPT_RUNFROMTRAY) == BST_CHECKED);
			SavePlayerOptions();
			SyncTrayIcon(g_App->hPlayerWnd);
			UpdateWindowTitle(g_App->hPlayerWnd);
			EndDialog(hWnd, TRUE);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWnd, FALSE);
			return TRUE;
		}
	}
	return FALSE;
}

void RefreshPlayerUI(HWND hWnd)
{
	UpdateMenuState(hWnd);
	UpdateStatusBar();
}

static void UnloadSmfInternal(void)
{
	EnterCriticalSection(&g_App->synthCS);
	timid_unload_smf(g_App->synth);
	LeaveCriticalSection(&g_App->synthCS);
	g_App->bFileLoaded = FALSE;
	g_App->title[0] = _T('\0');
	g_App->copyright[0] = _T('\0');
}

static void UnloadLoadedFile(HWND hWnd)
{
	if (!g_App->bFileLoaded)
	{
		return;
	}
	StopPlayback();
	ResumeMidiInputIfIdle();
	UnloadSmfInternal();
	g_App->loadedFilePath[0] = _T('\0');
	UpdateWindowTitle(hWnd);
}

void UnloadCurrentFile(HWND hWnd)
{
	int idx = g_App->playlistIndex;
	UnloadLoadedFile(hWnd);
	if (g_App->bRemoveOnUnload && idx >= 0 && idx < g_App->playlistCount)
	{
		RemovePlaylistItemAt(hWnd, idx);
	}
	else
	{
		g_App->playlistIndex = -1;
		RefreshPlayerUI(hWnd);
	}
}

static void MarkPlaylistModified(void)
{
	g_App->bPlaylistSavedManually = FALSE;
}

void ClearPlaylist(HWND hWnd)
{
	UnloadLoadedFile(hWnd);
	g_App->playlistCount = 0;
	g_App->playlistIndex = -1;
	MarkPlaylistModified();
	RefreshPlayerUI(hWnd);
}

void RebuildRecentFilesMenu(void)
{
	int i, count;
	TCHAR emptyLabel[32];
	if (!g_App->hRecentMenu)
	{
		return;
	}
	while (GetMenuItemCount(g_App->hRecentMenu) > 0)
	{
		RemoveMenu(g_App->hRecentMenu, 0, MF_BYPOSITION);
	}
	if (g_App->recentFileCount == 0)
	{
		LoadAppString(g_App->hInst, IDS_RECENTEMPTY, emptyLabel, 32);
		AppendMenu(g_App->hRecentMenu, MF_STRING | MF_GRAYED, ID_FILE_RECENT_EMPTY, emptyLabel);
		return;
	}
	if (g_App->recentFileCount < MAX_RECENT_FILES)
	{
		count = g_App->recentFileCount;
	}
	else
	{
		count = MAX_RECENT_FILES;
	}
	for (i = 0; i < count; i++)
	{
		TCHAR label[MAX_PATH + 8];
		const TCHAR *fname = GetBaseName(g_App->recentFiles[i]);
		SafeFormat(label, MAX_PATH + 8, _T("&%d %s"), i + 1, fname);
		AppendMenu(g_App->hRecentMenu, MF_STRING, ID_FILE_RECENT1 + i, label);
	}
}

static int FindRecentFileIndex(const TCHAR *path)
{
	int i;
	for (i = 0; i < g_App->recentFileCount; i++)
	{
		if (_tcsicmp(g_App->recentFiles[i], path) == 0)
		{
			break;
		}
	}
	return i;
}

static void AddRecentFile(const TCHAR *path)
{
	int i, dst;
	TCHAR tmp[MAX_PATH];
	i = FindRecentFileIndex(path);
	if (i < g_App->recentFileCount)
	{
		SafeFormat(tmp, MAX_PATH, _T("%s"), g_App->recentFiles[i]);
		MoveMemory(&g_App->recentFiles[1], &g_App->recentFiles[0], sizeof(TCHAR) * MAX_PATH * i);
		SafeFormat(g_App->recentFiles[0], MAX_PATH, _T("%s"), tmp);
	}
	else
	{
		if (g_App->recentFileCount < MAX_RECENT_FILES)
		{
			dst = g_App->recentFileCount;
		}
		else
		{
			dst = MAX_RECENT_FILES - 1;
		}
		if (g_App->recentFileCount < MAX_RECENT_FILES)
		{
			g_App->recentFileCount++;
		}
		MoveMemory(&g_App->recentFiles[1], &g_App->recentFiles[0], sizeof(TCHAR) * MAX_PATH * dst);
		SafeFormat(g_App->recentFiles[0], MAX_PATH, _T("%s"), path);
	}
	SaveRecentFiles();
	RebuildRecentFilesMenu();
}

void RemoveRecentFile(const TCHAR *path)
{
	int i;
	i = FindRecentFileIndex(path);
	if (i >= g_App->recentFileCount)
	{
		return;
	}
	MoveMemory(&g_App->recentFiles[i], &g_App->recentFiles[i+1], sizeof(TCHAR) * MAX_PATH * (g_App->recentFileCount - i - 1));
	g_App->recentFileCount--;
	SaveRecentFiles();
	RebuildRecentFilesMenu();
}

static void RemovePlaylistArrayEntry(int idx)
{
	MoveMemory(&g_App->playlist[idx], &g_App->playlist[idx+1], sizeof(FileEntry) * (g_App->playlistCount-idx-1));
	g_App->playlistCount--;
	MarkPlaylistModified();
}

static BOOL TryLoadAndPlayFile(HWND hWnd, const TCHAR *path)
{
	char ansiPath[MAX_PATH];
	BOOL loadOk;
	if (!g_App->synth)
	{
		RefreshSynth();
	}
	if (!g_App->synth)
	{
		return FALSE;
	}
	StopPlayback();
	if (g_App->bFileLoaded)
	{
		UnloadSmfInternal();
	}
	TCharToAnsi(path, ansiPath, MAX_PATH);
	EnterCriticalSection(&g_App->synthCS);
	if (timid_load_smf(g_App->synth, ansiPath))
	{
		loadOk = TRUE;
	}
	else
	{
		loadOk = FALSE;
	}
	if (loadOk)
	{
		GetSongMetadata(g_App->synth, g_App->title, g_App->copyright);
		if (g_App->pendingSeekMs >= 0)
		{
			timid_seek_smf(g_App->synth, g_App->pendingSeekMs);
		}
	}
	LeaveCriticalSection(&g_App->synthCS);
	g_App->pendingSeekMs = -1;
	if (!loadOk)
	{
		if (IsWindowEnabled(hWnd))
		{
			ShowFormattedAppMessage(hWnd, IDS_LOADFAILED, MB_ICONERROR, path);
		}
		UpdateWindowTitle(hWnd);
		if (g_App->playlistIndex >= 0 && g_App->playlistIndex < g_App->playlistCount && _tcsicmp(g_App->playlist[g_App->playlistIndex].path, path) == 0)
		{
			RemovePlaylistArrayEntry(g_App->playlistIndex);
			if (g_App->playlistIndex >= g_App->playlistCount)
			{
				g_App->playlistIndex = -1;
			}
		}
		else
		{
			g_App->playlistIndex = -1;
		}
		RefreshPlayerUI(hWnd);
		return FALSE;
	}
	g_App->bFileLoaded = TRUE;
	SafeFormat(g_App->loadedFilePath, MAX_PATH, _T("%s"), path);
	UpdateWindowTitle(hWnd);
	AddRecentFile(path);
	if (g_App->bAutoStartPlayback)
	{
		StartPlayback(hWnd);
	}
	RefreshPlayerUI(hWnd);
	return TRUE;
}

void ReloadCurrentFile(HWND hWnd)
{
	if (!g_App->bFileLoaded || !g_App->synth)
	{
		return;
	}
	StopPlayback();
	EnterCriticalSection(&g_App->synthCS);
	if (!timid_reload_smf(g_App->synth))
	{
		LeaveCriticalSection(&g_App->synthCS);
		return;
	}
	GetSongMetadata(g_App->synth, g_App->title, g_App->copyright);
	LeaveCriticalSection(&g_App->synthCS);
	UpdateWindowTitle(hWnd);
	UpdateStatusBar();
}

BOOL PlayPlaylistEntryAt(HWND hWnd, int index)
{
	if (g_App->bMidiInEnabled)
	{
		return FALSE;
	}
	g_App->playlistIndex = index;
	return LoadAndPlayFile(hWnd, g_App->playlist[index].path);
}

static BOOL PlayFirstPlaylistEntry(HWND hWnd)
{
	return PlayPlaylistEntryAt(hWnd, 0);
}

BOOL LoadAndPlayFile(HWND hWnd, const TCHAR *path)
{
	TCHAR attempt[MAX_PATH];
	if (g_App->bMidiInEnabled)
	{
		return FALSE;
	}
	SafeFormat(attempt, MAX_PATH, _T("%s"), path);
	for (;;)
	{
		if (TryLoadAndPlayFile(hWnd, attempt))
		{
			return TRUE;
		}
		if (!g_App->synth)
		{
			return FALSE;
		}
		if (g_App->playlistIndex < 0 || g_App->playlistIndex >= g_App->playlistCount)
		{
			g_App->playlistIndex = -1;
			return FALSE;
		}
		SafeFormat(attempt, MAX_PATH, _T("%s"), g_App->playlist[g_App->playlistIndex].path);
	}
}

BOOL SeedSinglePlaylistAndPlay(HWND hWnd, const TCHAR *path)
{
	SafeFormat(g_App->playlist[0].path, MAX_PATH, _T("%s"), path);
	g_App->playlistCount = 1;
	MarkPlaylistModified();
	return PlayFirstPlaylistEntry(hWnd);
}

static BOOL IsCmdLineEnqueueSwitch(const TCHAR *token)
{
	return _tcsicmp(token, CMDLINE_ENQUEUE_SWITCH) == 0;
}

static BOOL PlaylistContainsPath(const TCHAR *path)
{
	int i;
	for (i = 0; i < g_App->playlistCount; i++)
	{
		if (_tcsicmp(g_App->playlist[i].path, path) == 0)
		{
			return TRUE;
		}
	}
	return FALSE;
}

static BOOL AppendPlaylistPath(const TCHAR *path)
{
	if (g_App->playlistCount >= MAX_PLAYLIST)
	{
		return FALSE;
	}
	if (g_App->bPreventDuplicatePlaylistItems && PlaylistContainsPath(path))
	{
		return FALSE;
	}
	SafeFormat(g_App->playlist[g_App->playlistCount].path, MAX_PATH, _T("%s"), path);
	g_App->playlistCount++;
	MarkPlaylistModified();
	return TRUE;
}

static BOOL IsM3UPlaylistPath(const TCHAR *path)
{
	TCHAR drive[_MAX_DRIVE], dirPart[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
	_tsplitpath(path, drive, dirPart, fname, ext);
	return _tcsicmp(ext, _T(".m3u")) == 0 || _tcsicmp(ext, _T(".m3u8")) == 0;
}

static int AppendM3UPlaylistEntries(const TCHAR *m3uPath);

static BOOL AppendDirectoryFileEntry(void *ctx, const TCHAR *path)
{
	(void)ctx;
	if (IsM3UPlaylistPath(path))
	{
		return AppendM3UPlaylistEntries(path) > 0;
	}
	return AppendPlaylistPath(path);
}

static BOOL AppendCmdLineToken(const TCHAR *token)
{
	return AppendPathOrDirectory(token, AppendDirectoryFileEntry, NULL);
}

void OpenCommandLinePath(HWND hWnd, const TCHAR *cmdPaths)
{
	const TCHAR *p = cmdPaths;
	BOOL any = FALSE;
	BOOL bEnqueue;
	BOOL wasStopped;
	int startIndex;
	if (!*p)
	{
		return;
	}
	bEnqueue = IsCmdLineEnqueueSwitch(p);
	if (bEnqueue)
	{
		p += _tcslen(p) + 1;
		if (!*p)
		{
			return;
		}
	}
	wasStopped = (g_App->state == PLAYER_STOPPED);
	if (!bEnqueue)
	{
		g_App->playlistCount = 0;
	}
	startIndex = g_App->playlistCount;
	while (*p && g_App->playlistCount < MAX_PLAYLIST)
	{
		if (AppendCmdLineToken(p))
		{
			any = TRUE;
		}
		p += _tcslen(p) + 1;
	}
	if (!any)
	{
		if (IsWindowEnabled(hWnd))
		{
			ShowAppStringMessage(hWnd, IDS_CMDLINE_NOFILES, MB_ICONERROR);
		}
		return;
	}
	if (!bEnqueue)
	{
		PlayFirstPlaylistEntry(hWnd);
	}
	else if (wasStopped && startIndex < g_App->playlistCount)
	{
		PlayPlaylistEntryAt(hWnd, startIndex);
	}
	RefreshPlayerUI(hWnd);
}

static void AppendPlaylistCallback(void *ctx, const TCHAR *path)
{
	(void)ctx;
	AppendPlaylistPath(path);
}

static void AppendPlaylistBuffer(TCHAR *buffer)
{
	ForEachMultiSelectPath(buffer, AppendPlaylistCallback, NULL);
}

static void ParsePlaylistBuffer(TCHAR *buffer)
{
	g_App->playlistCount = 0;
	AppendPlaylistBuffer(buffer);
}

static int AppendM3UPlaylistEntries(const TCHAR *m3uPath)
{
	FILE *f;
	TCHAR line[MAX_PATH];
	TCHAR dir[MAX_PATH];
	TCHAR fullPath[MAX_PATH];
	int added = 0;
	f = _tfopen(m3uPath, _T("r"));
	if (!f)
	{
		return 0;
	}
	GetDirectoryWithSlash(m3uPath, dir, MAX_PATH);
	while (g_App->playlistCount < MAX_PLAYLIST && _fgetts(line, MAX_PATH, f))
	{
		int len = (int)_tcslen(line);
		while (len > 0 && (line[len-1] == _T('\n') || line[len-1] == _T('\r')))
		{
			line[--len] = _T('\0');
		}
		if (len == 0 || line[0] == _T('#'))
		{
			continue;
		}
		if (!PathIsRelative(line))
		{
			SafeFormat(fullPath, MAX_PATH, _T("%s"), line);
		}
		else
		{
			SafeFormat(fullPath, MAX_PATH, _T("%s%s"), dir, line);
		}
		if (AppendPlaylistPath(fullPath))
		{
			added++;
		}
	}
	fclose(f);
	return added;
}

static BOOL LoadM3UPlaylist(const TCHAR *m3uPath)
{
	g_App->playlistCount = 0;
	return AppendM3UPlaylistEntries(m3uPath) > 0;
}

static BOOL SaveM3UPlaylist(const TCHAR *m3uPath)
{
	FILE *f;
	int i;
	if (g_App->playlistCount == 0)
	{
		return FALSE;
	}
	f = _tfopen(m3uPath, _T("w"));
	if (!f)
	{
		return FALSE;
	}
	_fputts(_T("#EXTM3U\n"), f);
	for (i = 0; i < g_App->playlistCount; i++)
	{
		_fputts(g_App->playlist[i].path, f);
		_fputts(_T("\n"), f);
	}
	fclose(f);
	return TRUE;
}

BOOL OpenMidiFilesDialog(HWND hWnd)
{
	TCHAR *buffer;
	buffer = PromptOpenMultipleMidiFiles(hWnd);
	if (!buffer)
	{
		return FALSE;
	}
	ParsePlaylistBuffer(buffer);
	free(buffer);
	if (g_App->playlistCount == 0)
	{
		return FALSE;
	}
	return PlayFirstPlaylistEntry(hWnd);
}

static BOOL FinishPlaylistAppend(HWND hWnd, BOOL wasEmpty)
{
	if (wasEmpty && g_App->playlistCount > 0)
	{
		return PlayFirstPlaylistEntry(hWnd);
	}
	RefreshPlayerUI(hWnd);
	return TRUE;
}

BOOL AddMidiFilesDialog(HWND hWnd)
{
	TCHAR *buffer;
	BOOL wasEmpty;
	buffer = PromptOpenMultipleMidiFiles(hWnd);
	if (!buffer)
	{
		return FALSE;
	}
	wasEmpty = (g_App->playlistCount == 0);
	AppendPlaylistBuffer(buffer);
	free(buffer);
	return FinishPlaylistAppend(hWnd, wasEmpty);
}

BOOL AddFolderDialog(HWND hWnd)
{
	TCHAR dir[MAX_PATH];
	BOOL wasEmpty;
	if (!PromptForFolder(hWnd, IDS_ADDFOLDERCAP, dir))
	{
		return FALSE;
	}
	wasEmpty = (g_App->playlistCount == 0);
	if (!WalkDirectoryFiles(dir, AppendDirectoryFileEntry, NULL))
	{
		return FALSE;
	}
	return FinishPlaylistAppend(hWnd, wasEmpty);
}

static BOOL AppendFilesFromDrop(HDROP hDrop)
{
	UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
	UINT i;
	BOOL any = FALSE;
	for (i = 0; i < count && g_App->playlistCount < MAX_PLAYLIST; i++)
	{
		TCHAR path[MAX_PATH];
		if (DragQueryFile(hDrop, i, path, MAX_PATH) && AppendCmdLineToken(path))
		{
			any = TRUE;
		}
	}
	return any;
}

static BOOL FinishDroppedOrPastedFiles(HWND hWnd, BOOL any, BOOL wasEmpty)
{
	if (!any)
	{
		return FALSE;
	}
	return FinishPlaylistAppend(hWnd, wasEmpty);
}

BOOL DropFiles(HWND hWnd, HDROP hDrop)
{
	BOOL wasEmpty = (g_App->playlistCount == 0);
	BOOL any = AppendFilesFromDrop(hDrop);
	DragFinish(hDrop);
	return FinishDroppedOrPastedFiles(hWnd, any, wasEmpty);
}

static BOOL AppendClipboardTextPaths(TCHAR *text)
{
	TCHAR *line = text;
	BOOL any = FALSE;
	while (line && g_App->playlistCount < MAX_PLAYLIST)
	{
		TCHAR *nextLine = _tcschr(line, _T('\n'));
		TCHAR *lineEnd;
		if (nextLine)
		{
			*nextLine = _T('\0');
			nextLine++;
		}
		while (*line == _T(' ') || *line == _T('\t'))
		{
			line++;
		}
		lineEnd = line + _tcslen(line);
		while (lineEnd > line && (lineEnd[-1] == _T('\r') || lineEnd[-1] == _T(' ') || lineEnd[-1] == _T('\t')))
		{
			*--lineEnd = _T('\0');
		}
		if (*line && AppendCmdLineToken(line))
		{
			any = TRUE;
		}
		line = nextLine;
	}
	return any;
}

BOOL ClipboardHasPasteableFiles(void)
{
	return IsClipboardFormatAvailable(CF_HDROP) || IsClipboardFormatAvailable(CF_TCHARTEXT);
}

BOOL PasteFilesFromClipboard(HWND hWnd)
{
	BOOL wasEmpty = (g_App->playlistCount == 0);
	BOOL any = FALSE;
	if (!OpenClipboard(hWnd))
	{
		return FALSE;
	}
	if (IsClipboardFormatAvailable(CF_HDROP))
	{
		HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
		if (hDrop)
		{
			any = AppendFilesFromDrop(hDrop);
		}
		CloseClipboard();
	}
	else if (IsClipboardFormatAvailable(CF_TCHARTEXT))
	{
		HANDLE hData = GetClipboardData(CF_TCHARTEXT);
		TCHAR *clipText;
		if (hData)
		{
			clipText = (TCHAR *)GlobalLock(hData);
		}
		else
		{
			clipText = NULL;
		}
		if (clipText)
		{
			size_t len = _tcslen(clipText);
			TCHAR *copy = (TCHAR *)malloc((len + 1) * sizeof(TCHAR));
			if (copy)
			{
				_tcscpy(copy, clipText);
				any = AppendClipboardTextPaths(copy);
				free(copy);
			}
			GlobalUnlock(hData);
		}
		CloseClipboard();
	}
	else
	{
		CloseClipboard();
		return FALSE;
	}
	return FinishDroppedOrPastedFiles(hWnd, any, wasEmpty);
}

BOOL OpenM3UPlaylistDialog(HWND hWnd)
{
	TCHAR path[MAX_PATH];
	ZeroMemory(path, sizeof(path));
	if (!PromptFileName(hWnd, FALSE, path, MAX_PATH, IDS_M3UFLT, IDS_M3UOPENCAP, _T("m3u"), NULL, OFN_FILEMUSTEXIST))
	{
		return FALSE;
	}
	if (!LoadM3UPlaylist(path))
	{
		ShowStringErrorWithCaption(hWnd, IDS_M3ULOADFAILED, IDS_M3UOPENCAP);
		return FALSE;
	}
	return PlayFirstPlaylistEntry(hWnd);
}

void SaveM3UPlaylistDialog(HWND hWnd)
{
	TCHAR path[MAX_PATH];
	if (g_App->playlistCount == 0)
	{
		ShowAppStringMessage(hWnd, IDS_M3UNOPLAYLIST, MB_ICONWARNING);
		return;
	}
	ZeroMemory(path, sizeof(path));
	if (!PromptFileName(hWnd, TRUE, path, MAX_PATH, IDS_M3UFLT, IDS_M3USAVECAP, _T("m3u"), NULL, OFN_OVERWRITEPROMPT))
	{
		return;
	}
	if (!SaveM3UPlaylist(path))
	{
		ShowStringErrorWithCaption(hWnd, IDS_M3USAVEFAILED, IDS_M3USAVECAP);
		return;
	}
	g_App->bPlaylistSavedManually = TRUE;
}

static void PlayTrackContinuingIfWasPlaying(HWND hWnd, const TCHAR *path)
{
	BOOL wasPlaying = (g_App->state == PLAYER_PLAYING);
	LoadAndPlayFile(hWnd, path);
	if (wasPlaying && g_App->bFileLoaded && g_App->state != PLAYER_PLAYING)
	{
		StartPlayback(hWnd);
	}
}

static BOOL CanNavigatePlaylist(void)
{
	return !g_App->bMidiInEnabled && g_App->playlistCount > 0;
}

static void StepTrack(HWND hWnd, int delta)
{
	if (!CanNavigatePlaylist())
	{
		return;
	}
	if (g_App->playlistIndex < 0)
	{
		if (delta > 0)
		{
			g_App->playlistIndex = 0;
		}
		else
		{
			g_App->playlistIndex = g_App->playlistCount - 1;
		}
	}
	else
	{
		int newIndex = g_App->playlistIndex + delta;
		if (newIndex < 0 || newIndex >= g_App->playlistCount)
		{
			return;
		}
		g_App->playlistIndex = newIndex;
	}
	PlayTrackContinuingIfWasPlaying(hWnd, g_App->playlist[g_App->playlistIndex].path);
}

static void JumpToTrack(HWND hWnd, int index)
{
	if (!CanNavigatePlaylist() || g_App->playlistIndex == index)
	{
		return;
	}
	g_App->playlistIndex = index;
	PlayTrackContinuingIfWasPlaying(hWnd, g_App->playlist[g_App->playlistIndex].path);
}

void NextTrack(HWND hWnd)
{
	StepTrack(hWnd, 1);
}

void PreviousTrack(HWND hWnd)
{
	StepTrack(hWnd, -1);
}

void FirstTrack(HWND hWnd)
{
	JumpToTrack(hWnd, 0);
}

void LastTrack(HWND hWnd)
{
	JumpToTrack(hWnd, g_App->playlistCount-1);
}

void TogglePlayPause(HWND hWnd)
{
	if (g_App->bMidiInEnabled)
	{
		return;
	}
	if (!g_App->bFileLoaded)
	{
		if (g_App->playlistCount > 0)
		{
			if (g_App->playlistIndex < 0 || g_App->playlistIndex >= g_App->playlistCount)
			{
				g_App->playlistIndex = 0;
			}
			LoadAndPlayFile(hWnd, g_App->playlist[g_App->playlistIndex].path);
		}
		return;
	}
	if (g_App->state == PLAYER_STOPPED)
	{
		StartPlayback(hWnd);
	}
	else if (g_App->state == PLAYER_PLAYING)
	{
		g_App->bPaused = TRUE;
		PauseWaveOutDevice();
		g_App->state = PLAYER_PAUSED;
		PauseSleepTimer();
	}
	else if (g_App->state == PLAYER_PAUSED)
	{
		g_App->bPaused = FALSE;
		ResumeWaveOutDevice();
		g_App->state = PLAYER_PLAYING;
		StartPendingSleepTimer();
	}
	RefreshPlayerUI(hWnd);
}

void ShufflePlaylist(HWND hWnd)
{
	int i;
	TCHAR currentPath[MAX_PATH];
	BOOL hadCurrent = FALSE;
	if (g_App->playlistCount <= 1)
	{
		return;
	}
	if (g_App->playlistIndex >= 0 && g_App->playlistIndex < g_App->playlistCount)
	{
		SafeFormat(currentPath, MAX_PATH, _T("%s"), g_App->playlist[g_App->playlistIndex].path);
		hadCurrent = TRUE;
	}
	for (i = g_App->playlistCount-1; i > 0; i--)
	{
		int j = rand() % (i+1);
		FileEntry tmp = g_App->playlist[i];
		g_App->playlist[i] = g_App->playlist[j];
		g_App->playlist[j] = tmp;
	}
	MarkPlaylistModified();
	if (hadCurrent)
	{
		for (i = 0; i < g_App->playlistCount; i++)
		{
			if (_tcsicmp(g_App->playlist[i].path, currentPath) == 0)
			{
				g_App->playlistIndex = i;
				break;
			}
		}
	}
	RefreshPlayerUI(hWnd);
}

static BOOL ContainsSubstringI(const TCHAR *haystack, const TCHAR *needle)
{
	size_t needleLen = _tcslen(needle);
	const TCHAR *p;
	if (needleLen == 0)
	{
		return TRUE;
	}
	for (p = haystack; *p; p++)
	{
		size_t i;
		for (i = 0; i < needleLen; i++)
		{
			if (!p[i] || _totupper(p[i]) != _totupper(needle[i]))
			{
				break;
			}
		}
		if (i == needleLen)
		{
			return TRUE;
		}
	}
	return FALSE;
}

void PopulatePlaylistListBox(HWND hWnd)
{
	HWND hList = GetDlgItem(hWnd, IDC_PLMGRLIST);
	TCHAR filter[MAX_PATH];
	int i;
	int visible = 0;
	GetDlgItemText(hWnd, IDC_PLMGRSEARCH, filter, MAX_PATH);
	ListView_DeleteAllItems(hList);
	for (i = 0; i < g_App->playlistCount; i++)
	{
		if (!ContainsSubstringI(g_App->playlist[i].path, filter))
		{
			continue;
		}
		if (i == g_App->playlistIndex)
		{
			TCHAR buf[MAX_PATH+4];
			SafeFormat(buf, MAX_PATH+4, _T("> %s"), g_App->playlist[i].path);
			InsertListViewItem(hList, visible, buf);
		}
		else
		{
			InsertListViewItem(hList, visible, g_App->playlist[i].path);
		}
		g_App->plMgrFilterMap[visible] = i;
		visible++;
	}
	g_App->plMgrFilterCount = visible;
}

static void ReplayAtIndexOrRefreshUI(HWND hWnd, int index)
{
	if (index >= 0 && index < g_App->playlistCount && !g_App->bMidiInEnabled)
	{
		g_App->playlistIndex = index;
		LoadAndPlayFile(hWnd, g_App->playlist[index].path);
	}
	else
	{
		RefreshPlayerUI(hWnd);
	}
}

void RemovePlaylistItemAt(HWND hWnd, int idx)
{
	BOOL wasCurrent;
	if (idx < 0 || idx >= g_App->playlistCount)
	{
		return;
	}
	wasCurrent = (idx == g_App->playlistIndex);
	if (wasCurrent)
	{
		UnloadLoadedFile(hWnd);
		g_App->playlistIndex = -1;
	}
	else if (idx < g_App->playlistIndex)
	{
		g_App->playlistIndex--;
	}
	RemovePlaylistArrayEntry(idx);
	if (wasCurrent)
	{
		ReplayAtIndexOrRefreshUI(hWnd, idx);
	}
	else
	{
		ReplayAtIndexOrRefreshUI(hWnd, -1);
	}
}

static void SwapPlaylistEntries(int indexA, int indexB)
{
	FileEntry tmp = g_App->playlist[indexA];
	g_App->playlist[indexA] = g_App->playlist[indexB];
	g_App->playlist[indexB] = tmp;
	MarkPlaylistModified();
	if (g_App->playlistIndex == indexA)
	{
		g_App->playlistIndex = indexB;
	}
	else if (g_App->playlistIndex == indexB)
	{
		g_App->playlistIndex = indexA;
	}
}

static void MovePlaylistSelected(HWND hWnd, BOOL up)
{
	HWND hList = GetDlgItem(hWnd, IDC_PLMGRLIST);
	int count;
	int *items = MoveSelectedListViewItems(hList, g_App->playlistCount, up, &count, SwapPlaylistEntries);
	if (!items)
	{
		return;
	}
	PopulatePlaylistListBox(hWnd);
	ReselectMovedListViewItems(hList, items, count, g_App->playlistCount);
	RefreshPlayerUI(g_App->hPlayerWnd);
}

static void RemovePlaylistSelected(HWND hWnd)
{
	HWND hList = GetDlgItem(hWnd, IDC_PLMGRLIST);
	int count;
	int *items = GetSortedSelectedListViewItems(hList, &count, TRUE);
	int i;
	int origIndex = g_App->playlistIndex;
	BOOL removedCurrent = FALSE;
	int shiftCount = 0;
	int anchorIndex;
	if (!items)
	{
		return;
	}
	anchorIndex = items[count-1];
	for (i = 0; i < count; i++)
	{
		int visIdx = items[i];
		int idx;
		if (visIdx < 0 || visIdx >= g_App->plMgrFilterCount)
		{
			continue;
		}
		idx = g_App->plMgrFilterMap[visIdx];
		if (idx == origIndex)
		{
			removedCurrent = TRUE;
		}
		else if (idx < origIndex)
		{
			shiftCount++;
		}
		RemovePlaylistArrayEntry(idx);
	}
	free(items);
	PopulatePlaylistListBox(hWnd);
	SelectClampedListViewIndex(hList, anchorIndex, g_App->plMgrFilterCount);
	if (removedCurrent)
	{
		int newIndex = origIndex - shiftCount;
		UnloadLoadedFile(g_App->hPlayerWnd);
		g_App->playlistIndex = -1;
		ReplayAtIndexOrRefreshUI(g_App->hPlayerWnd, newIndex);
	}
	else
	{
		if (origIndex >= 0)
		{
			g_App->playlistIndex = origIndex - shiftCount;
		}
		RefreshPlayerUI(g_App->hPlayerWnd);
	}
}

static void PlaySelectedPlaylistMgrEntry(HWND hWnd)
{
	HWND hList = GetDlgItem(hWnd, IDC_PLMGRLIST);
	int visIdx = GetListViewCurSel(hList);
	if (visIdx < 0 || visIdx >= g_App->plMgrFilterCount)
	{
		return;
	}
	PlayPlaylistEntryAt(g_App->hPlayerWnd, g_App->plMgrFilterMap[visIdx]);
}

static void UpdatePlaylistMgrMoveButtons(HWND hWnd)
{
	TCHAR filter[MAX_PATH];
	BOOL enable;
	GetDlgItemText(hWnd, IDC_PLMGRSEARCH, filter, MAX_PATH);
	enable = (filter[0] == _T('\0'));
	EnableWindow(GetDlgItem(hWnd, IDC_PLMGRUP), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_PLMGRDOWN), enable);
}

BOOL WINAPI PlaylistMgrDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	switch (message)
	{
	case WM_INITDIALOG:
		g_App->hPlaylistMgrWnd = hWnd;
		g_App->bPlaylistMgrEdited = FALSE;
		InitSingleColumnListView(GetDlgItem(hWnd, IDC_PLMGRLIST));
		PopulatePlaylistListBox(hWnd);
		if (g_App->plMgrFilterCount > 0)
		{
			HWND hList = GetDlgItem(hWnd, IDC_PLMGRLIST);
			ListView_SetItemState(hList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		}
		UpdatePlaylistMgrMoveButtons(hWnd);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_PLMGRSEARCH:
			if (HIWORD(wParam) == EN_CHANGE)
			{
				HWND hList = GetDlgItem(hWnd, IDC_PLMGRLIST);
				PopulatePlaylistListBox(hWnd);
				if (g_App->plMgrFilterCount > 0)
				{
					SetListViewCurSel(hList, 0);
				}
				else
				{
					SetListViewCurSel(hList, -1);
				}
				UpdatePlaylistMgrMoveButtons(hWnd);
			}
			return TRUE;
		case IDC_PLMGRPLAY:
			PlaySelectedPlaylistMgrEntry(hWnd);
			return TRUE;
		case IDC_PLMGRUP:
			MovePlaylistSelected(hWnd, TRUE);
			g_App->bPlaylistMgrEdited = TRUE;
			return TRUE;
		case IDC_PLMGRDOWN:
			MovePlaylistSelected(hWnd, FALSE);
			g_App->bPlaylistMgrEdited = TRUE;
			return TRUE;
		case IDC_PLMGRDELETE:
			RemovePlaylistSelected(hWnd);
			g_App->bPlaylistMgrEdited = TRUE;
			return TRUE;
		case IDOK:
		case IDCANCEL:
			EndDialog(hWnd, TRUE);
			return TRUE;
		}
		break;
	case WM_DESTROY:
		if (g_App->bPlaylistMgrEdited && g_App->hPlaylistView && g_App->playlistIndex >= 0 && g_App->playlistIndex < g_App->playlistCount)
		{
			SetListViewCurSel(g_App->hPlaylistView, g_App->playlistIndex);
		}
		g_App->hPlaylistMgrWnd = NULL;
		return TRUE;
	}
	return FALSE;
}
