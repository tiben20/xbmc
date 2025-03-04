/*
 *  Copyright (C) 2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlHelper.h"
#include <mfobjects.h>
#include "Videorenderers/MPCVRRenderer.h"
#include "rendering/dx/RenderContext.h"
#include <DSUtil/MediaTypeEx.h>
#include "Helper.h"

using namespace PL;

static void pl_log_cb(void*, enum pl_log_level level, const char* msg)
{
  switch (level) {
  case PL_LOG_FATAL:
    CLog::Log(LOGFATAL, "libPlacebo Fatal: {}", msg);
    break;
  case PL_LOG_ERR:
    CLog::Log(LOGERROR, "libPlacebo Error: {}", msg);

    break;
  case PL_LOG_WARN:
    CLog::Log(LOGWARNING, "libPlacebo Warning: {}", msg);
    break;
  case PL_LOG_INFO:
    CLog::Log(LOGINFO, "libPlacebo Info: {}", msg);
    break;
  case PL_LOG_DEBUG:
    CLog::Log(LOGDEBUG, "libPlacebo Debug: {}", msg);
    break;
  case PL_LOG_NONE:
  case PL_LOG_TRACE:
    CLog::Log(LOGNONE, "libPlacebo Trace: {}", msg);
    break;
  }
}

const wchar_t* PL::PLCspPrimToString(const pl_color_primaries prim)
{
  switch (prim)
  { 
    case PL_COLOR_PRIM_UNKNOWN:       return L"auto";
    case PL_COLOR_PRIM_BT_601_525:    return L"bt.601-525";
    case PL_COLOR_PRIM_BT_601_625:    return L"bt.601-625";
    case PL_COLOR_PRIM_BT_709:        return L"bt.709";
    case PL_COLOR_PRIM_BT_2020:       return L"bt.2020";
    case PL_COLOR_PRIM_BT_470M:       return L"bt.470m";
    case PL_COLOR_PRIM_APPLE:         return L"apple";
    case PL_COLOR_PRIM_ADOBE:         return L"adobe";
    case PL_COLOR_PRIM_PRO_PHOTO:     return L"prophoto";
    case PL_COLOR_PRIM_CIE_1931:      return L"cie1931";
    case PL_COLOR_PRIM_DCI_P3:        return L"dci-p3";
    case PL_COLOR_PRIM_DISPLAY_P3:    return L"display-p3";
    case PL_COLOR_PRIM_V_GAMUT:       return L"v-gamut";
    case PL_COLOR_PRIM_S_GAMUT:       return L"s-gamut";
    case PL_COLOR_PRIM_EBU_3213:      return L"ebu3213";
    case PL_COLOR_PRIM_FILM_C:        return L"film-c";
    case PL_COLOR_PRIM_ACES_AP0:      return L"aces-ap0";
    case PL_COLOR_PRIM_ACES_AP1:      return L"aces-ap1";
  }
  return L"";
}

const wchar_t* PL::PLCspToString(const pl_color_system csp)
{
  switch (csp)
  {
    case PL_COLOR_SYSTEM_UNKNOWN:      return L"auto";
    case PL_COLOR_SYSTEM_BT_601:       return L"bt.601";
    case PL_COLOR_SYSTEM_BT_709:       return L"bt.709";
    case PL_COLOR_SYSTEM_SMPTE_240M:   return L"smpte-240m";
    case PL_COLOR_SYSTEM_BT_2020_NC:   return    L"bt.2020-ncl";
    case PL_COLOR_SYSTEM_BT_2020_C:    return    L"bt.2020-cl";
    case PL_COLOR_SYSTEM_BT_2100_PQ:   return    L"bt.2100-pq";
    case PL_COLOR_SYSTEM_BT_2100_HLG:  return    L"bt.2100-hlg";
    case PL_COLOR_SYSTEM_DOLBYVISION:  return    L"dolbyvision";
    case PL_COLOR_SYSTEM_RGB:          return    L"rgb";
    case PL_COLOR_SYSTEM_XYZ:          return    L"xyz";
    case PL_COLOR_SYSTEM_YCGCO:        return    L"ycgco";
  }
  return L"";
  
}

const wchar_t* PL::PLCspTransfertToString(const pl_color_transfer csp)
{
  switch (csp)
  {
  case  PL_COLOR_TRC_UNKNOWN: return    L"auto";
  case  PL_COLOR_TRC_BT_1886: return    L"bt.1886";
  case  PL_COLOR_TRC_SRGB: return    L"srgb";
  case  PL_COLOR_TRC_LINEAR: return    L"linear";
  case  PL_COLOR_TRC_GAMMA18: return    L"gamma1.8";
  case  PL_COLOR_TRC_GAMMA20: return    L"gamma2.0";
  case  PL_COLOR_TRC_GAMMA22: return    L"gamma2.2";
  case  PL_COLOR_TRC_GAMMA24: return    L"gamma2.4";
  case  PL_COLOR_TRC_GAMMA26:  return    L"gamma2.6";
  case  PL_COLOR_TRC_GAMMA28:   return    L"gamma2.8";
  case  PL_COLOR_TRC_PRO_PHOTO: return    L"prophoto";
  case  PL_COLOR_TRC_PQ:  return    L"pq";
  case  PL_COLOR_TRC_HLG:  return    L"hlg";
  case  PL_COLOR_TRC_V_LOG:  return    L"v-log";
  case  PL_COLOR_TRC_S_LOG1: return    L"s-log1";
  case  PL_COLOR_TRC_S_LOG2:  return    L"s-log2";
  case  PL_COLOR_TRC_ST428:  return    L"st428";
  }
  return L"";

}

CPlHelper::CPlHelper()
{ 
  m_plOptions = nullptr;
  m_plSwapchain = nullptr;
  m_plLog = nullptr;
  for (int i = 0; i < MAX_FRAME_PASSES; i++)
    frame_info[i] = {};
  for (int i = 0; i < MAX_BLEND_FRAMES; i++)
    for (int ii = 0; ii < MAX_BLEND_FRAMES; ii++)
      blend_info[i][ii] = {};
  for (int i = 0 ;i<8;i++)
    num_blend_passes[i] = 0;
  num_frame_passes = 0;
  m_pIcc = nullptr;
  m_pCache = nullptr;
}

CPlHelper::~CPlHelper()
{
  CLog::Log(LOGINFO, "{}", __FUNCTION__);
}

bool CPlHelper::Init(DXGI_FORMAT fmt)
{
  //TODO add pl_test_pixfmt verification to start we will only use dxgi_format_nv12
  //Log initiation
  if (!m_plLog)
  {
 
    pl_log_params log_param{};
    log_param.log_cb = pl_log_cb;
    log_param.log_level = PL_LOG_ERR;
    m_plLog = pl_log_create(PL_API_VER, &log_param);
  }
  if (m_plD3d11)
    return false;
  //d3d device
  pl_d3d11_params d3d_param{};
  d3d_param.device = DX::DeviceResources::Get()->GetD3DDevice();
  d3d_param.adapter = DX::DeviceResources::Get()->GetAdapter();
  d3d_param.adapter_luid = DX::DeviceResources::Get()->GetAdapterDesc().AdapterLuid;
  d3d_param.allow_software = true;
  d3d_param.force_software = false;
  d3d_param.debug = false;
  //libplacebo dont touch it if 0
  d3d_param.max_frame_latency = 0;
  d3d_param.use_deferred_context = true;
  m_plD3d11 = pl_d3d11_create(m_plLog, &d3d_param);
  if (!m_plD3d11)
    return false;
  //swap chain
  pl_d3d11_swapchain_params swapchain_param{};
  swapchain_param.swapchain = DX::DeviceResources::Get()->GetSwapChain();
  //everything else is not used
  m_plSwapchain = pl_d3d11_create_swapchain(m_plD3d11, &swapchain_param);
  if (!m_plSwapchain)
    return false;

  m_plRenderer = pl_renderer_create(m_plLog, m_plD3d11->gpu);
  if (!m_plRenderer)
    return false;

  return false;
}

void CPlHelper::Release()
{

  pl_renderer_destroy(&m_plRenderer);
  pl_options_free(&m_plOptions);
  m_plD3d11 = nullptr;
  m_plSwapchain = nullptr;
  for (std::vector<const pl_hook*>::iterator it = m_pShaderHooks.begin(); it != m_pShaderHooks.end(); it++)
  {
    pl_mpv_user_shader_destroy(&(*it));
  }

  for (int i = 0; i < MAX_FRAME_PASSES; i++)
    pl_shader_info_deref(&frame_info[i].shader);
  for (int j = 0; j < MAX_BLEND_FRAMES; j++) {
    for (int i = 0; i < MAX_BLEND_PASSES; i++)
      pl_shader_info_deref(&blend_info[j][i].shader);
  }

  m_pShaderHooks.clear();
  //free(shader_hooks); 
  m_pShaderPaths.clear();
  pl_icc_close(&m_pIcc);

  if (m_pCache) {
    if (pl_cache_signature(m_pCache) != m_pCacheSignature) {
      FILE* file = fopen(m_pCacheFile.c_str(), "wb");
      if (file) {
        pl_cache_save_file(m_pCache, file);
        fclose(file);
      }
    }
    pl_cache_destroy(&m_pCache);
  }

  pl_log_destroy(&m_plLog);

}

const pl_hook* PL::CPlHelper::SetupShader()
{
  return nullptr;
}



pl_color_repr CPlHelper::GetPlColorRepresentation(DXVA2_ExtendedFormat pFormat)
{
  pl_color_repr repr{};
  switch (pFormat.VideoTransferMatrix) {
  //case AVCOL_SPC_RGB:                 repr.sys = PL_COLOR_SYSTEM_RGB;
  case DXVA2_VideoTransferMatrix_BT709:
    repr.sys = PL_COLOR_SYSTEM_BT_709; 
    break;
  //case AVCOL_SPC_UNSPECIFIED:         repr.sys = PL_COLOR_SYSTEM_UNKNOWN;
  //case AVCOL_SPC_RESERVED:            repr.sys = PL_COLOR_SYSTEM_UNKNOWN;
  case (DXVA2_VideoTransferMatrix)6:                
    repr.sys = PL_COLOR_SYSTEM_UNKNOWN; 
    break; // missing
  case DXVA2_VideoTransferMatrix_BT601:             
    repr.sys = PL_COLOR_SYSTEM_BT_601; 
    break;
  case DXVA2_VideoTransferMatrix_SMPTE240M:           
    repr.sys = PL_COLOR_SYSTEM_SMPTE_240M; 
    break;
  case (DXVA2_VideoTransferMatrix)7:              
    repr.sys = PL_COLOR_SYSTEM_YCGCO; 
    break;
  case MFVideoTransferMatrix_BT2020_10:          
    repr.sys = PL_COLOR_SYSTEM_BT_2020_NC; 
    break;
  default:
    break;
  //case MFVideoTransferMatrix_BT2020_10:           repr.sys = PL_COLOR_SYSTEM_BT_2020_C;
  //case AVCOL_SPC_SMPTE2085:           repr.sys = PL_COLOR_SYSTEM_UNKNOWN; // missing
  //case AVCOL_SPC_CHROMA_DERIVED_NCL:  repr.sys = PL_COLOR_SYSTEM_UNKNOWN; // missing
  //case AVCOL_SPC_CHROMA_DERIVED_CL:   repr.sys = PL_COLOR_SYSTEM_UNKNOWN; // missing
    // Note: this colorspace is confused between PQ and HLG, which libav*
    // requires inferring from other sources, but libplacebo makes explicit.
    // Default to PQ as it's the more common scenario.
  //case AVCOL_SPC_ICTCP:               repr.sys = PL_COLOR_SYSTEM_BT_2100_PQ;
  //case AVCOL_SPC_NB:                  repr.sys = PL_COLOR_SYSTEM_COUNT;
  }
  
  switch (pFormat.NominalRange) {
  case DXVA2_NominalRange_Unknown:       
    repr.levels = PL_COLOR_LEVELS_UNKNOWN; 
    break;
  case DXVA2_NominalRange_16_235:              
    repr.levels = PL_COLOR_LEVELS_LIMITED; 
    break;
  case DXVA2_NominalRange_0_255:             
    repr.levels = PL_COLOR_LEVELS_FULL; 
    break;
  default:
    break;
  //case AVCOL_RANGE_NB:                repr.levels = PL_COLOR_LEVELS_COUNT;
  }
  //when we will get a later version it will spawn to PL_ALPHA_NONE
  //Will need to check according to the dxgi format if we use PL_ALPHA_INDEPENDENT or PL_ALPHA_NONE in the future
  repr.alpha = PL_ALPHA_UNKNOWN;

  //nv12 is 8 bits per color depth

  repr.bits.color_depth = 8;
  return repr;
}


pl_color_space CPlHelper::GetPlColorSpace(DXVA2_ExtendedFormat pFormat)
{
  pl_color_space csp{};

  switch (pFormat.VideoPrimaries) {
  //case AVCOL_PRI_RESERVED0:       csp.primaries = PL_COLOR_PRIM_UNKNOWN;
  case DXVA2_VideoPrimaries_BT709:           
    csp.primaries = PL_COLOR_PRIM_BT_709; 
    break;
  //case AVCOL_PRI_UNSPECIFIED:     csp.primaries = PL_COLOR_PRIM_UNKNOWN;
  //case AVCOL_PRI_RESERVED:        csp.primaries = PL_COLOR_PRIM_UNKNOWN;
  case DXVA2_VideoPrimaries_BT470_2_SysM:          
    csp.primaries = PL_COLOR_PRIM_BT_470M; 
    break;
  case DXVA2_VideoPrimaries_BT470_2_SysBG:         
    csp.primaries = PL_COLOR_PRIM_BT_601_625; 
    break;
  case DXVA2_VideoPrimaries_SMPTE170M:       
    csp.primaries = PL_COLOR_PRIM_BT_601_525; 
    break;
  case DXVA2_VideoPrimaries_SMPTE240M:       
    csp.primaries = PL_COLOR_PRIM_BT_601_525; 
    break;
  //case AVCOL_PRI_FILM:            csp.primaries = PL_COLOR_PRIM_FILM_C;
  case MFVideoPrimaries_BT2020:          
    csp.primaries = PL_COLOR_PRIM_BT_2020; 
    break;
  case MFVideoPrimaries_XYZ:        
    csp.primaries = PL_COLOR_PRIM_CIE_1931; 
    break;
  case MFVideoPrimaries_DCI_P3:        
    csp.primaries = PL_COLOR_PRIM_DCI_P3; 
    break;
  //case AVCOL_PRI_SMPTE432:        csp.primaries = PL_COLOR_PRIM_DISPLAY_P3;
  //case AVCOL_PRI_JEDEC_P22:       csp.primaries = PL_COLOR_PRIM_EBU_3213;
  //case AVCOL_PRI_NB:              csp.primaries = PL_COLOR_PRIM_COUNT;
  default:
    break;
  }

  switch (pFormat.VideoTransferFunction) {
  //case AVCOL_TRC_RESERVED0:       csp.transfer = PL_COLOR_TRC_UNKNOWN;
  case DXVA2_VideoTransFunc_709:          
    csp.transfer = PL_COLOR_TRC_BT_1886;// EOTF != OETF
    break;
  //case AVCOL_TRC_UNSPECIFIED:     csp.transfer = PL_COLOR_TRC_UNKNOWN;
  //case AVCOL_TRC_RESERVED:        csp.transfer = PL_COLOR_TRC_UNKNOWN;
  case DXVA2_VideoTransFunc_22:         csp.transfer = PL_COLOR_TRC_GAMMA22; break;
  case DXVA2_VideoTransFunc_28:         csp.transfer = PL_COLOR_TRC_GAMMA28; break;
  case DXVA2_VideoTransFunc_240M:       csp.transfer = PL_COLOR_TRC_BT_1886; // EOTF != OETF
    break;
  case DXVA2_VideoTransFunc_10:          csp.transfer = PL_COLOR_TRC_LINEAR; 
    break;
  case MFVideoTransFunc_Log_100:             csp.transfer = PL_COLOR_TRC_UNKNOWN; break; // missing
  case MFVideoTransFunc_Log_316:        csp.transfer = PL_COLOR_TRC_UNKNOWN; break; // missing
  //case AVCOL_TRC_IEC61966_2_4:    csp.transfer = PL_COLOR_TRC_BT_1886; // EOTF != OETF
  //case AVCOL_TRC_BT1361_ECG:      csp.transfer = PL_COLOR_TRC_BT_1886; // ETOF != OETF
  //case AVCOL_TRC_IEC61966_2_1:    csp.transfer = PL_COLOR_TRC_SRGB;
    /*for this one lavfilters use this will need to look into when on hdr
    case AVCOL_TRC_BT2020_10:
  case AVCOL_TRC_BT2020_12:
    fmt.VideoTransferFunction = (matrix == AVCOL_SPC_BT2020_CL) ? MFVideoTransFunc_2020_const : MFVideoTransFunc_2020;
    break;*/
  case MFVideoTransFunc_2020_const:       csp.transfer = PL_COLOR_TRC_BT_1886; break; // EOTF != OETF
  case MFVideoTransFunc_2020:       csp.transfer = PL_COLOR_TRC_BT_1886; break; // EOTF != OETF
  case MFVideoTransFunc_2084:       csp.transfer = PL_COLOR_TRC_PQ; break;
  //case AVCOL_TRC_SMPTE428:        csp.transfer = PL_COLOR_TRC_ST428;
  case MFVideoTransFunc_HLG:    csp.transfer = PL_COLOR_TRC_HLG; break;
  //case AVCOL_TRC_NB:              csp.transfer = PL_COLOR_TRC_COUNT;
  }
  return csp;


}

