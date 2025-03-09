/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "guilib/D3DResource.h"
#include "utils/ColorUtils.h"
#include "utils/Geometry.h"
#include <strmif.h>
#include <map>

#include <DirectXMath.h>
#include <d3dx11effect.h>
#include <wrl/client.h>
#include "utils/log.h"
#include "libplacebo/colorspace.h"
#include "DSUtil/Geometry.h"
#include <queue>

#define IDF_DITHER_32X32_FLOAT16 "special://xbmc/system/players/dsplayer/MPCShaders/dither32x32float16.bin"
#define IDF_HLSL_ST2084 "special://xbmc/system/players/dsplayer/MPCShaders/st2084.hlsl"
#define IDF_HLSL_HLG "special://xbmc/system/players/dsplayer/MPCShaders/hlg.hlsl"
#define IDF_HLSL_HDR_TONE_MAPPING "special://xbmc/system/players/dsplayer/MPCShaders/hdr_tone_mapping.hlsl"

#define IDF_PS_9_CONVERT_COLOR "special://xbmc/system/players/dsplayer/mpcshaders/convert_color.cso"
#define IDF_PS_9_CONVERT_YUY2 "special://xbmc/system/players/dsplayer/mpcshaders/convert_yuy2.cso"
#define IDF_PS_9_CONVERT_BIPLANAR "special://xbmc/system/players/dsplayer/mpcshaders/convert_biplanar.cso"
#define IDF_PS_9_CONVERT_PLANAR "special://xbmc/system/players/dsplayer/mpcshaders/convert_planar.cso"
#define IDF_PS_9_CONVERT_PLANAR_YV "special://xbmc/system/players/dsplayer/mpcshaders/convert_planar_yv.cso"
#define IDF_PS_9_FIXCONVERT_PQ_TO_SDR "special://xbmc/system/players/dsplayer/mpcshaders/fixconvert_pq_to_sdr.cso"
#define IDF_PS_9_FIXCONVERT_HLG_TO_SDR "special://xbmc/system/players/dsplayer/mpcshaders/fixconvert_hlg_to_sdr.cso"
#define IDF_PS_9_FIX_BT2020 "special://xbmc/system/players/dsplayer/mpcshaders/fix_bt2020.cso"
#define IDF_PS_9_INTERP_MITCHELL4_X "special://xbmc/system/players/dsplayer/mpcshaders/resizer_mitchell4_x.cso"
#define IDF_PS_9_INTERP_MITCHELL4_Y "special://xbmc/system/players/dsplayer/mpcshaders/resizer_mitchell4_y.cso"
#define IDF_PS_9_INTERP_CATMULL4_X "special://xbmc/system/players/dsplayer/mpcshaders/resizer_catmull4_x.cso"
#define IDF_PS_9_INTERP_CATMULL4_Y "special://xbmc/system/players/dsplayer/mpcshaders/resizer_catmull4_y.cso"
#define IDF_PS_9_INTERP_LANCZOS2_X "special://xbmc/system/players/dsplayer/mpcshaders/resizer_lanczos2_x.cso"
#define IDF_PS_9_INTERP_LANCZOS2_Y "special://xbmc/system/players/dsplayer/mpcshaders/resizer_lanczos2_y.cso"
#define IDF_PS_9_INTERP_LANCZOS3_X "special://xbmc/system/players/dsplayer/mpcshaders/resizer_lanczos3_x.cso"
#define IDF_PS_9_INTERP_LANCZOS3_Y "special://xbmc/system/players/dsplayer/mpcshaders/resizer_lanczos3_y.cso"
#define IDF_PS_9_INTERP_JINC2 "special://xbmc/system/players/dsplayer/mpcshaders/resizer_onepass_jinc2.cso"
#define IDF_PS_9_CONVOL_BOX_X "special://xbmc/system/players/dsplayer/mpcshaders/downscaler_box_x.cso"
#define IDF_PS_9_CONVOL_BOX_Y "special://xbmc/system/players/dsplayer/mpcshaders/downscaler_box_y.cso"
#define IDF_PS_9_CONVOL_BILINEAR_X "special://xbmc/system/players/dsplayer/mpcshaders/downscaler_bilinear_x.cso"
#define IDF_PS_9_CONVOL_BILINEAR_Y "special://xbmc/system/players/dsplayer/mpcshaders/downscaler_bilinear_y.cso"
#define IDF_PS_9_CONVOL_HAMMING_X "special://xbmc/system/players/dsplayer/mpcshaders/downscaler_hamming_x.cso"
#define IDF_PS_9_CONVOL_HAMMING_Y "special://xbmc/system/players/dsplayer/mpcshaders/downscaler_hamming_y.cso"
#define IDF_PS_9_CONVOL_BICUBIC05_X "special://xbmc/system/players/dsplayer/mpcshaders/downscaler_bicubic05_x.cso"
#define IDF_PS_9_CONVOL_BICUBIC05_Y "special://xbmc/system/players/dsplayer/mpcshaders/downscaler_bicubic05_y.cso"
#define IDF_PS_9_CONVOL_BICUBIC15_X "special://xbmc/system/players/dsplayer/mpcshaders/downscaler_bicubic15_x.cso"
#define IDF_PS_9_CONVOL_BICUBIC15_Y "special://xbmc/system/players/dsplayer/mpcshaders/downscaler_bicubic15_y.cso"
#define IDF_PS_9_CONVOL_LANCZOS_X "special://xbmc/system/players/dsplayer/mpcshaders/downscaler_lanczos_x.cso"
#define IDF_PS_9_CONVOL_LANCZOS_Y "special://xbmc/system/players/dsplayer/mpcshaders/downscaler_lanczos_y.cso"
#define IDF_PS_9_HALFOU_TO_INTERLACE "special://xbmc/system/players/dsplayer/mpcshaders/halfoverunder_to_interlace.cso"
#define IDF_PS_9_FINAL_PASS "special://xbmc/system/players/dsplayer/mpcshaders/final_pass.cso"

