/*
 * (C) 2018-2024 see Authors.txt
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
#include <atomic>
#include <evr.h> // for MR_VIDEO_ACCELERATION_SERVICE, because the <mfapi.h> does not contain it
#include <Mferror.h>
#include "Helper.h"
#include "VideoRendererInputPin.h"
#include "Include/Version.h"
#include "VideoRenderer.h"
#include "Registry.h"
#include "windowing/windows/WinSystemWin32DX.h"
#include "ServiceBroker.h"
#include "utils/SystemInfo.h"
#include "DSUtil/DSUtil.h"

#define WM_SWITCH_FULLSCREEN (WM_APP + 0x1000)

#define OPT_REGKEY_VIDEORENDERER           L"Software\\MPC-BE Filters\\MPC Video Renderer"
#define OPT_UseD3D11                       L"UseD3D11"
#define OPT_ShowStatistics                 L"ShowStatistics"
#define OPT_ResizeStatistics               L"ResizeStatistics"
#define OPT_TextureFormat                  L"TextureFormat"
#define OPT_VPEnableNV12                   L"VPEnableNV12"
#define OPT_VPEnableP01x                   L"VPEnableP01x"
#define OPT_VPEnableYUY2                   L"VPEnableYUY2"
#define OPT_VPEnableOther                  L"VPEnableOther"
#define OPT_DoubleFrateDeint               L"DoubleFramerateDeinterlace"
#define OPT_VPScaling                      L"VPScaling"
#define OPT_VPSuperResolution              L"VPSuperResolution"
#define OPT_VPRTXVideoHDR                  L"VPRTXVideoHDR"
#define OPT_ChromaUpsampling               L"ChromaUpsampling"
#define OPT_Upscaling                      L"Upscaling"
#define OPT_Downscaling                    L"Downscaling"
#define OPT_InterpolateAt50pct             L"InterpolateAt50pct"
#define OPT_Dither                         L"Dither"
#define OPT_DeintBlend                     L"DeinterlaceBlend"
#define OPT_SwapEffect                     L"SwapEffect"
#define OPT_ExclusiveFullscreen            L"ExclusiveFullscreen"
#define OPT_VBlankBeforePresent            L"VBlankBeforePresent"
#define OPT_ReinitByDisplay                L"ReinitWhenChangingDisplay"
#define OPT_HdrPreferDoVi                  L"HdrPreferDoVi"
#define OPT_HdrPassthrough                 L"HdrPassthrough"
#define OPT_HdrToggleDisplay               L"HdrToggleDisplay"
#define OPT_HdrOsdBrightness               L"HdrOsdBrightness"
#define OPT_ConvertToSdr                   L"ConvertToSdr"
#define OPT_UseD3DFullscreen               L"UseD3DFullscreen"

static std::atomic_int g_nInstance = 0;
static const wchar_t g_szClassName[] = L"VRWindow";

LPCWSTR g_pszOldParentWndProc = L"OldParentWndProc";
LPCWSTR g_pszThis = L"This";

static void RemoveParentWndProc(HWND hWnd)
{
	DLog("RemoveParentWndProc()");
	auto pfnOldProc = (WNDPROC)GetPropW(hWnd, g_pszOldParentWndProc);
	if (pfnOldProc) {
		SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)pfnOldProc);
		RemovePropW(hWnd, g_pszOldParentWndProc);
		RemovePropW(hWnd, g_pszThis);
	}
}

static LRESULT CALLBACK ParentWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	auto pfnOldProc = (WNDPROC)GetPropW(hWnd, g_pszOldParentWndProc);
	auto pThis = static_cast<CMpcVideoRenderer*>(GetPropW(hWnd, g_pszThis));

	switch (Msg) {
		case WM_DESTROY:
			SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)pfnOldProc);
			RemovePropW(hWnd, g_pszOldParentWndProc);
			RemovePropW(hWnd, g_pszThis);
			break;
		case WM_DISPLAYCHANGE:
			DLog("ParentWndProc() - WM_DISPLAYCHANGE");
			pThis->OnDisplayModeChange(true);
			break;
		case WM_MOVE:
			CLog::Log(LOGINFO, "{} WM_MOVE", __FUNCTION__);
			if (pThis->m_bIsFullscreen) {
				// I don't know why, but without this, the filter freezes when switching from fullscreen to window in DX9 mode.
				SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)pfnOldProc);
				SetWindowLongPtrW(hWnd, GWLP_WNDPROC, (LONG_PTR)ParentWndProc);
			} else {
				pThis->OnWindowMove();
			}
			break;
		case WM_NCACTIVATE:
			if (!wParam && pThis->m_bIsFullscreen && !pThis->m_bIsD3DFullscreen) {
				return 0;
			}
			break;
		case WM_RBUTTONUP:
			if (pThis->m_bIsFullscreen) {
				// block context menu in exclusive fullscreen
				return 0;
			}
			break;
/*
		case WM_SYSCOMMAND:
			if (pThis->m_bIsFullscreen && wParam == SC_MINIMIZE) {
				// block minimize in exclusive fullscreen
				return 0;
			}
			break;
*/
	}

	return CallWindowProcW(pfnOldProc, hWnd, Msg, wParam, lParam);
}

