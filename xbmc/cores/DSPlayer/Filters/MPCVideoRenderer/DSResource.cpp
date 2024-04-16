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
/*          D3D Vertex Shader Class                 */
/****************************************************/
CD3DDSVertexShader::CD3DDSVertexShader()
{
  m_VS = nullptr;
  m_vertexLayout = nullptr;
  m_VSBuffer = nullptr;
  m_inputLayout = nullptr;
  m_vertexLayoutSize = 0;
  m_inited = false;
}

CD3DDSVertexShader::~CD3DDSVertexShader()
{
  Release();
}

void CD3DDSVertexShader::Release()
{
  Unregister();
  ReleaseShader();
  m_VSBuffer = nullptr;
  if (m_vertexLayout)
  {
    delete[] m_vertexLayout;
    m_vertexLayout = nullptr;
  }
}

void CD3DDSVertexShader::ReleaseShader()
{
  UnbindShader();

  m_VS = nullptr;
  m_inputLayout = nullptr;
  m_inited = false;
}

bool CD3DDSVertexShader::Create(const std::wstring& vertexFile, D3D11_INPUT_ELEMENT_DESC* vertexLayout, unsigned int vertexLayoutSize)
{
  ReleaseShader();

  ComPtr<ID3D11Device> pDevice = DX::DeviceResources::Get()->GetD3DDevice();

  if (!pDevice)
    return false;

  if (FAILED(D3DReadFileToBlob(vertexFile.c_str(), m_VSBuffer.ReleaseAndGetAddressOf())))
  {
    CLog::LogF(LOGERROR, "failed to load the vertex shader.");
    return false;
  }

  if (vertexLayout && vertexLayoutSize)
  {
    m_vertexLayoutSize = vertexLayoutSize;
    m_vertexLayout = new D3D11_INPUT_ELEMENT_DESC[vertexLayoutSize];
    for (unsigned int i = 0; i < vertexLayoutSize; ++i)
      m_vertexLayout[i] = vertexLayout[i];
  }
  else
    return false;

  m_inited = CreateInternal();

  if (m_inited)
    Register();

  return m_inited;
}

bool CD3DDSVertexShader::Create(const void* code, size_t codeLength, D3D11_INPUT_ELEMENT_DESC* vertexLayout, unsigned int vertexLayoutSize)
{
  ReleaseShader();

  ComPtr<ID3D11Device> pDevice = DX::DeviceResources::Get()->GetD3DDevice();

  if (!pDevice)
    return false;

  // trick to load bytecode into ID3DBlob
  if (FAILED(D3DStripShader(code, codeLength, D3DCOMPILER_STRIP_REFLECTION_DATA, m_VSBuffer.ReleaseAndGetAddressOf())))
  {
    CLog::LogF(LOGERROR, "failed to load the vertex shader.");
    return false;
  }

  if (vertexLayout && vertexLayoutSize)
  {
    m_vertexLayoutSize = vertexLayoutSize;
    m_vertexLayout = new D3D11_INPUT_ELEMENT_DESC[vertexLayoutSize];
    for (unsigned int i = 0; i < vertexLayoutSize; ++i)
      m_vertexLayout[i] = vertexLayout[i];
  }
  else
    return false;

  m_inited = CreateInternal();

  if (m_inited)
    Register();

  return m_inited;
}

bool CD3DDSVertexShader::CreateInternal()
{
  ComPtr<ID3D11Device> pDevice = DX::DeviceResources::Get()->GetD3DDevice();

  CLog::LogF(LOGDEBUG, "creating vertex shader.");

  // Create the vertex shader
  if (FAILED(pDevice->CreateVertexShader(m_VSBuffer->GetBufferPointer(), m_VSBuffer->GetBufferSize(), nullptr, m_VS.ReleaseAndGetAddressOf())))
  {
    CLog::LogF(LOGERROR, "failed to Create the vertex shader.");
    m_VSBuffer = nullptr;
    return false;
  }

  CLog::LogF(LOGDEBUG, "creating input layout.");

  if (FAILED(pDevice->CreateInputLayout(m_vertexLayout, m_vertexLayoutSize, m_VSBuffer->GetBufferPointer(), m_VSBuffer->GetBufferSize(), m_inputLayout.ReleaseAndGetAddressOf())))
  {
    CLog::LogF(LOGERROR, "failed to create the input layout.");
    return false;
  }

  return true;
}

void CD3DDSVertexShader::BindShader()
{
  if (!m_inited)
    return;

  ComPtr<ID3D11DeviceContext> pContext = DX::DeviceResources::Get()->GetD3DContext();
  if (!pContext)
    return;

  pContext->IASetInputLayout(m_inputLayout.Get());
  pContext->VSSetShader(m_VS.Get(), nullptr, 0);
}

