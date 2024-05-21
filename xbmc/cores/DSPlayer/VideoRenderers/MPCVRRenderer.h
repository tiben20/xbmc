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
#include "cores/VideoSettings.h"
#include "guilib/D3DResource.h"
#include "../VideoPlayer/VideoRenderers/BaseRenderer.h"
#include "Filters/MPCVideoRenderer/ShadersLoader.h"
#include "Filters/MPCVideoRenderer/ShaderFactory.h"
#include "Filters/MPCVideoRenderer/scaler.h"
#include <vector>

#include <d3d11_4.h>
#include <dxgi1_5.h>

class CD3D11DynamicScaler;

struct DS_VERTEX {
  DirectX::XMFLOAT3 Pos;
  DirectX::XMFLOAT2 TexCoord;
};

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

  void LoadShaders();
  void InitShaders();

  void Start(uint32_t passcount);
  void Stop();
  void OnBeginEffects();
  void OnEndEffects();
  void OnEndPass();
  void CopyToBackBuffer();
  void DrawSubtitles();

  void Reset();
  //samples,shader and unorderer access views saved in maps to not duplicate access views
  ID3D11SamplerState* GetSampler(D3D11_FILTER filterMode, D3D11_TEXTURE_ADDRESS_MODE addressMode);
  ID3D11ShaderResourceView* GetShaderResourceView(ID3D11Texture2D* texture);

  ID3D11UnorderedAccessView* GetUnorderedAccessView(ID3D11Texture2D* texture);

  ID3D11UnorderedAccessView* GetUnorderedAccessView(ID3D11Buffer* buffer, uint32_t numElements, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);

  CD3DTexture GetIntermediateTexture(){ return m_IntermediateTarget; }
  CD3DTexture GetInputTexture(bool forcopy, REFERENCE_TIME subtime = 0)
  {
    if (subtime > 0)
      m_rSubTime = subtime;
    if (forcopy)
      m_bImageProcessed = false;
    return m_InputTarget; 
  }
  bool CreateInputTarget(unsigned int width, unsigned int height, DXGI_FORMAT format);
  
protected:

  bool CreateIntermediateTarget(unsigned int width,
                                unsigned int height,
                                bool dynamic = false,
                                DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
  
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



  Microsoft::WRL::ComPtr<ID3D11Query> m_pDisjointQuery;
  Microsoft::WRL::ComPtr<ID3D11Query> m_pStartQuery;
  std::vector<Microsoft::WRL::ComPtr<ID3D11Query>> m_pPassQueries;
  uint32_t m_pPasses;
  uint32_t m_pCurrentPasses = 0;
  std::vector<CD3D11DynamicScaler*> m_pShaders;
  phmap::flat_hash_map<std::pair<D3D11_FILTER, D3D11_TEXTURE_ADDRESS_MODE>, Microsoft::WRL::ComPtr<ID3D11SamplerState>> m_pSamplers;
  phmap::flat_hash_map<ID3D11Texture2D*, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_pShaderRSV;
  phmap::flat_hash_map<void*, Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView>> m_pUAVViews;

  CD3DTexture m_InputTarget;
  CD3DTexture m_IntermediateTarget;

  CRect GetScreenRect() const;
private:
  HRESULT FillVertexBuffer(const UINT srcW, const UINT srcH, const CRect& srcRect, const int iRotation, const bool bFlip);
  void FillVertices(DS_VERTEX(&Vertices)[4], const UINT srcW, const UINT srcH, const CRect& srcRect, const int iRotation, const bool bFlip);
  HRESULT CreateVertexBuffer();
};
