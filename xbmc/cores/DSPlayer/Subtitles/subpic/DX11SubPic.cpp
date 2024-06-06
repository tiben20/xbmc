/*
 * (C) 2022-2023 see Authors.txt
 * This file is part of MPC-BE.
 *
 * MPC-BE is free software; you can redistribute it and/or modify
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

#include "stdafx.h"
#include "DX11SubPic.h"
#include <DirectXMath.h>
#include "..\libsubs\libsubs.h"
#include "wrl/client.h"
#define ENABLE_DUMP_SUBPIC 0

struct VERTEX {
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT2 TexCoord;
};

void fill_u32(void* dst, uint32_t c, size_t count)
{
#ifndef _WIN64
	__asm {
		mov eax, c
		mov ecx, count
		mov edi, dst
		cld
		rep stosd
	}
#else
	size_t& n = count;
	size_t o = n - (n % 4);

	__m128i val = _mm_set1_epi32((int)c);
	if (((uintptr_t)dst & 0x0F) == 0) { // 16-byte aligned
		for (size_t i = 0; i < o; i += 4) {
			_mm_store_si128((__m128i*) & (((DWORD*)dst)[i]), val);
		}
	}
	else {
		for (size_t i = 0; i < o; i += 4) {
			_mm_storeu_si128((__m128i*) & (((DWORD*)dst)[i]), val);
		}
	}

	switch (n - o) {
	case 3:
		((DWORD*)dst)[o + 2] = c;
	case 2:
		((DWORD*)dst)[o + 1] = c;
	case 1:
		((DWORD*)dst)[o + 0] = c;
	}
#endif
}

#define ALIGN(x, a)           __ALIGN_MASK(x,(decltype(x))(a)-1)
#define __ALIGN_MASK(x, mask) (((x)+(mask))&~(mask))

#if 1
static HRESULT SaveToBMP(BYTE* src, const UINT src_pitch, const UINT width, const UINT height, const UINT bitdepth, const wchar_t* filename)
{
	if (!src || !filename) {
		return E_POINTER;
	}

	if (!src_pitch || !width || !height || bitdepth != 32) {
		return E_ABORT;
	}

	const UINT dst_pitch = width * bitdepth / 8;
	const UINT len = dst_pitch * height;
	ASSERT(dst_pitch <= src_pitch);

	BITMAPFILEHEADER bfh;
	bfh.bfType      = 0x4d42;
	bfh.bfOffBits   = sizeof(bfh) + sizeof(BITMAPINFOHEADER);
	bfh.bfSize      = bfh.bfOffBits + len;
	bfh.bfReserved1 = bfh.bfReserved2 = 0;

	BITMAPINFOHEADER bih = {};
	bih.biSize      = sizeof(BITMAPINFOHEADER);
	bih.biWidth     = width;
	bih.biHeight    = -(LONG)height;
	bih.biBitCount  = bitdepth;
	bih.biPlanes    = 1;
	bih.biSizeImage = DIBSIZE(bih);

	ASSERT(len == bih.biSizeImage);

	FILE* fp;
	if (_wfopen_s(&fp, filename, L"wb") == 0 && fp) {
		fwrite(&bfh, sizeof(bfh), 1, fp);
		fwrite(&bih, sizeof(bih), 1, fp);

		if (dst_pitch == src_pitch) {
			fwrite(src, len, 1, fp);
		}
		else if (dst_pitch < src_pitch) {
			for (UINT y = 0; y < height; ++y) {
				fwrite(src, dst_pitch, 1, fp);
				src += src_pitch;
			}
		}
		fclose(fp);

		return S_OK;
	}

	return E_FAIL;
}

static HRESULT DumpTexture2D(ID3D11DeviceContext* pDeviceContext, ID3D11Texture2D* pTexture2D, const wchar_t* filename)
{
	D3D11_TEXTURE2D_DESC desc;
	pTexture2D->GetDesc(&desc);

	if (desc.Format != DXGI_FORMAT_B8G8R8X8_UNORM && desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
		return E_INVALIDARG;
	}

	HRESULT hr = S_OK;
	Com::SmartPtr<ID3D11Texture2D> pTexture2DShared;

	if (desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ) {
		pTexture2DShared = pTexture2D;
	}
	else {
		ID3D11Device* pDevice;
		pTexture2D->GetDevice(&pDevice);

		D3D11_TEXTURE2D_DESC desc2;
		desc2.Width = desc.Width;
		desc2.Height = desc.Height;
		desc2.MipLevels = 1;
		desc2.ArraySize = 1;
		desc2.Format = desc.Format;
		desc2.SampleDesc = { 1, 0 };
		desc2.Usage = D3D11_USAGE_STAGING;
		desc2.BindFlags = 0;
		desc2.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		desc2.MiscFlags = 0;
		hr = pDevice->CreateTexture2D(&desc2, nullptr, &pTexture2DShared);
		pDevice->Release();

		//pDeviceContext->CopyResource(pTexture2DShared, pTexture2D);
		pDeviceContext->CopySubresourceRegion(pTexture2DShared, 0, 0, 0, 0, pTexture2D, 0, nullptr);
	}

	if (SUCCEEDED(hr)) {
		D3D11_MAPPED_SUBRESOURCE mappedResource = {};
		hr = pDeviceContext->Map(pTexture2DShared, 0, D3D11_MAP_READ, 0, &mappedResource);

		if (SUCCEEDED(hr)) {
			hr = SaveToBMP((BYTE*)mappedResource.pData, mappedResource.RowPitch, desc.Width, desc.Height, 32, filename);

			pDeviceContext->Unmap(pTexture2DShared, 0);
		}
	}

	return hr;
}
#endif

//
// CDX11SubPic
//

CDX11SubPic::CDX11SubPic(MemPic_t&& pMemPic, CDX11SubPicAllocator *pAllocator)
	: m_MemPic(std::move(pMemPic))
	, m_pAllocator(pAllocator)
{
	m_maxsize.SetSize(m_MemPic.w, m_MemPic.h);
	m_rcDirty.SetRect(0, 0, m_maxsize.cx, m_maxsize.cy);
}

CDX11SubPic::~CDX11SubPic()
{
	CAutoLock Lock(&CDX11SubPicAllocator::ms_SurfaceQueueLock);
	// Add surface to cache
	if (m_pAllocator) {
		for (auto it = m_pAllocator->m_AllocatedSurfaces.begin(), end = m_pAllocator->m_AllocatedSurfaces.end(); it != end; ++it) {
			if (*it == this) {
				m_pAllocator->m_AllocatedSurfaces.erase(it);
				break;
			}
		}
		m_pAllocator->m_FreeSurfaces.push_back(std::move(m_MemPic));
	}
}

// ISubPic

STDMETHODIMP_(void*) CDX11SubPic::GetObject()
{
	return reinterpret_cast<void*>(&m_MemPic);
}

STDMETHODIMP CDX11SubPic::GetDesc(SubPicDesc& spd)
{
	spd.type    = 0;
	spd.w       = m_size.cx;
	spd.h       = m_size.cy;
	spd.bpp     = 32;
	spd.pitch   = 0;
	spd.bits    = nullptr;
	spd.vidrect = m_vidrect;

	return S_OK;
}

STDMETHODIMP CDX11SubPic::CopyTo(ISubPic* pSubPic)
{
	HRESULT hr = __super::CopyTo(pSubPic);
	if (FAILED(hr)) {
		return hr;
	}

	if (m_rcDirty.IsRectEmpty()) {
		return S_FALSE;
	}

	auto pDstMemPic = reinterpret_cast<MemPic_t*>(pSubPic->GetObject());

	Com::SmartRect copyRect(m_rcDirty);
	copyRect.InflateRect(1, 1);
	RECT subpicRect = { 0, 0, std::min<UINT>(pDstMemPic->w, m_MemPic.w), std::min<UINT>(pDstMemPic->h, m_MemPic.h) };
	if (!copyRect.IntersectRect(copyRect, &subpicRect)) {
		return S_FALSE;
	}

	const UINT copyW_bytes = copyRect.Width() * 4;
	UINT copyH = copyRect.Height();
	auto src = m_MemPic.data.get() + m_MemPic.w * copyRect.top + copyRect.left;
	auto dst = pDstMemPic->data.get() + pDstMemPic->w * copyRect.top + copyRect.left;

	while (copyH--) {
		memcpy(dst, src, copyW_bytes);
		src += m_MemPic.w;
		dst += pDstMemPic->w;
	}

	return S_OK;
}

STDMETHODIMP CDX11SubPic::ClearDirtyRect(DWORD color)
{
	if (m_rcDirty.IsRectEmpty()) {
		return S_FALSE;
	}

	m_rcDirty.InflateRect(1, 1);
#ifdef _WIN64
	const LONG a = 16 / sizeof(uint32_t) - 1;
	m_rcDirty.left &= ~a;
	m_rcDirty.right = (m_rcDirty.right + a) & ~a;
#endif
	m_rcDirty.IntersectRect(m_rcDirty, Com::SmartRect(0, 0, m_MemPic.w, m_MemPic.h));

	uint32_t* ptr = m_MemPic.data.get() + m_MemPic.w * m_rcDirty.top + m_rcDirty.left;
	const UINT dirtyW = m_rcDirty.Width();
	UINT dirtyH = m_rcDirty.Height();

	while (dirtyH-- > 0) {
		fill_u32(ptr, m_bInvAlpha ? 0x00000000 : 0xFF000000, dirtyW);
		ptr += m_MemPic.w;
	}

	m_rcDirty.SetRectEmpty();

	return S_OK;
}

STDMETHODIMP CDX11SubPic::Lock(SubPicDesc& spd)
{
	if (!m_MemPic.data) {
		return E_FAIL;
	}

	spd.type    = 0;
	spd.w       = m_size.cx;
	spd.h       = m_size.cy;
	spd.bpp     = 32;
	spd.pitch   = m_MemPic.w * 4;
	spd.bits    = (BYTE*)m_MemPic.data.get();
	spd.vidrect = m_vidrect;

	return S_OK;
}

STDMETHODIMP CDX11SubPic::Unlock(RECT* pDirtyRect)
{
	if (pDirtyRect) {
		m_rcDirty.IntersectRect(pDirtyRect, Com::SmartRect(0, 0, m_size.cx, m_size.cy));
	} else {
		m_rcDirty = Com::SmartRect(Com::SmartPoint(0, 0), m_size);
	}

	return S_OK;
}

STDMETHODIMP CDX11SubPic::AlphaBlt(RECT* pSrc, RECT* pDst, SubPicDesc* pTarget)
{
	ASSERT(pTarget == nullptr);

	if (!pSrc || !pDst) {
		return E_POINTER;
	}
	Com::SmartRect rSrc(*pSrc), rDst(*pDst);
	if (m_pAllocator)
		return m_pAllocator->Render(m_MemPic, m_rcDirty, rSrc, rDst);
	return E_FAIL;
}

//
// CDX11SubPicAllocator
//

CDX11SubPicAllocator::CDX11SubPicAllocator(ID3D11Device1* pDevice, SIZE maxsize)
	: ISubPicAllocatorImpl(maxsize, true,true)
	, m_pDevice(pDevice)
	, m_maxsize(maxsize)
{
	CreateBlendState();
	CreateOtherStates();
	m_pOutputTexture = nullptr;
}

CCritSec CDX11SubPicAllocator::ms_SurfaceQueueLock;

CDX11SubPicAllocator::~CDX11SubPicAllocator()
{
	ReleaseAllStates();
	ClearCache();
}

void CDX11SubPicAllocator::GetStats(int& _nFree, int& _nAlloc)
{
	CAutoLock Lock(&ms_SurfaceQueueLock);
	_nFree = (int)m_FreeSurfaces.size();
	_nAlloc = (int)m_AllocatedSurfaces.size();
}

void CDX11SubPicAllocator::ClearCache()
{
	g_log->Log(LOGINFO, "CDX11SubPicAllocator::ClearCache");
	// Clear the allocator of any remaining subpics
	CAutoLock Lock(&ms_SurfaceQueueLock);
	for (auto& pSubPic : m_AllocatedSurfaces) {
		pSubPic->m_pAllocator = nullptr;
	}
	m_AllocatedSurfaces.clear();
	m_FreeSurfaces.clear();

	m_pOutputShaderResource = nullptr;
	m_pOutputTexture = nullptr;
}

// ISubPicAllocator

STDMETHODIMP_(HRESULT __stdcall) CDX11SubPicAllocator::SetDeviceContext(IUnknown* pDev)
{
	m_pDeviceContext = (ID3D11DeviceContext1*)pDev;
	return S_OK;
}

STDMETHODIMP CDX11SubPicAllocator::ChangeDevice(IUnknown* pDev)
{
	g_log->Log(LOGINFO, "CDX11SubPicAllocator::ChangeDevice");
	ClearCache();
	Com::SmartQIPtr<ID3D11Device> pDevice = pDev;
	if (!pDevice) {
		return E_NOINTERFACE;
	}

	CAutoLock cAutoLock(this);
	HRESULT hr = S_FALSE;
	if (m_pDevice != pDevice) {
		ReleaseAllStates();
		ClearCache();

		m_pDevice = pDevice;
		hr = __super::ChangeDevice(pDev);

		CreateBlendState();
		CreateOtherStates();
	}

	return hr;
}

STDMETHODIMP CDX11SubPicAllocator::SetMaxTextureSize(SIZE MaxTextureSize)
{
	CAutoLock cAutoLock(this);
	if (m_maxsize != MaxTextureSize) {
		if (m_maxsize.cx < MaxTextureSize.cx || m_maxsize.cy < MaxTextureSize.cy) {
			ClearCache();
		}
		m_maxsize = MaxTextureSize;
	}

	SetCurSize(MaxTextureSize);
	SetCurVidRect(Com::SmartRect(Com::SmartPoint(0,0), MaxTextureSize));

	return S_OK;
}

STDMETHODIMP_(void) CDX11SubPicAllocator::SetInverseAlpha(bool bInverted)
{
	if (m_bInvAlpha != bInverted) {
		m_bInvAlpha = bInverted;
		m_pAlphaBlendState = nullptr;
		CreateBlendState();
	}
}

HRESULT CDX11SubPicAllocator::CreateOutputTex()
{
	g_log->Log(LOGINFO, "CDX11SubPicAllocator::CreateOutputTex");
	bool bDynamicTex = false;

	Com::SmartQIPtr<IDXGIDevice> pDxgiDevice;
	HRESULT hr = m_pDevice->QueryInterface(IID_PPV_ARGS(&pDxgiDevice));
	if (SUCCEEDED(hr)) {
		Com::SmartPtr<IDXGIAdapter> pDxgiAdapter;
		hr = pDxgiDevice->GetAdapter(&pDxgiAdapter);
		if (SUCCEEDED(hr)) {
			DXGI_ADAPTER_DESC desc;
			hr = pDxgiAdapter->GetDesc(&desc);
			if (SUCCEEDED(hr)) {
				if (desc.VendorId == 0x8086) {
					// workaround for an Intel driver bug where frequent UpdateSubresource calls caused high memory consumption
					bDynamicTex = true;
				}
			}
		}
	}

	D3D11_TEXTURE2D_DESC texDesc = {};
	if (bDynamicTex) {
		texDesc.Usage = D3D11_USAGE_DYNAMIC;
		texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	} else {
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.CPUAccessFlags = 0;
	}
	texDesc.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
	texDesc.MiscFlags      = 0;
	texDesc.Width          = m_maxsize.cx;
	texDesc.Height         = m_maxsize.cy;
	texDesc.MipLevels      = 1;
	texDesc.ArraySize      = 1;
	texDesc.Format         = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.SampleDesc     = { 1, 0 };

	hr = m_pDevice->CreateTexture2D(&texDesc, nullptr, &m_pOutputTexture);
	if (FAILED(hr)) {
		return hr;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	hr = m_pDevice->CreateShaderResourceView(m_pOutputTexture.Get(), &srvDesc, &m_pOutputShaderResource);
	if (FAILED(hr)) {
		m_pOutputTexture = nullptr;
	}

	return hr;
}

void CDX11SubPicAllocator::CreateBlendState()
{
	g_log->Log(LOGINFO, "CDX11SubPicAllocator::CreateBlendState");
	D3D11_BLEND_DESC bdesc = {};
	bdesc.RenderTarget[0].BlendEnable = TRUE;
	bdesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	bdesc.RenderTarget[0].DestBlend = m_bInvAlpha ? D3D11_BLEND_INV_SRC_ALPHA : D3D11_BLEND_SRC_ALPHA;
	bdesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bdesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bdesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	bdesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bdesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	EXECUTE_ASSERT(S_OK == m_pDevice->CreateBlendState(&bdesc, &m_pAlphaBlendState));
}

void CDX11SubPicAllocator::CreateOtherStates()
{
	g_log->Log(LOGINFO, "CDX11SubPicAllocator::CreateOtherStates");
	D3D11_BUFFER_DESC BufferDesc = { sizeof(VERTEX) * 4, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
	EXECUTE_ASSERT(S_OK == m_pDevice->CreateBuffer(&BufferDesc, nullptr, &m_pVertexBuffer));

	D3D11_SAMPLER_DESC SampDesc = {};
	SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SampDesc.MinLOD = 0;
	SampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	EXECUTE_ASSERT(S_OK == m_pDevice->CreateSamplerState(&SampDesc, &m_pSamplerPoint));

	SampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT; // linear interpolation for minification and magnification
	EXECUTE_ASSERT(S_OK == m_pDevice->CreateSamplerState(&SampDesc, &m_pSamplerLinear));
}

void CDX11SubPicAllocator::ReleaseAllStates()
{
	m_pAlphaBlendState = nullptr;
	m_pVertexBuffer = nullptr;
	m_pSamplerPoint = nullptr;
	m_pSamplerLinear = nullptr;

}

HRESULT CDX11SubPicAllocator::Render(const MemPic_t& memPic, const Com::SmartRect& dirtyRect, const Com::SmartRect& srcRect, const Com::SmartRect& dstRect)
{
	HRESULT hr = S_OK;

	if (!m_pOutputTexture) {
		hr = CreateOutputTex();
		if (FAILED(hr)) {
			g_log->Log(LOGERROR, "CDX11SubPicAllocator::Render CreateOutputTex");
			return hr;
		}
	}

	bool stretching = (srcRect.Size() != dstRect.Size());

	Com::SmartRect copyRect(dirtyRect);
	if (stretching) {
		copyRect.InflateRect(1, 1);
		RECT subpicRect = { 0, 0, memPic.w, memPic.h };
		EXECUTE_ASSERT(copyRect.IntersectRect(copyRect, &subpicRect));
	}

	D3D11_TEXTURE2D_DESC texDesc = {};
	m_pOutputTexture->GetDesc(&texDesc);

	uint32_t* src = memPic.data.get() + memPic.w * copyRect.top + copyRect.left;
	if (texDesc.Usage == D3D11_USAGE_DYNAMIC) {
		// workaround for an Intel driver bug where frequent UpdateSubresource calls caused high memory consumption
		D3D11_MAPPED_SUBRESOURCE mr;
		hr = m_pDeviceContext->Map(m_pOutputTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mr);
		if (SUCCEEDED(hr))
		{
			BYTE* dst = (BYTE*)mr.pData + mr.RowPitch * copyRect.top + (copyRect.left * 4);
			const UINT copyW_bytes = copyRect.Width() * 4;
			UINT copyH = copyRect.Height();
			while (copyH-- > 0)
			{
				memcpy(dst, src, copyW_bytes);
				src += memPic.w;
				dst += mr.RowPitch;
			}
			m_pDeviceContext->Unmap(m_pOutputTexture.Get(), 0);
		}
	}
	else {
		D3D11_BOX dstBox = { copyRect.left, copyRect.top, 0, copyRect.right, copyRect.bottom, 1 };
		m_pDeviceContext->UpdateSubresource1(m_pOutputTexture.Get(), 0, &dstBox, src, memPic.w * 4, 0, D3D11_COPY_DISCARD);
	}

	const float src_dx = 1.0f / texDesc.Width;
	const float src_dy = 1.0f / texDesc.Height;
	const float src_l = src_dx * srcRect.left;
	const float src_r = src_dx * srcRect.right;
	const float src_t = src_dy * srcRect.top;
	const float src_b = src_dy * srcRect.bottom;

	const POINT points[4] = {
		{ -1, -1 },
		{ -1, +1 },
		{ +1, -1 },
		{ +1, +1 }
	};

	const VERTEX Vertices[4] = {
		// Vertices for drawing whole texture
		// 2 ___4
		//  |\ |
		// 1|_\|3
		{ {(float)points[0].x, (float)points[0].y, 0}, {src_l, src_b} },
		{ {(float)points[1].x, (float)points[1].y, 0}, {src_l, src_t} },
		{ {(float)points[2].x, (float)points[2].y, 0}, {src_r, src_b} },
		{ {(float)points[3].x, (float)points[3].y, 0}, {src_r, src_t} },
	};

	D3D11_MAPPED_SUBRESOURCE mr;
	if (!m_pVertexBuffer.Get())
	{
		//dont know why losing dynamic buffer something
		D3D11_BUFFER_DESC BufferDesc = { sizeof(VERTEX) * 4, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
		m_pDevice->CreateBuffer(&BufferDesc, nullptr, &m_pVertexBuffer);
	}
	

	hr = m_pDeviceContext->Map(m_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mr);
	if (FAILED(hr)) {
		return hr;
	}

	memcpy(mr.pData, &Vertices, sizeof(Vertices));
	m_pDeviceContext->Unmap(m_pVertexBuffer.Get(), 0);

	UINT Stride = sizeof(VERTEX);
	UINT Offset = 0;
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &Stride, &Offset);
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_pDeviceContext->PSSetSamplers(0, 1, &(stretching ? m_pSamplerLinear : m_pSamplerPoint));
	m_pDeviceContext->PSSetShaderResources(0, 1, &m_pOutputShaderResource);

	m_pDeviceContext->OMSetBlendState(m_pAlphaBlendState.Get(), nullptr, D3D11_DEFAULT_SAMPLE_MASK);

	D3D11_VIEWPORT vp;
	vp.TopLeftX = dstRect.left;
	vp.TopLeftY = dstRect.top;
	vp.Width    = dstRect.Width();
	vp.Height   = dstRect.Height();
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	m_pDeviceContext->RSSetViewports(1, &vp);

	m_pDeviceContext->Draw(4, 0);
	//g_log->Log(LOGINFO, "Rendered subtitles");
#if 0
	{
		static int counter = 0;
		CStdStringW filepath;
		filepath.Format(L"C:\\temp\\subpictex%04d.bmp", counter++);
		DumpTexture2D(pDeviceContext, m_pOutputTexture, filepath);
	}
#endif

	return hr;
}

// ISubPicAllocatorImpl

void CDX11SubPicAllocator::FreeTextures()
{
	// Clear the allocator of any remaining subpics
	/*CAutoLock Lock(&ms_SurfaceQueueLock);
	for (std::list<CDX9SubPic*>::iterator pos = m_AllocatedSurfaces.begin(); pos != m_AllocatedSurfaces.end(); )
	{
		CDX9SubPic* pSubPic = *pos; pos++;
		pSubPic->m_pAllocator = NULL;
		delete pSubPic;
	}
	m_AllocatedSurfaces.clear();

	for (std::list<Com::SmartPtrForList<IDirect3DSurface9>>::iterator it = m_FreeSurfaces.begin();
		it != m_FreeSurfaces.end(); it++)
		it->FullRelease();

	m_FreeSurfaces.clear();*/
}

