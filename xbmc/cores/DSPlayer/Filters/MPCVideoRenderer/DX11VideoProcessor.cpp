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
#include <uuids.h>
#include <Mferror.h>
#include <Mfidl.h>
#include <dwmapi.h>
#include <optional>
#include "Helper.h"
#include "Times.h"
#include "resource.h"
#include "VideoRenderer.h"
#include "Include/Version.h"
#include "DX11VideoProcessor.h"
#include "Include/ID3DVideoMemoryConfiguration.h"
#include "shaders.h"


#include "minhook/include/MinHook.h"
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

bool g_bPresent = false;
bool bCreateSwapChain = false;

#define GetDevice DX::DeviceResources::Get()->GetD3DDevice()
#define GetSwapChain DX::DeviceResources::Get()->GetSwapChain()

typedef BOOL(WINAPI* pSetWindowPos)(
	_In_ HWND hWnd,
	_In_opt_ HWND hWndInsertAfter,
	_In_ int X,
	_In_ int Y,
	_In_ int cx,
	_In_ int cy,
	_In_ UINT uFlags);

pSetWindowPos pOrigSetWindowPosDX11 = nullptr;
static BOOL WINAPI pNewSetWindowPosDX11(
	_In_ HWND hWnd,
	_In_opt_ HWND hWndInsertAfter,
	_In_ int X,
	_In_ int Y,
	_In_ int cx,
	_In_ int cy,
	_In_ UINT uFlags)
{
	if (g_bPresent) {
		CLog::Log(LOGINFO,"call SetWindowPos() function during Present()");
		uFlags |= SWP_ASYNCWINDOWPOS;
	}
	return pOrigSetWindowPosDX11(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

typedef LONG(WINAPI* pSetWindowLongA)(
	_In_ HWND hWnd,
	_In_ int nIndex,
	_In_ LONG dwNewLong);

pSetWindowLongA pOrigSetWindowLongADX11 = nullptr;
static LONG WINAPI pNewSetWindowLongADX11(
	_In_ HWND hWnd,
	_In_ int nIndex,
	_In_ LONG dwNewLong)
{
	if (bCreateSwapChain) {
		CLog::Log(LOGINFO,"Blocking call SetWindowLongA() function during create fullscreen swap chain");
		return 0L;
	}
	return pOrigSetWindowLongADX11(hWnd, nIndex, dwNewLong);
}

template <typename T>
inline bool HookFunc(T** ppSystemFunction, PVOID pHookFunction)
{
	return MH_CreateHook(*ppSystemFunction, pHookFunction, reinterpret_cast<LPVOID*>(ppSystemFunction)) == MH_OK;
}

struct MPCPixFmtDesc
{
	int codedbytes;     ///< coded byte per pixel in one plane (for packed and multibyte formats)
	int planes;         ///< number of planes
	int planeWidth[4];  ///< log2 width factor
	int planeHeight[4]; ///< log2 height factor
};

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
/*static MPCPixFmtDesc lav_pixfmt_desc[] = {
		{1, 3, {1, 2, 2}, {1, 2, 2}}, ///< LAVPixFmt_YUV420
		{2, 3, {1, 2, 2}, {1, 2, 2}}, ///< LAVPixFmt_YUV420bX
		{1, 3, {1, 2, 2}, {1, 1, 1}}, ///< LAVPixFmt_YUV422
		{2, 3, {1, 2, 2}, {1, 1, 1}}, ///< LAVPixFmt_YUV422bX
		{1, 3, {1, 1, 1}, {1, 1, 1}}, ///< LAVPixFmt_YUV444
		{2, 3, {1, 1, 1}, {1, 1, 1}}, ///< LAVPixFmt_YUV444bX
		{1, 2, {1, 1}, {1, 2}},       ///< LAVPixFmt_NV12
		{2, 1, {1}, {1}},             ///< LAVPixFmt_YUY2
		{2, 2, {1, 1}, {1, 2}},       ///< LAVPixFmt_P016
		{3, 1, {1}, {1}},             ///< LAVPixFmt_RGB24
		{4, 1, {1}, {1}},             ///< LAVPixFmt_RGB32
		{4, 1, {1}, {1}},             ///< LAVPixFmt_ARGB32
		{6, 1, {1}, {1}},             ///< LAVPixFmt_RGB48
};*/

MPCPixFmtDesc getPixelFormatDesc(DXGI_FORMAT fmt)
{
	if (fmt == DXGI_FORMAT_NV12)
		return MPCPixFmtDesc{ 1, 2, {1, 1}, {1, 2} };
	return MPCPixFmtDesc{};
}

//
// CDX11VideoProcessor
//

// CDX11VideoProcessor

CDX11VideoProcessor::CDX11VideoProcessor(CMpcVideoRenderer* pFilter, HRESULT& hr)
	: CVideoProcessor(pFilter)
{
	g_dsSettings.Initialize("mpcvr");
	
	MPC_SETTINGS->displayStats = (DS_STATS)CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_DSPLAYER_VR_DISPLAY_STATS);
	MPC_SETTINGS->m_pPlaceboOptions = (LIBPLACEBO_SHADERS)CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_DSPLAYER_VR_LIBPLACEBO_SHADERS);
	MPC_SETTINGS->bVPUseRTXVideoHDR = true;// config.bVPRTXVideoHDR;
	MPC_SETTINGS->bD3D11TextureSampler = (D3D11_TEXTURE_SAMPLER)CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(CSettings::SETTING_DSPLAYER_VR_TEXTURE_SAMPLER);
	

	m_iVPSuperRes = true;// config.iVPSuperRes;

	m_nCurrentAdapter = -1;
	CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>()->Register(this);
	hr = CreateDXGIFactory1(IID_IDXGIFactory1, (void**)&m_pDXGIFactory1);
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO,"CDX11VideoProcessor::CDX11VideoProcessor() : CreateDXGIFactory1() failed with error {}", WToA(HR2Str(hr)).c_str());
		return;
	}

	// set default ProcAmp ranges and values
	SetDefaultDXVA2ProcAmpRanges(m_DXVA2ProcAmpRanges);
	SetDefaultDXVA2ProcAmpValues(m_DXVA2ProcAmpValues);

	/*pOrigSetWindowPosDX11 = SetWindowPos;
	auto ret = HookFunc(&pOrigSetWindowPosDX11, pNewSetWindowPosDX11);
	DLogIf(!ret, L"CDX11VideoProcessor::CDX11VideoProcessor() : hook for SetWindowPos() fail");

	pOrigSetWindowLongADX11 = SetWindowLongA;
	ret = HookFunc(&pOrigSetWindowLongADX11, pNewSetWindowLongADX11);
	DLogIf(!ret, L"CDX11VideoProcessor::CDX11VideoProcessor() : hook for SetWindowLongA() fail");

	MH_EnableHook(MH_ALL_HOOKS);*/

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

			pDXGIOutput= nullptr;
		}

		pDXGIAdapter= nullptr;
	}
	//CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>()->Register(this);
}

static bool ToggleHDR(const DisplayConfig_t& displayConfig, const BOOL bEnableAdvancedColor)
{
	auto GetCurrentDisplayMode = [](LPCWSTR lpszDeviceName) -> std::optional<DEVMODEW> {
		DEVMODEW devmode = {};
		devmode.dmSize = sizeof(DEVMODEW);
		auto ret = EnumDisplaySettingsW(lpszDeviceName, ENUM_CURRENT_SETTINGS, &devmode);
		if (ret) {
			return devmode;
		}

		return {};
	};

	auto beforeModeOpt = GetCurrentDisplayMode(displayConfig.displayName);

	DISPLAYCONFIG_SET_ADVANCED_COLOR_STATE setColorState = {};
	setColorState.header.type         = DISPLAYCONFIG_DEVICE_INFO_SET_ADVANCED_COLOR_STATE;
	setColorState.header.size         = sizeof(setColorState);
	setColorState.header.adapterId    = displayConfig.modeTarget.adapterId;
	setColorState.header.id           = displayConfig.modeTarget.id;
	setColorState.enableAdvancedColor = bEnableAdvancedColor;

	const auto ret = DisplayConfigSetDeviceInfo(&setColorState.header);
	DLogIf(ERROR_SUCCESS != ret, "ToggleHDR() : DisplayConfigSetDeviceInfo({}) failed with error {}", bEnableAdvancedColor, WToA(HR2Str(HRESULT_FROM_WIN32(ret))));

	if (ret == ERROR_SUCCESS && beforeModeOpt.has_value()) {
		auto afterModeOpt = GetCurrentDisplayMode(displayConfig.displayName);
		if (afterModeOpt.has_value()) {
			auto& beforeMode = *beforeModeOpt;
			auto& afterMode = *afterModeOpt;
			if (beforeMode.dmPelsWidth != afterMode.dmPelsWidth || beforeMode.dmPelsHeight != afterMode.dmPelsHeight
					|| beforeMode.dmBitsPerPel != afterMode.dmBitsPerPel || beforeMode.dmDisplayFrequency != afterMode.dmDisplayFrequency) {
				CLog::Log(LOGINFO,"ToggleHDR() : Display mode changed from {}x{}@{} to {}x{}@{}, restoring",
					 beforeMode.dmPelsWidth, beforeMode.dmPelsHeight, beforeMode.dmDisplayFrequency,
					 afterMode.dmPelsWidth, afterMode.dmPelsHeight, afterMode.dmDisplayFrequency);

				auto ret = ChangeDisplaySettingsExW(displayConfig.displayName, &beforeMode, nullptr, CDS_FULLSCREEN, nullptr);
				DLogIf(DISP_CHANGE_SUCCESSFUL != ret, "ToggleHDR() : ChangeDisplaySettingsExW() failed with error {}", WToA(HR2Str(HRESULT_FROM_WIN32(ret))));
			}
		}
	}

	return ret == ERROR_SUCCESS;
}



CDX11VideoProcessor::~CDX11VideoProcessor()
{
	for (const auto& [displayName, state] : m_hdrModeSavedState) {
		DisplayConfig_t displayConfig = {};
		if (GetDisplayConfig(displayName.c_str(), displayConfig)) {
			const auto& ac = displayConfig.advancedColor;

			if (ac.advancedColorSupported && ac.advancedColorEnabled != state) {
				const auto ret = ToggleHDR(displayConfig, state);
				DLogIf(!ret, "CDX11VideoProcessor::~CDX11VideoProcessor() : Toggle HDR {} for '{}' failed", state ? "ON" : "OFF", WToA(displayName));
			}
		}
	}

	ReleaseDevice();

	m_pDXGIFactory1= nullptr;

	MH_RemoveHook(SetWindowPos);
	MH_RemoveHook(SetWindowLongA);
}

HRESULT CDX11VideoProcessor::Init(const HWND hwnd, bool* pChangeDevice/* = nullptr*/)
{
	CLog::Log(LOGINFO,"CDX11VideoProcessor::Init()");
	
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
			UpdateStatsStatic();
			m_pFilter->OnDisplayModeChange();
		}
		
		return S_OK;
	}
	m_nCurrentAdapter = currentAdapter;

	if (m_bDecoderDevice && GetSwapChain) {
		return S_OK;
	}

	ReleaseDevice();

	CLog::LogF(LOGINFO,"{} Settings device ",__FUNCTION__);
	
	HRESULT hr = SetDevice(GetDevice, false);


	if (S_OK == hr) {
		if (pChangeDevice) {
			*pChangeDevice = true;
		}
	}

	if (m_VendorId == PCIV_Intel && CPUInfo::HaveSSE41()) {
		m_pCopyGpuFn = CopyGpuFrame_SSE41;
	} else {
		m_pCopyGpuFn = CopyFrameAsIs;
	}

	return hr;
}

