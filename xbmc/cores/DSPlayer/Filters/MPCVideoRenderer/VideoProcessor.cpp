
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


#include "VideoProcessor.h"
#include <uuids.h>
#include <Mferror.h>
#include <Mfidl.h>
#include <dwmapi.h>
#include <optional>
#include "Helper.h"
#include "Times.h"
#include "VideoRenderer.h"
#include "Include/Version.h"
#include "Include/ID3DVideoMemoryConfiguration.h"
#include "shaders.h"

#include "CPUInfo2.h"
 //kodi
#include "DSResource.h"
#include "DSPlayer.h"
#include "ServiceBroker.h"
#include "application/ApplicationComponents.h"
#include "application/ApplicationPlayer.h"
#include "windowing/windows/WinSystemWin32DX.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "GUIInfoManager.h"
#include "Filters/RendererSettings.h"
#include "PlHelper.h"
#include "settings/SettingsComponent.h"

#define DEBUGEXTREME 0

#define GetDevice DX::DeviceResources::Get()->GetD3DDevice()
#define GetSwapChain DX::DeviceResources::Get()->GetSwapChain()

struct VERTEX {
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT2 TexCoord;
};

static void FillVertices(VERTEX(&Vertices)[4], const UINT srcW, const UINT srcH, const RECT& srcRect,
	const int iRotation, const bool bFlip)
{
	const float src_dx = 1.0f / srcW;
	const float src_dy = 1.0f / srcH;
	float src_l = src_dx * srcRect.left;
	float src_r = src_dx * srcRect.right;
	const float src_t = src_dy * srcRect.top;
	const float src_b = src_dy * srcRect.bottom;

	POINT points[4];
	switch (iRotation) {
	case 90:
		points[0] = { -1, +1 };
		points[1] = { +1, +1 };
		points[2] = { -1, -1 };
		points[3] = { +1, -1 };
		break;
	case 180:
		points[0] = { +1, +1 };
		points[1] = { +1, -1 };
		points[2] = { -1, +1 };
		points[3] = { -1, -1 };
		break;
	case 270:
		points[0] = { +1, -1 };
		points[1] = { -1, -1 };
		points[2] = { +1, +1 };
		points[3] = { -1, +1 };
		break;
	default:
		points[0] = { -1, -1 };
		points[1] = { -1, +1 };
		points[2] = { +1, -1 };
		points[3] = { +1, +1 };
	}

	if (bFlip) {
		std::swap(src_l, src_r);
	}

	// Vertices for drawing whole texture
	// 2 ___4
	//  |\ |
	// 1|_\|3
	Vertices[0] = { {(float)points[0].x, (float)points[0].y, 0}, {src_l, src_b} };
	Vertices[1] = { {(float)points[1].x, (float)points[1].y, 0}, {src_l, src_t} };
	Vertices[2] = { {(float)points[2].x, (float)points[2].y, 0}, {src_r, src_b} };
	Vertices[3] = { {(float)points[3].x, (float)points[3].y, 0}, {src_r, src_t} };
}

static HRESULT CreateVertexBuffer(ID3D11Device* pDevice, ID3D11Buffer** ppVertexBuffer,
	const UINT srcW, const UINT srcH, const RECT& srcRect,
	const int iRotation, const bool bFlip)
{
	ASSERT(ppVertexBuffer);
	ASSERT(*ppVertexBuffer == nullptr);

	VERTEX Vertices[4];
	FillVertices(Vertices, srcW, srcH, srcRect, iRotation, bFlip);

	D3D11_BUFFER_DESC BufferDesc = { sizeof(Vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
	D3D11_SUBRESOURCE_DATA InitData = { Vertices, 0, 0 };

	HRESULT hr = pDevice->CreateBuffer(&BufferDesc, &InitData, ppVertexBuffer);
	DLogIf(FAILED(hr), "CreateVertexBuffer() : CreateBuffer() failed with error {}", WToA(HR2Str(hr)));

	return hr;
}

//
// CVideoProcessor
//

CVideoProcessor::CVideoProcessor(CMpcVideoRenderer* pFilter)
	: m_pFilter(pFilter),
	m_hUploadEvent(nullptr),
	m_iPresCount(0)
{
	//initialize and set settings comming from settings.xml
	g_dsSettings.Initialize("mpcvr");
	std::shared_ptr<CSettings> pSetting = CServiceBroker::GetSettingsComponent()->GetSettings();

	MPC_SETTINGS->displayStats = (DS_STATS)pSetting->GetInt(CSettings::SETTING_DSPLAYER_VR_DISPLAY_STATS);
	MPC_SETTINGS->m_pPlaceboOptions = (LIBPLACEBO_SHADERS)pSetting->GetInt(CSettings::SETTING_DSPLAYER_VR_LIBPLACEBO_SHADERS);
	MPC_SETTINGS->bVPUseRTXVideoHDR = pSetting->GetBool("dsplayer.vr.rtxhdr");
	MPC_SETTINGS->bD3D11TextureSampler = (D3D11_TEXTURE_SAMPLER)pSetting->GetInt(CSettings::SETTING_DSPLAYER_VR_TEXTURE_SAMPLER);
	MPC_SETTINGS->iVPUseSuperRes = pSetting->GetInt("dsplayer.vr.superres");
	MPC_SETTINGS->iUploadBuffers = pSetting->GetInt("dsplayer.vr.uploadbuffer");//todo make it work
	MPC_SETTINGS->iProcessingBuffers = pSetting->GetInt("dsplayer.vr.processingbuffer");
	MPC_SETTINGS->iPresentationBuffers = pSetting->GetInt("dsplayer.vr.presentationbuffer");


	//TODO add buffer size
	m_pFreePresentationQueue.Resize(MPC_SETTINGS->iProcessingBuffers);
	m_pFreeProcessingQueue.Resize(MPC_SETTINGS->iPresentationBuffers);

	m_pFinalTextureSampler = D3D11_INTERNAL_SHADERS;//this is set during the init media type

	m_nCurrentAdapter = -1;
	CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>()->Register(this);
	HRESULT hr = CreateDXGIFactory1(IID_IDXGIFactory1, (void**)&m_pDXGIFactory1);
	if (FAILED(hr)) {
		CLog::Log(LOGERROR, "{} : CreateDXGIFactory1() failed with error {}", __FUNCTION__, WToA(HR2Str(hr)).c_str());
		return;
	}

	// set default ProcAmp ranges and values
	SetDefaultDXVA2ProcAmpRanges(m_DXVA2ProcAmpRanges);
	SetDefaultDXVA2ProcAmpValues(m_DXVA2ProcAmpValues);

	Microsoft::WRL::ComPtr<IDXGIAdapter> pDXGIAdapter;
	for (UINT adapter = 0; m_pDXGIFactory1->EnumAdapters(adapter, &pDXGIAdapter) != DXGI_ERROR_NOT_FOUND; ++adapter) {
		Microsoft::WRL::ComPtr<IDXGIOutput> pDXGIOutput;
		for (UINT output = 0; pDXGIAdapter->EnumOutputs(output, &pDXGIOutput) != DXGI_ERROR_NOT_FOUND; ++output) {
			DXGI_OUTPUT_DESC desc{};
			if (SUCCEEDED(pDXGIOutput->GetDesc(&desc))) {
				DisplayConfig_t displayConfig = {};
				if (GetDisplayConfig(desc.DeviceName, displayConfig)) {
					m_hdrModeStartState[desc.DeviceName] = displayConfig.advancedColor.advancedColorEnabled;
				}
			}

			pDXGIOutput = nullptr;
		}

		pDXGIAdapter = nullptr;
	}
	// Create a stop event to signal thread termination.
	m_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	// Create events for each stage.
	m_hUploadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hProcessEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hFlushEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hResizeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	// Start the threads for each queue.
	m_hUploadThread = CreateThread(NULL, 0, UploadThread, this, 0, NULL);
	m_hProcessThread = CreateThread(NULL, 0, ProcessThread, this, 0, NULL);

	CMPCVRRenderer::Get()->SetCallback(this);
	m_rtStartStream = -1;
	m_iPresCount = 0;
}

CVideoProcessor::~CVideoProcessor()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	ReleaseDevice();

	m_pDXGIFactory1 = nullptr;
	// Signal termination.
	if (m_hStopEvent)
		SetEvent(m_hStopEvent);

	// Wait for each thread to finish.
	if (m_hUploadThread) {
		WaitForSingleObject(m_hUploadThread, INFINITE);
		CloseHandle(m_hUploadThread);
	}
	if (m_hProcessThread) {
		WaitForSingleObject(m_hProcessThread, INFINITE);
		CloseHandle(m_hProcessThread);
	}

	// Clean up event handles.
	if (m_hStopEvent)       CloseHandle(m_hStopEvent);
	if (m_hUploadEvent)     CloseHandle(m_hUploadEvent);
	if (m_hProcessEvent)    CloseHandle(m_hProcessEvent);
	if (m_hFlushEvent)      CloseHandle(m_hFlushEvent);

	// Cleanup queues left ... should we call the flush before??
	m_uploadQueue.flush();
	m_processingQueue.flush();
	m_presentationQueue.flush();
	m_pFreeProcessingQueue.flush();
	m_pFreePresentationQueue.flush();
}

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

HRESULT CVideoProcessor::ProcessSample(IMediaSample* pSample)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	{

		if (m_uploadQueue.size() == 4)
			return S_FALSE;
		pSample->AddRef();
		m_uploadQueue.push(pSample);
	}
	//temp


	SetEvent(m_hUploadEvent);
	return S_OK;
}


void CVideoProcessor::FlushSampledQueue()
{
	m_uploadQueue.flush();
}

// ----------------------
		// HELPER FUNCTIONS
		// ----------------------

		// Convert an incoming IMediaSample into a D3D11 texture.
		// This function creates a new dynamic texture and fills it with the sample’s data.
