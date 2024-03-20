#pragma once
/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

#include "PixelShaderCompiler.h"

class TiXmlElement;

class CExternalPixelShader
{
public:
  HRESULT Compile(CPixelShaderCompiler *pCompiler);
  bool Load();
  CExternalPixelShader(TiXmlElement* xml);
  CExternalPixelShader(std::string strFile, std::string strProfile);
  bool IsValid() const { return m_valid; }
  int GetId() const { return m_id; }
  std::string GetSourceData() const { return m_SourceData; }
  void DeleteSourceData() { m_SourceData = ""; }
  void DeletePixelShader() { m_pPixelShader = nullptr; }

  bool IsEnabled() const { return m_enabled; }
  void SetEnabled(const bool enabled) { m_enabled = enabled; }
  int GetStage() const { return m_shaderStage; }
  void SetStage(const int stage) { m_shaderStage = stage; }

  std::string GetName() const { return m_name; }
  void SetName(const std::string& name) { m_name = name; }

  TiXmlElement ToXML();

  Com::SmartPtr<IDirect3DPixelShader9> m_pPixelShader;
private:
  std::string m_SourceFile;
  std::string m_SourceTarget;
  std::string m_SourceData;
  std::string m_name;
  int m_shaderStage;
  int m_id;
  bool m_valid;
  bool m_enabled;
};

bool SortPixelShader(CExternalPixelShader* p1, CExternalPixelShader* p2);