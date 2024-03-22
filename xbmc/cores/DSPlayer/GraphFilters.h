#pragma once
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

#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

#include "DSUtil/DSUtil.h"
#include "DSUtil/SmartPtr.h"
#include "streams.h"
#include "utils/CharsetConverter.h"
#include "Filters/Sanear/Interfaces.h"
#include "settings/lib/SettingDefinitions.h"
#include "D3d9.h"

static const std::string XYSUBFILTER_FILTERSTR = "XySubFilter";
static const std::string MADVR_FILTERSTR = "madVR";

/// Informations about a filter
struct SFilterInfos
{
  SFilterInfos()
  {
    Clear();
  }

  ~SFilterInfos()
  {
    if (isinternal != true)
      SAFE_DELETE(pData);
  }

  void Clear()
  {
    pBF = NULL;
    osdname = "";
    guid = GUID_NULL;
    isinternal = false;
    internalFilter = false;
    pData = NULL;
  }

  void SetFilterInfo(IBaseFilter * pBF)
  {
    std::string filterName;
    g_charsetConverter.wToUTF8(GetFilterName(pBF), filterName);
    osdname = filterName;
    guid = GetCLSID(pBF);
  }

  Com::SmartPtr<IBaseFilter> pBF; ///< Pointer to the IBaseFilter interface. May be NULL
  std::string osdname; ///< OSD Name of the filter
  GUID guid; ///< GUID of the filter
  bool isinternal; ///<  Releasing is not done the same way for internal filters
  bool internalFilter = false;
  void *pData; ///< If the filter is internal, there may be some additionnal data
};

/// Specific informations about the video renderer filter
struct SVideoRendererFilterInfos : SFilterInfos
{
  SVideoRendererFilterInfos()
    : SFilterInfos()
  {
    Clear();
  }

  void Clear()
  {
    pQualProp = NULL;
    __super::Clear();
  }
  Com::SmartPtr<IQualProp> pQualProp; ///< Pointer to IQualProp interface. May be NULL if the video renderer doesn't implement IQualProp
};

/// Informations about DVD filters
struct SDVDFilters
{
  SDVDFilters()
  {
    Clear();
  }

  void Clear()
  {
    dvdControl.Release();
    dvdInfo.Release();
  }

  Com::SmartQIPtr<IDvdControl2> dvdControl; ///< Pointer to IDvdControl2 interface. May be NULL
  Com::SmartQIPtr<IDvdInfo2> dvdInfo; ///< Pointer to IDvdInfo2 interface. May be NULL
};

enum FILTERSMAN_TYPE
{
  INTERNALFILTERS,
  MEDIARULES,
  DSMERITS,
  NOFILTERMAN
};

/** @brief Centralize graph filters management

  Our graph can contains Sources, Splitters, Audio renderers, Audio decoders, Video decoders and Extras filters. This singleton class centralize all the data related to these filters.
  */
class CGraphFilters
{
public:
  static const std::string INTERNAL_LAVVIDEO;
  static const std::string INTERNAL_LAVAUDIO;
  static const std::string INTERNAL_LAVSPLITTER;
  static const std::string INTERNAL_XYVSFILTER;
  static const std::string INTERNAL_XYSUBFILTER;
  static const std::string INTERNAL_SANEAR;
  static const std::string MADSHI_VIDEO_RENDERER;

  /// Retrieve singleton instance
  static CGraphFilters* Get();
  /// Destroy singleton instance
  static void Destroy()
  {
    delete m_pSingleton;
    m_pSingleton = NULL;
  }

  /**
   * Informations about the source filter
   * @note It may no have a source filter in the graph, because splitters are also source filters. A source filter is only needed when playing from internet, RAR, ... but not for media file
   **/
  SFilterInfos Source;
  ///Informations about the splitter filter
  SFilterInfos Splitter;
  ///Informations about the video decoder
  SFilterInfos Video;
  ///Informations about the audio decoder
  SFilterInfos Audio;
  ///Informations about the video decoder
  SFilterInfos Subs;
  ///Informations about the audio renderer
  SFilterInfos AudioRenderer;
  ///Informations about the video renderer
  SVideoRendererFilterInfos VideoRenderer;
  ///Informations about extras filters
  std::vector<SFilterInfos> Extras;
  /**
   * Informations about the DVD filters
   * @note The structure is not filled is the current media file isn't a DVD
   */
  SDVDFilters DVD;

  /// @return True if using DXVA, false otherwise
  bool IsUsingDXVADecoder() { return m_UsingDXVADecoder; }

  /// @return True if we are playing a DVD, false otherwise
  bool IsDVD() { return m_isDVD; }

  void SetIsUsingDXVADecoder(bool val) { m_UsingDXVADecoder = val; }
  void SetIsDVD(bool val) { m_isDVD = val; }

  // Internal Filters
  void ShowInternalPPage(const std::string &type, bool showPropertyPage);
  bool ShowOSDPPage(IBaseFilter *pBF);
  void CreateInternalFilter(const std::string &type, IBaseFilter **ppBF);
  void GetInternalFilter(const std::string &type, IBaseFilter **ppBF);
  std::string GetInternalType(IBaseFilter *pBF);
  void SetupLavSettings(const std::string &type, IBaseFilter *pBF);
  bool SetLavInternal(const std::string &type, IBaseFilter *pBF);
  bool GetLavSettings(const std::string &type, IBaseFilter *pBF);
  bool SetLavSettings(const std::string &type, IBaseFilter *pBF);
  bool LoadLavSettings(const std::string &type);
  bool SaveLavSettings(const std::string &type);
  void EraseLavSetting(const std::string &type);
  void GetActiveDecoder(std::pair<std::string, bool> &activeDecoder);
  void GetHWDeviceList(DWORD dwHWAccel, DynamicIntegerSettingOptions &list) { list = m_mapHWAccelDeviceInfo[dwHWAccel];};

  void SetSanearSettings();

  void SetKodiRealFS(bool b) { m_isKodiRealFS = b; }
  void SetAuxAudioDelay();
  bool GetAuxAudioDelay() { return m_auxAudioDelay; }

  bool IsDialogProcessInfo() { return m_bDialogProcessInfo; };
  void SetDialogProcessInfo(bool dialogProcessInfo) { m_bDialogProcessInfo = dialogProcessInfo; };

  bool UsingMediaPortalTsReader() 
  { 
    return ((Splitter.guid != GUID_NULL) && !(StringFromGUID(Splitter.guid).compare(L"{B9559486-E1BB-45D3-A2A2-9A7AFE49B23F}"))); 
  }
  void SetD3DDevice(IDirect3DDevice9 * pD3DDevice){ m_pD3DDevice = pD3DDevice; }
  IDirect3DDevice9* GetD3DDevice(){ return m_pD3DDevice; }
  Com::SmartPtr<SaneAudioRenderer::ISettings> sanear;
  
private:
  CGraphFilters();
  ~CGraphFilters();

  static CGraphFilters* m_pSingleton;

  bool m_isKodiRealFS;
  bool m_isDVD;
  bool m_UsingDXVADecoder;
  bool m_auxAudioDelay;

  bool m_bDialogProcessInfo = false;
  
  std::map<int, DynamicIntegerSettingOptions> m_mapHWAccelDeviceInfo;

  Com::SmartPtr<IBaseFilter> m_pBF;
  IDirect3DDevice9 * m_pD3DDevice;
};
