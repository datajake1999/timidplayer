#ifndef TIMIDPLAYER_H
#define TIMIDPLAYER_H

#include "targetver.h"
#include "resource.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <limits.h>
#include <mmsystem.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <time.h>
#include <windowsx.h>

#include "../timidity/timid.h"
#include "../timidity/config.h"
#include "../common/registry.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BATCH_FILES 1024
#define MAX_PLAYLIST 1024
#define MAX_RECENT_FILES 8
#define SEEK_STEP_MS 10000
#define PLAYER_DEFAULT_CHUNK_MS 10
#define PLAYER_MIN_CHUNK_MS 1
#define PLAYER_MAX_CHUNK_MS 100
#define PLAYER_DEFAULT_BUFFER_MS 100
#define PLAYER_MIN_BUFFER_MS 20
#define PLAYER_MAX_BUFFER_MS 1000
#define MAX_PLAYER_CHUNKS ((PLAYER_MAX_BUFFER_MS + PLAYER_MIN_CHUNK_MS - 1) / PLAYER_MIN_CHUNK_MS)

#define CMDLINE_ENQUEUE_SWITCH _T("/add")
#define CMDLINE_BATCH_SWITCH _T("/c")
#define CMDLINE_OUTDIR_SWITCH _T("/o")
#define CMDLINE_FORMAT_SWITCH _T("/f")
#define CMDLINE_OVERWRITE_SWITCH _T("/w")
#define CMDLINE_PRESERVEDIRS_SWITCH _T("/p")
#define CMDLINE_AUTOSTART_SWITCH _T("/a")
#define CMDLINE_AUTOSAVECSV_SWITCH _T("/s")

#define TIMER_ID_SLEEP 1

#define WM_PLAYBACK_ENDED (WM_APP+1)
#define WM_QUICKCONVERT_PROGRESS (WM_APP+2)
#define WM_QUICKCONVERT_DONE (WM_APP+3)

#define COPYDATA_OPENPATH 0x54494D49

#define PLAYER_STOPPED 0
#define PLAYER_PLAYING 1
#define PLAYER_PAUSED 2

#define REPEAT_OFF 0
#define REPEAT_ONE 1
#define REPEAT_ALL 2

typedef enum {
	MIDIIN_RESET_ALLNOTESOFF,
	MIDIIN_RESET_ALLSOUNDSOFF,
	MIDIIN_RESET_CONTROLLERS,
	MIDIIN_RESET_PANIC,
	MIDIIN_RESET_FULL
} MidiInResetType;

typedef struct {
	unsigned long msg;
	DWORD frame;
} MidiInQueueEntry;

#define MIDIIN_QUEUE_SIZE 1024
#define MIDI_SYSEX_BUFFER_SIZE 256
#define CHANMIX_SLIDER_COUNT 8

typedef struct {
	TCHAR path[MAX_PATH];
} FileEntry;

typedef struct {
	TCHAR text[600];
} FileResultText;

typedef struct {
	TCHAR dir[MAX_PATH];
} FileSubDir;

typedef struct {
	BOOL hasResult;
	int outcome;
	TCHAR title[256];
	TCHAR copyright[256];
	int durationMs;
	int eventCount;
	int bitrate;
	int lostNotes;
	int cutNotes;
	double realtimeRate;
} FileResultData;

typedef struct {
	BOOL success;
	TCHAR errorMsg[256];
} QuickConvertResult;

