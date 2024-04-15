/*
 *  Copyright (C) 2003-2006 Gabest
 *  http://www.gabest.org
 *
 *  Copyright (C) 2005-2010 Team XBMC
 *  http://www.xbmc.org
 *
 *  Copyright (C) 2010-2013 Eduard Kytmanov
 *  http://www.avmedia.su
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
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#if HAS_DS_PLAYER

#include "DSUtil/DSUtil.h"
#include "AllocatorCommon.h"
#include "madVRAllocatorPresenter.h"
#include "mpcvideorenderer/VideoRenderer.h"
#include "utils/log.h"

HRESULT CreateMadVR(const CLSID& clsid, HWND hWnd, ISubPicAllocatorPresenter** ppAP)
{
  HRESULT    hr = E_FAIL;
  if (clsid == CLSID_madVRAllocatorPresenter)
  {
    std::string Error;
    *ppAP  = DNew CmadVRAllocatorPresenter(hWnd, hr, Error);
    (*ppAP)->AddRef();

    if(FAILED(hr))
    {
      Error += "\n";
      Error += GetWindowsErrorMessage(hr, NULL);
      CLog::Log(LOGERROR, "{} {}", __FUNCTION__, Error.c_str());
      (*ppAP)->Release();
      *ppAP = NULL;
    }
    else if (!Error.empty())
    {
      CLog::Log(LOGWARNING, "{} {}", __FUNCTION__, Error.c_str());
    }
  }

  return hr;
}

HRESULT CreateMPCVideoRenderer(const CLSID& clsid, HWND hWnd, IBaseFilter** ppAP)
{
  HRESULT    hr = S_OK;
  if (clsid == __uuidof(CMpcVideoRenderer))
  {
    std::string Error;
    *ppAP = DNew CMpcVideoRenderer(nullptr, &hr);
    
    (*ppAP)->AddRef();

    if (FAILED(hr))
    {
      Error += "\n";
      Error += GetWindowsErrorMessage(hr, NULL);
      CLog::Log(LOGERROR, "%s %s", __FUNCTION__, Error.c_str());
      (*ppAP)->Release();
      *ppAP = NULL;
    }
    else if (!Error.empty())
    {
      CLog::Log(LOGWARNING, "%s %s", __FUNCTION__, Error.c_str());
    }
  }

  return hr;
}

std::string GetWindowsErrorMessage(HRESULT _Error, HMODULE _Module)
{

  switch (_Error)
  {
  case D3DERR_WRONGTEXTUREFORMAT: return "D3DERR_WRONGTEXTUREFORMAT";
  case D3DERR_UNSUPPORTEDCOLOROPERATION: return "D3DERR_UNSUPPORTEDCOLOROPERATION";
  case D3DERR_UNSUPPORTEDCOLORARG: return "D3DERR_UNSUPPORTEDCOLORARG";
  case D3DERR_UNSUPPORTEDALPHAOPERATION: return "D3DERR_UNSUPPORTEDALPHAOPERATION";
  case D3DERR_UNSUPPORTEDALPHAARG: return "D3DERR_UNSUPPORTEDALPHAARG";
  case D3DERR_TOOMANYOPERATIONS: return "D3DERR_TOOMANYOPERATIONS";
  case D3DERR_CONFLICTINGTEXTUREFILTER: return "D3DERR_CONFLICTINGTEXTUREFILTER";
  case D3DERR_UNSUPPORTEDFACTORVALUE: return "D3DERR_UNSUPPORTEDFACTORVALUE";
  case D3DERR_CONFLICTINGRENDERSTATE: return "D3DERR_CONFLICTINGRENDERSTATE";
  case D3DERR_UNSUPPORTEDTEXTUREFILTER: return "D3DERR_UNSUPPORTEDTEXTUREFILTER";
  case D3DERR_CONFLICTINGTEXTUREPALETTE: return "D3DERR_CONFLICTINGTEXTUREPALETTE";
  case D3DERR_DRIVERINTERNALERROR: return "D3DERR_DRIVERINTERNALERROR";
  case D3DERR_NOTFOUND: return "D3DERR_NOTFOUND";
  case D3DERR_MOREDATA: return "D3DERR_MOREDATA";
  case D3DERR_DEVICELOST: return "D3DERR_DEVICELOST";
  case D3DERR_DEVICENOTRESET: return "D3DERR_DEVICENOTRESET";
  case D3DERR_NOTAVAILABLE: return "D3DERR_NOTAVAILABLE";
  case D3DERR_OUTOFVIDEOMEMORY: return "D3DERR_OUTOFVIDEOMEMORY";
  case D3DERR_INVALIDDEVICE: return "D3DERR_INVALIDDEVICE";
  case D3DERR_INVALIDCALL: return "D3DERR_INVALIDCAL";
  case D3DERR_DRIVERINVALIDCALL: return "D3DERR_DRIVERINVALIDCAL";
  case D3DERR_WASSTILLDRAWING: return "D3DERR_WASSTILLDRAWING";
  case D3DOK_NOAUTOGEN: return "D3DOK_NOAUTOGEN";
  case D3DERR_DEVICEREMOVED: return "D3DERR_DEVICEREMOVED";
  case S_NOT_RESIDENT: return "S_NOT_RESIDENT";
  case S_RESIDENT_IN_SHARED_MEMORY: return "S_RESIDENT_IN_SHARED_MEMORY";
  case S_PRESENT_MODE_CHANGED: return "S_PRESENT_MODE_CHANGED";
  case S_PRESENT_OCCLUDED: return "S_PRESENT_OCCLUDED";
  case D3DERR_DEVICEHUNG: return "D3DERR_DEVICEHUNG";
  }

  std::string errmsg;
  LPVOID lpMsgBuf;
  if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
    _Module, _Error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPCH)&lpMsgBuf, 0, NULL))
  {
    errmsg = (LPCH)lpMsgBuf;
    LocalFree(lpMsgBuf);
  }
  std::string Temp;
  Temp = StringUtils::Format("0x%08x ", _Error);
  return Temp + errmsg;
}

const char *GetD3DFormatStr(D3DFORMAT Format)
{
  switch (Format)
  {
  case D3DFMT_R8G8B8: return "R8G8B8";
  case D3DFMT_A8R8G8B8: return "A8R8G8B8";
  case D3DFMT_X8R8G8B8: return "X8R8G8B8";
  case D3DFMT_R5G6B5: return "R5G6B5";
  case D3DFMT_X1R5G5B5: return "X1R5G5B5";
  case D3DFMT_A1R5G5B5: return "A1R5G5B5";
  case D3DFMT_A4R4G4B4: return "A4R4G4B4";
  case D3DFMT_R3G3B2: return "R3G3B2";
  case D3DFMT_A8: return "A8";
  case D3DFMT_A8R3G3B2: return "A8R3G3B2";
  case D3DFMT_X4R4G4B4: return "X4R4G4B4";
  case D3DFMT_A2B10G10R10: return "A2B10G10R10";
  case D3DFMT_A8B8G8R8: return "A8B8G8R8";
  case D3DFMT_X8B8G8R8: return "X8B8G8R8";
  case D3DFMT_G16R16: return "G16R16";
  case D3DFMT_A2R10G10B10: return "A2R10G10B10";
  case D3DFMT_A16B16G16R16: return "A16B16G16R16";
  case D3DFMT_A8P8: return "A8P8";
  case D3DFMT_P8: return "P8";
  case D3DFMT_L8: return "L8";
  case D3DFMT_A8L8: return "A8L8";
  case D3DFMT_A4L4: return "A4L4";
  case D3DFMT_V8U8: return "V8U8";
  case D3DFMT_L6V5U5: return "L6V5U5";
  case D3DFMT_X8L8V8U8: return "X8L8V8U8";
  case D3DFMT_Q8W8V8U8: return "Q8W8V8U8";
  case D3DFMT_V16U16: return "V16U16";
  case D3DFMT_A2W10V10U10: return "A2W10V10U10";
  case D3DFMT_UYVY: return "UYVY";
  case D3DFMT_R8G8_B8G8: return "R8G8_B8G8";
  case D3DFMT_YUY2: return "YUY2";
  case D3DFMT_G8R8_G8B8: return "G8R8_G8B8";
  case D3DFMT_DXT1: return "DXT1";
  case D3DFMT_DXT2: return "DXT2";
  case D3DFMT_DXT3: return "DXT3";
  case D3DFMT_DXT4: return "DXT4";
  case D3DFMT_DXT5: return "DXT5";
  case D3DFMT_D16_LOCKABLE: return "D16_LOCKABLE";
  case D3DFMT_D32: return "D32";
  case D3DFMT_D15S1: return "D15S1";
  case D3DFMT_D24S8: return "D24S8";
  case D3DFMT_D24X8: return "D24X8";
  case D3DFMT_D24X4S4: return "D24X4S4";
  case D3DFMT_D16: return "D16";
  case D3DFMT_D32F_LOCKABLE: return "D32F_LOCKABLE";
  case D3DFMT_D24FS8: return "D24FS8";
  case D3DFMT_D32_LOCKABLE: return "D32_LOCKABLE";
  case D3DFMT_S8_LOCKABLE: return "S8_LOCKABLE";
  case D3DFMT_L16: return "L16";
  case D3DFMT_VERTEXDATA: return "VERTEXDATA";
  case D3DFMT_INDEX16: return "INDEX16";
  case D3DFMT_INDEX32: return "INDEX32";
  case D3DFMT_Q16W16V16U16: return "Q16W16V16U16";
  case D3DFMT_MULTI2_ARGB8: return "MULTI2_ARGB8";
  case D3DFMT_R16F: return "R16F";
  case D3DFMT_G16R16F: return "G16R16F";
  case D3DFMT_A16B16G16R16F: return "A16B16G16R16F";
  case D3DFMT_R32F: return "R32F";
  case D3DFMT_G32R32F: return "G32R32F";
  case D3DFMT_A32B32G32R32F: return "A32B32G32R32F";
  case D3DFMT_CxV8U8: return "CxV8U8";
  case D3DFMT_A1: return "A1";
  case D3DFMT_BINARYBUFFER: return "BINARYBUFFER";
  }
  return "Unknown";
}

#endif