#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include "registry.h"

#define REG_KEY_TIMDRV			_T("Software\\Datajake\\TimidityDriver")
#define REG_NAME_CONFIGFILE		_T("ConfigFile")
#define REG_NAME_SAMPLERATE		_T("SampleRate")
#define REG_NAME_CTRLRATE		_T("ControlRate")
#define REG_NAME_VOICES			_T("Voices")
#define REG_NAME_AMP			_T("Amp")
#define REG_NAME_ADJUSTPAN		_T("AdjustPanning")
#define REG_NAME_MONO			_T("Mono")
#define REG_NAME_8BIT			_T("8Bit")
#define REG_NAME_ANTIALIAS		_T("Antialiasing")
#define REG_NAME_PRERESAMPLE		_T("PreResample")
#define REG_NAME_FASTDEC		_T("FastDecay")
#define REG_NAME_DYNALOAD		_T("DynamicLoad")
#define REG_NAME_DEFINST		_T("DefaultInstrument")
#define REG_NAME_DEFPROG		_T("DefaultProgram")
#define REG_NAME_DRUMCHANNELS		_T("DrumChannels")
#define REG_NAME_QUIETCHANNELS		_T("QuietChannels")
#define REG_NAME_REVERBENABLED		_T("ReverbEnabled")
#define REG_NAME_REVERBONLY		_T("ReverbOnly")
#define REG_NAME_REVERBLEVEL		_T("ReverbLevel")
#define REG_NAME_REVERBPRESET		_T("ReverbPreset")
#define REG_NAME_CHORUSENABLED		_T("ChorusEnabled")
#define REG_NAME_CHORUSDEPTH		_T("ChorusDepth")
#define REG_NAME_DITHERENABLED		_T("DitherEnabled")

