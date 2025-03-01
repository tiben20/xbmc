/*
 *  Copyright (C) 2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */
#pragma once

#include "VideoRenderers/ColorManager.h"
#include "VideoRenderers/DebugInfo.h"
#include "VideoRenderers/RenderInfo.h"
#include "cores/VideoSettings.h"
#include "guilib/D3DResource.h"
#include "../VideoPlayer/VideoRenderers/BaseRenderer.h"
#include "../Filters/MPCVideoRenderer/PlHelper.h"
#include "Filters/MPCVideoRenderer/D3D11Font.h"
#include "Filters/MPCVideoRenderer/D3D11Geometry.h"
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

#include <d3d11_4.h>
#include <dxgi1_5.h>


#define MPC_SETTINGS static_cast<CMPCVRSettings*>(g_dsSettings.pRendererSettings)
struct DS_VERTEX {
  DirectX::XMFLOAT3 Pos;
  DirectX::XMFLOAT2 TexCoord;
};


// maps unsigned 8 bits/channel to D3DCOLOR
#define COLOR_ARGB(a,r,g,b) \
    ((KODI::UTILS::COLOR::Color)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))
#define COLOR_RGBA(r,g,b,a) COLOR_ARGB(a,r,g,b)
#define COLOR_XRGB(r,g,b)   COLOR_ARGB(0xff,r,g,b)

// Thread-safe queue template
template <typename T>
class ThreadSafeQueue {
private:
  std::queue<T> queue_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
public:
  void push(T value) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push(std::move(value));
    }
    condition_.notify_one();
  }
  
  void pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.pop();
  }

  bool empty() {
    return queue_.empty();
  }

  int size() {
    return queue_.size();
  }

  T front() {
    return queue_.front();
  }

  void wait_and_pop(T& result) {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return !queue_.empty(); });
    result = std::move(queue_.front());
    queue_.pop();
  }

  void flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<T> empty;
    std::swap(queue_, empty);
  }
};

struct CMPCVRFrame
{
  CD3DTexture pTexture;
  REFERENCE_TIME pStartTime;
  REFERENCE_TIME pEndTime;
  REFERENCE_TIME pUploadTime;
  REFERENCE_TIME pMergingTime;
};

class CMPCVRRenderer : public CBaseRenderer
{
public:
  static std::shared_ptr<CMPCVRRenderer> Get();
  CMPCVRRenderer();
  virtual ~CMPCVRRenderer();
  void Init();

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

  void CopyToBackBuffer(ID3D11Texture2D* intext);
  void DrawSubtitles();
  void DrawStats();
  void SetStats(CStdStringW thetext) { m_statsText = thetext; };

  void Reset();
  
  PL::CPlHelper* GetPlHelper() { return m_pPlacebo; };

  CD3DTexture GetIntermediateTexture(){ return m_IntermediateTarget; }
 
  void SetCallback(IMpcVRCallback* callback) { pMpcCallback = callback; };

  bool CreateIntermediateTarget(unsigned int width,
    unsigned int height,
    bool dynamic = false,
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
protected:

  
  

  virtual void CheckVideoParameters();
  
  bool m_bConfigured = false;
  bool m_bImageProcessed = false;
  UINT m_iRedraw = 0;
  int m_iBufferIndex = 0;
  int m_iNumBuffers = 0;
  int m_iBuffersRequired = 0;
  int m_ditherDepth = 0;
  int m_cmsToken = -1;
  int m_lutSize = 0;
  REFERENCE_TIME m_rSubTime;
  unsigned m_sourceWidth = 0;
  unsigned m_sourceHeight = 0;
  unsigned m_viewWidth = 0;
  unsigned m_viewHeight = 0;
  unsigned m_renderOrientation = 0;
  
  Microsoft::WRL::ComPtr<ID3D11VertexShader>   m_pVS_Simple;
  Microsoft::WRL::ComPtr<ID3D11PixelShader>    m_pPS_Simple;
  Microsoft::WRL::ComPtr<ID3D11PixelShader>    m_pPS_BitmapToFrame;
  Microsoft::WRL::ComPtr<ID3D11InputLayout>    m_pVSimpleInputLayout;
  Microsoft::WRL::ComPtr<ID3D11Buffer>         m_pVertexBuffer;
  Microsoft::WRL::ComPtr<ID3D11SamplerState>   m_pSampler;

  //stats drawing
  void SetGraphSize();
  CD3D11Rectangle m_StatsBackground;
  CD3D11Font      m_Font3D;
  CD3D11Rectangle m_Rect3D;
  CD3D11Rectangle m_Underlay;
  CD3D11Lines     m_Lines;
  CD3D11Polyline  m_SyncLine;
  int m_StatsFontH = 14;
  RECT m_StatsRect = { 10, 10, 10 + 5 + 63 * 8 + 3, 10 + 5 + 18 * 17 + 3 };
  int  m_iResizeStats = 0;
  int m_Xstep = 4;
  int m_Yscale = 2;
  RECT m_GraphRect = {};
  int m_Yaxis = 0;
  CStdStringW m_statsText;
  const POINT m_StatsTextPoint = { 10 + 5, 10 + 5 };
  KODI::UTILS::COLOR::Color m_dwStatsTextColor = COLOR_XRGB(255, 255, 255);
  CRect m_screenRect;


  PL::CPlHelper* m_pPlacebo;
  CD3DTexture m_IntermediateTarget;

  CRect GetScreenRect() const;
private:
  HRESULT FillVertexBuffer(const UINT srcW, const UINT srcH, const CRect& srcRect, const int iRotation, const bool bFlip);
  void FillVertices(DS_VERTEX(&Vertices)[4], const UINT srcW, const UINT srcH, const CRect& srcRect, const int iRotation, const bool bFlip);
  HRESULT CreateVertexBuffer();


  IMpcVRCallback* pMpcCallback;
};
