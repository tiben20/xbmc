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
#include <memory>
#include <wincodec.h>
#include "Utils/gpu_memcpy_sse4.h"
#include "Include/Version.h"
#include "Helper.h"
#include "CPUInfo2.h"
#include "utils/StringUtils.h"

CStdStringW GetVersionStr()
{
	CStdStringW version = _CRT_WIDE(VERSION_STR);
#if VER_RELEASE != 1
	if (strcmp(BRANCH_STR, "master") != 0) {
		version.AppendFormat(L".{}", _CRT_WIDE(BRANCH_STR));
	}
	version.AppendFormat(L" (git-{}-{})",
		_CRT_WIDE(_CRT_STRINGIZE(REV_DATE)),
		_CRT_WIDE(_CRT_STRINGIZE(REV_HASH))
	);
#endif
#ifdef _WIN64
	version.append(L" x64");
#endif
#ifdef _DEBUG
	version.append(L" DEBUG");
#endif
	return version;
}

CStdStringA GetNameAndVersion()
{
	static CStdStringA version = "MPC Video Renderer ";

	return version.c_str();
}

CStdStringW MediaType2Str(const CMediaType *pmt)
{
	if (!pmt) {
		return L"no media type";
	}

	const auto& FmtParams = GetFmtConvParams(pmt);

	CStdStringW str(L"MajorType : ");
	str.append((pmt->majortype == MEDIATYPE_Video) ? L"Video" : L"unknown");

	str.AppendFormat(L"\nSubType   : %s", FmtParams.str);

	str.append(L"\nFormatType: ");
	if (pmt->formattype == FORMAT_VideoInfo2) {
		str.append(L"VideoInfo2");
		const VIDEOINFOHEADER2* vih2 = (VIDEOINFOHEADER2*)pmt->pbFormat;
		str.AppendFormat(L"\nBimapSize : %i x %i", vih2->bmiHeader.biWidth, vih2->bmiHeader.biHeight);
		str.AppendFormat(L"\nSourceRect: (%i, %i, %i, %i)", vih2->rcSource.left, vih2->rcSource.top, vih2->rcSource.right, vih2->rcSource.bottom);
		str.AppendFormat(L"\nSizeImage : %u bytes", vih2->bmiHeader.biSizeImage);
	}
	else if (pmt->formattype == FORMAT_VideoInfo) {
		str.append(L"VideoInfo");
		const VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)pmt->pbFormat;
		str.AppendFormat(L"\nBimapSize : %i x %i", vih->bmiHeader.biWidth, vih->bmiHeader.biHeight);
		str.AppendFormat(L"\nSourceRect: (%i, %i, %i, %i)", vih->rcSource.left, vih->rcSource.top, vih->rcSource.right, vih->rcSource.bottom);
		str.AppendFormat(L"\nSizeImage : %u bytes", vih->bmiHeader.biSizeImage);
	}
	else {
		str.append(L"unknown");
	}

	return str;
}

const wchar_t* D3DFormatToString(const D3DFORMAT format)
{
	switch (format) {
	case D3DFMT_A8R8G8B8:      return L"A8R8G8B8";
	case D3DFMT_X8R8G8B8:      return L"X8R8G8B8";
	case D3DFMT_A2B10G10R10:   return L"A2B10G10R10";
	case D3DFMT_A8B8G8R8:      return L"A8B8G8R8";      // often not supported
	case D3DFMT_G16R16:        return L"G16R16";
	case D3DFMT_A2R10G10B10:   return L"A2R10G10B10";
	case D3DFMT_A8P8:          return L"A8P8";          // DXVA-HD
	case D3DFMT_P8:            return L"P8";            // DXVA-HD
	case D3DFMT_L8:            return L"L8";
	case D3DFMT_A8L8:          return L"A8L8";
	case D3DFMT_L16:           return L"L16";
	case D3DFMT_A16B16G16R16F: return L"A16B16G16R16F";
	case D3DFMT_A32B32G32R32F: return L"A32B32G32R32F";
	case D3DFMT_YUY2:          return L"YUY2";
	case D3DFMT_UYVY:          return L"UYVY";
	case D3DFMT_NV12:          return L"NV12";
	case D3DFMT_YV12:          return L"YV12";
	case D3DFMT_P010:          return L"P010";
	case D3DFMT_P016:          return L"P016";
	case D3DFMT_P210:          return L"P210";
	case D3DFMT_P216:          return L"P216";
	case D3DFMT_AYUV:          return L"AYUV";
	case D3DFMT_Y410:          return L"Y410";
	case D3DFMT_Y416:          return L"Y416";
	case FCC('Y210'):          return L"Y210";          // Intel
	case FCC('Y216'):          return L"Y216";          // Intel
	case FCC('AIP8'):          return L"AIP8";          // DXVA-HD
	case FCC('AI44'):          return L"AI44";          // DXVA-HD
	};

	return L"UNKNOWN";
}

const wchar_t* DXGIFormatToString(const DXGI_FORMAT format)
{
	switch (format) {
	case DXGI_FORMAT_R16G16B16A16_FLOAT:         return L"R16G16B16A16_FLOAT";
	case DXGI_FORMAT_R16G16B16A16_UNORM:         return L"R16G16B16A16_UNORM";
	case DXGI_FORMAT_R10G10B10A2_UNORM:          return L"R10G10B10A2_UNORM";
	case DXGI_FORMAT_R8G8B8A8_UNORM:             return L"R8G8B8A8_UNORM";
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:        return L"R8G8B8A8_UNORM_SRGB";
	case DXGI_FORMAT_R16G16_UNORM:               return L"R16G16_UNORM";
	case DXGI_FORMAT_R8G8_UNORM:                 return L"R8G8_UNORM";
	case DXGI_FORMAT_R16_TYPELESS:               return L"R16_TYPELESS";
	case DXGI_FORMAT_R16_UNORM:                  return L"R16_UNORM";
	case DXGI_FORMAT_R8_TYPELESS:                return L"R8_TYPELES";
	case DXGI_FORMAT_R8_UNORM:                   return L"R8_UNORM";
	case DXGI_FORMAT_B8G8R8A8_UNORM:             return L"B8G8R8A8_UNORM";
	case DXGI_FORMAT_B8G8R8X8_UNORM:             return L"B8G8R8X8_UNORM";
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM: return L"R10G10B10_XR_BIAS_A2_UNORM";
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:        return L"B8G8R8A8_UNORM_SRGB";
	case DXGI_FORMAT_AYUV:                       return L"AYUV";
	case DXGI_FORMAT_Y410:                       return L"Y410";
	case DXGI_FORMAT_Y416:                       return L"Y416";
	case DXGI_FORMAT_NV12:                       return L"NV12";
	case DXGI_FORMAT_P010:                       return L"P010";
	case DXGI_FORMAT_P016:                       return L"P016";
	case DXGI_FORMAT_420_OPAQUE:                 return L"420_OPAQUE";
	case DXGI_FORMAT_YUY2:                       return L"YUY2";
	case DXGI_FORMAT_Y210:                       return L"Y210";
	case DXGI_FORMAT_Y216:                       return L"Y216";
	case DXGI_FORMAT_AI44:                       return L"AI44";
	case DXGI_FORMAT_IA44:                       return L"IA44";
	case DXGI_FORMAT_P8:                         return L"P8";
	case DXGI_FORMAT_A8P8:                       return L"A8P8";
	};

	return L"UNKNOWN";
}