//
// CMpcVideoRenderer
//

CMpcVideoRenderer::CMpcVideoRenderer(LPUNKNOWN pUnk, HRESULT* phr)
	: CBaseVideoRenderer2(__uuidof(this), L"MPC Video Renderer", pUnk, phr)
{
	DLog("CMpcVideoRenderer::CMpcVideoRenderer()");

	auto nPrevInstance = g_nInstance++; // always increment g_nInstance in the constructor
	if (nPrevInstance > 0) {
		*phr = E_ABORT;
		DLog("Previous copy of CMpcVideoRenderer found! Initialization aborted.");
		return;
	}

	DLog("Windows {}", WToA(GetWindowsVersion()));
	//DLog(GetNameAndVersion().c_str());

	ASSERT(S_OK == *phr);
	m_pInputPin = new CVideoRendererInputPin(this, phr, L"In", this);
	ASSERT(S_OK == *phr);
	
	HRESULT hr = S_FALSE;

	
		m_VideoProcessor.reset(new CDX11VideoProcessor(this, hr));
		if (SUCCEEDED(hr)) {
			hr = m_VideoProcessor->Init(g_hWnd);
		}

		if (FAILED(hr)) {
			m_VideoProcessor.reset();
		}
		DLogIf(S_OK == hr, "Direct3D11 initialization successfully!");
	

	*phr = hr;

	return;
}

CMpcVideoRenderer::~CMpcVideoRenderer()
{
	DLog("CMpcVideoRenderer::~CMpcVideoRenderer()");

	if (m_hWndWindow) {
		::SendMessageW(m_hWndWindow, WM_CLOSE, 0, 0);
	}

	UnregisterClassW(g_szClassName, g_hInst);

	if (m_hWndParentMain) {
		RemoveParentWndProc(m_hWndParentMain);
	}

	if (m_bIsFullscreen && !m_bIsD3DFullscreen && m_hWndParentMain) {
		PostMessageW(m_hWndParentMain, WM_SWITCH_FULLSCREEN, 0, 0);
	}

	m_VideoProcessor.reset();

	g_nInstance--; // always decrement g_nInstance in the destructor
}

void CMpcVideoRenderer::NewSegment(REFERENCE_TIME startTime)
{
	CLog::Log(LOGINFO, "{} {}", __FUNCTION__, CRefTime(startTime).Millisecs());

	m_rtStartTime = startTime;
}

HRESULT CMpcVideoRenderer::BeginFlush()
{
	DLog("CMpcVideoRenderer::BeginFlush()");

	m_VideoProcessor->FlushSampledQueue();
	m_VideoProcessor->Flush();
	m_bFlushing = true;
	
	return __super::BeginFlush();
}

HRESULT CMpcVideoRenderer::EndFlush()
{
	DLog("CMpcVideoRenderer::EndFlush()");

	

	HRESULT hr = __super::EndFlush();

	m_bFlushing = false;

	return hr;
}

