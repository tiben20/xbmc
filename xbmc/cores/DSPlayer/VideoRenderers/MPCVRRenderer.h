/*
 *  Copyright (C) 2017-2019 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */
#pragma once

#include "VideoRenderers/ColorManager.h"
#include "VideoRenderers/DebugInfo.h"
#include "VideoRenderers/RenderInfo.h"
#include "VideoRenderers/VideoShaders/WinVideoFilter.h"
#include "cores/VideoSettings.h"
#include "guilib/D3DResource.h"
#include "../VideoPlayer/VideoRenderers/BaseRenderer.h"

#include <vector>

#include <d3d11_4.h>
#include <dxgi1_5.h>


class CMPCVRRenderer : public CBaseRenderer
{
public:
  static std::shared_ptr<CMPCVRRenderer> Get();
  CMPCVRRenderer();
  virtual ~CMPCVRRenderer();

  void Release();

  void Render(int index, int index2, CD3DTexture& target, const CRect& sourceRect, 
              const CRect& destRect, const CRect& viewRect, unsigned flags);
  void Render(CD3DTexture& target, const CRect& sourceRect, const CRect& destRect, 
              const CRect& viewRect, unsigned flags = 0);

  // Player functions
  virtual bool Configure(const VideoPicture& picture, float fps, unsigned int orientation) override;
  virtual bool         IsConfigured() { return m_bConfigured; }
  void AddVideoPicture(const VideoPicture& picture, int index) override {}
  bool IsPictureHW(const VideoPicture& picture) override { return false; }
  void UnInit() override {}
  bool Flush(bool saveBuffers) override;
  void SetBufferSize(int numBuffers) override { }
  void ReleaseBuffer(int idx) override { }
  bool NeedBuffer(int idx) override { return false; }
  bool IsGuiLayer() override { return true; }
  // Render info, can be called before configure
  CRenderInfo GetRenderInfo() override { return CRenderInfo(); }
  void Update() override { }
  void RenderUpdate(int index, int index2, bool clear, unsigned int flags, unsigned int alpha) override;
  bool RenderCapture(int index, CRenderCapture* capture) override { return false; }
  bool ConfigChanged(const VideoPicture& picture) override { return false; }

  bool Configure(unsigned int width, unsigned int height, unsigned int d_width, unsigned int d_height, float fps);
  CD3DTexture& GetIntermediateTarget();


  // Feature support
  virtual bool SupportsMultiPassRendering() override { return false; }
  virtual bool Supports(ERENDERFEATURE feature) const override { return false; }
  virtual bool Supports(ESCALINGMETHOD method) const override { return false; }

  bool WantsDoublePass() override { return false; }

  /*! \brief Get video rectangle and view window
  \param source is original size of the video
  \param dest is the target rendering area honoring aspect ratio of source
  \param view is the entire target rendering area for the video (including black bars)
  */
  void GetVideoRect(CRect& source, CRect& dest, CRect& view) const;
  float GetAspectRatio() const;

  static void SettingOptionsRenderMethodsFiller(const std::shared_ptr<const CSetting>& setting,
    std::vector<IntegerSettingOption>& list,
    int& current,
    void* data);

  void SetVideoSettings(const CVideoSettings& settings);

  // Gets debug info from render buffer
  DEBUG_INFO_VIDEO GetDebugInfo(int idx) { return {}; }

  CRenderCapture* GetRenderCapture() { return nullptr; }
protected:

  bool CreateIntermediateTarget(unsigned int width,
                                unsigned int height,
                                bool dynamic = false,
                                DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
  
  virtual void CheckVideoParameters();
  
  bool m_bConfigured = false;
  int m_iBufferIndex = 0;
  int m_iNumBuffers = 0;
  int m_iBuffersRequired = 0;
  int m_ditherDepth = 0;
  int m_cmsToken = -1;
  int m_lutSize = 0;
  unsigned m_sourceWidth = 0;
  unsigned m_sourceHeight = 0;
  unsigned m_viewWidth = 0;
  unsigned m_viewHeight = 0;
  unsigned m_renderOrientation = 0;
  
  CD3DTexture m_IntermediateTarget;
  CRect GetScreenRect() const;
};