#define IDF_VS_11_SIMPLE "special://xbmc/system/players/dsplayer/MPCShaders/vs_simple.cso"
#define IDF_PS_11_SIMPLE "special://xbmc/system/players/dsplayer/MPCShaders/ps_simple.cso"
#define IDF_VS_11_GEOMETRY "special://xbmc/system/players/dsplayer/MPCShaders/vs_geometry.cso"
#define IDF_PS_11_GEOMETRY "special://xbmc/system/players/dsplayer/MPCShaders/ps_geometry.cso"
#define IDF_PS_11_FONT "special://xbmc/system/players/dsplayer/MPCShaders/ps_font.cso"
#define IDF_PS_11_CONVERT_COLOR "special://xbmc/system/players/dsplayer/MPCShaders/ps_convert_color.cso"
#define IDF_PS_11_CONVERT_YUY2 "special://xbmc/system/players/dsplayer/MPCShaders/ps_convert_yuy2.cso"
#define IDF_PS_11_CONVERT_BIPLANAR "special://xbmc/system/players/dsplayer/MPCShaders/ps_convert_biplanar.cso"
#define IDF_PS_11_CONVERT_PLANAR "special://xbmc/system/players/dsplayer/MPCShaders/ps_convert_planar.cso"
#define IDF_PS_11_CONVERT_PLANAR_YV "special://xbmc/system/players/dsplayer/MPCShaders/ps_convert_planar_yv.cso"
#define IDF_PS_11_CONVERT_PQ_TO_SDR "special://xbmc/system/players/dsplayer/MPCShaders/ps_convert_pq_to_sdr.cso"
#define IDF_PS_11_FIXCONVERT_PQ_TO_SDR "special://xbmc/system/players/dsplayer/MPCShaders/ps_fixconvert_pq_to_sdr.cso"
#define IDF_PS_11_FIXCONVERT_HLG_TO_SDR "special://xbmc/system/players/dsplayer/MPCShaders/ps_fixconvert_hlg_to_sdr.cso"
#define IDF_PS_11_FIX_BT2020 "special://xbmc/system/players/dsplayer/MPCShaders/ps_fix_bt2020.cso"
#define IDF_PS_11_CONVERT_HLG_TO_PQ "special://xbmc/system/players/dsplayer/MPCShaders/ps_convert_hlg_to_pq.cso"
#define IDF_PS_11_CONVERT_BITMAP_TO_PQ "special://xbmc/system/players/dsplayer/MPCShaders/ps_convert_bitmap_to_pq.cso"
#define IDF_PS_11_CONVERT_BITMAP_TO_PQ1 "special://xbmc/system/players/dsplayer/MPCShaders/ps_convert_bitmap_to_pq1.cso"
#define IDF_PS_11_CONVERT_BITMAP_TO_PQ2 "special://xbmc/system/players/dsplayer/MPCShaders/ps_convert_bitmap_to_pq2.cso"
#define IDF_PS_11_INTERP_MITCHELL4_X "special://xbmc/system/players/dsplayer/MPCShaders/ps_resizer_mitchell4_x.cso"
#define IDF_PS_11_INTERP_MITCHELL4_Y "special://xbmc/system/players/dsplayer/MPCShaders/ps_resizer_mitchell4_y.cso"
#define IDF_PS_11_INTERP_CATMULL4_X "special://xbmc/system/players/dsplayer/MPCShaders/ps_resizer_catmull4_x.cso"
#define IDF_PS_11_INTERP_CATMULL4_Y "special://xbmc/system/players/dsplayer/MPCShaders/ps_resizer_catmull4_y.cso"
#define IDF_PS_11_INTERP_LANCZOS2_X "special://xbmc/system/players/dsplayer/MPCShaders/ps_resizer_lanczos2_x.cso"
#define IDF_PS_11_INTERP_LANCZOS2_Y "special://xbmc/system/players/dsplayer/MPCShaders/ps_resizer_lanczos2_y.cso"
#define IDF_PS_11_INTERP_LANCZOS3_X "special://xbmc/system/players/dsplayer/MPCShaders/ps_resizer_lanczos3_x.cso"
#define IDF_PS_11_INTERP_LANCZOS3_Y "special://xbmc/system/players/dsplayer/MPCShaders/ps_resizer_lanczos3_y.cso"
#define IDF_PS_11_INTERP_JINC2 "special://xbmc/system/players/dsplayer/MPCShaders/ps_resize_onepass_jinc2.cso"
#define IDF_PS_11_CONVOL_BOX_X "special://xbmc/system/players/dsplayer/MPCShaders/ps_downscaler_box_x.cso"
#define IDF_PS_11_CONVOL_BOX_Y "special://xbmc/system/players/dsplayer/MPCShaders/ps_downscaler_box_y.cso"
#define IDF_PS_11_CONVOL_BILINEAR_X "special://xbmc/system/players/dsplayer/MPCShaders/ps_downscaler_bilinear_x.cso"
#define IDF_PS_11_CONVOL_BILINEAR_Y "special://xbmc/system/players/dsplayer/MPCShaders/ps_downscaler_bilinear_y.cso"
#define IDF_PS_11_CONVOL_HAMMING_X "special://xbmc/system/players/dsplayer/MPCShaders/ps_downscaler_hamming_x.cso"
#define IDF_PS_11_CONVOL_HAMMING_Y "special://xbmc/system/players/dsplayer/MPCShaders/ps_downscaler_hamming_y.cso"
#define IDF_PS_11_CONVOL_BICUBIC05_X "special://xbmc/system/players/dsplayer/MPCShaders/ps_downscaler_bicubic05_x.cso"
#define IDF_PS_11_CONVOL_BICUBIC05_Y "special://xbmc/system/players/dsplayer/MPCShaders/ps_downscaler_bicubic05_y.cso"
#define IDF_PS_11_CONVOL_BICUBIC15_X "special://xbmc/system/players/dsplayer/MPCShaders/ps_downscaler_bicubic15_x.cso"
#define IDF_PS_11_CONVOL_BICUBIC15_Y "special://xbmc/system/players/dsplayer/MPCShaders/ps_downscaler_bicubic15_y.cso"
#define IDF_PS_11_CONVOL_LANCZOS_X "special://xbmc/system/players/dsplayer/MPCShaders/ps_downscaler_lanczos_x.cso"
#define IDF_PS_11_CONVOL_LANCZOS_Y "special://xbmc/system/players/dsplayer/MPCShaders/ps_downscaler_lanczos_y.cso"
#define IDF_PS_11_HALFOU_TO_INTERLACE "special://xbmc/system/players/dsplayer/MPCShaders/ps_halfoverunder_to_interlace.cso"
#define IDF_PS_11_FINAL_PASS "special://xbmc/system/players/dsplayer/MPCShaders/ps_final_pass.cso"
#define IDF_PS_11_FINAL_PASS_10 "special://xbmc/system/players/dsplayer/MPCShaders/ps_final_pass_10.cso"


