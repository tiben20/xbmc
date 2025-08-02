#include "stdafx.h"
#include "SubManager.h"
#include <d3d9.h>
#include "..\subpic\ISubPic.h"
#include "..\subpic\DX9SubPic.h"
#include "..\subpic\DX11SubPic.h"
#include <moreuuids.h>
#include "..\subtitles\VobSubFile.h"
#include "..\subtitles\RTS.h"
#include "..\subtitles\RenderedHdmvSubtitle.h"
#include "..\DSUtil\NullRenderers.h"
#include "TextPassThruFilter.h"
#include "..\ILog.h"

BOOL g_overrideUserStyles;

CSubManager::CSubManager(ID3D11Device1* d3DDev, SIZE size, SSubSettings settings, HRESULT& hr) :
  m_d3DDev(d3DDev), m_iSubtitleSel(-1), m_rtNow(-1), m_lastSize(size),m_fps(0),
  m_rtTimePerFrame(0), m_bOverrideStyle(false), m_pSubPicProvider(NULL)
{
  if (! d3DDev)
  {
    hr = E_POINTER;
    return;
  }

  //memcpy(&m_settings, settings, sizeof(m_settings));
  /*m_settings.bufferAhead = settings.bufferAhead;
  m_settings.disableAnimations = settings.disableAnimations;
  m_settings.forcePowerOfTwoTextures = settings.forcePowerOfTwoTextures;
  m_settings.textureSize = settings.textureSize;*/
  m_settings = settings;
  m_bUseD3d9 = false;

  //g_log->Log(LOGDEBUG, "texture size {}x{}, buffer ahead: {}", m_settings.textureSize.cx, m_settings.textureSize.cy, m_settings.bufferAhead);
  m_pAllocator = new CDX11SubPicAllocator(d3DDev, m_settings.textureSize);
  hr = S_OK;
  if (m_settings.bufferAhead > 0)
    m_pSubPicQueue.reset(new CSubPicQueue(m_settings.bufferAhead, m_settings.disableAnimations, m_pAllocator, &hr));
  else
    m_pSubPicQueue.reset(new CSubPicQueueNoThread(m_pAllocator, &hr));
  if (FAILED(hr))
    g_log->Log(_LOGERROR, "CSubManager::CSubManager SubPicQueue creation error: %x",  hr);
}

CSubManager::CSubManager(IDirect3DDevice9* d3DDev, SIZE size, SSubSettings settings, HRESULT& hr) :
  m_d3D9Dev(d3DDev), m_iSubtitleSel(-1), m_rtNow(-1), m_lastSize(size), m_fps(0),
  m_rtTimePerFrame(0), m_bOverrideStyle(false), m_pSubPicProvider(NULL)
{
  if (!m_d3D9Dev)
  {
    hr = E_POINTER;
    return;
  }

  //memcpy(&m_settings, settings, sizeof(m_settings));
  /*m_settings.bufferAhead = settings.bufferAhead;
  m_settings.disableAnimations = settings.disableAnimations;
  m_settings.forcePowerOfTwoTextures = settings.forcePowerOfTwoTextures;
  m_settings.textureSize = settings.textureSize;*/
  m_settings = settings;
  m_bUseD3d9 = true;

  //g_log->Log(LOGDEBUG, "texture size {}x{}, buffer ahead: {}", m_settings.textureSize.cx, m_settings.textureSize.cy, m_settings.bufferAhead);

  m_pAllocator = new CDX9SubPicAllocator(m_d3D9Dev.Get(), m_settings.textureSize, true);

  hr = S_OK;
  if (m_settings.bufferAhead > 0)
    m_pSubPicQueue.reset(new CSubPicQueue(m_settings.bufferAhead, m_settings.disableAnimations, m_pAllocator, &hr));
  else
    m_pSubPicQueue.reset(new CSubPicQueueNoThread(m_pAllocator, &hr));
  if (FAILED(hr))
    g_log->Log(_LOGERROR, "CSubManager::CSubManager SubPicQueue creation error: %x", hr);
}

CSubManager::~CSubManager(void)
{
}

void CSubManager::StopThread()
{
  // Delete queue
  m_pSubPicQueue->GetSubPicProvider(&m_pSubPicProvider);
  m_pSubPicQueue.reset();
}

