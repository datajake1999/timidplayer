#include "timidplayer.h"

AppState *g_App = NULL;

static int ExtractCmdLineTokens(TCHAR *outBuffer, int outBufferChars)
{
	int argc = 0;
	LPWSTR *argv;
	int used = 0;
	int count = 0;
	int i;
	BOOL bStop = FALSE;
	BOOL bLiteralNext = FALSE;
	outBuffer[0] = _T('\0');
	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (!argv)
	{
		return 0;
	}
	for (i = 1; i < argc && !bStop; i++)
	{
		TCHAR tpath[MAX_PATH];
		TCHAR fullPath[MAX_PATH];
		DWORD fullLen;
		BOOL bLiteral;
#ifdef _UNICODE
		SafeFormat(tpath, MAX_PATH, _T("%s"), argv[i]);
#else
		WideCharToMultiByte(CP_ACP, 0, argv[i], -1, tpath, MAX_PATH, NULL, NULL);
#endif
		bLiteral = bLiteralNext || _tcsicmp(tpath, CMDLINE_ENQUEUE_SWITCH) == 0 || _tcsicmp(tpath, CMDLINE_BATCH_SWITCH) == 0 || _tcsicmp(tpath, CMDLINE_OUTDIR_SWITCH) == 0 || _tcsicmp(tpath, CMDLINE_FORMAT_SWITCH) == 0 || _tcsicmp(tpath, CMDLINE_OVERWRITE_SWITCH) == 0 || _tcsicmp(tpath, CMDLINE_PRESERVEDIRS_SWITCH) == 0 || _tcsicmp(tpath, CMDLINE_AUTOSTART_SWITCH) == 0 || _tcsicmp(tpath, CMDLINE_AUTOSAVECSV_SWITCH) == 0;
		bLiteralNext = (_tcsicmp(tpath, CMDLINE_FORMAT_SWITCH) == 0);
		if (bLiteral)
		{
			fullLen = 0;
		}
		else
		{
			fullLen = GetFullPathName(tpath, MAX_PATH, fullPath, NULL);
		}
		if (fullLen == 0 || fullLen >= MAX_PATH)
		{
			SafeFormat(fullPath, MAX_PATH, _T("%s"), tpath);
		}
		if (fullLen != 0 && (_tcschr(fullPath, _T('*')) || _tcschr(fullPath, _T('?'))))
		{
			TCHAR drive[_MAX_DRIVE], dirPart[_MAX_DIR];
			WIN32_FIND_DATA fd;
			HANDLE hFind;
			_tsplitpath(fullPath, drive, dirPart, NULL, NULL);
			hFind = FindFirstFile(fullPath, &fd);
			if (hFind != INVALID_HANDLE_VALUE)
			{
				do
				{
					TCHAR matchPath[MAX_PATH];
					if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					{
						continue;
					}
					_tmakepath(matchPath, drive, dirPart, fd.cFileName, NULL);
					if (!AppendBoundedToken(outBuffer, outBufferChars, &used, matchPath))
					{
						bStop = TRUE;
						break;
					}
					count++;
				} while (FindNextFile(hFind, &fd));
				FindClose(hFind);
			}
			continue;
		}
		if (!AppendBoundedToken(outBuffer, outBufferChars, &used, fullPath))
		{
			break;
		}
		count++;
	}
	outBuffer[used] = _T('\0');
	LocalFree(argv);
	return count;
}

static TCHAR *AllocAndExtractCmdLineTokens(PSTR szCmdLine, int bufChars, int *pCount)
{
	TCHAR *buf = NULL;
	*pCount = 0;
	if (szCmdLine && szCmdLine[0])
	{
		buf = (TCHAR *)malloc(sizeof(TCHAR) * bufChars);
		if (buf)
		{
			*pCount = ExtractCmdLineTokens(buf, bufChars);
		}
	}
	return buf;
}

static void ShowInitError(HINSTANCE hInstance, HWND hOwner, const TCHAR *appTitle, UINT stringId)
{
	TCHAR errMsg[MAX_PATH];
	LoadAppString(hInstance, stringId, errMsg, MAX_PATH);
	ShowMessageBox(hOwner, errMsg, appTitle, MB_ICONERROR);
}