bool CDX11VideoProcessor::Initialized()
{
	return (GetDevice != nullptr && m_pDeviceContext != nullptr);
}

void CDX11VideoProcessor::ReleaseVP()
{
	CLog::Log(LOGINFO,"CDX11VideoProcessor::ReleaseVP()");

	m_pFilter->ResetStreamingTimes2();
	m_RenderStats.Reset();

	if (m_pDeviceContext) {
		m_pDeviceContext->ClearState();
	}

	m_TexSrcVideo.Release();
	m_TexConvertOutput.Release();
	m_TexResize.Release();

	m_PSConvColorData.Release();
	m_pDoviCurvesConstantBuffer= nullptr;

	m_D3D11VP.ReleaseVideoProcessor();
	m_strCorrection = nullptr;

	m_srcParams      = {};
	m_srcDXGIFormat  = DXGI_FORMAT_UNKNOWN;
	m_pConvertFn     = nullptr;
	m_srcWidth       = 0;
	m_srcHeight      = 0;
}

void CDX11VideoProcessor::ReleaseDevice()
{
	CLog::Log(LOGINFO,"CDX11VideoProcessor::ReleaseDevice()");

	ReleaseVP();
	m_D3D11VP.ReleaseVideoDevice();
	m_D3D11VP.ResetFrameOrder();
	
	m_bAlphaBitmapEnable = false;
	

	m_pPSCorrection= nullptr;
	m_pPSConvertColor= nullptr;
	m_pPSConvertColorDeint= nullptr;

	m_pVSimpleInputLayout= nullptr;
	m_pVS_Simple= nullptr;
	
	m_pPS_BitmapToFrame = nullptr;

	m_Alignment.texture.Release();
	m_Alignment.cformat = {};
	m_Alignment.cx = {};
	if (m_pDeviceContext)
		m_pDeviceContext->Flush();
	m_pDeviceContext= nullptr;
	
	m_bCallbackDeviceIsSet = false;
}


HRESULT CDX11VideoProcessor::CreatePShaderFromResource(ID3D11PixelShader** ppPixelShader, std::string resid)
{
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
	//HRESULT hr = GetDataFromResource(data, size, resid);
	

	return GetDevice->CreatePixelShader(data, size, nullptr, ppPixelShader);
}

