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

#pragma once

#include <DXGI1_2.h>
#include <d3d9types.h>
#include <dxva2api.h>
#include <dxgi1_5.h>
#include <strmif.h>
#include <map>
#include <queue>
#include "IVideoRenderer.h"
#include "DX11Helper.h" 
#include "D3D11VP.h"
#include "D3D11Font.h"
#include "D3D11Geometry.h"
#include "VideoProcessor.h"
#include "Filters/RendererSettings.h"
/*kodi*/
#include "guilib/D3DResource.h"
#include "../IDSPlayer.h"

#include "libplacebo/log.h"
#include "libplacebo/renderer.h"
#include "libplacebo/d3d11.h"
#include <libplacebo/options.h>
#include "libplacebo/utils/frame_queue.h"
#include "libplacebo/utils/upload.h"
#include "libplacebo/colorspace.h"
#include "../../VideoRenderers/MPCVRRenderer.h"
#include "threads/Thread.h"

#define TEST_SHADER 0





class CVideoRendererInputPin;



class CDX11VideoProcessor
	: public CVideoProcessor,
	  public IDSRendererAllocatorCallback,
		public IMpcVRCallback, 
	  public CThread
{
public:
	// IDSRendererAllocatorCallback
	CRect GetActiveVideoRect() override { return CRect(); };
	bool IsEnteringExclusive() override { return false; }
	void EnableExclusive(bool bEnable) override {};
	void SetPixelShader() override {};
	void SetResolution() override;
	void SetPosition(CRect sourceRect, CRect videoRect, CRect viewRect) override {};
	bool ParentWindowProc(HWND hWnd, UINT uMsg, WPARAM* wParam, LPARAM* lParam, LRESULT* ret) const override;
	void DisplayChange(bool bExternalChange) override {};
	void Reset(bool bForceWindowed) override;

	ID3D11DeviceContext1* GetD3DContext() const { return m_pDeviceContext.Get(); }

	HRESULT GetPresentationTexture(ID3D11Texture2D** texture);
	void RenderRectChanged(CRect newRect);

private:
	friend class CVideoRendererInputPin;
	/*Kodi Specific*/
	D3D11_TEXTURE_SAMPLER m_pFinalTextureSampler;


	FrameQueue      m_processingQueue;
	FrameQueue      m_pFreeProcessingQueue;

	FrameQueue      m_presentationQueue;
	FrameQueue      m_pFreePresentationQueue;

	// THREAD HANDLES AND EVENTS
	HANDLE m_hUploadThread;
	HANDLE m_hProcessThread;

	HANDLE m_hProcessEvent;
	HANDLE m_hStopEvent;
	HANDLE m_hFlushEvent;
	HANDLE m_hResizeEvent;

	//count between update of osd
	int m_iPresCount;

	// Direct3D 11
	Microsoft::WRL::ComPtr<ID3D11DeviceContext1> m_pDeviceContext;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>   m_pVS_Simple;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>    m_pPS_BitmapToFrame;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>    m_pVSimpleInputLayout;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>   m_pSamplerPoint;
	Microsoft::WRL::ComPtr<ID3D11SamplerState>   m_pSamplerLinear;
	CD3DTexture m_pInputTexture;
#if TEST_SHADER
	Microsoft::WRL::ComPtr<ID3D11PixelShader>    m_pPS_TEST;
#endif
	HRESULT ConvertColorPass(ID3D11Texture2D* pRenderTarget);
	Tex11Video_t m_TexSrcVideo; // for copy of frame
	Tex2D_t m_TexConvertOutput;
	Tex2D_t m_TexResize;        // for intermediate result of two-pass resize

	// for GetAlignmentSize()
	struct Alignment_t {
		Tex11Video_t texture;
		ColorFormat_t cformat = {};
		LONG cx = {};
	} m_Alignment;

	// D3D11 Video Processor
	CD3D11VP m_D3D11VP;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pPSCorrection;
	Microsoft::WRL::ComPtr<ID3D11Buffer > m_pCorrectionConstants;
	const wchar_t* m_strCorrection = nullptr;

	// D3D11 Shader Video Processor
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pPSConvertColor;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pPSConvertColorDeint;
	struct {
		bool bEnable = false;
		ID3D11Buffer* pVertexBuffer = nullptr;
		ID3D11Buffer* pConstants = nullptr;
		void Release() {
			bEnable = false;
			SAFE_RELEASE(pVertexBuffer);
			SAFE_RELEASE(pConstants);
		}
	} m_PSConvColorData;

	Microsoft::WRL::ComPtr<ID3D11Buffer> m_pDoviCurvesConstantBuffer;

	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pPSHalfOUtoInterlace;

	DXGI_COLOR_SPACE_TYPE m_currentSwapChainColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

	// Input parameters
	DXGI_FORMAT m_srcDXGIFormat = DXGI_FORMAT_UNKNOWN;

	// D3D11 VP texture format
	DXGI_FORMAT m_D3D11OutputFmt = DXGI_FORMAT_UNKNOWN;

	// intermediate texture format
	DXGI_FORMAT m_InternalTexFmt = DXGI_FORMAT_B8G8R8A8_UNORM;

	// swap chain format
	DXGI_FORMAT m_SwapChainFmt = DXGI_FORMAT_B8G8R8A8_UNORM;
	UINT32 m_DisplayBitsPerChannel = 8;

	D3D11_VIDEO_FRAME_FORMAT m_SampleFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;

	Microsoft::WRL::ComPtr<IDXGIFactory1> m_pDXGIFactory1;

	bool m_bSubPicWasRendered = false;

	bool m_bDecoderDevice = false;
	bool m_bIsFullscreen = false;
	bool m_bHdrPassthroughSupport = false;
	bool m_bVPUseSuperRes = false; // but it is not exactly

	
	bool m_bHdrDisplaySwitching   = false; // switching HDR display in progress
	bool m_bHdrDisplayModeEnabled = false;
	bool m_bHdrAllowSwitchDisplay = true;
	UINT m_srcVideoTransferFunction = 0; // need a description or rename

	std::map<CStdStringW, BOOL> m_hdrModeSavedState;
	std::map<CStdStringW, BOOL> m_hdrModeStartState;

	struct HDRMetadata {
		DXGI_HDR_METADATA_HDR10 hdr10 = {};
		bool bValid = false;
	};
	HDRMetadata m_hdr10 = {};
	HDRMetadata m_lastHdr10 = {};

	UINT m_DoviMaxMasteringLuminance = 0;
	UINT m_DoviMinMasteringLuminance = 0;

	HMONITOR m_lastFullscreenHMonitor = nullptr;

	D3DCOLOR m_dwStatsTextColor = D3DCOLOR_XRGB(255, 255, 255);

	bool m_bCallbackDeviceIsSet = false;
	void SetCallbackDevice();

	bool m_bDsplayerNotified = false;
	bool m_bKodiResizeBuffers = true;
	Com::SmartRect m_destRect;

public:
	CDX11VideoProcessor(CMpcVideoRenderer* pFilter, HRESULT& hr);
	~CDX11VideoProcessor() override;

	static DWORD __stdcall UploadThread(LPVOID lpParameter);

	CMPCVRFrame ConvertSampleToFrame(IMediaSample* pSample);

	void UploadLoop();

	static DWORD __stdcall ProcessThread(LPVOID lpParameter);

	void ProcessLoop();
	void ProcessFrameLibplacebo(CMPCVRFrame& inputFrame, CMPCVRFrame& outputFrame);
	void ProcessFrameVP(CMPCVRFrame& inputFrame, CMPCVRFrame& outputFrame);


	HRESULT Init(const HWND hwnd, bool* pChangeDevice = nullptr) override;
	bool Initialized();

private:
	void ProcessFrame(IMediaSample* pSample, CMPCVRFrame &frame);
	void ResizeProcessFrame();
	void ReleaseVP();
	void ReleaseDevice();

	HRESULT CreatePShaderFromResource(ID3D11PixelShader** ppPixelShader, std::string resid);

	void UpdateTexParams(int cdepth);
	void UpdateRenderRect();

	HRESULT MemCopyToTexSrcVideo(const BYTE* srcData, const int srcPitch);

	bool Preferred10BitOutput() {
		return m_DisplayBitsPerChannel >= 10 && (m_InternalTexFmt == DXGI_FORMAT_R10G10B10A2_UNORM || m_InternalTexFmt == DXGI_FORMAT_R16G16B16A16_FLOAT);
	}

	/*added for kodi*/
	std::vector<CD3DPixelShader> m_pPixelShaders;

	pl_frame CreateFrame(DXVA2_ExtendedFormat pFormat, IMediaSample* pSample, int width, int height, FmtConvParams_t srcParams);

public:
	//temporary used because i cant make planes copy work
	HRESULT UpdateConvertColorShader();
	void SetShaderConvertColorParams();
	void SetShaderLuminanceParams();
	HRESULT SetShaderDoviCurvesPoly();
	HRESULT SetShaderDoviCurves();

	/*libplacebo*/
	static void render_info_cb(void* priv, const struct pl_render_info* info);

	HRESULT CopySampleToLibplacebo(IMediaSample* pSample);

	HRESULT SetDevice(ID3D11Device1 *pDevice, const bool bDecoderDevice);
	HRESULT InitSwapChain();

	void UpdateStatsInputFmt();

	BOOL VerifyMediaType(const CMediaType* pmt) override;
	BOOL InitMediaType(const CMediaType* pmt) override;

	HRESULT InitializeD3D11VP(const FmtConvParams_t& params, const UINT width, const UINT height);
	HRESULT InitializeTexVP(const FmtConvParams_t& params, const UINT width, const UINT height);
	void UpdatFrameProperties(); // use this after receiving modified frame from hardware decoder

	BOOL GetAlignmentSize(const CMediaType& mt, SIZE& Size) override;


	void SetVideoRect(const Com::SmartRect& videoRect)      override;
	HRESULT SetWindowRect(const Com::SmartRect& windowRect) override;
	HRESULT Reset() override;
	bool IsInit() const override { return m_bHdrDisplaySwitching; }

	HRESULT GetCurentImage(long* pDIBImage) override { return E_FAIL; };
	HRESULT GetDisplayedImage(BYTE **ppDib, unsigned* pSize) override { return E_FAIL; };

	void SetRotation(int value) override;
	void SetStereo3dTransform(int value) override;

	void Flush() override;

private:
	void UpdateTexures();

	HRESULT D3D11VPPass(ID3D11Texture2D* pRenderTarget, const Com::SmartRect& srcRect, const Com::SmartRect& dstRect, const bool second);


	HRESULT Process(ID3D11Texture2D* pRenderTarget, const Com::SmartRect& srcRect, const Com::SmartRect& dstRect, const bool second);

	void UpdateStatsPresent();
	void UpdateStatsStatic();
	void SendStats(const struct pl_color_space csp,const struct pl_color_repr repr);

public:
	// IMFVideoProcessor
	STDMETHODIMP SetProcAmpValues(DWORD dwFlags, DXVA2_ProcAmpValues *pValues) override;

	// IMFVideoMixerBitmap
	STDMETHODIMP SetAlphaBitmap(const MFVideoAlphaBitmap* pBmpParms) override { return S_OK; };
	STDMETHODIMP UpdateAlphaBitmapParameters(const MFVideoAlphaBitmapParams *pBmpParms) override { return S_OK; };
};