void CSubManager::StartThread(ID3D11Device1* pD3DDevice)
{
  HRESULT hr = S_OK;
  m_d3DDev = pD3DDevice;
  m_pAllocator->ChangeDevice(pD3DDevice);
  if (m_settings.bufferAhead > 0)
    m_pSubPicQueue.reset(new CSubPicQueue(m_settings.bufferAhead, m_settings.disableAnimations, m_pAllocator, &hr));
  else
    m_pSubPicQueue.reset(new CSubPicQueueNoThread(m_pAllocator, &hr));

  m_pSubPicQueue->SetSubPicProvider(m_pSubPicProvider);
}

void CSubManager::SetDeviceContext(ID3D11DeviceContext1* pDevContext)
{
  m_pAllocator->SetDeviceContext(pDevContext);
}

void CSubManager::SetStyle(SSubStyle* style, bool bOverride)
{
  m_style.SetDefault();

  m_style.charSet = style->charSet;
  m_style.fontSize = style->fontSize;
  m_style.fontWeight = style->fontWeight;
  m_style.fItalic = style->fItalic;
  memcpy(m_style.colors, style->colors, sizeof(COLORREF) * 4);
  memcpy(m_style.alpha, style->alpha, sizeof(BYTE) * 4);
  m_style.borderStyle = style->borderStyle;
  m_style.shadowDepthX = style->shadowDepthX;
  m_style.shadowDepthY = style->shadowDepthY;
  m_style.outlineWidthX = style->outlineWidthX;
  m_style.outlineWidthY = style->outlineWidthY;
  if (style->fontName)
    m_style.fontName = style->fontName;
  
  if (style->fontName)
    CoTaskMemFree(style->fontName);

  m_bOverrideStyle = bOverride;
}
void CSubManager::ApplyStyle(CRenderedTextSubtitle* pRTS)
{

}

void CSubManager::ApplyStyleSubStream(ISubStream* pSubStream)
{
  if (!pSubStream)
    return;

  CLSID clsid;
  if (FAILED(pSubStream->GetClassID(&clsid)))
    return;

  if(clsid == __uuidof(CRenderedTextSubtitle))
  {
    CRenderedTextSubtitle* pRTS = (CRenderedTextSubtitle*)(ISubStream*)pSubStream;

    pRTS->SetDefaultStyle(m_style);

    pRTS->SetOverride(m_bOverrideStyle, &m_style); 

    pRTS->Deinit();
  }
}

void CSubManager::SetSubPicProvider(ISubStream* pSubStream)
{
  ApplyStyleSubStream(pSubStream);

  m_pSubPicQueue->SetSubPicProvider(Com::SComQIPtr<ISubPicProvider>(pSubStream));
}

void CSubManager::SetTextPassThruSubStream(ISubStream* pSubStreamNew)
{
  ApplyStyleSubStream(pSubStreamNew);
  m_pInternalSubStream = pSubStreamNew;
  SetSubPicProvider(m_pInternalSubStream);
}

void CSubManager::InvalidateSubtitle(DWORD_PTR nSubtitleId, REFERENCE_TIME rtInvalidate)
{
  m_pSubPicQueue->Invalidate(rtInvalidate);
}

void CSubManager::UpdateSubtitle()
{
  ISubStream* pSubPic = NULL;
  if (SUCCEEDED(m_pSubPicQueue->GetSubPicProvider((ISubPicProvider**) &pSubPic)) && pSubPic)
  {
    ApplyStyleSubStream(pSubPic);
  }
}

void CSubManager::SetEnable(bool enable)
{
  if ((enable && m_iSubtitleSel < 0) || (!enable && m_iSubtitleSel >= 0))
  {
    m_iSubtitleSel ^= 0x80000000;
  }
}

void CSubManager::SetTime(REFERENCE_TIME rtNow)
{
  if (! m_pSubPicQueue)
    return;

  m_rtNow = rtNow;
  m_pSubPicQueue->SetTime(m_rtNow);
}


