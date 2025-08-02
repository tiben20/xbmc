#pragma once

#include "ISubManager.h"
#include "../subpic/ISubPic.h"
#include "../subtitles/STS.h"
#include "wrl/client.h"

class CRenderedTextSubtitle;

extern BOOL g_overrideUserStyles;

class CSubManager: public ISubManager
{
public:
  CSubManager(ID3D11Device1* d3DDev, SIZE size, SSubSettings settings, HRESULT& hr);
  CSubManager(IDirect3DDevice9* d3DDev, SIZE size, SSubSettings settings, HRESULT& hr);
  ~CSubManager(void);

  void SetEnable(bool enable);

  void SetTimePerFrame(REFERENCE_TIME timePerFrame);
  void SetTime(REFERENCE_TIME rtNow);

  void SetStyle(SSubStyle* style, bool bOverride);
  void SetSubPicProvider(ISubStream* pSubStream);
  void SetTextureSize(Com::SmartSize& pSize);
  void SetSizes(Com::SmartRect window, Com::SmartRect video);

  void StopThread();
  void StartThread(ID3D11Device1* pD3DDevice);

  void SetDeviceContext(ID3D11DeviceContext1* pDevContext);

  //load internal subtitles through TextPassThruFilter
  HRESULT InsertPassThruFilter(IGraphBuilder* pGB);
  HRESULT LoadExternalSubtitle(const wchar_t* subPath, ISubStream** pSubPic);
  HRESULT SetSubPicProviderToInternal();

  HRESULT AlphaBlt(Com::SmartRect& pSrc, Com::SmartRect& pDest, Com::SmartRect& renderRect);

  void Free();

  HRESULT GetStreamTitle(ISubStream* pSubStream, wchar_t **subTitle);

private:
  friend class CTextPassThruInputPin;
  friend class CTextPassThruFilter;
  //SetTextPassThruSubStream, InvalidateSubtitle are called from CTextPassThruInputPin
  void SetTextPassThruSubStream(ISubStream* pSubStreamNew);
  void InvalidateSubtitle(DWORD_PTR nSubtitleId, REFERENCE_TIME rtInvalidate);

  void UpdateSubtitle();
  void ApplyStyle(CRenderedTextSubtitle* pRTS);
  void ApplyStyleSubStream(ISubStream* pSubStream);

  CCritSec m_csSubLock; 
  int m_iSubtitleSel; // if(m_iSubtitleSel&(1<<31)): disabled
  REFERENCE_TIME m_rtNow; //current time
  double m_fps;
  
  STSStyle m_style;
  bool m_bOverrideStyle;

  SSubSettings m_settings;
  Com::SmartSize m_lastSize;

  std::shared_ptr<ISubPicQueue> m_pSubPicQueue;
  Com::SComPtr<ISubPicAllocator> m_pAllocator;

  Com::SComQIPtr<IAMStreamSelect> m_pSS; //graph filter with subtitles
  Microsoft::WRL::ComPtr<IDirect3DDevice9> m_d3D9Dev;
  Microsoft::WRL::ComPtr<ID3D11Device1> m_d3DDev;
  Com::SComPtr<ISubStream> m_pInternalSubStream;
  ISubPicProvider* m_pSubPicProvider; // save when changing d3d device

  bool m_bUseD3d9;

  REFERENCE_TIME m_rtTimePerFrame;

  //CSubresync m_subresync;
};