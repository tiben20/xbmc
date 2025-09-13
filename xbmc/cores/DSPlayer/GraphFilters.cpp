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

#if HAS_DS_PLAYER

#include "GraphFilters.h"
#include "settings/Settings.h"
#include "settings/MediaSettings.h"
#include "Filters/LavSettings.h"
#include "Filters/LAVAudioSettings.h"
#include "Filters/LAVVideoSettings.h"
#include "Filters/LAVSplitterSettings.h"
#include "DSPropertyPage.h"
#include "FGFilter.h"
#include "DSPlayerDatabase.h"
#include "filtercorefactory/filtercorefactory.h"
#include "Utils/DSFilterEnumerator.h"
#include "Utils/AudioEnumerator.h"
#include "Filters/Sanear/Factory.h"
#include "settings/AdvancedSettings.h"
#include "ServiceBroker.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "guilib/GUIComponent.h"

#pragma comment(lib , "version.lib")

const std::string CGraphFilters::INTERNAL_LAVVIDEO = "lavvideo_internal";
const std::string CGraphFilters::INTERNAL_LAVAUDIO = "lavaudio_internal";
const std::string CGraphFilters::INTERNAL_LAVSPLITTER = "lavsource_internal";
const std::string CGraphFilters::INTERNAL_XYVSFILTER = "xyvsfilter_internal";
const std::string CGraphFilters::INTERNAL_XYSUBFILTER ="xysubfilter_internal";
const std::string CGraphFilters::INTERNAL_SANEAR = "sanear_internal";
const std::string CGraphFilters::MADSHI_VIDEO_RENDERER = "madVR";

CGraphFilters *CGraphFilters::m_pSingleton = NULL;

CGraphFilters::CGraphFilters() :
  m_isDVD(false), 
  m_UsingDXVADecoder(false),
  m_isKodiRealFS(false),
  m_auxAudioDelay(false),
  m_bDialogProcessInfo(false),
  m_pD3DDevice(nullptr)
{
  m_mapHWAccelDeviceInfo.clear();
}

CGraphFilters::~CGraphFilters()
{
  if (m_isKodiRealFS)
  {
    
    CServiceBroker::GetSettingsComponent()->GetSettings()->SetBool(CSettings::SETTING_VIDEOSCREEN_FAKEFULLSCREEN, false);
    m_isKodiRealFS = false;
  }
}

CGraphFilters* CGraphFilters::Get()
{
  return (m_pSingleton) ? m_pSingleton : (m_pSingleton = new CGraphFilters());
}

void CGraphFilters::SetSanearSettings()
{

  if (!sanear)
    if (FAILED(SaneAudioRenderer::Factory::CreateSettings(&sanear)))
      return;


  std::wstring adeviceW;
  std::string adevice = CServiceBroker::GetSettingsComponent()->GetSettings()->GetString(CSettings::SETTING_DSPLAYER_SANEARDEVICES);  
  if (adevice == "System Default")
    adevice = "";
  g_charsetConverter.utf8ToW(adevice, adeviceW, false);

  bool bSanearExclusive = CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_DSPLAYER_SANEAREXCLUSIVE);
  bool bSanearAllowbitstream = CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_DSPLAYER_SANEARALLOWBITSTREAM);
  bool bSanearStereoCrossfeed = CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_DSPLAYER_SANEARSTEREOCROSSFEED);
  bool bSanearIgnoreSystemChannelMixer = CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_DSPLAYER_SANEARIGNORESYSTEMCHANNELMIXER);
  int iSanearCutoff = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_DSPLAYER_SANEARCUTOFF);
  int iSanearLevel = CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_DSPLAYER_SANEARLEVEL);
  if (!CGraphFilters::Get()->AudioRenderer.pBF)
    return;
  sanear = CGraphFilters::Get()->AudioRenderer.pBF;
  if (!sanear)
    return;
  UINT32 buffer;
  sanear->GetOuputDevice(nullptr, nullptr, &buffer);
  sanear->SetOuputDevice(adeviceW.c_str(), bSanearExclusive, buffer);
  sanear->SetAllowBitstreaming(bSanearAllowbitstream);
  sanear->SetCrossfeedEnabled(bSanearStereoCrossfeed);
  sanear->SetIgnoreSystemChannelMixer(bSanearIgnoreSystemChannelMixer);
  sanear->SetCrossfeedSettings(iSanearCutoff, iSanearLevel);
}