CMPCVRFrame CVideoProcessor::ConvertSampleToFrame(IMediaSample* pSample)
{
	BYTE* data = nullptr;
	const long size = pSample->GetActualDataLength();
	REFERENCE_TIME startUpload, endUpload;
	m_pFilter->m_pClock->GetTime(&startUpload);

	CMPCVRFrame pFrame;
	m_pFreeProcessingQueue.wait_and_pop(pFrame);

	pFrame.color = {};
	pFrame.repr = {};

	//Create the texture if not already created
	if (!pFrame.pTexture.Get())
		pFrame.pTexture.Create(m_srcWidth, m_srcHeight, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, DX::DeviceResources::Get()->GetBackBuffer().GetFormat(), "CMPCVRRenderer Merged plane", true, 0U);

	//
	ProcessFrame(pSample, pFrame);

	if (size > 0 && S_OK == pSample->GetPointer(&data))
	{
		HRESULT hr = S_FALSE;
		D3D11_MAPPED_SUBRESOURCE mappedResource = {};

		if (m_TexSrcVideo.pTexture2.Get()) {
			hr = m_pDeviceContext->Map(pFrame.pTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			if (SUCCEEDED(hr)) {
				CopyFrameAsIs(m_srcHeight, (BYTE*)mappedResource.pData, mappedResource.RowPitch, data, m_srcPitch);
				m_pDeviceContext->Unmap(pFrame.pTexture.Get(), 0);

				hr = m_pDeviceContext->Map(m_TexSrcVideo.pTexture2.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
				if (SUCCEEDED(hr)) {
					const UINT cromaH = m_srcHeight / m_srcParams.pDX11Planes->div_chroma_h;
					const int cromaPitch = (m_TexSrcVideo.pTexture3) ? m_srcPitch / m_srcParams.pDX11Planes->div_chroma_w : m_srcPitch;
					data += m_srcPitch * m_srcHeight;
					CopyFrameAsIs(cromaH, (BYTE*)mappedResource.pData, mappedResource.RowPitch, data, cromaPitch);
					m_pDeviceContext->Unmap(m_TexSrcVideo.pTexture2.Get(), 0);

					if (m_TexSrcVideo.pTexture3) {
						hr = m_pDeviceContext->Map(m_TexSrcVideo.pTexture3.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
						if (SUCCEEDED(hr)) {
							data += cromaPitch * cromaH;
							CopyFrameAsIs(cromaH, (BYTE*)mappedResource.pData, mappedResource.RowPitch, data, cromaPitch);
							m_pDeviceContext->Unmap(m_TexSrcVideo.pTexture3.Get(), 0);
						}
					}
				}
			}
		}
		else {
			hr = DX::DeviceResources::Get()->GetImmediateContext()->Map(m_TexSrcVideo.pTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			if (SUCCEEDED(hr)) {
				ASSERT(m_pConvertFn);
				const BYTE* src = (m_srcPitch < 0) ? data + m_srcPitch * (1 - (int)m_srcLines) : data;
				m_pConvertFn(m_srcLines, (BYTE*)mappedResource.pData, mappedResource.RowPitch, src, m_srcPitch);
				DX::DeviceResources::Get()->GetImmediateContext()->Unmap(m_TexSrcVideo.pTexture.Get(), 0);
			}
		}
	}
	if (m_pFinalTextureSampler == D3D11_VP)
	{
		m_pDeviceContext->CopyResource(m_D3D11VP.GetNextInputTexture(m_SampleFormat), m_TexSrcVideo.pTexture.Get());
	}
	else
	{
		HRESULT hr = ConvertColorPass(pFrame.pTexture.Get());
	}

	Microsoft::WRL::ComPtr<ID3D11CommandList> pCommandList;

	if (FAILED(m_pDeviceContext->FinishCommandList(1, &pCommandList)))
	{
		CLog::LogF(LOGERROR, "{} failed to finish command queue.", __FUNCTION__);
	}
	else
	{
		D3DSetDebugName(pCommandList.Get(), "CommandList mpc deferred context");
		DX::DeviceResources::Get()->GetImmediateContext()->ExecuteCommandList(pCommandList.Get(), 0);
	}
	//gettime at start of stream
	//m_rtStartStream

	pSample->GetTime(&pFrame.pStartTime, &pFrame.pEndTime);

	m_pFilter->m_pClock->GetTime(&endUpload);

	if (m_rtStartStream == -1)
		m_rtStartStream = endUpload;
	pFrame.pStartTime += m_rtStartStream;
	pFrame.pEndTime += m_rtStartStream;
	pFrame.pUploadTime = endUpload - startUpload;

	return pFrame;
}

// ----------------------
// UPLOAD STAGE
// ----------------------
// This thread takes IMediaSamples from the upload queue,
// converts each sample into a D3D11 texture, and pushes the texture
// into the processing queue.
DWORD WINAPI CVideoProcessor::UploadThread(LPVOID lpParameter)
{
	CVideoProcessor* pThis = reinterpret_cast<CVideoProcessor*>(lpParameter);
	pThis->UploadLoop();
	return 0;
}

void CVideoProcessor::UploadLoop()
{
	HANDLE events[] = { m_hStopEvent, m_hFlushEvent ,m_hUploadEvent };
	while (true) {
		DWORD dwWait = WaitForMultipleObjects(3, events, FALSE, 250);
		//this fix the move window without having to send an event on stall
		if (dwWait == WAIT_TIMEOUT)
		{
			if (m_uploadQueue.size() > 0)
			{
				dwWait = WAIT_OBJECT_0 + 2;
				CLog::Log(LOGDEBUG, "{} stalled waking up upload thread", __FUNCTION__);
			}
		}
		if (dwWait == WAIT_OBJECT_0) // Stop event signaled
			break;
		else if (dwWait == WAIT_OBJECT_0 + 1) {
			//FLUSH
			m_uploadQueue.flush();
		}
		else if (dwWait == WAIT_OBJECT_0 + 2) {
			IMediaSample* pSample = nullptr;
			{
				//if the processing queue is not full process a sample
				if (m_processingQueue.size() < MPC_SETTINGS->iProcessingBuffers)
					m_uploadQueue.wait_and_pop(pSample);

			}
			if (pSample) {
				CMPCVRFrame pFrame;
				//the frame come from m_pFreeProcessingQueue
				pFrame = ConvertSampleToFrame(pSample);
				if (m_iPresCount == 10)
				{
					CStdStringW sNow;
					CRefTime rUploadTime(pFrame.pUploadTime);
					pFrame.pProcessingTime = 5;
					CRefTime rProcessingTime(pFrame.pProcessingTime);
					sNow.Format(L"Upload queue: %i ", m_uploadQueue.size());
					sNow.AppendFormat(L"Upload time : % ld", rUploadTime.Millisecs());
					CMPCVRRenderer::Get()->SetStatsTimings(sNow.c_str(), 3);

				}
				pSample->Release();
				if (pFrame.pTexture.Get()) {
					{
						m_processingQueue.push(pFrame);
					}
					SetEvent(m_hProcessEvent);
				}
			}
		}
	}
	CLog::Log(LOGINFO, "{} Upload Loop end", __FUNCTION__);
}

// ----------------------
// PROCESSING STAGE
// ----------------------
// This thread takes textures from the processing queue,
// applies any necessary processing (e.g. OSD compositing),
// and then pushes the processed texture into the presentation queue.
DWORD WINAPI CVideoProcessor::ProcessThread(LPVOID lpParameter)
{
	CVideoProcessor* pThis = reinterpret_cast<CVideoProcessor*>(lpParameter);
	pThis->ProcessLoop();
	return 0;
}

void CVideoProcessor::ProcessLoop()
{
	HANDLE events[] = { m_hStopEvent,m_hFlushEvent , m_hResizeEvent, m_hProcessEvent };
	while (true) {
		DWORD dwWait = WaitForMultipleObjects(4, events, FALSE, 250);
		//this fix the move window without having to send an event on stall
		if (dwWait == WAIT_TIMEOUT)
		{
			if (m_processingQueue.size() > 0)
			{
				dwWait = WAIT_OBJECT_0 + 3;
				CLog::Log(LOGDEBUG, "{} stalled waking up process thread", __FUNCTION__);
			}
		}
		if (dwWait == WAIT_OBJECT_0) // Stop event signaled
			break;
		else if (dwWait == WAIT_OBJECT_0 + 1) {
			//FLUSH
			for (;;)
			{
				if (m_processingQueue.size() > 0)
				{
					m_pFreeProcessingQueue.push(m_processingQueue.front());
					m_processingQueue.pop();
				}
				else
					break;
			}
			for (;;)
			{
				if (m_presentationQueue.size() > 0)
				{
					m_pFreePresentationQueue.push(m_presentationQueue.front());
					m_presentationQueue.pop();
				}
				else
					break;
			}
		}
		else if (dwWait == WAIT_OBJECT_0 + 2)
		{
			ResizeProcessFrame();
		}
		else if (dwWait == WAIT_OBJECT_0 + 3) {
			CMPCVRFrame pInputFrame, pOutputFrame;
			{
				if (!m_processingQueue.empty() && m_presentationQueue.size() < MPC_SETTINGS->iPresentationBuffers) {
					m_processingQueue.wait_and_pop(pInputFrame);
				}
			}
			if (pInputFrame.pTexture.Get())
			{
				m_pFreePresentationQueue.wait_and_pop(pOutputFrame);
				// Process the texture (for example, composite your OSD).
				REFERENCE_TIME startUpload, endUpload;
				m_pFilter->m_pClock->GetTime(&startUpload);
				if (m_pFinalTextureSampler == D3D11_VP)
				{
					ProcessFrameVP(pInputFrame, pOutputFrame);
					{
						m_presentationQueue.push(pOutputFrame);
						m_pFreeProcessingQueue.push(pInputFrame);
					}
				}
				else if (m_pFinalTextureSampler == D3D11_LIBPLACEBO)
				{
					ProcessFrameLibplacebo(pInputFrame, pOutputFrame);
					{
						m_presentationQueue.push(pOutputFrame);
						m_pFreeProcessingQueue.push(pInputFrame);
					}
				}
				else
				{
					CLog::Log(LOGINFO, "{} DSPLAYER notify playback started sent", __FUNCTION__);
				}
				m_pFilter->m_pClock->GetTime(&endUpload);
				CRefTime rProcessingTime = CRefTime(endUpload) - CRefTime(startUpload);
				CStdStringW sNow;

				sNow.Format(L"Processing queue: %i ", m_processingQueue.size());
				sNow.AppendFormat(L"Processing time : % ld", rProcessingTime.Millisecs());
				CMPCVRRenderer::Get()->SetStatsTimings(sNow.c_str(), 4);
				float fps;
				fps = 10000000.0 / m_rtAvgTimePerFrame;
				g_application.GetComponent<CApplicationPlayer>()->Configure(m_srcRectWidth, m_srcRectHeight, m_srcAspectRatioX, m_srcAspectRatioY, fps, 0);
				if (!m_bDsplayerNotified)
				{
					m_bDsplayerNotified = true;
					CDSPlayer::PostMessage(new CDSMsg(CDSMsg::PLAYER_PLAYBACK_STARTED), false);
					CLog::Log(LOGINFO, "{} DSPLAYER notify playback started sent", __FUNCTION__);
				}
			}
		}
	}
	CLog::Log(LOGINFO, "{} Process loop end", __FUNCTION__);
}

// Process the texture (e.g. blend in an OSD).
// For this example, the function is a no-op.
void CVideoProcessor::ProcessFrameLibplacebo(CMPCVRFrame& inputFrame, CMPCVRFrame& outputFrame)
{
	// Insert your D3D11 processing code here.
	// For example, you might use a compute shader or render-to-texture pass to composite your OSD.
	PL::CPlHelper* pHelper = CMPCVRRenderer::Get()->GetPlHelper();
	pl_d3d11_wrap_params frameInParams{};
	pl_d3d11_wrap_params frameOutParams{};
	pl_frame frameIn{};
	pl_frame frameOut{};
	pl_render_params params;
	pl_tex inTexture, outTexture;

	if (!outputFrame.pTexture.Get())
	{
		outputFrame.pTexture.Create(DX::DeviceResources::Get()->GetBackBuffer().GetWidth(), DX::DeviceResources::Get()->GetBackBuffer().GetHeight(), D3D11_USAGE_DEFAULT | D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, DX::DeviceResources::Get()->GetBackBuffer().GetFormat());

	}

	frameInParams.w = inputFrame.pTexture.GetWidth();
	frameInParams.h = inputFrame.pTexture.GetHeight();
	frameInParams.fmt = inputFrame.pTexture.GetFormat();
	frameInParams.tex = inputFrame.pTexture.Get();
	inTexture = pl_d3d11_wrap(pHelper->GetPLD3d11()->gpu, &frameInParams);
	frameIn.num_planes = 1;
	frameIn.planes[0].texture = inTexture;
	frameIn.planes[0].components = 3;
	frameIn.planes[0].component_mapping[0] = 0;
	frameIn.planes[0].component_mapping[1] = 1;
	frameIn.planes[0].component_mapping[2] = 2;
	frameIn.planes[0].component_mapping[3] = -1;
	frameIn.planes[0].flipped = false;
	frameIn.repr = inputFrame.repr;
	//If pre merged the output of the shaders is a rgb image
	frameIn.repr.sys = PL_COLOR_SYSTEM_RGB;
	frameIn.color = inputFrame.color;

	frameOutParams.w = outputFrame.pTexture.GetWidth();
	frameOutParams.h = outputFrame.pTexture.GetHeight();
	frameOutParams.fmt = outputFrame.pTexture.GetFormat();
	frameOutParams.tex = outputFrame.pTexture.Get();
	frameOutParams.array_slice = 1;

	outTexture = pl_d3d11_wrap(pHelper->GetPLD3d11()->gpu, &frameOutParams);
	frameOut.num_planes = 1;
	frameOut.planes[0].texture = outTexture;
	frameOut.planes[0].components = 4;
	frameOut.planes[0].component_mapping[0] = PL_CHANNEL_R;
	frameOut.planes[0].component_mapping[1] = PL_CHANNEL_G;
	frameOut.planes[0].component_mapping[2] = PL_CHANNEL_B;
	frameOut.planes[0].component_mapping[3] = PL_CHANNEL_A;
	frameOut.planes[0].flipped = false;

	frameIn.crop.x1 = m_srcWidth;
	frameIn.crop.y1 = m_srcHeight;
	frameOut.crop.x1 = outputFrame.pTexture.GetWidth();
	frameOut.crop.y1 = outputFrame.pTexture.GetHeight();
	frameOut.repr = frameIn.repr;
	frameOut.color = frameIn.color;
	frameOut.repr.sys = PL_COLOR_SYSTEM_RGB;
	frameOut.repr.levels = PL_COLOR_LEVELS_FULL;
	pl_frame_set_chroma_location(&frameOut, PL_CHROMA_LEFT);

	params = pl_render_default_params;

	params.info_priv = pHelper;
	params.info_callback = render_info_cb;

	pl_render_image(pHelper->GetPLRenderer(), &frameIn, &frameOut, &params);
	pl_gpu_finish(pHelper->GetPLD3d11()->gpu);

}

void CVideoProcessor::ProcessFrameVP(CMPCVRFrame& inputFrame, CMPCVRFrame& outputFrame)
{
	if (!outputFrame.pTexture.Get())
	{
		outputFrame.pTexture.Create(DX::DeviceResources::Get()->GetBackBuffer().GetWidth(), DX::DeviceResources::Get()->GetBackBuffer().GetHeight(), D3D11_USAGE_DEFAULT | D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, DX::DeviceResources::Get()->GetBackBuffer().GetFormat());
	}

	HRESULT hr = D3D11VPPass(outputFrame.pTexture.Get(), m_srcRect, outputFrame.pCurrentRect, false);
	if (FAILED(hr))
		CLog::Log(LOGERROR, "{}", __FUNCTION__);

}

HRESULT CVideoProcessor::Init(const HWND hwnd, bool* pChangeDevice/* = nullptr*/)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	CLog::Log(LOGINFO, "CVideoProcessor::Init()");

	auto winSystem = dynamic_cast<CWinSystemWin32*>(CServiceBroker::GetWinSystem());

	const bool bWindowChanged = (m_hWnd != winSystem->GetHwnd());
	m_hWnd = winSystem->GetHwnd();
	m_bHdrPassthroughSupport = false;
	m_bHdrDisplayModeEnabled = false;
	m_DisplayBitsPerChannel = 8;

	MONITORINFOEXW mi = { sizeof(mi) };
	GetMonitorInfoW(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY), (MONITORINFO*)&mi);
	DisplayConfig_t displayConfig = {};

	if (GetDisplayConfig(mi.szDevice, displayConfig)) {
		const auto& ac = displayConfig.advancedColor;
		m_bHdrPassthroughSupport = ac.advancedColorSupported && ac.advancedColorEnabled;
		m_bHdrDisplayModeEnabled = ac.advancedColorEnabled;
		m_DisplayBitsPerChannel = displayConfig.bitsPerChannel;
	}

	if (m_bIsFullscreen != m_pFilter->m_bIsFullscreen) {
		m_srcVideoTransferFunction = 0;
	}

	IDXGIAdapter* pDXGIAdapter = nullptr;

	const UINT currentAdapter = GetAdapter(winSystem->GetHwnd(), DX::DeviceResources::Get()->GetIDXGIFactory2(), &pDXGIAdapter);
	CheckPointer(pDXGIAdapter, E_FAIL);
	if (m_nCurrentAdapter == currentAdapter) {
		SAFE_RELEASE(pDXGIAdapter);

		SetCallbackDevice();

		if (!GetSwapChain || m_bIsFullscreen != m_pFilter->m_bIsFullscreen || bWindowChanged) {
			InitSwapChain();
			//UpdateStatsStatic();
			
		}

		return S_OK;
	}
	m_nCurrentAdapter = currentAdapter;

	if (m_bDecoderDevice && GetSwapChain) {
		return S_OK;
	}

	ReleaseDevice();

	CLog::LogF(LOGINFO, "{} Settings device ", __FUNCTION__);

	HRESULT hr = SetDevice(GetDevice, false);


	if (S_OK == hr) {
		if (pChangeDevice) {
			*pChangeDevice = true;
		}
	}

	if (m_VendorId == PCIV_Intel && CPUInfo::HaveSSE41()) {
		m_pCopyGpuFn = CopyGpuFrame_SSE41;
	}
	else {
		m_pCopyGpuFn = CopyFrameAsIs;
	}

	return hr;
}

bool CVideoProcessor::Initialized()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	return (GetDevice != nullptr && m_pDeviceContext != nullptr);
}


void CVideoProcessor::ProcessFrame(IMediaSample* pSample, CMPCVRFrame& frame)
{
	switch (m_srcExFmt.VideoPrimaries)
	{
		//case AVCOL_PRI_RESERVED0:       csp.primaries = PL_COLOR_PRIM_UNKNOWN;
	case DXVA2_VideoPrimaries_BT709: frame.color.primaries = PL_COLOR_PRIM_BT_709;	break;
		//case AVCOL_PRI_UNSPECIFIED:     csp.primaries = PL_COLOR_PRIM_UNKNOWN;
		//case AVCOL_PRI_RESERVED:        csp.primaries = PL_COLOR_PRIM_UNKNOWN;
	case DXVA2_VideoPrimaries_BT470_2_SysM:	frame.color.primaries = PL_COLOR_PRIM_BT_470M;	break;
	case DXVA2_VideoPrimaries_BT470_2_SysBG: frame.color.primaries = PL_COLOR_PRIM_BT_601_625;	break;
	case DXVA2_VideoPrimaries_SMPTE170M: frame.color.primaries = PL_COLOR_PRIM_BT_601_525;	break;
	case DXVA2_VideoPrimaries_SMPTE240M: frame.color.primaries = PL_COLOR_PRIM_BT_601_525;	break;
		//case AVCOL_PRI_FILM:            frame.color.primaries = PL_COLOR_PRIM_FILM_C;
	case MFVideoPrimaries_BT2020: frame.color.primaries = PL_COLOR_PRIM_BT_2020;	break;
	case MFVideoPrimaries_XYZ: frame.color.primaries = PL_COLOR_PRIM_CIE_1931; break;
	case MFVideoPrimaries_DCI_P3: frame.color.primaries = PL_COLOR_PRIM_DCI_P3; break;
		//case AVCOL_PRI_SMPTE432:        csp.primaries = PL_COLOR_PRIM_DISPLAY_P3;
		//case AVCOL_PRI_JEDEC_P22:       csp.primaries = PL_COLOR_PRIM_EBU_3213;
		//case AVCOL_PRI_NB:              csp.primaries = PL_COLOR_PRIM_COUNT;
	default: break;
	}

	switch (m_srcExFmt.VideoTransferFunction)
	{
		//case AVCOL_TRC_RESERVED0:       csp.transfer = PL_COLOR_TRC_UNKNOWN;
	case DXVA2_VideoTransFunc_709: frame.color.transfer = PL_COLOR_TRC_BT_1886; break;// EOTF != OETF	
		//case AVCOL_TRC_UNSPECIFIED:     csp.transfer = PL_COLOR_TRC_UNKNOWN;
		//case AVCOL_TRC_RESERVED:        csp.transfer = PL_COLOR_TRC_UNKNOWN;
	case DXVA2_VideoTransFunc_22:         frame.color.transfer = PL_COLOR_TRC_GAMMA22; break;
	case DXVA2_VideoTransFunc_28:         frame.color.transfer = PL_COLOR_TRC_GAMMA28; break;
	case DXVA2_VideoTransFunc_240M:       frame.color.transfer = PL_COLOR_TRC_BT_1886; break;// EOTF != OETF
	case DXVA2_VideoTransFunc_10:          frame.color.transfer = PL_COLOR_TRC_LINEAR; break;
	case MFVideoTransFunc_Log_100:             frame.color.transfer = PL_COLOR_TRC_UNKNOWN; break; // missing
	case MFVideoTransFunc_Log_316:        frame.color.transfer = PL_COLOR_TRC_UNKNOWN; break; // missing
		//case AVCOL_TRC_IEC61966_2_4:    frame.color.transfer = PL_COLOR_TRC_BT_1886; // EOTF != OETF
		//case AVCOL_TRC_BT1361_ECG:      frame.color.transfer = PL_COLOR_TRC_BT_1886; // ETOF != OETF
		//case AVCOL_TRC_IEC61966_2_1:    frame.color.transfer = PL_COLOR_TRC_SRGB;
			/*for this one lavfilters use this will need to look into when on hdr
			case AVCOL_TRC_BT2020_10:
		case AVCOL_TRC_BT2020_12:	fmt.VideoTransferFunction = (matrix == AVCOL_SPC_BT2020_CL) ? MFVideoTransFunc_2020_const : MFVideoTransFunc_2020; break;*/
	case MFVideoTransFunc_2020_const:       frame.color.transfer = PL_COLOR_TRC_BT_1886; break; // EOTF != OETF
	case MFVideoTransFunc_2020:       frame.color.transfer = PL_COLOR_TRC_BT_1886; break; // EOTF != OETF
	case MFVideoTransFunc_2084:       frame.color.transfer = PL_COLOR_TRC_PQ; break;
	case MFVideoTransFunc_HLG:    frame.color.transfer = PL_COLOR_TRC_HLG; break;
	}
	switch (m_srcExFmt.VideoTransferMatrix)
	{
		//case AVCOL_SPC_RGB:                 repr.sys = PL_COLOR_SYSTEM_RGB;
	case DXVA2_VideoTransferMatrix_BT709:	frame.repr.sys = PL_COLOR_SYSTEM_BT_709; break;
		//case AVCOL_SPC_UNSPECIFIED:         repr.sys = PL_COLOR_SYSTEM_UNKNOWN;
		//case AVCOL_SPC_RESERVED:            repr.sys = PL_COLOR_SYSTEM_UNKNOWN;
	case (DXVA2_VideoTransferMatrix)6: frame.repr.sys = PL_COLOR_SYSTEM_UNKNOWN;	break; // missing
	case DXVA2_VideoTransferMatrix_BT601:	frame.repr.sys = PL_COLOR_SYSTEM_BT_601;	break;
	case DXVA2_VideoTransferMatrix_SMPTE240M:	frame.repr.sys = PL_COLOR_SYSTEM_SMPTE_240M;	break;
	case (DXVA2_VideoTransferMatrix)7: frame.repr.sys = PL_COLOR_SYSTEM_YCGCO;	break;
	case MFVideoTransferMatrix_BT2020_10:	frame.repr.sys = PL_COLOR_SYSTEM_BT_2020_NC;	break;
	default: break;
		//case MFVideoTransferMatrix_BT2020_10:           frame.repr.sys = PL_COLOR_SYSTEM_BT_2020_C;
		//case AVCOL_SPC_SMPTE2085:           frame.repr.sys = PL_COLOR_SYSTEM_UNKNOWN; // missing
		//case AVCOL_SPC_CHROMA_DERIVED_NCL:  frame.repr.sys = PL_COLOR_SYSTEM_UNKNOWN; // missing
		//case AVCOL_SPC_CHROMA_DERIVED_CL:   frame.repr.sys = PL_COLOR_SYSTEM_UNKNOWN; // missing
			// Note: this colorspace is confused between PQ and HLG, which libav*
			// requires inferring from other sources, but libplacebo makes explicit.
			// Default to PQ as it's the more common scenario.
		//case AVCOL_SPC_ICTCP:               frame.repr.sys = PL_COLOR_SYSTEM_BT_2100_PQ;
		//case AVCOL_SPC_NB:                  frame.repr.sys = PL_COLOR_SYSTEM_COUNT;
	}

	switch (m_srcExFmt.NominalRange)
	{
	case DXVA2_NominalRange_Unknown: frame.repr.levels = PL_COLOR_LEVELS_UNKNOWN; break;
	case DXVA2_NominalRange_16_235:	frame.repr.levels = PL_COLOR_LEVELS_LIMITED;	break;
	case DXVA2_NominalRange_0_255: frame.repr.levels = PL_COLOR_LEVELS_FULL;	break;
	default: break;
		//case AVCOL_RANGE_NB:                repr.levels = PL_COLOR_LEVELS_COUNT;
	}
	//when we will get a later version it will spawn to PL_ALPHA_NONE
	//Will need to check according to the dxgi format if we use PL_ALPHA_INDEPENDENT or PL_ALPHA_NONE in the future
	frame.repr.alpha = PL_ALPHA_UNKNOWN;

	//nv12 is 8 bits per color depth
	frame.repr.bits.color_depth = m_srcParams.CDepth;
}

void CVideoProcessor::ResizeProcessFrame()
{
	for (;;)
	{
		if (m_presentationQueue.size() > 0)
		{
			m_pFreePresentationQueue.push(m_presentationQueue.front());
			m_presentationQueue.pop();
		}
		else
			break;
	}
	for (auto& frame : m_pFreePresentationQueue)
	{
		frame.pTexture.Release();
		frame.pTexture.Create(m_destRect.Width(), m_destRect.Height(), D3D11_USAGE_DEFAULT | D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, DX::DeviceResources::Get()->GetBackBuffer().GetFormat());
		frame.pCurrentRect = m_destRect;
	};

}

void CVideoProcessor::ReleaseVP()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	CLog::Log(LOGINFO, "CVideoProcessor::ReleaseVP()");

	m_pFilter->ResetStreamingTimes2();
	m_RenderStats.Reset();

	if (m_pDeviceContext) {
		m_pDeviceContext->ClearState();
	}

	m_TexSrcVideo.Release();
	m_TexConvertOutput.Release();
	m_TexResize.Release();

	m_PSConvColorData.Release();
	m_pDoviCurvesConstantBuffer = nullptr;

	m_D3D11VP.ReleaseVideoProcessor();
	m_strCorrection = nullptr;

	m_srcParams = {};
	m_srcDXGIFormat = DXGI_FORMAT_UNKNOWN;
	m_pConvertFn = nullptr;
	m_srcWidth = 0;
	m_srcHeight = 0;
}