void CDX11VideoProcessor::UpdateTexParams(int cdepth)
{
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

void CDX11VideoProcessor::UpdateRenderRect()
{
	CLog::Log(LOGINFO, "{}", __FUNCTION__);
	m_renderRect.IntersectRect(m_videoRect, m_windowRect);

}



HRESULT CDX11VideoProcessor::MemCopyToTexSrcVideo(const BYTE* srcData, const int srcPitch)
{
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
	} else {
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

HRESULT CDX11VideoProcessor::SetShaderDoviCurvesPoly()
{
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

HRESULT CDX11VideoProcessor::SetShaderDoviCurves()
{
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

HRESULT CDX11VideoProcessor::UpdateConvertColorShader()
{
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

HRESULT CDX11VideoProcessor::ConvertColorPass(ID3D11Texture2D* pRenderTarget)
{
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
	//m_pDeviceContext.Get()->PSSetConstantBuffers(0, 1, &m_PSConvColorData.pConstants);
	//m_pDeviceContext.Get()->PSSetConstantBuffers(1, 1, m_pDoviCurvesConstantBuffer.GetAddressOf());
	
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
void CDX11VideoProcessor::SetShaderConvertColorParams()
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

void CDX11VideoProcessor::SetShaderLuminanceParams()
{
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

void CDX11VideoProcessor::render_info_cb(void* priv, const pl_render_info* info)
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

HRESULT CDX11VideoProcessor::CopySampleToLibplacebo(IMediaSample* pSample)
{
	HRESULT hr;
	BYTE* data = nullptr;
	Com::SComQIPtr<ID3D11Texture2D> pD3D11Texture2D;
	Microsoft::WRL::ComPtr<IMediaSampleD3D11> pMSD3D11;
	UINT ArraySlice = 0;
	const long size = pSample->GetActualDataLength();
	Microsoft::WRL::ComPtr<ID3D11CommandList> pCommandList;

	PL::CPlHelper* pHelper = CMPCVRRenderer::Get()->GetPlHelper();
	pl_frame frameOut{};
	pl_d3d11_wrap_params outputParams{};
	pl_d3d11_wrap_params inParams{};
	pl_frame frameIn{};
	struct pl_color_space csp {};
	struct pl_color_repr repr {};
	struct pl_dovi_metadata dovi {};
	pl_render_params params;

	csp = pHelper->GetPlColorSpace(m_srcExFmt);
	repr = pHelper->GetPlColorRepresentation(m_srcExFmt);
	csp.hdr = pHelper->GetHdrData(pSample);
	
	m_Dovi.bValid = true;
	if (pHelper->ProcessDoviData(pSample, &csp, &repr, &dovi, m_videoRect.Width(), m_videoRect.Height()))
	{

		Microsoft::WRL::ComPtr<IMediaSideData> pMediaSideData;
		pSample->QueryInterface(IID_PPV_ARGS(&pMediaSideData));
		MediaSideDataDOVIMetadata* pDOVIMetadata = nullptr;
		size_t sized = 0;
		pMediaSideData->GetSideData(IID_MediaSideDataDOVIMetadataV2, (const BYTE**)&pDOVIMetadata, &sized);
		bool bCurveChanged = memcmp(&m_Dovi.msd.Mapping.curves, &pDOVIMetadata->Mapping.curves, sizeof(MediaSideDataDOVIMetadata::Mapping.curves)) != 0;
		const bool bYCCtoRGBChanged = !m_PSConvColorData.bEnable ||	(memcmp(&m_Dovi.msd.ColorMetadata.ycc_to_rgb_matrix,
																	&pDOVIMetadata->ColorMetadata.ycc_to_rgb_matrix,
																	sizeof(MediaSideDataDOVIMetadata::ColorMetadata.ycc_to_rgb_matrix) + sizeof(MediaSideDataDOVIMetadata::ColorMetadata.ycc_to_rgb_offset)) != 0);
		const bool bRGBtoLMSChanged =	(memcmp(&m_Dovi.msd.ColorMetadata.rgb_to_lms_matrix,
																	&pDOVIMetadata->ColorMetadata.rgb_to_lms_matrix,
																	sizeof(MediaSideDataDOVIMetadata::ColorMetadata.rgb_to_lms_matrix)) != 0);
		const bool bMasteringLuminanceChanged = m_Dovi.msd.ColorMetadata.source_max_pq != pDOVIMetadata->ColorMetadata.source_max_pq
																	|| m_Dovi.msd.ColorMetadata.source_min_pq != pDOVIMetadata->ColorMetadata.source_min_pq;

		bool bMMRChanged = false;
		if (bCurveChanged) {
			bool has_mmr = false;
			for (const auto& curve : pDOVIMetadata->Mapping.curves) {
				for (uint8_t i = 0; i < (curve.num_pivots - 1); i++) {
					if (curve.mapping_idc[i] == 1) {
						has_mmr = true;
						break;
					}
				}
			}
			if (m_Dovi.bHasMMR != has_mmr) {
				m_Dovi.bHasMMR = has_mmr;
				m_pDoviCurvesConstantBuffer.Reset();
				bMMRChanged = true;
			}
		}

		memcpy(&m_Dovi.msd, pDOVIMetadata, sizeof(MediaSideDataDOVIMetadata));
		const bool doviStateChanged = !m_Dovi.bValid;
		m_Dovi.bValid = true;

		if (bMasteringLuminanceChanged)
		{
			// based on libplacebo source code
			constexpr float
				PQ_M1 = 2610.f / (4096.f * 4.f),
				PQ_M2 = 2523.f / 4096.f * 128.f,
				PQ_C1 = 3424.f / 4096.f,
				PQ_C2 = 2413.f / 4096.f * 32.f,
				PQ_C3 = 2392.f / 4096.f * 32.f;

			auto hdr_rescale = [&](float x) {
				x = powf(x, 1.0f / PQ_M2);
				x = fmaxf(x - PQ_C1, 0.0f) / (PQ_C2 - PQ_C3 * x);
				x = powf(x, 1.0f / PQ_M1);
				x *= 10000.0f;

				return x;
				};

			m_DoviMaxMasteringLuminance = static_cast<UINT>(hdr_rescale(m_Dovi.msd.ColorMetadata.source_max_pq / 4095.f) * 10000.0);
			m_DoviMinMasteringLuminance = static_cast<UINT>(hdr_rescale(m_Dovi.msd.ColorMetadata.source_min_pq / 4095.f) * 10000.0);
		}

		if (m_D3D11VP.IsReady())
			InitMediaType(&m_pFilter->m_inputMT);
		else if (doviStateChanged)
			UpdateStatsStatic();

		if (bYCCtoRGBChanged)
		{
			CLog::Log(LOGINFO,"{} DoVi ycc_to_rgb_matrix is changed",__FUNCTION__);
			SetShaderConvertColorParams();
		}
		if (bRGBtoLMSChanged || bMMRChanged) {
			CLog::Log(LOGINFO,"{} DoVi rgb_to_lms_matrix is changed",__FUNCTION__);//bRGBtoLMSChanged
			CLog::Log(LOGINFO, "{} DoVi has_mmr is changed", __FUNCTION__); //bMMRChanged
			UpdateConvertColorShader();
		}
		if (bCurveChanged) {
			if (m_Dovi.bHasMMR) {
				hr = SetShaderDoviCurves();
			}
			else {
				hr = SetShaderDoviCurvesPoly();
			}
		}

		if (doviStateChanged && !SourceIsPQorHLG()) {
			//ReleaseSwapChain();
			//Init(m_hWnd);

			m_srcVideoTransferFunction = 0;
			InitMediaType(&m_pFilter->m_inputMT);
		}
	}//end dovi

	

	pSample->QueryInterface(__uuidof(IMediaSampleD3D11), &pMSD3D11);
	//if the input is directly a d3d11 texture lets use it
	if (pMSD3D11)
	{
		hr = pMSD3D11->GetD3D11Texture(0, &pD3D11Texture2D, &ArraySlice);
		if (FAILED(hr)) {
			CLog::LogF(LOGINFO, "CDX11VideoProcessor::CopySample() : GetD3D11Texture() failed with error {}", WToA(HR2Str(hr)).c_str());
			return hr;
		}
		// here should be used CopySubresourceRegion instead of CopyResource
		D3D11_BOX srcBox = { 0, 0, 0, m_srcWidth, m_srcHeight, 1 };
		if (m_D3D11VP.IsReady()) {
			m_pDeviceContext->CopySubresourceRegion(m_D3D11VP.GetNextInputTexture(m_SampleFormat), 0, 0, 0, 0, pD3D11Texture2D, ArraySlice, &srcBox);
		}
		else {
			m_pDeviceContext->CopySubresourceRegion(m_TexSrcVideo.pTexture.Get(), 0, 0, 0, 0, pD3D11Texture2D, ArraySlice, &srcBox);
		}
	}

	if (size > 0 && S_OK == pSample->GetPointer(&data))
	{
		// do not use UpdateSubresource for D3D11 VP here
		// because it can cause green screens and freezes on some configurations
		hr = MemCopyToTexSrcVideo(data, m_srcPitch);
		if (!CMPCVRRenderer::Get()->GetIntermediateTarget().Get())
		{
			CMPCVRRenderer::Get()->CreateIntermediateTarget(m_srcWidth, m_srcHeight, false, DX::DeviceResources::Get()->GetBackBuffer().GetFormat());
			//call there to be sure we have the correct ouput
			UpdateStatsStatic();
		}
		if (!m_pInputTexture.Get())
			m_pInputTexture.Create(m_srcWidth, m_srcHeight, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, DX::DeviceResources::Get()->GetBackBuffer().GetFormat(),"CMPCVRRenderer Merged plane", true, 0U);
	  
		CMPCVRSettings* mpc = static_cast<CMPCVRSettings*>(g_dsSettings.pRendererSettings);
		if (MPC_SETTINGS->bD3D11TextureSampler == D3D11_VP && m_D3D11VP.IsReady())
		{
			m_pDeviceContext->CopyResource(m_D3D11VP.GetNextInputTexture(m_SampleFormat), m_TexSrcVideo.pTexture.Get());
			D3D11VPPass(m_pInputTexture.Get(), m_srcRect, m_srcRect, false);
		}
		else if (MPC_SETTINGS->bD3D11TextureSampler == D3D11_LIBPLACEBO)
		{
			frameIn = pHelper->CreateFrame(m_srcExFmt, pSample, m_srcWidth, m_srcHeight);
		}
		else if (MPC_SETTINGS->bD3D11TextureSampler == D3D11_INTERNAL_SHADERS)
		{
			//use the shaders to merge the planes
			ConvertColorPass(m_pInputTexture.Get());
		}
	}


	if (FAILED(m_pDeviceContext->FinishCommandList(1, &pCommandList)))
	{
		CLog::LogF(LOGERROR, "failed to finish command queue.");
		return E_FAIL;
	}
	else
	{
		D3DSetDebugName(pCommandList.Get(), "CommandList mpc deferred context");
		DX::DeviceResources::Get()->GetImmediateContext()->ExecuteCommandList(pCommandList.Get(), 0);
	}


	outputParams.array_slice = 1;
	inParams.w = m_pInputTexture.GetWidth();
	inParams.h = m_pInputTexture.GetHeight();
	inParams.fmt = m_pInputTexture.GetFormat();
	inParams.tex = m_pInputTexture.Get();
	pl_tex inTexture = pl_d3d11_wrap(pHelper->GetPLD3d11()->gpu, &inParams);

	if (MPC_SETTINGS->bD3D11TextureSampler != D3D11_LIBPLACEBO)
	{
		frameIn.num_planes = 1;
		frameIn.planes[0].texture = inTexture;
		frameIn.planes[0].components = 3;
		frameIn.planes[0].component_mapping[0] = 0;
		frameIn.planes[0].component_mapping[1] = 1;
		frameIn.planes[0].component_mapping[2] = 2;
		frameIn.planes[0].component_mapping[3] = -1;
		frameIn.planes[0].flipped = false;
		repr.bits.color_depth = 10;
		frameIn.repr = repr;
		frameIn.color = csp;
	}
	CD3DTexture outputTarget = CMPCVRRenderer::Get()->GetIntermediateTarget();
	outputParams.w = outputTarget.GetWidth();
	outputParams.h = outputTarget.GetHeight();
	outputParams.fmt = outputTarget.GetFormat();
	outputParams.tex = outputTarget.Get();

	pl_tex interTexture = pl_d3d11_wrap(pHelper->GetPLD3d11()->gpu, &outputParams);
	frameOut.num_planes = 1;
	frameOut.planes[0].texture = interTexture;
	frameOut.planes[0].components = 3;
	frameOut.planes[0].component_mapping[0] = 0;
	frameOut.planes[0].component_mapping[1] = 1;
	frameOut.planes[0].component_mapping[2] = 2;
	frameOut.planes[0].component_mapping[3] = -1;
	frameOut.planes[0].flipped = false;

	frameOut.crop.x1 = m_srcWidth;
	frameOut.crop.y1 = m_srcHeight;
	frameOut.repr = frameIn.repr;
	frameOut.color = frameIn.color;
	pl_chroma_location loc = PL_CHROMA_UNKNOWN;
	//todo fix when not left
	switch (m_srcExFmt.VideoChromaSubsampling) {
	case DXVA2_VideoChromaSubsampling_MPEG2:
		loc = PL_CHROMA_LEFT;
		break;
	case DXVA2_VideoChromaSubsampling_MPEG1:
		loc = PL_CHROMA_CENTER;
		break;
	case DXVA2_VideoChromaSubsampling_Cosited:
		loc = PL_CHROMA_TOP_LEFT;
		break;
	}
	pl_frame_set_chroma_location(&frameOut, PL_CHROMA_LEFT);

	
	pl_swapchain_colorspace_hint(pHelper->GetPLSwapChain(), &csp);

	switch (MPC_SETTINGS->m_pPlaceboOptions)
	{
	case PLACEBO_DEFAULT:
		params = pl_render_default_params;
		break;
	case PLACEBO_FAST:
		params = pl_render_fast_params;
		break;
	case PLACEBO_HIGH:
		params = pl_render_high_quality_params;
		break;
	case PLACEBO_CUSTOM://TODO
		params = pl_render_high_quality_params;
		break;
	default:
		break;
	}

	params.info_priv = pHelper;
	params.info_callback = render_info_cb;

	pl_render_image(pHelper->GetPLRenderer(), &frameIn, &frameOut, &params);
	pl_gpu_finish(pHelper->GetPLD3d11()->gpu);

	CRect src, dst, vw;
	CMPCVRRenderer::Get()->GetVideoRect(src, dst, vw);
	m_renderRect = Com::SmartRect(vw.x1, vw.y1, vw.x2, vw.y2);
	m_videoRect = Com::SmartRect(dst.x1, dst.y1, dst.x2, dst.y2);
	m_windowRect = Com::SmartRect(vw.x1, vw.y1, vw.x2, vw.y2);


	SendStats(csp, repr);
	
	return S_OK;
}

HRESULT CDX11VideoProcessor::SetDevice(ID3D11Device1 *pDevice, const bool bDecoderDevice)
{
	HRESULT hr = S_OK;

	CLog::LogF(LOGINFO,"CDX11VideoProcessor::SetDevice()");

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
		CLog::LogF(LOGERROR, "{} CreateDeferredContext1 failed",__FUNCTION__);
		return hr;
	}
	CLog::LogF(LOGINFO, "{} CreateDeferredContext1 succeeded",__FUNCTION__);
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
		CLog::LogF(LOGINFO,"Graphics DXGI adapter: {}", WToA(m_strAdapterDescription).c_str());
	}

	HRESULT hr2 = m_D3D11VP.InitVideoDevice(pDevice, m_pDeviceContext.Get(), m_VendorId);
	DLogIf(FAILED(hr2), "CDX11VideoProcessor::SetDevice() : InitVideoDevice failed with error %s", WToA(HR2Str(hr2)));

	EXECUTE_ASSERT(S_OK == CreatePShaderFromResource(&m_pPS_BitmapToFrame, IDF_PS_11_SIMPLE));
	
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

	m_pFilter->OnDisplayModeChange();
	UpdateStatsStatic();

	return hr;

}

HRESULT CDX11VideoProcessor::InitSwapChain()
{
	CLog::LogF(LOGINFO, "{} ", __FUNCTION__);

	const auto bHdrOutput = MPC_SETTINGS->bHdrPassthrough && m_bHdrPassthroughSupport && (SourceIsHDR() || MPC_SETTINGS->bVPUseRTXVideoHDR);
	m_currentSwapChainColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
	DXGI_SWAP_CHAIN_DESC1 desc = { 0 };
	GetSwapChain->GetDesc1(&desc);
	m_SwapChainFmt = desc.Format;
	return S_OK;
}

BOOL CDX11VideoProcessor::VerifyMediaType(const CMediaType* pmt)
{
	const auto& FmtParams = GetFmtConvParams(pmt);
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

bool CDX11VideoProcessor::HandleHDRToggle()
{
	m_bHdrDisplaySwitching = true;
	bool bRet = false;
	if (MPC_SETTINGS->bHdrPassthrough && SourceIsHDR()) {
		MONITORINFOEXW mi = { sizeof(mi) };
		GetMonitorInfoW(m_lastFullscreenHMonitor ? m_lastFullscreenHMonitor : MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY), (MONITORINFO*)&mi);
		DisplayConfig_t displayConfig = {};

		if (GetDisplayConfig(mi.szDevice, displayConfig)) {
			const auto& ac = displayConfig.advancedColor;

			if (ac.advancedColorSupported && MPC_SETTINGS->iHdrToggleDisplay) {
				BOOL bHDREnabled = FALSE;
				const auto it = m_hdrModeStartState.find(mi.szDevice);
				if (it != m_hdrModeStartState.cend()) {
					bHDREnabled = it->second;
				}

				const bool bNeedToggleOn  = !ac.advancedColorEnabled &&
											(MPC_SETTINGS->iHdrToggleDisplay == HDRTD_On || MPC_SETTINGS->iHdrToggleDisplay == HDRTD_OnOff
											 || m_bIsFullscreen && (MPC_SETTINGS->iHdrToggleDisplay == HDRTD_On_Fullscreen || MPC_SETTINGS->iHdrToggleDisplay == HDRTD_OnOff_Fullscreen));
				const bool bNeedToggleOff = ac.advancedColorEnabled &&
											!bHDREnabled && !m_bIsFullscreen && MPC_SETTINGS->iHdrToggleDisplay == HDRTD_OnOff_Fullscreen;
				CLog::LogF(LOGINFO,"HandleHDRToggle() : {}, {}", bNeedToggleOn, bNeedToggleOff);
				if (bNeedToggleOn) {
					bRet = ToggleHDR(displayConfig, TRUE);
					DLogIf(!bRet, "CDX11VideoProcessor::HandleHDRToggle() : Toggle HDR ON failed");

					if (bRet) {
						CStdStringW deviceName(mi.szDevice);
						const auto& it = m_hdrModeSavedState.find(deviceName);
						if (it == m_hdrModeSavedState.cend()) {
							m_hdrModeSavedState[std::move(deviceName)] = FALSE;
						}
					}
				} else if (bNeedToggleOff) {
					bRet = ToggleHDR(displayConfig, FALSE);
					DLogIf(!bRet, "CDX11VideoProcessor::HandleHDRToggle() : Toggle HDR OFF failed");

					if (bRet) {
						CStdStringW deviceName(mi.szDevice);
						const auto& it = m_hdrModeSavedState.find(deviceName);
						if (it == m_hdrModeSavedState.cend()) {
							m_hdrModeSavedState[std::move(deviceName)] = TRUE;
						}
					}
				}
			}
		}
	} else if (MPC_SETTINGS->iHdrToggleDisplay) {
		MONITORINFOEXW mi = { sizeof(mi) };
		GetMonitorInfoW(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY), (MONITORINFO*)&mi);
		DisplayConfig_t displayConfig = {};

		if (GetDisplayConfig(mi.szDevice, displayConfig)) {
			const auto& ac = displayConfig.advancedColor;

			// check if HDR was already enabled in Windows before starting
			BOOL bWindowsHDREnabled = FALSE;
			const auto& it = m_hdrModeStartState.find(mi.szDevice);
			if (it != m_hdrModeStartState.cend()) {
				bWindowsHDREnabled = it->second;
			}

			if (ac.advancedColorSupported && ac.advancedColorEnabled &&
					(!bWindowsHDREnabled || (MPC_SETTINGS->iHdrToggleDisplay == HDRTD_OnOff || MPC_SETTINGS->iHdrToggleDisplay == HDRTD_OnOff_Fullscreen && m_bIsFullscreen))) {
				bRet = ToggleHDR(displayConfig, FALSE);
				DLogIf(!bRet, "CDX11VideoProcessor::HandleHDRToggle() : Toggle HDR OFF failed");

				if (bRet) {
					CStdStringW deviceName(mi.szDevice);
					const auto& it = m_hdrModeSavedState.find(deviceName);
					if (it == m_hdrModeSavedState.cend()) {
						m_hdrModeSavedState[std::move(deviceName)] = TRUE;
					}
				}
			}
		}
	}
	m_bHdrDisplaySwitching = false;

	if (bRet) {
		MONITORINFOEXW mi = { sizeof(mi) };
		GetMonitorInfoW(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY), (MONITORINFO*)&mi);
		DisplayConfig_t displayConfig = {};

		if (GetDisplayConfig(mi.szDevice, displayConfig)) {
			const auto& ac = displayConfig.advancedColor;
			m_bHdrPassthroughSupport = ac.advancedColorSupported && ac.advancedColorEnabled;
			m_bHdrDisplayModeEnabled = ac.advancedColorEnabled;
			m_DisplayBitsPerChannel = displayConfig.bitsPerChannel;
		}
	}

	return bRet;
}

void CDX11VideoProcessor::UpdateStatsInputFmt()
{
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
			/*int dv_profile = 0;
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
				dv_profile = 8;
			}*/

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

BOOL CDX11VideoProcessor::InitMediaType(const CMediaType* pmt)
{
	CLog::LogF(LOGINFO,"CDX11VideoProcessor::InitMediaType()");

	if (!VerifyMediaType(pmt)) {
		return FALSE;
	}

	ReleaseVP();

	auto FmtParams = GetFmtConvParams(pmt);

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

	UINT biWidth  = pBIH->biWidth;
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
			origW  = vrextra->FrameWidth;
			origH = abs(vrextra->FrameHeight);
		}
	}

	if (m_srcRect.IsRectNull()) {
		m_srcRect.SetRect(0, 0, origW, origH);
	}
	m_srcRectWidth  = m_srcRect.Width();
	m_srcRectHeight = m_srcRect.Height();

	m_srcExFmt = SpecifyExtendedFormat(m_decExFmt, FmtParams, m_srcRectWidth, m_srcRectHeight);
	
	
	bool disableD3D11VP = (MPC_SETTINGS->bD3D11TextureSampler != D3D11_VP);
	
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

	if (m_bHdrAllowSwitchDisplay && m_srcVideoTransferFunction != m_srcExFmt.VideoTransferFunction) {
		auto ret = HandleHDRToggle();
		if (!ret && (MPC_SETTINGS->bHdrPassthrough && m_bHdrPassthroughSupport && SourceIsPQorHLG())) {
			ret = true;
		}
		if (ret) {
			Init(m_hWnd);
		}
	}

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

			if (resId.length()>0) {
				EXECUTE_ASSERT(S_OK == CreatePShaderFromResource(&m_pPSCorrection, resId));
				DLogIf(m_pPSCorrection, "CDX11VideoProcessor::InitMediaType() m_pPSCorrection('{}') created", WToA(m_strCorrection));
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
	
	if (SUCCEEDED(hr)) {
		UpdateBitmapShader();
		UpdateTexures();
		UpdateStatsStatic();

		m_pFilter->m_inputMT = *pmt;

		return TRUE;
	}

	return FALSE;
}

HRESULT CDX11VideoProcessor::InitializeD3D11VP(const FmtConvParams_t& params, const UINT width, const UINT height)
{
	if (!m_D3D11VP.IsVideoDeviceOk()) {
		return E_ABORT;
	}

	const auto& dxgiFormat = params.VP11Format;

	CLog::LogF(LOGINFO,"CDX11VideoProcessor::InitializeD3D11VP() started with input surface: {}, {} x {}", WToA(DXGIFormatToString(dxgiFormat)).c_str(), width, height);

	m_TexSrcVideo.Release();

	const bool bHdrPassthrough = m_bHdrDisplayModeEnabled && (SourceIsPQorHLG() || (MPC_SETTINGS->bVPUseRTXVideoHDR && params.CDepth == 8));
	m_D3D11OutputFmt = m_InternalTexFmt;
	HRESULT hr = m_D3D11VP.InitVideoProcessor(dxgiFormat, width, height, m_srcExFmt, m_bInterlaced, bHdrPassthrough, m_D3D11OutputFmt);
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO,"CDX11VideoProcessor::InitializeD3D11VP() : InitVideoProcessor() failed with error {}", WToA(HR2Str(hr)).c_str());
		return hr;
	}

	hr = m_D3D11VP.InitInputTextures(GetDevice);
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO,"CDX11VideoProcessor::InitializeD3D11VP() : InitInputTextures() failed with error {}", WToA(HR2Str(hr)).c_str());
		return hr;
	}

	auto superRes = (MPC_SETTINGS->bVPScaling && !(m_bHdrPassthroughSupport && MPC_SETTINGS->bHdrPassthrough && SourceIsHDR())) ? m_iVPSuperRes : SUPERRES_Disable;
	m_bVPUseSuperRes = (m_D3D11VP.SetSuperRes(superRes) == S_OK);
	
	auto rtxHDR = MPC_SETTINGS->bVPUseRTXVideoHDR && m_bHdrPassthroughSupport && MPC_SETTINGS->bHdrPassthrough && MPC_SETTINGS->iTexFormat != TEXFMT_8INT && !SourceIsHDR();
	MPC_SETTINGS->bVPUseRTXVideoHDR = (m_D3D11VP.SetRTXVideoHDR(rtxHDR) == S_OK);

	if ((MPC_SETTINGS->bVPUseRTXVideoHDR)
			|| (!MPC_SETTINGS->bVPUseRTXVideoHDR && !SourceIsHDR())) {
		InitSwapChain();
	}

	hr = m_TexSrcVideo.Create(GetDevice, dxgiFormat, width, height, Tex2D_DynamicShaderWriteNoSRV);
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO,"CDX11VideoProcessor::InitializeD3D11VP() : m_TexSrcVideo.Create() failed with error {}", WToA(HR2Str(hr)).c_str());
		return hr;
	}

	m_srcWidth       = width;
	m_srcHeight      = height;
	m_srcParams      = params;
	m_srcDXGIFormat  = dxgiFormat;
	m_pConvertFn     = GetCopyFunction(params);

	CLog::LogF(LOGINFO,"CDX11VideoProcessor::InitializeD3D11VP() completed successfully");

	return S_OK;
}