void CGraphFilters::ShowInternalPPage(const std::string &type, bool showPropertyPage)
{
  m_pBF = NULL;

  IBaseFilter *pBF;
  GetInternalFilter(type, &pBF);

  // If there is not a playback create a filter to show propertypage
  if (pBF == NULL)
  {
    CreateInternalFilter(type, &m_pBF);
    pBF = m_pBF;
  }

  if (showPropertyPage)
  {
    CDSPropertyPage *pDSPropertyPage = DNew CDSPropertyPage(pBF, type);
    pDSPropertyPage->Initialize();
  }
  else
  {
    if (type == INTERNAL_LAVVIDEO)
      CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_DIALOG_LAVVIDEO);
    if (type == INTERNAL_LAVAUDIO)
      CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_DIALOG_LAVAUDIO);
    if (type == INTERNAL_LAVSPLITTER)
      CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_DIALOG_LAVSPLITTER);
    if (type == INTERNAL_SANEAR)
      CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_DIALOG_SANEAR);
  }
}

bool CGraphFilters::ShowOSDPPage(IBaseFilter *pBF)
{
  if (Video.pBF == pBF && Video.internalFilter)
  {
    CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_DIALOG_LAVVIDEO);
    return true;
  }
  if (Audio.pBF == pBF && Audio.internalFilter)
  {
    CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_DIALOG_LAVAUDIO);
    return true;
  }
  if ((Source.pBF == pBF && Source.internalFilter) || (Splitter.pBF == pBF && Splitter.internalFilter))
  {
    CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_DIALOG_LAVSPLITTER);
    return true;
  }
  if (AudioRenderer.pBF == pBF && AudioRenderer.internalFilter)
  {
    CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_DIALOG_SANEAR);
    return true;
  }

  return false;
}

void CGraphFilters::CreateInternalFilter(const std::string &type, IBaseFilter **ppBF)
{
  std::string filterName = type;
  if (type == INTERNAL_XYSUBFILTER) 
    filterName = CGraphFilters::INTERNAL_XYVSFILTER;

  CFGLoader *pLoader = new CFGLoader();
  pLoader->LoadConfig();

  CFGFilter *pFilter = NULL;
  if (!(pFilter = CFilterCoreFactory::GetFilterFromName(filterName)))
    return;

  pFilter->Create(ppBF);

  // Init LavFilters settings
  SetupLavSettings(type, *ppBF);
}

void CGraphFilters::GetInternalFilter(const std::string &type, IBaseFilter **ppBF)
{
  *ppBF = m_pBF;

  if (type == INTERNAL_LAVVIDEO && Video.pBF && Video.internalFilter)
    *ppBF = Video.pBF;

  if (type == INTERNAL_LAVAUDIO && Audio.pBF && Audio.internalFilter)
    *ppBF = Audio.pBF;

  if (type == INTERNAL_LAVSPLITTER && Splitter.pBF && Splitter.internalFilter)
    *ppBF = Splitter.pBF;

  if (type == INTERNAL_LAVSPLITTER && Source.pBF && Source.internalFilter)
    *ppBF = Source.pBF;

  if (type == INTERNAL_XYSUBFILTER && Subs.pBF && Subs.internalFilter)
    *ppBF = Subs.pBF;

  if (type == INTERNAL_SANEAR && AudioRenderer.pBF && AudioRenderer.internalFilter)
    *ppBF = AudioRenderer.pBF;
}

std::string CGraphFilters::GetInternalType(IBaseFilter *pBF)
{
  if (Video.pBF == pBF && Video.internalFilter)
    return INTERNAL_LAVVIDEO;
  if (Audio.pBF == pBF && Audio.internalFilter)
    return INTERNAL_LAVAUDIO;
  if ((Source.pBF == pBF && Source.internalFilter) || (Splitter.pBF == pBF && Splitter.internalFilter))
    return INTERNAL_LAVSPLITTER;
  if ((Subs.pBF == pBF && Subs.internalFilter))
    return INTERNAL_XYSUBFILTER;
  if ((AudioRenderer.pBF == pBF && AudioRenderer.internalFilter))
    return INTERNAL_SANEAR;

  return "";
}