long CMpcVideoRenderer::CalcImageSize(CMediaType& mt, bool redefine_mt)
{
	BITMAPINFOHEADER* pBIH = GetBIHfromVIHs(&mt);
	if (!pBIH) {
		ASSERT(FALSE); // excessive checking
		return 0;
	}

	if (redefine_mt) {
		Com::SmartSize Size(pBIH->biWidth, pBIH->biHeight);

		BOOL ret = m_VideoProcessor->GetAlignmentSize(mt, Size);

		if (ret && (Size.cx != pBIH->biWidth || Size.cy != pBIH->biHeight)) {
			BYTE* pbFormat = mt.ReallocFormatBuffer(112 + sizeof(VR_Extradata));
			if (pbFormat) {
				// update pointer after realoc
				pBIH = GetBIHfromVIHs(&mt);
				// copy data to VR_Extradata
				VR_Extradata* vrextra = reinterpret_cast<VR_Extradata*>(pbFormat + 112);
				vrextra->QueryWidth  = Size.cx;
				vrextra->QueryHeight = Size.cy;
				vrextra->FrameWidth  = pBIH->biWidth;
				vrextra->FrameHeight = pBIH->biHeight;
				vrextra->Compression = pBIH->biCompression;
			}

			// new media type must have non-empty rcSource
			RECT& rcSource = ((VIDEOINFOHEADER*)mt.pbFormat)->rcSource;
			if (IsRectEmpty(&rcSource)) {
				rcSource = { 0, 0, pBIH->biWidth, abs(pBIH->biHeight) };
			}
			RECT& rcTarget = ((VIDEOINFOHEADER*)mt.pbFormat)->rcTarget;
			if (IsRectEmpty(&rcTarget)) {
				// CoreAVC Video Decoder does not work correctly with empty rcTarget
				rcTarget = rcSource;
			}

			DLog("CMpcVideoRenderer::CalcImageSize() buffer size changed from {}x{} to {}x{}", pBIH->biWidth, pBIH->biHeight, Size.cx, Size.cy);
			// overwrite buffer size
			pBIH->biWidth  = Size.cx;
			pBIH->biHeight = Size.cy;
			pBIH->biSizeImage = DIBSIZE(*pBIH);
		}
	}

	return pBIH->biSizeImage ? pBIH->biSizeImage : DIBSIZE(*pBIH);
}

// CBaseRenderer

HRESULT CMpcVideoRenderer::CheckMediaType(const CMediaType* pmt)
{
	CheckPointer(pmt, E_POINTER);
	CheckPointer(pmt->pbFormat, E_POINTER);

	if (pmt->majortype == MEDIATYPE_Video && (pmt->formattype == FORMAT_VideoInfo2 || pmt->formattype == FORMAT_VideoInfo)) {
		for (const auto& sudPinType : sudPinTypesIn) {
			if (pmt->subtype == *sudPinType.clsMinorType) {
				CAutoLock cRendererLock(&m_RendererLock);

				if (!m_VideoProcessor->VerifyMediaType(pmt)) {
					return VFW_E_UNSUPPORTED_VIDEO;
				}

				return S_OK;
			}
		}
	}

	return E_FAIL;
}