typedef struct {
	HINSTANCE hInst;
	TCHAR appTitle[MAX_PATH];
	HWND hBatchWnd;
	FileEntry *files;
	FileResultText *fileResultText;
	FileResultData *fileResultData;
	FileSubDir *fileSubDirs;
	int fileCount;
	HANDLE hThread;
	HANDLE hCancelEvent;
	BOOL bConverting;
	TCHAR outDir[MAX_PATH];
	BOOL bOverwrite;
	BOOL bPreserveDirStructure;
	int batchOutputFormatIndex;
	BOOL bBatchFormatFromCmdLine;
	BOOL bBatchAutoStart;
	BOOL bBatchAutoSaveCsv;
	HWND hPlayerWnd;
	HWND hStatusBar;
	HWND hSeekBar;
	BOOL bSeekBarDragging;
	HWND hPlaylistView;
	HWND hPlaylistLabel;
	HWND hLastFocus;
	CRITICAL_SECTION synthCS;
	Timid *synth;
	int channels, bitDepth, audioFormat, sampleRate, chunkFrames;
	int outputDeviceId;
	int bufferMs;
	int chunkMs;
	int numChunks;
	BOOL bInterceptVolumeKeys;
	BOOL bFileLoaded;
	TCHAR loadedFilePath[MAX_PATH];
	TCHAR title[256];
	TCHAR copyright[256];
	int state;
	BOOL bConfigDirty;
	DriverConfig configCfg;
	LARGE_INTEGER cpuLoadLastStart;
	LARGE_INTEGER cpuLoadLastEnd;
	LONGLONG cpuLoadLastSamples;
	FileEntry *playlist;
	int playlistCount;
	int playlistIndex;
	int repeatMode;
	DWORD lastPlaylistFingerprint;
	HWND hPlaylistMgrWnd;
	BOOL bPlaylistMgrEdited;
	int plMgrFilterMap[MAX_PLAYLIST];
	int plMgrFilterCount;
	BOOL bLoadPreviousPlaylist;
	BOOL bLoadFilesIntoRunningInstance;
	BOOL bShowFilenameInTitle;
	BOOL bPreventDuplicatePlaylistItems;
	BOOL bSaveTrack;
	BOOL bSavePosition;
	BOOL bAutoStartPlayback;
	BOOL bRemoveOnUnload;
	BOOL bConfirmPlaylistDelete;
	BOOL bPreventSeekTrackChange;
	BOOL bAutoscrollPlaylist;
	BOOL bRunFromTray;
	BOOL bTrayIconActive;
	BOOL bExitRequested;
	BOOL bAlwaysOnTop;
	BOOL bShowStatusBar;
	BOOL bShowSeekBar;
	BOOL bShowPlaylistView;
	BOOL bSettingsRestored;
	BOOL bPlaylistSavedManually;
	TCHAR recentFiles[MAX_RECENT_FILES][MAX_PATH];
	int recentFileCount;
	HMENU hRecentMenu;
	int sleepTimerMinutes;
	BOOL bSleepTimerRunning;
	DWORD sleepTimerEndTick;
	DWORD sleepTimerRemainingMs;
	int volume;
	HWAVEOUT hWaveOut;
	HANDLE hWaveEvent;
	WAVEHDR *waveHdr;
	BYTE *waveBuffer;
	HANDLE hPlaybackThread;
	volatile BOOL bStopRequested;
	volatile BOOL bPaused;
	DWORD playbackGeneration;
	BOOL bQuickConverting;
	HANDLE hQuickConvertThread;
	TCHAR quickConvertInPath[MAX_PATH];
	TCHAR quickConvertOutPath[MAX_PATH];
	QuickConvertResult *pDeferredQuickConvertResult;
	int jumpTargetMs;
	int pendingSeekMs;
	BOOL bMidiInEnabled;
	int midiInDeviceId;
	int midiInChannelMap[16];
	HMIDIIN hMidiIn;
	BOOL bMidiInActive;
	MIDIHDR midiInSysexHdr;
	BYTE midiInSysexBuffer[MIDI_SYSEX_BUFFER_SIZE];
	MidiInQueueEntry midiInQueue[MIDIIN_QUEUE_SIZE];
	int midiInQueueHead;
	int midiInQueueTail;
	DWORD midiInFramesWritten;
	DWORD midiInPlayPosPrev;
	DWORD midiInPlayPosWraps;
	WNDPROC vkbdDefButtonProc;
	BOOL vkbdNoteDown[128];
	int vkbdActiveChannel;
	int vkbdVelocity;
	int vkbdPressure;
	int vkbdPressureNote;
	int vkbdBaseNote;
	int chanMixActiveChannel;
	BOOL chanMixIsDrum;
	BOOL chanMixInstrComboOpen;
	BOOL chanMixBankComboOpen;
	BOOL chanMixDragging[CHANMIX_SLIDER_COUNT];
} AppState;

extern AppState *g_App;