void CGraphFilters::SetupLavSettings(const std::string &type, IBaseFilter* pBF)
{
  if (type != INTERNAL_LAVVIDEO && type != INTERNAL_LAVAUDIO && type != INTERNAL_LAVSPLITTER)
    return;

  // Set LavFilters in RunTimeConfig to have personal settings only for DSPlayer
  // this will reset LavFilters to default settings
  SetLavInternal(type, pBF);

  // Use LavFilters settings stored in DSPlayer DB if they are present
  if (LoadLavSettings(type))
  {
    SetLavSettings(type, pBF);
  }
  else
  {    
    // If DSPlayer DB it's empty load default LavFilters settings and then save into DB
    GetLavSettings(type, pBF);
    SaveLavSettings(type);  
  }
}

bool CGraphFilters::SetLavInternal(const std::string &type, IBaseFilter *pBF)
{
  if (type != INTERNAL_LAVVIDEO && type != INTERNAL_LAVAUDIO && type != INTERNAL_LAVSPLITTER)
    return false;

  if (type == INTERNAL_LAVVIDEO)
  {
    Com::SComQIPtr<ILAVVideoSettings> pLAVVideoSettings = pBF;
    pLAVVideoSettings->SetRuntimeConfig(TRUE);
  }
  else if (type == INTERNAL_LAVAUDIO)
  {
    Com::SComQIPtr<ILAVAudioSettings> pLAVAudioSettings = pBF;
    pLAVAudioSettings->SetRuntimeConfig(TRUE);
  }
  else if (type == INTERNAL_LAVSPLITTER)
  {
    Com::SComQIPtr<ILAVFSettings> pLAVFSettings = pBF;
    pLAVFSettings->SetRuntimeConfig(TRUE);
  }

  return true;
}