HRESULT CDX11VideoProcessor::InitializeTexVP(const FmtConvParams_t& params, const UINT width, const UINT height)
{
	const auto& srcDXGIFormat = params.DX11Format;

	CLog::LogF(LOGINFO,"CDX11VideoProcessor::InitializeTexVP() started with input surface: {}, {} x {}", WToA(DXGIFormatToString(srcDXGIFormat)).c_str(), width, height);

	HRESULT hr = m_TexSrcVideo.CreateEx(GetDevice, srcDXGIFormat, params.pDX11Planes, width, height, Tex2D_DynamicShaderWrite);
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO,"CDX11VideoProcessor::InitializeTexVP() : m_TexSrcVideo.CreateEx() failed with error {}", WToA(HR2Str(hr)).c_str());
		return hr;
	}

	m_srcWidth       = width;
	m_srcHeight      = height;
	m_srcParams      = params;
	m_srcDXGIFormat  = srcDXGIFormat;
	m_pConvertFn     = GetCopyFunction(params);

	// set default ProcAmp ranges
	SetDefaultDXVA2ProcAmpRanges(m_DXVA2ProcAmpRanges);

	CreateVertexBuffer(GetDevice, &m_PSConvColorData.pVertexBuffer, m_srcWidth, m_srcHeight, m_srcRect, 0, false);
	UpdateConvertColorShader();
	CMPCVRRenderer::Get()->CreateIntermediateTarget(m_srcWidth, m_srcHeight, false, DX::DeviceResources::Get()->GetBackBuffer().GetFormat());
	CLog::Log(LOGINFO,"CDX11VideoProcessor::InitializeTexVP() completed successfully");

	return S_OK;
}

void CDX11VideoProcessor::UpdatFrameProperties()
{
	m_srcPitch = m_srcWidth * m_srcParams.Packsize;
	m_srcLines = m_srcHeight * m_srcParams.PitchCoeff / 2;
}

BOOL CDX11VideoProcessor::GetAlignmentSize(const CMediaType& mt, SIZE& Size)
{
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
		} else {
			Size.cy = abs(Size.cy);
		}

		return TRUE;

	}

	return FALSE;
}

HRESULT CDX11VideoProcessor::ProcessSample(IMediaSample* pSample)
{
	if (m_bKodiResizeBuffers)
	{
		//when m_windowRect is modified and in video processor it should be the size of the window
		//not the cleanest way but we need to resize buffers on wm_size
		winrt::Windows::Foundation::Size sz;
		sz = DX::DeviceResources::Get()->GetLogicalSize();
		
		m_windowRect.right = sz.Width;
		m_windowRect.bottom= sz.Height;
		m_videoRect.right = sz.Width;
		m_videoRect.bottom = sz.Height;
		UpdateRenderRect();
		HRESULT hr = SetDevice(GetDevice, false);
		if (SUCCEEDED(hr))
		{
			CLog::Log(LOGDEBUG, "{} kodi asking for a device resize and reset", __FUNCTION__);
			
			m_bKodiResizeBuffers = false;
		}

	}
	REFERENCE_TIME rtStart, rtEnd;
	if (FAILED(pSample->GetTime(&rtStart, &rtEnd))) {
		rtStart = m_pFilter->m_FrameStats.GeTimestamp();
	}
	const REFERENCE_TIME rtFrameDur = m_pFilter->m_FrameStats.GetAverageFrameDuration();
	rtEnd = rtStart + rtFrameDur;

	m_rtStart = rtStart;
	CRefTime rtClock(rtStart);
	

	

	//temp
	if (m_pFilter->m_filterState == State_Running) {
		m_pFilter->StreamTime(rtClock);
	}
	//tell the stream where we are for subtitles
	if (CStreamsManager::Get() && CStreamsManager::Get()->SubtitleManager)
		CStreamsManager::Get()->SubtitleManager->SetTime(rtStart + m_pFilter->m_rtStartTime);

	if (!g_application.GetComponent<CApplicationPlayer>()->IsRenderingVideo())
	{
		// Configure Render Manager
		float fps;
		fps = 10000000.0 / m_rtAvgTimePerFrame;
		if (CStreamsManager::Get()->SubtitleManager)
			CStreamsManager::Get()->SubtitleManager->SetTimePerFrame(m_rtAvgTimePerFrame);

		g_application.GetComponent<CApplicationPlayer>()->Configure(m_srcRectWidth, m_srcRectHeight, m_srcAspectRatioX, m_srcAspectRatioY, fps, 0);
		CLog::Log(LOGINFO, "{} Render manager configured (FPS: {}) {} {} {} {}", __FUNCTION__, fps, m_srcRectWidth, m_srcRectHeight, m_srcAspectRatioX, m_srcAspectRatioY);
	}
	else
	{
		if (!m_bDsplayerNotified)
		{
			m_bDsplayerNotified = true;
			CDSPlayer::PostMessage(new CDSMsg(CDSMsg::PLAYER_PLAYBACK_STARTED), false);
			CLog::Log(LOGINFO, "{} DSPLAYER notify playback started sent", __FUNCTION__);
		}
	}

	//std::unique_lock<CCriticalSection> lock(CServiceBroker::GetWinSystem()->GetGfxContext());
	HRESULT hr = CopySampleToLibplacebo(pSample);


	if (FAILED(hr)) {
		m_RenderStats.failed++;
		return hr;
	}

	if (!g_application.GetComponent<CApplicationPlayer>()->IsRenderingVideo())
		return E_FAIL;

	return hr;
	// always Render(1) a frame after CopySample()
	hr = Render(1, rtStart);
	m_pFilter->m_DrawStats.Add(GetPreciseTick());
	if (m_pFilter->m_filterState == State_Running) {
		m_pFilter->StreamTime(rtClock);
	}

	m_RenderStats.syncoffset = rtClock - rtStart;

	int so = (int)std::clamp(m_RenderStats.syncoffset, -UNITS, UNITS);
#if SYNC_OFFSET_EX
	m_SyncDevs.Add(so - m_Syncs.Last());
#endif
	m_Syncs.Add(so);

	if (m_bDoubleFrames) {
		if (rtEnd < rtClock) {
			m_RenderStats.dropped2++;
			return S_FALSE; // skip frame
		}

		rtStart += rtFrameDur / 2;

		hr = Render(2, rtStart);
		m_pFilter->m_DrawStats.Add(GetPreciseTick());
		if (m_pFilter->m_filterState == State_Running) {
			m_pFilter->StreamTime(rtClock);
		}

		m_RenderStats.syncoffset = rtClock - rtStart;

		so = (int)std::clamp(m_RenderStats.syncoffset, -UNITS, UNITS);
#if SYNC_OFFSET_EX
		m_SyncDevs.Add(so - m_Syncs.Last());
#endif
		m_Syncs.Add(so);
	}

	return hr;
}

