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

#include <d3d9types.h>/*needed for dxva2api.h*/
#include <dxva2api.h>
#include <mfobjects.h>
#include "Util2.h"
#include "utils/StringUtils.h"
#include "csputils.h"
#include "Include/IMediaSideData.h"
#include <dxgiformat.h>

#define D3DFMT_YV12 (D3DFORMAT)FCC('YV12')
#define D3DFMT_NV12 (D3DFORMAT)FCC('NV12')
#define D3DFMT_P010 (D3DFORMAT)FCC('P010')
#define D3DFMT_P016 (D3DFORMAT)FCC('P016')
#define D3DFMT_P210 (D3DFORMAT)FCC('P210')
#define D3DFMT_P216 (D3DFORMAT)FCC('P216')
#define D3DFMT_AYUV (D3DFORMAT)FCC('AYUV')
#define D3DFMT_Y410 (D3DFORMAT)FCC('Y410')
#define D3DFMT_Y416 (D3DFORMAT)FCC('Y416')

#define D3DFMT_PLANAR (D3DFORMAT)0xFFFF

#define DXGI_FORMAT_PLANAR (DXGI_FORMAT)0xFFFF

//#define PCIV_AMD      0x1002
//#define PCIV_NVIDIA      0x10DE
//#define PCIV_Intel       0x8086

#define INVALID_TIME INT64_MIN

struct VR_Extradata {
	LONG  QueryWidth;
	LONG  QueryHeight;
	LONG  FrameWidth;
	LONG  FrameHeight;
	DWORD Compression;
};

struct ScalingShaderResId {
	std::string shaderX;
	std::string shaderY;
	const wchar_t* const description;
};

CStdStringA GetNameAndVersion();

CStdStringW MediaType2Str(const CMediaType *pmt);

const wchar_t* DXGIFormatToString(const DXGI_FORMAT format);
CStdStringW DXVA2VPDeviceToString(const GUID& guid);
void SetDefaultDXVA2ProcAmpRanges(DXVA2_ValueRange(&DXVA2ProcAmpRanges)[4]);
void SetDefaultDXVA2ProcAmpValues(DXVA2_ProcAmpValues& DXVA2ProcAmpValues);
bool IsDefaultDXVA2ProcAmpValues(const DXVA2_ProcAmpValues& DXVA2ProcAmpValues);

typedef void(*CopyFrameDataFn)(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch);

enum ColorFormat_t {
	CF_NONE = 0,
	CF_NV12,
	CF_P010,
	CF_P016,
	CF_YUY2,
	CF_P210,
	CF_P216,
	CF_Y210, // experimental
	CF_Y216, // experimental
	CF_AYUV,
	CF_Y410,
	CF_Y416,
	CF_YV12,
	CF_YV16,
	CF_YV24,
	CF_YUV420P8,
	CF_YUV422P8,
	CF_YUV444P8,
	CF_YUV420P16,
	CF_YUV422P16,
	CF_YUV444P16,
	CF_GBRP8,
	CF_GBRP16,
	CF_RGB24,
	CF_XRGB32,
	CF_ARGB32,
	CF_r210,
	CF_RGB48,
	CF_BGR48,
	CF_BGRA64,
	CF_B64A,
	CF_Y8,
	CF_Y16,
};

enum ColorSystem_t {
	CS_YUV,
	CS_RGB,
	CS_GRAY
};

struct DX11PlaneConfig_t {
	DXGI_FORMAT FmtPlane1;
	DXGI_FORMAT FmtPlane2;
	DXGI_FORMAT FmtPlane3;
	UINT        div_chroma_w;
	UINT        div_chroma_h;
};

enum libplacebo_format_t {
	PL_UNKNOWN = 0, // also used for inconsistent multi-component formats
	PL_UNORM,       // unsigned, normalized integer format (sampled as float)
	PL_SNORM,       // signed, normalized integer format (sampled as float)
	PL_UINT,        // unsigned integer format (sampled as integer)
	PL_SINT,        // signed integer format (sampled as integer)
	PL_FLOAT,       // (signed) float formats, any bit size
	PL_TYPE_COUNT,
};

