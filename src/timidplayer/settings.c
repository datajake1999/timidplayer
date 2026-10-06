#include "timidplayer.h"

#define PLAYERCFG_REG_KEY _T("Software\\Datajake\\TimidityPlayer")
#define PLAYERCFG_REG_DEFAULTOUTDIR _T("DefaultOutDir")
#define PLAYERCFG_REG_DEFAULTOUTFORMAT _T("DefaultOutFormat")
#define PLAYERCFG_REG_DEVICE _T("OutputDevice")
#define PLAYERCFG_REG_BUFFERMS _T("BufferMS")
#define PLAYERCFG_REG_CHUNKMS _T("ChunkMS")
#define PLAYERCFG_REG_VOLUME _T("Volume")
#define PLAYERCFG_REG_INTERCEPTVOLUMEKEYS _T("InterceptVolumeKeys")
#define PLAYERCFG_REG_LOADPLAYLIST _T("LoadPreviousPlaylist")
#define PLAYERCFG_REG_LOADINTORUNNING _T("LoadFilesIntoRunningInstance")
#define PLAYERCFG_REG_SHOWFILENAMEINTITLE _T("ShowFilenameInTitle")
#define PLAYERCFG_REG_PREVENTDUPLICATES _T("PreventDuplicatePlaylistItems")
#define PLAYERCFG_REG_SAVETRACK _T("SaveTrack")
#define PLAYERCFG_REG_SAVEPOSITION _T("SavePosition")
#define PLAYERCFG_REG_AUTOSTART _T("AutoStartPlayback")
#define PLAYERCFG_REG_REMOVEONUNLOAD _T("RemoveOnUnload")
#define PLAYERCFG_REG_CONFIRMDELETE _T("ConfirmPlaylistDelete")
#define PLAYERCFG_REG_PREVENTSEEKTRACKCHANGE _T("PreventSeekTrackChange")
#define PLAYERCFG_REG_AUTOSCROLLPLAYLIST _T("AutoscrollPlaylist")
#define PLAYERCFG_REG_RUNFROMTRAY _T("RunFromTray")
#define PLAYERCFG_REG_SKIPALREADYRUNNINGPROMPT _T("SkipAlreadyRunningPrompt")
#define PLAYERCFG_REG_ALWAYSONTOP _T("AlwaysOnTop")
#define PLAYERCFG_REG_SHOWSTATUSBAR _T("ShowStatusBar")
#define PLAYERCFG_REG_SHOWSEEKBAR _T("ShowSeekBar")
#define PLAYERCFG_REG_SHOWPLAYLISTVIEW _T("ShowPlaylistView")
#define PLAYERCFG_REG_RECENTFILES _T("RecentFiles")
#define PLAYERCFG_REG_SESSION_FILES _T("SessionFiles")
#define PLAYERCFG_REG_SESSION_INDEX _T("SessionIndex")
#define PLAYERCFG_REG_SESSION_POSITION _T("SessionPositionMs")
#define PLAYERCFG_REG_SESSION_REPEAT _T("SessionRepeatMode")
#define PLAYERCFG_REG_MIDIIN_ENABLED _T("MidiInEnabled")
#define PLAYERCFG_REG_MIDIIN_DEVICE _T("MidiInDevice")
#define PLAYERCFG_REG_MIDIIN_CHANNELMAP _T("MidiInChannelMap")

static int ClampOrDefault(int value, int minValue, int maxValue, int defaultValue)
{
	if (value < minValue || value > maxValue)
	{
		return defaultValue;
	}
	return value;
}

static BOOL OpenAppRegKeyRead(HKEY *phKey)
{
	return RegOpenKeyEx(HKEY_CURRENT_USER, PLAYERCFG_REG_KEY, 0, KEY_READ, phKey) == ERROR_SUCCESS;
}

static BOOL OpenAppRegKeyWrite(HKEY *phKey)
{
	DWORD dwDispos;
	if (g_App && g_App->bSettingsRestored)
	{
		return FALSE;
	}
	return RegCreateKeyEx(HKEY_CURRENT_USER, PLAYERCFG_REG_KEY, 0, NULL, 0, KEY_WRITE, NULL, phKey, &dwDispos) == ERROR_SUCCESS;
}

static DWORD RegReadDword(HKEY hKey, const TCHAR *valueName, DWORD defaultValue)
{
	DWORD dwType, dwSize, val;
	dwSize = sizeof(DWORD);
	val = 0;
	if (RegQueryValueEx(hKey, valueName, 0, &dwType, (LPBYTE)&val, &dwSize) == ERROR_SUCCESS && dwType == REG_DWORD)
	{
		return val;
	}
	return defaultValue;
}

static void RegWriteDword(HKEY hKey, const TCHAR *valueName, DWORD value)
{
	RegSetValueEx(hKey, valueName, 0, REG_DWORD, (LPBYTE)&value, sizeof(DWORD));
}

static BOOL RegReadBool(HKEY hKey, const TCHAR *valueName, BOOL defaultValue)
{
	return RegReadDword(hKey, valueName, (DWORD)defaultValue) != 0;
}

