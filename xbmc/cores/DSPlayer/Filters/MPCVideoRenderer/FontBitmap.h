/*
* (C) 2019-2021 see Authors.txt
*
* This file is part of MPC-BE.
*
* MPC-BE is free software; you can redistribute it and/or modify
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

#include "D3DCommon.h"

#ifndef FONTBITMAP_MODE
#define FONTBITMAP_MODE 2
// 0 - GDI
// 1 - GDI+ (no longer supported)
// 2 - DirectWrite
#endif

#define DUMP_BITMAP 0

struct Grid_t {
	UINT stepX = 0;
	UINT stepY = 0;
	UINT columns = 0;
	UINT lines = 0;
};

inline uint16_t A8R8G8B8toA8L8(uint32_t pix)
{
	uint32_t a = (pix & 0xff000000) >> 24;
	uint32_t r = (pix & 0x00ff0000) >> 16;
	uint32_t g = (pix & 0x0000ff00) >> 8;
	uint32_t b = (pix & 0x000000ff);
	uint32_t l = ((r * 1063 + g * 3576 + b * 361) / 5000);

	return (uint16_t)((a << 8) + l);
}

inline uint16_t X8R8G8B8toA8L8(uint32_t pix)
{
	uint32_t r = (pix & 0x00ff0000) >> 16;
	uint32_t g = (pix & 0x0000ff00) >> 8;
	uint32_t b = (pix & 0x000000ff);
	uint32_t l = ((r * 1063 + g * 3576 + b * 361) / 5000);

	return (uint16_t)((l << 8) + l); // the darker the more transparent
}

#if FONTBITMAP_MODE == 0

class CFontBitmapGDI
{
private:
	HBITMAP m_hBitmap = nullptr;
	DWORD* m_pBitmapBits = nullptr;
	UINT m_bmWidth = 0;
	UINT m_bmHeight = 0;
	std::vector<RECT> m_charCoords;
	SIZE m_MaxCharMetric = {};

public:
	CFontBitmapGDI()
	{
	}

	~CFontBitmapGDI()
	{
		DeleteObject(m_hBitmap);
	}

	HRESULT Initialize(const WCHAR* fontName, const int fontHeight, UINT fontFlags, const WCHAR* chars, UINT length)
	{
		DeleteObject(m_hBitmap);
		m_pBitmapBits = nullptr;
		m_charCoords.clear();

		HRESULT hr = S_OK;

		// Create a font. By specifying ANTIALIASED_QUALITY, we might get an
		// antialiased font, but this is not guaranteed.
		HFONT hFont = CreateFontW(
			fontHeight, 0, 0, 0,
			(fontFlags & D3DFONT_BOLD)   ? FW_BOLD : FW_NORMAL,
			(fontFlags & D3DFONT_ITALIC) ? TRUE    : FALSE,
			FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
			DEFAULT_PITCH, fontName);

		HDC hDC = CreateCompatibleDC(nullptr);
		SetMapMode(hDC, MM_TEXT);
		HFONT hFontOld = (HFONT)SelectObject(hDC, hFont);

		std::vector<SIZE> charSizes;
		charSizes.reserve(length);
		SIZE size;
		LONG maxWidth = 0;
		LONG maxHeight = 0;
		for (UINT i = 0; i < length; i++) {
			if (GetTextExtentPoint32W(hDC, &chars[i], 1, &size) == FALSE) {
				hr = E_FAIL;
				break;
			}
			charSizes.emplace_back(size);

			if (size.cx > maxWidth) {
				maxWidth = size.cx;
			}
			if (size.cy > maxHeight) {
				maxHeight = size.cy;
			}
		}

		if (S_OK == hr) {
			m_MaxCharMetric = { maxWidth, maxHeight };
			UINT stepX = m_MaxCharMetric.cx + 2;
			UINT stepY = m_MaxCharMetric.cy;
			UINT bmWidth = 128;
			UINT bmHeight = 128;
			UINT columns = bmWidth / stepX;
			UINT lines = bmHeight / stepY;

			while (length > lines * columns) {
				if (bmWidth <= bmHeight) {
					bmWidth *= 2;
				} else {
					bmHeight += 128;
				}
				columns = bmWidth / stepX;
				lines = bmHeight / stepY;
			};

			// Prepare to create a bitmap
			BITMAPINFO bmi = {};
			bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth       =  (LONG)bmWidth;
			bmi.bmiHeader.biHeight      = -(LONG)bmHeight;
			bmi.bmiHeader.biPlanes      = 1;
			bmi.bmiHeader.biCompression = BI_RGB;
			bmi.bmiHeader.biBitCount    = 32;

			// Create a bitmap for the font
			m_hBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, (void**)&m_pBitmapBits, nullptr, 0);
			m_bmWidth = bmWidth;
			m_bmHeight = bmHeight;

			HGDIOBJ m_hBitmapOld = SelectObject(hDC, m_hBitmap);

			SetTextColor(hDC, RGB(255, 255, 255));
			SetBkColor(hDC, 0x00000000);
			SetTextAlign(hDC, TA_TOP);

			UINT idx = 0;
			for (UINT y = 0; y < lines; y++) {
				for (UINT x = 0; x < columns; x++) {
					if (idx >= length) {
						break;
					}
					UINT X = x * stepX + 1;
					UINT Y = y * stepY;
					if (ExtTextOutW(hDC, X, Y, ETO_OPAQUE, nullptr, &chars[idx], 1, nullptr) == FALSE) {
						hr = E_FAIL;
						break;
					}
					RECT rect = {
						X,
						Y,
						X + charSizes[idx].cx,
						Y + charSizes[idx].cy
					};
					m_charCoords.emplace_back(rect);
					idx++;
				}
			}
			GdiFlush();

			SelectObject(hDC, m_hBitmapOld);
		}

		SelectObject(hDC, hFontOld);
		DeleteObject(hFont);
		DeleteDC(hDC);

		// set transparency
		const UINT nPixels = m_bmWidth * m_bmHeight;
		for (UINT i = 0; i < nPixels; i++) {
			uint32_t pix = m_pBitmapBits[i];
			uint32_t r = (pix & 0x00ff0000) >> 16;
			uint32_t g = (pix & 0x0000ff00) >> 8;
			uint32_t b = (pix & 0x000000ff);
			uint32_t l = ((r * 1063 + g * 3576 + b * 361) / 5000);
			m_pBitmapBits[i] = (l << 24) | (pix & 0x00ffffff); // the darker the more transparent
		}

#if _DEBUG && DUMP_BITMAP
		if (S_OK == hr) {
			SaveToBMP((BYTE*)m_pBitmapBits, m_bmWidth * 4, m_bmWidth, m_bmHeight, 32, L"c:\\temp\\font_gdi_bitmap.bmp");
		}
#endif

		return hr;
	}

	UINT GetWidth()
	{
		return m_bmWidth;
	}

	UINT GetHeight()
	{
		return m_bmHeight;
	}

	SIZE GetMaxCharMetric()
	{
		return m_MaxCharMetric;
	}

	HRESULT GetFloatCoords(FloatRect* pTexCoords, const UINT length)
	{
		ASSERT(pTexCoords);

		if (!m_hBitmap || !m_pBitmapBits || length != m_charCoords.size()) {
			return E_ABORT;
		}

		for (const auto coord : m_charCoords) {
			*pTexCoords++ = {
				(float)coord.left   / m_bmWidth,
				(float)coord.top    / m_bmHeight,
				(float)coord.right  / m_bmWidth,
				(float)coord.bottom / m_bmHeight,
			};
		}

		return S_OK;
	}

	HRESULT Lock(BYTE** ppData, UINT& uStride)
	{
		if (!m_hBitmap || !m_pBitmapBits) {
			return E_ABORT;
		}

		*ppData = (BYTE*)m_pBitmapBits;
		uStride = m_bmWidth * 4;

		return S_OK;
	}

	void Unlock()
	{
		// nothing
	}
};

typedef CFontBitmapGDI CFontBitmap;

#elif FONTBITMAP_MODE == 2

#include <dwrite.h>
#include <d2d1.h>
#include <wincodec.h>

class CFontBitmapDWrite
{
private:
	Microsoft::WRL::ComPtr<ID2D1Factory>       m_pD2D1Factory;
	Microsoft::WRL::ComPtr<IDWriteFactory>     m_pDWriteFactory;
	Microsoft::WRL::ComPtr<IWICImagingFactory> m_pWICFactory;

	Microsoft::WRL::ComPtr<IWICBitmap>     m_pWICBitmap;
	Microsoft::WRL::ComPtr<IWICBitmapLock> m_pWICBitmapLock;
	std::vector<RECT> m_charCoords;
	SIZE m_MaxCharMetric = {};

public:
	CFontBitmapDWrite()
	{
		HRESULT hr = D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
#ifdef _DEBUG
			{ D2D1_DEBUG_LEVEL_INFORMATION },
#endif
			&m_pD2D1Factory);
		DLogIf(FAILED(hr), L"D2D1CreateFactory() failed with error {}", HR2Str(hr));

		if (SUCCEEDED(hr)) {
			IDWriteFactory* pDWriteFactory;
			hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(m_pDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory));
			m_pDWriteFactory = pDWriteFactory;
			DLogIf(FAILED(hr), L"DWriteCreateFactory() failed with error {}", HR2Str(hr));
		}

		if (SUCCEEDED(hr)) {
			hr = CoCreateInstance(
				CLSID_WICImagingFactory1, // we use CLSID_WICImagingFactory1 to support Windows 7 without Platform Update
				nullptr,
				CLSCTX_INPROC_SERVER,
				IID_IWICImagingFactory,
				(LPVOID*)&m_pWICFactory
			);
			DLogIf(FAILED(hr), L"CoCreateInstance(WICImagingFactory) failed with error {}", HR2Str(hr));
		}
	}

	~CFontBitmapDWrite()
	{
		m_pWICBitmapLock= nullptr;
		m_pWICBitmap= nullptr;

		m_pWICFactory= nullptr;
		m_pDWriteFactory= nullptr;
		m_pD2D1Factory= nullptr;
	}

	HRESULT Initialize(const WCHAR* fontName, const int fontHeight, UINT fontFlags, const WCHAR* chars, UINT length)
	{
		if (!m_pWICFactory) {
			return E_ABORT;
		}

		m_pWICBitmapLock= nullptr;
		m_pWICBitmap= nullptr;
		m_charCoords.clear();

		Microsoft::WRL::ComPtr<IDWriteTextFormat> pTextFormat;
		HRESULT hr = m_pDWriteFactory->CreateTextFormat(
			fontName,
			nullptr,
			(fontFlags & D3DFONT_BOLD)   ? DWRITE_FONT_WEIGHT_BOLD  : DWRITE_FONT_WEIGHT_NORMAL,
			(fontFlags & D3DFONT_ITALIC) ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			fontHeight,
			L"", //locale
			&pTextFormat);
		if (FAILED(hr)) {
			return hr;
		}

		std::vector<SIZE> charSizes;
		charSizes.reserve(length);
		DWRITE_TEXT_METRICS textMetrics = {};
		float maxWidth = 0;
		float maxHeight = 0;

		for (UINT i = 0; i < length; i++) {
			IDWriteTextLayout* pTextLayout;
			hr = m_pDWriteFactory->CreateTextLayout(&chars[i], 1, pTextFormat.Get(), 0, 0, &pTextLayout);
			if (S_OK == hr) {
				hr = pTextLayout->GetMetrics(&textMetrics);
				pTextLayout->Release();
			}
			if (FAILED(hr)) {
				break;
			}

			SIZE size = { (LONG)ceil(textMetrics.widthIncludingTrailingWhitespace), (LONG)ceil(textMetrics.height) };
			charSizes.emplace_back(size);

			if (textMetrics.width > maxWidth) {
				maxWidth = textMetrics.width;
			}
			if (textMetrics.height > maxHeight) {
				maxHeight = textMetrics.height;
			}
			ASSERT(textMetrics.left == 0 && textMetrics.top == 0);
		}

		if (S_OK == hr) {
			m_MaxCharMetric = { (LONG)ceil(maxWidth), (LONG)ceil(maxHeight) };
			UINT stepX = m_MaxCharMetric.cx + 2;
			UINT stepY = m_MaxCharMetric.cy;
			UINT bmWidth = 128;
			UINT bmHeight = 128;
			UINT columns = bmWidth / stepX;
			UINT lines = bmHeight / stepY;

			while (length > lines * columns) {
				if (bmWidth <= bmHeight) {
					bmWidth *= 2;
				} else {
					bmHeight += 128;
				}
				columns = bmWidth / stepX;
				lines = bmHeight / stepY;
			};

			hr = m_pWICFactory->CreateBitmap(bmWidth, bmHeight, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &m_pWICBitmap);
			if (S_OK != hr) {
				return hr;
			}

			Microsoft::WRL::ComPtr<ID2D1RenderTarget>    pD2D1RenderTarget;
			Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> pD2D1Brush;

			D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
				D2D1_RENDER_TARGET_TYPE_DEFAULT,
				D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN),
				96, 96);

			hr = m_pD2D1Factory->CreateWicBitmapRenderTarget(m_pWICBitmap.Get(), &props, &pD2D1RenderTarget);
			if (S_OK != hr) {
				return hr;
			}
			hr = pD2D1RenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pD2D1Brush);
			if (S_OK != hr) {
				return hr;
			}

			m_charCoords.reserve(length);
			pD2D1RenderTarget->BeginDraw();
			UINT idx = 0;
			for (UINT y = 0; y < lines; y++) {
				for (UINT x = 0; x < columns; x++) {
					if (idx >= length) {
						break;
					}
					UINT X = x * stepX + 1;
					UINT Y = y * stepY;
					IDWriteTextLayout* pTextLayout;
					hr = m_pDWriteFactory->CreateTextLayout(&chars[idx], 1, pTextFormat.Get(), 0, 0, &pTextLayout);
					if (S_OK == hr) {
						pD2D1RenderTarget->DrawTextLayout({ (FLOAT)X, (FLOAT)Y }, pTextLayout, pD2D1Brush.Get());
						pTextLayout->Release();
					}
					if (FAILED(hr)) {
						break;
					}
					RECT rect = {
						X,
						Y,
						X + charSizes[idx].cx,
						Y + charSizes[idx].cy
					};
					m_charCoords.emplace_back(rect);
					idx++;
				}
			}
			pD2D1RenderTarget->EndDraw();

			pD2D1Brush= nullptr;
			pD2D1RenderTarget= nullptr;
		}

		pTextFormat= nullptr;

#if _DEBUG && DUMP_BITMAP
		if (S_OK == hr) {
			SaveBitmap(L"C:\\TEMP\\font_directwrite_bitmap.png");
		}
#endif

		return hr;
	}

	UINT GetWidth()
	{
		if (m_pWICBitmap) {
			UINT w, h;
			if (S_OK == m_pWICBitmap->GetSize(&w, &h)) {
				return w;
			}
		}

		return 0;
	}

	UINT GetHeight()
	{
		if (m_pWICBitmap) {
			UINT w, h;
			if (S_OK == m_pWICBitmap->GetSize(&w, &h)) {
				return h;
			}
		}

		return 0;
	}

	SIZE GetMaxCharMetric()
	{
		return m_MaxCharMetric;
	}

	HRESULT GetFloatCoords(FloatRect* pTexCoords, const UINT length)
	{
		ASSERT(pTexCoords);

		if (!m_pWICBitmap || length != m_charCoords.size()) {
			return E_ABORT;
		}

		UINT w, h;
		HRESULT hr = m_pWICBitmap->GetSize(&w, &h);
		if (FAILED(hr)) {
			return hr;
		}

		for (const auto coord : m_charCoords) {
			*pTexCoords++ = {
				(float)coord.left   / w,
				(float)coord.top    / h,
				(float)coord.right  / w,
				(float)coord.bottom / h,
			};
		}

		return S_OK;
	}

	HRESULT Lock(BYTE** ppData, UINT& uStride)
	{
		if (!m_pWICBitmap || m_pWICBitmapLock) {
			return E_ABORT;
		}

		UINT w, h;
		HRESULT hr = m_pWICBitmap->GetSize(&w, &h);
		if (FAILED(hr)) {
			return hr;
		}

		WICRect rcLock = { 0, 0, w, h };
		hr = m_pWICBitmap->Lock(&rcLock, WICBitmapLockRead, &m_pWICBitmapLock);
		if (S_OK == hr) {
			hr = m_pWICBitmapLock->GetStride(&uStride);
			if (S_OK == hr) {
				UINT cbBufferSize = 0;
				hr = m_pWICBitmapLock->GetDataPointer(&cbBufferSize, ppData);
			}
		}

		return hr;
	}

	void Unlock()
	{
		m_pWICBitmapLock= nullptr;
	}

private:
	HRESULT SaveBitmap(const WCHAR* filename)
	{
		if (!m_pWICBitmap) {
			return E_ABORT;
		}

		UINT w, h;
		HRESULT hr = m_pWICBitmap->GetSize(&w, &h);
		if (FAILED(hr)) {
			return hr;
		}

		GUID guidContainerFormat = GUID_NULL;

		if (auto ext = wcsrchr(filename, '.')) {
			// the "count" parameter for "_wcsnicmp" function must be greater than the length of the short string to be compared
			if (_wcsnicmp(ext, L".bmp", 8) == 0) {
				guidContainerFormat = GUID_ContainerFormatBmp;
			}
			else if (_wcsnicmp(ext, L".png", 8) == 0) {
				guidContainerFormat = GUID_ContainerFormatPng;
			}
		}

		if (guidContainerFormat == GUID_NULL) {
			return E_INVALIDARG;
		}

		Microsoft::WRL::ComPtr<IWICStream> pStream;
		Microsoft::WRL::ComPtr<IWICBitmapEncoder> pEncoder;
		Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> pFrameEncode;
		WICPixelFormatGUID format = GUID_WICPixelFormatDontCare;

		hr = m_pWICFactory->CreateStream(&pStream);
		if (SUCCEEDED(hr)) {
			hr = pStream->InitializeFromFilename(filename, GENERIC_WRITE);
		}
		if (SUCCEEDED(hr)) {
			hr = m_pWICFactory->CreateEncoder(guidContainerFormat, nullptr, &pEncoder);
		}
		if (SUCCEEDED(hr)) {
			hr = pEncoder->Initialize(pStream.Get(), WICBitmapEncoderNoCache);
		}
		if (SUCCEEDED(hr)) {
			hr = pEncoder->CreateNewFrame(&pFrameEncode, nullptr);
		}
		if (SUCCEEDED(hr)) {
			hr = pFrameEncode->Initialize(nullptr);
		}
		if (SUCCEEDED(hr)) {
			hr = pFrameEncode->SetSize(w, h);
		}
		if (SUCCEEDED(hr)) {
			hr = pFrameEncode->SetPixelFormat(&format);
		}
		if (SUCCEEDED(hr)) {
			hr = pFrameEncode->WriteSource(m_pWICBitmap.Get(), nullptr);
		}
		if (SUCCEEDED(hr)) {
			hr = pFrameEncode->Commit();
		}
		if (SUCCEEDED(hr)) {
			hr = pEncoder->Commit();
		}

		return hr;
	}
};

typedef CFontBitmapDWrite CFontBitmap;

#endif
