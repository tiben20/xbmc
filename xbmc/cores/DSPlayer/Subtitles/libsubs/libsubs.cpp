#include "stdafx.h"
#include <d3d9.h>
#include <d3d11.h>
#include "libsubs.h"
#include "..\subtitles\RTS.h"
#include "SubManager.h"

/*
void SetDefaultStyle(const SubtitleStyle* s, BOOL overrideUserStyles)
{
  g_style.fontName = s->fontName;
  g_style.colors[0] = s->fontColor;
  g_style.fontWeight = (s->fontIsBold ? 700 : 400);
  g_style.fontSize = s->fontSize; 
  g_style.charSet = s->fontCharset;
  g_style.shadowDepthX  = g_style.shadowDepthY = s->shadow;
  g_style.outlineWidthX = g_style.outlineWidthY = s->borderWidth;
  g_style.borderStyle = (s->isBorderOutline ? 0 : 1); // 0: outline, 1: opaque box
  g_overrideUserStyles = overrideUserStyles;
}

void SetAdvancedOptions(int subPicsBufferAhead, SIZE textureSize, BOOL pow2tex, BOOL disableAnim)
{
  g_subPicsBufferAhead = subPicsBufferAhead;
  g_textureSize = textureSize;
  g_pow2tex = pow2tex != 0;
  g_disableAnim = disableAnim;
}*/

/*
BOOL LoadSubtitles(IDirect3DDevice9* d3DDev, SIZE size, const wchar_t* fn, IGraphBuilder* pGB, const wchar_t* paths, ISubManager** manager)
{
  *manager = NULL;
  g_subManager.reset();
  HRESULT hr = S_OK;
  CSubManager *subManager(new CSubManager(d3DDev, size, hr));
  if (FAILED(hr))
  {
    delete subManager;
    return FALSE;
  }
  subManager->LoadSubtitlesForFile(fn, pGB, paths);
  g_subManager.reset(subManager);
  *manager = g_subManager.get();
  return TRUE;
}*/

ILog* g_log = NULL;

bool CreateD3D11SubtitleManager(ID3D11Device1* pDevice, SIZE size, ILog* logger, SSubSettings settings, ISubManager** pManager)
{
  logger->Log(_LOGINFO, "Starting CreateD3D11SubtitleManager");
  if (!logger)
    return false;
  if (!pManager)
    logger->Log(_LOGERROR, "CreateSubtitleManager pManager is null");
  if (!pDevice)
    logger->Log(_LOGERROR, "CreateSubtitleManager ID3D11Device is null");
  if (!pManager || !pDevice)
    return false;

  *pManager = NULL;
  g_log = logger;

  HRESULT hr = S_OK;
  //TODO d3d11
  *pManager = new CSubManager(pDevice, size, settings, hr);
  if (FAILED(hr))
  {
    delete* pManager;
    *pManager = NULL;
    logger->Log(_LOGERROR, "Failed to create subtitles manager (hr: %X)", hr);
    return false;
  }

  return true;
}

/*bool CreateSubtitleManager(IDirect3DDevice9* d3DDev, SIZE size, ILog* logger, SSubSettings settings, ISubManager** pManager)
{
  if (!logger)
    return false;
  if (!pManager)
    logger->Log(LOGERROR, "CreateSubtitleManager pManager is null");
  if (!d3DDev)
    logger->Log(LOGERROR, "CreateSubtitleManager d3DDev is null");
  if (!pManager || !d3DDev)
    return false;

  *pManager = NULL;
  g_log = logger;

  HRESULT hr = S_OK;
  *pManager = new CSubManager(d3DDev, size, settings, hr);
  if (FAILED(hr))
  {
    delete *pManager;
    *pManager = NULL;
    logger->Log(LOGERROR, "Failed to create subtitles manager (hr: %X)", hr);
    return false;
  }

  return true;
}*/

bool DeleteSubtitleManager(ISubManager * pManager)
{
  if (pManager)
    delete pManager;

  g_log = NULL;

  return true;
}