static void RegWriteBool(HKEY hKey, const TCHAR *valueName, BOOL value)
{
	DWORD val;
	if (value)
	{
		val = 1;
	}
	else
	{
		val = 0;
	}
	RegWriteDword(hKey, valueName, val);
}

static BOOL ReadRegBoolSetting(const TCHAR *valueName, BOOL defaultValue)
{
	HKEY hKey;
	BOOL value;
	if (!OpenAppRegKeyRead(&hKey))
	{
		return defaultValue;
	}
	value = RegReadBool(hKey, valueName, defaultValue);
	RegCloseKey(hKey);
	return value;
}

static void WriteRegBoolSetting(const TCHAR *valueName, BOOL value)
{
	HKEY hKey;
	if (!OpenAppRegKeyWrite(&hKey))
	{
		return;
	}
	RegWriteBool(hKey, valueName, value);
	RegCloseKey(hKey);
}

BOOL AlreadyRunningPromptSuppressed(void)
{
	return ReadRegBoolSetting(PLAYERCFG_REG_SKIPALREADYRUNNINGPROMPT, FALSE);
}

void SuppressAlreadyRunningPrompt(void)
{
	WriteRegBoolSetting(PLAYERCFG_REG_SKIPALREADYRUNNINGPROMPT, TRUE);
}

BOOL RunFromTrayEnabled(void)
{
	return ReadRegBoolSetting(PLAYERCFG_REG_RUNFROMTRAY, FALSE);
}

BOOL LoadFilesIntoRunningInstanceEnabled(void)
{
	return ReadRegBoolSetting(PLAYERCFG_REG_LOADINTORUNNING, TRUE);
}

void SaveLoadFilesIntoRunningInstanceSetting(BOOL enabled)
{
	WriteRegBoolSetting(PLAYERCFG_REG_LOADINTORUNNING, enabled);
}

void LoadAudioOutputSettings(void)
{
	HKEY hKey;
	g_App->outputDeviceId = WAVE_MAPPER;
	g_App->bufferMs = PLAYER_DEFAULT_BUFFER_MS;
	g_App->chunkMs = PLAYER_DEFAULT_CHUNK_MS;
	g_App->volume = 100;
	g_App->bInterceptVolumeKeys = FALSE;
	if (!OpenAppRegKeyRead(&hKey))
	{
		return;
	}
	g_App->outputDeviceId = (int)RegReadDword(hKey, PLAYERCFG_REG_DEVICE, (DWORD)g_App->outputDeviceId);
	g_App->bufferMs = (int)RegReadDword(hKey, PLAYERCFG_REG_BUFFERMS, (DWORD)g_App->bufferMs);
	g_App->chunkMs = (int)RegReadDword(hKey, PLAYERCFG_REG_CHUNKMS, (DWORD)g_App->chunkMs);
	g_App->volume = (int)RegReadDword(hKey, PLAYERCFG_REG_VOLUME, (DWORD)g_App->volume);
	g_App->bInterceptVolumeKeys = RegReadBool(hKey, PLAYERCFG_REG_INTERCEPTVOLUMEKEYS, g_App->bInterceptVolumeKeys);
	RegCloseKey(hKey);
	if (!IsValidWaveOutDeviceId(g_App->outputDeviceId))
	{
		g_App->outputDeviceId = WAVE_MAPPER;
	}
	g_App->bufferMs = ClampOrDefault(g_App->bufferMs, PLAYER_MIN_BUFFER_MS, PLAYER_MAX_BUFFER_MS, PLAYER_DEFAULT_BUFFER_MS);
	g_App->chunkMs = ClampOrDefault(g_App->chunkMs, PLAYER_MIN_CHUNK_MS, PLAYER_MAX_CHUNK_MS, PLAYER_DEFAULT_CHUNK_MS);
	if (g_App->chunkMs > g_App->bufferMs)
	{
		g_App->chunkMs = g_App->bufferMs;
	}
	g_App->volume = ClampOrDefault(g_App->volume, 0, 100, 100);
}

void SaveAudioOutputSettings(void)
{
	HKEY hKey;
	if (!OpenAppRegKeyWrite(&hKey))
	{
		return;
	}
	RegWriteDword(hKey, PLAYERCFG_REG_DEVICE, (DWORD)g_App->outputDeviceId);
	RegWriteDword(hKey, PLAYERCFG_REG_BUFFERMS, (DWORD)g_App->bufferMs);
	RegWriteDword(hKey, PLAYERCFG_REG_CHUNKMS, (DWORD)g_App->chunkMs);
	RegWriteDword(hKey, PLAYERCFG_REG_VOLUME, (DWORD)g_App->volume);
	RegWriteBool(hKey, PLAYERCFG_REG_INTERCEPTVOLUMEKEYS, g_App->bInterceptVolumeKeys);
	RegCloseKey(hKey);
}