HRESULT CMpcVideoRenderer::SetMediaType(const CMediaType *pmt)
{
	DLog("CMpcVideoRenderer::SetMediaType()\n{}", WToA(MediaType2Str(pmt)));

	CheckPointer(pmt, E_POINTER);
	CheckPointer(pmt->pbFormat, E_POINTER);

	CAutoLock cVideoLock(&m_InterfaceLock);
	CAutoLock cRendererLock(&m_RendererLock);

	Com::SmartSize aspect, framesize;
	m_VideoProcessor->GetAspectRatio(&aspect.cx, &aspect.cy);
	m_VideoProcessor->GetVideoSize(&framesize.cx, &framesize.cy);

	CMediaType mt(*pmt);

	auto inputPin = static_cast<CVideoRendererInputPin*>(m_pInputPin);
	inputPin->ClearNewMediaType();
	if (!inputPin->FrameInVideoMem()) {
		CMediaType mtNew(*pmt);
		long ret = CalcImageSize(mtNew, true);

		if (mtNew != mt) {
			if (S_OK == m_pInputPin->GetConnected()->QueryAccept(&mtNew)) {
				DLog("CMpcVideoRenderer::SetMediaType() : upstream filter accepted new media type. QueryAccept return S_OK");
				inputPin->SetNewMediaType(mtNew);
				mt = std::move(mtNew);
			}
		}
	}

	if (!m_VideoProcessor->InitMediaType(&mt)) {
		return VFW_E_UNSUPPORTED_VIDEO;
	}

	if (!m_videoRect.IsRectNull()) {
		m_VideoProcessor->SetVideoRect(m_videoRect);
	}

	Com::SmartSize aspectNew, framesizeNew;
	m_VideoProcessor->GetAspectRatio(&aspectNew.cx, &aspectNew.cy);
	m_VideoProcessor->GetVideoSize(&framesizeNew.cx, &framesizeNew.cy);

	if (framesize.cx && aspect.cx && m_pSink) {
		if (aspectNew != aspect || framesizeNew != framesize
				|| aspectNew != m_videoAspectRatio || framesizeNew != m_videoSize) {
			m_pSink->Notify(EC_VIDEO_SIZE_CHANGED, MAKELPARAM(framesizeNew.cx, framesizeNew.cy), 0);
		}
	}

	m_videoSize = framesizeNew;
	m_videoAspectRatio = aspectNew;

	return S_OK;
}

HRESULT CMpcVideoRenderer::DoRenderSample(IMediaSample* pSample)
{
	CheckPointer(pSample, E_POINTER);

	HRESULT hr = m_VideoProcessor->ProcessSample(pSample);

	if (SUCCEEDED(hr)) {
		m_bValidBuffer = true;
	}

	if (m_Stepping && !(--m_Stepping)) {
		this->NotifyEvent(EC_STEP_COMPLETE, 0, 0);
	}

	return hr;
}

HRESULT CMpcVideoRenderer::Receive(IMediaSample* pSample)
{
	// override CBaseRenderer::Receive() for the implementation of the search during the pause

	if (m_bFlushing) {
		DLog("CMpcVideoRenderer::Receive() - flushing, skip sample");
		return S_OK;
	}

	ASSERT(pSample);

	// It may return VFW_E_SAMPLE_REJECTED code to say don't bother

	HRESULT hr = PrepareReceive(pSample);
	ASSERT(m_bInReceive == SUCCEEDED(hr));
	if (FAILED(hr)) {
		if (hr == VFW_E_SAMPLE_REJECTED) {
			return NOERROR;
		}
		return hr;
	}

	// We realize the palette in "PrepareRender()" so we have to give away the
	// filter lock here.
	if (m_State == State_Paused) {
		// no need to use InterlockedExchange
		m_bInReceive = FALSE;
		{
			// We must hold both these locks
			CAutoLock cVideoLock(&m_InterfaceLock);
			if (m_State == State_Stopped)
				return NOERROR;

			m_bInReceive = TRUE;
			CAutoLock cRendererLock(&m_RendererLock);
		}
		Ready();
	}

	if (m_State == State_Paused) {
		m_bInReceive = FALSE;

		CAutoLock cRendererLock(&m_RendererLock);
		DoRenderSample(m_pMediaSample);
	}

	// Having set an advise link with the clock we sit and wait. We may be
	// awoken by the clock firing or by a state change. The rendering call
	// will lock the critical section and check we can still render the data

	hr = WaitForRenderTime();
	if (FAILED(hr)) {
		m_bInReceive = FALSE;
		return NOERROR;
	}

	//  Set this here and poll it until we work out the locking correctly
	//  It can't be right that the streaming stuff grabs the interface
	//  lock - after all we want to be able to wait for this stuff
	//  to complete
	m_bInReceive = FALSE;

	// We must hold both these locks
	CAutoLock cVideoLock(&m_InterfaceLock);

	// since we gave away the filter wide lock, the sate of the filter could
	// have chnaged to Stopped
	if (m_State == State_Stopped)
		return NOERROR;

	CAutoLock cRendererLock(&m_RendererLock);

	// Deal with this sample

	if (m_State == State_Running) {
		Render(m_pMediaSample);
	}

	ClearPendingSample();
	SendEndOfStream();
	CancelNotification();
	return NOERROR;
}