void ReadRegistry(DriverConfig *cfg)
{
	HKEY hKey;
	DWORD dwType, dwSize;

	if (RegOpenKeyEx(HKEY_CURRENT_USER, REG_KEY_TIMDRV, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		return;

	dwSize = sizeof(int);
	RegQueryValueEx(hKey, REG_NAME_SAMPLERATE, 0, &dwType, (LPBYTE)&cfg->nSampleRate, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_CTRLRATE, 0, &dwType, (LPBYTE)&cfg->nControlRate, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_VOICES, 0, &dwType, (LPBYTE)&cfg->nVoices, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_AMP, 0, &dwType, (LPBYTE)&cfg->nAmp, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_ADJUSTPAN, 0, &dwType, (LPBYTE)&cfg->fAdjustPanning, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_MONO, 0, &dwType, (LPBYTE)&cfg->fMono, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_8BIT, 0, &dwType, (LPBYTE)&cfg->f8Bit, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_ANTIALIAS, 0, &dwType, (LPBYTE)&cfg->fAntialiasing, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_PRERESAMPLE, 0, &dwType, (LPBYTE)&cfg->fPreResample, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_FASTDEC, 0, &dwType, (LPBYTE)&cfg->fFastDecay, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_DYNALOAD, 0, &dwType, (LPBYTE)&cfg->fDynamicLoad, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_DEFPROG, 0, &dwType, (LPBYTE)&cfg->nDefaultProgram, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_DRUMCHANNELS, 0, &dwType, (LPBYTE)&cfg->nDrumChannels, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_QUIETCHANNELS, 0, &dwType, (LPBYTE)&cfg->nQuietChannels, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_REVERBENABLED, 0, &dwType, (LPBYTE)&cfg->fReverbEnabled, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_REVERBONLY, 0, &dwType, (LPBYTE)&cfg->fReverbOnly, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_REVERBLEVEL, 0, &dwType, (LPBYTE)&cfg->nReverbLevel, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_REVERBPRESET, 0, &dwType, (LPBYTE)&cfg->nReverbPreset, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_CHORUSENABLED, 0, &dwType, (LPBYTE)&cfg->fChorusEnabled, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_CHORUSDEPTH, 0, &dwType, (LPBYTE)&cfg->nChorusDepth, &dwSize);
	RegQueryValueEx(hKey, REG_NAME_DITHERENABLED, 0, &dwType, (LPBYTE)&cfg->fDitherEnabled, &dwSize);

	dwSize = sizeof(cfg->szConfigFile);
	RegQueryValueEx(hKey, REG_NAME_CONFIGFILE, 0, &dwType, (LPBYTE)&cfg->szConfigFile, &dwSize);
	dwSize = sizeof(cfg->szDefaultInstrument);
	RegQueryValueEx(hKey, REG_NAME_DEFINST, 0, &dwType, (LPBYTE)&cfg->szDefaultInstrument, &dwSize);

	RegCloseKey(hKey);
}

void WriteRegistry(DriverConfig *cfg)
{
	HKEY hKey;
	DWORD dwDispos;

	if (RegCreateKeyEx(HKEY_CURRENT_USER, REG_KEY_TIMDRV, 
			0, NULL, 0, KEY_WRITE, NULL, &hKey, &dwDispos) != ERROR_SUCCESS)
		return;

	RegSetValueEx(hKey, REG_NAME_SAMPLERATE, 0, REG_DWORD, (LPBYTE)&cfg->nSampleRate, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_CTRLRATE, 0, REG_DWORD, (LPBYTE)&cfg->nControlRate, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_VOICES, 0, REG_DWORD, (LPBYTE)&cfg->nVoices, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_AMP, 0, REG_DWORD, (LPBYTE)&cfg->nAmp, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_ADJUSTPAN, 0, REG_DWORD, (LPBYTE)&cfg->fAdjustPanning, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_MONO, 0, REG_DWORD, (LPBYTE)&cfg->fMono, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_8BIT, 0, REG_DWORD, (LPBYTE)&cfg->f8Bit, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_ANTIALIAS, 0, REG_DWORD, (LPBYTE)&cfg->fAntialiasing, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_PRERESAMPLE, 0, REG_DWORD, (LPBYTE)&cfg->fPreResample, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_FASTDEC, 0, REG_DWORD, (LPBYTE)&cfg->fFastDecay, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_DYNALOAD, 0, REG_DWORD, (LPBYTE)&cfg->fDynamicLoad, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_DEFPROG, 0, REG_DWORD, (LPBYTE)&cfg->nDefaultProgram, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_DRUMCHANNELS, 0, REG_DWORD, (LPBYTE)&cfg->nDrumChannels, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_QUIETCHANNELS, 0, REG_DWORD, (LPBYTE)&cfg->nQuietChannels, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_REVERBENABLED, 0, REG_DWORD, (LPBYTE)&cfg->fReverbEnabled, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_REVERBONLY, 0, REG_DWORD, (LPBYTE)&cfg->fReverbOnly, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_REVERBLEVEL, 0, REG_DWORD, (LPBYTE)&cfg->nReverbLevel, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_REVERBPRESET, 0, REG_DWORD, (LPBYTE)&cfg->nReverbPreset, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_CHORUSENABLED, 0, REG_DWORD, (LPBYTE)&cfg->fChorusEnabled, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_CHORUSDEPTH, 0, REG_DWORD, (LPBYTE)&cfg->nChorusDepth, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_DITHERENABLED, 0, REG_DWORD, (LPBYTE)&cfg->fDitherEnabled, sizeof(DWORD));
	RegSetValueEx(hKey, REG_NAME_CONFIGFILE, 0, REG_SZ, 
				(LPBYTE)&cfg->szConfigFile, sizeof(TCHAR) * (_tcslen(cfg->szConfigFile) + 1));
	RegSetValueEx(hKey, REG_NAME_DEFINST, 0, REG_SZ, 
				(LPBYTE)&cfg->szDefaultInstrument, sizeof(TCHAR) * (_tcslen(cfg->szDefaultInstrument) + 1));

	RegCloseKey(hKey);
}