static void LoadMidiInChannelMap(HKEY hKey)
{
	BYTE buf[16];
	DWORD dwType, dwSize;
	int i;
	for (i = 0; i < 16; i++)
	{
		g_App->midiInChannelMap[i] = i;
	}
	dwSize = sizeof(buf);
	if (RegQueryValueEx(hKey, PLAYERCFG_REG_MIDIIN_CHANNELMAP, 0, &dwType, buf, &dwSize) == ERROR_SUCCESS && dwType == REG_BINARY && dwSize == sizeof(buf))
	{
		for (i = 0; i < 16; i++)
		{
			if (buf[i] < 16)
			{
				g_App->midiInChannelMap[i] = buf[i];
			}
		}
	}
}

static void SaveMidiInChannelMap(HKEY hKey)
{
	BYTE buf[16];
	int i;
	for (i = 0; i < 16; i++)
	{
		buf[i] = (BYTE)g_App->midiInChannelMap[i];
	}
	RegSetValueEx(hKey, PLAYERCFG_REG_MIDIIN_CHANNELMAP, 0, REG_BINARY, buf, sizeof(buf));
}

void LoadMidiInputSettings(void)
{
	HKEY hKey;
	int i;
	g_App->bMidiInEnabled = FALSE;
	g_App->midiInDeviceId = MIDI_MAPPER;
	for (i = 0; i < 16; i++)
	{
		g_App->midiInChannelMap[i] = i;
	}
	if (!OpenAppRegKeyRead(&hKey))
	{
		return;
	}
	g_App->bMidiInEnabled = RegReadBool(hKey, PLAYERCFG_REG_MIDIIN_ENABLED, g_App->bMidiInEnabled);
	g_App->midiInDeviceId = (int)RegReadDword(hKey, PLAYERCFG_REG_MIDIIN_DEVICE, (DWORD)g_App->midiInDeviceId);
	LoadMidiInChannelMap(hKey);
	RegCloseKey(hKey);
	if (!IsValidMidiInDeviceId(g_App->midiInDeviceId))
	{
		g_App->midiInDeviceId = MIDI_MAPPER;
	}
}

void SaveMidiInputSettings(void)
{
	HKEY hKey;
	if (!OpenAppRegKeyWrite(&hKey))
	{
		return;
	}
	RegWriteBool(hKey, PLAYERCFG_REG_MIDIIN_ENABLED, g_App->bMidiInEnabled);
	RegWriteDword(hKey, PLAYERCFG_REG_MIDIIN_DEVICE, (DWORD)g_App->midiInDeviceId);
	SaveMidiInChannelMap(hKey);
	RegCloseKey(hKey);
}

void LoadPlayerOptions(void)
{
	HKEY hKey;
	g_App->bLoadPreviousPlaylist = TRUE;
	g_App->bLoadFilesIntoRunningInstance = TRUE;
	g_App->bShowFilenameInTitle = TRUE;
	g_App->bPreventDuplicatePlaylistItems = FALSE;
	g_App->bSaveTrack = TRUE;
	g_App->bSavePosition = TRUE;
	g_App->bAutoStartPlayback = TRUE;
	g_App->bRemoveOnUnload = FALSE;
	g_App->bConfirmPlaylistDelete = TRUE;
	g_App->bPreventSeekTrackChange = TRUE;
	g_App->bAutoscrollPlaylist = TRUE;
	g_App->bRunFromTray = FALSE;
	if (!OpenAppRegKeyRead(&hKey))
	{
		return;
	}
	g_App->bLoadPreviousPlaylist = RegReadBool(hKey, PLAYERCFG_REG_LOADPLAYLIST, g_App->bLoadPreviousPlaylist);
	g_App->bLoadFilesIntoRunningInstance = RegReadBool(hKey, PLAYERCFG_REG_LOADINTORUNNING, g_App->bLoadFilesIntoRunningInstance);
	g_App->bShowFilenameInTitle = RegReadBool(hKey, PLAYERCFG_REG_SHOWFILENAMEINTITLE, g_App->bShowFilenameInTitle);
	g_App->bPreventDuplicatePlaylistItems = RegReadBool(hKey, PLAYERCFG_REG_PREVENTDUPLICATES, g_App->bPreventDuplicatePlaylistItems);
	g_App->bSaveTrack = RegReadBool(hKey, PLAYERCFG_REG_SAVETRACK, g_App->bSaveTrack);
	g_App->bSavePosition = RegReadBool(hKey, PLAYERCFG_REG_SAVEPOSITION, g_App->bSavePosition);
	g_App->bAutoStartPlayback = RegReadBool(hKey, PLAYERCFG_REG_AUTOSTART, g_App->bAutoStartPlayback);
	g_App->bRemoveOnUnload = RegReadBool(hKey, PLAYERCFG_REG_REMOVEONUNLOAD, g_App->bRemoveOnUnload);
	g_App->bConfirmPlaylistDelete = RegReadBool(hKey, PLAYERCFG_REG_CONFIRMDELETE, g_App->bConfirmPlaylistDelete);
	g_App->bPreventSeekTrackChange = RegReadBool(hKey, PLAYERCFG_REG_PREVENTSEEKTRACKCHANGE, g_App->bPreventSeekTrackChange);
	g_App->bAutoscrollPlaylist = RegReadBool(hKey, PLAYERCFG_REG_AUTOSCROLLPLAYLIST, g_App->bAutoscrollPlaylist);
	g_App->bRunFromTray = RegReadBool(hKey, PLAYERCFG_REG_RUNFROMTRAY, g_App->bRunFromTray);
	RegCloseKey(hKey);
}

