@echo off
set BCC55=C:\BCC55
set PATH=%BCC55%\bin;%PATH%
bcc32 -q -c -DUNICODE -D_UNICODE -I"..\src\include\pstdint" -O2 -w-8004 -w-8057 ..\src\common\*.c ..\src\timidity\*.c ..\src\timidplayer\*.c ..\src\wav_writer\*.c
if errorlevel 1 goto :eof
brcc32 -fotimidplayer.res -I"%BCC55%\Include" ..\src\timidplayer\timidplayer.rc
if errorlevel 1 goto :eof
ilink32 -q -Tpe -aa c0w32.obj registry.obj chorus.obj common.obj filter.obj instrum.obj mix.obj mtwister.obj playmidi.obj readcfg.obj readmidi.obj resample.obj reverb.obj ReverbEffect.obj skchorus.obj tables.obj batchconv.obj chanmix.obj configdlg.obj mainwnd.obj midiin.obj midiinject.obj player.obj settings.obj timidplayer.obj util.obj vkbd.obj waveout.obj wav_writer.obj,timidplayer.exe,,advapi32.lib comctl32.lib comdlg32.lib cw32mt.lib kernel32.lib ole32.lib shell32.lib shlwapi.lib user32.lib winmm.lib,,timidplayer.res
if errorlevel 1 goto :eof
del *.ilc *.ild *.ilf *.ils *.map *.obj *.res *.tds