HRESULT CDX11VideoProcessor::CopySample(IMediaSample* pSample)
{
	CheckPointer(GetSwapChain, E_FAIL);

	uint64_t tick = GetPreciseTick();

	// Get frame type
	m_SampleFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE; // Progressive
	m_bDoubleFrames = false;
	if (m_bInterlaced) {
		if (Com::SComQIPtr<IMediaSample2> pMS2 = pSample) {
			AM_SAMPLE2_PROPERTIES props;
			if (SUCCEEDED(pMS2->GetProperties(sizeof(props), (BYTE*)&props))) {
				if ((props.dwTypeSpecificFlags & AM_VIDEO_FLAG_WEAVE) == 0) {
					if (props.dwTypeSpecificFlags & AM_VIDEO_FLAG_FIELD1FIRST) {
						m_SampleFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST; // Top-field first
					} else {
						m_SampleFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST; // Bottom-field first
					}
					m_bDoubleFrames = MPC_SETTINGS->bDeintDouble && m_D3D11VP.IsReady();
				}
			}
		}
	}

	HRESULT hr = S_OK;
	m_FieldDrawn = 0;
	bool updateStats = false;

	m_hdr10 = {};
	if (Com::SComQIPtr<IMediaSideData> pMediaSideData = pSample) {
		if (MPC_SETTINGS->bHdrPassthrough && SourceIsPQorHLG()) {
			MediaSideDataHDR* hdr = nullptr;
			size_t size = 0;
			hr = pMediaSideData->GetSideData(IID_MediaSideDataHDR, (const BYTE**)&hdr, &size);
			if (SUCCEEDED(hr) && size == sizeof(MediaSideDataHDR)) {
				m_hdr10.bValid = true;

				m_hdr10.hdr10.RedPrimary[0]   = static_cast<UINT16>(std::lround(hdr->display_primaries_x[2] * 50000.0));
				m_hdr10.hdr10.RedPrimary[1]   = static_cast<UINT16>(std::lround(hdr->display_primaries_y[2] * 50000.0));
				m_hdr10.hdr10.GreenPrimary[0] = static_cast<UINT16>(std::lround(hdr->display_primaries_x[0] * 50000.0));
				m_hdr10.hdr10.GreenPrimary[1] = static_cast<UINT16>(std::lround(hdr->display_primaries_y[0] * 50000.0));
				m_hdr10.hdr10.BluePrimary[0]  = static_cast<UINT16>(std::lround(hdr->display_primaries_x[1] * 50000.0));
				m_hdr10.hdr10.BluePrimary[1]  = static_cast<UINT16>(std::lround(hdr->display_primaries_y[1] * 50000.0));
				m_hdr10.hdr10.WhitePoint[0]   = static_cast<UINT16>(std::lround(hdr->white_point_x * 50000.0));
				m_hdr10.hdr10.WhitePoint[1]   = static_cast<UINT16>(std::lround(hdr->white_point_y * 50000.0));

				m_hdr10.hdr10.MaxMasteringLuminance = static_cast<UINT>(std::lround(hdr->max_display_mastering_luminance * 10000.0));
				m_hdr10.hdr10.MinMasteringLuminance = static_cast<UINT>(std::lround(hdr->min_display_mastering_luminance * 10000.0));
			}

			MediaSideDataHDRContentLightLevel* hdrCLL = nullptr;
			size = 0;
			hr = pMediaSideData->GetSideData(IID_MediaSideDataHDRContentLightLevel, (const BYTE**)&hdrCLL, &size);
			if (SUCCEEDED(hr) && size == sizeof(MediaSideDataHDRContentLightLevel)) {
				m_hdr10.hdr10.MaxContentLightLevel      = hdrCLL->MaxCLL;
				m_hdr10.hdr10.MaxFrameAverageLightLevel = hdrCLL->MaxFALL;
			}
		}

		size_t size = 0;
		MediaSideData3DOffset* offset = nullptr;
		hr = pMediaSideData->GetSideData(IID_MediaSideData3DOffset, (const BYTE**)&offset, &size);
		if (SUCCEEDED(hr) && size == sizeof(MediaSideData3DOffset) && offset->offset_count > 0 && offset->offset[0]) {
			m_nStereoSubtitlesOffsetInPixels = offset->offset[0];
		}

		if (m_srcParams.CSType == CS_YUV && (MPC_SETTINGS->bHdrPreferDoVi || !SourceIsPQorHLG())) {
			MediaSideDataDOVIMetadata* pDOVIMetadata = nullptr;
			hr = pMediaSideData->GetSideData(IID_MediaSideDataDOVIMetadata, (const BYTE**)&pDOVIMetadata, &size);
			if (SUCCEEDED(hr) && size == sizeof(MediaSideDataDOVIMetadata) && CheckDoviMetadata(pDOVIMetadata, 1)) {

				const bool bYCCtoRGBChanged = !m_PSConvColorData.bEnable ||
					(memcmp(
						&m_Dovi.msd.ColorMetadata.ycc_to_rgb_matrix,
						&pDOVIMetadata->ColorMetadata.ycc_to_rgb_matrix,
						sizeof(MediaSideDataDOVIMetadata::ColorMetadata.ycc_to_rgb_matrix) + sizeof(MediaSideDataDOVIMetadata::ColorMetadata.ycc_to_rgb_offset)
					) != 0);
				const bool bRGBtoLMSChanged =
					(memcmp(
						&m_Dovi.msd.ColorMetadata.rgb_to_lms_matrix,
						&pDOVIMetadata->ColorMetadata.rgb_to_lms_matrix,
						sizeof(MediaSideDataDOVIMetadata::ColorMetadata.rgb_to_lms_matrix)
					) != 0);
				const bool bMappingCurvesChanged = !m_pDoviCurvesConstantBuffer ||
					(memcmp(
						&m_Dovi.msd.Mapping.curves,
						&pDOVIMetadata->Mapping.curves,
						sizeof(MediaSideDataDOVIMetadata::Mapping.curves)
					) != 0);
				const bool bMasteringLuminanceChanged = m_Dovi.msd.ColorMetadata.source_max_pq != pDOVIMetadata->ColorMetadata.source_max_pq
					|| m_Dovi.msd.ColorMetadata.source_min_pq != pDOVIMetadata->ColorMetadata.source_min_pq;

				bool bMMRChanged = false;
				if (bMappingCurvesChanged) {
					bool has_mmr = false;
					for (const auto& curve : pDOVIMetadata->Mapping.curves) {
						for (uint8_t i = 0; i < (curve.num_pivots - 1); i++) {
							if (curve.mapping_idc[i] == 1) {
								has_mmr = true;
								break;
							}
						}
					}
					if (m_Dovi.bHasMMR != has_mmr) {
						m_Dovi.bHasMMR = has_mmr;
						m_pDoviCurvesConstantBuffer = nullptr;
						bMMRChanged = true;
					}
				}

				memcpy(&m_Dovi.msd, pDOVIMetadata, sizeof(MediaSideDataDOVIMetadata));
				const bool doviStateChanged = !m_Dovi.bValid;
				m_Dovi.bValid = true;

				if (bMasteringLuminanceChanged) {
					// based on libplacebo source code
					constexpr float
						PQ_M1 = 2610.f / (4096.f * 4.f),
						PQ_M2 = 2523.f / 4096.f * 128.f,
						PQ_C1 = 3424.f / 4096.f,
						PQ_C2 = 2413.f / 4096.f * 32.f,
						PQ_C3 = 2392.f / 4096.f * 32.f;

					auto pl_hdr_rescale = [PQ_M2, PQ_C1, PQ_C2, PQ_C3, PQ_M1](float x) {
						x = powf(x, 1.0f / PQ_M2);
						x = fmaxf(x - PQ_C1, 0.0f) / (PQ_C2 - PQ_C3 * x);
						x = powf(x, 1.0f / PQ_M1);
						x *= 10000.0f;

						return x;
					};

					m_DoviMaxMasteringLuminance = static_cast<UINT>(pl_hdr_rescale(m_Dovi.msd.ColorMetadata.source_max_pq / 4095.f) * 10000.0);
					m_DoviMinMasteringLuminance = static_cast<UINT>(pl_hdr_rescale(m_Dovi.msd.ColorMetadata.source_min_pq / 4095.f) * 10000.0);
				}

				if (m_D3D11VP.IsReady()) {
					InitMediaType(&m_pFilter->m_inputMT);
					UpdateStatsStatic();
				}
				else if (doviStateChanged) {
					UpdateStatsStatic();
				}

				if (bYCCtoRGBChanged) {
					CLog::LogF(LOGINFO,"CDX11VideoProcessor::CopySample() : DoVi ycc_to_rgb_matrix is changed");
					//SetShaderConvertColorParams();
				}
				if (bRGBtoLMSChanged || bMMRChanged) {
					DLogIf(bRGBtoLMSChanged, "CDX11VideoProcessor::CopySample() : DoVi rgb_to_lms_matrix is changed");
					DLogIf(bMMRChanged, "CDX11VideoProcessor::CopySample() : DoVi has_mmr is changed");
					//UpdateConvertColorShader();
				}
				if (bMappingCurvesChanged) {
					if (m_Dovi.bHasMMR) {
						//hr = SetShaderDoviCurves();
					} else {
						//hr = SetShaderDoviCurvesPoly();
					}
				}

				if (doviStateChanged && !SourceIsPQorHLG()) {
					Init(m_hWnd);

					m_srcVideoTransferFunction = 0;
					InitMediaType(&m_pFilter->m_inputMT);
				}
			}
		}
	}

	if (Com::SComQIPtr<IMediaSampleD3D11> pMSD3D11 = pSample) {
		if (m_iSrcFromGPU != 11) {
			m_iSrcFromGPU = 11;
			updateStats = true;
		}

		Com::SComQIPtr<ID3D11Texture2D> pD3D11Texture2D;
		UINT ArraySlice = 0;
		hr = pMSD3D11->GetD3D11Texture(0, &pD3D11Texture2D, &ArraySlice);
		if (FAILED(hr)) {
			CLog::LogF(LOGINFO,"CDX11VideoProcessor::CopySample() : GetD3D11Texture() failed with error {}", WToA(HR2Str(hr)).c_str());
			return hr;
		}

		D3D11_TEXTURE2D_DESC desc = {};
		pD3D11Texture2D->GetDesc(&desc);
		if (desc.Format != m_srcDXGIFormat) {
			return E_UNEXPECTED;
		}

		// here should be used CopySubresourceRegion instead of CopyResource
		D3D11_BOX srcBox = { 0, 0, 0, m_srcWidth, m_srcHeight, 1 };
		if (m_D3D11VP.IsReady()) {
			m_pDeviceContext.Get()->CopySubresourceRegion(m_D3D11VP.GetNextInputTexture(m_SampleFormat), 0, 0, 0, 0, pD3D11Texture2D, ArraySlice, &srcBox);
		} else {
			m_pDeviceContext.Get()->CopySubresourceRegion(m_TexSrcVideo.pTexture.Get(), 0, 0, 0, 0, pD3D11Texture2D, ArraySlice, &srcBox);
		}
	}
	else {
		if (m_iSrcFromGPU != 0) {
			m_iSrcFromGPU = 0;
			updateStats = true;
		}

		BYTE* data = nullptr;
		const long size = pSample->GetActualDataLength();
		if (size > 0 && S_OK == pSample->GetPointer(&data)) {
			// do not use UpdateSubresource for D3D11 VP here
			// because it can cause green screens and freezes on some configurations
			hr = MemCopyToTexSrcVideo(data, m_srcPitch);
			if (m_D3D11VP.IsReady()) {
				// ID3D11VideoProcessor does not use textures with D3D11_CPU_ACCESS_WRITE flag
				m_pDeviceContext.Get()->CopyResource(m_D3D11VP.GetNextInputTexture(m_SampleFormat), m_TexSrcVideo.pTexture.Get());
			}
		}
	}

	if (updateStats) {
		UpdateStatsStatic();
	}

	m_RenderStats.copyticks = GetPreciseTick() - tick;

	return hr;
}