void SavePlayerOptions(void)
{
	HKEY hKey;
	if (!OpenAppRegKeyWrite(&hKey))
	{
		return;
	}
	RegWriteBool(hKey, PLAYERCFG_REG_LOADPLAYLIST, g_App->bLoadPreviousPlaylist);
	RegWriteBool(hKey, PLAYERCFG_REG_LOADINTORUNNING, g_App->bLoadFilesIntoRunningInstance);
	RegWriteBool(hKey, PLAYERCFG_REG_SHOWFILENAMEINTITLE, g_App->bShowFilenameInTitle);
	RegWriteBool(hKey, PLAYERCFG_REG_PREVENTDUPLICATES, g_App->bPreventDuplicatePlaylistItems);
	RegWriteBool(hKey, PLAYERCFG_REG_SAVETRACK, g_App->bSaveTrack);
	RegWriteBool(hKey, PLAYERCFG_REG_SAVEPOSITION, g_App->bSavePosition);
	RegWriteBool(hKey, PLAYERCFG_REG_AUTOSTART, g_App->bAutoStartPlayback);
	RegWriteBool(hKey, PLAYERCFG_REG_REMOVEONUNLOAD, g_App->bRemoveOnUnload);
	RegWriteBool(hKey, PLAYERCFG_REG_CONFIRMDELETE, g_App->bConfirmPlaylistDelete);
	RegWriteBool(hKey, PLAYERCFG_REG_PREVENTSEEKTRACKCHANGE, g_App->bPreventSeekTrackChange);
	RegWriteBool(hKey, PLAYERCFG_REG_AUTOSCROLLPLAYLIST, g_App->bAutoscrollPlaylist);
	RegWriteBool(hKey, PLAYERCFG_REG_RUNFROMTRAY, g_App->bRunFromTray);
	RegCloseKey(hKey);
}

void RefreshLoadFilesIntoRunningInstanceSetting(void)
{
	HKEY hKey;
	if (!OpenAppRegKeyRead(&hKey))
	{
		return;
	}
	g_App->bLoadFilesIntoRunningInstance = RegReadBool(hKey, PLAYERCFG_REG_LOADINTORUNNING, g_App->bLoadFilesIntoRunningInstance);
	RegCloseKey(hKey);
}

void LoadAlwaysOnTopSetting(void)
{
	g_App->bAlwaysOnTop = ReadRegBoolSetting(PLAYERCFG_REG_ALWAYSONTOP, FALSE);
}

void SaveAlwaysOnTopSetting(void)
{
	WriteRegBoolSetting(PLAYERCFG_REG_ALWAYSONTOP, g_App->bAlwaysOnTop);
}

void LoadViewSettings(void)
{
	HKEY hKey;
	g_App->bShowStatusBar = TRUE;
	g_App->bShowSeekBar = TRUE;
	g_App->bShowPlaylistView = TRUE;
	if (!OpenAppRegKeyRead(&hKey))
	{
		return;
	}
	g_App->bShowStatusBar = RegReadBool(hKey, PLAYERCFG_REG_SHOWSTATUSBAR, g_App->bShowStatusBar);
	g_App->bShowSeekBar = RegReadBool(hKey, PLAYERCFG_REG_SHOWSEEKBAR, g_App->bShowSeekBar);
	g_App->bShowPlaylistView = RegReadBool(hKey, PLAYERCFG_REG_SHOWPLAYLISTVIEW, g_App->bShowPlaylistView);
	RegCloseKey(hKey);
}

void SaveViewSettings(void)
{
	HKEY hKey;
	if (!OpenAppRegKeyWrite(&hKey))
	{
		return;
	}
	RegWriteBool(hKey, PLAYERCFG_REG_SHOWSTATUSBAR, g_App->bShowStatusBar);
	RegWriteBool(hKey, PLAYERCFG_REG_SHOWSEEKBAR, g_App->bShowSeekBar);
	RegWriteBool(hKey, PLAYERCFG_REG_SHOWPLAYLISTVIEW, g_App->bShowPlaylistView);
	RegCloseKey(hKey);
}

void LoadDefaultOutputDir(TCHAR *buf, int bufSize)
{
	HKEY hKey;
	DWORD dwType, dwSize;
	buf[0] = _T('\0');
	if (!OpenAppRegKeyRead(&hKey))
	{
		return;
	}
	dwSize = (DWORD)(bufSize * sizeof(TCHAR));
	if (RegQueryValueEx(hKey, PLAYERCFG_REG_DEFAULTOUTDIR, 0, &dwType, (LPBYTE)buf, &dwSize) == ERROR_SUCCESS && dwType == REG_SZ)
	{
		int termIndex = (int)(dwSize / sizeof(TCHAR));
		if (termIndex >= bufSize)
		{
			termIndex = bufSize - 1;
		}
		buf[termIndex] = _T('\0');
	}
	else
	{
		buf[0] = _T('\0');
	}
	RegCloseKey(hKey);
}