bool CheckDoviMetadata(const MediaSideDataDOVIMetadata* pDOVIMetadata, const uint8_t maxReshapeMethon)
{
  if (!pDOVIMetadata->Header.disable_residual_flag) {
    return false;
  }

  for (const auto& curve : pDOVIMetadata->Mapping.curves) {
    if (curve.num_pivots < 2 || curve.num_pivots > 9) {
      return false;
    }
    for (int i = 0; i < int(curve.num_pivots - 1); i++) {
      if (curve.mapping_idc[i] > maxReshapeMethon) { // 0 polynomial, 1 mmr
        return false;
      }
    }
  }

  return true;
}

bool CPlHelper::ProcessDoviData(IMediaSample* pSample,
                            struct pl_color_space* color,
                            struct pl_color_repr* repr,
                            struct pl_dovi_metadata* doviout,
                            int width,
                            int height)
{
  
  Microsoft::WRL::ComPtr<IMediaSideData> pMediaSideData;
  HRESULT hr = pSample->QueryInterface(IID_PPV_ARGS(&pMediaSideData));
  size_t size = 0;
  MediaSideDataDOVIMetadata* pDOVIMetadata = nullptr;
  if (FAILED(hr))
    return false;
  hr = pMediaSideData->GetSideData(IID_MediaSideDataDOVIMetadataV2, (const BYTE**)&pDOVIMetadata, &size);

  if (!color || !repr || !doviout)
    return false;
  if (SUCCEEDED(hr) && size == sizeof(MediaSideDataDOVIMetadata) && CheckDoviMetadata(pDOVIMetadata, 1))
  {
    if (!pDOVIMetadata->Header.disable_residual_flag)
      return false;
    for (int i = 0; i < 3; i++)
      doviout->nonlinear_offset[i] = pDOVIMetadata->ColorMetadata.ycc_to_rgb_offset[i];
    for (int i = 0; i < 9; i++) {
      float* nonlinear = &doviout->nonlinear.m[0][0];
      float* linear = &doviout->linear.m[0][0];
      nonlinear[i] = pDOVIMetadata->ColorMetadata.ycc_to_rgb_matrix[i];
      linear[i] = pDOVIMetadata->ColorMetadata.rgb_to_lms_matrix[i];
    }
    for (int c = 0; c < 3; c++)
    {

      //pDOVIMetadata->Mapping.curves[c];
      struct pl_dovi_metadata::pl_reshape_data* cdst = &doviout->comp[c];
      cdst->num_pivots = pDOVIMetadata->Mapping.curves[c].num_pivots;
      for (int i = 0; i < pDOVIMetadata->Mapping.curves[c].num_pivots; i++) {
        
        const float scale = 1.0f / ((1 << pDOVIMetadata->Header.bl_bit_depth) - 1);
        cdst->pivots[i] = scale * pDOVIMetadata->Mapping.curves[c].pivots[i];
      }
      for (int i = 0; i < pDOVIMetadata->Mapping.curves[c].num_pivots - 1; i++) {
        const float scale = 1.0f / (1 << pDOVIMetadata->Header.coef_log2_denom);
        cdst->method[i] = pDOVIMetadata->Mapping.curves[c].mapping_idc[i];
        switch (pDOVIMetadata->Mapping.curves[c].mapping_idc[i]) {
        case 0://AV_DOVI_MAPPING_POLYNOMIAL:
          for (int k = 0; k < 3; k++) {
            cdst->poly_coeffs[i][k] = (k <= pDOVIMetadata->Mapping.curves[c].poly_order[i])
              ? scale * pDOVIMetadata->Mapping.curves[c].poly_coef[i][k]
              : 0.0f;
          }
          break;
        case 1://AV_DOVI_MAPPING_MMR:
          cdst->mmr_order[i] = pDOVIMetadata->Mapping.curves[c].mmr_order[i];
          cdst->mmr_constant[i] = scale * pDOVIMetadata->Mapping.curves[c].mmr_constant[i];
          for (int j = 0; j < pDOVIMetadata->Mapping.curves[c].mmr_order[i]; j++) {
            for (int k = 0; k < 7; k++)
              cdst->mmr_coeffs[i][j][k] = scale * pDOVIMetadata->Mapping.curves[c].mmr_coef[i][j][k];
          }
          break;
        }
      }
    }
  //
    repr->dovi = doviout;
    repr->sys = PL_COLOR_SYSTEM_BT_2020_NC;
    color->transfer = PL_COLOR_TRC_PQ;
    
    color->hdr.min_luma = pl_hdr_rescale(PL_HDR_PQ, PL_HDR_NITS, pDOVIMetadata->Extensions[0].Level1.min_pq / 4095.0f);
    color->hdr.max_luma = pl_hdr_rescale(PL_HDR_PQ, PL_HDR_NITS, pDOVIMetadata->Extensions[0].Level1.max_pq / 4095.0f);

    
    /*if ((dovi_ext = av_dovi_find_level(metadata, 1))) {
      color->hdr.max_pq_y = dovi_ext->l1.max_pq / 4095.0f;
      color->hdr.avg_pq_y = dovi_ext->l1.avg_pq / 4095.0f;
    }*/
    return true;
  }
  return false;
}