void CMpcVideoRenderer::UpdateDisplayInfo()
{
	const HMONITOR hMonPrimary = MonitorFromPoint(Com::SmartPoint(0, 0), MONITOR_DEFAULTTOPRIMARY);

	MONITORINFOEXW mi = { sizeof(mi) };
	GetMonitorInfoW(m_hMon, (MONITORINFO*)&mi);

	bool ret = GetDisplayConfig(mi.szDevice, m_DisplayConfig);
	if (m_hMon == hMonPrimary) {
		m_bPrimaryDisplay = true;
	} else {
		m_bPrimaryDisplay = false;
	}

	m_VideoProcessor->SetDisplayInfo(m_DisplayConfig, m_bPrimaryDisplay, m_bIsFullscreen);
}

void CMpcVideoRenderer::OnDisplayModeChange(const bool bReset/* = false*/)
{
	if (m_bDisplayModeChanging) {
		return;
	}

	m_bDisplayModeChanging = true;

	if (bReset && !m_VideoProcessor->IsInit()) {
		m_VideoProcessor->Reset();
	}
	auto winSystem = dynamic_cast<CWinSystemWin32*>(CServiceBroker::GetWinSystem());
	
	m_hMon = MonitorFromWindow(winSystem->GetHwnd(), MONITOR_DEFAULTTONEAREST);
	UpdateDisplayInfo();

	m_bDisplayModeChanging = false;
}

void CMpcVideoRenderer::OnWindowMove()
{
	/*
	const HMONITOR hMon = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
	if (hMon != m_hMon) {
		if (m_Sets.bReinitByDisplay) {
			CAutoLock cRendererLock(&m_RendererLock);

			Init(true);
		}
		else if (m_VideoProcessor->Type() == VP_DX11) {
			CAutoLock cRendererLock(&m_RendererLock);

			m_VideoProcessor->Reset();
		}

		m_hMon = hMon;
		UpdateDisplayInfo();
	}*/
	
}

STDMETHODIMP CMpcVideoRenderer::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
	CheckPointer(ppv, E_POINTER);

	return
		QI(IKsPropertySet)
		QI(IMFGetService)
		QI(IBasicVideo)
		QI(IBasicVideo2)
		QI(IVideoWindow)
		QI(IExFilterConfig)
		__super::NonDelegatingQueryInterface(riid, ppv);
}

// IMediaFilter
STDMETHODIMP CMpcVideoRenderer::Run(REFERENCE_TIME rtStart)
{
	CLog::Log(LOGINFO, "{} {}", __FUNCTION__, CRefTime(rtStart).Millisecs());
	m_rtReftimeStart = rtStart;
	if (m_State == State_Running) {
		return NOERROR;
	}

	CAutoLock cVideoLock(&m_InterfaceLock);
	m_filterState = State_Running;


	return CBaseVideoRenderer2::Run(rtStart);
}

STDMETHODIMP CMpcVideoRenderer::Pause()
{
	DLog("CMpcVideoRenderer::Pause()");

	m_filterState = State_Paused;

	return CBaseVideoRenderer2::Pause();
}

STDMETHODIMP CMpcVideoRenderer::Stop()
{
	DLog("CMpcVideoRenderer::Stop()");

	m_filterState = State_Stopped;
	m_bValidBuffer = false;

	return CBaseVideoRenderer2::Stop();
}