void SaveDefaultOutputDir(const TCHAR *dir)
{
	HKEY hKey;
	if (!OpenAppRegKeyWrite(&hKey))
	{
		return;
	}
	RegSetValueEx(hKey, PLAYERCFG_REG_DEFAULTOUTDIR, 0, REG_SZ, (const BYTE *)dir, (DWORD)((_tcslen(dir) + 1) * sizeof(TCHAR)));
	RegCloseKey(hKey);
}

int LoadDefaultOutputFormat(int defaultIndex)
{
	HKEY hKey;
	int result = defaultIndex;
	if (!OpenAppRegKeyRead(&hKey))
	{
		return result;
	}
	result = (int)RegReadDword(hKey, PLAYERCFG_REG_DEFAULTOUTFORMAT, (DWORD)defaultIndex);
	RegCloseKey(hKey);
	return result;
}

void SaveDefaultOutputFormat(int formatIndex)
{
	HKEY hKey;
	if (!OpenAppRegKeyWrite(&hKey))
	{
		return;
	}
	RegWriteDword(hKey, PLAYERCFG_REG_DEFAULTOUTFORMAT, (DWORD)formatIndex);
	RegCloseKey(hKey);
}

static size_t CopyBoundedRegString(TCHAR *dst, const TCHAR *src, const TCHAR *end)
{
	size_t realLen = 0;
	size_t copyLen;
	while (src + realLen < end && src[realLen])
	{
		realLen++;
	}
	copyLen = realLen;
	if (copyLen > MAX_PATH-1)
	{
		copyLen = MAX_PATH-1;
	}
	_tcsncpy(dst, src, copyLen);
	dst[copyLen] = _T('\0');
	return realLen;
}

static int LoadMultiStringList(HKEY hKey, const TCHAR *valueName, TCHAR *items, size_t itemStride, int maxItems)
{
	DWORD dwType, dwSize;
	TCHAR *buffer;
	TCHAR *p, *end;
	int count = 0;
	dwSize = 0;
	if (RegQueryValueEx(hKey, valueName, 0, &dwType, NULL, &dwSize) != ERROR_SUCCESS || dwType != REG_MULTI_SZ || dwSize == 0)
	{
		return 0;
	}
	buffer = (TCHAR *)malloc(dwSize);
	if (!buffer)
	{
		return 0;
	}
	if (RegQueryValueEx(hKey, valueName, 0, &dwType, (LPBYTE)buffer, &dwSize) == ERROR_SUCCESS)
	{
		p = buffer;
		end = buffer + (dwSize / sizeof(TCHAR));
		while (p < end && *p && count < maxItems)
		{
			TCHAR *dst = (TCHAR *)((BYTE *)items + (size_t)count * itemStride);
			p += CopyBoundedRegString(dst, p, end) + 1;
			count++;
		}
	}
	free(buffer);
	return count;
}

static void SaveMultiStringList(HKEY hKey, const TCHAR *valueName, const TCHAR *items, size_t itemStride, int itemCount, size_t maxItems)
{
	TCHAR *buffer;
	size_t bufChars;
	int used, i;
	bufChars = maxItems * MAX_PATH + 1;
	buffer = (TCHAR *)malloc(sizeof(TCHAR) * bufChars);
	if (!buffer)
	{
		return;
	}
	used = 0;
	for (i = 0; i < itemCount; i++)
	{
		const TCHAR *item = (const TCHAR *)((const BYTE *)items + (size_t)i * itemStride);
		if (!AppendBoundedToken(buffer, (int)bufChars, &used, item))
		{
			break;
		}
	}
	buffer[used] = _T('\0');
	used++;
	RegSetValueEx(hKey, valueName, 0, REG_MULTI_SZ, (LPBYTE)buffer, (DWORD)((size_t)used * sizeof(TCHAR)));
	free(buffer);
}

void LoadRecentFiles(void)
{
	HKEY hKey;
	g_App->recentFileCount = 0;
	if (!OpenAppRegKeyRead(&hKey))
	{
		return;
	}
	g_App->recentFileCount = LoadMultiStringList(hKey, PLAYERCFG_REG_RECENTFILES, g_App->recentFiles[0], sizeof(g_App->recentFiles[0]), MAX_RECENT_FILES);
	RegCloseKey(hKey);
}

void SaveRecentFiles(void)
{
	HKEY hKey;
	if (!OpenAppRegKeyWrite(&hKey))
	{
		return;
	}
	SaveMultiStringList(hKey, PLAYERCFG_REG_RECENTFILES, g_App->recentFiles[0], sizeof(g_App->recentFiles[0]), g_App->recentFileCount, MAX_RECENT_FILES);
	RegCloseKey(hKey);
}

void ClearRecentFiles(void)
{
	HKEY hKey;
	g_App->recentFileCount = 0;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, PLAYERCFG_REG_KEY, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
	{
		return;
	}
	RegDeleteValue(hKey, PLAYERCFG_REG_RECENTFILES);
	RegCloseKey(hKey);
}

