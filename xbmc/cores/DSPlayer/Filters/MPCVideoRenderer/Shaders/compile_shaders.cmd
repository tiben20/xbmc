@ECHO OFF
REM (C) 2018-2023 see Authors.txt
REM
REM This file is part of MPC-BE.
REM
REM MPC-BE is free software; you can redistribute it and/or modify
REM it under the terms of the GNU General Public License as published by
REM the Free Software Foundation; either version 3 of the License, or
REM (at your option) any later version.
REM
REM MPC-BE is distributed in the hope that it will be useful,
REM but WITHOUT ANY WARRANTY; without even the implied warranty of
REM MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
REM GNU General Public License for more details.
REM
REM You should have received a copy of the GNU General Public License
REM along with this program.  If not, see <http://www.gnu.org/licenses/>.

PUSHD %~dp0

IF /I %PROCESSOR_ARCHITECTURE%==AMD64 (
  SET fxcfolder=x64
) ELSE (
  SET fxcfolder=x86
)

SET fxcexe="%ProgramFiles(x86)%\Windows Kits\10\bin\10.0.19041.0\%fxcfolder%\fxc.exe"
IF EXIST %fxcexe% GOTO fxc_OK
SET fxcexe="%ProgramFiles(x86)%\Windows Kits\10\bin\10.0.18362.0\%fxcfolder%\fxc.exe"
IF EXIST %fxcexe% GOTO fxc_OK
SET fxcexe="%ProgramFiles(x86)%\Windows Kits\10\bin\10.0.17763.0\%fxcfolder%\fxc.exe"
IF EXIST %fxcexe% GOTO fxc_OK
SET fxcexe="%ProgramFiles(x86)%\Windows Kits\10\bin\%fxcfolder%\fxc.exe"
IF EXIST %fxcexe% GOTO fxc_OK
SET fxcexe="%ProgramFiles(x86)%\Windows Kits\8.1\bin\%fxcfolder%\fxc.exe"
IF EXIST %fxcexe% GOTO fxc_OK

SET fxcexe="fxc.exe"
where /q %fxcexe%
IF %ERRORLEVEL% EQU 0 goto fxc_Ok

CALL :SubColorText "0C" "fxc.exe not found!" & ECHO.
EXIT /B

:fxc_OK

SET workdir=..\_bin\shaders
IF NOT EXIST "%workdir%\" MKDIR "%workdir%"

CALL :SubColorText "0A" "=== Compiling D3D9 shaders ===" & ECHO.

SET fxc_ps3=%fxcexe% /nologo /O2 /T ps_3_0

%fxc_ps3% /Fo "%workdir%\resizer_mitchell4_x.cso"        "d3d9\interpolation_spline4.hlsl" /DMETHOD=0 /DAXIS=0
%fxc_ps3% /Fo "%workdir%\resizer_mitchell4_y.cso"        "d3d9\interpolation_spline4.hlsl" /DMETHOD=0 /DAXIS=1
%fxc_ps3% /Fo "%workdir%\resizer_catmull4_x.cso"         "d3d9\interpolation_spline4.hlsl" /DMETHOD=1 /DAXIS=0
%fxc_ps3% /Fo "%workdir%\resizer_catmull4_y.cso"         "d3d9\interpolation_spline4.hlsl" /DMETHOD=1 /DAXIS=1
%fxc_ps3% /Fo "%workdir%\resizer_lanczos2_x.cso"         "d3d9\interpolation_lanczos2.hlsl" /DAXIS=0
%fxc_ps3% /Fo "%workdir%\resizer_lanczos2_y.cso"         "d3d9\interpolation_lanczos2.hlsl" /DAXIS=1
%fxc_ps3% /Fo "%workdir%\resizer_lanczos3_x.cso"         "d3d9\interpolation_lanczos3.hlsl" /DAXIS=0
%fxc_ps3% /Fo "%workdir%\resizer_lanczos3_y.cso"         "d3d9\interpolation_lanczos3.hlsl" /DAXIS=1

%fxc_ps3% /Fo "%workdir%\downscaler_box_x.cso"           "d3d9\convolution.hlsl" /DFILTER=0 /DAXIS=0
%fxc_ps3% /Fo "%workdir%\downscaler_box_y.cso"           "d3d9\convolution.hlsl" /DFILTER=0 /DAXIS=1
%fxc_ps3% /Fo "%workdir%\downscaler_bilinear_x.cso"      "d3d9\convolution.hlsl" /DFILTER=1 /DAXIS=0
%fxc_ps3% /Fo "%workdir%\downscaler_bilinear_y.cso"      "d3d9\convolution.hlsl" /DFILTER=1 /DAXIS=1
%fxc_ps3% /Fo "%workdir%\downscaler_hamming_x.cso"       "d3d9\convolution.hlsl" /DFILTER=2 /DAXIS=0
%fxc_ps3% /Fo "%workdir%\downscaler_hamming_y.cso"       "d3d9\convolution.hlsl" /DFILTER=2 /DAXIS=1
%fxc_ps3% /Fo "%workdir%\downscaler_bicubic05_x.cso"     "d3d9\convolution.hlsl" /DFILTER=3 /DAXIS=0 /DA=-0.5
%fxc_ps3% /Fo "%workdir%\downscaler_bicubic05_y.cso"     "d3d9\convolution.hlsl" /DFILTER=3 /DAXIS=1 /DA=-0.5
%fxc_ps3% /Fo "%workdir%\downscaler_bicubic15_x.cso"     "d3d9\convolution.hlsl" /DFILTER=3 /DAXIS=0 /DA=-1.5
%fxc_ps3% /Fo "%workdir%\downscaler_bicubic15_y.cso"     "d3d9\convolution.hlsl" /DFILTER=3 /DAXIS=1 /DA=-1.5
%fxc_ps3% /Fo "%workdir%\downscaler_lanczos_x.cso"       "d3d9\convolution.hlsl" /DFILTER=4 /DAXIS=0
%fxc_ps3% /Fo "%workdir%\downscaler_lanczos_y.cso"       "d3d9\convolution.hlsl" /DFILTER=4 /DAXIS=1

