#include "timidplayer.h"

void CloseHandleAndClear(HANDLE *phHandle)
{
	if (*phHandle)
	{
		CloseHandle(*phHandle);
		*phHandle = NULL;
	}
}

int ClampInt(int value, int minValue, int maxValue)
{
	if (value > maxValue)
	{
		return maxValue;
	}
	else if (value < minValue)
	{
		return minValue;
	}
	return value;
}

void ClampDlgItemInt(HWND hWnd, int id, int minValue, int maxValue)
{
	int val = GetDlgItemInt(hWnd, id, NULL, FALSE);
	int clamped = ClampInt(val, minValue, maxValue);
	if (clamped != val)
	{
		SetDlgItemInt(hWnd, id, clamped, FALSE);
	}
}

void SetDlgItemIntText(HWND hWnd, int id, int value)
{
	TCHAR buf[16];
	SafeFormat(buf, 16, _T("%d"), value);
	SetDlgItemText(hWnd, id, buf);
}

void TCharToAnsi(const TCHAR *src, char *dst, int dstSize)
{
	if (dstSize <= 0)
	{
		return;
	}
#ifdef _UNICODE
	if (!WideCharToMultiByte(CP_ACP, 0, src, -1, dst, dstSize, NULL, NULL))
	{
		dst[0] = '\0';
	}
#else
	strncpy(dst, src, dstSize-1);
	dst[dstSize-1] = '\0';
#endif
}

void AnsiToTChar(const char *src, TCHAR *dst, int dstSize)
{
	if (dstSize <= 0)
	{
		return;
	}
#ifdef _UNICODE
	if (!MultiByteToWideChar(CP_ACP, 0, src, -1, dst, dstSize))
	{
		dst[0] = _T('\0');
	}
#else
	strncpy(dst, src, dstSize-1);
	dst[dstSize-1] = '\0';
#endif
}

BOOL AppendBoundedToken(TCHAR *buf, int bufChars, int *pUsed, const TCHAR *text)
{
	int tlen = (int)_tcslen(text);
	if (*pUsed + tlen + 1 >= bufChars)
	{
		return FALSE;
	}
	_tcsncpy(buf + *pUsed, text, tlen);
	buf[*pUsed + tlen] = _T('\0');
	*pUsed += tlen + 1;
	return TRUE;
}

void AppendFormat(TCHAR *buf, int bufSize, const TCHAR *fmt, ...)
{
	int len = (int)_tcslen(buf);
	va_list args;
	if (len >= bufSize-1)
	{
		return;
	}
	va_start(args, fmt);
	_vsntprintf(buf + len, bufSize - len, fmt, args);
	va_end(args);
	buf[bufSize-1] = _T('\0');
}

void SafeFormat(TCHAR *buf, int bufSize, const TCHAR *fmt, ...)
{
	va_list args;
	if (bufSize <= 0)
	{
		return;
	}
	va_start(args, fmt);
	_vsntprintf(buf, bufSize, fmt, args);
	va_end(args);
	buf[bufSize-1] = _T('\0');
}

static int MsToSeconds(int ms)
{
	if (ms > 0)
	{
		return ms / 1000;
	}
	return 0;
}

void FormatTime(int ms, TCHAR *buf, int bufSize)
{
	int totalSec = MsToSeconds(ms);
	int m = totalSec / 60;
	int s = totalSec % 60;
	SafeFormat(buf, bufSize, _T("%d:%02d"), m, s);
}

void FormatTimeHMS(int ms, TCHAR *buf, int bufSize)
{
	int totalSec = MsToSeconds(ms);
	int h = totalSec / 3600;
	int m = (totalSec % 3600) / 60;
	int s = totalSec % 60;
	SafeFormat(buf, bufSize, _T("%d:%02d:%02d"), h, m, s);
}

const TCHAR *GetBaseName(const TCHAR *path)
{
	const TCHAR *backslash = _tcsrchr(path, _T('\\'));
	const TCHAR *slash = _tcsrchr(path, _T('/'));
	const TCHAR *name = backslash;
	if (!name || (slash && slash > name))
	{
		name = slash;
	}
	if (name)
	{
		return name + 1;
	}
	return path;
}

