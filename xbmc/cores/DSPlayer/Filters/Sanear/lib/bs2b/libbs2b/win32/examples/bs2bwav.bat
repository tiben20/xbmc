@echo off

rem
rem 'C:\Program Files\UnxUtils\find.exe' is a GNU 'find'.
rem You can get ported to Windows GNU utils from:
rem http://sourceforge.net/projects/unxutils/
rem

set TMPDR=C:\temp\bs2b

set BS2B=C:\"\Program Files\"\bs2b\bs2bconvert.exe
set BS2BOPT=-l c

set UFIND=C:\Program Files\UnxUtils\find.exe
set UFINDOPT=-name *.wav -exec cmd /c %BS2B% %BS2BOPT% \"{}\" \"%TMPDR%\{}\" ;


if exist "%TMPDR%" echo '%TMPDR%' exist! & echo Press Ctr+C to cancel. & pause

"%UFIND%" -type d -exec cmd /c mkdir \"%TMPDR%\{}\" ;
"%UFIND%" %UFINDOPT%
