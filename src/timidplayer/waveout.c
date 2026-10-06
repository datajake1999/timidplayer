#include "timidplayer.h"

#define MIDIIN_SUBSLICE_FRAMES 64

Timid *CreateConfiguredSynth(int *outChannels, int *outBitDepth, int *outAudioFormat, int *outSampleRate)
{
	Timid *synth;
	DriverConfig cfg;
	char szAnsi[MAX_PATH];
	TCHAR prevDir[MAX_PATH];
	int defInstLoaded, configLoaded, i;
	InitDriverConfigDefaults(&cfg);
	ReadRegistry(&cfg);
	cfg.nSampleRate = ClampInt(cfg.nSampleRate, MIN_OUTPUT_RATE, MAX_OUTPUT_RATE);
	*outSampleRate = cfg.nSampleRate;
	if (cfg.fMono)
	{
		*outChannels = 1;
	}
	else
	{
		*outChannels = 2;
	}
	if (cfg.f8Bit)
	{
		*outBitDepth = 8;
		*outAudioFormat = AU_CHAR;
	}
	else
	{
		*outBitDepth = 16;
		*outAudioFormat = AU_SHORT;
	}
	synth = timid_init();
	if (!synth)
	{
		return NULL;
	}
	timid_set_sample_rate(synth, cfg.nSampleRate);
	timid_set_control_rate(synth, cfg.nControlRate);
	timid_set_max_voices(synth, cfg.nVoices);
	timid_set_amplification(synth, cfg.nAmp);
	timid_set_immediate_panning(synth, cfg.fAdjustPanning);
	timid_set_mono(synth, cfg.fMono);
	timid_set_antialiasing(synth, cfg.fAntialiasing);
	timid_set_pre_resample(synth, cfg.fPreResample);
	timid_set_fast_decay(synth, cfg.fFastDecay);
	timid_set_dynamic_instrument_load(synth, cfg.fDynamicLoad);
	timid_set_default_program(synth, cfg.nDefaultProgram);
	for (i = 0; i < 16; i++)
	{
		timid_set_drum_channel(synth, i, (cfg.nDrumChannels & (1<<i)) != 0);
		timid_set_quiet_channel(synth, i, (cfg.nQuietChannels & (1<<i)) != 0);
	}
	timid_set_reverb_enabled(synth, cfg.fReverbEnabled);
	timid_set_reverb_only(synth, cfg.fReverbOnly);
	timid_set_reverb_level(synth, cfg.nReverbLevel);
	timid_set_reverb_preset(synth, cfg.nReverbPreset);
	timid_set_chorus_enabled(synth, cfg.fChorusEnabled);
	timid_set_chorus_depth(synth, cfg.nChorusDepth);
	timid_set_dither_enabled(synth, cfg.fDitherEnabled);
	prevDir[0] = _T('\0');
	GetCurrentDirectory(MAX_PATH, prevDir);
	SetAppDirectory();
	TCharToAnsi(cfg.szDefaultInstrument, szAnsi, MAX_PATH);
	defInstLoaded = timid_set_default_instrument(synth, szAnsi);
	TCharToAnsi(cfg.szConfigFile, szAnsi, MAX_PATH);
	if (!strlen(szAnsi))
	{
		strncpy(szAnsi, CONFIG_FILE, MAX_PATH-1);
		szAnsi[MAX_PATH-1] = '\0';
	}
	configLoaded = timid_load_config(synth, szAnsi);
	if (prevDir[0])
	{
		SetCurrentDirectory(prevDir);
	}
	if (!defInstLoaded && !configLoaded)
	{
		timid_close(synth);
		return NULL;
	}
	timid_reset(synth);
	return synth;
}