void CVideoProcessor::ReleaseDevice()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	CLog::Log(LOGINFO, "CVideoProcessor::ReleaseDevice()");

	ReleaseVP();
	m_D3D11VP.ReleaseVideoDevice();
	m_D3D11VP.ResetFrameOrder();

	m_bAlphaBitmapEnable = false;


	m_pPSCorrection = nullptr;
	m_pPSConvertColor = nullptr;
	m_pPSConvertColorDeint = nullptr;

	m_pVSimpleInputLayout = nullptr;
	m_pVS_Simple = nullptr;

	m_Alignment.texture.Release();
	m_Alignment.cformat = {};
	m_Alignment.cx = {};
	if (m_pDeviceContext)
		m_pDeviceContext->Flush();
	m_pDeviceContext = nullptr;

	m_bCallbackDeviceIsSet = false;
}


HRESULT CVideoProcessor::CreatePShaderFromResource(ID3D11PixelShader** ppPixelShader, std::string resid)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	if (!GetDevice || !ppPixelShader) {
		return E_POINTER;
	}
	CD3DDSPixelShader shdr;
	if (!shdr.LoadFromFile(resid))
		CLog::Log(LOGERROR, "{} failed loading the file", __FUNCTION__);


	LPVOID data;
	DWORD size;
	data = shdr.GetData();
	size = shdr.GetSize();

	return GetDevice->CreatePixelShader(data, size, nullptr, ppPixelShader);
}

void CVideoProcessor::UpdateTexParams(int cdepth)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	switch (MPC_SETTINGS->iTexFormat) {
	case TEXFMT_AUTOINT:
		m_InternalTexFmt = (cdepth > 8 || MPC_SETTINGS->bVPUseRTXVideoHDR) ? DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
		break;
	case TEXFMT_8INT:    m_InternalTexFmt = DXGI_FORMAT_B8G8R8A8_UNORM;     break;
	case TEXFMT_10INT:   m_InternalTexFmt = DXGI_FORMAT_R10G10B10A2_UNORM;  break;
	case TEXFMT_16FLOAT: m_InternalTexFmt = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
	default:
		ASSERT(FALSE);
	}
}

void CVideoProcessor::UpdateRenderRect()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	CLog::Log(LOGINFO, "{}", __FUNCTION__);


	if (m_videoRect.Width() == 0)
		assert(0);
	m_renderRect.IntersectRect(m_videoRect, m_windowRect);
}