BOOL LoadSession(HWND hWnd)
{
	HKEY hKey;
	int savedIndex = 0, savedPositionMs = 0, savedRepeat = REPEAT_OFF;
	g_App->playlistCount = 0;
	g_App->playlistIndex = -1;
	if (!OpenAppRegKeyRead(&hKey))
	{
		return FALSE;
	}
	g_App->playlistCount = LoadMultiStringList(hKey, PLAYERCFG_REG_SESSION_FILES, g_App->playlist[0].path, sizeof(g_App->playlist[0]), MAX_PLAYLIST);
	savedIndex = (int)RegReadDword(hKey, PLAYERCFG_REG_SESSION_INDEX, (DWORD)savedIndex);
	savedPositionMs = (int)RegReadDword(hKey, PLAYERCFG_REG_SESSION_POSITION, (DWORD)savedPositionMs);
	savedRepeat = (int)RegReadDword(hKey, PLAYERCFG_REG_SESSION_REPEAT, (DWORD)savedRepeat);
	RegCloseKey(hKey);
	if (savedRepeat == REPEAT_OFF || savedRepeat == REPEAT_ONE || savedRepeat == REPEAT_ALL)
	{
		g_App->repeatMode = savedRepeat;
	}
	UpdateMenuState(hWnd);
	if (g_App->playlistCount == 0)
	{
		return FALSE;
	}
	if (g_App->bMidiInEnabled)
	{
		return FALSE;
	}
	if (savedIndex < 0 || savedIndex >= g_App->playlistCount)
	{
		savedIndex = 0;
	}
	g_App->playlistIndex = savedIndex;
	if (savedPositionMs > 0)
	{
		g_App->pendingSeekMs = savedPositionMs;
	}
	if (!LoadAndPlayFile(hWnd, g_App->playlist[g_App->playlistIndex].path))
	{
		return FALSE;
	}
	return TRUE;
}

static void ClearSavedSession(void)
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, PLAYERCFG_REG_KEY, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
	{
		return;
	}
	RegDeleteValue(hKey, PLAYERCFG_REG_SESSION_FILES);
	RegDeleteValue(hKey, PLAYERCFG_REG_SESSION_INDEX);
	RegDeleteValue(hKey, PLAYERCFG_REG_SESSION_POSITION);
	RegDeleteValue(hKey, PLAYERCFG_REG_SESSION_REPEAT);
	RegCloseKey(hKey);
}

void SaveSession(void)
{
	HKEY hKey;
	int curMs;
	if (!g_App->bLoadPreviousPlaylist)
	{
		ClearSavedSession();
		return;
	}
	if (!OpenAppRegKeyWrite(&hKey))
	{
		return;
	}
	SaveMultiStringList(hKey, PLAYERCFG_REG_SESSION_FILES, g_App->playlist[0].path, sizeof(g_App->playlist[0]), g_App->playlistCount, MAX_PLAYLIST);
	if (g_App->bSaveTrack)
	{
		RegWriteDword(hKey, PLAYERCFG_REG_SESSION_INDEX, (DWORD)g_App->playlistIndex);
	}
	else
	{
		RegWriteDword(hKey, PLAYERCFG_REG_SESSION_INDEX, 0);
	}
	curMs = 0;
	if (g_App->bSaveTrack && g_App->bSavePosition && g_App->bFileLoaded && g_App->synth)
	{
		EnterCriticalSection(&g_App->synthCS);
		curMs = timid_get_current_time(g_App->synth);
		LeaveCriticalSection(&g_App->synthCS);
	}
	RegWriteDword(hKey, PLAYERCFG_REG_SESSION_POSITION, (DWORD)curMs);
	RegWriteDword(hKey, PLAYERCFG_REG_SESSION_REPEAT, (DWORD)g_App->repeatMode);
	RegCloseKey(hKey);
}

#define SETTINGS_FILE_MAGIC "TPCFG003"
#define SETTINGS_FILE_MAGIC_SIZE 8
#define SETTINGS_FILE_MAX_VALUES 4096
#define SETTINGS_FILE_MAX_DATA_SIZE (4 * 1024 * 1024)
#define SETTINGS_FILE_NAME_RAW_BYTES 512

#if defined(UNICODE) || defined(_UNICODE)
#define SETTINGS_FILE_THIS_BUILD_IS_UNICODE 1
#else
#define SETTINGS_FILE_THIS_BUILD_IS_UNICODE 0
#endif

typedef struct {
	DWORD size;
	TCHAR name[256];
	DWORD type;
	DWORD dataSize;
	BYTE *data;
} SettingsFileRecord;

static BOOL ConvertUnicodeBufferToAnsi(const BYTE *src, DWORD srcBytes, BYTE **outData, DWORD *outBytes)
{
	int wcharCount, needed;
	BYTE *buf;
	wcharCount = (int)(srcBytes / sizeof(WCHAR));
	needed = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)src, wcharCount, NULL, 0, NULL, NULL);
	if (needed <= 0)
	{
		return FALSE;
	}
	buf = (BYTE *)malloc((size_t)needed);
	if (!buf)
	{
		return FALSE;
	}
	if (WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)src, wcharCount, (LPSTR)buf, needed, NULL, NULL) <= 0)
	{
		free(buf);
		return FALSE;
	}
	*outData = buf;
	*outBytes = (DWORD)needed;
	return TRUE;
}

