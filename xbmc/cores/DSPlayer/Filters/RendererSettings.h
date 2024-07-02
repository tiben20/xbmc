/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
 *
 *		Copyright (C) 2010-2013 Eduard Kytmanov
 *		http://www.avmedia.su
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once
#ifndef RENDERERSETTINGS_H
#define RENDERERSETTINGS_H

#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

#include "..\Subtitles\libsubs\ISubManager.h"
#include <memory>


class CPixelShaderList;

enum :int {
  TEXFMT_AUTOINT = 0,
  TEXFMT_8INT = 8,
  TEXFMT_10INT = 10,
  TEXFMT_16FLOAT = 16,
};

enum :int {
  SUPERRES_Disable = 0,
  SUPERRES_SD,
  SUPERRES_720p,
  SUPERRES_1080p,
  SUPERRES_1440p,
  SUPERRES_COUNT
};

enum :int {
  CHROMA_Nearest = 0,
  CHROMA_Bilinear,
  CHROMA_CatmullRom,
  CHROMA_COUNT
};

enum :int {
  UPSCALE_Nearest = 0,
  UPSCALE_Mitchell,
  UPSCALE_CatmullRom,
  UPSCALE_Lanczos2,
  UPSCALE_Lanczos3,
  UPSCALE_Jinc2,
  UPSCALE_COUNT
};

enum :int {
  DOWNSCALE_Box = 0,
  DOWNSCALE_Bilinear,
  DOWNSCALE_Hamming,
  DOWNSCALE_Bicubic,
  DOWNSCALE_BicubicSharp,
  DOWNSCALE_Lanczos,
  DOWNSCALE_COUNT
};

enum :int {
  SWAPEFFECT_Discard = 0,
  SWAPEFFECT_Flip,
  SWAPEFFECT_COUNT
};

enum :int {
  HDRTD_Disabled = 0,
  HDRTD_On_Fullscreen,
  HDRTD_On,
  HDRTD_OnOff_Fullscreen,
  HDRTD_OnOff
};

struct VPEnableFormats_t {
  bool bNV12;
  bool bP01x;
  bool bYUY2;
  bool bOther;
};

enum AP_SURFACE_USAGE
{
  VIDRNDT_AP_SURFACE,
  VIDRNDT_AP_TEXTURE2D,
  VIDRNDT_AP_TEXTURE3D,
};

enum DS_STATS
{
  DS_STATS_NONE = 0,
  DS_STATS_1 = 3,
  DS_STATS_2 = 2,
  DS_STATS_3 = 1
};

enum LIBPLACEBO_SHADERS
{
  PLACEBO_DEFAULT = 0,
  PLACEBO_FAST = 1,
  PLACEBO_HIGH = 2,
  PLACEBO_CUSTOM = 3
};

enum D3D11_TEXTURE_SAMPLER
{
  D3D11_VP = 0,
  D3D11_LIBPLACEBO = 1,
  D3D11_INTERNAL_SHADERS =2
};

class CRendererSettings
{
public:
  CRendererSettings()
  {
    SetDefault();
  }
  virtual void SetDefault()
  {
    apSurfaceUsage = VIDRNDT_AP_TEXTURE3D; // Fixed setting
    vSyncOffset = 0;
    vSyncAccurate = true;

    fullscreenGUISupport = false;
    alterativeVSync = false;
    vSync = true;
    disableDesktopComposition = false;

    flushGPUBeforeVSync = true; //Flush GPU before VSync
    flushGPUAfterPresent = false; //Flush GPU after Present
    flushGPUWait = false; //Wait for flushes

    d3dFullscreen = true;

	bAllowFullscreen = true;
  }

public:
  
  bool alterativeVSync;
  int vSyncOffset;
  bool vSyncAccurate;
  bool fullscreenGUISupport; // TODO: Not sure if it's really needed
  bool vSync;
  bool disableDesktopComposition;
  bool flushGPUBeforeVSync;
  bool flushGPUAfterPresent;
  bool flushGPUWait;
  AP_SURFACE_USAGE apSurfaceUsage;
  bool d3dFullscreen;
  bool bAllowFullscreen;
  SSubSettings subtitlesSettings;
};

class CMPCVRSettings : public CRendererSettings
{
public:
  CMPCVRSettings()
  {
    SetDefault();
  }
  void SetDefault()
  {
    CRendererSettings::SetDefault();

    displayStats = DS_STATS_1; // On GUI
    m_pPlaceboOptions = PLACEBO_DEFAULT;
    bD3D11TextureSampler = D3D11_INTERNAL_SHADERS;// D3D11_INTERNAL_SHADERS;//D3D11_VP
    bUseHDR = false;
    enableFrameTimeCorrection = false;
    iBuffers = 4;

    bVPUseRTXVideoHDR = false;
    bVPUseSuperRes = false;
    bUseHDR = true;
    bHdrPassthrough = true;
  }

public:
  D3D11_TEXTURE_SAMPLER bD3D11TextureSampler;// which plane merger we use
  bool                  bVPUseRTXVideoHDR;   // if d3d11 vp can use rtx hdr rescaler
  bool                  bVPUseSuperRes;   // if d3d11 vp can use rtx hdr rescaler
  bool                  bUseHDR;
  bool                  bHdrPassthrough;     //if we passthrough the hdr metadata
  bool                  enableFrameTimeCorrection;//not implemented
  int                   iBuffers;
  //libplacebo options
  LIBPLACEBO_SHADERS m_pPlaceboOptions;
  DS_STATS displayStats;
  //to add in gui settings
  int  iResizeStats = 0;
  int  iTexFormat = TEXFMT_AUTOINT;
  
  bool bDeintDouble = true;
  bool bVPScaling = true;
  int  iChromaScaling = CHROMA_Bilinear;
  int  iUpscaling = UPSCALE_CatmullRom; // interpolation
  int  iDownscaling = DOWNSCALE_Hamming;  // convolution
  bool bInterpolateAt50pct = true;
  bool bUseDither = true;
  bool bDeintBlend = false;
  int  iSwapEffect = SWAPEFFECT_Flip;
  bool bVBlankBeforePresent = false;
  bool bHdrPreferDoVi = false;

  int  iHdrToggleDisplay = HDRTD_On;
  int  iHdrOsdBrightness = 0;
  bool bConvertToSdr = true;
};

class CMADVRRendererSettings : public CRendererSettings
{
public:
  CMADVRRendererSettings()
  {
    SetDefault();
  }
  void SetDefault()
  {
    CRendererSettings::SetDefault();
  };

public:

};

class CDSSettings
{
public:
  std::wstring    m_strD3DX9Version;
  HINSTANCE      m_hD3DX9Dll;
  int            m_nDXSdkRelease;
  std::wstring   D3D9RenderDevice;

  CRendererSettings* pRendererSettings;
  std::unique_ptr<CPixelShaderList> pixelShaderList;

  //TODO
  bool  IsD3DFullscreen() {return false;};

public:
  CDSSettings(void);
  virtual ~CDSSettings(void);
  void Initialize(std::string renderer);

  void LoadConfig();

  HINSTANCE GetD3X9Dll();
  int GetDXSdkRelease() { return m_nDXSdkRelease; };

  HRESULT (__stdcall * m_pDwmIsCompositionEnabled)(__out BOOL* pfEnabled);
  HRESULT (__stdcall * m_pDwmEnableComposition)(UINT uCompositionAction);
  HMODULE m_hDWMAPI;

  bool isEVR;
};
extern class CDSSettings g_dsSettings;
extern bool g_bNoDuration;
extern bool g_bExternalSubtitleTime;

#endif