HRESULT CVideoProcessor::MemCopyToTexSrcVideo(const BYTE* srcData, const int srcPitch)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	HRESULT hr = S_FALSE;
	D3D11_MAPPED_SUBRESOURCE mappedResource = {};

	if (m_TexSrcVideo.pTexture2) {
		hr = m_pDeviceContext->Map(m_TexSrcVideo.pTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (SUCCEEDED(hr)) {
			CopyFrameAsIs(m_srcHeight, (BYTE*)mappedResource.pData, mappedResource.RowPitch, srcData, srcPitch);
			m_pDeviceContext->Unmap(m_TexSrcVideo.pTexture.Get(), 0);

			hr = m_pDeviceContext->Map(m_TexSrcVideo.pTexture2.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			if (SUCCEEDED(hr)) {
				const UINT cromaH = m_srcHeight / m_srcParams.pDX11Planes->div_chroma_h;
				const int cromaPitch = (m_TexSrcVideo.pTexture3) ? srcPitch / m_srcParams.pDX11Planes->div_chroma_w : srcPitch;
				srcData += srcPitch * m_srcHeight;
				CopyFrameAsIs(cromaH, (BYTE*)mappedResource.pData, mappedResource.RowPitch, srcData, cromaPitch);
				m_pDeviceContext->Unmap(m_TexSrcVideo.pTexture2.Get(), 0);

				if (m_TexSrcVideo.pTexture3) {
					hr = m_pDeviceContext->Map(m_TexSrcVideo.pTexture3.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
					if (SUCCEEDED(hr)) {
						srcData += cromaPitch * cromaH;
						CopyFrameAsIs(cromaH, (BYTE*)mappedResource.pData, mappedResource.RowPitch, srcData, cromaPitch);
						m_pDeviceContext->Unmap(m_TexSrcVideo.pTexture3.Get(), 0);
					}
				}
			}
		}
	}
	else {
		hr = DX::DeviceResources::Get()->GetImmediateContext()->Map(m_TexSrcVideo.pTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (SUCCEEDED(hr)) {
			ASSERT(m_pConvertFn);
			const BYTE* src = (srcPitch < 0) ? srcData + srcPitch * (1 - (int)m_srcLines) : srcData;
			m_pConvertFn(m_srcLines, (BYTE*)mappedResource.pData, mappedResource.RowPitch, src, srcPitch);
			DX::DeviceResources::Get()->GetImmediateContext()->Unmap(m_TexSrcVideo.pTexture.Get(), 0);
		}
	}

	return hr;
}

HRESULT CVideoProcessor::SetShaderDoviCurvesPoly()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	ASSERT(m_Dovi.bValid);

	PS_DOVI_POLY_CURVE polyCurves[3] = {};

	const float scale = 1.0f / ((1 << m_Dovi.msd.Header.bl_bit_depth) - 1);
	const float scale_coef = 1.0f / (1u << m_Dovi.msd.Header.coef_log2_denom);

	for (int c = 0; c < 3; c++) {
		const auto& curve = m_Dovi.msd.Mapping.curves[c];
		auto& out = polyCurves[c];

		const int num_coef = curve.num_pivots - 1;
		bool has_poly = false;
		bool has_mmr = false;

		for (int i = 0; i < num_coef; i++) {
			switch (curve.mapping_idc[i]) {
			case 0: // polynomial
				has_poly = true;
				out.coeffs_data[i].x = scale_coef * curve.poly_coef[i][0];
				out.coeffs_data[i].y = (curve.poly_order[i] >= 1) ? scale_coef * curve.poly_coef[i][1] : 0.0f;
				out.coeffs_data[i].z = (curve.poly_order[i] >= 2) ? scale_coef * curve.poly_coef[i][2] : 0.0f;
				out.coeffs_data[i].w = 0.0f; // order=0 signals polynomial
				break;
			case 1: // mmr
				has_mmr = true;
				// not supported, leave as is
				out.coeffs_data[i].x = 0.0f;
				out.coeffs_data[i].y = 1.0f;
				out.coeffs_data[i].z = 0.0f;
				out.coeffs_data[i].w = 0.0f;
				break;
			}
		}

		const int n = curve.num_pivots - 2;
		for (int i = 0; i < n; i++) {
			out.pivots_data[i].x = scale * curve.pivots[i + 1];
		}
		for (int i = n; i < 7; i++) {
			out.pivots_data[i].x = 1e9f;
		}
	}

	HRESULT hr;

	if (m_pDoviCurvesConstantBuffer) {
		D3D11_MAPPED_SUBRESOURCE mr;
		hr = m_pDeviceContext->Map(m_pDoviCurvesConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mr);
		if (SUCCEEDED(hr)) {
			memcpy(mr.pData, &polyCurves, sizeof(polyCurves));
			m_pDeviceContext->Unmap(m_pDoviCurvesConstantBuffer.Get(), 0);
		}
	}
	else {
		D3D11_BUFFER_DESC BufferDesc = { sizeof(polyCurves), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
		D3D11_SUBRESOURCE_DATA InitData = { &polyCurves, 0, 0 };
		hr = GetDevice->CreateBuffer(&BufferDesc, &InitData, &m_pDoviCurvesConstantBuffer);
	}

	return hr;
}

HRESULT CVideoProcessor::SetShaderDoviCurves()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	ASSERT(m_Dovi.bValid);

	PS_DOVI_CURVE cbuffer[3] = {};

	for (int c = 0; c < 3; c++) {
		const auto& curve = m_Dovi.msd.Mapping.curves[c];
		auto& out = cbuffer[c];

		bool has_poly = false, has_mmr = false, mmr_single = true;
		uint32_t mmr_idx = 0, min_order = 3, max_order = 1;

		const float scale_coef = 1.0f / (1 << m_Dovi.msd.Header.coef_log2_denom);
		const int num_coef = curve.num_pivots - 1;
		for (int i = 0; i < num_coef; i++) {
			switch (curve.mapping_idc[i]) {
			case 0: // polynomial
				has_poly = true;
				out.coeffs_data[i].x = scale_coef * curve.poly_coef[i][0];
				out.coeffs_data[i].y = (curve.poly_order[i] >= 1) ? scale_coef * curve.poly_coef[i][1] : 0.0f;
				out.coeffs_data[i].z = (curve.poly_order[i] >= 2) ? scale_coef * curve.poly_coef[i][2] : 0.0f;
				out.coeffs_data[i].w = 0.0f; // order=0 signals polynomial
				break;
			case 1: // mmr
				min_order = std::min<int>(min_order, curve.mmr_order[i]);
				max_order = std::max<int>(max_order, curve.mmr_order[i]);
				mmr_single = !has_mmr;
				has_mmr = true;
				out.coeffs_data[i].x = scale_coef * curve.mmr_constant[i];
				out.coeffs_data[i].y = static_cast<float>(mmr_idx);
				out.coeffs_data[i].w = static_cast<float>(curve.mmr_order[i]);
				for (int j = 0; j < curve.mmr_order[i]; j++) {
					// store weights per order as two packed float4s
					out.mmr_data[mmr_idx].x = scale_coef * curve.mmr_coef[i][j][0];
					out.mmr_data[mmr_idx].y = scale_coef * curve.mmr_coef[i][j][1];
					out.mmr_data[mmr_idx].z = scale_coef * curve.mmr_coef[i][j][2];
					out.mmr_data[mmr_idx].w = 0.0f; // unused
					mmr_idx++;
					out.mmr_data[mmr_idx].x = scale_coef * curve.mmr_coef[i][j][3];
					out.mmr_data[mmr_idx].y = scale_coef * curve.mmr_coef[i][j][4];
					out.mmr_data[mmr_idx].z = scale_coef * curve.mmr_coef[i][j][5];
					out.mmr_data[mmr_idx].w = scale_coef * curve.mmr_coef[i][j][6];
					mmr_idx++;
				}
				break;
			}
		}

		const float scale = 1.0f / ((1 << m_Dovi.msd.Header.bl_bit_depth) - 1);
		const int n = curve.num_pivots - 2;
		for (int i = 0; i < n; i++) {
			out.pivots_data[i].x = scale * curve.pivots[i + 1];
		}
		for (int i = n; i < 7; i++) {
			out.pivots_data[i].x = 1e9f;
		}

		if (has_poly) {
			out.params.methods = PS_RESHAPE_POLY;
		}
		if (has_mmr) {
			out.params.methods |= PS_RESHAPE_MMR;
			out.params.mmr_single = mmr_single;
			out.params.min_order = min_order;
			out.params.max_order = max_order;
		}
	}

	HRESULT hr;

	if (m_pDoviCurvesConstantBuffer) {
		D3D11_MAPPED_SUBRESOURCE mr;
		hr = m_pDeviceContext->Map(m_pDoviCurvesConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mr);
		if (SUCCEEDED(hr)) {
			memcpy(mr.pData, &cbuffer, sizeof(cbuffer));
			m_pDeviceContext->Unmap(m_pDoviCurvesConstantBuffer.Get(), 0);
		}
	}
	else {
		D3D11_BUFFER_DESC BufferDesc = { sizeof(cbuffer), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
		D3D11_SUBRESOURCE_DATA InitData = { &cbuffer, 0, 0 };
		hr = GetDevice->CreateBuffer(&BufferDesc, &InitData, &m_pDoviCurvesConstantBuffer);
	}

	return hr;
}

HRESULT CVideoProcessor::UpdateConvertColorShader()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	m_pPSConvertColor = nullptr;
	m_pPSConvertColorDeint = nullptr;
	ID3DBlob* pShaderCode = nullptr;

	int convertType = (MPC_SETTINGS->bConvertToSdr && !(m_bHdrPassthroughSupport && MPC_SETTINGS->bHdrPassthrough)) ? SHADER_CONVERT_TO_SDR
		: (m_bHdrPassthroughSupport && MPC_SETTINGS->bHdrPassthrough && m_srcExFmt.VideoTransferFunction == MFVideoTransFunc_HLG) ? SHADER_CONVERT_TO_PQ
		: SHADER_CONVERT_NONE;

	MediaSideDataDOVIMetadata* pDOVIMetadata = m_Dovi.bValid ? &m_Dovi.msd : nullptr;

	HRESULT hr = GetShaderConvertColor(true,
		m_srcWidth,
		m_TexSrcVideo.desc.Width, m_TexSrcVideo.desc.Height,
		m_srcRect, m_srcParams, m_srcExFmt, pDOVIMetadata,
		MPC_SETTINGS->iChromaScaling, convertType, false,
		&pShaderCode);
	if (S_OK == hr) {
		hr = GetDevice->CreatePixelShader(pShaderCode->GetBufferPointer(), pShaderCode->GetBufferSize(), nullptr, &m_pPSConvertColor);
		pShaderCode->Release();
	}

	if (m_bInterlaced && m_srcParams.Subsampling == 420 && m_srcParams.pDX11Planes) {
		hr = GetShaderConvertColor(true,
			m_srcWidth,
			m_TexSrcVideo.desc.Width, m_TexSrcVideo.desc.Height,
			m_srcRect, m_srcParams, m_srcExFmt, pDOVIMetadata,
			MPC_SETTINGS->iChromaScaling, convertType, true,
			&pShaderCode);
		if (S_OK == hr) {
			hr = GetDevice->CreatePixelShader(pShaderCode->GetBufferPointer(), pShaderCode->GetBufferSize(), nullptr, &m_pPSConvertColorDeint);
			pShaderCode->Release();
		}
	}

	if (FAILED(hr)) {
		ASSERT(0);
		std::string resid = 0;
		if (m_srcParams.cformat == CF_YUY2) {
			resid = IDF_PS_11_CONVERT_YUY2;
		}
		else if (m_srcParams.pDX11Planes) {
			if (m_srcParams.pDX11Planes->FmtPlane3) {
				if (m_srcParams.cformat == CF_YV12 || m_srcParams.cformat == CF_YV16 || m_srcParams.cformat == CF_YV24) {
					resid = IDF_PS_11_CONVERT_PLANAR_YV;
				}
				else {
					resid = IDF_PS_11_CONVERT_PLANAR;
				}
			}
			else {
				resid = IDF_PS_11_CONVERT_BIPLANAR;
			}
		}
		else {
			resid = IDF_PS_11_CONVERT_COLOR;
		}
		EXECUTE_ASSERT(S_OK == CreatePShaderFromResource(&m_pPSConvertColor, resid));

		return S_FALSE;
	}

	return hr;
}

HRESULT CVideoProcessor::ConvertColorPass(ID3D11Texture2D* pRenderTarget)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRenderTargetView;

	HRESULT hr = DX::DeviceResources::Get()->GetD3DDevice()->CreateRenderTargetView(pRenderTarget, nullptr, &pRenderTargetView);
	if (FAILED(hr)) {
		return hr;
	}

	D3D11_VIEWPORT VP;
	VP.TopLeftX = 0;
	VP.TopLeftY = 0;
	VP.Width = (FLOAT)m_TexConvertOutput.desc.Width;
	VP.Height = (FLOAT)m_TexConvertOutput.desc.Height;
	VP.MinDepth = 0.0f;
	VP.MaxDepth = 1.0f;

	const UINT Stride = sizeof(VERTEX);
	const UINT Offset = 0;

	// Set resources
	m_pDeviceContext.Get()->IASetInputLayout(m_pVSimpleInputLayout.Get());
	m_pDeviceContext.Get()->OMSetRenderTargets(1, pRenderTargetView.GetAddressOf(), nullptr);
	m_pDeviceContext.Get()->RSSetViewports(1, &VP);
	m_pDeviceContext.Get()->OMSetBlendState(nullptr, nullptr, D3D11_DEFAULT_SAMPLE_MASK);
	m_pDeviceContext.Get()->VSSetShader(m_pVS_Simple.Get(), nullptr, 0);
	if (MPC_SETTINGS->bDeintBlend && m_SampleFormat != D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE && m_pPSConvertColorDeint) {
		m_pDeviceContext.Get()->PSSetShader(m_pPSConvertColorDeint.Get(), nullptr, 0);
	}
	else {
		m_pDeviceContext.Get()->PSSetShader(m_pPSConvertColor.Get(), nullptr, 0);
	}
	m_pDeviceContext.Get()->PSSetShaderResources(0, 1, m_TexSrcVideo.pShaderResource.GetAddressOf());
	m_pDeviceContext.Get()->PSSetShaderResources(1, 1, m_TexSrcVideo.pShaderResource2.GetAddressOf());
	m_pDeviceContext.Get()->PSSetShaderResources(2, 1, m_TexSrcVideo.pShaderResource3.GetAddressOf());

	m_pDeviceContext.Get()->PSSetSamplers(0, 1, m_pSamplerPoint.GetAddressOf());
	m_pDeviceContext.Get()->PSSetSamplers(1, 1, m_pSamplerLinear.GetAddressOf());

	m_pDeviceContext.Get()->PSSetConstantBuffers(0, 1, &m_PSConvColorData.pConstants);
	m_pDeviceContext.Get()->PSSetConstantBuffers(1, 1, m_pCorrectionConstants.GetAddressOf());
	m_pDeviceContext.Get()->PSSetConstantBuffers(2, 1, m_pDoviCurvesConstantBuffer.GetAddressOf());

	m_pDeviceContext.Get()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	m_pDeviceContext.Get()->IASetVertexBuffers(0, 1, &m_PSConvColorData.pVertexBuffer, &Stride, &Offset);

	// Draw textured quad onto render target
	m_pDeviceContext.Get()->Draw(4, 0);

	ID3D11ShaderResourceView* views[3] = {};
	m_pDeviceContext.Get()->PSSetShaderResources(0, 3, views);

	return hr;
}
void CVideoProcessor::SetShaderConvertColorParams()
{
	mp_cmat cmatrix;

	if (m_Dovi.bValid) {
		const float brightness = DXVA2FixedToFloat(m_DXVA2ProcAmpValues.Brightness) / 255;
		const float contrast = DXVA2FixedToFloat(m_DXVA2ProcAmpValues.Contrast);

		for (int i = 0; i < 3; i++) {
			cmatrix.m[i][0] = (float)m_Dovi.msd.ColorMetadata.ycc_to_rgb_matrix[i * 3 + 0] * contrast;
			cmatrix.m[i][1] = (float)m_Dovi.msd.ColorMetadata.ycc_to_rgb_matrix[i * 3 + 1] * contrast;
			cmatrix.m[i][2] = (float)m_Dovi.msd.ColorMetadata.ycc_to_rgb_matrix[i * 3 + 2] * contrast;
		}

		for (int i = 0; i < 3; i++) {
			cmatrix.c[i] = brightness;
			for (int j = 0; j < 3; j++) {
				cmatrix.c[i] -= cmatrix.m[i][j] * m_Dovi.msd.ColorMetadata.ycc_to_rgb_offset[j];
			}
		}

		m_PSConvColorData.bEnable = true;
	}
	else {
		mp_csp_params csp_params;
		set_colorspace(m_srcExFmt, csp_params.color);
		csp_params.brightness = DXVA2FixedToFloat(m_DXVA2ProcAmpValues.Brightness) / 255;
		csp_params.contrast = DXVA2FixedToFloat(m_DXVA2ProcAmpValues.Contrast);
		csp_params.hue = DXVA2FixedToFloat(m_DXVA2ProcAmpValues.Hue) / 180 * acos(-1);
		csp_params.saturation = DXVA2FixedToFloat(m_DXVA2ProcAmpValues.Saturation);
		csp_params.gray = m_srcParams.CSType == CS_GRAY;

		csp_params.input_bits = csp_params.texture_bits = m_srcParams.CDepth;

		mp_get_csp_matrix(&csp_params, &cmatrix);

		m_PSConvColorData.bEnable =
			m_srcParams.CSType == CS_YUV ||
			m_srcParams.cformat == CF_GBRP8 || m_srcParams.cformat == CF_GBRP16 ||
			csp_params.gray ||
			fabs(csp_params.brightness) > 1e-4f || fabs(csp_params.contrast - 1.0f) > 1e-4f;
	}

	PS_COLOR_TRANSFORM cbuffer = {
		{cmatrix.m[0][0], cmatrix.m[0][1], cmatrix.m[0][2], 0},
		{cmatrix.m[1][0], cmatrix.m[1][1], cmatrix.m[1][2], 0},
		{cmatrix.m[2][0], cmatrix.m[2][1], cmatrix.m[2][2], 0},
		{cmatrix.c[0],    cmatrix.c[1],    cmatrix.c[2],    0},
	};

	if (m_srcParams.cformat == CF_GBRP8 || m_srcParams.cformat == CF_GBRP16) {
		std::swap(cbuffer.cm_r.x, cbuffer.cm_r.y); std::swap(cbuffer.cm_r.y, cbuffer.cm_r.z);
		std::swap(cbuffer.cm_g.x, cbuffer.cm_g.y); std::swap(cbuffer.cm_g.y, cbuffer.cm_g.z);
		std::swap(cbuffer.cm_b.x, cbuffer.cm_b.y); std::swap(cbuffer.cm_b.y, cbuffer.cm_b.z);
	}
	else if (m_srcParams.CSType == CS_GRAY) {
		cbuffer.cm_g.x = cbuffer.cm_g.y;
		cbuffer.cm_g.y = 0;
		cbuffer.cm_b.x = cbuffer.cm_b.z;
		cbuffer.cm_b.z = 0;
	}

	SAFE_RELEASE(m_PSConvColorData.pConstants);

	D3D11_BUFFER_DESC BufferDesc = {};
	BufferDesc.Usage = D3D11_USAGE_DEFAULT;
	BufferDesc.ByteWidth = sizeof(cbuffer);
	BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	BufferDesc.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData = { &cbuffer, 0, 0 };
	EXECUTE_ASSERT(S_OK == DX::DeviceResources::Get()->GetD3DDevice()->CreateBuffer(&BufferDesc, &InitData, &m_PSConvColorData.pConstants));
}

void CVideoProcessor::SetShaderLuminanceParams()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	//todo add this param
	int m_iSDRDisplayNits = 125;
	FLOAT cbuffer[4] = { 10000.0f / m_iSDRDisplayNits, 0, 0, 0 };

	if (m_pCorrectionConstants) {
		m_pDeviceContext->UpdateSubresource(m_pCorrectionConstants.Get(), 0, nullptr, &cbuffer, 0, 0);
	}
	else {
		D3D11_BUFFER_DESC BufferDesc = {};
		BufferDesc.ByteWidth = sizeof(cbuffer);
		BufferDesc.Usage = D3D11_USAGE_DEFAULT;
		BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		D3D11_SUBRESOURCE_DATA InitData = { &cbuffer, 0, 0 };
		EXECUTE_ASSERT(S_OK == GetDevice->CreateBuffer(&BufferDesc, &InitData, &m_pCorrectionConstants));
	}
}

