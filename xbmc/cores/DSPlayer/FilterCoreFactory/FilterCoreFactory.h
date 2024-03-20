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

#include "FilterSelectionRule.h"
#include <tinyxml.h>
#include "globalfilterselectionrule.h"
#include "dialogs/GUIDialogOK.h"
#include "guilib/GUIWindowManager.h"
#include "utils/StringUtils.h"

typedef CFGFilter* (*InternalFilterConstructorPtr) (std::string name);
template< class T > CFGFilter* InternalFilterConstructor(std::string name = "") {
  return new CFGFilterInternal<T>(name);
};

struct InternalFilters
{
  std::string name;
  std::string osdname;
  InternalFilterConstructorPtr cst;
};

class CFilterCoreFactory
{
public:
  static std::vector<CFGFilterFile *> m_Filters;

  static HRESULT LoadFiltersConfiguration(TiXmlElement* pConfig);
  static HRESULT LoadMediasConfiguration(TiXmlElement* pConfig, int iPriority);

  static HRESULT GetSourceFilter(const CFileItem& pFileItem, std::string& filter);
  static HRESULT GetSplitterFilter(const CFileItem& pFileItem, std::string& filter);
  static HRESULT GetAudioRendererFilter(const CFileItem& pFileItem, std::string& filter);
  static HRESULT GetVideoFilter(const CFileItem& pFileItem, std::string& filter, bool dxva = false);
  static HRESULT GetAudioFilter(const CFileItem& pFileItem, std::string& filter, bool dxva = false);
  static HRESULT GetSubsFilter(const CFileItem& pFileItem, std::string& filter, bool dxva = false);
  static HRESULT GetExtraFilters(const CFileItem& pFileItem, std::vector<std::string>& filters, bool dxva = false);
  static HRESULT GetShaders(const CFileItem& pFileItem, std::vector<uint32_t>& shaders, std::vector<uint32_t>& shadersStage, bool dxva = false);

  static CFGFilter* GetFilterFromName(const std::string& filter, bool showError = true);

  static bool SomethingMatch(const CFileItem& pFileItem)
  {
    return (GetGlobalFilterSelectionRule(pFileItem) != NULL);
  }

  static void Destroy()
  {
    while (!m_Filters.empty())
    {
      if (m_Filters.back())
        delete m_Filters.back();
      m_Filters.pop_back();
    }

    while (!m_selecRules.empty())
    {
      if (m_selecRules.back())
        delete m_selecRules.back();
      m_selecRules.pop_back();
    }

    // CLog::Log(LOGDEBUG, "%s Ressources released", __FUNCTION__);
  }

private:
  static bool CompareCFGFilterFileToString(CFGFilterFile * f, std::string s)
  {
    return StringUtils::EqualsNoCase(f->GetInternalName(),s);
  }
  static CGlobalFilterSelectionRule* GetGlobalFilterSelectionRule(const CFileItem& pFileItem, bool checkUrl = false);

  static std::vector<CGlobalFilterSelectionRule *> m_selecRules;

  static bool compare_by_word(CGlobalFilterSelectionRule* lhs, CGlobalFilterSelectionRule* rhs)
  {
    std::string strLine1 = lhs->GetPriority();
    std::string strLine2 = rhs->GetPriority();
    StringUtils::ToLower(strLine1);
    StringUtils::ToLower(strLine2);
    return strcmp(strLine1.c_str(), strLine2.c_str()) < 0;
  }
};