static BOOL ConvertAnsiBufferToUnicode(const BYTE *src, DWORD srcBytes, BYTE **outData, DWORD *outBytes)
{
	int needed;
	BYTE *buf;
	needed = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)src, (int)srcBytes, NULL, 0);
	if (needed <= 0)
	{
		return FALSE;
	}
	buf = (BYTE *)malloc((size_t)needed * sizeof(WCHAR));
	if (!buf)
	{
		return FALSE;
	}
	if (MultiByteToWideChar(CP_ACP, 0, (LPCSTR)src, (int)srcBytes, (LPWSTR)buf, needed) <= 0)
	{
		free(buf);
		return FALSE;
	}
	*outData = buf;
	*outBytes = (DWORD)needed * sizeof(WCHAR);
	return TRUE;
}

static BOOL ConvertRegString(DWORD fileIsUnicode, const BYTE *src, DWORD srcBytes, BYTE **outData, DWORD *outBytes)
{
	if (fileIsUnicode)
	{
		return ConvertUnicodeBufferToAnsi(src, srcBytes, outData, outBytes);
	}
	return ConvertAnsiBufferToUnicode(src, srcBytes, outData, outBytes);
}

static DWORD ComputeSettingsRecordSize(DWORD nameBytes, DWORD dataSize)
{
	return sizeof(DWORD) + nameBytes + sizeof(DWORD) + sizeof(DWORD) + dataSize;
}