HRESULT CDX11VideoProcessor::Render(int field, const REFERENCE_TIME frameStartTime)
{
	//lock kodi gui
	std::unique_lock<CCriticalSection> lock(CServiceBroker::GetWinSystem()->GetGfxContext());

	CheckPointer(m_TexSrcVideo.pTexture, E_FAIL);
	CheckPointer(GetSwapChain, E_FAIL);

	if (CStreamsManager::Get() && CStreamsManager::Get()->SubtitleManager)
		CStreamsManager::Get()->SubtitleManager->SetTime(frameStartTime + m_pFilter->m_rtStartTime);

	if (!g_application.GetComponent<CApplicationPlayer>()->IsRenderingVideo())
	{
		// Configure Render Manager
		float fps;
		fps = 10000000.0 / m_rtAvgTimePerFrame;
		if (CStreamsManager::Get()->SubtitleManager)
			CStreamsManager::Get()->SubtitleManager->SetTimePerFrame(m_rtAvgTimePerFrame);
		g_application.GetComponent<CApplicationPlayer>()->Configure(m_srcRectWidth, m_srcRectHeight, m_srcAspectRatioX, m_srcAspectRatioY, fps, 0);
		CLog::Log(LOGINFO, "{} Render manager configured (FPS: {}) {} {} {} {}", __FUNCTION__, fps, m_srcRectWidth, m_srcRectHeight, m_srcAspectRatioX, m_srcAspectRatioY);
	}
	else
	{
		if (!m_bDsplayerNotified)
		{
			m_bDsplayerNotified = true;
			CDSPlayer::PostMessage(new CDSMsg(CDSMsg::PLAYER_PLAYBACK_STARTED),false);
			CLog::Log(LOGINFO, "{} DSPLAYER notify playback started sent",__FUNCTION__);
		}
	}
	if (!g_application.GetComponent<CApplicationPlayer>()->IsRenderingVideo())
		return E_FAIL;

	if (field) {
		m_FieldDrawn = field;
	}


	HRESULT hr = S_OK;

	uint64_t tick1 = GetPreciseTick();
	float fColor[4];
	CD3DHelper::XMStoreColor(fColor, KODI::UTILS::COLOR::NONE);
	CRect src,dst, vw;
	Com::SmartRect oldrect;
	oldrect = m_videoRect;
	CMPCVRRenderer::Get()->GetVideoRect(src, dst, vw);
	m_renderRect = Com::SmartRect(vw.x1,vw.y1, vw.x2,vw.y2);
	m_videoRect = Com::SmartRect(dst.x1, dst.y1, dst.x2, dst.y2);
	m_windowRect = Com::SmartRect(vw.x1, vw.y1, vw.x2, vw.y2);
	if (oldrect != m_videoRect)
	{
		UpdateTexures();
	}
	//ID3D11RenderTargetView* pRTView = CMPCVRRenderer::Get()->GetIntermediateTarget().GetRenderTarget();
	//DX::DeviceResources::Get()->GetImmediateContext()->ClearRenderTargetView(pRTView, fColor);
	//DX::DeviceResources::Get()->GetImmediateContext()->ClearDepthStencilView(DX::DeviceResources::Get()->GetDSV(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0, 0);

	if (!m_renderRect.IsRectEmpty()) {
		hr = Process(CMPCVRRenderer::Get()->GetIntermediateTarget().Get(), m_srcRect, m_videoRect, m_FieldDrawn == 2);
	}
	return hr;
	//m_pDeviceContext->OMSetRenderTargets(1, &pRTView, nullptr);

	
	if (!m_pPSHalfOUtoInterlace) {
		DrawSubtitles(CMPCVRRenderer::Get()->GetIntermediateTarget().Get());
	}


	uint64_t tick3 = GetPreciseTick();
	m_RenderStats.paintticks = tick3 - tick1;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain4;
	//temporary added i think we will only do hdr manipulation from kodi
	const auto bHdrOutput = m_bHdrPassthroughSupport && MPC_SETTINGS->bHdrPassthrough && (SourceIsHDR() || MPC_SETTINGS->bVPUseRTXVideoHDR);
	if (bHdrOutput && SUCCEEDED(GetSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain4)))) {
		if (m_hdr10.bValid) {
			if (m_DoviMaxMasteringLuminance > m_hdr10.hdr10.MaxMasteringLuminance) {
				m_hdr10.hdr10.MaxMasteringLuminance = m_DoviMaxMasteringLuminance;
			}
			if (m_DoviMinMasteringLuminance && m_DoviMinMasteringLuminance != m_hdr10.hdr10.MinMasteringLuminance) {
				m_hdr10.hdr10.MinMasteringLuminance = m_DoviMinMasteringLuminance;
			}
		}
		
		const DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
		if (m_currentSwapChainColorSpace != colorSpace) {
			if (m_hdr10.bValid) {
				hr = swapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &m_hdr10.hdr10);
				DLogIf(FAILED(hr), "CDX11VideoProcessor::Render() : SetHDRMetaData(hdr) failed with error %s", WToA(HR2Str(hr)));

				m_lastHdr10 = m_hdr10;
				UpdateStatsStatic();
			} else if (m_lastHdr10.bValid) {
				hr = swapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &m_lastHdr10.hdr10);
				DLogIf(FAILED(hr), "CDX11VideoProcessor::Render() : SetHDRMetaData(lastHdr) failed with error %s", WToA(HR2Str(hr)));
			} else {
				m_lastHdr10.bValid = true;

				m_lastHdr10.hdr10.RedPrimary[0]   = 34000; // Display P3 primaries
				m_lastHdr10.hdr10.RedPrimary[1]   = 16000;
				m_lastHdr10.hdr10.GreenPrimary[0] = 13250;
				m_lastHdr10.hdr10.GreenPrimary[1] = 34500;
				m_lastHdr10.hdr10.BluePrimary[0]  = 7500;
				m_lastHdr10.hdr10.BluePrimary[1]  = 3000;
				m_lastHdr10.hdr10.WhitePoint[0]   = 15635;
				m_lastHdr10.hdr10.WhitePoint[1]   = 16450;
				m_lastHdr10.hdr10.MaxMasteringLuminance = m_DoviMaxMasteringLuminance ? m_DoviMaxMasteringLuminance : 1000 * 10000; // 1000 nits
				m_lastHdr10.hdr10.MinMasteringLuminance = m_DoviMinMasteringLuminance ? m_DoviMinMasteringLuminance : 50;           // 0.005 nits
				hr = swapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &m_lastHdr10.hdr10);
				DLogIf(FAILED(hr), "CDX11VideoProcessor::Render() : SetHDRMetaData(Display P3 standard) failed with error %s", WToA(HR2Str(hr)));

				UpdateStatsStatic();
			}

			UINT colorSpaceSupport = 0;
			if (SUCCEEDED(swapChain4->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport))
					&& (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) {
				hr = swapChain4->SetColorSpace1(colorSpace);
				DLogIf(FAILED(hr), "CDX11VideoProcessor::Render() : SetColorSpace1() failed with error %s", WToA(HR2Str(hr)));
				if (SUCCEEDED(hr)) {
					m_currentSwapChainColorSpace = colorSpace;
				}
			}
		} else if (m_hdr10.bValid) {
			if (memcmp(&m_hdr10.hdr10, &m_lastHdr10.hdr10, sizeof(m_hdr10.hdr10)) != 0) {
				hr = swapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &m_hdr10.hdr10);
				DLogIf(FAILED(hr), "CDX11VideoProcessor::Render() : SetHDRMetaData(hdr) failed with error %s", WToA(HR2Str(hr)));

				m_lastHdr10 = m_hdr10;
				UpdateStatsStatic();
			}
		}
	}
	Microsoft::WRL::ComPtr<IDXGIOutput>    pDXGIOutput;
	DX::DeviceResources::Get()->GetOutput(&pDXGIOutput);
	if (MPC_SETTINGS->bVBlankBeforePresent && pDXGIOutput) {
		hr = pDXGIOutput->WaitForVBlank();
		DLogIf(FAILED(hr), "WaitForVBlank failed with error %s", WToA(HR2Str(hr)));
	}

	SyncFrameToStreamTime(frameStartTime);
	
	Microsoft::WRL::ComPtr<ID3D11CommandList> pCommandList;
	if (FAILED(m_pDeviceContext->FinishCommandList(1, &pCommandList)))
	{
		CLog::LogF(LOGERROR, "failed to finish command queue.");
		return E_FAIL;
	}
	else
	{
		D3DSetDebugName(pCommandList.Get(), "CommandList mpc deferred context");
		DX::DeviceResources::Get()->GetImmediateContext()->ExecuteCommandList(pCommandList.Get(), 0);
	}

	
		
	
	g_bPresent = true;
	//hr = GetSwapChain->Present(1, 0);
	g_bPresent = false;
	

	m_RenderStats.presentticks = GetPreciseTick() - tick3;

	if (hr == DXGI_ERROR_INVALID_CALL && m_pFilter->m_bIsD3DFullscreen) {
		InitSwapChain();
	}

	return hr;
}

