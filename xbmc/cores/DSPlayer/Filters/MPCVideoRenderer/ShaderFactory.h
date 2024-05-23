/*
 *      Copyright (C) 2024 Team XBMC
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
#pragma once


#include "ShadersLoader.h"
#include <memory>
#include <string>
#include "Scaler.h"
class CSetting;
class CSettingCategory;
class CSettingGroup;
class CSettingSection;
class TiXmlNode;
class CD3DScaler;

class CShaderFactory
{
public:
  CShaderFactory();
  ~CShaderFactory();
  bool LoadShader(std::string shadername);
private:

  std::map < std::string, CD3DScaler> m_pShaders;
};

class CShadersXmlLoader
{
public:
  CShadersXmlLoader(std::string filepath);
  ~CShadersXmlLoader();

  void AddShader(ShaderDesc desc);
  void LoadFolder();
  bool LoadFile();
  void LoadSingleShader();
  ShaderDesc GetShaderDesc() { return m_pShaders.at(0); }
private:
  std::string m_pFilePath;
  std::vector<ShaderDesc> m_pShaders;
  ShaderDesc m_pDesc;

};