CStdStringW DXVA2VPDeviceToString(const GUID& guid)
{
	if (guid == DXVA2_VideoProcProgressiveDevice) {
		return L"ProgressiveDevice";
	}
	else if (guid == DXVA2_VideoProcBobDevice) {
		return L"BobDevice";
	}
	else if (guid == DXVA2_VideoProcSoftwareDevice) {
		return L"SoftwareDevice";
	}

	return GUIDtoWString(guid);
}

static const DXVA2_ValueRange s_DefaultDXVA2ProcAmpRanges[4] = {
	{ DXVA2FloatToFixed(-100), DXVA2FloatToFixed(100), DXVA2FloatToFixed(0), DXVA2FloatToFixed(1)     },
	{ DXVA2FloatToFixed(0),    DXVA2FloatToFixed(2),   DXVA2FloatToFixed(1), DXVA2FloatToFixed(0.01f) },
	{ DXVA2FloatToFixed(-180), DXVA2FloatToFixed(180), DXVA2FloatToFixed(0), DXVA2FloatToFixed(1)     },
	{ DXVA2FloatToFixed(0),    DXVA2FloatToFixed(2),   DXVA2FloatToFixed(1), DXVA2FloatToFixed(0.01f) },
};

void SetDefaultDXVA2ProcAmpRanges(DXVA2_ValueRange(&DXVA2ProcAmpRanges)[4])
{
	DXVA2ProcAmpRanges[0] = s_DefaultDXVA2ProcAmpRanges[0];
	DXVA2ProcAmpRanges[1] = s_DefaultDXVA2ProcAmpRanges[1];
	DXVA2ProcAmpRanges[2] = s_DefaultDXVA2ProcAmpRanges[2];
	DXVA2ProcAmpRanges[3] = s_DefaultDXVA2ProcAmpRanges[3];
}

void SetDefaultDXVA2ProcAmpValues(DXVA2_ProcAmpValues& DXVA2ProcAmpValues)
{
	DXVA2ProcAmpValues.Brightness = s_DefaultDXVA2ProcAmpRanges[0].DefaultValue;
	DXVA2ProcAmpValues.Contrast   = s_DefaultDXVA2ProcAmpRanges[1].DefaultValue;
	DXVA2ProcAmpValues.Hue        = s_DefaultDXVA2ProcAmpRanges[2].DefaultValue;
	DXVA2ProcAmpValues.Saturation = s_DefaultDXVA2ProcAmpRanges[3].DefaultValue;
}

bool IsDefaultDXVA2ProcAmpValues(const DXVA2_ProcAmpValues& DXVA2ProcAmpValues)
{
	return DXVA2ProcAmpValues.Brightness.ll == s_DefaultDXVA2ProcAmpRanges[0].DefaultValue.ll
		&& DXVA2ProcAmpValues.Contrast.ll   == s_DefaultDXVA2ProcAmpRanges[1].DefaultValue.ll
		&& DXVA2ProcAmpValues.Hue.ll        == s_DefaultDXVA2ProcAmpRanges[2].DefaultValue.ll
		&& DXVA2ProcAmpValues.Saturation.ll == s_DefaultDXVA2ProcAmpRanges[3].DefaultValue.ll;
}

static ColorFormat_t fourcc_to_cformat(const DWORD fourcc)
{
	ColorFormat_t cformat;

	switch (fourcc) {
	case FCC('NV12'): cformat = CF_NV12; break;
	case FCC('P010'): cformat = CF_P010; break;
	case FCC('P016'): cformat = CF_P016; break;
	case FCC('YUY2'): cformat = CF_YUY2; break;
	case FCC('P210'): cformat = CF_P210; break;
	case FCC('P216'): cformat = CF_P216; break;
	case FCC('Y210'): cformat = CF_Y210; break;
	case FCC('Y216'): cformat = CF_Y216; break;
	case FCC('AYUV'): cformat = CF_AYUV; break;
	case FCC('Y410'): cformat = CF_Y410; break;
	case FCC('Y416'): cformat = CF_Y416; break;
	case FCC('YV12'): cformat = CF_YV12; break;
	case FCC('YV16'): cformat = CF_YV16; break;
	case FCC('YV24'): cformat = CF_YV24; break;
	case FCC('I420'): cformat = CF_YUV420P8; break;
	case FCC('Y42B'): cformat = CF_YUV422P8; break;
	case FCC('444P'): cformat = CF_YUV444P8; break;
	case FCC('Y800'):
	case MAKEFOURCC('Y','8',0x20,0x20): cformat = CF_Y8;        break;
	case MAKEFOURCC('Y','1',0,16):      cformat = CF_Y16;       break;
	case MAKEFOURCC('Y','3',11,16):     cformat = CF_YUV420P16; break;
	case MAKEFOURCC('Y','3',10,16):     cformat = CF_YUV422P16; break;
	case MAKEFOURCC('Y','3',0,16):      cformat = CF_YUV444P16; break;
	case MAKEFOURCC('G','3',0,8):       cformat = CF_GBRP8;     break;
	case MAKEFOURCC('G','3',0,16):      cformat = CF_GBRP16;    break;
	case FCC('b48r'):
	case MAKEFOURCC('R','G','B',48):    cformat = CF_RGB48;     break;
	case MAKEFOURCC('B','G','R',48):    cformat = CF_BGR48;     break;
	case FCC('b64a'): cformat = CF_B64A; break;
	case MAKEFOURCC('B','R','A',64):    cformat = CF_BGRA64;    break;
	case FCC('r210'): cformat = CF_r210; break;
	default: cformat = CF_NONE;
	}

	return cformat;
};

ColorFormat_t GetColorFormat(const CMediaType* pmt)
{
	if (pmt) {
		if (pmt->subtype == MEDIASUBTYPE_RGB24)  { return CF_RGB24;  }
		if (pmt->subtype == MEDIASUBTYPE_RGB32)  { return CF_XRGB32; }
		if (pmt->subtype == MEDIASUBTYPE_ARGB32) { return CF_ARGB32; }

		if (memcmp(&pmt->subtype.Data2, &MEDIASUBTYPE_YUY2.Data2, sizeof(GUID) - sizeof(GUID::Data1)) == 0) {
			return fourcc_to_cformat(pmt->subtype.Data1);
		}
		else if (pmt->subtype == MEDIASUBTYPE_LAV_RAWVIDEO) {
			const BITMAPINFOHEADER* pBIH = GetBIHfromVIHs(pmt);
			if (pBIH) {
				return fourcc_to_cformat(pBIH->biCompression);
			}
		}
	}
	return CF_NONE;
}