double CalculateCpuLoad(LARGE_INTEGER start, LARGE_INTEGER end, LONGLONG numSamples)
{
	static LARGE_INTEGER s_freq;
	static BOOL s_haveFreq = FALSE;
	double elapsedSeconds, realSeconds;
	if (!s_haveFreq)
	{
		if (!QueryPerformanceFrequency(&s_freq))
		{
			s_freq.QuadPart = 0;
		}
		s_haveFreq = TRUE;
	}
	if (s_freq.QuadPart <= 0 || g_App->sampleRate <= 0 || numSamples <= 0)
	{
		return 0.0;
	}
	elapsedSeconds = (double)(end.QuadPart - start.QuadPart) / (double)s_freq.QuadPart;
	realSeconds = (double)numSamples / (double)g_App->sampleRate;
	if (realSeconds <= 0.0)
	{
		return 0.0;
	}
	return (elapsedSeconds / realSeconds) * 100.0;
}

DWORD GetMidiInPlaybackFrame(void)
{
	MMTIME mmTime;
	int delta;
	mmTime.wType = TIME_SAMPLES;
	if (!g_App->hWaveOut || waveOutGetPosition(g_App->hWaveOut, &mmTime, sizeof(MMTIME)) != MMSYSERR_NOERROR || mmTime.wType != TIME_SAMPLES)
	{
		return g_App->midiInFramesWritten;
	}
	delta = (int)(mmTime.u.sample - g_App->midiInPlayPosPrev);
	if (delta < -(1 << 26))
	{
		g_App->midiInPlayPosWraps++;
	}
	g_App->midiInPlayPosPrev = mmTime.u.sample;
	return mmTime.u.sample + g_App->midiInPlayPosWraps * (1UL << 27);
}

static void FinishChunkRender(int i, LARGE_INTEGER start, LARGE_INTEGER end)
{
	g_App->cpuLoadLastStart = start;
	g_App->cpuLoadLastEnd = end;
	g_App->cpuLoadLastSamples = g_App->chunkFrames;
	waveOutWrite(g_App->hWaveOut, &g_App->waveHdr[i], sizeof(WAVEHDR));
}

static void RenderLiveChunk(int i)
{
	unsigned char *bufpos = (unsigned char *)g_App->waveHdr[i].lpData;
	long framesLeft = g_App->chunkFrames;
	int frameBytes = g_App->channels * (g_App->bitDepth / 8);
	LARGE_INTEGER start, end;
	QueryPerformanceCounter(&start);
	while (framesLeft > 0)
	{
		long subFrames;
		EnterCriticalSection(&g_App->synthCS);
		while (g_App->midiInQueueTail != g_App->midiInQueueHead && (long)(g_App->midiInQueue[g_App->midiInQueueTail].frame - g_App->midiInFramesWritten) <= 0)
		{
			unsigned long msg = g_App->midiInQueue[g_App->midiInQueueTail].msg;
			g_App->midiInQueueTail = (g_App->midiInQueueTail + 1) % MIDIIN_QUEUE_SIZE;
			timid_write_midi_packed(g_App->synth, msg);
		}
		subFrames = framesLeft;
		if (g_App->midiInQueueTail != g_App->midiInQueueHead)
		{
			long dueIn = (long)(g_App->midiInQueue[g_App->midiInQueueTail].frame - g_App->midiInFramesWritten);
			if (dueIn > 0 && dueIn < subFrames)
			{
				subFrames = dueIn;
			}
		}
		if (subFrames > MIDIIN_SUBSLICE_FRAMES)
		{
			subFrames = MIDIIN_SUBSLICE_FRAMES;
		}
		if (g_App->audioFormat == AU_CHAR)
		{
			timid_render_char(g_App->synth, (unsigned char *)bufpos, subFrames);
		}
		else
		{
			timid_render_short(g_App->synth, (short *)bufpos, subFrames);
		}
		g_App->midiInFramesWritten += subFrames;
		LeaveCriticalSection(&g_App->synthCS);
		bufpos += (size_t)subFrames * frameBytes;
		framesLeft -= subFrames;
	}
	QueryPerformanceCounter(&end);
	FinishChunkRender(i, start, end);
}