#if _DEBUG
CStdStringW PropSetAndIdToString(REFGUID PropSet, ULONG Id)
{
#define UNPACK_VALUE(VALUE) case VALUE: str += L#VALUE; break;
	CStdStringW str;
	if (PropSet == AM_KSPROPSETID_CopyProt) {
		str.assign(L"AM_KSPROPSETID_CopyProt, ");
		switch (Id) {
			UNPACK_VALUE(AM_PROPERTY_COPY_MACROVISION);
			UNPACK_VALUE(AM_PROPERTY_COPY_ANALOG_COMPONENT);
			UNPACK_VALUE(AM_PROPERTY_COPY_DIGITAL_CP);
		default:
			str += std::to_wstring(Id);
		};
	}
	else if (PropSet == AM_KSPROPSETID_FrameStep) {
		str.assign(L"AM_KSPROPSETID_FrameStep, ");
		switch (Id) {
			UNPACK_VALUE(AM_PROPERTY_FRAMESTEP_STEP);
			UNPACK_VALUE(AM_PROPERTY_FRAMESTEP_CANCEL);
			UNPACK_VALUE(AM_PROPERTY_FRAMESTEP_CANSTEP);
			UNPACK_VALUE(AM_PROPERTY_FRAMESTEP_CANSTEPMULTIPLE);
		default:
			str += std::to_wstring(Id);
		};
	}
	else {
		str.assign(GUIDtoWString(PropSet) + L", " + std::to_wstring(Id).c_str());
	}
	return str;
#undef UNPACK_VALUE
}

#endif

// IKsPropertySet
STDMETHODIMP CMpcVideoRenderer::Set(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength)
{
	DLog("IKsPropertySet::Set({}, {}, {}, {}, {})", WToA(PropSetAndIdToString(PropSet, Id)), pInstanceData, InstanceLength, pPropertyData, DataLength);

	if (PropSet == AM_KSPROPSETID_CopyProt) {
		if (Id == AM_PROPERTY_COPY_MACROVISION || Id == AM_PROPERTY_COPY_DIGITAL_CP) {
			DLogIf(Id == AM_PROPERTY_COPY_MACROVISION, "No Macrovision please");
			DLogIf(Id == AM_PROPERTY_COPY_DIGITAL_CP, "No Digital CP please");
			return S_OK;
		}
	}
	else if (PropSet == AM_KSPROPSETID_FrameStep) {
		if (Id == AM_PROPERTY_FRAMESTEP_STEP) {
			m_Stepping = 1;
			return S_OK;
		}
		if (Id == AM_PROPERTY_FRAMESTEP_CANSTEP || Id == AM_PROPERTY_FRAMESTEP_CANSTEPMULTIPLE) {
			return S_OK;
		}
	}
	else {
		return E_PROP_SET_UNSUPPORTED;
	}

	return E_PROP_ID_UNSUPPORTED;
}

STDMETHODIMP CMpcVideoRenderer::Get(REFGUID PropSet, ULONG Id, LPVOID pInstanceData, ULONG InstanceLength, LPVOID pPropertyData, ULONG DataLength, ULONG* pBytesReturned)
{
	DLog("IKsPropertySet::Get({}, {}, {}, {}, {}, ...)", WToA(PropSetAndIdToString(PropSet, Id)), pInstanceData, InstanceLength, pPropertyData, DataLength);

	if (PropSet == AM_KSPROPSETID_CopyProt) {
		if (Id == AM_PROPERTY_COPY_ANALOG_COMPONENT) {
			if (pPropertyData && DataLength >= sizeof(ULONG) && pBytesReturned) {
				*(ULONG*)pPropertyData = FALSE;
				*pBytesReturned = sizeof(ULONG);
				return S_OK;
			}
			return E_INVALIDARG;
		}
	}
	else {
		return E_PROP_SET_UNSUPPORTED;
	}

	return E_PROP_ID_UNSUPPORTED;
}

STDMETHODIMP CMpcVideoRenderer::QuerySupported(REFGUID PropSet, ULONG Id, ULONG* pTypeSupport)
{
	DLog("IKsPropertySet::QuerySupported({}, ...)", WToA(PropSetAndIdToString(PropSet, Id)));

	if (PropSet == AM_KSPROPSETID_CopyProt) {
		if (Id == AM_PROPERTY_COPY_MACROVISION || Id == AM_PROPERTY_COPY_DIGITAL_CP) {
			*pTypeSupport = KSPROPERTY_SUPPORT_SET;
			return S_OK;
		}
		if (Id == AM_PROPERTY_COPY_ANALOG_COMPONENT) {
			*pTypeSupport = KSPROPERTY_SUPPORT_GET;
			return S_OK;
		}
	}
	else {
		return E_PROP_SET_UNSUPPORTED;
	}

	return E_PROP_ID_UNSUPPORTED;
}