static DX11PlaneConfig_t DX11PlanesNV12   = { DXGI_FORMAT_R8_UNORM,  DXGI_FORMAT_R8G8_UNORM,   DXGI_FORMAT_UNKNOWN,   2, 2 };
static DX11PlaneConfig_t DX11PlanesP01x   = { DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_UNKNOWN,   2, 2 };
static DX11PlaneConfig_t DX11PlanesP21x   = { DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_UNKNOWN,   2, 1 };
static DX11PlaneConfig_t DX11Planes420P   = { DXGI_FORMAT_R8_UNORM,  DXGI_FORMAT_R8_UNORM,     DXGI_FORMAT_R8_UNORM,  2, 2 };
static DX11PlaneConfig_t DX11Planes422P   = { DXGI_FORMAT_R8_UNORM,  DXGI_FORMAT_R8_UNORM,     DXGI_FORMAT_R8_UNORM,  2, 1 };
static DX11PlaneConfig_t DX11Planes444P   = { DXGI_FORMAT_R8_UNORM,  DXGI_FORMAT_R8_UNORM,     DXGI_FORMAT_R8_UNORM,  1, 1 };
static DX11PlaneConfig_t DX11Planes420P16 = { DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM,    DXGI_FORMAT_R16_UNORM, 2, 2 };
static DX11PlaneConfig_t DX11Planes422P16 = { DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM,    DXGI_FORMAT_R16_UNORM, 2, 1 };
static DX11PlaneConfig_t DX11Planes444P16 = { DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_UNORM,    DXGI_FORMAT_R16_UNORM, 1, 1 };

static DX11PlaneConfig_t DX11Plane_RGBA8   = { DXGI_FORMAT_R8G8B8A8_UNORM,     DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, 1, 1 }; // YUY2, AYUV
static DX11PlaneConfig_t DX11Plane_RGB10A2 = { DXGI_FORMAT_R10G10B10A2_UNORM,  DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, 1, 1 }; // Y410
static DX11PlaneConfig_t DX11Plane_RGBA16  = { DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, 1, 1 }; // Y210, Y216, Y416

static const FmtConvParams_t s_FmtConvMapping[] = {
	// cformat    | str         |VP11Format                | DX11Format                |  pDX11Planes  |Packsize|PitchCoeff| CSType|Subsampling|CDepth| Func           |FuncSSSE3        |SupportLibplacebo
	{CF_NONE,      L"unknown",   DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_UNKNOWN,               nullptr,       0, 0,        CS_YUV,    0,       0,     nullptr,                  nullptr, false},
	{CF_NV12,      L"NV12",      DXGI_FORMAT_NV12,           DXGI_FORMAT_NV12,          &DX11PlanesNV12,       1, 3,        CS_YUV,  420,       8,     &CopyFrameAsIs,           nullptr, true},
	{CF_P010,      L"P010",      DXGI_FORMAT_P010,           DXGI_FORMAT_P010,          &DX11PlanesP01x,       2, 3,        CS_YUV,  420,       16,    &CopyFrameAsIs,           nullptr, true},
	{CF_P016,      L"P016",      DXGI_FORMAT_P016,           DXGI_FORMAT_P016,          &DX11PlanesP01x,       2, 3,        CS_YUV,  420,       16,    &CopyFrameAsIs,           nullptr, false},
	{CF_YUY2,      L"YUY2",      DXGI_FORMAT_YUY2,           DXGI_FORMAT_YUY2,         &DX11Plane_RGBA8,       2, 2,        CS_YUV,  422,       8,     &CopyFrameAsIs,           nullptr, false},
	{CF_P210,      L"P210",      DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_PLANAR,        &DX11PlanesP21x,       2, 4,        CS_YUV,  422,       16,    &CopyFrameAsIs,           nullptr, false},
	{CF_P216,      L"P216",      DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_PLANAR,        &DX11PlanesP21x,       2, 4,        CS_YUV,  422,       16,    &CopyFrameAsIs,           nullptr, false},
	{CF_Y210,      L"Y210",      DXGI_FORMAT_Y210,           DXGI_FORMAT_Y210,        &DX11Plane_RGBA16,       4, 2,        CS_YUV,  422,       10,    &CopyFrameAsIs,           nullptr, false},
	{CF_Y216,      L"Y216",      DXGI_FORMAT_Y216,           DXGI_FORMAT_Y216,        &DX11Plane_RGBA16,       4, 2,        CS_YUV,  422,       16,    &CopyFrameAsIs,           nullptr, false},
	{CF_AYUV,      L"AYUV",      DXGI_FORMAT_AYUV,           DXGI_FORMAT_AYUV,         &DX11Plane_RGBA8,       4, 2,        CS_YUV,  444,       8,     &CopyFrameAsIs,           nullptr, false},
	{CF_Y410,      L"Y410",      DXGI_FORMAT_Y410,           DXGI_FORMAT_Y410,       &DX11Plane_RGB10A2,       4, 2,        CS_YUV,  444,       10,    &CopyFrameAsIs,           nullptr, false},
	{CF_Y416,      L"Y416",      DXGI_FORMAT_Y416,           DXGI_FORMAT_Y416,        &DX11Plane_RGBA16,       8, 2,        CS_YUV,  444,       16,    &CopyFrameAsIs,           nullptr, false},

	{CF_YV12,      L"YV12",      DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_UNKNOWN,        &DX11Planes420P,       1, 3,        CS_YUV,  420,       8,     &CopyFrameYV12,           nullptr, false},
	{CF_YV16,      L"YV16",      DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_PLANAR,        &DX11Planes422P,       1, 4,        CS_YUV,  422,       8,     &CopyFrameAsIs,           nullptr, false},
	{CF_YV24,      L"YV24",      DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_PLANAR,        &DX11Planes444P,       1, 6,        CS_YUV,  444,       8,     &CopyFrameAsIs,           nullptr, false},

	{CF_YUV420P8,  L"YUV420P8",  DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_PLANAR,        &DX11Planes420P,       1, 3,        CS_YUV,  420,       8,     &CopyFrameAsIs,           nullptr, false},
	{CF_YUV422P8,  L"YUV422P8",  DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_PLANAR,        &DX11Planes422P,       1, 4,        CS_YUV,  422,       8,     &CopyFrameAsIs,           nullptr, false},
	{CF_YUV444P8,  L"YUV444P8",  DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_PLANAR,        &DX11Planes444P,       1, 6,        CS_YUV,  444,       8,     &CopyFrameAsIs,           nullptr, false},

	{CF_YUV420P16, L"YUV420P16", DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_PLANAR,      &DX11Planes420P16,       2, 3,        CS_YUV,  420,       16,    &CopyFrameAsIs,           nullptr, false},
	{CF_YUV422P16, L"YUV422P16", DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_PLANAR,      &DX11Planes422P16,       2, 4,        CS_YUV,  422,       16,    &CopyFrameAsIs,           nullptr, false},
	{CF_YUV444P16, L"YUV444P16", DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_PLANAR,      &DX11Planes444P16,       2, 6,        CS_YUV,  444,       16,    &CopyFrameAsIs,           nullptr, false},

	{CF_GBRP8,     L"GBRP8",     DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_PLANAR,        &DX11Planes444P,       1, 6,        CS_RGB,  444,       8,     &CopyFrameAsIs,           nullptr, false},
	{CF_GBRP16,    L"GBRP16",    DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_PLANAR,      &DX11Planes444P16,       2, 6,        CS_RGB,  444,       16,    &CopyFrameAsIs,           nullptr, false},

	{CF_RGB24,     L"RGB24",     DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM,        nullptr,       3, 2,        CS_RGB,  444,       8,     &CopyFrameRGB24, &CopyRGB24_SSSE3, false},
	{CF_XRGB32,    L"RGB32",     DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_B8G8R8X8_UNORM,        nullptr,       4, 2,        CS_RGB,  444,       8,     &CopyFrameAsIs,           nullptr, false},
	{CF_ARGB32,    L"ARGB32",    DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM,        nullptr,       4, 2,        CS_RGB,  444,       8,     &CopyFrameAsIs,           nullptr, false},

	{CF_r210,      L"r210",      DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_R10G10B10A2_UNORM,     nullptr,       4, 2,        CS_RGB,  444,       10,    &CopyFrameR210,           nullptr, false},

	{CF_RGB48,     L"RGB48",     DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_R16G16B16A16_UNORM,    nullptr,       6, 2,        CS_RGB,  444,       16,    &CopyFrameRGB48,          nullptr, false},
	{CF_BGR48,     L"BGR48",     DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_R16G16B16A16_UNORM,    nullptr,       6, 2,        CS_RGB,  444,       16,    &CopyFrameBGR48,          nullptr, false},
	{CF_BGRA64,    L"BGRA64",    DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_R16G16B16A16_UNORM,    nullptr,       8, 2,        CS_RGB,  444,       16,    &CopyFrameBGRA64,         nullptr, false},
	{CF_B64A,      L"b64a",      DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_R16G16B16A16_UNORM,    nullptr,       8, 2,        CS_RGB,  444,       16,    &CopyFrameB64A,           nullptr, false},

	{CF_Y8,        L"Y8",        DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_R8_UNORM,              nullptr,       1, 2,        CS_GRAY, 400,       8,     &CopyFrameAsIs,           nullptr, false},
	{CF_Y16,       L"Y16",       DXGI_FORMAT_UNKNOWN,        DXGI_FORMAT_R16_UNORM,             nullptr,       2, 2,        CS_GRAY, 400,       16,    &CopyFrameAsIs,           nullptr, false},
};
//                                                format   |width |height|size     |padding  |mapping     |pixel stride |row stride | swapped
static libplacebo_plane_t PlaceboPlanesNV12_0 = { PL_UNORM ,1     ,1     ,{8,0,0,0},{0,0,0,0},{0,-1,-1,-1},1            ,0          ,false};
static libplacebo_plane_t PlaceboPlanesNV12_1 = { PL_UNORM ,.5    ,.5    ,{8,8,0,0},{0,0,0,0},{1,2,-1,-1} ,2            ,0          ,false};
static libplacebo_plane_t PlaceboPlanesP010_0 = { PL_UNORM ,1     ,1     ,{16,0,0,0},{0,0,0,0},{0,-1,-1,-1},2            ,0          ,false };
static libplacebo_plane_t PlaceboPlanesP010_1 = { PL_UNORM ,.5    ,.5    ,{16,16,0,0},{0,0,0,0},{1,2,-1,-1} ,4            ,0          ,false };