void CloseHandleAndClear(HANDLE *phHandle);
int ClampInt(int value, int minValue, int maxValue);
void ClampDlgItemInt(HWND hWnd, int id, int minValue, int maxValue);
void SetDlgItemIntText(HWND hWnd, int id, int value);
void TCharToAnsi(const TCHAR *src, char *dst, int dstSize);
void AnsiToTChar(const char *src, TCHAR *dst, int dstSize);
BOOL AppendBoundedToken(TCHAR *buf, int bufChars, int *pUsed, const TCHAR *text);
void AppendFormat(TCHAR *buf, int bufSize, const TCHAR *fmt, ...);
void SafeFormat(TCHAR *buf, int bufSize, const TCHAR *fmt, ...);
void FormatTime(int ms, TCHAR *buf, int bufSize);
void FormatTimeHMS(int ms, TCHAR *buf, int bufSize);
const TCHAR *GetBaseName(const TCHAR *path);
void GetDirectoryPart(const TCHAR *path, TCHAR *dirOut, int dirOutChars);
void GetDirectoryWithSlash(const TCHAR *path, TCHAR *dirOut, int dirOutChars);
void SetAppDirectory(void);
typedef void (*MultiSelectPathFn)(void *ctx, const TCHAR *path);
void ForEachMultiSelectPath(TCHAR *buffer, MultiSelectPathFn addFn, void *ctx);
typedef BOOL (*DirectoryFileFn)(void *ctx, const TCHAR *path);
BOOL WalkDirectoryFiles(const TCHAR *dir, DirectoryFileFn fn, void *ctx);
BOOL AppendPathOrDirectory(const TCHAR *path, DirectoryFileFn fn, void *ctx);
BOOL PromptFileName(HWND hWndOwner, BOOL bSave, TCHAR *path, int pathChars, UINT filterStringId, UINT captionStringId, const TCHAR *defExt, const TCHAR *initialDir, DWORD extraFlags);
TCHAR *PromptOpenMultipleMidiFiles(HWND hWndOwner);
BOOL PromptForFolder(HWND hWndOwner, UINT titleStringId, TCHAR *pathOut);
void GetSongMetadata(Timid *synth, TCHAR *titleOut, TCHAR *copyrightOut);
void InitDriverConfigDefaults(DriverConfig *cfg);
void LoadAppString(HINSTANCE hInst, UINT id, TCHAR *buf, int bufChars);
int ShowMessageBox(HWND hWndOwner, const TCHAR *text, const TCHAR *caption, UINT type);
void ShowAppStringMessage(HWND hWnd, UINT msgId, UINT iconFlags);
int ShowFormattedAppMessage(HWND hWnd, UINT fmtId, UINT iconFlags, ...);
BOOL ConfirmAppString(HWND hWnd, UINT msgId);
void ShowStringErrorWithCaption(HWND hWnd, UINT msgId, UINT capId);
INT_PTR ShowDialogBox(HINSTANCE hInstance, LPCTSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc);
typedef BOOL (*DeviceNameFn)(UINT index, TCHAR *nameOut, int nameOutChars);
void PopulateDeviceCombo(HWND hCombo, UINT numDevs, DeviceNameFn getName, UINT defaultLabelId, int currentDeviceId);
int ComboSelToDeviceId(int sel, int mapperValue);
void PopulateChannelCombo(HWND hCombo);
void InitSingleColumnListView(HWND hList);
void InsertListViewItem(HWND hList, int index, const TCHAR *text);
void SetListViewCurSel(HWND hList, int index);
int GetListViewCurSel(HWND hList);
void SelectClampedListViewIndex(HWND hList, int anchorIndex, int newCount);
int *GetSortedSelectedListViewItems(HWND hList, int *pCount, BOOL descending);
int *MoveSelectedListViewItems(HWND hList, int itemCount, BOOL up, int *pCount, void (*swapFn)(int indexA, int indexB));
void ReselectMovedListViewItems(HWND hList, int *items, int count, int maxCount);

Timid *CreateConfiguredSynth(int *outChannels, int *outBitDepth, int *outAudioFormat, int *outSampleRate);
double CalculateCpuLoad(LARGE_INTEGER start, LARGE_INTEGER end, LONGLONG numSamples);
DWORD GetMidiInPlaybackFrame(void);
void ChangeVolume(int delta);
BOOL IsValidWaveOutDeviceId(int deviceId);
BOOL RefreshSynth(void);
BOOL StartPlayback(HWND hWnd);
BOOL StartMidiInputStream(HWND hWnd);
void StopMidiInputStream(void);
void ResumeMidiInputIfIdle(void);
void PauseWaveOutDevice(void);
void ResumeWaveOutDevice(void);
void StopPlayback(void);
void ReloadEngineConfig(void);
void ForceInstrumentLoad(void);
void FreeDefaultInstrument(void);
void UnloadEngineConfig(void);
void RestoreEngineDefaults(void);
void SeekAbsolute(int targetMs);
void RestartTrack(void);
void JumpToTrackEnd(void);
void SeekRelative(int deltaMs);
BOOL WINAPI AudioOutputDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

