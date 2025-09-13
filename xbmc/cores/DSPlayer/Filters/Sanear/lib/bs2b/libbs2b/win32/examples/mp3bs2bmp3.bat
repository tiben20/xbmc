@echo off

set LAME="C:\Program Files\lame\lame.exe"
set BS2B="C:\Program Files\bs2b\bs2bconvert.exe"

set LAMEDECODEOPT=--decode
set LAMEENCODEOPT=--preset standard --tc bs2b
set BS2BOPT=-l c

set TW=tmpfile.wav
set TWB=tmpfile-bs2b.wav

set DC=%LAME% %LAMEDECODEOPT%
set EC=%LAME% %LAMEENCODEOPT%
set BC=%BS2B% %BS2BOPT%

if exist %TW%  del %TW%
if exist %TWB% del %TWB%

if exist *-!bs2b.mp3 del *-!bs2b.mp3

for %%f in (*.mp3) do (%DC% "%%f" %TW% || exit) & (%BC% %TW% %TWB% || exit) & (%EC% %TWB% "%%~nf-!bs2b.mp3" || exit) & del %TW% %TWB%