bool CGraphFilters::GetLavSettings(const std::string &type, IBaseFilter* pBF)
{
  if (type != INTERNAL_LAVVIDEO && type != INTERNAL_LAVAUDIO && type != INTERNAL_LAVSPLITTER)
    return false;

  if (type == INTERNAL_LAVVIDEO)
  {
    Com::SComQIPtr<ILAVVideoSettings> pLAVVideoSettings = pBF;

    CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();

    if (!pLAVVideoSettings)
      return false;

    m_mapHWAccelDeviceInfo.clear();

    lavSettings.video_bTrayIcon = pLAVVideoSettings->GetTrayIcon();
    lavSettings.video_dwStreamAR = pLAVVideoSettings->GetStreamAR();
    lavSettings.video_dwNumThreads = pLAVVideoSettings->GetNumThreads();
    lavSettings.video_dwDeintFieldOrder = pLAVVideoSettings->GetDeintFieldOrder();
    lavSettings.video_deintMode = pLAVVideoSettings->GetDeinterlacingMode();
    lavSettings.video_dwRGBRange = pLAVVideoSettings->GetRGBOutputRange();
    lavSettings.video_dwSWDeintMode = pLAVVideoSettings->GetSWDeintMode();
    lavSettings.video_dwSWDeintOutput = pLAVVideoSettings->GetSWDeintOutput();
    lavSettings.video_dwDitherMode = pLAVVideoSettings->GetDitherMode();
    for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
      lavSettings.video_bPixFmts[i] = pLAVVideoSettings->GetPixelFormat((LAVOutPixFmts)i);
    }
    lavSettings.video_dwHWAccel = pLAVVideoSettings->GetHWAccel();
    for (int i = 0; i < HWAccel_NB; ++i)
    {
      std::string sval;
      int ival;
      ival = -1;
     
      if ( i == HWAccel_D3D11)
        sval = "Automatic (Native)";
      else
        sval = "Automatic";
     //TranslatableIntegerSettingOption
      //m_mapHWAccelDeviceInfo[i].emplace_back(sval, ival);
      int iDevices;
      iDevices = pLAVVideoSettings->GetHWAccelNumDevices((LAVHWAccel)i);
      lavSettings.video_dwHWAccelDeviceIndex[(LAVHWAccel)i] = -1;
      if (iDevices > 0)
      {
        lavSettings.video_dwHWAccelDeviceIndex[(LAVHWAccel)i] = pLAVVideoSettings->GetHWAccelDeviceIndex((LAVHWAccel)i, 0);
       
        for (int index = 0; index < iDevices; ++index) {
          BSTR deviceInfo;
          std::string sDeviceInfo;
          pLAVVideoSettings->GetHWAccelDeviceInfo((LAVHWAccel)i, index, &deviceInfo, 0);
          if (deviceInfo != nullptr)
          {
            g_charsetConverter.wToUTF8(deviceInfo, sDeviceInfo);
            SysFreeString(deviceInfo);
          }


          //m_mapHWAccelDeviceInfo[i].emplace_back(sDeviceInfo, index);

        }
      }
    }

    for (int i = 0; i < HWCodec_NB; ++i) {
      lavSettings.video_bHWFormats[i] = pLAVVideoSettings->GetHWAccelCodec((LAVVideoHWCodec)i);
    }
    for (int i = 0; i < Codec_VideoNB; ++i) {
      lavSettings.video_bVideoFormats[i] = pLAVVideoSettings->GetFormatConfiguration((LAVVideoCodec)i);
    }
    lavSettings.video_dwHWAccelResFlags = pLAVVideoSettings->GetHWAccelResolutionFlags();
    lavSettings.video_dwHWDeintMode = pLAVVideoSettings->GetHWAccelDeintMode();
    lavSettings.video_dwHWDeintOutput = pLAVVideoSettings->GetHWAccelDeintOutput();
    lavSettings.video_bUseMSWMV9Decoder = pLAVVideoSettings->GetUseMSWMV9Decoder();
    lavSettings.video_bDVDVideoSupport = pLAVVideoSettings->GetDVDVideoSupport();   
  } 
  if (type == INTERNAL_LAVAUDIO)
  {
    Com::SComQIPtr<ILAVAudioSettings> pLAVAudioSettings = pBF;

    CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();

    if (!pLAVAudioSettings)
      return false;

    lavSettings.audio_bTrayIcon = pLAVAudioSettings->GetTrayIcon();
    pLAVAudioSettings->GetDRC(&lavSettings.audio_bDRCEnabled, &lavSettings.audio_iDRCLevel);
    lavSettings.audio_bDTSHDFraming = pLAVAudioSettings->GetDTSHDFraming();
    lavSettings.audio_bAutoAVSync = pLAVAudioSettings->GetAutoAVSync();
    lavSettings.audio_bExpandMono = pLAVAudioSettings->GetExpandMono();
    lavSettings.audio_bExpand61 = pLAVAudioSettings->GetExpand61();
    lavSettings.audio_bOutputStandardLayout = pLAVAudioSettings->GetOutputStandardLayout();
    lavSettings.audio_b51Legacy = pLAVAudioSettings->GetOutput51LegacyLayout();
    lavSettings.audio_bMixingEnabled = pLAVAudioSettings->GetMixingEnabled();
    lavSettings.audio_dwMixingLayout = pLAVAudioSettings->GetMixingLayout();
    lavSettings.audio_dwMixingFlags = pLAVAudioSettings->GetMixingFlags();
    lavSettings.audio_dwMixingMode = pLAVAudioSettings->GetMixingMode();
    pLAVAudioSettings->GetMixingLevels(&lavSettings.audio_dwMixingCenterLevel, &lavSettings.audio_dwMixingSurroundLevel, &lavSettings.audio_dwMixingLFELevel);
    //pLAVAudioSettings->GetAudioDelay(&lavSettings.audio_bAudioDelayEnabled, &lavSettings.audio_iAudioDelay);

    for (int i = 0; i < Bitstream_NB; ++i) {
      lavSettings.audio_bBitstream[i] = pLAVAudioSettings->GetBitstreamConfig((LAVBitstreamCodec)i);
    }
    for (int i = 0; i < SampleFormat_Bitstream; ++i) {
      lavSettings.audio_bSampleFormats[i] = pLAVAudioSettings->GetSampleFormat((LAVAudioSampleFormat)i);
    }
    for (int i = 0; i < Codec_AudioNB; ++i) {
      lavSettings.audio_bAudioFormats[i] = pLAVAudioSettings->GetFormatConfiguration((LAVAudioCodec)i);
    }
    lavSettings.audio_bSampleConvertDither = pLAVAudioSettings->GetSampleConvertDithering();
  }
  if (type == INTERNAL_LAVSPLITTER)
  {
    Com::SComQIPtr<ILAVFSettings> pLAVFSettings = pBF;

    CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();

    if (!pLAVFSettings)
      return false;

    lavSettings.splitter_bTrayIcon = pLAVFSettings->GetTrayIcon();

    HRESULT hr;
    LPWSTR lpwstr = nullptr;
    hr = pLAVFSettings->GetPreferredLanguages(&lpwstr);
    if (SUCCEEDED(hr) && lpwstr) {
      lavSettings.splitter_prefAudioLangs = lpwstr;
      CoTaskMemFree(lpwstr);
    }
    lpwstr = nullptr;
    hr = pLAVFSettings->GetPreferredSubtitleLanguages(&lpwstr);
    if (SUCCEEDED(hr) && lpwstr) {
      lavSettings.splitter_prefSubLangs = lpwstr;
      CoTaskMemFree(lpwstr);
    }
    lpwstr = nullptr;
    hr = pLAVFSettings->GetAdvancedSubtitleConfig(&lpwstr);
    if (SUCCEEDED(hr) && lpwstr) {
      lavSettings.splitter_subtitleAdvanced = lpwstr;
      CoTaskMemFree(lpwstr);
    }

    lavSettings.splitter_subtitleMode = pLAVFSettings->GetSubtitleMode();
    lavSettings.splitter_bPGSForcedStream = pLAVFSettings->GetPGSForcedStream();
    lavSettings.splitter_bPGSOnlyForced = pLAVFSettings->GetPGSOnlyForced();
    lavSettings.splitter_iVC1Mode = pLAVFSettings->GetVC1TimestampMode();
    lavSettings.splitter_bSubstreams = pLAVFSettings->GetSubstreamsEnabled();
    lavSettings.splitter_bMatroskaExternalSegments = pLAVFSettings->GetLoadMatroskaExternalSegments();
    lavSettings.splitter_bStreamSwitchRemoveAudio = pLAVFSettings->GetStreamSwitchRemoveAudio();
    lavSettings.splitter_bImpairedAudio = pLAVFSettings->GetUseAudioForHearingVisuallyImpaired();
    lavSettings.splitter_bPreferHighQualityAudio = pLAVFSettings->GetPreferHighQualityAudioStreams();
    lavSettings.splitter_dwQueueMaxSize = pLAVFSettings->GetMaxQueueMemSize();
    lavSettings.splitter_dwQueueMaxPacketsSize = pLAVFSettings->GetMaxQueueSize();
    lavSettings.splitter_dwNetworkAnalysisDuration = pLAVFSettings->GetNetworkStreamAnalysisDuration();
  }

  return true;
}