/*HRESULT CSubManager::GetTexture(Com::SComPtr<ID3D11Texture2D>& pTexture, Com::SmartRect& pSrc, Com::SmartRect& pDest, Com::SmartRect& renderRect)
{
  if (m_iSubtitleSel < 0)
    return E_INVALIDARG;

  if (m_rtTimePerFrame > 0 && m_pSubPicQueue.get())
  {
    m_fps = 10000000.0 / m_rtTimePerFrame;
    m_pSubPicQueue->SetFPS(m_fps);
  }

  Com::SmartSize renderSize(renderRect.right, renderRect.bottom);
  if (m_lastSize != renderSize && renderRect.right > 0 && renderRect.bottom > 0)
  { 
    m_pAllocator->ChangeDevice(m_d3DDev.Get());
    m_pAllocator->SetCurSize(renderSize);
    m_pAllocator->SetCurVidRect(renderRect);
    if (m_pSubPicQueue)
    {
      m_pSubPicQueue->Invalidate(m_rtNow + 100000000);
    }
    m_lastSize = renderSize;
  }

  Com::SComPtr<ISubPic> pSubPic;
  if(m_pSubPicQueue->LookupSubPic(m_rtNow, pSubPic))
  {
    if (SUCCEEDED (pSubPic->GetSourceAndDest(&renderSize, pSrc, pDest)))
    {
      return pSubPic->GetTexture(pTexture);
    }
  }

  return E_FAIL;
}*/

static bool IsTextPin(IPin* pPin)
{
  bool isText = false;
  BeginEnumMediaTypes(pPin, pEMT, pmt)
  {
    if (pmt->majortype == MEDIATYPE_Text || pmt->majortype == MEDIATYPE_Subtitle)
    {
      isText = true;
      break;
    }
  }
  EndEnumMediaTypes(pmt)
  return isText;
}

static bool isTextConnection(IPin* pPin)
{
  AM_MEDIA_TYPE mt;
  if (FAILED(pPin->ConnectionMediaType(&mt)))
    return false;
  bool isText = (mt.majortype == MEDIATYPE_Text || mt.majortype == MEDIATYPE_Subtitle);
  FreeMediaType(mt);
  return isText;
}

//load internal subtitles through TextPassThruFilter
HRESULT CSubManager::InsertPassThruFilter(IGraphBuilder* pGB)
{
  BeginEnumFilters(pGB, pEF, pBF)
  {
    if(!IsSplitter(pBF)) continue;

    BeginEnumPins(pBF, pEP, pPin)
    {
      PIN_DIRECTION pindir;
      pPin->QueryDirection(&pindir);
      if (pindir != PINDIR_OUTPUT)
        continue;
      Com::SComPtr<IPin> pPinTo;
      pPin->ConnectedTo(&pPinTo);
      if (pPinTo)
      {
        if (!isTextConnection(pPin))
          continue;
        pGB->Disconnect(pPin);
        pGB->Disconnect(pPinTo);
      }
      else if (!IsTextPin(pPin))
        continue;
      
      Com::SComQIPtr<IBaseFilter> pTPTF = new CTextPassThruFilter(this);
      CStdStringW name = L"Kodi Subtitles Pass Thru";
      if(FAILED(pGB->AddFilter(pTPTF, name)))
        continue;

      Com::SComQIPtr<ISubStream> pSubStream;
      HRESULT hr;
      do
      {
        if (FAILED(hr = pGB->ConnectDirect(pPin, GetFirstPin(pTPTF, PINDIR_INPUT), NULL)))
        {
          break;
        }
        Com::SComQIPtr<IBaseFilter> pNTR = new CNullTextRenderer(NULL, &hr);
        name = L"Kodi Null Renderer";
        if (FAILED(hr) || FAILED(pGB->AddFilter(pNTR, name)))
          break;

        if FAILED(hr = pGB->ConnectDirect(GetFirstPin(pTPTF, PINDIR_OUTPUT), GetFirstPin(pNTR, PINDIR_INPUT), NULL))
          break;
        pSubStream = pTPTF;
      } while(0);

      if (pSubStream)
      {
        //SetSubPicProvider(pSubStream);
        return S_OK;
      }
      else
      {
        pGB->RemoveFilter(pTPTF);
      }
    }
    EndEnumPins
  }
  EndEnumFilters
  
  return E_FAIL;
}

CStdString GetExtension(CStdString&  filename)
{
  const size_t i = filename.rfind('.');
  return filename.substr(i+1, filename.size());
}

