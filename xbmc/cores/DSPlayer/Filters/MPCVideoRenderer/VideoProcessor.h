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

#pragma once

#include <evr9.h>
#include "DisplayConfig.h"
#include "FrameStats.h"
#include "dsutil/Geometry.h"
#include "DSResource.h"
#include "VideoRenderers/MPCVRRenderer.h"
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

#include "threads/Thread.h"
enum : int {
	STEREO3D_AsIs = 0,
	STEREO3D_HalfOverUnder_to_Interlace,
};

class CMpcVideoRenderer;

class CVideoProcessor
	: public IMFVideoProcessor,
		public IDSRendererAllocatorCallback,
		public IMpcVRCallback
{
public:
	CVideoProcessor(CMpcVideoRenderer* pFilter);
	~CVideoProcessor();
protected:
	long m_nRefCount = 1;
	CMpcVideoRenderer* m_pFilter = nullptr;

	CopyFrameDataFn m_pConvertFn = nullptr;
	CopyFrameDataFn m_pCopyGpuFn = CopyFrameAsIs;

	// Input parameters
	FmtConvParams_t m_srcParams = GetFmtConvParams(CF_NONE);
	FmtConvParamsLibplacebo_t m_srcParamsLibplacebo;
	UINT  m_srcWidth        = 0;
	UINT  m_srcHeight       = 0;
	
	UINT  m_srcRectWidth    = 0;
	UINT  m_srcRectHeight   = 0;
	int   m_srcPitch        = 0;
	UINT  m_srcLines        = 0;
	DWORD m_srcAspectRatioX = 0;
	DWORD m_srcAspectRatioY = 0;
	bool  m_srcAnamorphic   = false;
	Com::SmartRect m_srcRect;
	DXVA2_ExtendedFormat m_decExFmt = {};
	DXVA2_ExtendedFormat m_srcExFmt = {};
	bool  m_bInterlaced = false;
	REFERENCE_TIME m_rtAvgTimePerFrame = 0;
	
	Com::SmartRect m_videoRect;
	Com::SmartRect m_windowRect;
	Com::SmartRect m_renderRect;

	int  m_iRotation   = 0;
	bool m_bFlip       = false;
	int  m_iStereo3dTransform = 0;
	bool m_bFinalPass  = false;
	bool m_bDitherUsed = false;

	DXVA2_ValueRange m_DXVA2ProcAmpRanges[4] = {};
	DXVA2_ProcAmpValues m_DXVA2ProcAmpValues = {};

	struct DOVIMetadata {
		MediaSideDataDOVIMetadata msd = {};
		bool bValid = false;
		bool bHasMMR = false;
	} m_Dovi;

	bool CheckDoviMetadata(const MediaSideDataDOVIMetadata* pDOVIMetadata, const uint8_t maxReshapeMethon);

	HWND m_hWnd = nullptr;
	UINT m_nCurrentAdapter; // set it in subclasses
	DWORD m_VendorId = 0;
	CStdStringW m_strAdapterDescription;

	REFERENCE_TIME m_rtStart = 0;
	REFERENCE_TIME m_rtStartStream = 0;

	int m_FieldDrawn = 0;
	bool m_bDoubleFrames = false;

	UINT32 m_uHalfRefreshPeriodMs = 0;

	// AlphaBitmap
	bool m_bAlphaBitmapEnable = false;
	RECT m_AlphaBitmapRectSrc = {};
	MFVideoNormalizedRect m_AlphaBitmapNRectDest = {};

	// Statistics
	CRenderStats m_RenderStats;
	CStdStringW m_strStatsHeader;
	CStdStringW m_strStatsInputFmt;
	CStdStringW m_strStatsVProc;
	CStdStringW m_strStatsDispInfo;
	//CStdStringW m_strStatsPostProc;
	CStdStringW m_strStatsHDR;
	CStdStringW m_strStatsPresent;
	int m_iSrcFromGPU = 0;
	int m_StatsFontH = 14;
	RECT m_StatsRect = { 10, 10, 10 + 5 + 63*8 + 3, 10 + 5 + 18*17 + 3 };
	const POINT m_StatsTextPoint = { 10 + 5, 10 + 5};

	// Graph of a function
	CMovingAverage<int> m_Syncs = CMovingAverage<int>(120);

	int m_Xstep  = 4;
	int m_Yscale = 2;
	RECT m_GraphRect = {};
	int m_Yaxis  = 0;

	int m_nStereoSubtitlesOffsetInPixels = 4;



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


	bool m_bHdrDisplaySwitching = false; // switching HDR display in progress
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
	Com::SmartRect m_destRect;
public:

	//temporary used because i cant make planes copy work
	HRESULT UpdateConvertColorShader();
	void SetShaderConvertColorParams();
	void SetShaderLuminanceParams();
	HRESULT SetShaderDoviCurvesPoly();
	HRESULT SetShaderDoviCurves();

	void SendStats(const pl_color_space csp, const pl_color_repr repr);

	/*libplacebo*/
	static void render_info_cb(void* priv, const struct pl_render_info* info);

	HRESULT SetDevice(ID3D11Device1* pDevice, const bool bDecoderDevice);
	HRESULT InitSwapChain();

	void UpdateStatsInputFmt();

	BOOL VerifyMediaType(const CMediaType* pmt);
	BOOL InitMediaType(const CMediaType* pmt);

	HRESULT InitializeD3D11VP(const FmtConvParams_t& params, const UINT width, const UINT height);
	HRESULT InitializeTexVP(const FmtConvParams_t& params, const UINT width, const UINT height);
	void UpdatFrameProperties(); // use this after receiving modified frame from hardware decoder

	BOOL GetAlignmentSize(const CMediaType& mt, SIZE& Size);

	void UpdateTexures();

	HRESULT D3D11VPPass(ID3D11Texture2D* pRenderTarget, const Com::SmartRect& srcRect, const Com::SmartRect& dstRect, const bool second);


	void SetVideoRect(const Com::SmartRect& videoRect);
	HRESULT SetWindowRect(const Com::SmartRect& windowRect);
	HRESULT Reset();

	void Flush();
	void UpdateStatsPresent();
	void UpdateStatsStatic();
	void SetRotation(int value);
	void SetStereo3dTransform(int value);

	CMPCVRFrame ConvertSampleToFrame(IMediaSample* pSample);


	void ProcessLoop();
	void ProcessFrameLibplacebo(CMPCVRFrame& inputFrame, CMPCVRFrame& outputFrame);
	void ProcessFrameVP(CMPCVRFrame& inputFrame, CMPCVRFrame& outputFrame);

	bool Initialized();

	//Sample queueing
	HRESULT ProcessSample(IMediaSample* pSample);

	ID3D11DeviceContext1* GetD3DContext() const { return m_pDeviceContext.Get(); }

	UINT GetCurrentAdapter() { return m_nCurrentAdapter; }

	void Start() { m_rtStart = 0; }

	void FlushSampledQueue();

	static DWORD __stdcall UploadThread(LPVOID lpParameter);
	void UploadLoop();
	static DWORD __stdcall ProcessThread(LPVOID lpParameter);

	ColorFormat_t GetColorFormat() { return m_srcParams.cformat; }

	void GetSourceRect(Com::SmartRect& sourceRect) { sourceRect = m_srcRect; }
	void GetVideoRect(Com::SmartRect& videoRect) { videoRect = m_videoRect; }

	HRESULT GetVideoSize(long *pWidth, long *pHeight);
	HRESULT GetAspectRatio(long *plAspectX, long *plAspectY);

	int GetRotation() { return m_iRotation; }
	bool GetFlip() { return m_bFlip; }
	void SetFlip(bool value) { m_bFlip = value; }

	void SetDisplayInfo(const DisplayConfig_t& dc, const bool primary, const bool fullscreen);

	bool GetDoubleRate() { return m_bDoubleFrames; }
	HRESULT Init(const HWND hwnd, bool* pChangeDevice = nullptr);


protected:
	void ProcessFrame(IMediaSample* pSample, CMPCVRFrame& frame);
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





	inline bool SourceIsPQorHLG() {
		return m_srcExFmt.VideoTransferFunction == MFVideoTransFunc_2084 || m_srcExFmt.VideoTransferFunction == MFVideoTransFunc_HLG;
	}
	// source is PQ, HLG or Dolby Vision
	inline bool SourceIsHDR() {
		return SourceIsPQorHLG() || m_Dovi.bValid;
	}

	CRefTime m_streamTime;
	void SyncFrameToStreamTime(const REFERENCE_TIME frameStartTime);


	// QUEUES AND THEIR CRITICAL SECTIONS
	ThreadSafeQueue<IMediaSample*> m_uploadQueue;

	HANDLE m_hUploadEvent;


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

	//IMpcVRCallback
	HRESULT GetPresentationTexture(ID3D11Texture2D** texture);
	void RenderRectChanged(CRect newRect);

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, void **ppv);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	// IMFVideoProcessor
	STDMETHODIMP GetAvailableVideoProcessorModes(UINT *lpdwNumProcessingModes, GUID **ppVideoProcessingModes) { return E_NOTIMPL; }
	STDMETHODIMP GetVideoProcessorCaps(LPGUID lpVideoProcessorMode, DXVA2_VideoProcessorCaps *lpVideoProcessorCaps) { return E_NOTIMPL; }
	STDMETHODIMP GetVideoProcessorMode(LPGUID lpMode) { return E_NOTIMPL; }
	STDMETHODIMP SetVideoProcessorMode(LPGUID lpMode) { return E_NOTIMPL; }
	STDMETHODIMP GetProcAmpRange(DWORD dwProperty, DXVA2_ValueRange *pPropRange);
	STDMETHODIMP GetProcAmpValues(DWORD dwFlags, DXVA2_ProcAmpValues *Values);
	STDMETHODIMP SetProcAmpValues(DWORD dwFlags, DXVA2_ProcAmpValues* pValues) override;
	STDMETHODIMP GetFilteringRange(DWORD dwProperty, DXVA2_ValueRange *pPropRange) { return E_NOTIMPL; }
	STDMETHODIMP GetFilteringValue(DWORD dwProperty, DXVA2_Fixed32 *pValue) { return E_NOTIMPL; }
	STDMETHODIMP SetFilteringValue(DWORD dwProperty, DXVA2_Fixed32 *pValue) { return E_NOTIMPL; }
	STDMETHODIMP GetBackgroundColor(COLORREF *lpClrBkg);
	STDMETHODIMP SetBackgroundColor(COLORREF ClrBkg) { return E_NOTIMPL; }
};
