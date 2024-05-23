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

#include "ShaderFactory.h"
#include "utils/XBMCTinyXML.h"
#include "utils/XMLUtils.h"
#include "utils/Log.h"
#include "url.h"
#include "filesystem/Directory.h"
#include "Fileitem.h"
#include "DSUtil/DSUtil.h"


CShaderFactory::CShaderFactory()
{
}

CShaderFactory::~CShaderFactory()
{

}

bool CShaderFactory::LoadShader(std::string shadername)
{
  //verify we dont already have it loaded
  if (auto search = m_pShaders.find(shadername); search != m_pShaders.end())
  {
    CLog::Log(LOGINFO, "{} This shader is already loaded {}", __FUNCTION__, shadername);
    return true;
  }
  CD3DScaler shader;
  //shader.Create()

  return false;
}

CShadersXmlLoader::CShadersXmlLoader(std::string filepath)
{
  m_pFilePath = filepath;
  LoadFolder();
}

CShadersXmlLoader::~CShadersXmlLoader()
{
}

void CShadersXmlLoader::LoadSingleShader()
{
  CShaderFileLoader* shader;
  shader = new CShaderFileLoader(L"special://xbmc/system/shaders/mpcvr/Bicubic.hlsl");
  ShaderDesc desc;
  desc.name = "Bicubic";
  phmap::flat_hash_map<std::wstring, float> parameters;
  int res = shader->Compile(desc, 0, &parameters);
  if (!res)
    m_pShaders.push_back(desc);
}

void CShadersXmlLoader::AddShader(ShaderDesc desc)
{
  m_pShaders.push_back(desc);
}

void CShadersXmlLoader::LoadFolder()
{
  CFileItemList layouts;
  std::vector<CFileItemPtr> thelist;
  
  if (XFILE::CDirectory::GetDirectory(CURL("special://xbmc/system/shaders/mpcvr"), layouts, ".hlsl",
    XFILE::DIR_FLAG_DEFAULTS))
  {
    thelist = layouts.GetList();
    for (std::vector<CFileItemPtr>::iterator it = thelist.begin(); thelist.end() != it; it++)
    {
      
      CLog::Log(LOGINFO, "Loading {}", it->get()->GetURL().GetFileName());
      CShaderFileLoader* shader;
      shader = new CShaderFileLoader(AToW("special://"+it->get()->GetURL().GetFileName()));
      
      ShaderDesc desc;
      desc.name = it->get()->GetURL().GetFileNameWithoutPath();
      phmap::flat_hash_map<std::wstring, float> parameters;
      int res = shader->Compile(desc, 0, &parameters);
      if (!res)
      { 
        m_pShaders.push_back(desc);
        if (m_pShaders.size() == 4)
          return;
      }
    }

    
  }
  
}

bool CShadersXmlLoader::LoadFile()
{
  CXBMCTinyXML configXML;
  if (!configXML.LoadFile(m_pFilePath))
  {
    CLog::Log(LOGERROR, "{} Failed to load {}", __FUNCTION__,m_pFilePath);
    return false;
  }
  TiXmlElement* pElement = configXML.RootElement();
  if (pElement && pElement->Value())

  return true;
}

