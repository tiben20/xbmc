#pragma once

/*
 *      Copyright (C) 2005-2014 Team XBMC
 *      http://xbmc.org
 *
 *      Copyright (C) 2014-2015 Aracnoz
 *      http://github.com/aracnoz/xbmc
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "settings/dialogs/GUIDialogSettingsManualBase.h"
#include "utils/XMLUtils.h"


enum ConfigType {
  EDITATTR,
  EDITATTREXTRA,
  EDITATTRSHADER,
  SPINNERATTR,
  SPINNERATTRSHADER,
  BOOLATTR,
  FILTER,
  EXTRAFILTER,
  SHADER,
  OSDGUID,
  FILTERSYSTEM
};

enum xmlType {
  MEDIASCONFIG,
  FILTERSCONFIG,
  HOMEFILTERSCONFIG,
  SHADERS,
  PLAYERCOREFACTORY
};

typedef std::pair<std::string, std::string> DynamicStringSettingOption;
typedef std::vector<DynamicStringSettingOption> DynamicStringSettingOptions;

class DSConfigList
{
public:
  DSConfigList(ConfigType type);

  std::string m_attr;
  std::string m_nodeName;
  std::string m_nodeList;
  std::string m_value;
  std::string m_setting;
  int m_subNode;
  int m_label;
  ConfigType m_configType;
  StringSettingOptionsFiller m_filler;
  bool GetBoolValue() { return m_value == "true" ? true : false; }
  void SetBoolValue(bool value) { value ? m_value = "true" : m_value = "false"; }
};

class CGUIDialogDSManager
{
public:
  CGUIDialogDSManager();
  virtual ~CGUIDialogDSManager();

  static CGUIDialogDSManager* Get();
  static void Destroy()
  {
    delete m_pSingleton;
    m_pSingleton = NULL;
  }

  bool GetNew() { return m_bNew; };  
  void SetConfig(bool bNew, int iIndex)  { m_bNew = bNew;  m_iIndex = iIndex; };

  void InitConfig(std::vector<DSConfigList *> &configList, ConfigType type, const std::string &strSetting, int label, const std::string &strAttr = "", const std::string &strNodeName = "", StringSettingOptionsFiller filler = NULL, int subNode = 0, const std::string &strNodeList = "");
  void ResetValue(std::vector<DSConfigList *>& configList);
  void LoadDsXML(xmlType type, TiXmlElement* &pNode, bool forceCreate = false);
  void SaveDsXML(xmlType type);
  void GetPath(xmlType type, std::string &xmlFile, std::string &xmlNode, std::string &xmlRoot);
  static void AllFiltersConfigOptionFiller(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data);
  static void ShadersOptionFiller(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data);
  static void ShadersScaleOptionFiller(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data);
  static void DSFilterOptionFiller(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data);
  static void BoolOptionFiller(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data);
  static void PriorityOptionFiller(const CSetting *setting, std::vector< std::pair<std::string, std::string> > &list, std::string &current, void *data);
  static bool compare_by_word(const DynamicStringSettingOption& lhs, const DynamicStringSettingOption& rhs);
  void GetFilterList(xmlType type, std::vector<DynamicStringSettingOption> &list);
  TiXmlElement* KeepSelectedNode(TiXmlElement* pNode, const std::string &subNodeName);
  bool FindPrepend(TiXmlElement* &pNode, const std::string &xmlNode);

protected:

  static CGUIDialogDSManager* m_pSingleton;

  bool m_bNew;
  int m_iIndex;
  CXBMCTinyXML m_XML;
};