void GetDirectoryPart(const TCHAR *path, TCHAR *dirOut, int dirOutChars)
{
	TCHAR *separator;
	_tcsncpy(dirOut, path, dirOutChars-1);
	dirOut[dirOutChars-1] = _T('\0');
	separator = _tcsrchr(dirOut, _T('\\'));
	if (separator)
	{
		*separator = _T('\0');
	}
}

void GetDirectoryWithSlash(const TCHAR *path, TCHAR *dirOut, int dirOutChars)
{
	TCHAR drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
	_tsplitpath(path, drive, dir, fname, ext);
	SafeFormat(dirOut, dirOutChars, _T("%s%s"), drive, dir);
}

static void GetAppDirectory(TCHAR *buf, int bufSize)
{
	TCHAR modulePath[MAX_PATH];
	buf[0] = _T('\0');
	if (GetModuleFileName(NULL, modulePath, MAX_PATH) == 0)
	{
		return;
	}
	GetDirectoryWithSlash(modulePath, buf, bufSize);
}

void SetAppDirectory(void)
{
	TCHAR appDir[MAX_PATH];
	GetAppDirectory(appDir, MAX_PATH);
	if (appDir[0])
	{
		SetCurrentDirectory(appDir);
	}
}

void ForEachMultiSelectPath(TCHAR *buffer, MultiSelectPathFn addFn, void *ctx)
{
	TCHAR *p = buffer;
	TCHAR dir[MAX_PATH];
	TCHAR path[MAX_PATH];
	SafeFormat(dir, MAX_PATH, _T("%s"), p);
	p += _tcslen(p) + 1;
	if (*p == _T('\0'))
	{
		addFn(ctx, dir);
		return;
	}
	while (*p)
	{
		SafeFormat(path, MAX_PATH, _T("%s\\%s"), dir, p);
		addFn(ctx, path);
		p += _tcslen(p) + 1;
	}
}

BOOL WalkDirectoryFiles(const TCHAR *dir, DirectoryFileFn fn, void *ctx)
{
	TCHAR pattern[MAX_PATH];
	TCHAR entryPath[MAX_PATH];
	WIN32_FIND_DATA fd;
	HANDLE hFind;
	BOOL any = FALSE;
	SafeFormat(pattern, MAX_PATH, _T("%s\\*"), dir);
	hFind = FindFirstFile(pattern, &fd);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	do
	{
		if (_tcscmp(fd.cFileName, _T(".")) == 0 || _tcscmp(fd.cFileName, _T("..")) == 0)
		{
			continue;
		}
		SafeFormat(entryPath, MAX_PATH, _T("%s\\%s"), dir, fd.cFileName);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (WalkDirectoryFiles(entryPath, fn, ctx))
			{
				any = TRUE;
			}
		}
		else if (fn(ctx, entryPath))
		{
			any = TRUE;
		}
	} while (FindNextFile(hFind, &fd));
	FindClose(hFind);
	return any;
}

BOOL AppendPathOrDirectory(const TCHAR *path, DirectoryFileFn fn, void *ctx)
{
	if (PathIsDirectory(path))
	{
		return WalkDirectoryFiles(path, fn, ctx);
	}
	return fn(ctx, path);
}

static UINT WINAPI FileDlgHookProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)hWnd;
	(void)message;
	(void)wParam;
	(void)lParam;
	return 0;
}

static void ApplyStandardOfnFlags(OPENFILENAME *ofn)
{
	ofn->Flags |= (OFN_ENABLEHOOK | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR);
#ifdef OFN_ENABLESIZING
	ofn->Flags |= OFN_ENABLESIZING;
#endif
#ifdef OFN_DONTADDTORECENT
	ofn->Flags |= OFN_DONTADDTORECENT;
#endif
	ofn->lpfnHook = (LPOFNHOOKPROC)FileDlgHookProc;
}