void CVideoProcessor::render_info_cb(void* priv, const pl_render_info* info)
{
	PL::CPlHelper* p = (PL::CPlHelper*)priv;

	switch (info->stage) {
	case PL_RENDER_STAGE_FRAME:
		if (info->index >= MAX_FRAME_PASSES)
			return;
		p->num_frame_passes = info->index + 1;
		pl_dispatch_info_move(&p->frame_info[info->index], info->pass);
		return;

	case PL_RENDER_STAGE_BLEND:
		if (info->index >= MAX_BLEND_PASSES || info->count >= MAX_BLEND_FRAMES)
			return;
		p->num_blend_passes[info->count] = info->index + 1;
		pl_dispatch_info_move(&p->blend_info[info->count][info->index], info->pass);
		return;

	case PL_RENDER_STAGE_COUNT:
		break;
	}
	CLog::Log(LOGERROR, "");
}

HRESULT CVideoProcessor::SetDevice(ID3D11Device1* pDevice, const bool bDecoderDevice)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	HRESULT hr = S_OK;

	CLog::LogF(LOGINFO, "CVideoProcessor::SetDevice()");

	//reset everything that is not from kodi
	ReleaseDevice();

	CheckPointer(pDevice, E_POINTER);

	hr = pDevice->CreateDeferredContext1(0, &m_pDeviceContext);

	D3D11_SAMPLER_DESC SampDesc = {};
	SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SampDesc.MinLOD = 0;
	SampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	EXECUTE_ASSERT(S_OK == DX::DeviceResources::Get()->GetD3DDevice()->CreateSamplerState(&SampDesc, &m_pSamplerPoint));

	SampDesc.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT; // linear interpolation for magnification
	EXECUTE_ASSERT(S_OK == DX::DeviceResources::Get()->GetD3DDevice()->CreateSamplerState(&SampDesc, &m_pSamplerLinear));

	if (FAILED(hr))
	{
		CLog::LogF(LOGERROR, "{} CreateDeferredContext1 failed", __FUNCTION__);
		return hr;
	}
	CLog::LogF(LOGINFO, "{} CreateDeferredContext1 succeeded", __FUNCTION__);
	DXGI_SWAP_CHAIN_DESC1 desc;
	GetSwapChain->GetDesc1(&desc);

	// for d3d11 subtitles
	//Com::SComQIPtr<ID3D10Multithread> pMultithread(m_pDeviceContext.Get());
	//pMultithread->SetMultithreadProtected(TRUE);


	DXGI_ADAPTER_DESC dxgiAdapterDesc = {};
	hr = DX::DeviceResources::Get()->GetAdapter()->GetDesc(&dxgiAdapterDesc);
	if (SUCCEEDED(hr)) {
		m_VendorId = dxgiAdapterDesc.VendorId;
		m_strAdapterDescription.Format(L"%s (%u:%u)", dxgiAdapterDesc.Description, dxgiAdapterDesc.VendorId, dxgiAdapterDesc.DeviceId);
		CLog::LogF(LOGINFO, "Graphics DXGI adapter: {}", WToA(m_strAdapterDescription).c_str());
	}

	HRESULT hr2 = m_D3D11VP.InitVideoDevice(pDevice, m_pDeviceContext.Get(), m_VendorId);
	DLogIf(FAILED(hr2), "CVideoProcessor::SetDevice() : InitVideoDevice failed with error %s", WToA(HR2Str(hr2)));

	LPVOID data;
	DWORD size;
	EXECUTE_ASSERT(S_OK == GetDataFromResource(data, size, IDF_VS_11_SIMPLE));
	EXECUTE_ASSERT(S_OK == pDevice->CreateVertexShader(data, size, nullptr, &m_pVS_Simple));

	D3D11_INPUT_ELEMENT_DESC Layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	EXECUTE_ASSERT(S_OK == pDevice->CreateInputLayout(Layout, std::size(Layout), data, size, m_pVSimpleInputLayout.ReleaseAndGetAddressOf()));

	if (m_pFilter->m_inputMT.IsValid()) {
		if (!InitMediaType(&m_pFilter->m_inputMT)) {
			CLog::LogF(LOGERROR, "{} m_inputMT not valid", __FUNCTION__);
			ReleaseDevice();
			return E_FAIL;
		}
	}

	if (m_hWnd) {
		hr = InitSwapChain();
		if (FAILED(hr)) {
			ReleaseDevice();
			return hr;
		}
	}

	//set the callback for subtitles
	SetCallbackDevice();

	SetStereo3dTransform(m_iStereo3dTransform);

	m_bDecoderDevice = bDecoderDevice;
	//UpdateStatsStatic();

	return hr;

}

HRESULT CVideoProcessor::InitSwapChain()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	CLog::LogF(LOGINFO, "{} ", __FUNCTION__);

	const auto bHdrOutput = MPC_SETTINGS->bHdrPassthrough && m_bHdrPassthroughSupport && (SourceIsHDR() || MPC_SETTINGS->bVPUseRTXVideoHDR);
	m_currentSwapChainColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
	DXGI_SWAP_CHAIN_DESC1 desc = { 0 };
	GetSwapChain->GetDesc1(&desc);
	m_SwapChainFmt = desc.Format;
	return S_OK;
}

BOOL CVideoProcessor::VerifyMediaType(const CMediaType* pmt)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	const auto& FmtParams = GetFmtConvParams(pmt);
	m_pFinalTextureSampler = MPC_SETTINGS->bD3D11TextureSampler;
	if (MPC_SETTINGS->bD3D11TextureSampler == D3D11_LIBPLACEBO)
	{
		if (FmtParams.cformat == CF_NV12 || FmtParams.cformat == CF_YV12)
		{
			m_pFinalTextureSampler = D3D11_LIBPLACEBO;
			return TRUE;
		}
		CLog::Log(LOGERROR, "{}libplacebo was selected for texture sample but format not supported yet falling back to internal shaders", __FUNCTION__);
		m_pFinalTextureSampler = D3D11_INTERNAL_SHADERS;

	}

	if (FmtParams.VP11Format == DXGI_FORMAT_UNKNOWN && FmtParams.DX11Format == DXGI_FORMAT_UNKNOWN) {
		return FALSE;
	}

	const BITMAPINFOHEADER* pBIH = GetBIHfromVIHs(pmt);
	if (!pBIH) {
		return FALSE;
	}

	if (pBIH->biWidth <= 0 || !pBIH->biHeight || (!pBIH->biSizeImage && pBIH->biCompression != BI_RGB)) {
		return FALSE;
	}

	if (FmtParams.Subsampling == 420 && ((pBIH->biWidth & 1) || (pBIH->biHeight & 1))) {
		return FALSE;
	}
	if (FmtParams.Subsampling == 422 && (pBIH->biWidth & 1)) {
		return FALSE;
	}

	return TRUE;
}

