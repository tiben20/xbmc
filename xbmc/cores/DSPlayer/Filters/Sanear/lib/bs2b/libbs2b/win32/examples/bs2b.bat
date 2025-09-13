@echo off

set BS2B="C:\Program Files\bs2b\bs2bconvert.exe"
set BS2BOPT=-l c

set BC=%BS2B% %BS2BOPT%

mkdir .\bs2b\
if not %ERRORLEVEL%==0 exit

for %%f in (*.wav) do (%BC% "%%f" ".\bs2b\%%f" || exit)