BOOL IsValidMidiInDeviceId(int deviceId);
void OpenMidiInDevice(HWND hWnd);
void CloseMidiInDevice(void);
void HandleMidiInShortMessage(DWORD_PTR packedMsg);
void HandleMidiInLongMessage(MIDIHDR *hdr);
void HandleMidiInReset(MidiInResetType type);
BOOL WINAPI MidiInputDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

BOOL WINAPI ChannelMixerDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

BOOL WINAPI VirtualKeyboardDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

BOOL WINAPI InjectMidiDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

BOOL AlreadyRunningPromptSuppressed(void);
void SuppressAlreadyRunningPrompt(void);
BOOL RunFromTrayEnabled(void);
BOOL LoadFilesIntoRunningInstanceEnabled(void);
void SaveLoadFilesIntoRunningInstanceSetting(BOOL enabled);
void LoadAudioOutputSettings(void);
void SaveAudioOutputSettings(void);
void LoadMidiInputSettings(void);
void SaveMidiInputSettings(void);
void LoadPlayerOptions(void);
void SavePlayerOptions(void);
void RefreshLoadFilesIntoRunningInstanceSetting(void);
void LoadAlwaysOnTopSetting(void);
void SaveAlwaysOnTopSetting(void);
void LoadViewSettings(void);
void SaveViewSettings(void);
void LoadDefaultOutputDir(TCHAR *buf, int bufSize);
void SaveDefaultOutputDir(const TCHAR *dir);
int LoadDefaultOutputFormat(int defaultIndex);
void SaveDefaultOutputFormat(int formatIndex);
void LoadRecentFiles(void);
void SaveRecentFiles(void);
void ClearRecentFiles(void);
BOOL LoadSession(HWND hWnd);
void SaveSession(void);
BOOL ImportPlayerSettings(const TCHAR *path);
BOOL ExportPlayerSettings(const TCHAR *path);
BOOL DeletePlayerSettings(void);

BOOL WINAPI ConfigDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void OpenCommandLineBatch(HWND hWnd, const TCHAR *cmdPaths);
void AddPlaylistToBatch(void);
BOOL PlaylistHasNewBatchFiles(void);
BOOL WINAPI BatchDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void ConvertCurrentToWav(HWND hWnd);
void ShowQuickConvertResult(HWND hWnd, const QuickConvertResult *result);

void StartPendingSleepTimer(void);
void CancelSleepTimer(void);
BOOL WINAPI JumpTimeDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL WINAPI SleepTimerDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL WINAPI PlayerOptionsDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void RefreshPlayerUI(HWND hWnd);
void UnloadCurrentFile(HWND hWnd);
void ClearPlaylist(HWND hWnd);
void RebuildRecentFilesMenu(void);
void RemoveRecentFile(const TCHAR *path);
void ReloadCurrentFile(HWND hWnd);
BOOL PlayPlaylistEntryAt(HWND hWnd, int index);
BOOL LoadAndPlayFile(HWND hWnd, const TCHAR *path);
BOOL SeedSinglePlaylistAndPlay(HWND hWnd, const TCHAR *path);
void OpenCommandLinePath(HWND hWnd, const TCHAR *cmdPath);
BOOL OpenMidiFilesDialog(HWND hWnd);
BOOL AddMidiFilesDialog(HWND hWnd);
BOOL AddFolderDialog(HWND hWnd);
BOOL DropFiles(HWND hWnd, HDROP hDrop);
BOOL ClipboardHasPasteableFiles(void);
BOOL PasteFilesFromClipboard(HWND hWnd);
BOOL OpenM3UPlaylistDialog(HWND hWnd);
void SaveM3UPlaylistDialog(HWND hWnd);
void NextTrack(HWND hWnd);
void PreviousTrack(HWND hWnd);
void FirstTrack(HWND hWnd);
void LastTrack(HWND hWnd);
void TogglePlayPause(HWND hWnd);
void ShufflePlaylist(HWND hWnd);
void PopulatePlaylistListBox(HWND hWnd);
void RemovePlaylistItemAt(HWND hWnd, int idx);
BOOL WINAPI PlaylistMgrDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

void UpdateVolumeKeyInterception(void);
void SyncTrayIcon(HWND hWnd);
void UpdateMenuState(HWND hWnd);
void UpdateStatusBar(void);
void UpdateWindowTitle(HWND hWnd);
LRESULT CALLBACK PlayerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

#ifdef __cplusplus
}
#endif

#endif
