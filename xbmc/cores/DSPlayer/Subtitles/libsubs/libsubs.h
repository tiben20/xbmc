#pragma once
#include "ISubManager.h"
#include "../ILog.h"
#include <d3d11_1.h>
#define MPCSUBS_API __declspec(dllexport)

extern "C" {
  MPCSUBS_API bool CreateD3D9SubtitleManager(IDirect3DDevice9* pDevice, SIZE size, ILog* logger, SSubSettings settings, ISubManager** pManager);
  MPCSUBS_API bool CreateD3D11SubtitleManager(ID3D11Device1* pDevice, SIZE size, ILog* logger, SSubSettings settings, ISubManager** pManager);
  MPCSUBS_API bool DeleteSubtitleManager(ISubManager * pManager);
}