pl_hdr_metadata CPlHelper::GetHdrData(IMediaSample* pSample)
{
  
  Microsoft::WRL::ComPtr<IMediaSideData> pMediaSideData;
  HRESULT hr = pSample->QueryInterface(IID_PPV_ARGS(&pMediaSideData));
  m_hdr10 = {};
  if (FAILED(hr))
  {
    CLog::Log(LOGERROR, "{} Failed querying side data for hdr", __FUNCTION__);
    return m_hdr10.hdr10;
  }
  MediaSideDataHDR* hdr = nullptr;
  size_t size = 0;
  hr = pMediaSideData->GetSideData(IID_MediaSideDataHDR, (const BYTE**)&hdr, &size);
  if (SUCCEEDED(hr) && size == sizeof(MediaSideDataHDR))
  {
    m_hdr10.bValid = true;
    
    m_hdr10.hdr10.prim.red.x = static_cast<UINT16>(std::lround(hdr->display_primaries_x[2] * 50000.0));
    m_hdr10.hdr10.prim.red.y= static_cast<UINT16>(std::lround(hdr->display_primaries_y[2] * 50000.0));
    m_hdr10.hdr10.prim.green.x = static_cast<UINT16>(std::lround(hdr->display_primaries_x[0] * 50000.0));
    m_hdr10.hdr10.prim.green.x = static_cast<UINT16>(std::lround(hdr->display_primaries_y[0] * 50000.0));
    m_hdr10.hdr10.prim.blue.x = static_cast<UINT16>(std::lround(hdr->display_primaries_x[1] * 50000.0));
    m_hdr10.hdr10.prim.blue.x = static_cast<UINT16>(std::lround(hdr->display_primaries_y[1] * 50000.0));
    m_hdr10.hdr10.prim.white.x = static_cast<UINT16>(std::lround(hdr->white_point_x * 50000.0));
    m_hdr10.hdr10.prim.white.x = static_cast<UINT16>(std::lround(hdr->white_point_y * 50000.0));


    m_hdr10.hdr10.max_luma = static_cast<UINT>(std::lround(hdr->max_display_mastering_luminance * 10000.0));
    m_hdr10.hdr10.min_luma = static_cast<UINT>(std::lround(hdr->min_display_mastering_luminance * 10000.0));
  }

  MediaSideDataHDRContentLightLevel* hdrCLL = nullptr;
  size = 0;
  hr = pMediaSideData->GetSideData(IID_MediaSideDataHDRContentLightLevel, (const BYTE**)&hdrCLL, &size);
  if (SUCCEEDED(hr) && size == sizeof(MediaSideDataHDRContentLightLevel))
  {
    m_hdr10.hdr10.max_cll = hdrCLL->MaxCLL;
    m_hdr10.hdr10.max_fall = hdrCLL->MaxFALL;
  }
  
  MediaSideDataHDR10Plus* hdr10Plus = nullptr;
  size = 0;
  hr = pMediaSideData->GetSideData(IID_MediaSideDataHDR10Plus, (const BYTE**)&hdr10Plus, &size);
  if (SUCCEEDED(hr) && size == sizeof(MediaSideDataHDR10Plus))
  {
    float hist_max = 0;
    for (int x = 0; x < 3; x++)
      m_hdr10.hdr10.scene_max[x] = hdr10Plus->windows[0].maxscl[x];
    m_hdr10.hdr10.scene_avg = hdr10Plus->windows[0].average_maxrgb;

    for (int i = 0; i < hdr10Plus->windows[0].num_distribution_maxrgb_percentiles; i++) {
      float hist_val = hdr10Plus->windows[0].distribution_maxrgb_percentiles[i].percentile;
      if (hist_val > hist_max)
        hist_max = hist_val;
    }
    hist_max *= 10000;
    if (!m_hdr10.hdr10.scene_max[0])
      m_hdr10.hdr10.scene_max[0] = hist_max;
    if (!m_hdr10.hdr10.scene_max[1])
      m_hdr10.hdr10.scene_max[1] = hist_max;
    if (!m_hdr10.hdr10.scene_max[2])
      m_hdr10.hdr10.scene_max[2] = hist_max;
    
    if (hdr10Plus->windows[0].tone_mapping_flag == 1) {
      
      m_hdr10.hdr10.ootf.target_luma = hdr10Plus->targeted_system_display_maximum_luminance;
      m_hdr10.hdr10.ootf.knee_x = hdr10Plus->windows[0].knee_point_x;
      m_hdr10.hdr10.ootf.knee_y = hdr10Plus->windows[0].knee_point_y;
      //assert(pars->num_bezier_curve_anchors < 16);
      for (int i = 0; i < hdr10Plus->windows[0].num_bezier_curve_anchors; i++)
        m_hdr10.hdr10.ootf.anchors[i] = hdr10Plus->windows[0].bezier_curve_anchors[i];
      m_hdr10.hdr10.ootf.num_anchors = hdr10Plus->windows[0].num_bezier_curve_anchors;
    }
  }

  return m_hdr10.hdr10;
}


pl_rotation CPlHelper::GetPlRotation()
{
  return pl_rotation(); 
}