void CD3DDSVertexShader::UnbindShader()
{
  if (!m_inited)
    return;

  ComPtr<ID3D11DeviceContext> pContext = DX::DeviceResources::Get()->GetD3DContext();
  pContext->IASetInputLayout(nullptr);
  pContext->VSSetShader(nullptr, nullptr, 0);
}

void CD3DDSVertexShader::OnCreateDevice()
{
  if (m_VSBuffer && !m_VS)
    m_inited = CreateInternal();
}

void CD3DDSVertexShader::OnDestroyDevice(bool fatal)
{
  ReleaseShader();
}

/****************************************************/
/*           D3D Pixel Shader Class                 */
/****************************************************/
CD3DDSPixelShader::CD3DDSPixelShader()
{
  m_PS = nullptr;
  m_PSBuffer = nullptr;
  m_inited = false;
}

CD3DDSPixelShader::~CD3DDSPixelShader()
{
  Release();
}

void CD3DDSPixelShader::Release()
{
  Unregister();
  ReleaseShader();
  m_PSBuffer = nullptr;
}

void CD3DDSPixelShader::ReleaseShader()
{
  m_inited = false;
  m_PS = nullptr;
}

bool CD3DDSPixelShader::LoadFromFile(std::string path)
{
  return false;
}

bool CD3DDSPixelShader::Create(const std::wstring& wstrFile)
{
  ReleaseShader();

  ComPtr<ID3D11Device> pDevice = DX::DeviceResources::Get()->GetD3DDevice();

  if (!pDevice)
    return false;

  if (FAILED(D3DReadFileToBlob(wstrFile.c_str(), m_PSBuffer.ReleaseAndGetAddressOf())))
  {
    CLog::LogF(LOGERROR, "Failed to load the vertex shader.");
    return false;
  }

  m_inited = CreateInternal();

  if (m_inited)
    Register();

  return m_inited;
}

bool CD3DDSPixelShader::Create(const void* code, size_t codeLength)
{
  ReleaseShader();

  ComPtr<ID3D11Device> pDevice = DX::DeviceResources::Get()->GetD3DDevice();

  if (!pDevice)
    return false;

  // trick to load bytecode into ID3DBlob
  if (FAILED(D3DStripShader(code, codeLength, D3DCOMPILER_STRIP_REFLECTION_DATA, m_PSBuffer.ReleaseAndGetAddressOf())))
  {
    CLog::LogF(LOGERROR, "Failed to load the vertex shader.");
    return false;
  }

  m_inited = CreateInternal();

  if (m_inited)
    Register();

  return m_inited;
}

bool CD3DDSPixelShader::CreateInternal()
{
  ComPtr<ID3D11Device> pDevice = DX::DeviceResources::Get()->GetD3DDevice();

  CLog::LogF(LOGDEBUG, "Create the pixel shader.");

  // Create the vertex shader
  if (FAILED(pDevice->CreatePixelShader(m_PSBuffer->GetBufferPointer(), m_PSBuffer->GetBufferSize(), nullptr, m_PS.ReleaseAndGetAddressOf())))
  {
    CLog::LogF(LOGERROR, "Failed to Create the pixel shader.");
    m_PSBuffer = nullptr;
    return false;
  }

  return true;
}

void CD3DDSPixelShader::BindShader()
{
  if (!m_inited)
    return;

  ComPtr<ID3D11DeviceContext> pContext = DX::DeviceResources::Get()->GetD3DContext();
  if (!pContext)
    return;

  pContext->PSSetShader(m_PS.Get(), nullptr, 0);
}

void CD3DDSPixelShader::UnbindShader()
{
  if (!m_inited)
    return;

  ComPtr<ID3D11DeviceContext> pContext = DX::DeviceResources::Get()->GetD3DContext();
  pContext->IASetInputLayout(nullptr);
  pContext->VSSetShader(nullptr, nullptr, 0);
}

void CD3DDSPixelShader::OnCreateDevice()
{
  if (m_PSBuffer && !m_PS)
    m_inited = CreateInternal();
}

void CD3DDSPixelShader::OnDestroyDevice(bool fatal)
{
  ReleaseShader();
}

bool CD3DDSVertexShader::LoadFromFile(std::string path)
{
  if (!XFILE::CFile::Exists(path))
    return false;
  std::string newpath = CSpecialProtocol::TranslatePath(path);
  if (FAILED(D3DReadFileToBlob(AToW(newpath).c_str(), m_VSBuffer.ReleaseAndGetAddressOf())))
  {
    CLog::LogF(LOGERROR, "failed to load the vertex shader.");
    return false;
  }
  m_data = m_VSBuffer->GetBufferPointer();
  m_size = m_VSBuffer->GetBufferSize();
}