struct libplacebo_plane_t {
	libplacebo_format_t type;
	float width, height;      // dimensions of the plane
	int component_size[4];  // size in bits of each coordinate
	int component_pad[4];   // ignored bits preceding each component
	int component_map[4];   // semantic meaning of each component (pixel order)
	size_t pixel_stride;    // offset in bytes between pixels (required)
	size_t row_stride;      // offset in bytes between rows (optional)
	bool swapped;           // pixel data is endian-swapped (non-native)
};


struct FmtConvParams_t {
	ColorFormat_t      cformat;
	const wchar_t*     str;
	DXGI_FORMAT        VP11Format;
	DXGI_FORMAT        DX11Format;
	DX11PlaneConfig_t* pDX11Planes;
	int                Packsize;
	int                PitchCoeff;
	ColorSystem_t      CSType;
	int                Subsampling;
	int                CDepth;
	CopyFrameDataFn    Func;
	CopyFrameDataFn    FuncSSSE3;
	bool               SupportLibplacebo;
};

/*libplacebo*/
struct FmtConvParamsLibplacebo_t {
	ColorFormat_t       cformat;
	libplacebo_plane_t  *planes[4];
};
ColorFormat_t GetColorFormat(const CMediaType* pmt);
const FmtConvParams_t& GetFmtConvParams(const ColorFormat_t fmt);
const FmtConvParamsLibplacebo_t& GetFmtConvParamsLibplacebo(const ColorFormat_t fmt);
const FmtConvParams_t& GetFmtConvParams(const CMediaType* pmt);
CopyFrameDataFn GetCopyFunction(const FmtConvParams_t& params);

// YUY2, AYUV, RGB32 to D3DFMT_X8R8G8B8, ARGB32 to D3DFMT_A8R8G8B8
void CopyFrameAsIs(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch);
void CopyGpuFrame_SSE41(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch);
// RGB24 to D3DFMT_X8R8G8B8
void CopyFrameRGB24(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch);
void CopyRGB24_SSSE3(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch); // 30% faster than CopyFrameRGB24().
// RGB48, b48r to D3DFMT_A16B16G16R16
void CopyFrameRGB48(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch);
void CopyRGB48_SSSE3(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch); // Not faster than CopyFrameRGB48().
// BGR48 to D3DFMT_A16B16G16R16
void CopyFrameBGR48(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch);
// BGRA64 to D3DFMT_A16B16G16R16
void CopyFrameBGRA64(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch);
// b64a to D3DFMT_A16B16G16R16
void CopyFrameB64A(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch);
// YV12
void CopyFrameYV12(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch);
// Y410 (not used)
void CopyFrameY410(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch);
// r210
void CopyFrameR210(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch);

void ConvertXRGB10toXRGB8(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch);

void ClipToSurface(const int texW, const int texH, RECT& s, RECT& d);

void set_colorspace(const DXVA2_ExtendedFormat extfmt, mp_colorspace& colorspace);

BITMAPINFOHEADER* GetBIHfromVIHs(const AM_MEDIA_TYPE* mt);

HRESULT SaveToBMP(BYTE* src, const UINT src_pitch, const UINT width, const UINT height, const UINT bitdepth, const wchar_t* filename);

HRESULT SaveToImage(BYTE* src, const UINT pitch, const UINT width, const UINT height, const UINT bitdepth, const CStdStringW filename);

DXVA2_ExtendedFormat SpecifyExtendedFormat(DXVA2_ExtendedFormat exFormat, const FmtConvParams_t& fmtParams, const UINT width, const UINT height);

void GetExtendedFormatString(LPCSTR (&strs)[6], const DXVA2_ExtendedFormat exFormat, const ColorSystem_t colorSystem);