bool CGraphFilters::SetLavSettings(const std::string &type, IBaseFilter* pBF)
{
  if (type != INTERNAL_LAVVIDEO && type != INTERNAL_LAVAUDIO && type != INTERNAL_LAVSPLITTER)
    return false;

  if (type == INTERNAL_LAVVIDEO)
  {
    Com::SComQIPtr<ILAVVideoSettings> pLAVVideoSettings = pBF;

    CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();

    if (!pLAVVideoSettings)
      return false;

    pLAVVideoSettings->SetTrayIcon(lavSettings.video_bTrayIcon);
    pLAVVideoSettings->SetStreamAR(lavSettings.video_dwStreamAR);
    pLAVVideoSettings->SetNumThreads(lavSettings.video_dwNumThreads);
    pLAVVideoSettings->SetDeintFieldOrder((LAVDeintFieldOrder)lavSettings.video_dwDeintFieldOrder);
    pLAVVideoSettings->SetDeinterlacingMode(lavSettings.video_deintMode);
    pLAVVideoSettings->SetRGBOutputRange(lavSettings.video_dwRGBRange);
    pLAVVideoSettings->SetSWDeintMode((LAVSWDeintModes)lavSettings.video_dwSWDeintMode);
    pLAVVideoSettings->SetSWDeintOutput((LAVDeintOutput)lavSettings.video_dwSWDeintOutput);
    pLAVVideoSettings->SetDitherMode((LAVDitherMode)lavSettings.video_dwDitherMode);
    for (int i = 0; i < LAVOutPixFmt_NB; ++i) {
      pLAVVideoSettings->SetPixelFormat((LAVOutPixFmts)i, lavSettings.video_bPixFmts[i]);
    }
    pLAVVideoSettings->SetHWAccel((LAVHWAccel)lavSettings.video_dwHWAccel);
    for (int i = 0; i < HWAccel_NB; ++i)
    {
      int iDevices;
      iDevices = pLAVVideoSettings->GetHWAccelNumDevices((LAVHWAccel)i);
      if (iDevices > 0)
      {
        pLAVVideoSettings->SetHWAccelDeviceIndex((LAVHWAccel)i, lavSettings.video_dwHWAccelDeviceIndex[(LAVHWAccel)i], 0);
      }
    }
    for (int i = 0; i < HWCodec_NB; ++i) {
      pLAVVideoSettings->SetHWAccelCodec((LAVVideoHWCodec)i, lavSettings.video_bHWFormats[i]);
    }
    for (int i = 0; i < Codec_VideoNB; ++i) {
      pLAVVideoSettings->SetFormatConfiguration((LAVVideoCodec)i, lavSettings.video_bVideoFormats[i]);
    }
    pLAVVideoSettings->SetHWAccelResolutionFlags(lavSettings.video_dwHWAccelResFlags);
    pLAVVideoSettings->SetHWAccelDeintMode((LAVHWDeintModes)lavSettings.video_dwHWDeintMode);
    pLAVVideoSettings->SetHWAccelDeintOutput((LAVDeintOutput)lavSettings.video_dwHWDeintOutput);
    pLAVVideoSettings->SetUseMSWMV9Decoder(lavSettings.video_bUseMSWMV9Decoder);
    pLAVVideoSettings->SetDVDVideoSupport(lavSettings.video_bDVDVideoSupport);

    // Custom interface
    if (Com::SComQIPtr<ILAVVideoSettingsDSPlayerCustom> pLAVFSettingsDSPlayerCustom = pLAVVideoSettings.p)
      pLAVFSettingsDSPlayerCustom->SetPropertyPageCallback(CDSPropertyPage::PropertyPageCallback);
  }
  if (type == INTERNAL_LAVAUDIO)
  {
    Com::SComQIPtr<ILAVAudioSettings> pLAVAudioSettings = pBF;

    CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();

    if (!pLAVAudioSettings)
      return false;

    pLAVAudioSettings->SetTrayIcon(lavSettings.audio_bTrayIcon);
    pLAVAudioSettings->SetDRC(lavSettings.audio_bDRCEnabled, lavSettings.audio_iDRCLevel);
    pLAVAudioSettings->SetDTSHDFraming(lavSettings.audio_bDTSHDFraming);
    pLAVAudioSettings->SetAutoAVSync(lavSettings.audio_bAutoAVSync);
    pLAVAudioSettings->SetExpandMono(lavSettings.audio_bExpandMono);
    pLAVAudioSettings->SetExpand61(lavSettings.audio_bExpand61);
    pLAVAudioSettings->SetOutputStandardLayout(lavSettings.audio_bOutputStandardLayout);
    pLAVAudioSettings->SetOutput51LegacyLayout(lavSettings.audio_b51Legacy);
    pLAVAudioSettings->SetMixingEnabled(lavSettings.audio_bMixingEnabled);
    pLAVAudioSettings->SetMixingLayout(lavSettings.audio_dwMixingLayout);
    pLAVAudioSettings->SetMixingFlags(lavSettings.audio_dwMixingFlags);
    pLAVAudioSettings->SetMixingMode((LAVAudioMixingMode)lavSettings.audio_dwMixingMode);
    pLAVAudioSettings->SetMixingLevels(lavSettings.audio_dwMixingCenterLevel, lavSettings.audio_dwMixingSurroundLevel, lavSettings.audio_dwMixingLFELevel);
    //pLAVAudioSettings->SetAudioDelay(lavSettings.audio_bAudioDelayEnabled, lavSettings.audio_iAudioDelay);
    for (int i = 0; i < Bitstream_NB; ++i) {
      pLAVAudioSettings->SetBitstreamConfig((LAVBitstreamCodec)i, lavSettings.audio_bBitstream[i]);
    }
    for (int i = 0; i < SampleFormat_Bitstream; ++i) {
      pLAVAudioSettings->SetSampleFormat((LAVAudioSampleFormat)i, lavSettings.audio_bSampleFormats[i]);
    }
    for (int i = 0; i < Codec_AudioNB; ++i) {
      pLAVAudioSettings->SetFormatConfiguration((LAVAudioCodec)i, lavSettings.audio_bAudioFormats[i]);
    }
    pLAVAudioSettings->SetSampleConvertDithering(lavSettings.audio_bSampleConvertDither);

    // The internal LAV Audio Decoder will not be registered to handle WMA formats
    // since the system decoder is preferred. However we can still enable those
    // formats internally so that they are used in low-merit mode.
    pLAVAudioSettings->SetFormatConfiguration(Codec_WMA2, TRUE);
    pLAVAudioSettings->SetFormatConfiguration(Codec_WMAPRO, TRUE);
    pLAVAudioSettings->SetFormatConfiguration(Codec_WMALL, TRUE);

    // Custom interface
    if (Com::SComQIPtr<ILAVAudioSettingsDSPlayerCustom> pLAVFSettingsDSPlayerCustom = pLAVAudioSettings.p)
      pLAVFSettingsDSPlayerCustom->SetPropertyPageCallback(CDSPropertyPage::PropertyPageCallback);
  }
  if (type == INTERNAL_LAVSPLITTER)
  {
    Com::SComQIPtr<ILAVFSettings> pLAVFSettings = pBF;

    CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();

    if (!pLAVFSettings)
      return false;

    pLAVFSettings->SetTrayIcon(lavSettings.splitter_bTrayIcon);
    pLAVFSettings->SetPreferredLanguages(lavSettings.splitter_prefAudioLangs.c_str());
    pLAVFSettings->SetPreferredSubtitleLanguages(lavSettings.splitter_prefSubLangs.c_str());
    pLAVFSettings->SetAdvancedSubtitleConfig(lavSettings.splitter_subtitleAdvanced.c_str());
    pLAVFSettings->SetSubtitleMode(lavSettings.splitter_subtitleMode);
    pLAVFSettings->SetPGSForcedStream(lavSettings.splitter_bPGSForcedStream);
    pLAVFSettings->SetPGSOnlyForced(lavSettings.splitter_bPGSOnlyForced);
    pLAVFSettings->SetVC1TimestampMode(lavSettings.splitter_iVC1Mode);
    pLAVFSettings->SetSubstreamsEnabled(lavSettings.splitter_bSubstreams);
    pLAVFSettings->SetLoadMatroskaExternalSegments(lavSettings.splitter_bMatroskaExternalSegments);
    pLAVFSettings->SetStreamSwitchRemoveAudio(lavSettings.splitter_bStreamSwitchRemoveAudio);
    pLAVFSettings->SetUseAudioForHearingVisuallyImpaired(lavSettings.splitter_bImpairedAudio);
    pLAVFSettings->SetPreferHighQualityAudioStreams(lavSettings.splitter_bPreferHighQualityAudio);
    pLAVFSettings->SetMaxQueueMemSize(lavSettings.splitter_dwQueueMaxSize);
    pLAVFSettings->SetMaxQueueSize(lavSettings.splitter_dwQueueMaxPacketsSize);
    pLAVFSettings->SetNetworkStreamAnalysisDuration(lavSettings.splitter_dwNetworkAnalysisDuration);

    // Custom interface
    if (Com::SComQIPtr<ILAVFSettingsDSPlayerCustom> pLAVFSettingsDSPlayerCustom = pLAVFSettings.p)
      pLAVFSettingsDSPlayerCustom->SetPropertyPageCallback(CDSPropertyPage::PropertyPageCallback);
  }
  return true;
}