// IMFGetService
STDMETHODIMP CMpcVideoRenderer::GetService(REFGUID guidService, REFIID riid, LPVOID *ppvObject)
{
	if (guidService == MR_VIDEO_ACCELERATION_SERVICE) {
		if (riid == IID_IDirect3DDeviceManager9 && m_VideoProcessor->GetDeviceManager9()) {
			return m_VideoProcessor->GetDeviceManager9()->QueryInterface(riid, ppvObject);
		}
	}
	else if (guidService == MR_VIDEO_MIXER_SERVICE) {
		if (riid == IID_IMFVideoProcessor || riid == IID_IMFVideoMixerBitmap) {
			return m_VideoProcessor->QueryInterface(riid, ppvObject);
		}
	}

	return E_NOINTERFACE;
}

// IBasicVideo
STDMETHODIMP CMpcVideoRenderer::GetSourcePosition(long *pLeft, long *pTop, long *pWidth, long *pHeight)
{
	CheckPointer(pLeft,E_POINTER);
	CheckPointer(pTop,E_POINTER);
	CheckPointer(pWidth,E_POINTER);
	CheckPointer(pHeight,E_POINTER);

	Com::SmartRect rect;
	{
		CAutoLock cVideoLock(&m_InterfaceLock);

		m_VideoProcessor->GetSourceRect(rect);
	}

	*pLeft = rect.left;
	*pTop = rect.top;
	*pWidth = rect.Width();
	*pHeight = rect.Height();

	return S_OK;
}

STDMETHODIMP CMpcVideoRenderer::SetDestinationPosition(long Left, long Top, long Width, long Height)
{
	const Com::SmartRect videoRect(Left, Top, Left + Width, Top + Height);
	if (videoRect.IsRectNull()) {
		return S_OK;
	}

	if (videoRect != m_videoRect) {
		m_videoRect = videoRect;

		CAutoLock cRendererLock(&m_RendererLock);

		m_VideoProcessor->SetVideoRect(videoRect);
	}

	return S_OK;
}

STDMETHODIMP CMpcVideoRenderer::GetDestinationPosition(long *pLeft, long *pTop, long *pWidth, long *pHeight)
{
	CheckPointer(pLeft,E_POINTER);
	CheckPointer(pTop,E_POINTER);
	CheckPointer(pWidth,E_POINTER);
	CheckPointer(pHeight,E_POINTER);

	Com::SmartRect rect;
	{
		CAutoLock cVideoLock(&m_InterfaceLock);

		m_VideoProcessor->GetVideoRect(rect);
	}

	*pLeft = rect.left;
	*pTop = rect.top;
	*pWidth = rect.Width();
	*pHeight = rect.Height();

	return S_OK;
}

STDMETHODIMP CMpcVideoRenderer::GetVideoSize(long *pWidth, long *pHeight)
{
	// retrieves the native video's width and height.
	return m_VideoProcessor->GetVideoSize(pWidth, pHeight);
}

STDMETHODIMP CMpcVideoRenderer::GetCurrentImage(long *pBufferSize, long *pDIBImage)
{
	CheckPointer(pBufferSize, E_POINTER);

	CAutoLock cVideoLock(&m_InterfaceLock);
	CAutoLock cRendererLock(&m_RendererLock);
	HRESULT hr;

	Com::SmartSize framesize;
	long aspectX, aspectY;
	int iRotation;

	m_VideoProcessor->GetVideoSize(&framesize.cx, &framesize.cy);
	m_VideoProcessor->GetAspectRatio(&aspectX, &aspectY);
	iRotation = m_VideoProcessor->GetRotation();

	if (aspectX > 0 && aspectY > 0) {
		if (iRotation == 90 || iRotation == 270) {
			framesize.cy = MulDiv(framesize.cx, aspectY, aspectX);
		} else {
			framesize.cx = MulDiv(framesize.cy, aspectX, aspectY);
		}
	}

	const auto w = framesize.cx;
	const auto h = framesize.cy;

	// VFW_E_NOT_PAUSED ?

	if (w <= 0 || h <= 0) {
		return E_FAIL;
	}
	long size = w * h * 4 + sizeof(BITMAPINFOHEADER);

	if (pDIBImage == nullptr) {
		*pBufferSize = size;
		return S_OK;
	}

	if (size > *pBufferSize) {
		return E_OUTOFMEMORY;
	}

	return E_FAIL;
}