void CDX11VideoProcessor::UpdateTexures()
{
	if (!m_srcWidth || !m_srcHeight)
		return;
	HRESULT hr = S_OK;
	
	if (m_D3D11VP.IsReady()) {
		if (MPC_SETTINGS->bVPScaling) {
			Com::SmartSize texsize = m_renderRect.Size();
			hr = m_TexConvertOutput.CheckCreate(GetDevice, m_D3D11OutputFmt, texsize.cx, texsize.cy, Tex2D_DefaultShaderRTarget);
		} else {
			hr = m_TexConvertOutput.CheckCreate(GetDevice, m_D3D11OutputFmt, m_srcRectWidth, m_srcRectHeight, Tex2D_DefaultShaderRTarget);
		}
	}
	else {
		hr = m_TexConvertOutput.CheckCreate(GetDevice, m_InternalTexFmt, m_srcRectWidth, m_srcRectHeight, Tex2D_DefaultShaderRTarget);
	}
}

void CDX11VideoProcessor::UpdateBitmapShader()
{
	if (m_bHdrDisplayModeEnabled
			&& (SourceIsPQorHLG() || MPC_SETTINGS->bVPUseRTXVideoHDR)) {
		std::string resid;
		float SDR_peak_lum;
		switch (MPC_SETTINGS->iHdrOsdBrightness) {
		default:
			resid = IDF_PS_11_CONVERT_BITMAP_TO_PQ;
			SDR_peak_lum = 100;
			break;
		case 1:
			resid = IDF_PS_11_CONVERT_BITMAP_TO_PQ1;
			SDR_peak_lum = 50;
			break;
		case 2:
			resid = IDF_PS_11_CONVERT_BITMAP_TO_PQ2;
			SDR_peak_lum = 30;
			break;
		}
		m_pPS_BitmapToFrame= nullptr;
		EXECUTE_ASSERT(S_OK == CreatePShaderFromResource(&m_pPS_BitmapToFrame, resid));
		m_dwStatsTextColor = TransferPQ(D3DCOLOR_XRGB(255, 255, 255), SDR_peak_lum);
	}
	else {
		
		m_dwStatsTextColor = D3DCOLOR_XRGB(255, 255, 255);
	}
}

HRESULT CDX11VideoProcessor::D3D11VPPass(ID3D11Texture2D* pRenderTarget, const Com::SmartRect& srcRect, const Com::SmartRect& dstRect, const bool second)
{
	HRESULT hr = m_D3D11VP.SetRectangles(srcRect, dstRect);

	hr = m_D3D11VP.Process(pRenderTarget, m_SampleFormat, second);
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO, "CDX11VideoProcessor::ProcessD3D11() : m_D3D11VP.Process() failed with error {}", WToA(HR2Str(hr)).c_str());
	}

	return hr;
}

void CDX11VideoProcessor::DrawSubtitles(ID3D11Texture2D* pRenderTarget)
{
	//draw subs
	// Subtitle drawing

	if (CStreamsManager::Get()->SubtitleManager)
	{
		ID3D11RenderTargetView* pRenderTargetView;
		HRESULT hr = GetDevice->CreateRenderTargetView(pRenderTarget, nullptr, &pRenderTargetView);
		D3DSetDebugName(pRenderTargetView, "MPC RenderTarget DrawSubtitles");

		if (SUCCEEDED(hr)) {
			const Com::SmartRect rSrcPri(Com::SmartPoint(0, 0), m_windowRect.Size());
			const Com::SmartRect rDstVid(m_videoRect);
			const auto rtStart = m_pFilter->m_rtStartTime + m_rtStart;

			// Set render target and shaders
			
			m_pDeviceContext->OMSetRenderTargets(1, &pRenderTargetView, nullptr);
			m_pDeviceContext->IASetInputLayout(m_pVSimpleInputLayout.Get());
			m_pDeviceContext->VSSetShader(m_pVS_Simple.Get(), nullptr, 0);
			m_pDeviceContext->PSSetShader(m_pPS_BitmapToFrame.Get(), nullptr, 0);

			Com::SmartRect pSrc, pDst;
			//sending the devicecontext to the subtitlemanager he will draw directly with it
			CStreamsManager::Get()->SubtitleManager->AlphaBlt(m_pDeviceContext.Get(), pSrc, pDst, m_windowRect);

			pRenderTargetView->Release();
		}
	}
}

HRESULT CDX11VideoProcessor::Process(ID3D11Texture2D* pRenderTarget, const Com::SmartRect& srcRect, const Com::SmartRect& dstRect, const bool second)
{
	HRESULT hr = S_OK;
	m_bDitherUsed = false;
	int rotation = m_iRotation;

	Com::SmartRect rSrc = srcRect;
	Tex2D_t* pInputTexture = nullptr;

	if (m_D3D11VP.IsReady()) {
		if (!(MPC_SETTINGS->iSwapEffect == SWAPEFFECT_Discard && (m_VendorId == PCIV_AMD || m_VendorId == PCIV_Intel))) {
			const bool bNeedShaderTransform =
				(m_TexConvertOutput.desc.Width != dstRect.Width() || m_TexConvertOutput.desc.Height != dstRect.Height() || m_bFlip
				|| dstRect.right > m_windowRect.right || dstRect.bottom > m_windowRect.bottom);

			if (!bNeedShaderTransform) {
				m_bVPScalingUseShaders = false;
				hr = D3D11VPPass(pRenderTarget, rSrc, dstRect, second);

				return hr;
			}
		}

		Com::SmartRect rect(0, 0, m_TexConvertOutput.desc.Width, m_TexConvertOutput.desc.Height);
		hr = D3D11VPPass(m_TexConvertOutput.pTexture.Get(), rSrc, rect, second);
		pInputTexture = &m_TexConvertOutput;
		rSrc = rect;
		rotation = 0;
	}
	else if (m_PSConvColorData.bEnable) {
		//ConvertColorPass(m_TexConvertOutput.pTexture.Get());
		pInputTexture = &m_TexConvertOutput;
		rSrc.SetRect(0, 0, m_TexConvertOutput.desc.Width, m_TexConvertOutput.desc.Height);
	}
	else {
		pInputTexture = &m_TexSrcVideo;
	}
	
	/*pl_frame inputFrame{};
	inputFrame.repr = GetPlaceboColorRepr((DXVA_VideoPrimaries)m_srcExFmt.VideoPrimaries, (DXVA_NominalRange)m_srcExFmt.NominalRange);
	pl_color_space col{};
	col.hdr = {};
	col.primaries = PL_COLOR_PRIM_BT_709;
	col.transfer = PL_COLOR_TRC_BT_1886;
	inputFrame.color = col;
	inputFrame.*/

	/*if (CMPCVRRenderer::Get()->CreateInputTarget(pInputTexture->desc.Width, pInputTexture->desc.Height, pInputTexture->desc.Format))
	{
		
		m_pDeviceContext->CopyResource(CMPCVRRenderer::Get()->GetInputTexture(true, m_pFilter->m_rtStartTime + m_rtStart).Get(), pInputTexture->pTexture.Get());
		Microsoft::WRL::ComPtr<ID3D11CommandList> pCommandList;
		if (FAILED(m_pDeviceContext->FinishCommandList(true, &pCommandList)))
		{
			CLog::LogF(LOGERROR, "failed to finish command queue.");
			return E_FAIL;
		}
		else
		{
			D3DSetDebugName(pCommandList.Get(), "CommandList mpc deferred context");
			DX::DeviceResources::Get()->GetImmediateContext()->ExecuteCommandList(pCommandList.Get(), false);
		}
		return S_OK;
	}*/
	return E_FAIL;
#if 0
	if (numSteps) {
		UINT step = 0;
		Tex2D_t* pTex = m_TexsPostScale.GetFirstTex();
		ID3D11Texture2D* pRT = pTex->pTexture.Get();

		auto StepSetting = [&]() {
			step++;
			pInputTexture = pTex;
			if (step < numSteps) {
				pTex = m_TexsPostScale.GetNextTex();
				pRT = pTex->pTexture.Get();
			} else {
				pRT = pRenderTarget;
			}
		};

		Com::SmartRect rect;
		rect.IntersectRect(dstRect, Com::SmartRect(0, 0, pTex->desc.Width, pTex->desc.Height));

		if (m_D3D11VP.IsReady()) {
			m_bVPScalingUseShaders = rSrc.Width() != dstRect.Width() || rSrc.Height() != dstRect.Height();
		}

		if (rSrc != dstRect) {
			hr = ResizeShaderPass(*pInputTexture, pRT, rSrc, dstRect, rotation);
		} else {
			pTex = pInputTexture; // Hmm
		}

		if (m_pPSCorrection) {
			StepSetting();
			hr = TextureCopyRect(*pInputTexture, pRT, rect, rect, m_pPSCorrection.Get(), nullptr, 0, false);
		}

		if (m_pPostScaleShaders.size()) {
			static __int64 counter = 0;
			static long start = GetTickCount();

			long stop = GetTickCount();
			long diff = stop - start;
			if (diff >= 10 * 60 * 1000) {
				start = stop;    // reset after 10 min (ps float has its limits in both range and accuracy)
			}

			PS_EXTSHADER_CONSTANTS ConstData = {
				{1.0f / pTex->desc.Width, 1.0f / pTex->desc.Height },
				{(float)pTex->desc.Width, (float)pTex->desc.Height},
				counter++,
				(float)diff / 1000,
				0, 0
			};
			m_pDeviceContext->UpdateSubresource(m_pPostScaleConstants.Get(), 0, nullptr, &ConstData, 0, 0);

			for (UINT idx = 0; idx < m_pPostScaleShaders.size(); idx++) {
				StepSetting();
				hr = TextureCopyRect(*pInputTexture, pRT, rect, rect, m_pPostScaleShaders[idx].shader.Get(), m_pPostScaleConstants.Get(), 0, false);
			}
		}

		if (m_pPSHalfOUtoInterlace) {
			DrawSubtitles(pRT);

			StepSetting();
			FLOAT ConstData[] = {
				(float)pTex->desc.Height, 0,
				(float)dstRect.top / pTex->desc.Height, (float)dstRect.bottom / pTex->desc.Height,
			};
			D3D11_MAPPED_SUBRESOURCE mr;
			hr = m_pDeviceContext->Map(m_pHalfOUtoInterlaceConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mr);
			if (SUCCEEDED(hr)) {
				memcpy(mr.pData, &ConstData, sizeof(ConstData));
				m_pDeviceContext->Unmap(m_pHalfOUtoInterlaceConstantBuffer.Get(), 0);
			}
			hr = TextureCopyRect(*pInputTexture, pRT, rect, rect, m_pPSHalfOUtoInterlace.Get(), m_pHalfOUtoInterlaceConstantBuffer.Get(), 0, false);
		}

		if (m_bFinalPass) {
			StepSetting();
			hr = FinalPass(*pTex, pRT, rect, rect);
			m_bDitherUsed = true;
		}
	}
	else {
		hr = ResizeShaderPass(*pInputTexture, pRenderTarget, rSrc, dstRect, rotation);
	}
#endif
	if (FAILED(hr))
		CLog::Log(LOGERROR,"{} : failed with error {}",__FUNCTION__, WToA(HR2Str(hr)));

	return hr;
}

void CDX11VideoProcessor::SetVideoRect(const Com::SmartRect& videoRect)
{
	m_videoRect = videoRect;
	UpdateRenderRect();
	UpdateTexures();
}

HRESULT CDX11VideoProcessor::SetWindowRect(const Com::SmartRect& windowRect)
{
	m_windowRect = windowRect;
	UpdateRenderRect();

	return S_OK;
}

void CDX11VideoProcessor::Reset(bool bForceWindowed)
{
	CAutoLock cRendererLock(&m_pFilter->m_RendererLock);
	ReleaseDevice();
	CMPCVRRenderer::Get()->Reset();

	m_pDXGIFactory1 = nullptr;
	m_bKodiResizeBuffers = true;
	
}