void CVideoProcessor::UpdateStatsInputFmt()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	m_strStatsInputFmt.assign(L"\nInput format  : ");

	if (m_iSrcFromGPU == 11) {
		m_strStatsInputFmt.append(L"D3D11_");
	}
	m_strStatsInputFmt.append(m_srcParams.str);

	if (m_srcWidth != m_srcRectWidth || m_srcHeight != m_srcRectHeight) {
		m_strStatsInputFmt.append(StringUtils::Format(L" {}x{} ->", m_srcWidth, m_srcHeight));
	}
	m_strStatsInputFmt.append(StringUtils::Format(L" {}x{}", m_srcRectWidth, m_srcRectHeight));
	if (m_srcAnamorphic) {
		m_strStatsInputFmt.append(StringUtils::Format(L" ({}:{})", m_srcAspectRatioX, m_srcAspectRatioY));
	}

	if (m_srcParams.CSType == CS_YUV) {
		if (m_Dovi.bValid)
		{

			//TODO add profile
			int dv_profile = 0;
			const auto& hdr = m_Dovi.msd.Header;
			const bool has_el = hdr.el_spatial_resampling_filter_flag && !hdr.disable_residual_flag;

			if ((hdr.vdr_rpu_profile == 0) && hdr.bl_video_full_range_flag) {
				dv_profile = 5;
			}
			else if (has_el) {
				// Profile 7 is max 12 bits, both MEL & FEL
				if (hdr.vdr_bit_depth == 12) {
					dv_profile = 7;
				}
				else {
					dv_profile = 4;
				}
			}
			else {
				//dv_profile = 8;
			}
			if (dv_profile > 0)
				m_strStatsInputFmt.append(StringUtils::Format(L" HDR DolbyVision"));
		}
		else
		{
			LPCSTR strs[6] = {};
			GetExtendedFormatString(strs, m_srcExFmt, m_srcParams.CSType);
			m_strStatsInputFmt.append(StringUtils::Format(L"\n  Range: {}", AToW(strs[1])));
			if (m_decExFmt.NominalRange == DXVA2_NominalRange_Unknown) {
				m_strStatsInputFmt += L'*';
			};
			m_strStatsInputFmt.append(StringUtils::Format(L", Matrix: {}", AToW(strs[2])));
			if (m_decExFmt.VideoTransferMatrix == DXVA2_VideoTransferMatrix_Unknown) {
				m_strStatsInputFmt += L'*';
			};
			if (m_decExFmt.VideoLighting != DXVA2_VideoLighting_Unknown) {
				// display Lighting only for values other than Unknown, but this never happens
				m_strStatsInputFmt.append(StringUtils::Format(L", Lighting: {}", AToW(strs[3])));
			};
			m_strStatsInputFmt.append(StringUtils::Format(L"\n  Primaries: {}", AToW(strs[4])));
			if (m_decExFmt.VideoPrimaries == DXVA2_VideoPrimaries_Unknown) {
				m_strStatsInputFmt += L'*';
			};
			m_strStatsInputFmt.append(StringUtils::Format(L", Function: {}", AToW(strs[5])));
			if (m_decExFmt.VideoTransferFunction == DXVA2_VideoTransFunc_Unknown) {
				m_strStatsInputFmt += L'*';
			};
			if (m_srcParams.Subsampling == 420) {
				m_strStatsInputFmt.append(StringUtils::Format(L"\n  ChromaLocation: {}", AToW(strs[0])));
				if (m_decExFmt.VideoChromaSubsampling == DXVA2_VideoChromaSubsampling_Unknown) {
					m_strStatsInputFmt += L'*';
				};
			}
		}
	}
}

BOOL CVideoProcessor::InitMediaType(const CMediaType* pmt)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	CLog::LogF(LOGINFO, "CVideoProcessor::InitMediaType()");

	if (!VerifyMediaType(pmt)) {
		return FALSE;
	}

	ReleaseVP();

	auto FmtParams = GetFmtConvParams(pmt);

	m_srcParamsLibplacebo = GetFmtConvParamsLibplacebo(FmtParams.cformat);

	const BITMAPINFOHEADER* pBIH = nullptr;
	m_decExFmt.value = 0;

	if (pmt->formattype == FORMAT_VideoInfo2) {
		const VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*)pmt->pbFormat;
		pBIH = &vih2->bmiHeader;
		m_srcRect = vih2->rcSource;
		m_srcAspectRatioX = vih2->dwPictAspectRatioX;
		m_srcAspectRatioY = vih2->dwPictAspectRatioY;
		if (FmtParams.CSType == CS_YUV && (vih2->dwControlFlags & (AMCONTROL_USED | AMCONTROL_COLORINFO_PRESENT))) {
			m_decExFmt.value = vih2->dwControlFlags;
			m_decExFmt.SampleFormat = AMCONTROL_USED | AMCONTROL_COLORINFO_PRESENT; // ignore other flags
		}
		m_bInterlaced = (vih2->dwInterlaceFlags & AMINTERLACE_IsInterlaced);
		m_rtAvgTimePerFrame = vih2->AvgTimePerFrame;
	}
	else if (pmt->formattype == FORMAT_VideoInfo) {
		const VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;
		pBIH = &vih->bmiHeader;
		m_srcRect = vih->rcSource;
		m_srcAspectRatioX = 0;
		m_srcAspectRatioY = 0;
		m_bInterlaced = 0;
		m_rtAvgTimePerFrame = vih->AvgTimePerFrame;
	}
	else {
		return FALSE;
	}

	m_pFilter->m_FrameStats.SetStartFrameDuration(m_rtAvgTimePerFrame);
	m_pFilter->m_bValidBuffer = false;

	UINT biWidth = pBIH->biWidth;
	UINT biHeight = labs(pBIH->biHeight);
	UINT biSizeImage = pBIH->biSizeImage;
	if (pBIH->biSizeImage == 0 && pBIH->biCompression == BI_RGB) { // biSizeImage may be zero for BI_RGB bitmaps
		biSizeImage = biWidth * biHeight * pBIH->biBitCount / 8;
	}

	m_srcLines = biHeight * FmtParams.PitchCoeff / 2;
	m_srcPitch = biWidth * FmtParams.Packsize;
	switch (FmtParams.cformat) {
	case CF_Y8:
	case CF_NV12:
	case CF_RGB24:
	case CF_BGR48:
		m_srcPitch = ALIGN(m_srcPitch, 4);
		break;
	}
	if (pBIH->biCompression == BI_RGB && pBIH->biHeight > 0) {
		m_srcPitch = -m_srcPitch;
	}

	UINT origW = biWidth;
	UINT origH = biHeight;
	if (pmt->FormatLength() == 112 + sizeof(VR_Extradata)) {
		const VR_Extradata* vrextra = reinterpret_cast<VR_Extradata*>(pmt->pbFormat + 112);
		if (vrextra->QueryWidth == pBIH->biWidth && vrextra->QueryHeight == pBIH->biHeight && vrextra->Compression == pBIH->biCompression) {
			origW = vrextra->FrameWidth;
			origH = abs(vrextra->FrameHeight);
		}
	}

	if (m_srcRect.IsRectNull()) {
		m_srcRect.SetRect(0, 0, origW, origH);
	}
	m_srcRectWidth = m_srcRect.Width();
	m_srcRectHeight = m_srcRect.Height();

	m_srcExFmt = SpecifyExtendedFormat(m_decExFmt, FmtParams, m_srcRectWidth, m_srcRectHeight);


	bool disableD3D11VP = (m_pFinalTextureSampler != D3D11_VP);

	if (m_srcExFmt.VideoTransferMatrix == VIDEOTRANSFERMATRIX_YCgCo || m_Dovi.bValid) {
		disableD3D11VP = true;
	}
	if (FmtParams.CSType == CS_RGB && m_VendorId == PCIV_NVIDIA) {
		// D3D11 VP does not work correctly if RGB32 with odd frame width (source or target) on Nvidia adapters
		disableD3D11VP = true;
	}
	if (disableD3D11VP) {
		FmtParams.VP11Format = DXGI_FORMAT_UNKNOWN;
	}

	const auto frm_gcd = std::gcd(m_srcRectWidth, m_srcRectHeight);
	const auto srcFrameARX = m_srcRectWidth / frm_gcd;
	const auto srcFrameARY = m_srcRectHeight / frm_gcd;

	if (!m_srcAspectRatioX || !m_srcAspectRatioY) {
		m_srcAspectRatioX = srcFrameARX;
		m_srcAspectRatioY = srcFrameARY;
		m_srcAnamorphic = false;
	}
	else {
		const auto ar_gcd = std::gcd(m_srcAspectRatioX, m_srcAspectRatioY);
		m_srcAspectRatioX /= ar_gcd;
		m_srcAspectRatioY /= ar_gcd;
		m_srcAnamorphic = (srcFrameARX != m_srcAspectRatioX || srcFrameARY != m_srcAspectRatioY);
	}

	m_pPSCorrection = nullptr;
	m_pPSConvertColor = nullptr;
	m_pPSConvertColorDeint = nullptr;
	m_PSConvColorData.bEnable = false;

	UpdateTexParams(FmtParams.CDepth);

	if (Preferred10BitOutput() && m_SwapChainFmt == DXGI_FORMAT_B8G8R8A8_UNORM) {
		Init(m_hWnd);
	}

	m_srcVideoTransferFunction = m_srcExFmt.VideoTransferFunction;

	HRESULT hr = E_NOT_VALID_STATE;
	DX::DeviceResources().Get()->SetMultithreadProtected(true);
	// D3D11 Video Processor
	if (FmtParams.VP11Format != DXGI_FORMAT_UNKNOWN) {
		hr = InitializeD3D11VP(FmtParams, origW, origH);
		if (SUCCEEDED(hr)) {
			std::string resId = "";
			bool bTransFunc22 = m_srcExFmt.VideoTransferFunction == DXVA2_VideoTransFunc_22
				|| m_srcExFmt.VideoTransferFunction == DXVA2_VideoTransFunc_709
				|| m_srcExFmt.VideoTransferFunction == DXVA2_VideoTransFunc_240M;

			if (m_srcExFmt.VideoTransferFunction == MFVideoTransFunc_2084 && !(m_bHdrPassthroughSupport && MPC_SETTINGS->bHdrPassthrough) && MPC_SETTINGS->bConvertToSdr) {
				resId = m_D3D11VP.IsPqSupported() ? IDF_PS_11_CONVERT_PQ_TO_SDR : IDF_PS_11_FIXCONVERT_PQ_TO_SDR;
				m_strCorrection = L"PQ to SDR";
			}
			else if (m_srcExFmt.VideoTransferFunction == MFVideoTransFunc_HLG) {
				if (m_bHdrPassthroughSupport && MPC_SETTINGS->bHdrPassthrough) {
					resId = IDF_PS_11_CONVERT_HLG_TO_PQ;
					m_strCorrection = L"HLG to PQ";
				}
				else if (MPC_SETTINGS->bConvertToSdr) {
					resId = IDF_PS_11_FIXCONVERT_HLG_TO_SDR;
					m_strCorrection = L"HLG to SDR";
				}
				else if (m_srcExFmt.VideoPrimaries == MFVideoPrimaries_BT2020) {
					// HLG compatible with SDR
					resId = IDF_PS_11_FIX_BT2020;
					m_strCorrection = L"Fix BT.2020";
				}
			}
			else if (bTransFunc22 && m_srcExFmt.VideoPrimaries == MFVideoPrimaries_BT2020) {
				resId = IDF_PS_11_FIX_BT2020;
				m_strCorrection = L"Fix BT.2020";
			}

			if (resId.length() > 0) {
				EXECUTE_ASSERT(S_OK == CreatePShaderFromResource(&m_pPSCorrection, resId));
				DLogIf(m_pPSCorrection, "CVideoProcessor::InitMediaType() m_pPSCorrection('{}') created", WToA(m_strCorrection));
			}
		}
		else {
			ReleaseVP();
		}
	}

	// Tex Video Processor
	if (FAILED(hr) && FmtParams.DX11Format != DXGI_FORMAT_UNKNOWN) {
		MPC_SETTINGS->bVPUseRTXVideoHDR = false;
		hr = InitializeTexVP(FmtParams, origW, origH);
		if (SUCCEEDED(hr)) {
			SetShaderConvertColorParams();
			SetShaderLuminanceParams();
		}
	}
	CMPCVRRenderer::Get()->GetPlHelper()->Init(FmtParams.DX11Format);
	if (m_pFinalTextureSampler == D3D11_LIBPLACEBO)
	{
		m_srcWidth = origW;
		m_srcHeight = origH;
		m_srcParams = FmtParams;
		hr = S_OK;
	}
	if (SUCCEEDED(hr)) {
		UpdateTexures();
		UpdateStatsStatic();

		m_pFilter->m_inputMT = *pmt;

		return TRUE;
	}

	return FALSE;
}

