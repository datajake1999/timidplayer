@echo off
windres ..\src\timidplayer\timidplayer.rc resource.o
if errorlevel 1 goto :eof
gcc -DUNICODE -D_UNICODE -O3 ..\src\common\*.c ..\src\timidity\*.c ..\src\timidplayer\*.c ..\src\wav_writer\*.c resource.o -s -static -Wl,-subsystem,windows -ladvapi32 -lcomctl32 -lcomdlg32 -lkernel32 -lole32 -lshell32 -lshlwapi -luser32 -lwinmm -o timidplayer.exe
if errorlevel 1 goto :eof
del resource.o
