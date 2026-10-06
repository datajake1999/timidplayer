#include "timidplayer.h"

#include "../wav_writer/wav_writer.h"

#define PROGRESS_UPDATE_MS 100

#define WM_CONV_PROGRESS (WM_APP+4)
#define WM_CONV_DONE (WM_APP+5)
#define WM_CONV_FILERESULT (WM_APP+6)

#define FILE_RESULT_CONVERTED 0
#define FILE_RESULT_LOAD_FAILED 1
#define FILE_RESULT_OUTPUT_FAILED 2
#define FILE_RESULT_SKIPPED 3

typedef struct {
	int fileIndex;
	int fileCount;
	int percent;
	TCHAR fileName[MAX_PATH];
} ProgressInfo;

typedef struct {
	int fileIndex;
	int outcome;
	TCHAR title[256];
	TCHAR copyright[256];
	int durationMs;
	int eventCount;
	int bitrate;
	int lostNotes;
	int cutNotes;
	double realtimeRate;
} FileResultInfo;

typedef struct {
	int converted;
	int failed;
	int skipped;
	int totalLostNotes;
	int totalCutNotes;
	int totalAudioMs;
	double totalWallSeconds;
	BOOL canceled;
} ConvResult;

typedef struct {
	int labelId;
	int audioFormat;
	int bitDepth;
	int wavFormatTag;
} BatchOutputFormat;

static const BatchOutputFormat s_outputFormats[] =
{
	{ IDS_FMT_8BITINT, AU_CHAR, 8, 1 },
	{ IDS_FMT_16BITINT, AU_SHORT, 16, 1 },
	{ IDS_FMT_24BITINT, AU_24, 24, 1 },
	{ IDS_FMT_32BITINT, AU_LONG, 32, 1 },
	{ IDS_FMT_32BITFLOAT, AU_FLOAT, 32, 3 },
	{ IDS_FMT_64BITFLOAT, AU_DOUBLE, 64, 3 },
	{ IDS_FMT_8BITULAW, AU_ULAW, 8, 7 }
};

#define BATCH_FORMAT_COUNT (sizeof(s_outputFormats) / sizeof(s_outputFormats[0]))
#define BATCH_FORMAT_DEFAULT 1

static BOOL ShouldPreserveSubDir(const TCHAR *subDir, const TCHAR *outDir, BOOL preserveDirStructure)
{
	return preserveDirStructure && outDir && outDir[0] && subDir && subDir[0];
}

static void BuildOutputPath(const TCHAR *inputPath, const TCHAR *subDir, const TCHAR *outDir, BOOL preserveDirStructure, TCHAR *outPath, int outPathSize)
{
	TCHAR drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
	_tsplitpath(inputPath, drive, dir, fname, ext);
	if (outDir && outDir[0])
	{
		if (ShouldPreserveSubDir(subDir, outDir, preserveDirStructure))
		{
			SafeFormat(outPath, outPathSize, _T("%s\\%s\\%s.wav"), outDir, subDir, fname);
		}
		else
		{
			SafeFormat(outPath, outPathSize, _T("%s\\%s.wav"), outDir, fname);
		}
	}
	else
	{
		SafeFormat(outPath, outPathSize, _T("%s%s%s.wav"), drive, dir, fname);
	}
}

static BOOL TryCreateDirectoryPath(HWND hWndOwner, const TCHAR *dir)
{
	return SHCreateDirectoryEx(hWndOwner, dir, NULL) == ERROR_SUCCESS || PathIsDirectory(dir);
}

static BOOL EnsureDirectorySilent(const TCHAR *dir)
{
	if (!dir || !dir[0])
	{
		return TRUE;
	}
	if (PathIsDirectory(dir))
	{
		return TRUE;
	}
	return TryCreateDirectoryPath(NULL, dir);
}

static void PostProgress(int fileIndex, int fileCount, int percent, const TCHAR *fileName)
{
	ProgressInfo *info = (ProgressInfo *)malloc(sizeof(ProgressInfo));
	if (!info)
	{
		return;
	}
	info->fileIndex = fileIndex;
	info->fileCount = fileCount;
	info->percent = percent;
	SafeFormat(info->fileName, MAX_PATH, _T("%s"), fileName);
	PostMessage(g_App->hBatchWnd, WM_CONV_PROGRESS, 0, (LPARAM)info);
}

static void PostFileResult(int fileIndex, int outcome, const TCHAR *title, const TCHAR *copyright, int durationMs, int eventCount, int bitrate, int lostNotes, int cutNotes, double realtimeRate)
{
	FileResultInfo *info = (FileResultInfo *)malloc(sizeof(FileResultInfo));
	if (!info)
	{
		return;
	}
	info->fileIndex = fileIndex;
	info->outcome = outcome;
	info->title[0] = _T('\0');
	info->copyright[0] = _T('\0');
	if (title && title[0])
	{
		SafeFormat(info->title, 256, _T("%s"), title);
	}
	if (copyright && copyright[0])
	{
		SafeFormat(info->copyright, 256, _T("%s"), copyright);
	}
	info->durationMs = durationMs;
	info->eventCount = eventCount;
	info->bitrate = bitrate;
	info->lostNotes = lostNotes;
	info->cutNotes = cutNotes;
	info->realtimeRate = realtimeRate;
	PostMessage(g_App->hBatchWnd, WM_CONV_FILERESULT, 0, (LPARAM)info);
}

static int ComputePercent(Timid *synth)
{
	int total = timid_get_sample_count(synth);
	int pos = timid_get_current_sample_position(synth);
	if (total <= 0)
	{
		return 0;
	}
	if (pos > total)
	{
		pos = total;
	}
	return (int)(((double)pos / (double)total) * 100.0);
}

static BOOL OpenTempWavWriter(WavWriter *wr, const TCHAR *outPath, TCHAR *tempPath, int tempPathSize, int sampleRate, int bit_depth, int channels, int wavFormatTag)
{
	char ansiOut[MAX_PATH];
	SafeFormat(tempPath, tempPathSize, _T("%s.tmp"), outPath);
	DeleteFile(tempPath);
	TCharToAnsi(tempPath, ansiOut, MAX_PATH);
	ZeroMemory(wr, sizeof(WavWriter));
	return WavFileOpen(wr, ansiOut, sampleRate, bit_depth, channels, wavFormatTag);
}

static BOOL FinalizeWavOutput(const TCHAR *tempPath, const TCHAR *outPath)
{
	DeleteFile(outPath);
	if (!MoveFile(tempPath, outPath))
	{
		DeleteFile(tempPath);
		return FALSE;
	}
	return TRUE;
}

static unsigned char *AllocAudioRenderBuffer(int channels, int bit_depth)
{
	return (unsigned char *)malloc(AUDIO_BUFFER_SIZE * channels * (bit_depth / 8));
}