static const FmtConvParamsLibplacebo_t s_FmtConvMappingLibplacebo[] = {
	{CF_NONE, {nullptr,nullptr,nullptr,nullptr}},
	{CF_NV12, {&PlaceboPlanesNV12_0,&PlaceboPlanesNV12_1,nullptr,nullptr}},
	{CF_P010, {&PlaceboPlanesP010_0,&PlaceboPlanesP010_1,nullptr,nullptr}},
	{CF_P016, {nullptr,nullptr,nullptr,nullptr}},
	{CF_YUY2, {nullptr,nullptr,nullptr,nullptr}},
	{CF_P210, {nullptr,nullptr,nullptr,nullptr}},
	{CF_P216, {nullptr,nullptr,nullptr,nullptr}},
	{CF_Y210, {nullptr,nullptr,nullptr,nullptr}},
	{CF_Y216, {nullptr,nullptr,nullptr,nullptr}},
	{CF_AYUV, {nullptr,nullptr,nullptr,nullptr}},
	{CF_Y410, {nullptr,nullptr,nullptr,nullptr}},
	{CF_Y416, {nullptr,nullptr,nullptr,nullptr}},
	{CF_YV12, {nullptr,nullptr,nullptr,nullptr}},
	{CF_YV16, {nullptr,nullptr,nullptr,nullptr}},
	{CF_YV24, {nullptr,nullptr,nullptr,nullptr}},
	{CF_YUV420P8, {nullptr,nullptr,nullptr,nullptr}},
	{CF_YUV422P8, {nullptr,nullptr,nullptr,nullptr}},
	{CF_YUV444P8, {nullptr,nullptr,nullptr,nullptr}},
	{CF_YUV420P16, {nullptr,nullptr,nullptr,nullptr}},
	{CF_YUV422P16, {nullptr,nullptr,nullptr,nullptr}},
	{CF_YUV444P16, {nullptr,nullptr,nullptr,nullptr}},
	{CF_GBRP8, {nullptr,nullptr,nullptr,nullptr}},
	{CF_GBRP16, {nullptr,nullptr,nullptr,nullptr}},
	{CF_RGB24, {nullptr,nullptr,nullptr,nullptr}},
	{CF_XRGB32, {nullptr,nullptr,nullptr,nullptr}},
	{CF_ARGB32, {nullptr,nullptr,nullptr,nullptr}},
	{CF_r210, {nullptr,nullptr,nullptr,nullptr}},
	{CF_RGB48, {nullptr,nullptr,nullptr,nullptr}},
	{CF_BGR48, {nullptr,nullptr,nullptr,nullptr}},
	{CF_BGRA64, {nullptr,nullptr,nullptr,nullptr}},
	{CF_B64A, {nullptr,nullptr,nullptr,nullptr}},
	{CF_Y8, {nullptr,nullptr,nullptr,nullptr}},
	{CF_Y16, {nullptr,nullptr,nullptr,nullptr}},
};
// Remarks:
// 1. The table lists all possible formats. The real situation depends on the capabilities of the graphics card and drivers.
// 2. We do not use DXVA2 processor for AYUV format, it works very poorly.

const FmtConvParams_t& GetFmtConvParams(const ColorFormat_t fmt)
{
	ASSERT(fmt == s_FmtConvMapping[fmt].cformat);
	return s_FmtConvMapping[fmt];
}

const FmtConvParamsLibplacebo_t& GetFmtConvParamsLibplacebo(const ColorFormat_t fmt)
{
	ASSERT(fmt == s_FmtConvMappingLibplacebo[fmt].cformat);
	return s_FmtConvMappingLibplacebo[fmt];
}