// IBasicVideo2
STDMETHODIMP CMpcVideoRenderer::GetPreferredAspectRatio(long *plAspectX, long *plAspectY)
{
	return m_VideoProcessor->GetAspectRatio(plAspectX, plAspectY);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CMpcVideoRenderer* pThis = reinterpret_cast <CMpcVideoRenderer*>(GetWindowLongPtrW(hwnd, 0));
	if (!pThis) {
		if ((uMsg != WM_NCCREATE)
				|| (nullptr == (pThis = (CMpcVideoRenderer*)((LPCREATESTRUCTW)lParam)->lpCreateParams))) {
			return DefWindowProcW(hwnd, uMsg, wParam, lParam);
		}

		SetWindowLongPtrW(hwnd, 0, (LONG_PTR)pThis);
	}

	return pThis->OnReceiveMessage(hwnd, uMsg, wParam, lParam);
}

STDMETHODIMP CMpcVideoRenderer::put_MessageDrain(OAHWND Drain)
{
	if (m_pInputPin == nullptr || m_pInputPin->IsConnected() == FALSE) {
		return VFW_E_NOT_CONNECTED;
	}
	m_hWndDrain = (HWND)Drain;
	return S_OK;
}

STDMETHODIMP CMpcVideoRenderer::get_MessageDrain(OAHWND* Drain)
{
	CheckPointer(Drain, E_POINTER);
	if (m_pInputPin == nullptr || m_pInputPin->IsConnected() == FALSE) {
		return VFW_E_NOT_CONNECTED;
	}
	*Drain = (OAHWND)m_hWndDrain;
	return S_OK;
}


void CMpcVideoRenderer::DoAfterChangingDevice()
{
	if (m_pInputPin->IsConnected() == TRUE && m_pSink) {
		DLog("CMpcVideoRenderer::DoAfterChangingDevice()");
		m_bValidBuffer = false;
		auto pPin = (IPin*)m_pInputPin;
		m_pInputPin->AddRef();
		EXECUTE_ASSERT(S_OK == m_pSink->Notify(EC_DISPLAY_CHANGED, (LONG_PTR)pPin, 0));
		SetAbortSignal(TRUE);
		SAFE_RELEASE(m_pMediaSample);
		m_pInputPin->Release();
	}
}

LRESULT CMpcVideoRenderer::OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (m_hWndDrain && !InSendMessage() && !m_bIsFullscreen) {
		switch (uMsg) {
			case WM_CHAR:
			case WM_DEADCHAR:
			case WM_KEYDOWN:
			case WM_KEYUP:
			case WM_LBUTTONDBLCLK:
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_MBUTTONDBLCLK:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			case WM_MOUSEACTIVATE:
			case WM_MOUSEMOVE:
			case WM_NCLBUTTONDBLCLK:
			case WM_NCLBUTTONDOWN:
			case WM_NCLBUTTONUP:
			case WM_NCMBUTTONDBLCLK:
			case WM_NCMBUTTONDOWN:
			case WM_NCMBUTTONUP:
			case WM_NCMOUSEMOVE:
			case WM_NCRBUTTONDBLCLK:
			case WM_NCRBUTTONDOWN:
			case WM_NCRBUTTONUP:
			case WM_RBUTTONDBLCLK:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_XBUTTONDOWN:
			case WM_XBUTTONUP:
			case WM_XBUTTONDBLCLK:
			case WM_MOUSEWHEEL:
			case WM_MOUSEHWHEEL:
			case WM_SYSCHAR:
			case WM_SYSDEADCHAR:
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
				PostMessageW(m_hWndDrain, uMsg, wParam, lParam);
				return 0L;
		}
	}

	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