typedef void (*RenderProgressFn)(void *ctx, int percent);

static BOOL RenderSmfToWav(Timid *synth, int audio_format, unsigned char *buffer, WavWriter *wr, HANDLE hCancelEvent, RenderProgressFn onProgress, void *progressCtx, BOOL *pCanceled)
{
	DWORD lastUpdate = GetTickCount();
	DWORD now;
	while (timid_play_smf(synth, audio_format, buffer, AUDIO_BUFFER_SIZE))
	{
		if (!WavFileWrite(wr, buffer, AUDIO_BUFFER_SIZE))
		{
			return FALSE;
		}
		if (hCancelEvent && WaitForSingleObject(hCancelEvent, 0) == WAIT_OBJECT_0)
		{
			*pCanceled = TRUE;
			break;
		}
		now = GetTickCount();
		if (now - lastUpdate >= PROGRESS_UPDATE_MS)
		{
			lastUpdate = now;
			onProgress(progressCtx, ComputePercent(synth));
		}
	}
	return TRUE;
}

typedef struct {
	BOOL canceled;
	BOOL createFailed;
	int lostNotes;
	int cutNotes;
	int audioMs;
	double timeSpent;
	double realtimeRate;
} SmfRenderStats;

static BOOL RenderSmfFileToWav(Timid *synth, int audio_format, int channels, int bit_depth, int sampleRate, int wavFormatTag, unsigned char *buffer, const TCHAR *outPath, HANDLE hCancelEvent, RenderProgressFn onProgress, void *progressCtx, SmfRenderStats *stats)
{
	WavWriter wr;
	TCHAR tempPath[MAX_PATH];
	clock_t begin, end;
	BOOL writeOk;
	ZeroMemory(stats, sizeof(SmfRenderStats));
	if (!OpenTempWavWriter(&wr, outPath, tempPath, MAX_PATH, sampleRate, bit_depth, channels, wavFormatTag))
	{
		stats->createFailed = TRUE;
		return FALSE;
	}
	begin = clock();
	writeOk = RenderSmfToWav(synth, audio_format, buffer, &wr, hCancelEvent, onProgress, progressCtx, &stats->canceled);
	end = clock();
	WavFileClose(&wr);
	stats->lostNotes = timid_get_lost_notes(synth);
	stats->cutNotes = timid_get_cut_notes(synth);
	stats->audioMs = timid_get_current_time(synth);
	stats->timeSpent = (double)(end - begin) / CLOCKS_PER_SEC;
	if (stats->timeSpent > 0.0)
	{
		stats->realtimeRate = ((double)stats->audioMs / 1000.0) / stats->timeSpent;
	}
	if (stats->canceled || !writeOk)
	{
		DeleteFile(tempPath);
		return FALSE;
	}
	return FinalizeWavOutput(tempPath, outPath);
}

typedef struct {
	int fileIndex;
	int fileCount;
	const TCHAR *fileName;
} ConversionProgressCtx;

static void ConversionProgressCallback(void *ctx, int percent)
{
	ConversionProgressCtx *pc = (ConversionProgressCtx *)ctx;
	PostProgress(pc->fileIndex, pc->fileCount, percent, pc->fileName);
}

static int FailConversion(ConvResult *result)
{
	if (result)
	{
		result->failed = g_App->fileCount;
	}
	PostMessage(g_App->hBatchWnd, WM_CONV_DONE, 0, (LPARAM)result);
	return 1;
}

static DWORD WINAPI ConversionThreadProc(LPVOID param)
{
	Timid *synth;
	unsigned char *buffer;
	ConvResult *result;
	int channels, bit_depth, audio_format, sampleRate, i;
	int fmtIndex;
	BOOL canceled = FALSE;
	(void)param;
	result = (ConvResult *)malloc(sizeof(ConvResult));
	if (!result)
	{
		return FailConversion(NULL);
	}
	result->converted = 0;
	result->failed = 0;
	result->skipped = 0;
	result->totalLostNotes = 0;
	result->totalCutNotes = 0;
	result->totalAudioMs = 0;
	result->totalWallSeconds = 0.0;
	result->canceled = FALSE;
	synth = CreateConfiguredSynth(&channels, &bit_depth, &audio_format, &sampleRate);
	if (!synth)
	{
		return FailConversion(result);
	}
	fmtIndex = ClampInt(g_App->batchOutputFormatIndex, 0, BATCH_FORMAT_COUNT - 1);
	bit_depth = s_outputFormats[fmtIndex].bitDepth;
	audio_format = s_outputFormats[fmtIndex].audioFormat;
	buffer = AllocAudioRenderBuffer(channels, bit_depth);
	if (!buffer)
	{
		timid_close(synth);
		return FailConversion(result);
	}
	for (i = 0; i < g_App->fileCount; i++)
	{
		TCHAR outPath[MAX_PATH];
		char ansiIn[MAX_PATH];
		TCHAR title[256], copyright[256];
		int bitrate, durationMs, eventCount;
		if (WaitForSingleObject(g_App->hCancelEvent, 0) == WAIT_OBJECT_0)
		{
			canceled = TRUE;
			break;
		}
		PostProgress(i, g_App->fileCount, 0, g_App->files[i].path);
		TCharToAnsi(g_App->files[i].path, ansiIn, MAX_PATH);
		if (!timid_load_smf(synth, ansiIn))
		{
			result->failed++;
			PostFileResult(i, FILE_RESULT_LOAD_FAILED, NULL, NULL, 0, 0, 0, 0, 0, 0.0);
			continue;
		}
		GetSongMetadata(synth, title, copyright);
		durationMs = timid_get_duration(synth);
		eventCount = timid_get_event_count(synth);
		bitrate = 0;
		if (durationMs > 0)
		{
			bitrate = timid_get_bitrate(synth);
		}
		BuildOutputPath(g_App->files[i].path, g_App->fileSubDirs[i].dir, g_App->outDir, g_App->bPreserveDirStructure, outPath, MAX_PATH);
		if (ShouldPreserveSubDir(g_App->fileSubDirs[i].dir, g_App->outDir, g_App->bPreserveDirStructure))
		{
			TCHAR outSubDir[MAX_PATH];
			SafeFormat(outSubDir, MAX_PATH, _T("%s\\%s"), g_App->outDir, g_App->fileSubDirs[i].dir);
			EnsureDirectorySilent(outSubDir);
		}
		if (!g_App->bOverwrite && PathFileExists(outPath))
		{
			result->skipped++;
			PostFileResult(i, FILE_RESULT_SKIPPED, title, copyright, durationMs, eventCount, bitrate, 0, 0, 0.0);
			timid_unload_smf(synth);
			continue;
		}
		{
			ConversionProgressCtx progressCtx;
			SmfRenderStats stats;
			BOOL fileOk;
			progressCtx.fileIndex = i;
			progressCtx.fileCount = g_App->fileCount;
			progressCtx.fileName = g_App->files[i].path;
			fileOk = RenderSmfFileToWav(synth, audio_format, channels, bit_depth, sampleRate, s_outputFormats[fmtIndex].wavFormatTag, buffer, outPath, g_App->hCancelEvent, ConversionProgressCallback, &progressCtx, &stats);
			timid_unload_smf(synth);
			if (stats.canceled)
			{
				canceled = TRUE;
				break;
			}
			if (!fileOk)
			{
				result->failed++;
				PostFileResult(i, FILE_RESULT_OUTPUT_FAILED, title, copyright, durationMs, eventCount, bitrate, stats.lostNotes, stats.cutNotes, stats.realtimeRate);
			}
			else
			{
				PostProgress(i, g_App->fileCount, 100, g_App->files[i].path);
				result->converted++;
				result->totalLostNotes += stats.lostNotes;
				result->totalCutNotes += stats.cutNotes;
				result->totalAudioMs += stats.audioMs;
				result->totalWallSeconds += stats.timeSpent;
				PostFileResult(i, FILE_RESULT_CONVERTED, title, copyright, durationMs, eventCount, bitrate, stats.lostNotes, stats.cutNotes, stats.realtimeRate);
			}
		}
	}
	free(buffer);
	timid_close(synth);
	result->canceled = canceled;
	PostMessage(g_App->hBatchWnd, WM_CONV_DONE, 0, (LPARAM)result);
	return 0;
}