HRESULT CVideoProcessor::InitializeD3D11VP(const FmtConvParams_t& params, const UINT width, const UINT height)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	if (!m_D3D11VP.IsVideoDeviceOk()) {
		return E_ABORT;
	}

	const auto& dxgiFormat = params.VP11Format;

	CLog::LogF(LOGINFO, "CVideoProcessor::InitializeD3D11VP() started with input surface: {}, {} x {}", WToA(DXGIFormatToString(dxgiFormat)).c_str(), width, height);

	m_TexSrcVideo.Release();

	const bool bHdrPassthrough = m_bHdrDisplayModeEnabled && (SourceIsPQorHLG() || (MPC_SETTINGS->bVPUseRTXVideoHDR && params.CDepth == 8));
	m_D3D11OutputFmt = m_InternalTexFmt;
	HRESULT hr = m_D3D11VP.InitVideoProcessor(dxgiFormat, width, height, m_srcExFmt, m_bInterlaced, bHdrPassthrough, m_D3D11OutputFmt);
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO, "CVideoProcessor::InitializeD3D11VP() : InitVideoProcessor() failed with error {}", WToA(HR2Str(hr)).c_str());
		return hr;
	}

	hr = m_D3D11VP.InitInputTextures(GetDevice);
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO, "CVideoProcessor::InitializeD3D11VP() : InitInputTextures() failed with error {}", WToA(HR2Str(hr)).c_str());
		return hr;
	}
	/*
	0 disabled
	1 sd
	2 720p
	3 1080p
	4 1440p
	*/
	auto superRes = (MPC_SETTINGS->bVPScaling && !(m_bHdrPassthroughSupport && MPC_SETTINGS->bHdrPassthrough && SourceIsHDR())) ? MPC_SETTINGS->iVPUseSuperRes : SUPERRES_Disable;
	m_bVPUseSuperRes = (m_D3D11VP.SetSuperRes(superRes) == S_OK);

	auto rtxHDR = MPC_SETTINGS->bVPUseRTXVideoHDR && m_bHdrPassthroughSupport && MPC_SETTINGS->bHdrPassthrough && MPC_SETTINGS->iTexFormat != TEXFMT_8INT && !SourceIsHDR();
	MPC_SETTINGS->bVPUseRTXVideoHDR = (m_D3D11VP.SetRTXVideoHDR(rtxHDR) == S_OK);

	if ((MPC_SETTINGS->bVPUseRTXVideoHDR)
		|| (!MPC_SETTINGS->bVPUseRTXVideoHDR && !SourceIsHDR())) {
		InitSwapChain();
	}

	hr = m_TexSrcVideo.Create(GetDevice, dxgiFormat, width, height, Tex2D_DynamicShaderWriteNoSRV);
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO, "CVideoProcessor::InitializeD3D11VP() : m_TexSrcVideo.Create() failed with error {}", WToA(HR2Str(hr)).c_str());
		return hr;
	}

	m_srcWidth = width;
	m_srcHeight = height;
	m_srcParams = params;
	m_srcDXGIFormat = dxgiFormat;
	m_pConvertFn = GetCopyFunction(params);

	CLog::LogF(LOGINFO, "CVideoProcessor::InitializeD3D11VP() completed successfully");

	return S_OK;
}

HRESULT CVideoProcessor::InitializeTexVP(const FmtConvParams_t& params, const UINT width, const UINT height)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	const auto& srcDXGIFormat = params.DX11Format;

	CLog::LogF(LOGINFO, "CVideoProcessor::InitializeTexVP() started with input surface: {}, {} x {}", WToA(DXGIFormatToString(srcDXGIFormat)).c_str(), width, height);

	HRESULT hr = m_TexSrcVideo.CreateEx(GetDevice, srcDXGIFormat, params.pDX11Planes, width, height, Tex2D_DynamicShaderWrite);
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO, "CVideoProcessor::InitializeTexVP() : m_TexSrcVideo.CreateEx() failed with error {}", WToA(HR2Str(hr)).c_str());
		return hr;
	}

	m_srcWidth = width;
	m_srcHeight = height;
	m_srcParams = params;
	m_srcDXGIFormat = srcDXGIFormat;
	m_pConvertFn = GetCopyFunction(params);

	CMPCVRRenderer::Get()->CreateIntermediateTarget(m_srcWidth, m_srcHeight, false, DX::DeviceResources::Get()->GetBackBuffer().GetFormat());



	// set default ProcAmp ranges
	SetDefaultDXVA2ProcAmpRanges(m_DXVA2ProcAmpRanges);

	CreateVertexBuffer(GetDevice, &m_PSConvColorData.pVertexBuffer, m_srcWidth, m_srcHeight, m_srcRect, 0, false);
	UpdateConvertColorShader();

	CLog::Log(LOGINFO, "CVideoProcessor::InitializeTexVP() completed successfully");

	return S_OK;
}

void CVideoProcessor::UpdatFrameProperties()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	m_srcPitch = m_srcWidth * m_srcParams.Packsize;
	m_srcLines = m_srcHeight * m_srcParams.PitchCoeff / 2;
}

BOOL CVideoProcessor::GetAlignmentSize(const CMediaType& mt, SIZE& Size)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	if (VerifyMediaType(&mt)) {
		const auto& FmtParams = GetFmtConvParams(&mt);

		if (FmtParams.cformat == CF_RGB24) {
			Size.cx = ALIGN(Size.cx, 4);
		}
		else if (FmtParams.cformat == CF_RGB48 || FmtParams.cformat == CF_BGR48) {
			Size.cx = ALIGN(Size.cx, 2);
		}
		else if (FmtParams.cformat == CF_BGRA64 || FmtParams.cformat == CF_B64A) {
			// nothing
		}
		else {
			auto pBIH = GetBIHfromVIHs(&mt);
			if (!pBIH) {
				return FALSE;
			}

			auto biWidth = pBIH->biWidth;
			auto biHeight = labs(pBIH->biHeight);

			if (!m_Alignment.cx || m_Alignment.cformat != FmtParams.cformat
				|| m_Alignment.texture.desc.Width != biWidth || m_Alignment.texture.desc.Height != biHeight) {
				m_Alignment.texture.Release();
				m_Alignment.cformat = {};
				m_Alignment.cx = {};
			}

			if (!m_Alignment.texture.pTexture) {
				auto VP11Format = FmtParams.VP11Format;


				HRESULT hr = E_FAIL;
				if (VP11Format != DXGI_FORMAT_UNKNOWN) {
					hr = m_Alignment.texture.Create(GetDevice, VP11Format, biWidth, biHeight, Tex2D_DynamicShaderWriteNoSRV);
				}
				if (FAILED(hr) && FmtParams.DX11Format != DXGI_FORMAT_UNKNOWN) {
					hr = m_Alignment.texture.CreateEx(GetDevice, FmtParams.DX11Format, FmtParams.pDX11Planes, biWidth, biHeight, Tex2D_DynamicShaderWrite);
				}
				if (FAILED(hr)) {
					return FALSE;
				}

				UINT RowPitch = 0;
				D3D11_MAPPED_SUBRESOURCE mappedResource = {};
				if (SUCCEEDED(m_pDeviceContext.Get()->Map(m_Alignment.texture.pTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
					RowPitch = mappedResource.RowPitch;
					m_pDeviceContext.Get()->Unmap(m_Alignment.texture.pTexture.Get(), 0);
				}

				if (!RowPitch) {
					return FALSE;
				}

				m_Alignment.cformat = FmtParams.cformat;
				m_Alignment.cx = RowPitch / FmtParams.Packsize;
			}

			Size.cx = m_Alignment.cx;
		}

		if (FmtParams.cformat == CF_RGB24 || FmtParams.cformat == CF_XRGB32 || FmtParams.cformat == CF_ARGB32) {
			Size.cy = -abs(Size.cy); // only for biCompression == BI_RGB
		}
		else {
			Size.cy = abs(Size.cy);
		}

		return TRUE;

	}

	return FALSE;
}

void CVideoProcessor::UpdateTexures()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	if (!m_srcWidth || !m_srcHeight)
		return;
	HRESULT hr = S_OK;

	if (m_D3D11VP.IsReady()) {
		if (MPC_SETTINGS->bVPScaling) {
			Com::SmartSize texsize = m_renderRect.Size();
			hr = m_TexConvertOutput.CheckCreate(GetDevice, m_D3D11OutputFmt, texsize.cx, texsize.cy, Tex2D_DefaultShaderRTarget);
		}
		else {
			hr = m_TexConvertOutput.CheckCreate(GetDevice, m_D3D11OutputFmt, m_srcRectWidth, m_srcRectHeight, Tex2D_DefaultShaderRTarget);
		}
	}
	else {
		hr = m_TexConvertOutput.CheckCreate(GetDevice, m_InternalTexFmt, m_srcRectWidth, m_srcRectHeight, Tex2D_DefaultShaderRTarget);
	}
}

HRESULT CVideoProcessor::D3D11VPPass(ID3D11Texture2D* pRenderTarget, const Com::SmartRect& srcRect, const Com::SmartRect& dstRect, const bool second)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	HRESULT hr = m_D3D11VP.SetRectangles(srcRect, dstRect);

	hr = m_D3D11VP.Process(pRenderTarget, m_SampleFormat, second);
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO, "CVideoProcessor::ProcessD3D11() : m_D3D11VP.Process() failed with error {}", WToA(HR2Str(hr)).c_str());
	}

	return hr;
}

void CVideoProcessor::SetVideoRect(const Com::SmartRect& videoRect)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	m_videoRect = videoRect;
	UpdateRenderRect();
	UpdateTexures();
}

HRESULT CVideoProcessor::SetWindowRect(const Com::SmartRect& windowRect)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	m_windowRect = windowRect;
	UpdateRenderRect();

	return S_OK;
}

void CVideoProcessor::Reset(bool bForceWindowed)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	CAutoLock cRendererLock(&m_pFilter->m_RendererLock);
	ReleaseDevice();
	CMPCVRRenderer::Get()->Reset();

	m_pDXGIFactory1 = nullptr;

}

HRESULT CVideoProcessor::GetPresentationTexture(ID3D11Texture2D** texture)
{

	if (m_presentationQueue.empty())
		return S_FALSE; // No sample available

	// Check the current time using the reference clock.
	REFERENCE_TIME rtNow = 0;
	REFERENCE_TIME rtDiff = 0;

	m_pFilter->m_pClock->GetTime(&rtNow);

	// Pop the next sample.

	CRefTime rtClock, rtRefDiff;
	CRefTime rtNowTime(rtNow);
	CRefTime rtStartTime(m_rtStartStream);
	CRefTime rtSampleStart(m_presentationQueue.front().pStartTime);
	CRefTime rtSampleEnd(m_presentationQueue.front().pEndTime);
	rtDiff = rtNow - m_presentationQueue.front().pStartTime;
	rtRefDiff = CRefTime(rtDiff);
	if (m_pFilter->m_filterState == State_Running) {
		//CLog::Log(LOGINFO, "now : {}ms sample start {}ms end {}ms started at {} diff {}", rtNowTime.Millisecs(), rtSampleStart.Millisecs(), rtSampleEnd.Millisecs(), rtStartTime.Millisecs(), rtRefDiff.Millisecs());

	}
	if (m_iPresCount == 10)
	{
		CStdStringW sNow;
		sNow.Format(L"Now: %ld", rtNowTime.Millisecs());
		CMPCVRRenderer::Get()->SetStatsTimings(sNow.c_str(), 0);
		sNow.Format(L"Diff: %ld", rtRefDiff.Millisecs());
		CMPCVRRenderer::Get()->SetStatsTimings(sNow.c_str(), 1);
		sNow.Format(L"Presentation queue: %i", m_presentationQueue.size());
		CMPCVRRenderer::Get()->SetStatsTimings(sNow.c_str(), 2);
		m_iPresCount = 0;
	}
	m_iPresCount++;

	for (;;)
	{
		if (rtNow < m_presentationQueue.front().pEndTime)
			break;
		if (m_presentationQueue.size() == 1)
			break;

		m_pFreePresentationQueue.push(m_presentationQueue.front());
		m_presentationQueue.pop();

		if (m_presentationQueue.empty())
			return S_FALSE; // No sample available with good timing
	}
	CMPCVRFrame ts = m_presentationQueue.front();

	*texture = ts.pTexture.Get();
	return S_OK;

}

void CVideoProcessor::RenderRectChanged(CRect newRect)
{
	m_destRect = Com::SmartRect(0, 0, newRect.Width(), newRect.Height());
	SetEvent(m_hResizeEvent);
}

HRESULT CVideoProcessor::Reset()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	CLog::LogF(LOGINFO, "CVideoProcessor::Reset()");

	if (MPC_SETTINGS->bHdrPassthrough && SourceIsPQorHLG()) {
		MONITORINFOEXW mi = { sizeof(mi) };
		GetMonitorInfoW(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY), (MONITORINFO*)&mi);
		DisplayConfig_t displayConfig = {};

		if (GetDisplayConfig(mi.szDevice, displayConfig)) {
			const auto& ac = displayConfig.advancedColor;
			const auto bHdrPassthroughSupport = ac.advancedColorSupported && ac.advancedColorEnabled;

			if (bHdrPassthroughSupport && !m_bHdrPassthroughSupport || !ac.advancedColorEnabled && m_bHdrPassthroughSupport) {
				m_hdrModeSavedState.erase(mi.szDevice);

				if (m_pFilter->m_inputMT.IsValid()) {
					if (MPC_SETTINGS->iSwapEffect == SWAPEFFECT_Discard && !ac.advancedColorEnabled) {
						//m_pFilter->Init(true);
					}
					else {
						Init(m_hWnd);
					}
					m_bHdrAllowSwitchDisplay = false;
					InitMediaType(&m_pFilter->m_inputMT);
					m_bHdrAllowSwitchDisplay = true;
				}
			}
		}
	}

	return S_OK;
}

void CVideoProcessor::SetRotation(int value)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	m_iRotation = value;
	if (m_D3D11VP.IsReady()) {
		m_D3D11VP.SetRotation(static_cast<D3D11_VIDEO_PROCESSOR_ROTATION>(value / 90));
	}
}