static void RenderChunk(int i, BOOL *pEof)
{
	BOOL ok;
	LARGE_INTEGER start, end;
	if (g_App->bMidiInActive)
	{
		RenderLiveChunk(i);
		return;
	}
	EnterCriticalSection(&g_App->synthCS);
	QueryPerformanceCounter(&start);
	if (timid_play_smf(g_App->synth, g_App->audioFormat, (unsigned char *)g_App->waveHdr[i].lpData, g_App->chunkFrames))
	{
		ok = TRUE;
	}
	else
	{
		ok = FALSE;
	}
	QueryPerformanceCounter(&end);
	if (!ok)
	{
		if (g_App->bitDepth == 8)
		{
			FillMemory(g_App->waveHdr[i].lpData, g_App->waveHdr[i].dwBufferLength, 0x80);
		}
		else
		{
			ZeroMemory(g_App->waveHdr[i].lpData, g_App->waveHdr[i].dwBufferLength);
		}
		*pEof = TRUE;
	}
	FinishChunkRender(i, start, end);
	LeaveCriticalSection(&g_App->synthCS);
}

static DWORD WINAPI PlaybackThreadProc(LPVOID param)
{
	BOOL eof = FALSE;
	DWORD myGeneration = (DWORD)(UINT_PTR)param;
	for (;;)
	{
		BOOL anyActive = FALSE;
		BOOL didWork = FALSE;
		int i;
		if (g_App->bStopRequested)
		{
			break;
		}
		for (i = 0; i < g_App->numChunks; i++)
		{
			if (g_App->waveHdr[i].dwFlags & WHDR_DONE)
			{
				if (!eof && !g_App->bPaused)
				{
					didWork = TRUE;
					RenderChunk(i, &eof);
					anyActive = TRUE;
				}
			}
			else
			{
				anyActive = TRUE;
			}
		}
		if (eof && !anyActive)
		{
			break;
		}
		if (!didWork)
		{
			WaitForSingleObject(g_App->hWaveEvent, g_App->chunkMs);
		}
	}
	if (!g_App->bStopRequested)
	{
		PostMessage(g_App->hPlayerWnd, WM_PLAYBACK_ENDED, (WPARAM)myGeneration, 0);
	}
	return 0;
}

static void ApplyWaveOutVolume(void)
{
	WORD level;
	DWORD volDword;
	if (!g_App->hWaveOut)
	{
		return;
	}
	level = (WORD)((g_App->volume * 0xFFFF) / 100);
	volDword = MAKELONG(level, level);
	waveOutSetVolume(g_App->hWaveOut, volDword);
}

void ChangeVolume(int delta)
{
	g_App->volume += delta;
	g_App->volume = ClampInt(g_App->volume, 0, 100);
	ApplyWaveOutVolume();
	UpdateStatusBar();
}

BOOL IsValidWaveOutDeviceId(int deviceId)
{
	return deviceId >= 0 && (UINT)deviceId < waveOutGetNumDevs();
}

static void CloseWaveOutDevice(void)
{
	int i;
	if (g_App->hWaveOut)
	{
		waveOutReset(g_App->hWaveOut);
		for (i = 0; i < g_App->numChunks; i++)
		{
			waveOutUnprepareHeader(g_App->hWaveOut, &g_App->waveHdr[i], sizeof(WAVEHDR));
		}
		waveOutClose(g_App->hWaveOut);
		g_App->hWaveOut = NULL;
	}
	if (g_App->waveBuffer)
	{
		free(g_App->waveBuffer);
		g_App->waveBuffer = NULL;
	}
	CloseHandleAndClear(&g_App->hWaveEvent);
}