static void ComputeRelativeSubDir(const TCHAR *root, const TCHAR *fullPath, TCHAR *out, int outSize)
{
	size_t rootLen;
	const TCHAR *rel;
	TCHAR *lastSlash;
	out[0] = _T('\0');
	if (!root || !root[0])
	{
		return;
	}
	rootLen = _tcslen(root);
	if (_tcsnicmp(fullPath, root, rootLen) != 0)
	{
		return;
	}
	rel = fullPath + rootLen;
	if (*rel == _T('\\'))
	{
		rel++;
	}
	SafeFormat(out, outSize, _T("%s"), rel);
	lastSlash = _tcsrchr(out, _T('\\'));
	if (lastSlash)
	{
		*lastSlash = _T('\0');
	}
	else
	{
		out[0] = _T('\0');
	}
}

static void ComputeFullSubDir(const TCHAR *fullPath, TCHAR *out, int outSize)
{
	TCHAR drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
	TCHAR trimmedDir[_MAX_DIR];
	TCHAR *dirStart;
	size_t dirLen;
	_tsplitpath(fullPath, drive, dir, fname, ext);
	out[0] = _T('\0');
	if (drive[0])
	{
		SafeFormat(out, outSize, _T("%c"), drive[0]);
	}
	dirStart = dir;
	while (*dirStart == _T('\\') || *dirStart == _T('/'))
	{
		dirStart++;
	}
	dirLen = _tcslen(dirStart);
	while (dirLen > 0 && (dirStart[dirLen-1] == _T('\\') || dirStart[dirLen-1] == _T('/')))
	{
		dirLen--;
	}
	if (dirLen > 0)
	{
		_tcsncpy(trimmedDir, dirStart, dirLen);
		trimmedDir[dirLen] = _T('\0');
		if (out[0])
		{
			AppendFormat(out, outSize, _T("\\%s"), trimmedDir);
		}
		else
		{
			SafeFormat(out, outSize, _T("%s"), trimmedDir);
		}
	}
}

static BOOL TryAddBatchFileWithSubDir(const TCHAR *path, const TCHAR *subDir)
{
	int i;
	if (g_App->fileCount >= MAX_BATCH_FILES)
	{
		return FALSE;
	}
	for (i = 0; i < g_App->fileCount; i++)
	{
		if (_tcsicmp(g_App->files[i].path, path) == 0)
		{
			return FALSE;
		}
	}
	SafeFormat(g_App->files[g_App->fileCount].path, MAX_PATH, _T("%s"), path);
	if (subDir)
	{
		SafeFormat(g_App->fileSubDirs[g_App->fileCount].dir, MAX_PATH, _T("%s"), subDir);
	}
	else
	{
		g_App->fileSubDirs[g_App->fileCount].dir[0] = _T('\0');
	}
	g_App->fileResultText[g_App->fileCount].text[0] = _T('\0');
	ZeroMemory(&g_App->fileResultData[g_App->fileCount], sizeof(FileResultData));
	g_App->fileCount++;
	return TRUE;
}

static BOOL TryAddBatchFile(const TCHAR *path)
{
	TCHAR subDir[MAX_PATH];
	ComputeFullSubDir(path, subDir, MAX_PATH);
	return TryAddBatchFileWithSubDir(path, subDir);
}

static BOOL TryAddBatchFileEntry(void *ctx, const TCHAR *path)
{
	const TCHAR *rootDir = (const TCHAR *)ctx;
	TCHAR subDir[MAX_PATH];
	if (!rootDir || !rootDir[0])
	{
		return TryAddBatchFile(path);
	}
	ComputeRelativeSubDir(rootDir, path, subDir, MAX_PATH);
	return TryAddBatchFileWithSubDir(path, subDir);
}

