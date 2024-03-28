#pragma once
/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
 *
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

#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

#include "utils/Geometry.h"
#include "settings/lib/SettingsManager.h"
#include "settings/lib/SettingDefinitions.h"
#include "Dshow.h"

enum DS_RENDER_LAYER
{
  RENDER_LAYER_ALL,
  RENDER_LAYER_UNDER,
  RENDER_LAYER_OVER
};

enum MADVR_GUI_SETTINGS
{
  KODIGUI_NEVER,
  KODIGUI_LOAD_DSPLAYER,
  KODIGUI_LOAD_MADVR
};

enum MADVR_D3D_MODE
{
  MADVR_D3D9,
  MADVR_D3D11_VSYNC,
  MADVR_D3D11_NOVSYNC,
};

enum MADVR_RES_SETTINGS
{
  MADVR_RES_SD = 480,
  MADVR_RES_720 = 720,
  MADVR_RES_1080 = 1080,
  MADVR_RES_2160 = 2160,
  MADVR_RES_ALL = 0,
  MADVR_RES_USER = 1,
  MADVR_RES_TVSHOW,
  MADVR_RES_ATSTART,
  MADVR_RES_DEFAULT
};

enum DIRECTSHOW_RENDERER
{
  DIRECTSHOW_RENDERER_VMR9 = 1,
  DIRECTSHOW_RENDERER_EVR = 2,
  DIRECTSHOW_RENDERER_MADVR = 3,
  DIRECTSHOW_RENDERER_UNDEF = 4
};

class IDSRendererAllocatorCallback
{
public:
  virtual ~IDSRendererAllocatorCallback() {};

  virtual CRect GetActiveVideoRect() { return CRect(0, 0, 0, 0); };
  virtual bool IsEnteringExclusive() { return false; };
  virtual void EnableExclusive(bool bEnable) {};
  virtual void SetPixelShader() {};
  virtual void SetResolution() const  {};
  virtual void SetPosition(CRect sourceRect, CRect videoRect, CRect viewRect) {};
  virtual bool ParentWindowProc(HWND hWnd, UINT uMsg, WPARAM *wParam, LPARAM *lParam, LRESULT *ret) const { return false; };
  virtual void Reset(bool bForceWindowed) {};
  virtual void DisplayChange(bool bExternalChange) {};
};

class IDSRendererPaintCallback
{
public:
  virtual ~IDSRendererPaintCallback() {};

  virtual void BeginRender() {};
  virtual void RenderToTexture(DS_RENDER_LAYER layer){};
  virtual void EndRender() {};
  virtual void IncRenderCount() {};
};

class IMadvrSettingCallback
{
public:
  virtual ~IMadvrSettingCallback() {};

  virtual void LoadSettings(int iSectionId) {};
  virtual void RestoreSettings() const {};
  virtual void GetProfileActiveName(const std::string &path, std::string *profile) {};
  virtual void OnSettingChanged(int iSectionId, CSettingsManager* settingsManager, const CSetting *setting) {};
  virtual void AddDependencies(const std::string &xml, CSettingsManager *settingsManager, CSetting *setting) {};
  virtual void ListSettings(const std::string &path) {};
};

class IDSPlayer
{
public:
  virtual ~IDSPlayer() {};

  virtual bool Configure(unsigned int width, unsigned int height, unsigned int d_width, unsigned int d_height, float fps, unsigned flags) { return false; };
  virtual bool UsingDS(DIRECTSHOW_RENDERER renderer = DIRECTSHOW_RENDERER_UNDEF) const { return false; };
  virtual bool ReadyDS(DIRECTSHOW_RENDERER renderer = DIRECTSHOW_RENDERER_UNDEF) { return false; };
  virtual void Register(IDSRendererAllocatorCallback* pAllocatorCallback) {};
  virtual void Register(IDSRendererPaintCallback* pPaintCallback) {};
  virtual void Register(IMadvrSettingCallback* pSettingCallback) {};
  virtual void Unregister(IDSRendererAllocatorCallback* pAllocatorCallback) {};
  virtual void Unregister(IDSRendererPaintCallback* pPaintCallback) {};
  virtual void Unregister(IMadvrSettingCallback* pSettingCallback) {};

  virtual int  GetEditionsCount() { return 0; };
  virtual int  GetEdition() { return -1; }
  virtual void GetEditionInfo(int iEdition, std::string &strEditionName, REFERENCE_TIME *prt) {};
  virtual void SetEdition(int iEdition) {};
  virtual bool IsMatroskaEditions() { return false; };
  virtual void ShowEditionDlg(bool playStart) {};
};