/*
 *  Copyright (C) 2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "libplacebo/log.h"
#include "libplacebo/renderer.h"
#include "libplacebo/d3d11.h"
#include <libplacebo/options.h>
#include "libplacebo/utils/frame_queue.h"
#include "libplacebo/utils/upload.h"
#include "libplacebo/colorspace.h"


#pragma comment(lib, "libplacebo-349.lib")

#include <string>
#include <strmif.h>
#include <d3d9types.h>
#include <dxva2api.h>

#define MAX_FRAME_PASSES 256
#define MAX_BLEND_PASSES 8
#define MAX_BLEND_FRAMES 8

namespace PL
{
  // playback statistics
  struct RenderStats {
    uint32_t decoded;
    uint32_t rendered;
    uint32_t mapped;
    uint32_t dropped;
    uint32_t missed;
    uint32_t stalled;
    double missed_ms;
    double stalled_ms;
    double current_pts;

    struct timing {
      double sum, sum2, peak;
      uint64_t count;
    } acquire, update, render, draw_ui, sleep, submit, swap,
      vsync_interval, pts_interval;
  };

  class CPlHelper
  {
  public:
    CPlHelper();
    virtual ~CPlHelper();

    bool Init(DXGI_FORMAT fmt);
    void Release();

    pl_frame CreateFrame(DXVA2_ExtendedFormat pFormat, IMediaSample* pSample, int width, int height);
    pl_color_repr GetPlColorRepresentation(DXVA2_ExtendedFormat pFormat);
    pl_color_space GetPlColorSpace(DXVA2_ExtendedFormat pFormat);

    //TODO
    pl_rotation GetPlRotation();

    pl_d3d11 GetPLD3d11() { return m_plD3d11; };
    pl_renderer GetPLRenderer() { return m_plRenderer; };


  protected:
    //options
    pl_options m_plOptions;
    pl_rotation target_rot;


    pl_log m_plLog = nullptr;
    pl_d3d11 m_plD3d11 = nullptr;
    pl_swapchain m_plSwapchain = nullptr;
    pl_renderer m_plRenderer = nullptr;
    pl_tex m_plTextures[PL_MAX_PLANES] = {};
    pl_color_space m_plLastColorspace = {};
    // Pending swapchain state shared between waitToRender(), renderFrame(), and cleanupRenderContext()
    pl_swapchain_frame m_SwapchainFrame = {};

    //queue
    pl_queue m_pQueue;

    //caching system TODO
    pl_cache m_pCache;
    uint64_t m_pCacheSignature;//signature of the current cache
    std::string m_pCacheFile;

    // ICC profile
    pl_icc_object m_pIcc;
    std::string m_sIccNname;
    bool m_bUseIccLuma;
    bool m_bForceBpc;

    // custom shaders
    const struct pl_hook** shader_hooks;
    char** shader_paths;
    size_t shader_num;
    size_t shader_size;

    // pass metadata
    struct pl_dispatch_info blend_info[MAX_BLEND_FRAMES][MAX_BLEND_PASSES];
    struct pl_dispatch_info frame_info[MAX_FRAME_PASSES];
    int num_frame_passes;
    int num_blend_passes[MAX_BLEND_FRAMES];
  };


};