static BOOL AppendFile(HWND hWnd, const TCHAR *path, const TCHAR *subDir)
{
	if (TryAddBatchFileWithSubDir(path, subDir))
	{
		HWND hList = GetDlgItem(hWnd, IDC_FILELIST);
		InsertListViewItem(hList, g_App->fileCount - 1, path);
		if (g_App->fileCount == 1)
		{
			ListView_SetItemState(hList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		}
		return TRUE;
	}
	return FALSE;
}

static BOOL ConsumeBoolSwitch(const TCHAR **pp, const TCHAR *switchName, BOOL *flag)
{
	if (_tcsicmp(*pp, switchName) != 0)
	{
		return FALSE;
	}
	*flag = TRUE;
	*pp += _tcslen(*pp) + 1;
	return TRUE;
}

static void PopulateFileList(HWND hWnd);

static void RefreshBatchDialog(HWND hWnd)
{
	SetDlgItemText(hWnd, IDC_OUTDIR, g_App->outDir);
	if (g_App->bOverwrite)
	{
		CheckDlgButton(hWnd, IDC_OVERWRITE, BST_CHECKED);
	}
	if (g_App->bPreserveDirStructure)
	{
		CheckDlgButton(hWnd, IDC_PRESERVEDIRS, BST_CHECKED);
	}
	PopulateFileList(hWnd);
	if (g_App->fileCount > 0)
	{
		HWND hList = GetDlgItem(hWnd, IDC_FILELIST);
		ListView_SetItemState(hList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	}
	SendDlgItemMessage(hWnd, IDC_OUTFORMAT, CB_SETCURSEL, g_App->batchOutputFormatIndex, 0);
}

void OpenCommandLineBatch(HWND hWnd, const TCHAR *cmdPaths)
{
	const TCHAR *p = cmdPaths;
	if (g_App->bConverting)
	{
		ShowAppStringMessage(g_App->hBatchWnd, IDS_BATCHCONVERTERBUSY, MB_ICONWARNING);
		return;
	}
	if (_tcsicmp(p, CMDLINE_BATCH_SWITCH) == 0)
	{
		p += _tcslen(p) + 1;
	}
	while (*p)
	{
		if (_tcsicmp(p, CMDLINE_OUTDIR_SWITCH) == 0)
		{
			p += _tcslen(p) + 1;
			if (*p)
			{
				SafeFormat(g_App->outDir, MAX_PATH, _T("%s"), p);
				p += _tcslen(p) + 1;
			}
			continue;
		}
		if (_tcsicmp(p, CMDLINE_FORMAT_SWITCH) == 0)
		{
			p += _tcslen(p) + 1;
			if (*p)
			{
				g_App->batchOutputFormatIndex = ClampInt(_ttoi(p), 0, BATCH_FORMAT_COUNT - 1);
				g_App->bBatchFormatFromCmdLine = TRUE;
				p += _tcslen(p) + 1;
			}
			continue;
		}
		if (ConsumeBoolSwitch(&p, CMDLINE_OVERWRITE_SWITCH, &g_App->bOverwrite))
		{
			continue;
		}
		if (ConsumeBoolSwitch(&p, CMDLINE_PRESERVEDIRS_SWITCH, &g_App->bPreserveDirStructure))
		{
			continue;
		}
		if (ConsumeBoolSwitch(&p, CMDLINE_AUTOSTART_SWITCH, &g_App->bBatchAutoStart))
		{
			continue;
		}
		if (ConsumeBoolSwitch(&p, CMDLINE_AUTOSAVECSV_SWITCH, &g_App->bBatchAutoSaveCsv))
		{
			continue;
		}
		{
			TCHAR rootDir[MAX_PATH];
			rootDir[0] = _T('\0');
			if (PathIsDirectory(p))
			{
				SafeFormat(rootDir, MAX_PATH, _T("%s"), p);
			}
			AppendPathOrDirectory(p, TryAddBatchFileEntry, rootDir);
		}
		p += _tcslen(p) + 1;
	}
	if (g_App->hBatchWnd)
	{
		RefreshBatchDialog(g_App->hBatchWnd);
	}
	if (!hWnd || IsWindowEnabled(hWnd))
	{
		ShowDialogBox(g_App->hInst, MAKEINTRESOURCE(IDD_BATCH), hWnd, (DLGPROC)BatchDialogProc);
	}
}

void AddPlaylistToBatch(void)
{
	int i;
	for (i = 0; i < g_App->playlistCount; i++)
	{
		TryAddBatchFile(g_App->playlist[i].path);
	}
}

BOOL PlaylistHasNewBatchFiles(void)
{
	int i, j;
	for (i = 0; i < g_App->playlistCount; i++)
	{
		BOOL found = FALSE;
		for (j = 0; j < g_App->fileCount; j++)
		{
			if (_tcsicmp(g_App->files[j].path, g_App->playlist[i].path) == 0)
			{
				found = TRUE;
				break;
			}
		}
		if (!found)
		{
			return TRUE;
		}
	}
	return FALSE;
}

static void AddFileCallback(void *ctx, const TCHAR *path)
{
	TCHAR subDir[MAX_PATH];
	ComputeFullSubDir(path, subDir, MAX_PATH);
	AppendFile((HWND)ctx, path, subDir);
}

static void AddFilesFromBuffer(HWND hWnd, TCHAR *buffer)
{
	ForEachMultiSelectPath(buffer, AddFileCallback, hWnd);
}

static BOOL AddFiles(HWND hWnd)
{
	TCHAR *buffer = PromptOpenMultipleMidiFiles(hWnd);
	if (!buffer)
	{
		return FALSE;
	}
	AddFilesFromBuffer(hWnd, buffer);
	free(buffer);
	return TRUE;
}

typedef struct {
	HWND hWnd;
	TCHAR rootDir[MAX_PATH];
} AddFolderCtx;

static BOOL AddDirectoryFileCallback(void *ctx, const TCHAR *path)
{
	AddFolderCtx *afc = (AddFolderCtx *)ctx;
	TCHAR subDir[MAX_PATH];
	ComputeRelativeSubDir(afc->rootDir, path, subDir, MAX_PATH);
	return AppendFile(afc->hWnd, path, subDir);
}

static BOOL AddFolder(HWND hWnd)
{
	AddFolderCtx ctx;
	ctx.hWnd = hWnd;
	if (!PromptForFolder(hWnd, IDS_ADDFOLDERCAP, ctx.rootDir))
	{
		return FALSE;
	}
	return WalkDirectoryFiles(ctx.rootDir, AddDirectoryFileCallback, &ctx);
}

static BOOL BrowseOutputFolder(HWND hWnd)
{
	TCHAR path[MAX_PATH];
	BOOL result = PromptForFolder(hWnd, IDS_OUTCAP, path);
	if (result)
	{
		SetDlgItemText(hWnd, IDC_OUTDIR, path);
	}
	return result;
}

static BOOL EnsureOutputDirectory(HWND hWnd, const TCHAR *dir)
{
	if (!dir || !dir[0])
	{
		return TRUE;
	}
	if (PathIsDirectory(dir))
	{
		return TRUE;
	}
	if (ShowFormattedAppMessage(hWnd, IDS_CREATEDIR, MB_ICONQUESTION | MB_YESNO, dir) != IDYES)
	{
		return FALSE;
	}
	if (!TryCreateDirectoryPath(hWnd, dir))
	{
		ShowFormattedAppMessage(hWnd, IDS_CREATEDIRFAIL, MB_ICONERROR, dir);
		return FALSE;
	}
	return TRUE;
}

static void PopulateFileList(HWND hWnd)
{
	HWND hList = GetDlgItem(hWnd, IDC_FILELIST);
	int idx;
	ListView_DeleteAllItems(hList);
	for (idx = 0; idx < g_App->fileCount; idx++)
	{
		if (g_App->fileResultText[idx].text[0])
		{
			TCHAR display[MAX_PATH + 600];
			SafeFormat(display, MAX_PATH+600, _T("%s%s"), g_App->files[idx].path, g_App->fileResultText[idx].text);
			InsertListViewItem(hList, idx, display);
		}
		else
		{
			InsertListViewItem(hList, idx, g_App->files[idx].path);
		}
	}
}

static void SwapBatchEntries(int indexA, int indexB)
{
	FileEntry tmp = g_App->files[indexA];
	FileResultText tmpText = g_App->fileResultText[indexA];
	FileResultData tmpData = g_App->fileResultData[indexA];
	FileSubDir tmpSubDir = g_App->fileSubDirs[indexA];
	g_App->files[indexA] = g_App->files[indexB];
	g_App->files[indexB] = tmp;
	g_App->fileResultText[indexA] = g_App->fileResultText[indexB];
	g_App->fileResultText[indexB] = tmpText;
	g_App->fileResultData[indexA] = g_App->fileResultData[indexB];
	g_App->fileResultData[indexB] = tmpData;
	g_App->fileSubDirs[indexA] = g_App->fileSubDirs[indexB];
	g_App->fileSubDirs[indexB] = tmpSubDir;
}

static void MoveSelected(HWND hWnd, BOOL up)
{
	HWND hList = GetDlgItem(hWnd, IDC_FILELIST);
	int count;
	int *items = MoveSelectedListViewItems(hList, g_App->fileCount, up, &count, SwapBatchEntries);
	if (!items)
	{
		return;
	}
	PopulateFileList(hWnd);
	ReselectMovedListViewItems(hList, items, count, g_App->fileCount);
}

static void RemoveSelected(HWND hWnd)
{
	HWND hList = GetDlgItem(hWnd, IDC_FILELIST);
	int count;
	int *items = GetSortedSelectedListViewItems(hList, &count, TRUE);
	int i;
	int anchorIndex;
	if (!items)
	{
		return;
	}
	anchorIndex = items[count-1];
	for (i = 0; i < count; i++)
	{
		int idx = items[i];
		if (idx < 0 || idx >= g_App->fileCount)
		{
			continue;
		}
		MoveMemory(&g_App->files[idx], &g_App->files[idx+1], sizeof(FileEntry) * (g_App->fileCount-idx-1));
		MoveMemory(&g_App->fileResultText[idx], &g_App->fileResultText[idx+1], sizeof(FileResultText) * (g_App->fileCount-idx-1));
		MoveMemory(&g_App->fileResultData[idx], &g_App->fileResultData[idx+1], sizeof(FileResultData) * (g_App->fileCount-idx-1));
		MoveMemory(&g_App->fileSubDirs[idx], &g_App->fileSubDirs[idx+1], sizeof(FileSubDir) * (g_App->fileCount-idx-1));
		g_App->fileCount--;
		ListView_DeleteItem(hList, idx);
	}
	free(items);
	SelectClampedListViewIndex(hList, anchorIndex, g_App->fileCount);
}

static void SetControlsEnabled(HWND hWnd, BOOL enabled)
{
	EnableWindow(GetDlgItem(hWnd, IDC_FILELIST), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_ADDFILES), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_ADDFOLDER), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_REMOVE), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_CLEARALL), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_MOVEUP), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_MOVEDOWN), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_OUTDIR), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_OUTDIRB), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_OUTDIRDEFAULT), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_OVERWRITE), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_PRESERVEDIRS), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_OUTFORMAT), enabled);
	EnableWindow(GetDlgItem(hWnd, IDC_SAVECSV), enabled);
	EnableWindow(GetDlgItem(hWnd, IDCANCEL), enabled);
}

