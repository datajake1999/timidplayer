@echo off
set WATCOM=C:\watcom
set PATH=%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h;%WATCOM%\h\nt
wcl386 -q -c -bt=nt -bm -j -ox -w3 -dUNICODE -d_UNICODE -d_WIN32_IE=0x0500 ..\src\common\*.c ..\src\timidity\*.c ..\src\timidplayer\*.c ..\src\wav_writer\*.c
if errorlevel 1 goto :eof
wrc -q -bt=nt -r -fo=timidplayer.res -i=..\src\timidplayer ..\src\timidplayer\timidplayer.rc
if errorlevel 1 goto :eof
del objs.lnk > nul 2>&1
for %%f in (*.obj) do echo file %%f >> objs.lnk
wlink option quiet system nt_win name timidplayer.exe @objs.lnk library advapi32,comctl32,comdlg32,kernel32,ole32,shell32,shlwapi,user32,winmm option resource=timidplayer.res
if errorlevel 1 goto :eof
del *.err *.obj *.res objs.lnk
