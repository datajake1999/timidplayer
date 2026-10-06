@echo off
set DMC=C:\dm
set PATH=%DMC%\bin;%PATH%
dmc -c -DSTRICT -DUNICODE -D_UNICODE -I..\src\dmc_compat\include ..\src\dmc_compat\dmc_compat.c ..\src\common\registry.c ..\src\timidity\chorus.c ..\src\timidity\common.c ..\src\timidity\filter.c ..\src\timidity\instrum.c ..\src\timidity\mix.c ..\src\timidity\mtwister.c ..\src\timidity\playmidi.c ..\src\timidity\readcfg.c ..\src\timidity\readmidi.c ..\src\timidity\resample.c ..\src\timidity\reverb.c ..\src\timidity\ReverbEffect.c ..\src\timidity\skchorus.c ..\src\timidity\tables.c ..\src\timidplayer\batchconv.c ..\src\timidplayer\chanmix.c ..\src\timidplayer\configdlg.c ..\src\timidplayer\mainwnd.c ..\src\timidplayer\midiin.c ..\src\timidplayer\midiinject.c ..\src\timidplayer\player.c ..\src\timidplayer\settings.c ..\src\timidplayer\timidplayer.c ..\src\timidplayer\util.c ..\src\timidplayer\vkbd.c ..\src\timidplayer\waveout.c ..\src\wav_writer\wav_writer.c
if errorlevel 1 goto :eof
rcc -32 ..\src\timidplayer\timidplayer.rc
if errorlevel 1 goto :eof
link dmc_compat.obj registry.obj chorus.obj common.obj filter.obj instrum.obj mix.obj mtwister.obj playmidi.obj readcfg.obj readmidi.obj resample.obj reverb.obj ReverbEffect.obj skchorus.obj tables.obj batchconv.obj chanmix.obj configdlg.obj mainwnd.obj midiin.obj midiinject.obj player.obj settings.obj timidplayer.obj util.obj vkbd.obj waveout.obj wav_writer.obj,timidplayer.exe,,advapi32.lib comctl32.lib comdlg32.lib kernel32.lib ole32.lib shell32.lib user32.lib winmm.lib,,timidplayer.res /SUBSYSTEM:WINDOWS
if errorlevel 1 goto :eof
del *.map *.obj *.res