void CVideoProcessor::SetStereo3dTransform(int value)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	m_iStereo3dTransform = value;

	if (m_iStereo3dTransform == 1) {
		if (!m_pPSHalfOUtoInterlace) {
			CreatePShaderFromResource(&m_pPSHalfOUtoInterlace, IDF_PS_11_HALFOU_TO_INTERLACE);
		}
	}
	else {
		m_pPSHalfOUtoInterlace = nullptr;
	}
}

void CVideoProcessor::Flush()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif

	if (m_D3D11VP.IsReady()) {
		m_D3D11VP.ResetFrameOrder();
	}

	m_rtStart = 0;
	m_rtStartStream = -1;
}

void CVideoProcessor::UpdateStatsPresent()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	DXGI_SWAP_CHAIN_DESC1 swapchain_desc;
	if (GetSwapChain && S_OK == GetSwapChain->GetDesc1(&swapchain_desc)) {
		m_strStatsPresent.assign(L"\nPresentation  : ");
		if (MPC_SETTINGS->bVBlankBeforePresent /*&& m_pDXGIOutput*/) {
			m_strStatsPresent.append(L"wait VBlank, ");
		}
		switch (swapchain_desc.SwapEffect) {
		case DXGI_SWAP_EFFECT_DISCARD:
			m_strStatsPresent.append(L"Discard");
			break;
		case DXGI_SWAP_EFFECT_SEQUENTIAL:
			m_strStatsPresent.append(L"Sequential");
			break;
		case DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL:
			m_strStatsPresent.append(L"Flip sequential");
			break;
		case DXGI_SWAP_EFFECT_FLIP_DISCARD:
			m_strStatsPresent.append(L"Flip discard");
			break;
		}
		m_strStatsPresent.append(L", ");
		m_strStatsPresent.append(DXGIFormatToString(swapchain_desc.Format));
	}
}

void CVideoProcessor::UpdateStatsStatic()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	if (m_srcParams.cformat) {
		m_strStatsHeader.Format(L"MPCVR modified for kodi with libplacebo", _CRT_WIDE(VERSION_STR));

		UpdateStatsInputFmt();

		m_strStatsVProc.assign(L"\nTexture sampler: ");

		if (m_pFinalTextureSampler == D3D11_VP)
			m_strStatsVProc.AppendFormat(L"D3D11 VP, from %s to %s", m_D3D11OutputFmt == DXGI_FORMAT_R10G10B10A2_UNORM ? L"YUV420P 10bit" : L"NV12 8 bit", DXGIFormatToString(m_D3D11OutputFmt));
		else if (m_pFinalTextureSampler == D3D11_INTERNAL_SHADERS)
			m_strStatsVProc.AppendFormat(L"Internal shaders, from %s to %s", m_D3D11OutputFmt == DXGI_FORMAT_R10G10B10A2_UNORM ? L"YUV420P 10bit" : L"NV12 8 bit", DXGIFormatToString(m_SwapChainFmt));
		else
			m_strStatsVProc.AppendFormat(L"Libplacebo directly, from %s to %s", m_D3D11OutputFmt == DXGI_FORMAT_R10G10B10A2_UNORM ? L"YUV420P 10bit" : L"NV12 8 bit", DXGIFormatToString(m_SwapChainFmt));
		//todo add else if for placebo merger when it will be readded

		if (SourceIsHDR() || MPC_SETTINGS->bVPUseRTXVideoHDR) {
			m_strStatsHDR.assign(L"\nHDR processing: ");
			if (m_bHdrPassthroughSupport && MPC_SETTINGS->bHdrPassthrough) {
				m_strStatsHDR.append(L"Passthrough");
				if (MPC_SETTINGS->bVPUseRTXVideoHDR) {
					m_strStatsHDR.append(L", RTX Video HDR*");
				}
				if (m_lastHdr10.bValid) {
					m_strStatsHDR.AppendFormat(L", %u nits", m_lastHdr10.hdr10.MaxMasteringLuminance / 10000);
				}
			}
			else if (MPC_SETTINGS->bConvertToSdr) {
				m_strStatsHDR.append(L"Convert to SDR");
			}
			else {
				m_strStatsHDR.append(L"Not used");
			}
		}
		else {
			m_strStatsHDR.clear();
		}

		UpdateStatsPresent();
	}
	else {
		m_strStatsHeader = L"Error";
		m_strStatsVProc.clear();
		m_strStatsInputFmt.clear();
		//m_strStatsPostProc.clear();
		m_strStatsHDR.clear();
		m_strStatsPresent.clear();
	}
}

bool ShouldShowHdr(double val)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	if (val > 0 && val < 203)
		return true;
	else
		return false;
}

void CVideoProcessor::SendStats(const struct pl_color_space csp, const struct pl_color_repr repr)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	CStdStringW str;
	if (MPC_SETTINGS->displayStats == DS_STATS_2)
	{
		str.reserve(700);
		CServiceBroker::GetGUI()->GetInfoManager().GetInfoProviders().GetSystemInfoProvider().UpdateFPS();
		str.Format(L"FPS:%4.3f\n", CServiceBroker::GetGUI()->GetInfoManager().GetInfoProviders().GetSystemInfoProvider().GetFPS());
		str.AppendFormat(L"RenderRect: x1:%u x2:%u y1:%u y2:%u\n", m_renderRect.left, m_renderRect.right, m_renderRect.top, m_renderRect.bottom);
		str.AppendFormat(L"Video Rect: x1:%u x2:%u y1:%u y2:%u\n", m_videoRect.left, m_videoRect.right, m_videoRect.top, m_videoRect.bottom);
		str.AppendFormat(L"Window Rect: x1:%u x2:%u y1:%u y2:%u\n", m_windowRect.left, m_windowRect.right, m_windowRect.top, m_windowRect.bottom);

		str.AppendFormat(L"BackBuffer Size: width:%u height:%u\n", DX::DeviceResources::Get()->GetBackBuffer().GetWidth(), DX::DeviceResources::Get()->GetBackBuffer().GetHeight());
	}
	else if (MPC_SETTINGS->displayStats == DS_STATS_1)
	{
		str.reserve(700);
		str.assign(m_strStatsHeader);
		str.append(m_strStatsDispInfo);
		str.AppendFormat(L"\nGraph. Adapter: %s", m_strAdapterDescription.c_str());

		wchar_t frametype = (m_SampleFormat != D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE) ? 'i' : 'p';
		str.AppendFormat(L"\n FPS: %4.3f %c,%4.3f", m_pFilter->m_FrameStats.GetAverageFps(), frametype, m_pFilter->m_DrawStats.GetAverageFps());

		str.append(m_strStatsInputFmt);
		str.append(m_strStatsVProc);

		const int dstW = m_videoRect.Width();
		const int dstH = m_videoRect.Height();
		if (m_iRotation) {
			str.AppendFormat(L"\nScaling       : %ux%u r%u\u00B0> %ix%i", m_srcRectWidth, m_srcRectHeight, m_iRotation, dstW, dstH);
		}
		else {
			str.AppendFormat(L"\nScaling       : %ux%u -> %ix%i", m_srcRectWidth, m_srcRectHeight, dstW, dstH);
		}

		if (m_strCorrection || m_bDitherUsed)
		{
			str.append(L"\nPostProcessing:");
			if (m_strCorrection)
				str.AppendFormat(L" %s,", m_strCorrection);
			if (m_bDitherUsed) {
				str.append(L" dither");
			}
			str = str.TrimRight(',');
		}
		str.append(m_strStatsHDR);
		str.append(m_strStatsPresent);

		str.AppendFormat(L"\nFrames: %u, skipped: %u/%u, failed: %u",
			m_pFilter->m_FrameStats.GetFrames(), m_pFilter->m_DrawStats.m_dropped, m_RenderStats.dropped2, m_RenderStats.failed);
		str.AppendFormat(L"\nTimes(ms): Copy: %u, Paint %u, Present %u",
			m_RenderStats.copyticks * 1000 / GetPreciseTicksPerSecondI(),
			m_RenderStats.paintticks * 1000 / GetPreciseTicksPerSecondI(),
			m_RenderStats.presentticks * 1000 / GetPreciseTicksPerSecondI());

		str.AppendFormat(L"\nSync offset   : %i ms", (m_RenderStats.syncoffset + 5000) / 10000);
	}
	else if (MPC_SETTINGS->displayStats == DS_STATS_3)
	{
		str.reserve(700);

		str = L"Video::\n";
		CServiceBroker::GetGUI()->GetInfoManager().GetInfoProviders().GetSystemInfoProvider().UpdateFPS();
		str.AppendFormat(L" Real FPS: %4.3f\n", CServiceBroker::GetGUI()->GetInfoManager().GetInfoProviders().GetSystemInfoProvider().GetFPS());
		str.AppendFormat(L" Video FPS: Avg: %4.3f\n", m_pFilter->m_FrameStats.GetAverageFps());
		str.AppendFormat(L" Colormatrix: %s Primaries: %s Transfer: %s\n", PL::PLCspToString(repr.sys), PL::PLCspPrimToString(csp.primaries), PL::PLCspTransfertToString(csp.transfer));
		pl_hdr_metadata hdr = csp.hdr;
		if (1)
		{

			if (hdr.max_luma > 0)
				str.AppendFormat(L" HDR10: %.4fg / %f cd/m²", hdr.min_luma, hdr.max_luma);

			if (hdr.max_cll > 0)
				str.AppendFormat(L" MaxCLL: %.0f cd/m²", hdr.max_cll);

			if (hdr.max_fall > 0)
				str.AppendFormat(L" MaxFALL: %.0f cd/m²\n", hdr.max_fall);

			if (hdr.scene_max[0] || hdr.scene_max[1] || hdr.scene_max[2] || hdr.scene_avg)
				str.AppendFormat(L" HDR10+: MaxRGB: %.1f/%.1f/%.1f cd/m² Avg: %.1f cd/m²\n", (hdr.scene_max[0] * 1000), (hdr.scene_max[1] * 1000), (hdr.scene_max[2] * 1000), (hdr.scene_avg * 1000));

			if (hdr.max_pq_y && hdr.avg_pq_y)
			{
				str.AppendFormat(L"  PQ(Y): Max:%.2f cd/m² (%.2f%% PQ) ", hdr.max_pq_y, (float)(hdr.max_pq_y * 100));
				str.AppendFormat(L"Avg:%.2f cd/m² (%.2f%% PQ) \n", hdr.avg_pq_y, (float)(hdr.avg_pq_y * 100));
			}
		}
	}
	CMPCVRRenderer::Get()->SetStats(str);

}

// IMFVideoProcessor

STDMETHODIMP CVideoProcessor::SetProcAmpValues(DWORD dwFlags, DXVA2_ProcAmpValues* pValues)
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	CheckPointer(pValues, E_POINTER);
	if (m_srcParams.cformat == CF_NONE) {
		return MF_E_TRANSFORM_TYPE_NOT_SET;
	}

	if (dwFlags & DXVA2_ProcAmp_Brightness) {
		m_DXVA2ProcAmpValues.Brightness.ll = std::clamp(pValues->Brightness.ll, m_DXVA2ProcAmpRanges[0].MinValue.ll, m_DXVA2ProcAmpRanges[0].MaxValue.ll);
	}
	if (dwFlags & DXVA2_ProcAmp_Contrast) {
		m_DXVA2ProcAmpValues.Contrast.ll = std::clamp(pValues->Contrast.ll, m_DXVA2ProcAmpRanges[1].MinValue.ll, m_DXVA2ProcAmpRanges[1].MaxValue.ll);
	}
	if (dwFlags & DXVA2_ProcAmp_Hue) {
		m_DXVA2ProcAmpValues.Hue.ll = std::clamp(pValues->Hue.ll, m_DXVA2ProcAmpRanges[2].MinValue.ll, m_DXVA2ProcAmpRanges[2].MaxValue.ll);
	}
	if (dwFlags & DXVA2_ProcAmp_Saturation) {
		m_DXVA2ProcAmpValues.Saturation.ll = std::clamp(pValues->Saturation.ll, m_DXVA2ProcAmpRanges[3].MinValue.ll, m_DXVA2ProcAmpRanges[3].MaxValue.ll);
	}

	if (dwFlags & DXVA2_ProcAmp_Mask) {
		CAutoLock cRendererLock(&m_pFilter->m_RendererLock);

		m_D3D11VP.SetProcAmpValues(&m_DXVA2ProcAmpValues);

		if (!m_D3D11VP.IsReady()) {
			//SetShaderConvertColorParams();
		}
	}

	return S_OK;
}

void CVideoProcessor::SetResolution()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif

}

bool CVideoProcessor::ParentWindowProc(HWND hWnd, UINT uMsg, WPARAM* wParam, LPARAM* lParam, LRESULT* ret) const
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif
	* ret = m_pFilter->OnReceiveMessage(hWnd, uMsg, (WPARAM)wParam, (LPARAM)lParam);
	return false;
}



void CVideoProcessor::SetCallbackDevice()
{
#if DEBUGEXTREME
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
#endif

	CLog::Log(LOGINFO, "{} setting callback", __FUNCTION__);
	if (!m_bCallbackDeviceIsSet && GetDevice && m_pFilter->m_pSub11CallBack) {
		m_bCallbackDeviceIsSet = SUCCEEDED(m_pFilter->m_pSub11CallBack->SetDevice11(GetDevice));
		CLog::Log(LOGINFO, "{} setting callback is set", __FUNCTION__);
	}
}
