#pragma once
#include <Shlwapi.h>
#include "Utils/Geometry.h"
#include "Dsutil/smartptr.h"
/*windows runtime library for comptr*/
#include <wrl.h>
#include <wrl/client.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "minhook.lib")

//#define LOG_HR(hr) \
//  CLog::LogF(LOGERROR, "function call at line {} ends with error: {}", __LINE__, \
//             DX::GetErrorDescription(hr));

