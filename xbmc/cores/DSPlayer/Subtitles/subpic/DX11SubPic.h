/*
 * (C) 2022-2024 see Authors.txt
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

#pragma once

#include "isubpic.h"
#include <d3d11.h>
#include <deque>
// CDX11SubPic


class CDX11SubPicAllocator;

struct MemPic_t {
	std::unique_ptr<uint32_t[]> data;
	UINT w = 0;
	UINT h = 0;
};

class CDX11SubPic : public ISubPicImpl
{
	MemPic_t m_MemPic;

protected:
	STDMETHODIMP_(void*) GetObject(); // returns MemPic_t*

public:
	CDX11SubPicAllocator *m_pAllocator;

	CDX11SubPic(MemPic_t&& pMemPic, CDX11SubPicAllocator *pAllocator);
	~CDX11SubPic();

	// ISubPic
	STDMETHODIMP GetDesc(SubPicDesc& spd) override;
	STDMETHODIMP CopyTo(ISubPic* pSubPic) override;
	STDMETHODIMP ClearDirtyRect(DWORD color) override;
	STDMETHODIMP Lock(SubPicDesc& spd) override;
	STDMETHODIMP Unlock(RECT* pDirtyRect) override;
	STDMETHODIMP AlphaBlt(RECT* pSrc, RECT* pDst, SubPicDesc* pTarget) override;
	STDMETHODIMP GetTexture(Com::SmartPtr<IDirect3DTexture9>& pTexture) { return S_OK; };
};

// CDX11SubPicAllocator

class CDX11SubPicAllocator : public ISubPicAllocatorImpl, public CCritSec
{
	ID3D11Device* m_pDevice;
	Com::SmartSize m_maxsize;

	Com::SmartPtr<ID3D11Texture2D> m_pOutputTexture;
	Com::SmartPtr<ID3D11ShaderResourceView> m_pOutputShaderResource;
	Com::SmartPtr<ID3D11BlendState> m_pAlphaBlendState;
	Com::SmartPtr<ID3D11Buffer> m_pVertexBuffer;
	Com::SmartPtr<ID3D11SamplerState> m_pSamplerPoint;
	Com::SmartPtr<ID3D11SamplerState> m_pSamplerLinear;

	bool Alloc(bool fStatic, ISubPic** ppSubPic) override;

	HRESULT CreateOutputTex();
	void CreateBlendState();
	void CreateOtherStates();
	void ReleaseAllStates();

public:
	static CCritSec ms_SurfaceQueueLock;
	std::deque<MemPic_t> m_FreeSurfaces;
	std::deque<CDX11SubPic*> m_AllocatedSurfaces;

	HRESULT Render(const MemPic_t& memPic, const Com::SmartRect& dirtyRect, const Com::SmartRect& srcRect, const Com::SmartRect& dstRect);

	void GetStats(int& _nFree, int& _nAlloc);

	CDX11SubPicAllocator(ID3D11Device* pDevice, SIZE maxsize);
	~CDX11SubPicAllocator();
	void ClearCache();

	// ISubPicAllocator
	STDMETHODIMP ChangeDevice(IUnknown* pDev) override;
	STDMETHODIMP SetMaxTextureSize(SIZE MaxTextureSize) override;
	STDMETHODIMP_(void) SetInverseAlpha(bool bInverted) override;
};