BOOL PromptFileName(HWND hWndOwner, BOOL bSave, TCHAR *path, int pathChars, UINT filterStringId, UINT captionStringId, const TCHAR *defExt, const TCHAR *initialDir, DWORD extraFlags)
{
	OPENFILENAME ofn;
	TCHAR filter[MAX_PATH];
	TCHAR caption[MAX_PATH];
	ZeroMemory(&ofn, sizeof(ofn));
	LoadAppString(g_App->hInst, filterStringId, filter, MAX_PATH);
	LoadAppString(g_App->hInst, captionStringId, caption, MAX_PATH);
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWndOwner;
	ofn.hInstance = g_App->hInst;
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = defExt;
	ofn.lpstrFile = path;
	ofn.nMaxFile = pathChars;
	ofn.lpstrInitialDir = initialDir;
	ofn.lpstrTitle = caption;
	ApplyStandardOfnFlags(&ofn);
	ofn.Flags |= extraFlags;
	if (bSave)
	{
		return GetSaveFileName(&ofn);
	}
	return GetOpenFileName(&ofn);
}

TCHAR *PromptOpenMultipleMidiFiles(HWND hWndOwner)
{
	OPENFILENAME ofn;
	TCHAR *buffer;
	const int bufferChars = 65536;
	TCHAR filter[MAX_PATH];
	TCHAR caption[MAX_PATH];
	buffer = (TCHAR *)malloc(sizeof(TCHAR) * bufferChars);
	if (!buffer)
	{
		return NULL;
	}
	ZeroMemory(&ofn, sizeof(ofn));
	ZeroMemory(buffer, sizeof(TCHAR) * bufferChars);
	LoadAppString(g_App->hInst, IDS_MIDFLT, filter, MAX_PATH);
	LoadAppString(g_App->hInst, IDS_MIDCAP, caption, MAX_PATH);
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWndOwner;
	ofn.hInstance = g_App->hInst;
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = bufferChars;
	ofn.lpstrTitle = caption;
	ofn.Flags = OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST;
	ApplyStandardOfnFlags(&ofn);
	if (!GetOpenFileName(&ofn))
	{
		free(buffer);
		return NULL;
	}
	return buffer;
}

BOOL PromptForFolder(HWND hWndOwner, UINT titleStringId, TCHAR *pathOut)
{
	BROWSEINFO bi;
	TCHAR display[MAX_PATH];
	TCHAR title[MAX_PATH];
	LPITEMIDLIST pidl;
	BOOL result = FALSE;
	ZeroMemory(&bi, sizeof(bi));
	ZeroMemory(display, sizeof(display));
	LoadAppString(g_App->hInst, titleStringId, title, MAX_PATH);
	bi.hwndOwner = hWndOwner;
	bi.pszDisplayName = display;
	bi.lpszTitle = title;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	pidl = SHBrowseForFolder(&bi);
	if (pidl)
	{
		if (SHGetPathFromIDList(pidl, pathOut))
		{
			result = TRUE;
		}
		CoTaskMemFree(pidl);
	}
	return result;
}

void GetSongMetadata(Timid *synth, TCHAR *titleOut, TCHAR *copyrightOut)
{
	char titleAnsi[256], copyrightAnsi[256];
	ZeroMemory(titleAnsi, sizeof(titleAnsi));
	ZeroMemory(copyrightAnsi, sizeof(copyrightAnsi));
	timid_get_song_title(synth, titleAnsi, sizeof(titleAnsi));
	timid_get_song_copyright(synth, copyrightAnsi, sizeof(copyrightAnsi));
	titleOut[0] = _T('\0');
	copyrightOut[0] = _T('\0');
	if (titleAnsi[0])
	{
		AnsiToTChar(titleAnsi, titleOut, 256);
	}
	if (copyrightAnsi[0])
	{
		AnsiToTChar(copyrightAnsi, copyrightOut, 256);
	}
}

void InitDriverConfigDefaults(DriverConfig *cfg)
{
	ZeroMemory(cfg, sizeof(DriverConfig));
	cfg->nSampleRate = DEFAULT_RATE;
	cfg->nControlRate = CONTROLS_PER_SECOND;
	cfg->nVoices = DEFAULT_VOICES;
	cfg->nAmp = DEFAULT_AMPLIFICATION;
	cfg->fAdjustPanning = TRUE;
	cfg->fMono = FALSE;
	cfg->f8Bit = FALSE;
	cfg->fAntialiasing = TRUE;
	cfg->fPreResample = TRUE;
	cfg->fFastDecay = TRUE;
	cfg->fDynamicLoad = FALSE;
	cfg->nDefaultProgram = DEFAULT_PROGRAM;
	cfg->nDrumChannels = DEFAULT_DRUMCHANNELS;
	cfg->nQuietChannels = 0;
	cfg->fReverbEnabled = FALSE;
	cfg->fReverbOnly = FALSE;
	cfg->nReverbLevel = 100;
	cfg->nReverbPreset = 0;
	cfg->fChorusEnabled = FALSE;
	cfg->nChorusDepth = 25;
	cfg->fDitherEnabled = FALSE;
}

