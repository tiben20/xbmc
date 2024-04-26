/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DSResource.h"

#include "filesystem/File.h"
#include "rendering/dx/DeviceResources.h"
#include "rendering/dx/RenderContext.h"
#include "utils/log.h"

#include <d3dcompiler.h>
#include "DSResource.h"
#include "DSUtil/DSUtil.h"
#include "filesystem/SpecialProtocol.h"

using namespace DirectX;
using namespace Microsoft::WRL;

/****************************************************/
/*           D3D Pixel Shader Class                 */
/****************************************************/
CD3DDSPixelShader::CD3DDSPixelShader()
{
  
}

CD3DDSPixelShader::~CD3DDSPixelShader()
{
  m_PSBuffer = nullptr;
}


bool CD3DDSPixelShader::LoadFromFile(std::string path)
{
  if (!XFILE::CFile::Exists(path))
    return false;
  std::string newpath = CSpecialProtocol::TranslatePath(path);
  m_path = newpath;
  if (FAILED(D3DReadFileToBlob(AToW(newpath).c_str(), &m_PSBuffer)))
  {
    CLog::LogF(LOGERROR, "{} failed to load the vertex shader.", __FUNCTION__);
    return false;
  }
  m_data = m_PSBuffer->GetBufferPointer();
  m_size = m_PSBuffer->GetBufferSize();
  return true;
}