class CD3DDSPixelShader
{
public:
  CD3DDSPixelShader();
  ~CD3DDSPixelShader();

  bool LoadFromFile(std::string path);
  LPVOID GetData() { return m_data; }
  SIZE_T GetSize() { return m_size; }
private:
  std::string m_path;

  LPVOID m_data;
  SIZE_T m_size;
  ID3DBlob* m_PSBuffer;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> m_PS;
};

struct CMPCVRFrameBase
{
	CD3DTexture pTexture;
	REFERENCE_TIME pStartTime;
	REFERENCE_TIME pEndTime;
	REFERENCE_TIME pUploadTime;
	REFERENCE_TIME pProcessingTime;

	//MediaSideDataDOVIMetadata pDOVIMetadata;
	//struct pl_dovi_metadata doviout;

	//pl_hdr_metadata pPlHdr;
	//MediaSideDataHDR pHdrMetadata;

	struct pl_color_space color;
	struct pl_color_repr repr;

	//used for scaling if it change we need to flush and recreate
	Com::SmartRect pCurrentRect;

	//MediaSideDataHDR10Plus pHDR10Plus;
	//MediaSideDataHDRContentLightLevel pHDRLightLevel;
};

struct CMPCVRFrame : CMPCVRFrameBase
{
	CMPCVRFrame()
	{
		color = {};
		repr = {};
		pStartTime = 0;
		pEndTime = 0;
		pUploadTime = 0;
		pProcessingTime = 0;
	}
};
// Frame thread-safe queue
class FrameQueue
{
private:
	std::list<CMPCVRFrame> pQueue;
	mutable std::mutex pMutex;
	std::condition_variable pCondition;
	int pMaxQueue;
public:
	void Resize(int queues)
	{
		pMaxQueue = queues;
		flush();
		for (int i = 0; i < queues; i++)
		{
			CMPCVRFrame frame;
			pQueue.emplace_back(frame);
		}

	}
	void push(CMPCVRFrame value)
	{
		{
			std::lock_guard<std::mutex> lock(pMutex);
			pQueue.push_back(std::move(value));
		}
		pCondition.notify_one();
	}

