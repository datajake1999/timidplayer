@echo off
set TCC=C:\tcc
"%TCC%\tcc.exe" -impdef advapi32.dll
"%TCC%\tcc.exe" -impdef comctl32.dll
"%TCC%\tcc.exe" -impdef comdlg32.dll
"%TCC%\tcc.exe" -impdef kernel32.dll
"%TCC%\tcc.exe" -impdef ole32.dll
"%TCC%\tcc.exe" -impdef shell32.dll
"%TCC%\tcc.exe" -impdef shlwapi.dll
"%TCC%\tcc.exe" -impdef user32.dll
"%TCC%\tcc.exe" -impdef winmm.dll
"%TCC%\tcc.exe" -c -DUNICODE -D_UNICODE ..\src\common\*.c ..\src\timidity\*.c ..\src\timidplayer\*.c ..\src\wav_writer\*.c
if errorlevel 1 goto :eof
windres ..\src\timidplayer\timidplayer.rc resource.o
if errorlevel 1 goto :eof
"%TCC%\tcc.exe" *.o advapi32.def comctl32.def comdlg32.def kernel32.def ole32.def shell32.def shlwapi.def user32.def winmm.def -o timidplayer.exe
if errorlevel 1 goto :eof
del *.def *.o