const FmtConvParams_t& GetFmtConvParams(const CMediaType* pmt)
{
	const ColorFormat_t fmt = GetColorFormat(pmt);
	return GetFmtConvParams(fmt);
}

CopyFrameDataFn GetCopyFunction(const FmtConvParams_t& params)
{
	if (CPUInfo::HaveSSSE3() && params.FuncSSSE3) {
		return params.FuncSSSE3;
	}
	return params.Func;
}

void CopyFrameAsIs(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	if (dst_pitch == src_pitch) {
		memcpy(dst, src, dst_pitch * lines);
		return;
	}

	const UINT linesize = std::min((UINT)abs(src_pitch), dst_pitch);

	for (UINT y = 0; y < lines; ++y) {
		memcpy(dst, src, linesize);
		src += src_pitch;
		dst += dst_pitch;
	}
}

void CopyGpuFrame_SSE41(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	if (dst_pitch == src_pitch) {
		gpu_memcpy(dst, src, dst_pitch * lines);
		return;
	}

	const UINT linesize = std::min((UINT)abs(src_pitch), dst_pitch);

	for (UINT y = 0; y < lines; ++y) {
		gpu_memcpy(dst, src, linesize);
		src += src_pitch;
		dst += dst_pitch;
	}
}

void CopyFrameRGB24(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	UINT line_pixels  = abs(src_pitch) / 3;
	UINT line_pixels4 = line_pixels & ~(4u - 1);

	for (UINT y = 0; y < lines; ++y) {
		uint32_t* src32 = (uint32_t*)src;
		uint32_t* dst32 = (uint32_t*)dst;

		UINT i = 0;
		for (; i < line_pixels4; i += 4) {
			uint32_t sa = *src32++;
			uint32_t sb = *src32++;
			uint32_t sc = *src32++;

			*dst32++ = sa;
			*dst32++ = (sa >> 24) | (sb << 8);
			*dst32++ = (sb >> 16) | (sc << 16);
			*dst32++ = sc >> 8;
		}

		if (i < line_pixels) {
			if (line_pixels & 1) {
				*dst32 = *src32;
			} else {
				uint32_t sa = *src32++;
				uint32_t sb = *src32;

				*dst32++ = sa;
				*dst32 = (sa >> 24) | (sb << 8);
			}
		}

		src += src_pitch;
		dst += dst_pitch;
	}
}

void CopyRGB24_SSSE3(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	UINT line_pixels   = abs(src_pitch) / 3;
	UINT line_pixels4  = line_pixels & ~(4u - 1);
	UINT line_pixels16 = line_pixels & ~(16u - 1);
	__m128i mask = _mm_setr_epi8(0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, 9, 10, 11, -1);

	for (UINT y = 0; y < lines; ++y) {
		__m128i *src128 = (__m128i*)src;
		__m128i *dst128 = (__m128i*)dst;

		UINT i = 0;
		for (; i < line_pixels16; i += 16) {
			__m128i sa = _mm_load_si128(src128++);
			__m128i sb = _mm_load_si128(src128++);
			__m128i sc = _mm_load_si128(src128++);

			__m128i val = _mm_shuffle_epi8(sa, mask);
			_mm_store_si128(dst128++, val);
			val = _mm_shuffle_epi8(_mm_alignr_epi8(sb, sa, 12), mask);
			_mm_store_si128(dst128++, val);
			val = _mm_shuffle_epi8(_mm_alignr_epi8(sc, sb, 8), mask);
			_mm_store_si128(dst128++, val);
			val = _mm_shuffle_epi8(_mm_alignr_epi8(sc, sc, 4), mask);
			_mm_store_si128(dst128++, val);
		}

		uint32_t* src32 = (uint32_t*)src128;
		uint32_t* dst32 = (uint32_t*)dst128;
		for (; i < line_pixels4; i += 4) {
			uint32_t sa = *src32++;
			uint32_t sb = *src32++;
			uint32_t sc = *src32++;

			*dst32++ = sa;
			*dst32++ = (sa >> 24) | (sb << 8);
			*dst32++ = (sb >> 16) | (sc << 16);
			*dst32++ = sc >> 8;
		}

		if (i < line_pixels) {
			if (line_pixels & 1) {
				*dst32 = *src32;
			} else {
				uint32_t sa = *src32++;
				uint32_t sb = *src32;

				*dst32++ = sa;
				*dst32 = (sa >> 24) | (sb << 8);
			}
		}

		src += src_pitch;
		dst += dst_pitch;
	}
}

void CopyFrameRGB48(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	UINT line_pixels  = abs(src_pitch) / 6;
	UINT line_pixels4 = line_pixels & ~(4u - 1);

	for (UINT y = 0; y < lines; ++y) {
		uint64_t* src64 = (uint64_t*)src;
		uint64_t* dst64 = (uint64_t*)dst;
		for (UINT i = 0; i < line_pixels4; i += 4) {
			uint64_t sa = src64[0];
			uint64_t sb = src64[1];
			uint64_t sc = src64[2];

			dst64[i + 0] = sa;
			dst64[i + 1] = (sa >> 48) | (sb << 16);
			dst64[i + 2] = (sb >> 32) | (sc << 32);
			dst64[i + 3] = sc >> 16;

			src64 += 3;
		}

		src += src_pitch;
		dst += dst_pitch;
	}
}

void CopyRGB48_SSSE3(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	UINT line_pixels  = abs(src_pitch) / 6;
	UINT line_pixels8 = line_pixels & ~(8u - 1);

	__m128i mask = _mm_setr_epi8(0, 1, 2, 3, 4, 5, -1, -1, 6, 7, 8, 9, 10, 11, -1, -1);

	for (UINT y = 0; y < lines; ++y) {
		__m128i *src128 = (__m128i*)src;
		__m128i *dst128 = (__m128i*)dst;
		for (UINT i = 0; i < line_pixels8; i += 8) {
			__m128i sa = _mm_load_si128(src128);
			__m128i sb = _mm_load_si128(src128 + 1);
			__m128i sc = _mm_load_si128(src128 + 2);

			__m128i val = _mm_shuffle_epi8(sa, mask);
			_mm_store_si128(dst128, val);
			val = _mm_shuffle_epi8(_mm_alignr_epi8(sb, sa, 12), mask);
			_mm_store_si128(dst128 + 1, val);
			val = _mm_shuffle_epi8(_mm_alignr_epi8(sc, sb, 8), mask);
			_mm_store_si128(dst128 + 2, val);
			val = _mm_shuffle_epi8(_mm_alignr_epi8(sc, sc, 4), mask);
			_mm_store_si128(dst128 + 3, val);

			src128 += 3;
			dst128 += 4;
		}

		src += src_pitch;
		dst += dst_pitch;
	}
}