static BOOL WINAPI AlreadyRunningDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	switch (message)
	{
	case WM_INITDIALOG:
		{
			TCHAR prompt[MAX_PATH];
			LoadAppString(GetModuleHandle(NULL), IDS_ALREADYRUNNINGQ, prompt, MAX_PATH);
			SetDlgItemText(hWnd, IDC_ALREADYRUNNINGTEXT, prompt);
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDYES:
		case IDNO:
			if (IsDlgButtonChecked(hWnd, IDC_DONTASKAGAIN) == BST_CHECKED)
			{
				SaveLoadFilesIntoRunningInstanceSetting(LOWORD(wParam) == IDYES);
				SuppressAlreadyRunningPrompt();
			}
			EndDialog(hWnd, LOWORD(wParam));
			return TRUE;
		}
	}
	return FALSE;
}

static void FreeGlobalBuffers(void)
{
	if (!g_App)
	{
		return;
	}
	free(g_App->files);
	g_App->files = NULL;
	free(g_App->fileResultText);
	g_App->fileResultText = NULL;
	free(g_App->fileResultData);
	g_App->fileResultData = NULL;
	free(g_App->fileSubDirs);
	g_App->fileSubDirs = NULL;
	free(g_App->playlist);
	g_App->playlist = NULL;
	free(g_App->waveHdr);
	g_App->waveHdr = NULL;
	free(g_App);
	g_App = NULL;
}

static BOOL AllocateGlobalBuffers(void)
{
	g_App = (AppState *)malloc(sizeof(AppState));
	if (!g_App)
	{
		return FALSE;
	}
	ZeroMemory(g_App, sizeof(AppState));
	g_App->files = (FileEntry *)malloc(sizeof(FileEntry) * MAX_BATCH_FILES);
	g_App->fileResultText = (FileResultText *)malloc(sizeof(FileResultText) * MAX_BATCH_FILES);
	g_App->fileResultData = (FileResultData *)malloc(sizeof(FileResultData) * MAX_BATCH_FILES);
	g_App->fileSubDirs = (FileSubDir *)malloc(sizeof(FileSubDir) * MAX_BATCH_FILES);
	g_App->playlist = (FileEntry *)malloc(sizeof(FileEntry) * MAX_PLAYLIST);
	g_App->waveHdr = (WAVEHDR *)malloc(sizeof(WAVEHDR) * MAX_PLAYER_CHUNKS);
	if (!g_App->files || !g_App->fileResultText || !g_App->fileResultData || !g_App->fileSubDirs || !g_App->playlist || !g_App->waveHdr)
	{
		FreeGlobalBuffers();
		return FALSE;
	}
	ZeroMemory(g_App->files, sizeof(FileEntry) * MAX_BATCH_FILES);
	ZeroMemory(g_App->fileResultText, sizeof(FileResultText) * MAX_BATCH_FILES);
	ZeroMemory(g_App->fileResultData, sizeof(FileResultData) * MAX_BATCH_FILES);
	ZeroMemory(g_App->fileSubDirs, sizeof(FileSubDir) * MAX_BATCH_FILES);
	ZeroMemory(g_App->playlist, sizeof(FileEntry) * MAX_PLAYLIST);
	ZeroMemory(g_App->waveHdr, sizeof(WAVEHDR) * MAX_PLAYER_CHUNKS);
	g_App->playlistIndex = -1;
	g_App->outputDeviceId = WAVE_MAPPER;
	g_App->pendingSeekMs = -1;
	return TRUE;
}