HRESULT CSubManager::LoadExternalSubtitle( const wchar_t* subPath, ISubStream** pSubPic )
{
  if (!pSubPic)
    return E_POINTER;

  CStdStringW path(subPath);
  *pSubPic = NULL;
  try
  {
    Com::SComPtr<ISubStream> pSubStream;

    if(!pSubStream)
    {
      std::auto_ptr<CVobSubFile> pVSF(new CVobSubFile(&m_csSubLock));
      if(CStdString(GetExtension(path).ToLower()) == _T("idx") && pVSF.get() && pVSF->Open(path) && pVSF->GetStreamCount() > 0)
        pSubStream = pVSF.release();
    }

    if (!pSubStream)
    {
      std::auto_ptr<CRenderedHdmvSubtitleFile> pPGS(new CRenderedHdmvSubtitleFile(&m_csSubLock, ST_HDMV));
      if ((CStdString(GetExtension(path).ToLower()) == _T("pgs") || CStdString(GetExtension(path).ToLower()) == _T("sup"))
        && pPGS.get() && pPGS->Open(path))
        pSubStream = pPGS.release();
    }

    if(!pSubStream)
    {
      std::auto_ptr<CRenderedTextSubtitle> pRTS(new CRenderedTextSubtitle(&m_csSubLock));
      if(pRTS.get() && pRTS->Open(path, DEFAULT_CHARSET) && pRTS->GetStreamCount() > 0) {
        ApplyStyleSubStream(pRTS.get());
        pSubStream = pRTS.release();
      }
    }
    if (pSubStream)
    {
      *pSubPic = pSubStream.Detach();
      return S_OK;
    }
  }
  catch(... /*CException* e*/)
  {
    //e->Delete();
  }

  return E_FAIL;
}

void CSubManager::SetTimePerFrame( REFERENCE_TIME timePerFrame )
{
  m_rtTimePerFrame = timePerFrame;
}

void CSubManager::Free()
{
  m_pSubPicQueue.reset();
  m_pAllocator = NULL;
}

HRESULT CSubManager::SetSubPicProviderToInternal()
{
  if (! m_pInternalSubStream)
    return E_FAIL;
  
  SetSubPicProvider(m_pInternalSubStream);
  return S_OK;
}

HRESULT CSubManager::AlphaBlt(Com::SmartRect& pSrc, Com::SmartRect& pDest, Com::SmartRect& renderRect)
{
  if (m_iSubtitleSel < 0)
    return E_INVALIDARG;

  if (m_rtTimePerFrame > 0 && m_pSubPicQueue.get())
  {
    m_fps = 10000000.0 / m_rtTimePerFrame;
    m_pSubPicQueue->SetFPS(m_fps);
  }

  Com::SmartSize renderSize(renderRect.right, renderRect.bottom);
  if (m_lastSize != renderSize && renderRect.right > 0 && renderRect.bottom > 0)
  {
    m_pAllocator->ChangeDevice(m_d3DDev.Get());
    m_pAllocator->SetCurSize(renderSize);
    m_pAllocator->SetCurVidRect(renderRect);
    if (m_pSubPicQueue)
    {
      m_pSubPicQueue->Invalidate(m_rtNow + 100000000);
    }
    m_lastSize = renderSize;
  }

  Com::SComPtr<ISubPic> pSubPic;
  if (m_pSubPicQueue->LookupSubPic(m_rtNow, pSubPic))
  {
    if (SUCCEEDED(pSubPic->GetSourceAndDest(&renderSize, pSrc, pDest)))
    {
      return pSubPic->AlphaBlt(pSrc, pDest);
    }
  }


  return E_FAIL;
}

void CSubManager::SetTextureSize( Com::SmartSize& pSize )
{
  m_settings.textureSize = pSize;
  if (m_pAllocator)
  {
    m_pAllocator->SetMaxTextureSize(m_settings.textureSize);
    m_pSubPicQueue->Invalidate(m_rtNow + 1000000);
  }
}

HRESULT CSubManager::GetStreamTitle(ISubStream* pSubStream, wchar_t **subTitle)
{
  if (! pSubStream || !subTitle) return E_POINTER;

  CStdStringW title = "";
  *subTitle = NULL;

  CLSID clsid; pSubStream->GetClassID(&clsid);
  if (clsid == VOBFILE_SUBTITLE)
  {
    // vob, extract title
    title = ((CVobSubFile *) pSubStream)->GetLanguage();
    if (title.empty())
      return E_FAIL;

    // Alloc mem
    (*subTitle) = (wchar_t *) CoTaskMemAlloc(32);
    wcscpy_s(*subTitle, 16, title.c_str());

    return S_OK;
  }

  return E_FAIL;
}

void CSubManager::SetSizes(Com::SmartRect window, Com::SmartRect video)
{
  if(m_pAllocator)
  {
    m_pAllocator->SetCurSize(window.Size());
    m_pAllocator->SetCurVidRect(video);
  }

  if(m_pSubPicQueue)
  {
    m_pSubPicQueue->Invalidate();
  }
}