%fxc_ps3% /Fo "%workdir%\convert_yuy2.cso"               "d3d9\convert_color.hlsl" /DC_YUY2=3

CALL :SubColorText "0A" "=== Compiling D3D11 shaders ===" & ECHO.

SET fxc_ps4=%fxcexe% /nologo /O2 /T ps_4_0

%fxc_ps4% /Fo "%workdir%\ps_resizer_mitchell4_x.cso"     "d3d11\ps_interpolation_spline4.hlsl" /DMETHOD=0 /DAXIS=0
%fxc_ps4% /Fo "%workdir%\ps_resizer_mitchell4_y.cso"     "d3d11\ps_interpolation_spline4.hlsl" /DMETHOD=0 /DAXIS=1
%fxc_ps4% /Fo "%workdir%\ps_resizer_catmull4_x.cso"      "d3d11\ps_interpolation_spline4.hlsl" /DMETHOD=1 /DAXIS=0
%fxc_ps4% /Fo "%workdir%\ps_resizer_catmull4_y.cso"      "d3d11\ps_interpolation_spline4.hlsl" /DMETHOD=1 /DAXIS=1
%fxc_ps4% /Fo "%workdir%\ps_resizer_lanczos2_x.cso"      "d3d11\ps_interpolation_lanczos2.hlsl" /DAXIS=0
%fxc_ps4% /Fo "%workdir%\ps_resizer_lanczos2_y.cso"      "d3d11\ps_interpolation_lanczos2.hlsl" /DAXIS=1
%fxc_ps4% /Fo "%workdir%\ps_resizer_lanczos3_x.cso"      "d3d11\ps_interpolation_lanczos3.hlsl" /DAXIS=0
%fxc_ps4% /Fo "%workdir%\ps_resizer_lanczos3_y.cso"      "d3d11\ps_interpolation_lanczos3.hlsl" /DAXIS=1

%fxc_ps4% /Fo "%workdir%\ps_downscaler_box_x.cso"        "d3d11\ps_convolution.hlsl" /DFILTER=0 /DAXIS=0
%fxc_ps4% /Fo "%workdir%\ps_downscaler_box_y.cso"        "d3d11\ps_convolution.hlsl" /DFILTER=0 /DAXIS=1
%fxc_ps4% /Fo "%workdir%\ps_downscaler_bilinear_x.cso"   "d3d11\ps_convolution.hlsl" /DFILTER=1 /DAXIS=0
%fxc_ps4% /Fo "%workdir%\ps_downscaler_bilinear_y.cso"   "d3d11\ps_convolution.hlsl" /DFILTER=1 /DAXIS=1
%fxc_ps4% /Fo "%workdir%\ps_downscaler_hamming_x.cso"    "d3d11\ps_convolution.hlsl" /DFILTER=2 /DAXIS=0
%fxc_ps4% /Fo "%workdir%\ps_downscaler_hamming_y.cso"    "d3d11\ps_convolution.hlsl" /DFILTER=2 /DAXIS=1
%fxc_ps4% /Fo "%workdir%\ps_downscaler_bicubic05_x.cso"  "d3d11\ps_convolution.hlsl" /DFILTER=3 /DAXIS=0 /DA=-0.5
%fxc_ps4% /Fo "%workdir%\ps_downscaler_bicubic05_y.cso"  "d3d11\ps_convolution.hlsl" /DFILTER=3 /DAXIS=1 /DA=-0.5
%fxc_ps4% /Fo "%workdir%\ps_downscaler_bicubic15_x.cso"  "d3d11\ps_convolution.hlsl" /DFILTER=3 /DAXIS=0 /DA=-1.5
%fxc_ps4% /Fo "%workdir%\ps_downscaler_bicubic15_y.cso"  "d3d11\ps_convolution.hlsl" /DFILTER=3 /DAXIS=1 /DA=-1.5
%fxc_ps4% /Fo "%workdir%\ps_downscaler_lanczos_x.cso"    "d3d11\ps_convolution.hlsl" /DFILTER=4 /DAXIS=0
%fxc_ps4% /Fo "%workdir%\ps_downscaler_lanczos_y.cso"    "d3d11\ps_convolution.hlsl" /DFILTER=4 /DAXIS=1

%fxc_ps4% /Fo "%workdir%\ps_convert_yuy2.cso"            "d3d11\ps_convert_color.hlsl" /DC_YUY2=3

%fxc_ps4% /Fo "%workdir%\ps_final_pass_10.cso"           "d3d11\ps_final_pass.hlsl" /DQUANTIZATION=1023

%fxc_ps4% /Fo "%workdir%\ps_convert_bitmap_to_pq1.cso"   "d3d11\ps_convert_bitmap_to_pq.hlsl" /DSDR_PEAK_LUM=50
%fxc_ps4% /Fo "%workdir%\ps_convert_bitmap_to_pq2.cso"   "d3d11\ps_convert_bitmap_to_pq.hlsl" /DSDR_PEAK_LUM=30

EXIT /B

:SubColorText
FOR /F "tokens=1,2 delims=#" %%A IN (
  '"PROMPT #$H#$E# & ECHO ON & FOR %%B IN (1) DO REM"') DO (
  SET "DEL=%%A")
<NUL SET /p ".=%DEL%" > "%~2"
FINDSTR /v /a:%1 /R ".18" "%~2" NUL
DEL "%~2" > NUL 2>&1
EXIT /B
