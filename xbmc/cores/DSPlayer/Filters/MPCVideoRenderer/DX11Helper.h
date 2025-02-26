/*
* (C) 2019-2022 see Authors.txt
*
* This file is part of MPC-BE. But modified for Kodi
*
* Kodi is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* MPC-BE is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#pragma once

#include <d3d11_1.h>
#include "rendering/dx/DeviceResources.h"
#include "Helper.h"
enum Tex2DType {
	Tex2D_Default,
	Tex2D_DefaultRTarget,
	Tex2D_DefaultShader,
	Tex2D_DefaultShaderRTarget,
	Tex2D_DynamicShaderWrite,
	Tex2D_DynamicShaderWriteNoSRV,
	Tex2D_StagingRead,
};

D3D11_TEXTURE2D_DESC CreateTex2DDesc(const DXGI_FORMAT format, const UINT width, const UINT height, const Tex2DType type);

struct Tex2D_t
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture;
	D3D11_TEXTURE2D_DESC desc = {};
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pShaderResource;

	HRESULT CheckCreate(ID3D11Device* pDevice, const DXGI_FORMAT format, const UINT width, const UINT height, const Tex2DType type) {
		if (!width || !height) {
			return E_FAIL;
		}

		if (format == desc.Format && width == desc.Width && height == desc.Height) {
			return S_OK;
		}

		return Create(pDevice, format, width, height, type);
	}

	HRESULT Create(ID3D11Device* pDevice, const DXGI_FORMAT format, const UINT width, const UINT height, const Tex2DType type) {
		Release();
		D3D11_TEXTURE2D_DESC texdesc = CreateTex2DDesc(format, width, height, type);

		HRESULT hr = pDevice->CreateTexture2D(&texdesc, nullptr, &pTexture);
		if (S_OK == hr) {
			pTexture->GetDesc(&desc);
			D3DSetDebugName(pTexture.Get(), "Tex2D_t");
			if (type != Tex2D_DynamicShaderWriteNoSRV && (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)) {
				D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc;
				shaderDesc.Format = format;
				shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				shaderDesc.Texture2D.MostDetailedMip = 0; // = Texture2D desc.MipLevels - 1
				shaderDesc.Texture2D.MipLevels = 1;       // = Texture2D desc.MipLevels

				hr = pDevice->CreateShaderResourceView(pTexture.Get(), &shaderDesc, &pShaderResource);
				D3DSetDebugName(pShaderResource.Get(), "Tex2D_t ShaderResourceView");
				if (FAILED(hr)) {
					Release();
				}
			}
		}

		return hr;
	}

	virtual void Release() {
		pShaderResource= nullptr;
		pTexture= nullptr;
		desc = {};
	}
};

struct Tex11Video_t : Tex2D_t
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture2;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture3;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pShaderResource2;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pShaderResource3;

	HRESULT CreateEx(ID3D11Device* pDevice, const DXGI_FORMAT format, const DX11PlaneConfig_t* pPlanes, UINT width, const UINT height, const Tex2DType type) {
		Release();
		D3D11_TEXTURE2D_DESC texdesc = CreateTex2DDesc(format, width, height, type);

		HRESULT hr = pDevice->CreateTexture2D(&texdesc, nullptr, &pTexture);
		if (S_OK == hr) {
			pTexture->GetDesc(&desc);

			if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
				D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc;
				shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				shaderDesc.Texture2D.MostDetailedMip = 0;
				shaderDesc.Texture2D.MipLevels = 1;

				if (pPlanes) {
					// 1 texture, 2 SRV
					shaderDesc.Format = pPlanes->FmtPlane1;
					hr = pDevice->CreateShaderResourceView(pTexture.Get(), &shaderDesc, &pShaderResource);
					if (S_OK == hr && pPlanes->FmtPlane2) {
						shaderDesc.Format = pPlanes->FmtPlane2;
						hr = pDevice->CreateShaderResourceView(pTexture.Get(), &shaderDesc, &pShaderResource2);
					}
				} else {
					// 1 texture, 1 SRV
					shaderDesc.Format = format;
					hr = pDevice->CreateShaderResourceView(pTexture.Get(), &shaderDesc, &pShaderResource);
				}
			}
		}
		else if (pPlanes) {
			if (format == DXGI_FORMAT_YUY2) {
				width /= 2;
			}
			hr = Create(pDevice, pPlanes->FmtPlane1, width, height, type);

			if (S_OK == hr && pPlanes->FmtPlane2) {
				// 2 textures, 2 SRV
				const UINT chromaWidth = width / pPlanes->div_chroma_w;
				const UINT chromaHeight = height / pPlanes->div_chroma_h;
				texdesc = CreateTex2DDesc(pPlanes->FmtPlane2, chromaWidth, chromaHeight, type);

				hr = pDevice->CreateTexture2D(&texdesc, nullptr, &pTexture2);
				if (S_OK == hr) {
					D3D11_SHADER_RESOURCE_VIEW_DESC shaderDesc;
					shaderDesc.Format = pPlanes->FmtPlane2;
					shaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					shaderDesc.Texture2D.MostDetailedMip = 0;
					shaderDesc.Texture2D.MipLevels = 1;
					hr = pDevice->CreateShaderResourceView(pTexture2.Get(), &shaderDesc, &pShaderResource2);

					if (S_OK == hr && pPlanes->FmtPlane3) {
						// 3 textures, 3 SRV
						texdesc = CreateTex2DDesc(pPlanes->FmtPlane3, chromaWidth, chromaHeight, type);

						hr = pDevice->CreateTexture2D(&texdesc, nullptr, &pTexture3);
						if (S_OK == hr) {
							shaderDesc.Format = pPlanes->FmtPlane3;
							hr = pDevice->CreateShaderResourceView(pTexture3.Get(), &shaderDesc, &pShaderResource3);
						}
					}
				}
			}
		}

		if (FAILED(hr)) {
			Release();
		}

		return hr;
	}

	void Release() override {
		pShaderResource3= nullptr;
		pTexture3= nullptr;
		pShaderResource2= nullptr;
		pTexture2= nullptr;
		Tex2D_t::Release();
	}
};

class CTex2DRing
{
	Tex2D_t Texs[2];
	int index = 0;
	UINT size = 0;

public:
	HRESULT CheckCreate(ID3D11Device* pDevice, const DXGI_FORMAT format, const UINT width, const UINT height, const UINT num) {
		HRESULT hr = S_FALSE;
		if (num == 0) {
			Release();
			return hr;
		}

		index = 0;
		size = 0;
		hr = Texs[0].CheckCreate(pDevice, format, width, height, Tex2D_DefaultShaderRTarget);
		size++;
		if (S_OK == hr && num >= 2) {
			hr = Texs[1].CheckCreate(pDevice, format, width, height, Tex2D_DefaultShaderRTarget);
			size++;
		}
		else {
			Texs[1].Release();
		}
		return hr;
	}

	HRESULT Create(ID3D11Device* pDevice, const DXGI_FORMAT format, const UINT width, const UINT height, const UINT num) {
		Release();
		HRESULT hr = S_FALSE;
		if (num >= 1) {
			hr = Texs[0].Create(pDevice, format, width, height, Tex2D_DefaultShaderRTarget);
			size++;
			if (S_OK == hr && num >= 2) {
				hr = Texs[1].Create(pDevice, format, width, height, Tex2D_DefaultShaderRTarget);
				size++;
			}
		}
		return hr;
	}

	UINT Size() {
		return size;
	}

	void Release() {
		Texs[0].Release();
		Texs[1].Release();
		index = 0;
		size = 0;
	}

	Tex2D_t* GetFirstTex() {
		index = 0;
		return &Texs[0];
	}

	Tex2D_t* GetNextTex() {
		index = (index + 1) & 1;
		return &Texs[index];
	}
};

struct ExternalPixelShader11_t
{
	CStdStringW name;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
};

inline constexpr DirectX::XMFLOAT4 D3DCOLORtoXMFLOAT4(const D3DCOLOR color)
{
	return DirectX::XMFLOAT4{
		(float)((color & 0x00FF0000) >> 16) / 255,
		(float)((color & 0x0000FF00) >> 8) / 255,
		(float)(color & 0x000000FF) / 255,
		(float)((color & 0xFF000000) >> 24) / 255,
	};
}

inline constexpr D3DCOLOR XMFLOAT4toD3DCOLOR(const DirectX::XMFLOAT4& colorRGBAf)
{
	return
		(D3DCOLOR)std::clamp(int(colorRGBAf.x * 255), 0, 255) << 16 |
		(D3DCOLOR)std::clamp(int(colorRGBAf.y * 255), 0, 255) <<  8 |
		(D3DCOLOR)std::clamp(int(colorRGBAf.z * 255), 0, 255)       |
		(D3DCOLOR)std::clamp(int(colorRGBAf.w * 255), 0, 255) << 24;
}

UINT GetAdapter(HWND hWnd, IDXGIFactory1* pDXGIFactory, IDXGIAdapter** ppDXGIAdapter);

HRESULT DumpTexture2D(ID3D11DeviceContext* pDeviceContext, ID3D11Texture2D* pTexture2D, const wchar_t* filename);

DirectX::XMFLOAT4 TransferPQ(DirectX::XMFLOAT4& colorF, const float SDR_peak_lum = 100.0f);
D3DCOLOR TransferPQ(const D3DCOLOR color, const float SDR_peak_lum = 100.0f);