bool CGraphFilters::SaveLavSettings(const std::string &type)
{
  if (type != INTERNAL_LAVVIDEO && type != INTERNAL_LAVAUDIO && type != INTERNAL_LAVSPLITTER)
    return false;

  CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();

  CDSPlayerDatabase dsdbs;
  if (dsdbs.Open())
  {
    if (type == INTERNAL_LAVVIDEO)
      dsdbs.SetLAVVideoSettings(lavSettings);
    if (type == INTERNAL_LAVAUDIO)
      dsdbs.SetLAVAudioSettings(lavSettings);
    if (type == INTERNAL_LAVSPLITTER)
      dsdbs.SetLAVSplitterSettings(lavSettings);
    dsdbs.Close();
  }

  return true;
}

bool CGraphFilters::LoadLavSettings(const std::string &type)
{
  if (type != INTERNAL_LAVVIDEO && type != INTERNAL_LAVAUDIO && type != INTERNAL_LAVSPLITTER)
    return false;

  CLavSettings &lavSettings = CMediaSettings::GetInstance().GetCurrentLavSettings();
  bool result = false;
  CDSPlayerDatabase dsdbs;
  if (dsdbs.Open())
  {
    if (type == INTERNAL_LAVVIDEO)
      result = dsdbs.GetLAVVideoSettings(lavSettings);
    if (type == INTERNAL_LAVAUDIO)
      result = dsdbs.GetLAVAudioSettings(lavSettings);
    if (type == INTERNAL_LAVSPLITTER)
      result = dsdbs.GetLAVSplitterSettings(lavSettings);

    dsdbs.Close();
  }
  return result;
}