static BOOL OpenWaveOutDevice(void)
{
	WAVEFORMATEX wfx;
	DWORD chunkBytes;
	UINT_PTR deviceId;
	int i;
	g_App->numChunks = (g_App->bufferMs + g_App->chunkMs - 1) / g_App->chunkMs;
	g_App->numChunks = ClampInt(g_App->numChunks, 1, MAX_PLAYER_CHUNKS);
	g_App->chunkFrames = timid_millis2samples(g_App->synth, g_App->chunkMs);
	if (g_App->chunkFrames < 1)
	{
		g_App->chunkFrames = 1;
	}
	chunkBytes = (DWORD)g_App->chunkFrames * g_App->channels * (g_App->bitDepth / 8);
	ZeroMemory(&wfx, sizeof(wfx));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = (WORD)g_App->channels;
	wfx.nSamplesPerSec = g_App->sampleRate;
	wfx.wBitsPerSample = (WORD)g_App->bitDepth;
	wfx.nBlockAlign = (WORD)(g_App->channels * (g_App->bitDepth / 8));
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	g_App->hWaveEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (!g_App->hWaveEvent)
	{
		return FALSE;
	}
	if (g_App->outputDeviceId < 0)
	{
		deviceId = (UINT_PTR)WAVE_MAPPER;
	}
	else
	{
		deviceId = (UINT_PTR)g_App->outputDeviceId;
	}
	if (waveOutOpen(&g_App->hWaveOut, deviceId, &wfx, (DWORD_PTR)g_App->hWaveEvent, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR)
	{
		BOOL fallbackFailed = TRUE;
		if (deviceId != (UINT_PTR)WAVE_MAPPER)
		{
			fallbackFailed = (waveOutOpen(&g_App->hWaveOut, (UINT_PTR)WAVE_MAPPER, &wfx, (DWORD_PTR)g_App->hWaveEvent, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR);
			if (!fallbackFailed)
			{
				g_App->outputDeviceId = WAVE_MAPPER;
			}
		}
		if (fallbackFailed)
		{
			g_App->hWaveOut = NULL;
			CloseHandleAndClear(&g_App->hWaveEvent);
			return FALSE;
		}
	}
	g_App->waveBuffer = (unsigned char *)malloc(chunkBytes * g_App->numChunks);
	if (!g_App->waveBuffer)
	{
		CloseWaveOutDevice();
		return FALSE;
	}
	ZeroMemory(g_App->waveHdr, sizeof(WAVEHDR) * MAX_PLAYER_CHUNKS);
	for (i = 0; i < g_App->numChunks; i++)
	{
		g_App->waveHdr[i].lpData = (LPSTR)(g_App->waveBuffer + (size_t)i * chunkBytes);
		g_App->waveHdr[i].dwBufferLength = chunkBytes;
		if (waveOutPrepareHeader(g_App->hWaveOut, &g_App->waveHdr[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			CloseWaveOutDevice();
			return FALSE;
		}
	}
	ApplyWaveOutVolume();
	return TRUE;
}

static BOOL BeginPlaybackStream(HWND hWnd, BOOL startPaused)
{
	int i;
	DWORD threadId;
	BOOL eof = FALSE;
	if (!OpenWaveOutDevice())
	{
		if (hWnd && IsWindowEnabled(hWnd))
		{
			ShowAppStringMessage(hWnd, IDS_WAVEOUTFAILED, MB_ICONERROR);
		}
		return FALSE;
	}
	if (g_App->bMidiInActive)
	{
		g_App->midiInFramesWritten = 0;
		g_App->midiInQueueHead = 0;
		g_App->midiInQueueTail = 0;
		g_App->midiInPlayPosPrev = 0;
		g_App->midiInPlayPosWraps = 0;
	}
	g_App->bStopRequested = FALSE;
	g_App->bPaused = FALSE;
	g_App->playbackGeneration++;
	for (i = 0; i < g_App->numChunks; i++)
	{
		RenderChunk(i, &eof);
	}
	g_App->hPlaybackThread = CreateThread(NULL, 0, PlaybackThreadProc, (LPVOID)(UINT_PTR)g_App->playbackGeneration, 0, &threadId);
	if (!g_App->hPlaybackThread)
	{
		CloseWaveOutDevice();
		return FALSE;
	}
	SetThreadPriority(g_App->hPlaybackThread, THREAD_PRIORITY_TIME_CRITICAL);
	g_App->state = PLAYER_PLAYING;
	if (startPaused)
	{
		g_App->bPaused = TRUE;
		PauseWaveOutDevice();
		g_App->state = PLAYER_PAUSED;
	}
	return TRUE;
}

static void RestartPlaybackStream(BOOL wasPaused)
{
	if (!BeginPlaybackStream(g_App->hPlayerWnd, wasPaused))
	{
		g_App->state = PLAYER_STOPPED;
		g_App->bPaused = FALSE;
	}
	RefreshPlayerUI(g_App->hPlayerWnd);
}

static void StopPlaybackThread(void)
{
	g_App->bStopRequested = TRUE;
	if (g_App->hWaveEvent)
	{
		SetEvent(g_App->hWaveEvent);
	}
	if (g_App->hPlaybackThread)
	{
		WaitForSingleObject(g_App->hPlaybackThread, INFINITE);
		CloseHandleAndClear(&g_App->hPlaybackThread);
	}
	CloseWaveOutDevice();
}

static void StopPlaybackStreamKeepPosition(void)
{
	if (g_App->state == PLAYER_STOPPED)
	{
		return;
	}
	StopPlaybackThread();
}

static void ReopenWaveOutDevice(void)
{
	BOOL wasPaused = (g_App->state == PLAYER_PAUSED);
	StopPlaybackStreamKeepPosition();
	RestartPlaybackStream(wasPaused);
}

BOOL RefreshSynth(void)
{
	int newChannels, newBitDepth, newAudioFormat, newSampleRate;
	Timid *newSynth;
	BOOL hadSynth;
	BOOL formatChanged;
	BOOL wasPaused = FALSE;
	int savedPositionMs = -1;
	if (g_App->synth && !g_App->bConfigDirty)
	{
		return TRUE;
	}
	hadSynth = (g_App->synth != NULL);
	newSynth = CreateConfiguredSynth(&newChannels, &newBitDepth, &newAudioFormat, &newSampleRate);
	if (!newSynth)
	{
		g_App->bConfigDirty = FALSE;
		if (hadSynth)
		{
			ShowAppStringMessage(g_App->hPlayerWnd, IDS_CONFIGAPPLYFAILED, MB_ICONERROR);
		}
		return (g_App->synth != NULL);
	}
	formatChanged = (g_App->hWaveOut != NULL) && (g_App->channels != newChannels || g_App->bitDepth != newBitDepth || g_App->sampleRate != newSampleRate);
	if (formatChanged)
	{
		wasPaused = (g_App->state == PLAYER_PAUSED);
		StopPlaybackStreamKeepPosition();
	}
	EnterCriticalSection(&g_App->synthCS);
	if (g_App->bFileLoaded)
	{
		char ansiPath[MAX_PATH];
		if (hadSynth)
		{
			savedPositionMs = timid_get_current_time(g_App->synth);
		}
		TCharToAnsi(g_App->loadedFilePath, ansiPath, MAX_PATH);
		if (!timid_load_smf(newSynth, ansiPath))
		{
			timid_close(newSynth);
			LeaveCriticalSection(&g_App->synthCS);
			g_App->bConfigDirty = FALSE;
			if (formatChanged)
			{
				RestartPlaybackStream(wasPaused);
			}
			if (hadSynth)
			{
				ShowAppStringMessage(g_App->hPlayerWnd, IDS_CONFIGAPPLYFAILED, MB_ICONERROR);
			}
			return (g_App->synth != NULL);
		}
		if (savedPositionMs > 0)
		{
			timid_seek_smf(newSynth, savedPositionMs);
		}
	}
	if (g_App->synth)
	{
		timid_close(g_App->synth);
	}
	g_App->synth = newSynth;
	g_App->channels = newChannels;
	g_App->bitDepth = newBitDepth;
	g_App->audioFormat = newAudioFormat;
	g_App->sampleRate = newSampleRate;
	LeaveCriticalSection(&g_App->synthCS);
	g_App->bConfigDirty = FALSE;
	if (formatChanged)
	{
		RestartPlaybackStream(wasPaused);
	}
	return TRUE;
}

BOOL StartPlayback(HWND hWnd)
{
	if (!g_App->bFileLoaded)
	{
		return FALSE;
	}
	if (g_App->bMidiInEnabled)
	{
		return FALSE;
	}
	if (!RefreshSynth())
	{
		return FALSE;
	}
	if (!BeginPlaybackStream(hWnd, FALSE))
	{
		return FALSE;
	}
	StartPendingSleepTimer();
	return TRUE;
}

BOOL StartMidiInputStream(HWND hWnd)
{
	if (g_App->state != PLAYER_STOPPED)
	{
		StopPlayback();
	}
	g_App->bMidiInActive = TRUE;
	if (!RefreshSynth())
	{
		g_App->bMidiInActive = FALSE;
		return FALSE;
	}
	if (!BeginPlaybackStream(hWnd, FALSE))
	{
		g_App->bMidiInActive = FALSE;
		return FALSE;
	}
	return TRUE;
}

static void MarkPlaybackStopped(void)
{
	g_App->state = PLAYER_STOPPED;
	g_App->bPaused = FALSE;
	g_App->bMidiInActive = FALSE;
}

void StopMidiInputStream(void)
{
	if (!g_App->bMidiInActive)
	{
		return;
	}
	StopPlaybackStreamKeepPosition();
	MarkPlaybackStopped();
}

void ResumeMidiInputIfIdle(void)
{
	if (g_App->state != PLAYER_STOPPED)
	{
		return;
	}
	if (!g_App->bMidiInEnabled || !g_App->hMidiIn || g_App->bMidiInActive)
	{
		return;
	}
	StartMidiInputStream(g_App->hPlayerWnd);
}

void PauseWaveOutDevice(void)
{
	waveOutPause(g_App->hWaveOut);
}

void ResumeWaveOutDevice(void)
{
	waveOutRestart(g_App->hWaveOut);
}

void StopPlayback(void)
{
	if (g_App->state == PLAYER_STOPPED)
	{
		return;
	}
	StopPlaybackThread();
	EnterCriticalSection(&g_App->synthCS);
	if (g_App->bFileLoaded && g_App->synth)
	{
		timid_restart_smf(g_App->synth);
	}
	g_App->cpuLoadLastSamples = 0;
	LeaveCriticalSection(&g_App->synthCS);
	MarkPlaybackStopped();
}

typedef void (*SynthActionFn)(Timid *tm);

static void RunSynthAction(SynthActionFn fn)
{
	if (!g_App->synth)
	{
		return;
	}
	EnterCriticalSection(&g_App->synthCS);
	fn(g_App->synth);
	LeaveCriticalSection(&g_App->synthCS);
}

void ReloadEngineConfig(void)
{
	RunSynthAction((SynthActionFn)timid_reload_config);
}

void ForceInstrumentLoad(void)
{
	RunSynthAction((SynthActionFn)timid_force_instrument_load);
}

void FreeDefaultInstrument(void)
{
	RunSynthAction(timid_free_default_instrument);
}

void UnloadEngineConfig(void)
{
	RunSynthAction(timid_unload_config);
}

void RestoreEngineDefaults(void)
{
	if (!g_App->synth)
	{
		return;
	}
	EnterCriticalSection(&g_App->synthCS);
	timid_restore_defaults(g_App->synth);
	timid_set_sample_rate(g_App->synth, g_App->sampleRate);
	timid_set_mono(g_App->synth, g_App->channels == 1);
	LeaveCriticalSection(&g_App->synthCS);
}

static BOOL BeginSynthMutation(void)
{
	BOOL wasPaused = g_App->bPaused;
	g_App->bPaused = TRUE;
	return wasPaused;
}

static void EndSynthMutation(BOOL wasPaused)
{
	if (g_App->hWaveOut)
	{
		waveOutReset(g_App->hWaveOut);
		if (g_App->state == PLAYER_PAUSED)
		{
			waveOutPause(g_App->hWaveOut);
		}
	}
	g_App->bPaused = wasPaused;
	if (g_App->hWaveEvent)
	{
		SetEvent(g_App->hWaveEvent);
	}
	UpdateStatusBar();
}

static void SeekAction(Timid *synth, long arg)
{
	timid_seek_smf(synth, arg);
}

static void RestartAction(Timid *synth, long arg)
{
	(void)arg;
	timid_restart_smf(synth);
}

static void StopAction(Timid *synth, long arg)
{
	(void)arg;
	timid_stop_smf(synth);
}

static void SeekDeltaAction(Timid *synth, long arg)
{
	if (arg >= 0)
	{
		timid_fast_forward_smf(synth, arg);
	}
	else
	{
		timid_rewind_smf(synth, -arg);
	}
}

static void MutateSynth(void (*fn)(Timid *, long), long arg)
{
	BOOL wasPaused = BeginSynthMutation();
	EnterCriticalSection(&g_App->synthCS);
	fn(g_App->synth, arg);
	LeaveCriticalSection(&g_App->synthCS);
	EndSynthMutation(wasPaused);
}

static void PerformSeek(int targetMs, int durMs)
{
	targetMs = ClampInt(targetMs, 0, durMs);
	MutateSynth(SeekAction, targetMs);
}

static BOOL CanMutateLoadedTrack(void)
{
	if (!g_App->bFileLoaded)
	{
		return FALSE;
	}
	if (g_App->hPlaybackThread && WaitForSingleObject(g_App->hPlaybackThread, 0) == WAIT_OBJECT_0)
	{
		return FALSE;
	}
	return TRUE;
}

static void SeekIntoPreviousTrack(int overshoot)
{
	PreviousTrack(g_App->hPlayerWnd);
	if (g_App->bFileLoaded)
	{
		int newDurMs;
		EnterCriticalSection(&g_App->synthCS);
		newDurMs = timid_get_duration(g_App->synth);
		LeaveCriticalSection(&g_App->synthCS);
		PerformSeek(newDurMs - overshoot, newDurMs);
	}
}

static BOOL TrySeekIntoAdjacentTrack(int targetMs, int durMs)
{
	if (targetMs < 0 && g_App->playlistIndex > 0)
	{
		SeekIntoPreviousTrack(-targetMs);
		return TRUE;
	}
	if (durMs > 0 && targetMs >= durMs && g_App->playlistIndex >= 0 && g_App->playlistIndex + 1 < g_App->playlistCount)
	{
		NextTrack(g_App->hPlayerWnd);
		return TRUE;
	}
	return FALSE;
}

void SeekAbsolute(int targetMs)
{
	int durMs;
	int seekLimitMs;
	if (!CanMutateLoadedTrack())
	{
		return;
	}
	EnterCriticalSection(&g_App->synthCS);
	durMs = timid_get_duration(g_App->synth);
	LeaveCriticalSection(&g_App->synthCS);
	if (!g_App->bPreventSeekTrackChange && TrySeekIntoAdjacentTrack(targetMs, durMs))
	{
		return;
	}
	seekLimitMs = durMs;
	if (g_App->bPreventSeekTrackChange && durMs > 0 && targetMs >= durMs)
	{
		seekLimitMs = ClampInt(durMs - SEEK_STEP_MS, 0, durMs);
	}
	PerformSeek(targetMs, seekLimitMs);
}

void RestartTrack(void)
{
	if (!CanMutateLoadedTrack())
	{
		return;
	}
	MutateSynth(RestartAction, 0);
}

void JumpToTrackEnd(void)
{
	if (!CanMutateLoadedTrack())
	{
		return;
	}
	MutateSynth(StopAction, 0);
}

static void PerformRelativeSeek(int deltaMs)
{
	MutateSynth(SeekDeltaAction, deltaMs);
}

void SeekRelative(int deltaMs)
{
	int curMs;
	int durMs;
	if (!CanMutateLoadedTrack())
	{
		return;
	}
	EnterCriticalSection(&g_App->synthCS);
	curMs = timid_get_current_time(g_App->synth);
	durMs = timid_get_duration(g_App->synth);
	LeaveCriticalSection(&g_App->synthCS);
	if (!g_App->bPreventSeekTrackChange)
	{
		if (TrySeekIntoAdjacentTrack(curMs + deltaMs, durMs))
		{
			return;
		}
	}
	else if (durMs > 0 && curMs + deltaMs >= durMs)
	{
		PerformSeek(curMs + deltaMs, ClampInt(durMs - SEEK_STEP_MS, 0, durMs));
		return;
	}
	PerformRelativeSeek(deltaMs);
}

static BOOL GetWaveOutDeviceName(UINT index, TCHAR *nameOut, int nameOutChars)
{
	WAVEOUTCAPS caps;
	if (waveOutGetDevCaps(index, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
	{
		return FALSE;
	}
	SafeFormat(nameOut, nameOutChars, _T("%s"), caps.szPname);
	return TRUE;
}

BOOL WINAPI AudioOutputDialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	switch (message)
	{
	case WM_INITDIALOG:
		PopulateDeviceCombo(GetDlgItem(hWnd, IDC_AUDIODEVICE), waveOutGetNumDevs(), GetWaveOutDeviceName, IDS_WAVEMAPPER, g_App->outputDeviceId);
		SendDlgItemMessage(hWnd, IDC_BUFFERMSS, UDM_SETRANGE32, PLAYER_MIN_BUFFER_MS, PLAYER_MAX_BUFFER_MS);
		SendDlgItemMessage(hWnd, IDC_BUFFERMSS, UDM_SETPOS32, 0, g_App->bufferMs);
		SetDlgItemInt(hWnd, IDC_BUFFERMS, g_App->bufferMs, FALSE);
		SendDlgItemMessage(hWnd, IDC_CHUNKMSS, UDM_SETRANGE32, PLAYER_MIN_CHUNK_MS, PLAYER_MAX_CHUNK_MS);
		SendDlgItemMessage(hWnd, IDC_CHUNKMSS, UDM_SETPOS32, 0, g_App->chunkMs);
		SetDlgItemInt(hWnd, IDC_CHUNKMS, g_App->chunkMs, FALSE);
		if (g_App->bInterceptVolumeKeys)
		{
			CheckDlgButton(hWnd, IDC_INTERCEPTVOLUMEKEYS, BST_CHECKED);
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BUFFERMS:
			if (HIWORD(wParam) == EN_KILLFOCUS)
			{
				ClampDlgItemInt(hWnd, IDC_BUFFERMS, PLAYER_MIN_BUFFER_MS, PLAYER_MAX_BUFFER_MS);
				return TRUE;
			}
			return FALSE;
		case IDC_CHUNKMS:
			if (HIWORD(wParam) == EN_KILLFOCUS)
			{
				ClampDlgItemInt(hWnd, IDC_CHUNKMS, PLAYER_MIN_CHUNK_MS, PLAYER_MAX_CHUNK_MS);
				return TRUE;
			}
			return FALSE;
		case IDOK:
			{
				int sel = (int)SendDlgItemMessage(hWnd, IDC_AUDIODEVICE, CB_GETCURSEL, 0, 0);
				int bufMs = ClampInt(GetDlgItemInt(hWnd, IDC_BUFFERMS, NULL, FALSE), PLAYER_MIN_BUFFER_MS, PLAYER_MAX_BUFFER_MS);
				int chunkMs = ClampInt(GetDlgItemInt(hWnd, IDC_CHUNKMS, NULL, FALSE), PLAYER_MIN_CHUNK_MS, PLAYER_MAX_CHUNK_MS);
				int newDeviceId = ComboSelToDeviceId(sel, WAVE_MAPPER);
				BOOL newInterceptVolumeKeys = (IsDlgButtonChecked(hWnd, IDC_INTERCEPTVOLUMEKEYS) == BST_CHECKED);
				BOOL outputSettingsChanged;
				BOOL interceptSettingChanged;
				if (chunkMs > bufMs)
				{
					chunkMs = bufMs;
				}
				outputSettingsChanged = (g_App->hWaveOut != NULL) && (g_App->outputDeviceId != newDeviceId || g_App->bufferMs != bufMs || g_App->chunkMs != chunkMs);
				interceptSettingChanged = (g_App->bInterceptVolumeKeys != newInterceptVolumeKeys);
				g_App->outputDeviceId = newDeviceId;
				g_App->bufferMs = bufMs;
				g_App->chunkMs = chunkMs;
				g_App->bInterceptVolumeKeys = newInterceptVolumeKeys;
				SaveAudioOutputSettings();
				EndDialog(hWnd, TRUE);
				if (outputSettingsChanged)
				{
					ReopenWaveOutDevice();
				}
				if (interceptSettingChanged)
				{
					UpdateVolumeKeyInterception();
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