void CopyFrameBGR48(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	UINT line_pixels  = abs(src_pitch) / 6;
	UINT line_pixels4 = line_pixels & ~(4u - 1);

	for (UINT y = 0; y < lines; ++y) {
		uint64_t* src64 = (uint64_t*)src;
		uint64_t* dst64 = (uint64_t*)dst;

		UINT i = 0;
		for (; i < line_pixels4; i += 4) {
			uint64_t sa = *src64++;
			uint64_t sb = *src64++;
			uint64_t sc = *src64++;

			*dst64++ = ((sa & 0xffff) << 32) | (sa & 0xffff0000) | ((sa & 0xffff00000000) >> 32);
			*dst64++ = ((sa & 0xffff000000000000) >> 16) | ((sb & 0xffff) << 16) | ((sb & 0xffff0000) >> 16);
			*dst64++ = (sb & 0xffff00000000) | ((sb & 0xffff000000000000) >> 32) | (sc & 0xffff);
			*dst64++ = ((sc & 0xffff0000) << 16) | ((sc & 0xffff00000000) >> 16) | ((sc & 0xffff000000000000) >> 48);
		}

		if (UINT remainder = line_pixels - i) {
			uint64_t sa = *src64++;
			*dst64++ = ((sa & 0xffff) << 32) | (sa & 0xffff0000) | ((sa & 0xffff00000000) >> 32);

			if (remainder==2) {
				uint64_t sb = *(uint32_t*)src64;
				*dst64 = ((sa & 0xffff000000000000) >> 16) | ((sb & 0xffff) << 16) | ((sb & 0xffff0000) >> 16);
			}
			else if (remainder == 3) {
				uint64_t sb = *src64++;
				uint64_t sc = *(uint32_t*)src64;
				*dst64++ = ((sa & 0xffff000000000000) >> 16) | ((sb & 0xffff) << 16) | ((sb & 0xffff0000) >> 16);
				*dst64 = (sb & 0xffff00000000) | ((sb & 0xffff000000000000) >> 32) | (sc & 0xffff);
			}
		}

		src += src_pitch;
		dst += dst_pitch;
	}
}

void CopyFrameBGRA64(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	UINT line_pixels = abs(src_pitch) / 8;

	for (UINT y = 0; y < lines; ++y) {
		uint64_t* src64 = (uint64_t*)src;
		uint64_t* dst64 = (uint64_t*)dst;
		for (UINT i = 0; i < line_pixels; ++i) {
			dst64[i] =
				((src64[i] & 0x000000000000ffff) << 32) |
				((src64[i] & 0x0000ffff00000000) >> 32) |
				( src64[i] & 0xffff0000ffff0000);
		}
		src += src_pitch;
		dst += dst_pitch;
	}
}

void CopyFrameB64A(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	UINT line_pixels = abs(src_pitch) / 8;

	for (UINT y = 0; y < lines; ++y) {
		uint64_t* src64 = (uint64_t*)src;
		uint64_t* dst64 = (uint64_t*)dst;
		for (UINT i = 0; i < line_pixels; ++i) {
			dst64[i] =
				((src64[i] & 0xFF00FF00FF000000) >> 24) +
				((src64[i] & 0x00FF00FF00FF0000) >>  8) +
				((src64[i] & 0x000000000000FF00) << 40) +
				((src64[i] & 0x00000000000000FF) << 56);
		}
		src += src_pitch;
		dst += dst_pitch;
	}
}

void CopyFrameYV12(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	ASSERT(src_pitch > 0);

	if (dst_pitch == src_pitch) {
		memcpy(dst, src, dst_pitch * lines);
		return;
	}

	const UINT chromaheight = lines / 3;
	const UINT lumaheight = chromaheight * 2;

	for (UINT y = 0; y < lumaheight; ++y) {
		memcpy(dst, src, src_pitch);
		src += src_pitch;
		dst += dst_pitch;
	}

	src_pitch /= 2;
	dst_pitch /= 2;
	for (UINT y = 0; y < chromaheight; ++y) {
		memcpy(dst, src, src_pitch);
		src += src_pitch;
		dst += dst_pitch;
		memcpy(dst, src, src_pitch);
		src += src_pitch;
		dst += dst_pitch;
	}
}

void CopyFrameY410(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	ASSERT(src_pitch > 0);
	UINT line_pixels = src_pitch / 4;

	for (UINT y = 0; y < lines; ++y) {
		uint32_t* src32 = (uint32_t*)src;
		uint32_t* dst32 = (uint32_t*)dst;
		for (UINT i = 0; i < line_pixels; i++) {
			uint32_t t = src32[i];
			// U = (t & 0x000003ff); Y = (t & 0x000ffc00) >> 10; V = (t & 0x3ff00000) >> 20; A = (t & 0xC0000000) >> 30;
			//dst32[i] = (t & 0xC0000000) | ((t & 0x000fffff) << 10) | ((t & 0x3ff00000) >> 20); // to D3DFMT_A2R10G10B10
			dst32[i] = (t & 0xfff00000) | ((t & 0x000003ff) << 10) | ((t & 0x000ffc00) >> 10); // to D3DFMT_A2B10G10R10
		}
		src += src_pitch;
		dst += dst_pitch;
	}
}

void CopyFrameR210(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	ASSERT(src_pitch > 0);
	UINT line_pixels = src_pitch / 4;

	for (UINT y = 0; y < lines; ++y) {
		uint32_t* src32 = (uint32_t*)src;
		uint32_t* dst32 = (uint32_t*)dst;
		for (UINT i = 0; i < line_pixels; i++) {
			const uint32_t t = src32[i];
			uint32_t r = ((t & 0x0000003f) << 4) | ((t & 0x0000f000) >> 12);
			uint32_t g = ((t & 0x00fc0000) >> 8) | ((t & 0x00000f00) << 8);
			uint32_t b = ((t & 0xff000000) >> 4) | ((t & 0x00030000) << 12);
			dst32[i] = r | g | b;
		}
		src += src_pitch;
		dst += dst_pitch;
	}
}

void ConvertXRGB10toXRGB8(const UINT lines, BYTE* dst, UINT dst_pitch, const BYTE* src, int src_pitch)
{
	UINT line_pixels = abs(src_pitch) / 4;

	for (UINT y = 0; y < lines; ++y) {
		uint32_t* src32 = (uint32_t*)src;
		uint32_t* dst32 = (uint32_t*)dst;
		for (UINT i = 0; i < line_pixels; i++) {
			uint32_t t = src32[i];
			dst32[i] = ((t & 0x000003fc) << 14)
					 | ((t & 0x000ff000) >> 4)
					 | ((t & 0x3fc00000) >> 22);
		}
		src += src_pitch;
		dst += dst_pitch;
	}
}

void ClipToSurface(const int texW, const int texH, RECT& s, RECT& d)
{
	const int sw = s.right - s.left;
	const int sh = s.bottom - s.top;
	const int dw = d.right - d.left;
	const int dh = d.bottom - d.top;

	if (d.left >= texW || d.right < 0 || d.top >= texH || d.bottom < 0
		|| sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) {
		SetRectEmpty(&s);
		SetRectEmpty(&d);
		return;
	}

	if (d.right > texW) {
		s.right -= (d.right - texW) * sw / dw;
		d.right = texW;
	}
	if (d.bottom > texH) {
		s.bottom -= (d.bottom - texH) * sh / dh;
		d.bottom = texH;
	}
	if (d.left < 0) {
		s.left += (0 - d.left) * sw / dw;
		d.left = 0;
	}
	if (d.top < 0) {
		s.top += (0 - d.top) * sh / dh;
		d.top = 0;
	}

	return;
}

