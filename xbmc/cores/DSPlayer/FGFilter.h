/*
 *  Copyright (C) 2003-2006 Gabest
 *  http://www.gabest.org
 *
 *  Copyright (C) 2005-2010 Team XBMC
 *  http://www.xbmc.org
 *
 *	Copyright (C) 2010-2013 Eduard Kytmanov
 *	http://www.avmedia.su
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

#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

#pragma once

#include "streams.h"
#include <list>
#include <tinyxml.h>
#include "Utils/CharsetConverter.h"

class CFGFilter
{
public:
  enum Type {
    NONE,
    FILE,
    INTERNAL,
    REGISTRY,
    VIDEORENDERER
  };

  CFGFilter(const CLSID& clsid, Type type, std::string name = "");
  CFGFilter(Type type) { m_type = type; };
  virtual ~CFGFilter() {};

  CLSID GetCLSID() { return m_clsid; }
  std::string GetName() { return m_name; }
  std::wstring GetNameW() { 
    std::wstring nameW;
    g_charsetConverter.utf8ToW(m_name, nameW);
    return nameW; 
  }
  Type GetType() const { return m_type; }

  void AddType(const GUID& majortype, const GUID& subtype);

  virtual HRESULT Create(IBaseFilter** ppBF) = 0;
protected:
  CLSID m_clsid;
  std::string m_name;
  Type m_type;
  std::list<GUID> m_types;
};

class CFGFilterRegistry : public CFGFilter
{
protected:
  std::string m_DisplayName;
  IMoniker* m_pMoniker;

  void ExtractFilterData(BYTE* p, UINT len);

public:
  CFGFilterRegistry(IMoniker* pMoniker);
  CFGFilterRegistry(std::string DisplayName);
  CFGFilterRegistry(const CLSID& clsid);

  std::string GetDisplayName() { return m_DisplayName; }
  IMoniker* GetMoniker() { return m_pMoniker; }

  HRESULT Create(IBaseFilter** ppBF);
};

template<class T>
class CFGFilterInternal : public CFGFilter
{
public:
  CFGFilterInternal(std::string name = "")
    : CFGFilter(__uuidof(T), INTERNAL, name) {}

  HRESULT Create(IBaseFilter** ppBF)
  {
    CheckPointer(ppBF, E_POINTER);

    HRESULT hr = S_OK;
    IBaseFilter* pBF = new T(NULL, &hr);
    if (FAILED(hr)) return hr;

    (*ppBF = pBF)->AddRef();
    pBF = NULL;

    return hr;
  }
};

class CFGFilterFile : public CFGFilter
{
protected:
  std::string m_path;
  std::string m_xFileType;
  std::string m_internalName;
  HINSTANCE m_hInst;
  bool m_isDMO;
  CLSID m_catDMO;

public:
  CFGFilterFile(const CLSID& clsid, std::string path, std::string name = "", std::string filtername = "", std::string filetype = "");
  CFGFilterFile(TiXmlElement *pFilter);

  HRESULT Create(IBaseFilter** ppBF);
  std::string GetXFileType() { return m_xFileType; };
  std::string GetInternalName() { return m_internalName; };
  std::string GetPath() { return m_path; }
};

interface IDsRenderer;
class CDSGraph;

class CFGFilterVideoRenderer : public CFGFilter
{
public:
  CFGFilterVideoRenderer(const CLSID& clsid, std::string name = "");
  ~CFGFilterVideoRenderer();

  HRESULT Create(IBaseFilter** ppBF);
};