static void SetConvertButtonLabel(HWND hWnd, UINT stringId)
{
	TCHAR text[MAX_PATH];
	LoadAppString(g_App->hInst, stringId, text, MAX_PATH);
	SetDlgItemText(hWnd, IDC_CONVERT, text);
}

static void ResetBatchFileResults(void)
{
	ZeroMemory(g_App->fileResultText, sizeof(FileResultText) * MAX_BATCH_FILES);
	ZeroMemory(g_App->fileResultData, sizeof(FileResultData) * MAX_BATCH_FILES);
}

static void StartConversion(HWND hWnd)
{
	DWORD threadId;
	if (!g_App->hCancelEvent)
	{
		ShowAppStringMessage(hWnd, IDS_BATCHINITFAILED, MB_ICONERROR);
		return;
	}
	if (g_App->bQuickConverting)
	{
		ShowAppStringMessage(hWnd, IDS_QUICKCONVERTBUSY, MB_ICONWARNING);
		return;
	}
	if (g_App->fileCount == 0)
	{
		ShowAppStringMessage(hWnd, IDS_NOFILES, MB_ICONWARNING);
		return;
	}
	GetDlgItemText(hWnd, IDC_OUTDIR, g_App->outDir, MAX_PATH);
	if (!EnsureOutputDirectory(hWnd, g_App->outDir))
	{
		return;
	}
	g_App->bOverwrite = IsDlgButtonChecked(hWnd, IDC_OVERWRITE) == BST_CHECKED;
	g_App->bPreserveDirStructure = IsDlgButtonChecked(hWnd, IDC_PRESERVEDIRS) == BST_CHECKED;
	g_App->batchOutputFormatIndex = (int)SendDlgItemMessage(hWnd, IDC_OUTFORMAT, CB_GETCURSEL, 0, 0);
	if (g_App->batchOutputFormatIndex < 0)
	{
		g_App->batchOutputFormatIndex = BATCH_FORMAT_DEFAULT;
	}
	SaveDefaultOutputFormat(g_App->batchOutputFormatIndex);
	ResetBatchFileResults();
	PopulateFileList(hWnd);
	if (g_App->fileCount > 0)
	{
		HWND hList = GetDlgItem(hWnd, IDC_FILELIST);
		ListView_SetItemState(hList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	}
	ResetEvent(g_App->hCancelEvent);
	SetFocus(GetDlgItem(hWnd, IDC_CONVERT));
	SetControlsEnabled(hWnd, FALSE);
	SetConvertButtonLabel(hWnd, IDS_CANCELBTN);
	SendDlgItemMessage(hWnd, IDC_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
	SendDlgItemMessage(hWnd, IDC_PROGRESS, PBM_SETPOS, 0, 0);
	SetDlgItemText(hWnd, IDC_STATUS, _T(""));
	g_App->bConverting = TRUE;
	g_App->hThread = CreateThread(NULL, 0, ConversionThreadProc, NULL, 0, &threadId);
	if (!g_App->hThread)
	{
		SetControlsEnabled(hWnd, TRUE);
		SetConvertButtonLabel(hWnd, IDS_CONVERTBTN);
		g_App->bConverting = FALSE;
	}
}

static void WriteCsvField(FILE *f, const TCHAR *value, BOOL first)
{
	BOOL needsQuotes = FALSE;
	const TCHAR *p;
	if (!first)
	{
		_fputtc(_T(','), f);
	}
	for (p = value; *p; p++)
	{
		if (*p == _T(',') || *p == _T('"') || *p == _T('\n') || *p == _T('\r'))
		{
			needsQuotes = TRUE;
			break;
		}
	}
	if (!needsQuotes)
	{
		_fputts(value, f);
		return;
	}
	_fputtc(_T('"'), f);
	for (p = value; *p; p++)
	{
		if (*p == _T('"'))
		{
			_fputtc(_T('"'), f);
		}
		_fputtc(*p, f);
	}
	_fputtc(_T('"'), f);
}

static void WriteCsvFieldOrEmpty(FILE *f, BOOL hasValue, const TCHAR *value)
{
	if (hasValue)
	{
		WriteCsvField(f, value, FALSE);
	}
	else
	{
		WriteCsvField(f, _T(""), FALSE);
	}
}

static void WriteCsvIntField(FILE *f, BOOL condition, int value)
{
	TCHAR num[32];
	num[0] = _T('\0');
	if (condition)
	{
		SafeFormat(num, 32, _T("%d"), value);
	}
	WriteCsvField(f, num, FALSE);
}

static void WriteCsvRateField(FILE *f, BOOL condition, double value)
{
	TCHAR num[32];
	num[0] = _T('\0');
	if (condition)
	{
		SafeFormat(num, 32, _T("%.2f"), value);
	}
	WriteCsvField(f, num, FALSE);
}

static BOOL WriteResultsCsv(const TCHAR *path)
{
	static const UINT outcomeStringIds[] = { IDS_OUTCOME_CONVERTED, IDS_OUTCOME_LOADFAILED, IDS_OUTCOME_WRITEFAILED, IDS_OUTCOME_SKIPPED };
	FILE *f;
	TCHAR header[MAX_PATH];
	int i;
	f = _tfopen(path, _T("w"));
	if (!f)
	{
		return FALSE;
	}
	LoadAppString(g_App->hInst, IDS_CSVHEADER, header, MAX_PATH);
	_fputts(header, f);
	_fputts(_T("\n"), f);
	for (i = 0; i < g_App->fileCount; i++)
	{
		FileResultData *data = &g_App->fileResultData[i];
		BOOL haveDetails = data->hasResult && data->outcome != FILE_RESULT_LOAD_FAILED && data->outcome != FILE_RESULT_SKIPPED;
		BOOL haveSongInfo = data->hasResult && data->outcome != FILE_RESULT_LOAD_FAILED;
		TCHAR outcomeText[64];
		TCHAR durText[32];
		if (data->hasResult && data->outcome >= 0 && data->outcome < (int)(sizeof(outcomeStringIds) / sizeof(outcomeStringIds[0])))
		{
			LoadAppString(g_App->hInst, outcomeStringIds[data->outcome], outcomeText, 64);
		}
		else
		{
			LoadAppString(g_App->hInst, IDS_OUTCOME_PENDING, outcomeText, 64);
		}
		WriteCsvField(f, g_App->files[i].path, TRUE);
		WriteCsvField(f, outcomeText, FALSE);
		WriteCsvFieldOrEmpty(f, data->hasResult, data->title);
		WriteCsvFieldOrEmpty(f, data->hasResult, data->copyright);
		durText[0] = _T('\0');
		if (haveSongInfo)
		{
			FormatTimeHMS(data->durationMs, durText, 32);
		}
		WriteCsvField(f, durText, FALSE);
		WriteCsvIntField(f, haveSongInfo, data->eventCount);
		WriteCsvIntField(f, haveSongInfo, data->bitrate);
		WriteCsvIntField(f, haveDetails, data->lostNotes);
		WriteCsvIntField(f, haveDetails, data->cutNotes);
		WriteCsvRateField(f, haveDetails && data->realtimeRate > 0.0, data->realtimeRate);
		_fputts(_T("\n"), f);
	}
	fclose(f);
	return TRUE;
}

static void SaveResultsToCsv(HWND hWnd)
{
	TCHAR path[MAX_PATH];
	TCHAR msg[MAX_PATH];
	ZeroMemory(path, sizeof(path));
	if (!PromptFileName(hWnd, TRUE, path, MAX_PATH, IDS_CSVFLT, IDS_CSVCAP, _T("csv"), NULL, OFN_OVERWRITEPROMPT))
	{
		return;
	}
	if (!WriteResultsCsv(path))
	{
		ShowStringErrorWithCaption(hWnd, IDS_CSVSAVEFAILED, IDS_CSVCAP);
		return;
	}
	LoadAppString(g_App->hInst, IDS_CSVSAVED, msg, MAX_PATH);
	SetDlgItemText(hWnd, IDC_STATUS, msg);
}

static void AutoSaveConversionCsv(void)
{
	SYSTEMTIME st;
	TCHAR dir[MAX_PATH];
	TCHAR path[MAX_PATH];
	GetLocalTime(&st);
	if (g_App->outDir[0])
	{
		SafeFormat(dir, MAX_PATH, _T("%s"), g_App->outDir);
	}
	else
	{
		GetCurrentDirectory(MAX_PATH, dir);
	}
	SafeFormat(path, MAX_PATH, _T("%s\\conversion_%04d%02d%02d_%02d%02d%02d.csv"), dir, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
	WriteResultsCsv(path);
}

static void HandleProgress(HWND hWnd, ProgressInfo *info)
{
	TCHAR status[MAX_PATH+64];
	TCHAR fmt[64];
	const TCHAR *name = GetBaseName(info->fileName);
	int overall;
	LoadAppString(g_App->hInst, IDS_CONVERTINGSTATUSFMT, fmt, 64);
	SafeFormat(status, MAX_PATH+64, fmt, info->fileIndex+1, info->fileCount, name, info->percent);
	SetDlgItemText(hWnd, IDC_STATUS, status);
	overall = (int)(((double)info->fileIndex + (info->percent / 100.0)) / (double)info->fileCount * 100.0);
	SendDlgItemMessage(hWnd, IDC_PROGRESS, PBM_SETPOS, overall, 0);
	free(info);
}

static void HandleFileResult(HWND hWnd, FileResultInfo *info)
{
	HWND hList = GetDlgItem(hWnd, IDC_FILELIST);
	TCHAR display[MAX_PATH + 600];
	TCHAR extra[600];
	TCHAR fmt[64];
	if (info->fileIndex < 0 || info->fileIndex >= g_App->fileCount)
	{
		free(info);
		return;
	}
	extra[0] = _T('\0');
	switch (info->outcome)
	{
	case FILE_RESULT_LOAD_FAILED:
		LoadAppString(g_App->hInst, IDS_RESULT_LOADFAILED, fmt, 64);
		AppendFormat(extra, 600, fmt);
		break;
	case FILE_RESULT_OUTPUT_FAILED:
		LoadAppString(g_App->hInst, IDS_RESULT_WRITEFAILED, fmt, 64);
		AppendFormat(extra, 600, fmt, info->lostNotes, info->cutNotes);
		break;
	case FILE_RESULT_SKIPPED:
		LoadAppString(g_App->hInst, IDS_RESULT_SKIPPED, fmt, 64);
		AppendFormat(extra, 600, fmt);
		break;
	default:
		LoadAppString(g_App->hInst, IDS_RESULT_LOSTCUT, fmt, 64);
		AppendFormat(extra, 600, fmt, info->lostNotes, info->cutNotes);
		break;
	}
	if (info->outcome != FILE_RESULT_LOAD_FAILED && info->outcome != FILE_RESULT_SKIPPED && info->realtimeRate > 0.0)
	{
		LoadAppString(g_App->hInst, IDS_RESULT_SPEED, fmt, 64);
		AppendFormat(extra, 600, fmt, info->realtimeRate);
	}
	if (info->title[0])
	{
		LoadAppString(g_App->hInst, IDS_RESULT_TITLE, fmt, 64);
		AppendFormat(extra, 600, fmt, info->title);
	}
	if (info->copyright[0])
	{
		LoadAppString(g_App->hInst, IDS_RESULT_COPYRIGHT, fmt, 64);
		AppendFormat(extra, 600, fmt, info->copyright);
	}
	if (info->outcome != FILE_RESULT_LOAD_FAILED)
	{
		TCHAR durBuf[32];
		FormatTimeHMS(info->durationMs, durBuf, 32);
		LoadAppString(g_App->hInst, IDS_RESULT_DURATION, fmt, 64);
		AppendFormat(extra, 600, fmt, durBuf);
		LoadAppString(g_App->hInst, IDS_RESULT_EVENTS, fmt, 64);
		AppendFormat(extra, 600, fmt, info->eventCount);
		LoadAppString(g_App->hInst, IDS_RESULT_BITRATE, fmt, 64);
		AppendFormat(extra, 600, fmt, info->bitrate);
	}
	SafeFormat(g_App->fileResultText[info->fileIndex].text, 600, _T("%s"), extra);
	SafeFormat(display, MAX_PATH+600, _T("%s%s"), g_App->files[info->fileIndex].path, extra);
	ListView_SetItemText(hList, info->fileIndex, 0, display);
	g_App->fileResultData[info->fileIndex].hasResult = TRUE;
	g_App->fileResultData[info->fileIndex].outcome = info->outcome;
	SafeFormat(g_App->fileResultData[info->fileIndex].title, 256, _T("%s"), info->title);
	SafeFormat(g_App->fileResultData[info->fileIndex].copyright, 256, _T("%s"), info->copyright);
	g_App->fileResultData[info->fileIndex].durationMs = info->durationMs;
	g_App->fileResultData[info->fileIndex].eventCount = info->eventCount;
	g_App->fileResultData[info->fileIndex].bitrate = info->bitrate;
	g_App->fileResultData[info->fileIndex].lostNotes = info->lostNotes;
	g_App->fileResultData[info->fileIndex].cutNotes = info->cutNotes;
	g_App->fileResultData[info->fileIndex].realtimeRate = info->realtimeRate;
	free(info);
}

static void HandleDone(HWND hWnd, ConvResult *result)
{
	int total;
	double avgRate;
	CloseHandleAndClear(&g_App->hThread);
	g_App->bConverting = FALSE;
	SetControlsEnabled(hWnd, TRUE);
	SetConvertButtonLabel(hWnd, IDS_CONVERTBTN);
	SendDlgItemMessage(hWnd, IDC_PROGRESS, PBM_SETPOS, 0, 0);
	SetDlgItemText(hWnd, IDC_STATUS, _T(""));
	if (!result)
	{
		return;
	}
	total = result->converted + result->failed + result->skipped;
	if (result->totalWallSeconds > 0.0)
	{
		avgRate = ((double)result->totalAudioMs / 1000.0) / result->totalWallSeconds;
	}
	else
	{
		avgRate = 0.0;
	}
	if (!result->canceled)
	{
		ShowFormattedAppMessage(hWnd, IDS_SUMMARY, MB_ICONINFORMATION, result->converted, total, result->failed, result->skipped, result->totalLostNotes, result->totalCutNotes, avgRate);
	}
	if (g_App->bBatchAutoSaveCsv)
	{
		g_App->bBatchAutoSaveCsv = FALSE;
		AutoSaveConversionCsv();
	}
	free(result);
}

BOOL WINAPI BatchDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		g_App->hBatchWnd = hWnd;
		g_App->hCancelEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		SendDlgItemMessage(hWnd, IDC_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
		if (!g_App->outDir[0])
		{
			LoadDefaultOutputDir(g_App->outDir, MAX_PATH);
		}
		InitSingleColumnListView(GetDlgItem(hWnd, IDC_FILELIST));
		{
			unsigned int fi;
			TCHAR label[32];
			for (fi = 0; fi < BATCH_FORMAT_COUNT; fi++)
			{
				LoadAppString(g_App->hInst, s_outputFormats[fi].labelId, label, 32);
				SendDlgItemMessage(hWnd, IDC_OUTFORMAT, CB_ADDSTRING, 0, (LPARAM)label);
			}
			if (!g_App->bBatchFormatFromCmdLine)
			{
				g_App->batchOutputFormatIndex = ClampInt(LoadDefaultOutputFormat(BATCH_FORMAT_DEFAULT), 0, BATCH_FORMAT_COUNT - 1);
			}
			g_App->bBatchFormatFromCmdLine = FALSE;
		}
		RefreshBatchDialog(hWnd);
		if (g_App->bBatchAutoStart)
		{
			g_App->bBatchAutoStart = FALSE;
			StartConversion(hWnd);
		}
		return TRUE;
	case WM_CONV_PROGRESS:
		HandleProgress(hWnd, (ProgressInfo *)lParam);
		return TRUE;
	case WM_CONV_FILERESULT:
		HandleFileResult(hWnd, (FileResultInfo *)lParam);
		return TRUE;
	case WM_CONV_DONE:
		HandleDone(hWnd, (ConvResult *)lParam);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_ADDFILES:
			AddFiles(hWnd);
			return TRUE;
		case IDC_ADDFOLDER:
			AddFolder(hWnd);
			return TRUE;
		case IDC_REMOVE:
			RemoveSelected(hWnd);
			return TRUE;
		case IDC_CLEARALL:
			ListView_DeleteAllItems(GetDlgItem(hWnd, IDC_FILELIST));
			g_App->fileCount = 0;
			ResetBatchFileResults();
			return TRUE;
		case IDC_MOVEUP:
			MoveSelected(hWnd, TRUE);
			return TRUE;
		case IDC_MOVEDOWN:
			MoveSelected(hWnd, FALSE);
			return TRUE;
		case IDC_OUTDIRB:
			BrowseOutputFolder(hWnd);
			return TRUE;
		case IDC_OUTDIRDEFAULT:
			{
				TCHAR dir[MAX_PATH];
				TCHAR savedMsg[MAX_PATH];
				GetDlgItemText(hWnd, IDC_OUTDIR, dir, MAX_PATH);
				SaveDefaultOutputDir(dir);
				LoadAppString(g_App->hInst, IDS_OUTDIRSAVED, savedMsg, MAX_PATH);
				SetDlgItemText(hWnd, IDC_STATUS, savedMsg);
			}
			return TRUE;
		case IDC_CONVERT:
			if (g_App->bConverting)
			{
				SetEvent(g_App->hCancelEvent);
			}
			else
			{
				StartConversion(hWnd);
			}
			return TRUE;
		case IDC_SAVECSV:
			SaveResultsToCsv(hWnd);
			return TRUE;
		case IDCANCEL:
			if (!g_App->bConverting)
			{
				EndDialog(hWnd, TRUE);
			}
			return TRUE;
		}
		break;
	case WM_CLOSE:
		if (!g_App->bConverting)
		{
			EndDialog(hWnd, TRUE);
		}
		return TRUE;
	case WM_DESTROY:
		CloseHandleAndClear(&g_App->hCancelEvent);
		g_App->hBatchWnd = NULL;
		g_App->bBatchFormatFromCmdLine = FALSE;
		g_App->bBatchAutoStart = FALSE;
		g_App->bBatchAutoSaveCsv = FALSE;
		return TRUE;
	}
	return FALSE;
}

static void QuickConvertProgressCallback(void *ctx, int percent)
{
	(void)ctx;
	PostMessage(g_App->hPlayerWnd, WM_QUICKCONVERT_PROGRESS, (WPARAM)percent, 0);
}

static int FailQuickConvert(QuickConvertResult *result, UINT msgId)
{
	LoadAppString(g_App->hInst, msgId, result->errorMsg, 256);
	PostMessage(g_App->hPlayerWnd, WM_QUICKCONVERT_DONE, 0, (LPARAM)result);
	return 1;
}

static DWORD WINAPI QuickConvertThreadProc(LPVOID param)
{
	Timid *synth;
	unsigned char *buffer;
	SmfRenderStats stats;
	int channels, bit_depth, audio_format, sampleRate;
	char ansiIn[MAX_PATH];
	QuickConvertResult *result;
	BOOL ok;
	(void)param;
	result = (QuickConvertResult *)malloc(sizeof(QuickConvertResult));
	if (!result)
	{
		PostMessage(g_App->hPlayerWnd, WM_QUICKCONVERT_DONE, 0, (LPARAM)NULL);
		return 1;
	}
	result->success = FALSE;
	result->errorMsg[0] = _T('\0');
	synth = CreateConfiguredSynth(&channels, &bit_depth, &audio_format, &sampleRate);
	if (!synth)
	{
		return FailQuickConvert(result, IDS_QC_SYNTHFAILED);
	}
	TCharToAnsi(g_App->quickConvertInPath, ansiIn, MAX_PATH);
	if (!timid_load_smf(synth, ansiIn))
	{
		timid_close(synth);
		return FailQuickConvert(result, IDS_QC_LOADFAILED);
	}
	buffer = AllocAudioRenderBuffer(channels, bit_depth);
	if (!buffer)
	{
		timid_unload_smf(synth);
		timid_close(synth);
		return FailQuickConvert(result, IDS_QC_OUTOFMEMORY);
	}
	ok = RenderSmfFileToWav(synth, audio_format, channels, bit_depth, sampleRate, 1, buffer, g_App->quickConvertOutPath, NULL, QuickConvertProgressCallback, NULL, &stats);
	free(buffer);
	timid_unload_smf(synth);
	timid_close(synth);
	if (!ok)
	{
		if (stats.createFailed)
		{
			LoadAppString(g_App->hInst, IDS_QC_CREATEFAILED, result->errorMsg, 256);
		}
		else
		{
			LoadAppString(g_App->hInst, IDS_QC_WRITEFAILED, result->errorMsg, 256);
		}
	}
	else
	{
		result->success = TRUE;
	}
	PostMessage(g_App->hPlayerWnd, WM_QUICKCONVERT_DONE, 0, (LPARAM)result);
	return 0;
}

void ConvertCurrentToWav(HWND hWnd)
{
	TCHAR outPath[MAX_PATH];
	TCHAR initialDir[MAX_PATH];
	TCHAR drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
	TCHAR fmt[MAX_PATH];
	TCHAR text[MAX_PATH];
	DWORD threadId;
	if (!g_App->bFileLoaded || !g_App->loadedFilePath[0] || g_App->bQuickConverting)
	{
		return;
	}
	if (g_App->bConverting)
	{
		ShowAppStringMessage(hWnd, IDS_CONVERTCURRENTBUSY, MB_ICONWARNING);
		return;
	}
	_tsplitpath(g_App->loadedFilePath, drive, dir, fname, ext);
	SafeFormat(initialDir, MAX_PATH, _T("%s%s"), drive, dir);
	SafeFormat(outPath, MAX_PATH, _T("%s.wav"), fname);
	if (!PromptFileName(hWnd, TRUE, outPath, MAX_PATH, IDS_WAVFLT, IDS_WAVCAP, _T("wav"), initialDir, OFN_OVERWRITEPROMPT))
	{
		return;
	}
	SafeFormat(g_App->quickConvertInPath, MAX_PATH, _T("%s"), g_App->loadedFilePath);
	SafeFormat(g_App->quickConvertOutPath, MAX_PATH, _T("%s"), outPath);
	g_App->bQuickConverting = TRUE;
	RefreshPlayerUI(hWnd);
	LoadAppString(g_App->hInst, IDS_CONVERTINGWAVFMT, fmt, MAX_PATH);
	SafeFormat(text, MAX_PATH, fmt, 0);
	SendMessage(g_App->hStatusBar, SB_SETTEXT, 0, (LPARAM)text);
	g_App->hQuickConvertThread = CreateThread(NULL, 0, QuickConvertThreadProc, NULL, 0, &threadId);
	if (!g_App->hQuickConvertThread)
	{
		g_App->bQuickConverting = FALSE;
		RefreshPlayerUI(hWnd);
	}
}

void ShowQuickConvertResult(HWND hWnd, const QuickConvertResult *result)
{
	if (result->success)
	{
		ShowFormattedAppMessage(hWnd, IDS_QUICKCONVERTDONE, MB_ICONINFORMATION, g_App->quickConvertOutPath);
	}
	else
	{
		ShowMessageBox(hWnd, result->errorMsg, g_App->appTitle, MB_ICONERROR);
	}
}