	void pop()
	{
		std::lock_guard<std::mutex> lock(pMutex);
		pQueue.pop_front();
	}

	bool empty()
	{
		return pQueue.empty();
	}

	int size()
	{
		return pQueue.size();
	}

	CMPCVRFrame& front()
	{
		return pQueue.front();
	}

	void wait_and_pop(CMPCVRFrame& result)
	{
		std::unique_lock<std::mutex> lock(pMutex);
		pCondition.wait(lock, [this] { return !pQueue.empty(); });
		result = std::move(pQueue.front());
		pQueue.pop_front();
	}

	void flush()
	{
		std::lock_guard<std::mutex> lock(pMutex);
		pQueue.clear();
	}

	// Iterators for range-based for loops
	auto begin()
	{
		return pQueue.begin();
	}

	auto end()
	{
		return pQueue.end();
	}
};

// Thread-safe queue template
template <typename T>
class ThreadSafeQueue
{
private:
	std::queue<T> queue_;
	mutable std::mutex mutex_;
	std::condition_variable condition_;
public:
	void push(T value) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			queue_.push(std::move(value));
		}
		condition_.notify_one();
	}

	void pop() {
		std::lock_guard<std::mutex> lock(mutex_);
		queue_.pop();
	}

	bool empty() {
		return queue_.empty();
	}

	int size() {
		return queue_.size();
	}

	T front() {
		return queue_.front();
	}

	void wait_and_pop(T& result) {
		std::unique_lock<std::mutex> lock(mutex_);
		condition_.wait(lock, [this] { return !queue_.empty(); });
		result = std::move(queue_.front());
		queue_.pop();
	}

	void flush() {
		std::lock_guard<std::mutex> lock(mutex_);
		std::queue<T> empty;
		std::swap(queue_, empty);
	}
};