static void TeardownAndExit(HANDLE hMutex, TCHAR *cmdBuf)
{
	free(cmdBuf);
	FreeGlobalBuffers();
	CoUninitialize();
	CloseHandle(hMutex);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
{
	WNDCLASS wc;
	MSG msg;
	TCHAR appTitle[MAX_PATH];
	const TCHAR className[] = _T("TimidityPlayerWndClass");
	const TCHAR mutexName[] = _T("TimidityPlayerMutex");
	const TCHAR promptMutexName[] = _T("TimidityPlayerPromptMutex");
	HANDLE hMutex;
	HACCEL hAccel;
	BOOL bComInitialized;
	BOOL bGetMsgRet;
	TCHAR *cmdBuf;
	int cmdCount;
	const int cmdBufChars = MAX_PLAYLIST * MAX_PATH + 1;
	(void)hPrevInstance;
	LoadAppString(hInstance, IDS_APPTITLE, appTitle, MAX_PATH);
	hMutex = CreateMutex(NULL, FALSE, mutexName);
	if (!hMutex)
	{
		ShowInitError(hInstance, NULL, appTitle, IDS_MUTEXFAILED);
		return 0;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		HANDLE hPromptMutex;
		BOOL bTrayMode;
		BOOL bSwitch;
		HWND hExisting;
		cmdBuf = AllocAndExtractCmdLineTokens(szCmdLine, cmdBufChars, &cmdCount);
		hPromptMutex = CreateMutex(NULL, FALSE, promptMutexName);
		if (hPromptMutex)
		{
			WaitForSingleObject(hPromptMutex, INFINITE);
		}
		hExisting = FindWindow(className, NULL);
		bTrayMode = RunFromTrayEnabled();
		if (!hExisting)
		{
			bSwitch = FALSE;
		}
		else if (cmdCount > 0)
		{
			bSwitch = LoadFilesIntoRunningInstanceEnabled();
			if (!bTrayMode && !AlreadyRunningPromptSuppressed())
			{
				bSwitch = (ShowDialogBox(hInstance, MAKEINTRESOURCE(IDD_ALREADYRUNNING), NULL, (DLGPROC)AlreadyRunningDialogProc) == IDYES);
			}
		}
		else
		{
			bSwitch = TRUE;
		}
		if (hExisting)
		{
			if (bSwitch)
			{
				if (cmdCount > 0 && cmdBuf)
				{
					COPYDATASTRUCT cds;
					TCHAR *p = cmdBuf;
					while (*p)
					{
						p += _tcslen(p) + 1;
					}
					p++;
					cds.dwData = COPYDATA_OPENPATH;
					cds.cbData = (DWORD)((p - cmdBuf) * sizeof(TCHAR));
					cds.lpData = cmdBuf;
					SendMessage(hExisting, WM_COPYDATA, 0, (LPARAM)&cds);
				}
				if (!bTrayMode && IsWindowEnabled(hExisting))
				{
					if (IsIconic(hExisting))
					{
						ShowWindow(hExisting, SW_RESTORE);
					}
					SetForegroundWindow(hExisting);
				}
			}
		}
		else
		{
			ShowInitError(hInstance, NULL, appTitle, IDS_ALREADYRUNNING);
		}
		free(cmdBuf);
		if (hPromptMutex)
		{
			ReleaseMutex(hPromptMutex);
			CloseHandle(hPromptMutex);
		}
		CloseHandle(hMutex);
		return 0;
	}
	InitCommonControls();
	bComInitialized = SUCCEEDED(CoInitialize(NULL));
	if (!bComInitialized)
	{
		ShowInitError(hInstance, NULL, appTitle, IDS_COMFAILED);
		CloseHandle(hMutex);
		return 0;
	}
	srand((unsigned)time(NULL));
	if (!AllocateGlobalBuffers())
	{
		ShowInitError(hInstance, NULL, appTitle, IDS_MEMFAILED);
		CoUninitialize();
		CloseHandle(hMutex);
		return 0;
	}
	g_App->hInst = hInstance;
	LoadAppString(g_App->hInst, IDS_APPTITLE, g_App->appTitle, MAX_PATH);
	cmdBuf = AllocAndExtractCmdLineTokens(szCmdLine, cmdBufChars, &cmdCount);
	if (cmdCount > 0 && _tcsicmp(cmdBuf, CMDLINE_BATCH_SWITCH) == 0)
	{
		OpenCommandLineBatch(NULL, cmdBuf);
		TeardownAndExit(hMutex, cmdBuf);
		return 0;
	}
	LoadAudioOutputSettings();
	LoadPlayerOptions();
	LoadAlwaysOnTopSetting();
	LoadViewSettings();
	LoadRecentFiles();
	ZeroMemory(&wc, sizeof(wc));
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = PlayerWndProc;
	wc.hInstance = g_App->hInst;
	wc.hIcon = LoadIcon(g_App->hInst, MAKEINTRESOURCE(IDI_ICON));
	if (!wc.hIcon)
	{
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	}
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MAINMENU);
	wc.lpszClassName = className;
	if (!RegisterClass(&wc))
	{
		ShowInitError(hInstance, NULL, appTitle, IDS_REGCLASSFAILED);
		TeardownAndExit(hMutex, cmdBuf);
		return 0;
	}
	g_App->hPlayerWnd = CreateWindow(className, g_App->appTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, g_App->hInst, NULL);
	if (!g_App->hPlayerWnd)
	{
		ShowInitError(hInstance, NULL, appTitle, IDS_CREATEWNDFAILED);
		UnregisterClass(className, g_App->hInst);
		TeardownAndExit(hMutex, cmdBuf);
		return 0;
	}
	hAccel = LoadAccelerators(g_App->hInst, MAKEINTRESOURCE(IDR_ACCEL));
	if (!hAccel)
	{
		ShowInitError(hInstance, g_App->hPlayerWnd, appTitle, IDS_ACCELFAILED);
		DestroyWindow(g_App->hPlayerWnd);
		UnregisterClass(className, g_App->hInst);
		TeardownAndExit(hMutex, cmdBuf);
		return 0;
	}
	UpdateWindowTitle(g_App->hPlayerWnd);
	if (!g_App->bRunFromTray)
	{
		ShowWindow(g_App->hPlayerWnd, iCmdShow);
		UpdateWindow(g_App->hPlayerWnd);
	}
	if (cmdCount > 0)
	{
		if (g_App->bLoadPreviousPlaylist && _tcsicmp(cmdBuf, CMDLINE_ENQUEUE_SWITCH) == 0)
		{
			LoadSession(g_App->hPlayerWnd);
		}
		OpenCommandLinePath(g_App->hPlayerWnd, cmdBuf);
		free(cmdBuf);
	}
	else if (g_App->bLoadPreviousPlaylist)
	{
		LoadSession(g_App->hPlayerWnd);
	}
	while ((bGetMsgRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bGetMsgRet == -1)
		{
			break;
		}
		if (msg.message == WM_KEYDOWN && msg.hwnd == g_App->hPlaylistView)
		{
			switch (msg.wParam)
			{
			case VK_TAB:
				SetFocus(g_App->hPlayerWnd);
				continue;
			case VK_RETURN:
				{
					int idx = GetListViewCurSel(g_App->hPlaylistView);
					if (idx >= 0 && idx < g_App->playlistCount)
					{
						PlayPlaylistEntryAt(g_App->hPlayerWnd, idx);
					}
				}
				continue;
			case VK_DELETE:
				{
					int idx = GetListViewCurSel(g_App->hPlaylistView);
					if (idx >= 0 && idx < g_App->playlistCount)
					{
						BOOL bRemove = TRUE;
						if (g_App->bConfirmPlaylistDelete)
						{
							bRemove = (ShowFormattedAppMessage(g_App->hPlayerWnd, IDS_CONFIRMDELETEPLAYLISTITEM, MB_ICONQUESTION | MB_YESNO, GetBaseName(g_App->playlist[idx].path)) == IDYES);
						}
						if (bRemove)
						{
							RemovePlaylistItemAt(g_App->hPlayerWnd, idx);
						}
					}
				}
				continue;
			case VK_SPACE:
			case VK_LEFT:
			case VK_RIGHT:
			case VK_UP:
			case VK_DOWN:
			case VK_PRIOR:
			case VK_NEXT:
			case VK_HOME:
			case VK_END:
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				continue;
			}
		}
		else if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB && msg.hwnd == g_App->hPlayerWnd && g_App->bShowPlaylistView)
		{
			if (g_App->playlistCount > 0 && GetListViewCurSel(g_App->hPlaylistView) == -1)
			{
				int sel;
				if (g_App->playlistIndex >= 0 && g_App->playlistIndex < g_App->playlistCount)
				{
					sel = g_App->playlistIndex;
				}
				else
				{
					sel = 0;
				}
				SetListViewCurSel(g_App->hPlaylistView, sel);
			}
			SetFocus(g_App->hPlaylistView);
			continue;
		}
		if (!TranslateAccelerator(g_App->hPlayerWnd, hAccel, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	UnregisterClass(className, g_App->hInst);
	FreeGlobalBuffers();
	CoUninitialize();
	CloseHandle(hMutex);
	return (int)msg.wParam;
}
