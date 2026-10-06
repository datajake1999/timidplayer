@echo off
call "C:\Program Files\Microsoft Visual Studio 8\VC\bin\vcvars32.bat"
call "C:\Program Files\Microsoft Platform SDK\SetEnv.Cmd" /SRV32 /RETAIL
md X86
rc src\timidplayer\timidplayer.rc
if errorlevel 1 goto :eof
cl /nologo -c /D "UNICODE" /D "_UNICODE" /I "src\include\msinttypes" /O2 src\common\*.c src\timidity\*.c src\timidplayer\*.c src\wav_writer\*.c
if errorlevel 1 goto :eof
link *.obj src\timidplayer\timidplayer.res advapi32.lib bufferoverflowU.lib comctl32.lib comdlg32.lib kernel32.lib ole32.lib shell32.lib shlwapi.lib user32.lib winmm.lib /nologo /OUT:X86\timidplayer.exe
if errorlevel 1 goto :eof
del *.obj
call "C:\Program Files\Microsoft Platform SDK\SetEnv.Cmd" /X64 /RETAIL
md X64
cl /nologo -c /D "UNICODE" /D "_UNICODE" /I "src\include\msinttypes" /O2 src\common\*.c src\timidity\*.c src\timidplayer\*.c src\wav_writer\*.c
if errorlevel 1 goto :eof
link *.obj src\timidplayer\timidplayer.res advapi32.lib bufferoverflowU.lib comctl32.lib comdlg32.lib kernel32.lib ole32.lib shell32.lib shlwapi.lib user32.lib winmm.lib /nologo /OUT:X64\timidplayer.exe
if errorlevel 1 goto :eof
del *.obj
call "C:\Program Files\Microsoft Platform SDK\SetEnv.Cmd" /SRV64 /RETAIL
md IA64
cl /nologo -c /D "UNICODE" /D "_UNICODE" /I "src\include\msinttypes" /O2 src\common\*.c src\timidity\*.c src\timidplayer\*.c src\wav_writer\*.c
if errorlevel 1 goto :eof
link *.obj src\timidplayer\timidplayer.res advapi32.lib bufferoverflowU.lib comctl32.lib comdlg32.lib kernel32.lib ole32.lib shell32.lib shlwapi.lib user32.lib winmm.lib /nologo /OUT:IA64\timidplayer.exe
if errorlevel 1 goto :eof
del *.obj
del src\timidplayer\timidplayer.res
