/*
 * (C) 2019-2020 see Authors.txt
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

#include "windowing/windows/WinSystemWin32DX.h"

struct POINTVERTEX11 {
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT4 Color;
};

// CD3D11Quadrilateral

class CD3D11Quadrilateral : public ID3DResource
{
protected:


	bool m_bAlphaBlend = false;
	Microsoft::WRL::ComPtr<ID3D11BlendState> m_pBlendState;

	POINTVERTEX11 m_Vertices[4] = {};
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pDeviceContext;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_pVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_pInputLayout;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_pVertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pPixelShader;

public:
	~CD3D11Quadrilateral();

	HRESULT InitDeviceObjects();
	

	HRESULT Set(const float x1, const float y1, const float x2, const float y2, const float x3, const float y3, const float x4, const float y4, KODI::UTILS::COLOR::Color color);
	HRESULT Draw(ID3D11RenderTargetView* pRenderTargetView, const SIZE& rtSize);

	void OnDestroyDevice(bool fatal) override;
	void OnCreateDevice() override;
};

// CD3D11Rectangle

class CD3D11Rectangle : public CD3D11Quadrilateral
{
private:
	using CD3D11Quadrilateral::Set;

public:
	HRESULT Set(const RECT& rect, const SIZE& rtSize, const KODI::UTILS::COLOR::Color color);
};

// CD3D11Stripe

class CD3D11Stripe : public CD3D11Quadrilateral
{
private:
	using CD3D11Quadrilateral::Set;

public:
	HRESULT Set(const int x1, const int y1, const int x2, const int y2, const int thickness, const KODI::UTILS::COLOR::Color color);
};

// CD3D11Dots

class CD3D11Dots : public ID3DResource
{
protected:
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_pVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_pInputLayout;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_pVertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pPixelShader;

	SIZE m_RTSize = {};
	bool m_bAlphaBlend = false;
	std::vector<POINTVERTEX11> m_Vertices;

	virtual inline bool CheckNumPoints(const UINT num)
	{
		return (num > 0);
	}

	virtual void DrawPrimitive();

public:
	~CD3D11Dots();

	HRESULT InitDeviceObjects();
	void InvalidateDeviceObjects();

	void ClearPoints(SIZE& newRTSize);
	bool AddPoints(POINT* poins, const UINT size, const KODI::UTILS::COLOR::Color color);
	bool AddGFPoints(
		int Xstart, int Xstep,
		int Yaxis, int Yscale,
		int* Ydata, UINT Yoffset,
		const UINT size, const KODI::UTILS::COLOR::Color color);

	HRESULT UpdateVertexBuffer();
	void Draw();

	void OnDestroyDevice(bool fatal) override;
	void OnCreateDevice() override;
};

// CD3D11Lines

class CD3D11Lines : public CD3D11Dots
{
private:
	using CD3D11Dots::AddGFPoints;

protected:
	inline bool CheckNumPoints(const UINT num) override
	{
		return (num >= 2 && !(num & 1));
	}

	void DrawPrimitive() override;
	
};

// CD3D11Polyline

class CD3D11Polyline : public CD3D11Dots
{
protected:
	inline bool CheckNumPoints(const UINT num) override
	{
		return (num >= 2 || m_Vertices.size() && num > 0);
	}

	void DrawPrimitive() override;

};