void CGraphFilters::EraseLavSetting(const std::string &type)
{
  if (type != INTERNAL_LAVVIDEO && type != INTERNAL_LAVAUDIO && type != INTERNAL_LAVSPLITTER)
    return;

  CDSPlayerDatabase dsdbs;
  if (dsdbs.Open())
  {
    if (type == INTERNAL_LAVVIDEO)
      dsdbs.EraseLAVVideo();
    if (type == INTERNAL_LAVAUDIO)
      dsdbs.EraseLAVAudio();
    if (type == INTERNAL_LAVSPLITTER)
      dsdbs.EraseLAVSplitter();

    dsdbs.Close();
  }
}

void CGraphFilters::GetActiveDecoder(std::pair<std::string, bool> &activeDecoder)
{  
  activeDecoder.first = "";
  activeDecoder.second = false;

  if (Com::SComQIPtr<ILAVVideoStatus> pLAVFStatus = Video.pBF.p)
  {
    const WCHAR *activeDecoderNameW = pLAVFStatus->GetActiveDecoderName();

    if (activeDecoderNameW != nullptr)
    {
      std::map<std::wstring, std::pair<std::string, bool> > decoderList;
      decoderList[L"avcodec"] = std::make_pair("FFMpeg", false);
      decoderList[L"dxva2n"] = std::make_pair("DXVA2 Native", true);
      decoderList[L"dxva2cb"] = std::make_pair("DXVA2 Copy-back", true);
      decoderList[L"dxva2cb direct"] = std::make_pair("DXVA2 Copy-back Direct", true);
      decoderList[L"cuvid"] = std::make_pair("NVIDIA CUVID", true);
      decoderList[L"quicksync"] = std::make_pair("Intel QuickSync", true);
      decoderList[L"d3d11 cb direct"] = std::make_pair("D3D11 Copy-back Direct", true);
      decoderList[L"d3d11 cb"] = std::make_pair("D3D11 Copy-back", true);
      decoderList[L"d3d11 native"] = std::make_pair("D3D11 Native", true);

      if (!decoderList[activeDecoderNameW].first.empty())
      {
        activeDecoder = decoderList[activeDecoderNameW];
      }
      else 
      {
        Com::SComQIPtr<ILAVVideoSettings> pLAVVideoSettings = Video.pBF.p;
        if (!pLAVVideoSettings)
          return;

        g_charsetConverter.wToUTF8(activeDecoderNameW, activeDecoder.first);
        activeDecoder.second = pLAVVideoSettings->GetHWAccel() > 0;
      }
    }
  }
}

void CGraphFilters::SetAuxAudioDelay()
{
  CAudioEnumerator pSound;
  m_auxAudioDelay = pSound.IsDevice(CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->GetAuxDeviceName());
}

#endif