HRESULT CDX11VideoProcessor::Reset()
{
	CLog::LogF(LOGINFO,"CDX11VideoProcessor::Reset()");

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
						m_pFilter->Init(true);
					} else {
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

HRESULT CDX11VideoProcessor::GetCurentImage(long *pDIBImage)
{
	Com::SmartSize framesize(m_srcRectWidth, m_srcRectHeight);
	if (m_srcAnamorphic) {
		framesize.cx = MulDiv(framesize.cy, m_srcAspectRatioX, m_srcAspectRatioY);
	}
	if (m_iRotation == 90 || m_iRotation == 270) {
		std::swap(framesize.cx, framesize.cy);
	}
	const auto w = framesize.cx;
	const auto h = framesize.cy;
	const Com::SmartRect imageRect(0, 0, w, h);

	BITMAPINFOHEADER* pBIH = (BITMAPINFOHEADER*)pDIBImage;
	ZeroMemory(pBIH, sizeof(BITMAPINFOHEADER));
	pBIH->biSize      = sizeof(BITMAPINFOHEADER);
	pBIH->biWidth     = w;
	pBIH->biHeight    = h;
	pBIH->biPlanes    = 1;
	pBIH->biBitCount  = 32;
	pBIH->biSizeImage = DIBSIZE(*pBIH);

	UINT dst_pitch = pBIH->biSizeImage / h;

	HRESULT hr = S_OK;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> pRGB32Texture2D;
	D3D11_TEXTURE2D_DESC texdesc = CreateTex2DDesc(DXGI_FORMAT_B8G8R8X8_UNORM, w, h, Tex2D_DefaultRTarget);

	hr = GetDevice->CreateTexture2D(&texdesc, nullptr, &pRGB32Texture2D);
	if (FAILED(hr)) {
		return hr;
	}

	const auto backupVidRect = m_videoRect;
	const auto backupWndRect = m_windowRect;
	m_videoRect  = imageRect;
	m_windowRect = imageRect;
	UpdateTexures();

	auto pSub11CallBack = m_pFilter->m_pSub11CallBack;
	m_pFilter->m_pSub11CallBack = nullptr;

	hr = Process(pRGB32Texture2D.Get(), m_srcRect, imageRect, false);

	m_pFilter->m_pSub11CallBack = pSub11CallBack;

	m_videoRect  = backupVidRect;
	m_windowRect = backupWndRect;
	UpdateTexures();

	if (FAILED(hr)) {
		return hr;
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D> pRGB32Texture2D_Shared;
	texdesc = CreateTex2DDesc(DXGI_FORMAT_B8G8R8X8_UNORM, w, h, Tex2D_StagingRead);

	hr = GetDevice->CreateTexture2D(&texdesc, nullptr, &pRGB32Texture2D_Shared);
	if (FAILED(hr)) {
		return hr;
	}
	m_pDeviceContext->CopyResource(pRGB32Texture2D_Shared.Get(), pRGB32Texture2D.Get());

	D3D11_MAPPED_SUBRESOURCE mr = {};
	if (S_OK == m_pDeviceContext->Map(pRGB32Texture2D_Shared.Get(), 0, D3D11_MAP_READ, 0, &mr)) {
		CopyFrameAsIs(h, (BYTE*)(pBIH + 1), dst_pitch, (BYTE*)mr.pData + mr.RowPitch * (h - 1), -(int)mr.RowPitch);
		m_pDeviceContext->Unmap(pRGB32Texture2D_Shared.Get(), 0);
	} else {
		return E_FAIL;
	}

	return S_OK;
}

HRESULT CDX11VideoProcessor::GetDisplayedImage(BYTE **ppDib, unsigned* pSize)
{
	if (!GetSwapChain || !GetDevice || !m_pDeviceContext) {
		return E_ABORT;
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
	HRESULT hr = GetSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO,"CDX11VideoProcessor::GetDisplayedImage() failed with error {}", WToA(HR2Str(hr)).c_str());
		return hr;
	}

	D3D11_TEXTURE2D_DESC desc;
	pBackBuffer->GetDesc(&desc);

	if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format != DXGI_FORMAT_R10G10B10A2_UNORM) {
		CLog::LogF(LOGINFO,"CDX11VideoProcessor::GetDisplayedImage() backbuffer format not supported");
		return E_FAIL;
	}

	D3D11_TEXTURE2D_DESC desc2 = CreateTex2DDesc(desc.Format, desc.Width, desc.Height, Tex2D_StagingRead);
	Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture2DShared;
	hr = GetDevice->CreateTexture2D(&desc2, nullptr, &pTexture2DShared);
	if (FAILED(hr)) {
		CLog::LogF(LOGINFO,"CDX11VideoProcessor::GetDisplayedImage() failed with error {}", WToA(HR2Str(hr)).c_str());
		return hr;
	}

	m_pDeviceContext->CopyResource(pTexture2DShared.Get(), pBackBuffer.Get());

	*pSize = desc.Width * desc.Height * 4 + sizeof(BITMAPINFOHEADER);
	BYTE* p = (BYTE*)LocalAlloc(LMEM_FIXED, *pSize); // only this allocator can be used
	if (!p) {
		return E_OUTOFMEMORY;
	}

	BITMAPINFOHEADER* pBIH = (BITMAPINFOHEADER*)p;
	ZeroMemory(pBIH, sizeof(BITMAPINFOHEADER));
	pBIH->biSize      = sizeof(BITMAPINFOHEADER);
	pBIH->biWidth     = desc.Width;
	pBIH->biHeight    = desc.Height;
	pBIH->biBitCount  = 32;
	pBIH->biPlanes    = 1;
	pBIH->biSizeImage = DIBSIZE(*pBIH);

	UINT dst_pitch = pBIH->biSizeImage / desc.Height;

	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	hr = m_pDeviceContext->Map(pTexture2DShared.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
	if (SUCCEEDED(hr)) {
		if (desc2.Format == DXGI_FORMAT_R10G10B10A2_UNORM) {
			ConvertXRGB10toXRGB8(desc.Height, (BYTE*)(pBIH + 1), dst_pitch, (BYTE*)mappedResource.pData + mappedResource.RowPitch * (desc.Height - 1), -(int)mappedResource.RowPitch);
		}
		else {
			CopyFrameAsIs(desc.Height, (BYTE*)(pBIH + 1), dst_pitch, (BYTE*)mappedResource.pData + mappedResource.RowPitch * (desc.Height - 1), -(int)mappedResource.RowPitch);
		}
		m_pDeviceContext->Unmap(pTexture2DShared.Get(), 0);
		*ppDib = p;
	} else {
		LocalFree(p);
	}

	return hr;
}

void CDX11VideoProcessor::SetRotation(int value)
{
	m_iRotation = value;
	if (m_D3D11VP.IsReady()) {
		m_D3D11VP.SetRotation(static_cast<D3D11_VIDEO_PROCESSOR_ROTATION>(value / 90));
	}
}

void CDX11VideoProcessor::SetStereo3dTransform(int value)
{
	m_iStereo3dTransform = value;

	if (m_iStereo3dTransform == 1) {
		if (!m_pPSHalfOUtoInterlace) {
			CreatePShaderFromResource(&m_pPSHalfOUtoInterlace, IDF_PS_11_HALFOU_TO_INTERLACE);
		}
	}
	else {
		m_pPSHalfOUtoInterlace= nullptr;
	}
}

void CDX11VideoProcessor::Flush()
{
	if (m_D3D11VP.IsReady()) {
		m_D3D11VP.ResetFrameOrder();
	}

	m_rtStart = 0;
}

void CDX11VideoProcessor::UpdateStatsPresent()
{
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

void CDX11VideoProcessor::UpdateStatsStatic()
{
	if (m_srcParams.cformat) {
		m_strStatsHeader.Format(L"MPCVR modified for kodi with libplacebo", _CRT_WIDE(VERSION_STR));

		UpdateStatsInputFmt();

		m_strStatsVProc.assign(L"\nTexture sampler: ");
		
		if (MPC_SETTINGS->bD3D11TextureSampler == D3D11_VP)
			m_strStatsVProc.AppendFormat(L"D3D11 VP, from %s to %s", m_D3D11OutputFmt == DXGI_FORMAT_R10G10B10A2_UNORM ? L"YUV420P 10bit" : L"NV12 8 bit",DXGIFormatToString(m_D3D11OutputFmt));
		else if (MPC_SETTINGS->bD3D11TextureSampler == D3D11_INTERNAL_SHADERS)
			m_strStatsVProc.AppendFormat(L"Internal shaders, from %s to %s", m_D3D11OutputFmt == DXGI_FORMAT_R10G10B10A2_UNORM ? L"YUV420P 10bit" : L"NV12 8 bit", DXGIFormatToString(m_SwapChainFmt));
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
			} else if (MPC_SETTINGS->bConvertToSdr) {
				m_strStatsHDR.append(L"Convert to SDR");
			} else {
				m_strStatsHDR.append(L"Not used");
			}
		} else {
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
	if (val > 0 && val < 203)
		return true;
	else
		return false;
}
void CDX11VideoProcessor::SendStats(const struct pl_color_space csp, const struct pl_color_repr repr)
{
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
			
			if (hdr.max_luma >0)
				str.AppendFormat(L" HDR10: %.2g / %.0f cd/m�", hdr.min_luma,hdr.max_luma);

			if (hdr.max_cll >0)
				str.AppendFormat(L" MaxCLL: %.0f cd/m�", hdr.max_cll);

			if (hdr.max_fall > 0)
				str.AppendFormat(L" MaxFALL: %.0f cd/m�\n", hdr.max_fall);

			if (hdr.scene_max[0] || hdr.scene_max[1] || hdr.scene_max[2] || hdr.scene_avg)
				str.AppendFormat(L" HDR10+: MaxRGB: %.1f/%.1f/%.1f cd/m� Avg: %.1f cd/m�\n", (hdr.scene_max[0]*1000), (hdr.scene_max[1] * 1000), (hdr.scene_max[2] * 1000), (hdr.scene_avg*1000));

			if (hdr.max_pq_y && hdr.avg_pq_y)
			{
				str.AppendFormat(L"  PQ(Y): Max:%.2f cd/m� (%.2f%% PQ) ", hdr.max_pq_y, (float)(hdr.max_pq_y*100));
				str.AppendFormat(L"Avg:%.2f cd/m� (%.2f%% PQ) \n", hdr.avg_pq_y, (float)(hdr.avg_pq_y * 100));
			}
		}
	}
	CMPCVRRenderer::Get()->SetStats(str);
	
}

// IMFVideoProcessor

STDMETHODIMP CDX11VideoProcessor::SetProcAmpValues(DWORD dwFlags, DXVA2_ProcAmpValues *pValues)
{
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

void CDX11VideoProcessor::SetResolution()
{

	
}

bool CDX11VideoProcessor::ParentWindowProc(HWND hWnd, UINT uMsg, WPARAM* wParam, LPARAM* lParam, LRESULT* ret) const
{
	*ret = m_pFilter->OnReceiveMessage(hWnd, uMsg, (WPARAM)wParam, (LPARAM)lParam);
	return false;
}



void CDX11VideoProcessor::SetCallbackDevice()
{
	CLog::Log(LOGINFO, "{} setting callback", __FUNCTION__);
	if (!m_bCallbackDeviceIsSet && GetDevice && m_pFilter->m_pSub11CallBack) {
		m_bCallbackDeviceIsSet = SUCCEEDED(m_pFilter->m_pSub11CallBack->SetDevice11(GetDevice));
		CLog::Log(LOGINFO, "{} setting callback is set", __FUNCTION__);
	}
}
