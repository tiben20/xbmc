/* 
 *  Copyright (C) 2003-2006 Gabest
 *  http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#define WINVER      0x0600

#define WIN32_LEAN_AND_MEAN    // Exclude rarely-used stuff from Windows headers
#define _ATL_CStdString_EXPLICIT_CONSTRUCTORS  // some CStdString constructors will be explicit

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN    // Exclude rarely-used stuff from Windows headers
#endif

#include <vector>
#include <map>
#include <list>
#include <memory>

// TODO: reference additional headers your program requires here

#include <streams.h>
#include "utils/StdString.h"
#include "..\..\dsutil\DSUtil.h"
#include "..\..\dsutil\vd.h"
#include "..\..\DSUtil\Geometry.h"

#include <xmmintrin.h>
#include <emmintrin.h>

#define _USE_MATH_DEFINES
#include <math.h>
