/*
 *      Copyright (C) 2005-2008 Team XBMC
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

#include "AudioEnumerator.h"
#include "threads/SingleLock.h"
#include "utils/log.h"
#include "utils/CharsetConverter.h"
#include "streams.h"

#include "DSUtil/DSUtil.h"
#include "DSUtil/SmartPtr.h"

#include "utils/StringUtils.h"

CAudioEnumerator::CAudioEnumerator(void)
{
}

HRESULT CAudioEnumerator::GetAudioRenderers(std::vector<DSFilterInfo>& pRenderers)
{
  CSingleExit lock(m_critSection);
  pRenderers.clear();

  Com::SmartPtr<IPropertyBag> propBag = NULL;
  BeginEnumSysDev(CLSID_AudioRendererCategory, pMoniker)
  {
    std::string displayName;
    LPOLESTR str = NULL;
    if (FAILED(pMoniker->GetDisplayName(0, 0, &str))) 
      return E_FAIL;

    g_charsetConverter.wToUTF8(str, displayName);

    if (SUCCEEDED(pMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void**)&propBag)))
    {
      _variant_t var;

      std::wstring filterName;
      std::wstring filterGuid;

      if (SUCCEEDED(propBag->Read(L"FriendlyName", &var, 0)))
        filterName = std::wstring(var.bstrVal);

      var.Clear();

      if (SUCCEEDED(propBag->Read(L"CLSID", &var, 0)))
        filterGuid = std::wstring(var.bstrVal);

      AddFilter(pRenderers, filterGuid, filterName, displayName);
      propBag = NULL;
    }
    else
      return E_FAIL;
  }
  EndEnumSysDev;

  return S_OK;
}

bool CAudioEnumerator::IsDevice(std::string strDevice)
{
  CSingleExit lock(m_critSection);
  if (strDevice.empty())
    return false;

  Com::SmartPtr<IPropertyBag> propBag = NULL;
  BeginEnumSysDev(CLSID_AudioRendererCategory, pMoniker)
  {
    if (SUCCEEDED(pMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void**)&propBag)))
    {
      _variant_t var;

      std::string filterName;

      if (SUCCEEDED(propBag->Read(L"FriendlyName", &var, 0)))
        g_charsetConverter.wToUTF8(std::wstring(var.bstrVal), filterName);

      StringUtils::ToLower(filterName);
      StringUtils::ToLower(strDevice);

      std::size_t found = StringUtils::FindWords(filterName.c_str(), strDevice.c_str());
      if (found != std::string::npos)
        return true;

      propBag = NULL;
    }
    else
      return false;
  }
  EndEnumSysDev;

  return false;
}


void CAudioEnumerator::AddFilter(std::vector<DSFilterInfo>& pRenderers, std::wstring lpGuid, std::wstring lpName, std::string lpDisplayName)
{
  DSFilterInfo filterInfo;

  g_charsetConverter.wToUTF8(lpGuid, filterInfo.lpstrGuid);
  g_charsetConverter.wToUTF8(lpName, filterInfo.lpstrName);
  filterInfo.lpstrDisplayName = lpDisplayName;

  pRenderers.push_back(filterInfo);

  CLog::Log(LOGDEBUG, "Found audio renderer device \"{}\" (guid: {})", filterInfo.lpstrName.c_str(), filterInfo.lpstrGuid.c_str());
}

#endif