void set_colorspace(const DXVA2_ExtendedFormat extfmt, mp_colorspace& colorspace)
{
	colorspace = {};

	if (extfmt.value == 0) {
		colorspace.space = MP_CSP_RGB;
		colorspace.levels = MP_CSP_LEVELS_PC;
		return;
	}

	switch (extfmt.NominalRange) {
	case DXVA2_NominalRange_0_255:   colorspace.levels = MP_CSP_LEVELS_PC;   break;
	case DXVA2_NominalRange_16_235:  colorspace.levels = MP_CSP_LEVELS_TV;   break;
	default:
		colorspace.levels = MP_CSP_LEVELS_AUTO;
	}

	switch (extfmt.VideoTransferMatrix) {
	case DXVA2_VideoTransferMatrix_BT709:     colorspace.space = MP_CSP_BT_709;     break;
	case DXVA2_VideoTransferMatrix_BT601:     colorspace.space = MP_CSP_BT_601;     break;
	case DXVA2_VideoTransferMatrix_SMPTE240M: colorspace.space = MP_CSP_SMPTE_240M; break;
	case MFVideoTransferMatrix_BT2020_10:     colorspace.space = MP_CSP_BT_2020_NC; break;
	case VIDEOTRANSFERMATRIX_YCgCo:           colorspace.space = MP_CSP_YCGCO;      break;
	default:
		colorspace.space = MP_CSP_AUTO;
	}

	switch (extfmt.VideoPrimaries) {
	case DXVA2_VideoPrimaries_BT709:         colorspace.primaries = MP_CSP_PRIM_BT_709;     break;
	case DXVA2_VideoPrimaries_BT470_2_SysM:  colorspace.primaries = MP_CSP_PRIM_BT_470M;    break;
	case DXVA2_VideoPrimaries_BT470_2_SysBG: colorspace.primaries = MP_CSP_PRIM_BT_601_625; break;
	case DXVA2_VideoPrimaries_SMPTE170M:
	case DXVA2_VideoPrimaries_SMPTE240M:     colorspace.primaries = MP_CSP_PRIM_BT_601_525; break;
	case MFVideoPrimaries_BT2020:            colorspace.primaries = MP_CSP_PRIM_BT_2020;    break;
	case MFVideoPrimaries_DCI_P3:            colorspace.primaries = MP_CSP_PRIM_DCI_P3;     break;
	default:
		colorspace.primaries = MP_CSP_PRIM_AUTO;
	}

	switch (extfmt.VideoTransferFunction) {
	case DXVA2_VideoTransFunc_10:      colorspace.gamma = MP_CSP_TRC_LINEAR;  break;
	case DXVA2_VideoTransFunc_18:      colorspace.gamma = MP_CSP_TRC_GAMMA18; break;
	case DXVA2_VideoTransFunc_20:      colorspace.gamma = MP_CSP_TRC_GAMMA20; break;
	case DXVA2_VideoTransFunc_22:      colorspace.gamma = MP_CSP_TRC_GAMMA22; break;
	case DXVA2_VideoTransFunc_709:
	case DXVA2_VideoTransFunc_240M:
	case MFVideoTransFunc_2020_const:
	case MFVideoTransFunc_2020:        colorspace.gamma = MP_CSP_TRC_BT_1886; break;
	case DXVA2_VideoTransFunc_sRGB:    colorspace.gamma = MP_CSP_TRC_SRGB;    break;
	case DXVA2_VideoTransFunc_28:      colorspace.gamma = MP_CSP_TRC_GAMMA28; break;
	case MFVideoTransFunc_2084:        colorspace.gamma = MP_CSP_TRC_PQ;      break;
	case MFVideoTransFunc_HLG:         colorspace.gamma = MP_CSP_TRC_HLG;     break;
	default:
		colorspace.gamma = MP_CSP_TRC_AUTO;
	}
}

BITMAPINFOHEADER* GetBIHfromVIHs(const AM_MEDIA_TYPE* pmt)
{
	if (pmt->formattype == FORMAT_VideoInfo2) {
		return &((VIDEOINFOHEADER2*)pmt->pbFormat)->bmiHeader;
	}

	if (pmt->formattype == FORMAT_VideoInfo) {
		return &((VIDEOINFOHEADER*)pmt->pbFormat)->bmiHeader;
	}

	return nullptr;
}

HRESULT SaveToBMP(BYTE* src, const UINT src_pitch, const UINT width, const UINT height, const UINT bitdepth, const wchar_t* filename)
{
	if (!src || !filename) {
		return E_POINTER;
	}

	if (!src_pitch || !width || !height || (bitdepth != 8 && bitdepth != 32)) {
		return E_ABORT;
	}

	const UINT tablecolors = (bitdepth == 8) ? 256 : 0;
	const UINT dst_pitch = width * bitdepth / 8;
	const UINT len = dst_pitch * height;

	std::unique_ptr<BYTE[]> dib(new(std::nothrow) BYTE[sizeof(BITMAPINFOHEADER) + tablecolors * 4 + len]);

	if (dib) {
		BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)dib.get();
		ZeroMemory(bih, sizeof(BITMAPINFOHEADER));
		bih->biSize = sizeof(BITMAPINFOHEADER);
		bih->biWidth = width;
		bih->biHeight = -(LONG)height;
		bih->biBitCount = bitdepth;
		bih->biPlanes = 1;
		bih->biSizeImage = DIBSIZE(*bih);
		bih->biClrUsed = tablecolors;

		BYTE* p = (BYTE*)(bih + 1);
		for (unsigned i = 0; i < tablecolors; i++) {
			*p++ = (BYTE)i;
			*p++ = (BYTE)i;
			*p++ = (BYTE)i;
			*p++ = 0;
		}

		CopyFrameAsIs(height, p, dst_pitch, src, src_pitch);

		BITMAPFILEHEADER bfh;
		bfh.bfType = 0x4d42;
		bfh.bfOffBits = sizeof(bfh) + sizeof(BITMAPINFOHEADER) + tablecolors * 4;
		bfh.bfSize = bfh.bfOffBits + len;
		bfh.bfReserved1 = bfh.bfReserved2 = 0;

		FILE* fp;
		if (_wfopen_s(&fp, filename, L"wb") == 0) {
			fwrite(&bfh, sizeof(bfh), 1, fp);
			fwrite(dib.get(), sizeof(BITMAPINFOHEADER) + tablecolors * 4 + len, 1, fp);
			fclose(fp);

			return S_OK;
		}
	}

	return E_FAIL;
}

