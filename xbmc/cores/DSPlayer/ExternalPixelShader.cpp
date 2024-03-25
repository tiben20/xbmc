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

#if HAS_DS_PLAYER

#include "ExternalPixelShader.h"
#include "PixelShaderCompiler.h"
#include "FileSystem\File.h"
#include "utils/XMLUtils.h"
#include "ServiceBroker.h"
#include "settings/SettingsComponent.h"
#include "profiles/ProfileManager.h"
#include "utils/log.h"
#include "Utils/StringUtils.h"
#include "Utils/DSFileUtils.h"

HRESULT CExternalPixelShader::Compile(CPixelShaderCompiler *pCompiler)
{
  if (!pCompiler)
    return E_FAIL;

  if (m_SourceData.empty())
  {
    if (!Load())
      return E_FAIL;
  }
  std::string errorMsg;
  HRESULT hr = pCompiler->CompileShader(m_SourceData.c_str(), "main", m_SourceTarget.c_str(), 0, &m_pPixelShader, NULL, &errorMsg);
  if (FAILED(hr))
  {
    CLog::Log(LOGERROR, "%s Shader's compilation failed : %s", __FUNCTION__, errorMsg.c_str());
    return hr;
  }

  // Delete buffer
  m_SourceData = "";

  CLog::Log(LOGINFO, "Pixel shader \"%s\" compiled", m_name.c_str());
  return S_OK;
}

CExternalPixelShader::CExternalPixelShader(TiXmlElement* xml)
  : m_id(-1), m_valid(false), m_enabled(false)
{
  m_name = CDSXMLUtils::GetString(xml, "name");
  CDSXMLUtils::GetInt(xml, "id", &m_id);

  if (!XMLUtils::GetString(xml, "path", m_SourceFile))
    return;

  if (!XMLUtils::GetString(xml, "profile", m_SourceTarget))
    return;

  if (!XFILE::CFile::Exists(m_SourceFile))
  {
    std::string originalFile = m_SourceFile;
    m_SourceFile = CServiceBroker::GetSettingsComponent()->GetProfileManager()->GetUserDataItem("dsplayer/shaders/" + originalFile);
    if (!XFILE::CFile::Exists(m_SourceFile))
    {
      m_SourceFile = "special://xbmc/system/players/dsplayer/shaders/" + originalFile;
      if (!XFILE::CFile::Exists(m_SourceFile))
      {
        m_SourceFile = "";
        return;
      }
    }
  }

  if (!StringUtils::EqualsNoCase(m_SourceTarget,"ps_1_1") 
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_1_2") 
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_1_3")
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_1_4") 
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_2_0") 
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_2_a")
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_2_b") 
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_3_0"))
    return;

  m_valid = true;
}

CExternalPixelShader::CExternalPixelShader(std::string strFile, std::string strProfile)
  : m_id(-1), m_valid(false), m_enabled(false), m_SourceFile(strFile),
  m_SourceTarget(strProfile)
{
  if (!XFILE::CFile::Exists(m_SourceFile))
  {
    std::string originalFile = m_SourceFile;
    m_SourceFile = CServiceBroker::GetSettingsComponent()->GetProfileManager()->GetUserDataItem("dsplayer/shaders/" + originalFile);
    if (!XFILE::CFile::Exists(m_SourceFile))
    {
      m_SourceFile = "special://xbmc/system/players/dsplayer/shaders/" + originalFile;
      if (!XFILE::CFile::Exists(m_SourceFile))
      {
        m_SourceFile = "";
        return;
      }
    }
  }

  if (!StringUtils::EqualsNoCase(m_SourceTarget, "ps_1_1")
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_1_2")
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_1_3")
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_1_4")
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_2_0")
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_2_a")
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_2_b")
    && !StringUtils::EqualsNoCase(m_SourceTarget, "ps_3_0"))
    return;

  m_valid = true;
}

bool CExternalPixelShader::Load()
{
  XFILE::CFileStream file;
  if (!file.Open(m_SourceFile))
    return false;

  std::string str;
  getline(file, str, '\0');

  if (str.empty())
  {
    m_SourceData = "";
    return false;
  }

  m_SourceData = str;
  return true;
}

TiXmlElement CExternalPixelShader::ToXML()
{
  TiXmlElement shader("shader");

  shader.SetAttribute("name", GetName().c_str());
  shader.SetAttribute("id", GetId());

  TiXmlText text("");

  {
    TiXmlElement path("path");
    text.SetValue(m_SourceFile);
    path.InsertEndChild(text);
    shader.InsertEndChild(path);
  }

  {
    TiXmlElement profile("profile");
    text.SetValue(m_SourceTarget);
    profile.InsertEndChild(text);
    shader.InsertEndChild(profile);
  }

  return shader;
}

#endif