/*
 * (C) 2018-2023 see Authors.txt
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

#include "stdafx.h"
#include <Mferror.h>
#include "VideoRenderer.h"
#include "VideoRendererInputPin.h"
#include "CustomAllocator.h"
#include "windowing/windows/WinSystemWin32DX.h"

//
// CVideoRendererInputPin
//

CVideoRendererInputPin::CVideoRendererInputPin(CDSBaseRenderer *pRenderer, HRESULT *phr, LPCWSTR Name, CMpcVideoRenderer* pBaseRenderer)
	: CRendererInputPin(pRenderer, phr, Name)
	, m_pBaseRenderer(pBaseRenderer)
{
}

CVideoRendererInputPin::~CVideoRendererInputPin()
{
	SAFE_DELETE(m_pNewMT);
}

STDMETHODIMP CVideoRendererInputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
	CheckPointer(ppv, E_POINTER);

  if (riid == __uuidof(ID3D11DecoderConfiguration1)) {
		return GetInterface((ID3D11DecoderConfiguration1*)this, ppv);
	}
	else {
		return __super::NonDelegatingQueryInterface(riid, ppv);
	}
}

STDMETHODIMP CVideoRendererInputPin::GetAllocator(IMemAllocator **ppAllocator)
{
	DLog("CVideoRendererInputPin::GetAllocator()");

	if (m_bDXVA || m_bD3D11) {
		// Renderer shouldn't manage allocator for DXVA/D3D11
		return E_NOTIMPL;
	}

	CheckPointer(ppAllocator, E_POINTER);

	if (m_pAllocator) {
		// We already have an allocator, so return that one.
		*ppAllocator = m_pAllocator;
		(*ppAllocator)->AddRef();
		return S_OK;
	}

	// No allocator yet, so propose our custom allocator.
	HRESULT hr = S_OK;
	m_pCustomAllocator = new(std::nothrow) CCustomAllocator(L"Custom allocator", nullptr, this, &hr);
	if (!m_pCustomAllocator) {
		return E_OUTOFMEMORY;
	}
	if (FAILED(hr)) {
		SAFE_DELETE(m_pCustomAllocator);
		return hr;
	}

	if (m_pNewMT) {
		m_pCustomAllocator->SetNewMediaType(*m_pNewMT);
		SAFE_DELETE(m_pNewMT);
	}

	// Return the IMemAllocator interface to the caller.
	return m_pCustomAllocator->QueryInterface(IID_IMemAllocator, (void**)ppAllocator);
}

STDMETHODIMP CVideoRendererInputPin::GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps)
{
	// 1 buffer required
	ZeroMemory(pProps, sizeof(ALLOCATOR_PROPERTIES));
	pProps->cbBuffer = 1;
	return S_OK;
}

STDMETHODIMP CVideoRendererInputPin::ReceiveConnection(IPin* pConnector, const AM_MEDIA_TYPE* pmt)
{
	DLog("CVideoRendererInputPin::ReceiveConnection()");

	CAutoLock cObjectLock(m_pLock);

	if (m_Connected) {
		CMediaType mt(*pmt);

		if (FAILED(CheckMediaType(&mt))) {
			return VFW_E_TYPE_NOT_ACCEPTED;
		}

		ALLOCATOR_PROPERTIES props, actual;

		Microsoft::WRL::ComPtr<IMemAllocator> pMemAllocator;
		if (FAILED(GetAllocator(&pMemAllocator))
				|| FAILED(pMemAllocator->Decommit())
				|| FAILED(pMemAllocator->GetProperties(&props))) {
			return FrameInVideoMem()
				? S_OK // hack for Microsoft DTV-DVD Video Decoder
				: E_FAIL;
		}

		CMediaType mtNew(*pmt);
		props.cbBuffer = m_pBaseRenderer->CalcImageSize(mtNew, !FrameInVideoMem());

		if (FAILED(pMemAllocator->SetProperties(&props, &actual))
				|| FAILED(pMemAllocator->Commit())
				|| props.cbBuffer != actual.cbBuffer) {
			return E_FAIL;
		}

		return SetMediaType(&mt) == S_OK // here set mt, not mtNew
			? S_OK
			: VFW_E_TYPE_NOT_ACCEPTED;
	} else {
		m_bDXVA = m_bD3D11 = false;
	}

	return __super::ReceiveConnection(pConnector, pmt);
}

STDMETHODIMP CVideoRendererInputPin::NewSegment(REFERENCE_TIME startTime, REFERENCE_TIME stopTime, double rate)
{
	CAutoLock cReceiveLock(&m_csReceive);

	m_pBaseRenderer->NewSegment(startTime);
	return CRendererInputPin::NewSegment(startTime, stopTime, rate);
}

STDMETHODIMP CVideoRendererInputPin::BeginFlush()
{
	CAutoLock cReceiveLock(&m_csReceive);
	return CRendererInputPin::BeginFlush();
}

// IMFGetService
STDMETHODIMP CVideoRendererInputPin::GetService(REFGUID guidService, REFIID riid, LPVOID *ppvObject)
{
	if (riid == __uuidof(IDirectXVideoMemoryConfiguration)) {
		if (m_mt.subtype == MEDIASUBTYPE_NV12 || m_mt.subtype == MEDIASUBTYPE_P010) {
			return GetInterface((IDirectXVideoMemoryConfiguration*)this, ppvObject);
		} else {
			return MF_E_UNSUPPORTED_SERVICE;
		}
	}

	return m_pBaseRenderer->GetService(guidService, riid, ppvObject);
}

// IDirectXVideoMemoryConfiguration
STDMETHODIMP CVideoRendererInputPin::GetAvailableSurfaceTypeByIndex(DWORD dwTypeIndex, DXVA2_SurfaceType *pdwType)
{
	if (dwTypeIndex == 0) {
		*pdwType = DXVA2_SurfaceType_DecoderRenderTarget;
		return S_OK;
	} else {
		return MF_E_NO_MORE_TYPES;
	}
}

STDMETHODIMP CVideoRendererInputPin::SetSurfaceType(DXVA2_SurfaceType dwType)
{
	m_bDXVA = (dwType == DXVA2_SurfaceType_DecoderRenderTarget);
	DLogIf(m_bDXVA, "CVideoRendererInputPin::SetSurfaceType() : set surface type for DXVA2 decoder");
	return S_OK;
}

// ID3D11DecoderConfiguration1
STDMETHODIMP CVideoRendererInputPin::ActivateD3D11Decoding1(ID3D11Device **pDevice, ID3D11DeviceContext **pContext, UINT nFlags)
{
	HRESULT hr = E_FAIL;
	
	if (auto pDX11VP = dynamic_cast<CDX11VideoProcessor*>(m_pBaseRenderer->m_VideoProcessor.get())) {
		hr = S_OK;

	  *pDevice = DX::DeviceResources::Get()->GetD3DDevice();
		(*pDevice)->AddRef();
		*pContext = DX::DeviceResources::Get()->GetImmediateContext();
		(*pContext)->AddRef();
		//hr = DX::DeviceResources::Get()->GetD3DDevice()->QueryInterface(__uuidof(ID3D11Device),(void**) &pDevice);
		//hr = pDX11VP->GetD3DContext()->QueryInterface(__uuidof(ID3D11DeviceContext), (void**)&pContext);


		hr = pDX11VP->SetDevice(DX::DeviceResources::Get()->GetD3DDevice(), true);
	}

	
	m_bD3D11 = (hr == S_OK);
	DLog("CVideoRendererInputPin::ActivateD3D11Decoding() : {}", m_bD3D11 ? "completed successfully" : "failed");
	return hr;
}

UINT STDMETHODCALLTYPE CVideoRendererInputPin::GetD3D11AdapterIndex1()
{
	return m_pBaseRenderer->m_VideoProcessor->GetCurrentAdapter();
}

void CVideoRendererInputPin::SetNewMediaType(const CMediaType& mt)
{
	DLog("CVideoRendererInputPin::SetNewMediaType()");

	SAFE_DELETE(m_pNewMT);
	if (m_pCustomAllocator) {
		m_pCustomAllocator->SetNewMediaType(mt);
	} else {
		m_pNewMT = new CMediaType(mt);
	}
}

void CVideoRendererInputPin::ClearNewMediaType()
{
	SAFE_DELETE(m_pNewMT);
	if (m_pCustomAllocator) {
		m_pCustomAllocator->ClearNewMediaType();
	}
}