HRESULT SaveToImage(BYTE* src, const UINT pitch, const UINT width, const UINT height, const UINT bitdepth, const CStdStringW filename)
{
	if (!src) {
		return E_POINTER;
	}

	if (!pitch || !width || !height || !filename.length()) {
		return E_INVALIDARG;
	}

	WICPixelFormatGUID format = {};
	if (bitdepth == 32) {
		format = GUID_WICPixelFormat32bppBGR;
	}
	else if (bitdepth == 24) {
		format = GUID_WICPixelFormat24bppBGR;
	}
	else if (bitdepth == 8) {
		format = GUID_WICPixelFormat8bppGray;
	}
	else {
		return E_INVALIDARG;
	}

	GUID wicFormat = {};
	CStdStringW ext;
	ext.assign(filename, filename.find_last_of(L"."));
	ext = ext.ToLower();
	
	if (ext == L".bmp") {
		wicFormat = GUID_ContainerFormatBmp;
	}
	else if (ext == L".png") {
		wicFormat = GUID_ContainerFormatPng;
	}
	else if (ext == L".jpg" || ext == L".jpeg") {
		wicFormat = GUID_ContainerFormatJpeg;
	}
	else if (ext == L".tif" || ext == L".tiff") {
		wicFormat = GUID_ContainerFormatTiff;
	}
	else {
		return E_INVALIDARG;
	}

	Microsoft::WRL::ComPtr<IWICImagingFactory> pWICFactory;
	Microsoft::WRL::ComPtr<IWICBitmapEncoder> pEncoder;
	Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> pFrame;
	Microsoft::WRL::ComPtr<IWICStream> pStream;

	HRESULT hr = CoCreateInstance(
		CLSID_WICImagingFactory1, // we use CLSID_WICImagingFactory1 to support Windows 7 without Platform Update
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_IWICImagingFactory,
		(LPVOID*)&pWICFactory
	);

	if (SUCCEEDED(hr)) {
		hr = pWICFactory->CreateStream(&pStream);
	};
	if (SUCCEEDED(hr)) {
		hr = pStream->InitializeFromFilename(filename.data(), GENERIC_WRITE);
	}
	if (SUCCEEDED(hr)) {
		hr = pWICFactory->CreateEncoder(wicFormat, nullptr, &pEncoder);
	}
	if (SUCCEEDED(hr)) {
		hr = pEncoder->Initialize(pStream.Get(), WICBitmapEncoderNoCache);
	}
	if (SUCCEEDED(hr)) {
		hr = pEncoder->CreateNewFrame(&pFrame, nullptr);
	}
	if (SUCCEEDED(hr)) {
		hr = pFrame->Initialize(nullptr);
	}
	if (SUCCEEDED(hr)) {
		hr = pFrame->SetSize(width, height);
	}
	if (SUCCEEDED(hr)) {
		hr = pFrame->SetPixelFormat(&format);
	}
	if (SUCCEEDED(hr)) {
		hr = pFrame->WritePixels(height, pitch, pitch * height, src);
	}
	if (SUCCEEDED(hr)) {
		hr = pFrame->Commit();
	}
	if (SUCCEEDED(hr)) {
		hr = pEncoder->Commit();
	}

	return hr;
}

DXVA2_ExtendedFormat SpecifyExtendedFormat(DXVA2_ExtendedFormat exFormat, const FmtConvParams_t& fmtParams, const UINT width, const UINT height)
{
	if (fmtParams.CSType == CS_RGB) {
		exFormat.value = 0u;
	}
	else if (fmtParams.CSType == CS_YUV) {
		// https://docs.microsoft.com/en-us/windows/desktop/api/dxva2api/ns-dxva2api-dxva2_extendedformat

		if (fmtParams.Subsampling != 420) {
			exFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Unknown;
		}
		else if (exFormat.VideoChromaSubsampling == DXVA2_VideoChromaSubsampling_Unknown) {
			exFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
		}

		if (exFormat.NominalRange == DXVA2_NominalRange_Unknown) {
			exFormat.NominalRange = DXVA2_NominalRange_16_235;
		}

		if (exFormat.VideoTransferMatrix == DXVA2_VideoTransferMatrix_Unknown) {
			if (width <= 1024 && height <= 576) { // SD (more reliable way to determine SD than MicroSoft offers)
				exFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT601;
			}
			else { // HD
				exFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT709;
			}
		}

		if (exFormat.VideoLighting == DXVA2_VideoLighting_Unknown) {
			exFormat.VideoLighting = DXVA2_VideoLighting_dim;
		}

		if (exFormat.VideoPrimaries == DXVA2_VideoPrimaries_Unknown) {
			exFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709; // pick BT.709 to minimize damage
		}

		if (exFormat.VideoTransferFunction == DXVA2_VideoTransFunc_Unknown) {
			exFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;
		}
	}

	return exFormat;
}

void GetExtendedFormatString(LPCSTR (&strs)[6], const DXVA2_ExtendedFormat exFormat, const ColorSystem_t colorSystem)
{
	static LPCSTR chromalocation[] = { "unknown", "Center(MPEG-1)", nullptr, nullptr, nullptr, "Left(MPEG-2)", "TopLeft(PAL DV)", "TopLeft(Co-sited)" };
	static LPCSTR nominalrange[] = { "unknown", "0-255", "16-235", "48-208" };
	static LPCSTR transfermatrix[] = { "unknown", "BT.709", "BT.601", "SMPTE 240M", "BT.2020", nullptr, nullptr, "YCgCo" };
	static LPCSTR lighting[] = { "unknown", "bright", "office", "dim", "dark" };
	static LPCSTR primaries[] = { "unknown", "Reserved", "BT.709", "BT.470-4 System M", "BT.470-4 System B,G",
		"SMPTE 170M", "SMPTE 240M", "EBU Tech. 3213", "SMPTE C", "BT.2020" };
	static LPCSTR transfunc[] = { "unknown", "Linear RGB", "1.8 gamma", "2.0 gamma", "2.2 gamma", "BT.709", "SMPTE 240M",
		"sRGB", "2.8 gamma", "Log100", "Log316", "Symmetric BT.709", "Constant luminance BT.2020", "Non-constant luminance BT.2020",
		"2.6 gamma", "SMPTE ST 2084 (PQ)", "ARIB STD-B67 (HLG)"};

	auto getDesc = [] (unsigned num, LPCSTR* descs, unsigned count) {
		if (num < count && descs[num]) {
			return descs[num];
		} else {
			return "invalid";
		}
	};

	if (colorSystem == CS_YUV) {
		strs[0] = getDesc(exFormat.VideoChromaSubsampling, chromalocation, std::size(chromalocation));
		strs[1] = getDesc(exFormat.NominalRange, nominalrange, std::size(nominalrange));
		strs[2] = getDesc(exFormat.VideoTransferMatrix, transfermatrix, std::size(transfermatrix));
		strs[3] = getDesc(exFormat.VideoLighting, lighting, std::size(lighting));
		strs[4] = getDesc(exFormat.VideoPrimaries, primaries, std::size(primaries));
		strs[5] = getDesc(exFormat.VideoTransferFunction, transfunc, std::size(transfunc));
	}
}