void LoadAppString(HINSTANCE hInst, UINT id, TCHAR *buf, int bufChars)
{
	ZeroMemory(buf, sizeof(TCHAR) * bufChars);
	LoadString(hInst, id, buf, bufChars);
}

static HWND ResolveVisibleOwner(HWND hWnd)
{
	if (hWnd && IsWindowVisible(hWnd))
	{
		return hWnd;
	}
	return NULL;
}

static void RestoreFocus(HWND hFocus)
{
	if (hFocus && IsWindow(hFocus))
	{
		SetFocus(hFocus);
	}
}

int ShowMessageBox(HWND hWndOwner, const TCHAR *text, const TCHAR *caption, UINT type)
{
	HWND hFocus = GetFocus();
	int result = MessageBox(ResolveVisibleOwner(hWndOwner), text, caption, type);
	RestoreFocus(hFocus);
	return result;
}

void ShowAppStringMessage(HWND hWnd, UINT msgId, UINT iconFlags)
{
	TCHAR msg[MAX_PATH];
	LoadAppString(g_App->hInst, msgId, msg, MAX_PATH);
	ShowMessageBox(hWnd, msg, g_App->appTitle, iconFlags);
}

int ShowFormattedAppMessage(HWND hWnd, UINT fmtId, UINT iconFlags, ...)
{
	TCHAR fmt[MAX_PATH];
	TCHAR msg[MAX_PATH*2];
	va_list args;
	LoadAppString(g_App->hInst, fmtId, fmt, MAX_PATH);
	va_start(args, iconFlags);
	_vsntprintf(msg, MAX_PATH*2, fmt, args);
	va_end(args);
	msg[MAX_PATH*2-1] = _T('\0');
	return ShowMessageBox(hWnd, msg, g_App->appTitle, iconFlags);
}

BOOL ConfirmAppString(HWND hWnd, UINT msgId)
{
	TCHAR msg[MAX_PATH];
	LoadAppString(g_App->hInst, msgId, msg, MAX_PATH);
	return ShowMessageBox(hWnd, msg, g_App->appTitle, MB_ICONQUESTION | MB_YESNO) == IDYES;
}

void ShowStringErrorWithCaption(HWND hWnd, UINT msgId, UINT capId)
{
	TCHAR msg[MAX_PATH];
	TCHAR caption[MAX_PATH];
	LoadAppString(g_App->hInst, msgId, msg, MAX_PATH);
	LoadAppString(g_App->hInst, capId, caption, MAX_PATH);
	ShowMessageBox(hWnd, msg, caption, MB_ICONERROR);
}

INT_PTR ShowDialogBox(HINSTANCE hInstance, LPCTSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc)
{
	HWND hFocus = GetFocus();
	INT_PTR result = DialogBox(hInstance, lpTemplate, ResolveVisibleOwner(hWndParent), lpDialogFunc);
	RestoreFocus(hFocus);
	return result;
}

void PopulateDeviceCombo(HWND hCombo, UINT numDevs, DeviceNameFn getName, UINT defaultLabelId, int currentDeviceId)
{
	UINT i;
	int selectIndex = 0;
	TCHAR devLabel[64];
	LoadAppString(g_App->hInst, defaultLabelId, devLabel, 64);
	SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)devLabel);
	for (i = 0; i < numDevs; i++)
	{
		if (!getName(i, devLabel, 64))
		{
			LoadAppString(g_App->hInst, IDS_UNKNOWNDEVICE, devLabel, 64);
		}
		SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)devLabel);
		if (currentDeviceId == (int)i)
		{
			selectIndex = (int)i + 1;
		}
	}
	SendMessage(hCombo, CB_SETCURSEL, selectIndex, 0);
}

int ComboSelToDeviceId(int sel, int mapperValue)
{
	if (sel <= 0)
	{
		return mapperValue;
	}
	return sel - 1;
}

