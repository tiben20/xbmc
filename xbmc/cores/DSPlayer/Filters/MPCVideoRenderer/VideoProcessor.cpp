/*
 * (C) 2020-2024 see Authors.txt
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
#include "Helper.h"
#include "VideoRenderer.h"
#include "VideoProcessor.h"
#include "DSUtil/DSUtil.h"

HRESULT CVideoProcessor::GetVideoSize(long *pWidth, long *pHeight)
{
	CheckPointer(pWidth, E_POINTER);
	CheckPointer(pHeight, E_POINTER);

	if (m_iRotation == 90 || m_iRotation == 270) {
		*pWidth  = m_srcRectHeight;
		*pHeight = m_srcRectWidth;
	} else {
		*pWidth  = m_srcRectWidth;
		*pHeight = m_srcRectHeight;
	}

	return S_OK;
}

HRESULT CVideoProcessor::GetAspectRatio(long *plAspectX, long *plAspectY)
{
	CheckPointer(plAspectX, E_POINTER);
	CheckPointer(plAspectY, E_POINTER);

	if (m_iRotation == 90 || m_iRotation == 270) {
		*plAspectX = m_srcAspectRatioY;
		*plAspectY = m_srcAspectRatioX;
	} else {
		*plAspectX = m_srcAspectRatioX;
		*plAspectY = m_srcAspectRatioY;
	}

	return S_OK;
}

void CVideoProcessor::CalcStatsFont()
{
	if (m_iResizeStats == 1) {
		int w = std::max(512, m_windowRect.Width() / 2 - 10) - 5 - 3;
		int h = std::max(280, m_windowRect.Height() - 10) - 5 - 3;
		m_StatsFontH = (int)std::ceil(std::min(w / 36.0, h / 19.4));
		m_StatsFontH &= ~1;
		if (m_StatsFontH < 14) {
			m_StatsFontH = 14;
		}
	}
	else {
		m_StatsFontH = 14;
	}
}

bool CVideoProcessor::CheckGraphPlacement()
{
	return m_GraphRect.left >= 0 && m_GraphRect.top >= 0
		&& !(m_GraphRect.left < m_StatsRect.right && m_GraphRect.top < m_StatsRect.bottom);
}

void CVideoProcessor::CalcGraphParams()
{
	auto CalcGraphRect = [&]() {
		m_GraphRect.right = m_windowRect.right - 20;
		m_GraphRect.left  = m_GraphRect.right - m_Xstep * m_Syncs.Size();

		m_GraphRect.bottom = m_windowRect.bottom - 20;
		m_GraphRect.top    = m_GraphRect.bottom - 120 * m_Yscale;
	};

	m_Xstep = 4;
	m_Yscale = 2;
	CalcGraphRect();

	if (!CheckGraphPlacement()) {
		m_Xstep = 2;
		m_Yscale = 1;
		CalcGraphRect();
	}

	m_Yaxis = m_GraphRect.bottom - 50 * m_Yscale;
}

void CVideoProcessor::SetDisplayInfo(const DisplayConfig_t& dc, const bool primary, const bool fullscreen)
{
	if (dc.refreshRate.Numerator) {
		m_uHalfRefreshPeriodMs = (UINT32)(500ull * dc.refreshRate.Denominator / dc.refreshRate.Numerator);
	} else {
		m_uHalfRefreshPeriodMs = 0;
	}

	m_strStatsDispInfo.assign(L"\nDisplay: ");

	CStdStringW str = DisplayConfigToString(dc);
	if (str.size()) {
		m_strStatsDispInfo.append(str);
		str.clear();

		if (dc.bitsPerChannel) { // if bitsPerChannel is not set then colorEncoding and other values are invalid
			const wchar_t* colenc = ColorEncodingToString(dc.colorEncoding);
			if (colenc) {
				str.Format(L"\nColor: %s %u-bit", colenc, dc.bitsPerChannel);
				if (dc.advancedColor.advancedColorSupported) {
					str.append(L" HDR10: ");
					str.append(dc.advancedColor.advancedColorEnabled ? L"on" : L"off");
				}
			}
		}
	}

	if (primary) {
		m_strStatsDispInfo.append(L" Primary");
	}
	m_strStatsDispInfo.append(fullscreen ? L" fullscreen" : L" windowed");

	if (str.size()) {
		m_strStatsDispInfo.append(str);
	}
}

void CVideoProcessor::SyncFrameToStreamTime(const REFERENCE_TIME frameStartTime)
{
	if (m_pFilter->m_filterState == State_Running && frameStartTime != INVALID_TIME) {
		if (SUCCEEDED(m_pFilter->StreamTime(m_streamTime)) && frameStartTime > m_streamTime) {
			const auto sleepTime = (frameStartTime - m_streamTime) / 10000LL - m_uHalfRefreshPeriodMs;
			if (sleepTime > 0 && sleepTime < 42) {
				// We are waiting for Preset to display the frame at the required display refresh interval.
				// This is relevant for displays with high frame rates (for example 144 Hz).
				// But no longer than 41 ms to avoid problems with the DVD-Video menu.
				Sleep(static_cast<DWORD>(sleepTime));
			}
		}
	}
}

bool CVideoProcessor::CheckDoviMetadata(const MediaSideDataDOVIMetadata* pDOVIMetadata, const uint8_t maxReshapeMethon)
{
	if (!pDOVIMetadata->Header.disable_residual_flag) {
		return false;
	}

	for (const auto& curve : pDOVIMetadata->Mapping.curves) {
		if (curve.num_pivots < 2 || curve.num_pivots > 9) {
			return false;
		}
		for (int i = 0; i < int(curve.num_pivots - 1); i++) {
			if (curve.mapping_idc[i] > maxReshapeMethon) { // 0 polynomial, 1 mmr
				return false;
			}
		}
	}

	return true;
}

// IUnknown

STDMETHODIMP CVideoProcessor::QueryInterface(REFIID riid, void **ppv)
{
	if (!ppv) {
		return E_POINTER;
	}
	if (riid == IID_IUnknown) {
		*ppv = static_cast<IUnknown*>(static_cast<IMFVideoProcessor*>(this));
	}
	else if (riid == IID_IMFVideoProcessor) {
		*ppv = static_cast<IMFVideoProcessor*>(this);
	}
	else if (riid == IID_IMFVideoMixerBitmap) {
		*ppv = static_cast<IMFVideoMixerBitmap*>(this);
	}
	else {
		*ppv = nullptr;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

STDMETHODIMP_(ULONG) CVideoProcessor::AddRef()
{
	return InterlockedIncrement(&m_nRefCount);
}

STDMETHODIMP_(ULONG) CVideoProcessor::Release()
{
	ULONG uCount = InterlockedDecrement(&m_nRefCount);
	if (uCount == 0) {
		delete this;
	}
	// For thread safety, return a temporary variable.
	return uCount;
}

// IMFVideoProcessor

STDMETHODIMP CVideoProcessor::GetProcAmpRange(DWORD dwProperty, DXVA2_ValueRange *pPropRange)
{
	CheckPointer(pPropRange, E_POINTER);
	if (m_srcParams.cformat == CF_NONE) {
		return MF_E_TRANSFORM_TYPE_NOT_SET;
	}

	switch (dwProperty) {
	case DXVA2_ProcAmp_Brightness: *pPropRange = m_DXVA2ProcAmpRanges[0]; break;
	case DXVA2_ProcAmp_Contrast:   *pPropRange = m_DXVA2ProcAmpRanges[1]; break;
	case DXVA2_ProcAmp_Hue:        *pPropRange = m_DXVA2ProcAmpRanges[2]; break;
	case DXVA2_ProcAmp_Saturation: *pPropRange = m_DXVA2ProcAmpRanges[3]; break;
	default:
		return E_INVALIDARG;
	}

	return S_OK;
}

STDMETHODIMP CVideoProcessor::GetProcAmpValues(DWORD dwFlags, DXVA2_ProcAmpValues *Values)
{
	CheckPointer(Values, E_POINTER);
	if (m_srcParams.cformat == CF_NONE) {
		return MF_E_TRANSFORM_TYPE_NOT_SET;
	}

	if (dwFlags & DXVA2_ProcAmp_Brightness) { Values->Brightness = m_DXVA2ProcAmpValues.Brightness; }
	if (dwFlags & DXVA2_ProcAmp_Contrast)   { Values->Contrast   = m_DXVA2ProcAmpValues.Contrast  ; }
	if (dwFlags & DXVA2_ProcAmp_Hue)        { Values->Hue        = m_DXVA2ProcAmpValues.Hue       ; }
	if (dwFlags & DXVA2_ProcAmp_Saturation) { Values->Saturation = m_DXVA2ProcAmpValues.Saturation; }

	return S_OK;
}

STDMETHODIMP CVideoProcessor::GetBackgroundColor(COLORREF *lpClrBkg)
{
	CheckPointer(lpClrBkg, E_POINTER);
	*lpClrBkg = RGB(0, 0, 0);
	return S_OK;
}

// IMFVideoMixerBitmap

STDMETHODIMP CVideoProcessor::ClearAlphaBitmap()
{
	CAutoLock cRendererLock(&m_pFilter->m_RendererLock);
	m_bAlphaBitmapEnable = false;

	return S_OK;
}

STDMETHODIMP CVideoProcessor::GetAlphaBitmapParameters(MFVideoAlphaBitmapParams *pBmpParms)
{
	CheckPointer(pBmpParms, E_POINTER);
	CAutoLock cRendererLock(&m_pFilter->m_RendererLock);

	if (m_bAlphaBitmapEnable) {
		pBmpParms->dwFlags      = MFVideoAlphaBitmap_SrcRect|MFVideoAlphaBitmap_DestRect;
		pBmpParms->clrSrcKey    = 0; // non used
		pBmpParms->rcSrc        = m_AlphaBitmapRectSrc;
		pBmpParms->nrcDest      = m_AlphaBitmapNRectDest;
		pBmpParms->fAlpha       = 0; // non used
		pBmpParms->dwFilterMode = D3DTEXF_LINEAR;
		return S_OK;
	} else {
		return MF_E_NOT_INITIALIZED;
	}
}