bool CDX11SubPicAllocator::Alloc(bool fStatic, ISubPic** ppSubPic)
{
	if (!ppSubPic) {
		return false;
	}

	CAutoLock cAutoLock(this);

	*ppSubPic = nullptr;

	MemPic_t pMemPic;

	if (!fStatic) {
		CAutoLock cAutoLock(&ms_SurfaceQueueLock);
		if (!m_FreeSurfaces.empty()) {
			pMemPic = std::move(m_FreeSurfaces.front());
			m_FreeSurfaces.pop_front();
		}
	}

	if (!pMemPic.data) {
		pMemPic.w = ALIGN(m_maxsize.cx, 16/sizeof(uint32_t));
		pMemPic.h = m_maxsize.cy;

		const UINT picSize = pMemPic.w * pMemPic.h;
		auto data = new(std::nothrow) uint32_t[picSize];
		if (!data) {
			return false;
		}

		pMemPic.data.reset(data);
	}

	*ppSubPic = DNew CDX11SubPic(std::move(pMemPic), fStatic ? 0 : this);
	if (!(*ppSubPic)) {
		return false;
	}

	(*ppSubPic)->AddRef();
	(*ppSubPic)->SetInverseAlpha(m_bInvAlpha);

	if (!fStatic) {
		CAutoLock cAutoLock(&ms_SurfaceQueueLock);
		m_AllocatedSurfaces.push_front((CDX11SubPic*)*ppSubPic);
	}

	return true;
}