BOOL ImportPlayerSettings(const TCHAR *path)
{
	FILE *fp;
	char magic[SETTINGS_FILE_MAGIC_SIZE];
	DWORD valueCount;
	DWORD i;
	SettingsFileRecord *records;
	BOOL ok = TRUE;
	HKEY hKey;
	DWORD fileIsUnicode;
	BOOL bNeedsStringConversion;
	fp = _tfopen(path, _T("rb"));
	if (!fp)
	{
		return FALSE;
	}
	if (fread(magic, 1, SETTINGS_FILE_MAGIC_SIZE, fp) != SETTINGS_FILE_MAGIC_SIZE || memcmp(magic, SETTINGS_FILE_MAGIC, SETTINGS_FILE_MAGIC_SIZE) != 0 || fread(&valueCount, sizeof(DWORD), 1, fp) != 1 || valueCount > SETTINGS_FILE_MAX_VALUES || fread(&fileIsUnicode, sizeof(DWORD), 1, fp) != 1)
	{
		fclose(fp);
		return FALSE;
	}
	bNeedsStringConversion = ((fileIsUnicode != 0) != (SETTINGS_FILE_THIS_BUILD_IS_UNICODE != 0));
	records = NULL;
	if (valueCount > 0)
	{
		records = (SettingsFileRecord *)calloc(valueCount, sizeof(SettingsFileRecord));
		if (!records)
		{
			fclose(fp);
			return FALSE;
		}
	}
	for (i = 0; i < valueCount; i++)
	{
		DWORD nameBytes;
		DWORD onDiskDataSize;
		DWORD expectedSize;
		BYTE rawName[SETTINGS_FILE_NAME_RAW_BYTES];
		BYTE *rawData;
		BOOL isStringType;
		if (fread(&records[i].size, sizeof(DWORD), 1, fp) != 1)
		{
			ok = FALSE;
			break;
		}
		if (fread(&nameBytes, sizeof(DWORD), 1, fp) != 1 || nameBytes == 0 || nameBytes > sizeof(rawName))
		{
			ok = FALSE;
			break;
		}
		if (fread(rawName, 1, nameBytes, fp) != nameBytes)
		{
			ok = FALSE;
			break;
		}
		if (fread(&records[i].type, sizeof(DWORD), 1, fp) != 1 || fread(&onDiskDataSize, sizeof(DWORD), 1, fp) != 1 || onDiskDataSize > SETTINGS_FILE_MAX_DATA_SIZE)
		{
			ok = FALSE;
			break;
		}
		rawData = NULL;
		if (onDiskDataSize > 0)
		{
			rawData = (BYTE *)malloc(onDiskDataSize);
			if (!rawData || fread(rawData, 1, onDiskDataSize, fp) != onDiskDataSize)
			{
				free(rawData);
				ok = FALSE;
				break;
			}
		}
		expectedSize = ComputeSettingsRecordSize(nameBytes, onDiskDataSize);
		if (records[i].size != expectedSize)
		{
			free(rawData);
			ok = FALSE;
			break;
		}
		isStringType = (records[i].type == REG_SZ || records[i].type == REG_EXPAND_SZ || records[i].type == REG_MULTI_SZ);
		if (!bNeedsStringConversion)
		{
			if (nameBytes > sizeof(records[i].name) || nameBytes % sizeof(TCHAR) != 0 || nameBytes < sizeof(TCHAR))
			{
				free(rawData);
				ok = FALSE;
				break;
			}
			memcpy(records[i].name, rawName, nameBytes);
			records[i].name[(nameBytes / sizeof(TCHAR)) - 1] = _T('\0');
			records[i].data = rawData;
			records[i].dataSize = onDiskDataSize;
			continue;
		}
		{
			BYTE *convertedName = NULL;
			DWORD convertedNameBytes = 0;
			ok = ConvertRegString(fileIsUnicode, rawName, nameBytes, &convertedName, &convertedNameBytes);
			if (ok && (convertedNameBytes > sizeof(records[i].name) || convertedNameBytes % sizeof(TCHAR) != 0 || convertedNameBytes < sizeof(TCHAR)))
			{
				free(convertedName);
				ok = FALSE;
			}
			if (!ok)
			{
				free(rawData);
				break;
			}
			memcpy(records[i].name, convertedName, convertedNameBytes);
			free(convertedName);
			records[i].name[(convertedNameBytes / sizeof(TCHAR)) - 1] = _T('\0');
		}
		if (isStringType && onDiskDataSize > 0)
		{
			BYTE *convertedData = NULL;
			DWORD convertedDataBytes = 0;
			ok = ConvertRegString(fileIsUnicode, rawData, onDiskDataSize, &convertedData, &convertedDataBytes);
			free(rawData);
			if (!ok)
			{
				break;
			}
			records[i].data = convertedData;
			records[i].dataSize = convertedDataBytes;
		}
		else
		{
			records[i].data = rawData;
			records[i].dataSize = onDiskDataSize;
		}
	}
	fclose(fp);
	if (ok)
	{
		SHDeleteKey(HKEY_CURRENT_USER, PLAYERCFG_REG_KEY);
		if (RegCreateKeyEx(HKEY_CURRENT_USER, PLAYERCFG_REG_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
		{
			for (i = 0; i < valueCount; i++)
			{
				RegSetValueEx(hKey, records[i].name, 0, records[i].type, records[i].data, records[i].dataSize);
			}
			RegCloseKey(hKey);
			g_App->bSettingsRestored = TRUE;
		}
		else
		{
			ok = FALSE;
		}
	}
	for (i = 0; i < valueCount; i++)
	{
		free(records[i].data);
	}
	free(records);
	return ok;
}

BOOL ExportPlayerSettings(const TCHAR *path)
{
	HKEY hKey;
	FILE *fp;
	DWORD valueCount;
	DWORD index;
	BOOL ok;
	DWORD isUnicodeBuild = SETTINGS_FILE_THIS_BUILD_IS_UNICODE;
	if (!OpenAppRegKeyRead(&hKey))
	{
		return FALSE;
	}
	if (RegQueryInfoKey(hKey, NULL, NULL, NULL, NULL, NULL, NULL, &valueCount, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FALSE;
	}
	fp = _tfopen(path, _T("wb"));
	if (!fp)
	{
		RegCloseKey(hKey);
		return FALSE;
	}
	ok = (fwrite(SETTINGS_FILE_MAGIC, 1, SETTINGS_FILE_MAGIC_SIZE, fp) == SETTINGS_FILE_MAGIC_SIZE) && (fwrite(&valueCount, sizeof(DWORD), 1, fp) == 1) && (fwrite(&isUnicodeBuild, sizeof(DWORD), 1, fp) == 1);
	for (index = 0; ok && index < valueCount; index++)
	{
		TCHAR name[256];
		DWORD nameSize;
		DWORD type;
		DWORD dataSize;
		DWORD nameBytes;
		DWORD recordSize;
		BYTE *data;
		nameSize = 256;
		dataSize = 0;
		if (RegEnumValue(hKey, index, name, &nameSize, NULL, &type, NULL, &dataSize) != ERROR_SUCCESS)
		{
			ok = FALSE;
			break;
		}
		data = NULL;
		if (dataSize > 0)
		{
			data = (BYTE *)malloc(dataSize);
			if (!data || RegQueryValueEx(hKey, name, NULL, &type, data, &dataSize) != ERROR_SUCCESS)
			{
				free(data);
				ok = FALSE;
				break;
			}
		}
		nameBytes = (DWORD)((_tcslen(name) + 1) * sizeof(TCHAR));
		recordSize = ComputeSettingsRecordSize(nameBytes, dataSize);
		ok = (fwrite(&recordSize, sizeof(DWORD), 1, fp) == 1) && (fwrite(&nameBytes, sizeof(DWORD), 1, fp) == 1) && (fwrite(name, 1, nameBytes, fp) == nameBytes) && (fwrite(&type, sizeof(DWORD), 1, fp) == 1) && (fwrite(&dataSize, sizeof(DWORD), 1, fp) == 1) && (dataSize == 0 || fwrite(data, 1, dataSize, fp) == dataSize);
		free(data);
	}
	fclose(fp);
	RegCloseKey(hKey);
	if (!ok)
	{
		_tremove(path);
	}
	return ok;
}

BOOL DeletePlayerSettings(void)
{
	LONG result = SHDeleteKey(HKEY_CURRENT_USER, PLAYERCFG_REG_KEY);
	if (result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND)
	{
		g_App->bSettingsRestored = TRUE;
		return TRUE;
	}
	return FALSE;
}