void PopulateChannelCombo(HWND hCombo)
{
	int i;
	TCHAR buf[8];
	for (i = 0; i < 16; i++)
	{
		SafeFormat(buf, 8, _T("%d"), i + 1);
		SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)buf);
	}
}

void InitSingleColumnListView(HWND hList)
{
	RECT rc;
	LVCOLUMN lvc;
	SendMessage(hList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
	GetClientRect(hList, &rc);
	ZeroMemory(&lvc, sizeof(lvc));
	lvc.mask = LVCF_FMT | LVCF_WIDTH;
	lvc.fmt = LVCFMT_LEFT;
	lvc.cx = rc.right - rc.left;
	ListView_InsertColumn(hList, 0, &lvc);
}

void InsertListViewItem(HWND hList, int index, const TCHAR *text)
{
	LVITEM lvi;
	ZeroMemory(&lvi, sizeof(lvi));
	lvi.mask = LVIF_TEXT;
	lvi.iItem = index;
	lvi.iSubItem = 0;
	lvi.pszText = (LPTSTR)text;
	ListView_InsertItem(hList, &lvi);
}

void SetListViewCurSel(HWND hList, int index)
{
	ListView_SetItemState(hList, -1, 0, LVIS_SELECTED);
	if (index >= 0)
	{
		ListView_SetItemState(hList, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		ListView_EnsureVisible(hList, index, FALSE);
	}
}

int GetListViewCurSel(HWND hList)
{
	return ListView_GetNextItem(hList, -1, LVNI_SELECTED);
}

void SelectClampedListViewIndex(HWND hList, int anchorIndex, int newCount)
{
	int newSel;
	if (newCount <= 0)
	{
		return;
	}
	newSel = anchorIndex;
	if (newSel >= newCount)
	{
		newSel = newCount - 1;
	}
	SetListViewCurSel(hList, newSel);
}

int *GetSortedSelectedListViewItems(HWND hList, int *pCount, BOOL descending)
{
	int count = ListView_GetSelectedCount(hList);
	int *items;
	int i;
	int idx;
	*pCount = 0;
	if (count <= 0)
	{
		return NULL;
	}
	items = (int *)malloc(sizeof(int) * count);
	if (!items)
	{
		return NULL;
	}
	idx = -1;
	i = 0;
	while (i < count && (idx = ListView_GetNextItem(hList, idx, LVNI_SELECTED)) != -1)
	{
		items[i++] = idx;
	}
	for (i = 0; i < count-1; i++)
	{
		int j;
		for (j = i+1; j < count; j++)
		{
			if ((descending && items[j] > items[i]) || (!descending && items[j] < items[i]))
			{
				int tmp = items[i];
				items[i] = items[j];
				items[j] = tmp;
			}
		}
	}
	*pCount = count;
	return items;
}

int *MoveSelectedListViewItems(HWND hList, int itemCount, BOOL up, int *pCount, void (*swapFn)(int indexA, int indexB))
{
	int count;
	int *items = GetSortedSelectedListViewItems(hList, &count, !up);
	int i;
	int limit;
	int delta;
	if (!items)
	{
		*pCount = 0;
		return NULL;
	}
	if (up)
	{
		limit = -1;
		delta = -1;
	}
	else
	{
		limit = itemCount;
		delta = 1;
	}
	for (i = 0; i < count; i++)
	{
		int idx = items[i];
		if (idx == limit - delta)
		{
			limit = idx;
			continue;
		}
		swapFn(idx, idx + delta);
		items[i] = idx + delta;
	}
	*pCount = count;
	return items;
}

void ReselectMovedListViewItems(HWND hList, int *items, int count, int maxCount)
{
	int i;
	int topIndex = -1;
	for (i = 0; i < count; i++)
	{
		if (items[i] >= 0 && items[i] < maxCount)
		{
			ListView_SetItemState(hList, items[i], LVIS_SELECTED, LVIS_SELECTED);
			if (topIndex < 0 || items[i] < topIndex)
			{
				topIndex = items[i];
			}
		}
	}
	if (topIndex >= 0)
	{
		ListView_SetItemState(hList, topIndex, LVIS_FOCUSED, LVIS_FOCUSED);
		ListView_EnsureVisible(hList, topIndex, FALSE);
	}
	free(items);
}
