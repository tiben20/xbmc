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

enum : int {
	STEREO3D_AsIs = 0,
	STEREO3D_HalfOverUnder_to_Interlace,
};

class CMpcVideoRenderer;

class CVideoProcessor
	: public IMFVideoProcessor
	, public IMFVideoMixerBitmap
{
protected:
	long m_nRefCount = 1;
	CMpcVideoRenderer* m_pFilter = nullptr;
	// Settings
	

	bool m_bVPScalingUseShaders = false;

	CopyFrameDataFn m_pConvertFn = nullptr;
	CopyFrameDataFn m_pCopyGpuFn = CopyFrameAsIs;

	// Input parameters
	FmtConvParams_t m_srcParams = GetFmtConvParams(CF_NONE);
	FmtConvParamsLibplacebo_t m_srcParamsLibplacebo;
	UINT  m_srcWidth        = 0;
	UINT  m_srcHeight       = 0;
	UINT  m_dstTargetWidth  = 0;
	UINT  m_dstTargetHeight = 0;
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

	CVideoProcessor(CMpcVideoRenderer* pFilter) : m_pFilter(pFilter){}

public:
	virtual ~CVideoProcessor() = default;

	virtual HRESULT Init(const HWND hwnd, bool* pChangeDevice = nullptr) = 0;

	virtual IDirect3DDeviceManager9* GetDeviceManager9() { return nullptr; }
	UINT GetCurrentAdapter() { return m_nCurrentAdapter; }

	virtual BOOL VerifyMediaType(const CMediaType* pmt) = 0;
	virtual BOOL InitMediaType(const CMediaType* pmt) = 0;

	virtual BOOL GetAlignmentSize(const CMediaType& mt, SIZE& Size) = 0;

	virtual HRESULT ProcessSample(IMediaSample* pSample) = 0;

	void Start() { m_rtStart = 0; }
	virtual void Flush() = 0;
	virtual HRESULT Reset() = 0;

	virtual bool IsInit() const { return false; }

	ColorFormat_t GetColorFormat() { return m_srcParams.cformat; }

	void GetSourceRect(Com::SmartRect& sourceRect) { sourceRect = m_srcRect; }
	void GetVideoRect(Com::SmartRect& videoRect) { videoRect = m_videoRect; }
	virtual void SetVideoRect(const Com::SmartRect& videoRect) = 0;
	virtual HRESULT SetWindowRect(const Com::SmartRect& windowRect) = 0;

	HRESULT GetVideoSize(long *pWidth, long *pHeight);
	HRESULT GetAspectRatio(long *plAspectX, long *plAspectY);

	int GetRotation() { return m_iRotation; }
	virtual void SetRotation(int value) = 0;
	bool GetFlip() { return m_bFlip; }
	void SetFlip(bool value) { m_bFlip = value; }
	virtual void SetStereo3dTransform(int value) = 0;

	virtual HRESULT GetCurentImage(long *pDIBImage) = 0;
	virtual HRESULT GetDisplayedImage(BYTE **ppDib, unsigned *pSize) = 0;

	void CalcStatsFont();
	bool CheckGraphPlacement();
	void CalcGraphParams();

	void SetDisplayInfo(const DisplayConfig_t& dc, const bool primary, const bool fullscreen);

	bool GetDoubleRate() { return m_bDoubleFrames; }



protected:
	inline bool SourceIsPQorHLG() {
		return m_srcExFmt.VideoTransferFunction == MFVideoTransFunc_2084 || m_srcExFmt.VideoTransferFunction == MFVideoTransFunc_HLG;
	}
	// source is PQ, HLG or Dolby Vision
	inline bool SourceIsHDR() {
		return SourceIsPQorHLG() || m_Dovi.bValid;
	}

	CRefTime m_streamTime;
	void SyncFrameToStreamTime(const REFERENCE_TIME frameStartTime);

public:
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
	STDMETHODIMP GetFilteringRange(DWORD dwProperty, DXVA2_ValueRange *pPropRange) { return E_NOTIMPL; }
	STDMETHODIMP GetFilteringValue(DWORD dwProperty, DXVA2_Fixed32 *pValue) { return E_NOTIMPL; }
	STDMETHODIMP SetFilteringValue(DWORD dwProperty, DXVA2_Fixed32 *pValue) { return E_NOTIMPL; }
	STDMETHODIMP GetBackgroundColor(COLORREF *lpClrBkg);
	STDMETHODIMP SetBackgroundColor(COLORREF ClrBkg) { return E_NOTIMPL; }

	// IMFVideoMixerBitmap
	STDMETHODIMP ClearAlphaBitmap() override;
	STDMETHODIMP GetAlphaBitmapParameters(MFVideoAlphaBitmapParams *pBmpParms) override;
};
