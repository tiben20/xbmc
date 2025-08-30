/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
 *
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

#include "DSFilterVersion.h"
#include "Utils/DSFilterEnumerator.h"
#include "Utils/AudioEnumerator.h"
#include "filtercorefactory/filtercorefactory.h"

#pragma comment(lib , "version.lib")

#if HAS_DS_PLAYER

CDSFilterVersion *CDSFilterVersion::m_pSingleton = NULL;

CDSFilterVersion::CDSFilterVersion()
{
  m_FilterVersions.clear();
}

CDSFilterVersion::~CDSFilterVersion()
{
}

CDSFilterVersion* CDSFilterVersion::Get()
{
  return (m_pSingleton) ? m_pSingleton : (m_pSingleton = new CDSFilterVersion());
}

bool CDSFilterVersion::IsRegisteredFilter(const std::string filter)
{
  CDSFilterEnumerator p_dsfilter;
  std::vector<DSFiltersInfo> dsfilterList;
  p_dsfilter.GetDSFilters(dsfilterList);
  std::vector<DSFiltersInfo>::const_iterator iter = dsfilterList.begin();

  for (int i = 1; iter != dsfilterList.end(); i++)
  {
    DSFiltersInfo dev = *iter;
    if (dev.lpstrName == filter)
    {
      return true;
      break;
    }
    ++iter;
  }
  return false;
}

std::string CDSFilterVersion::GetStringVersion(const std::string &type)
{
  return m_FilterVersions[type].sVersion;
}

int CDSFilterVersion::GetIntVersion(const std::string &type)
{
  return m_FilterVersions[type].iVersion;
}

void CDSFilterVersion::InitVersion()
{
  // Internal Filters
  //GetVersionByFilter(CGraphFilters::INTERNAL_LAVSPLITTER, true);
  //GetVersionByFilter(CGraphFilters::INTERNAL_LAVVIDEO);
  //GetVersionByFilter(CGraphFilters::INTERNAL_LAVAUDIO);
  //GetVersionByFilter(CGraphFilters::INTERNAL_XYSUBFILTER);
  //GetVersionByFilter(CGraphFilters::INTERNAL_XYVSFILTER);

  // madvR
  FilterVersion filterVersion;
  GetVersionByPath(GetMadvrFilePath(), filterVersion);
  m_FilterVersions[CGraphFilters::MADSHI_VIDEO_RENDERER] = filterVersion;

  // saneAR (hardcoded)
  filterVersion.sVersion = "v0.3.0.0";
  filterVersion.iVersion = (0 << 24 | 3 << 16 | 0 << 8 | 0);
  m_FilterVersions[CGraphFilters::INTERNAL_SANEAR] = filterVersion;
}

void CDSFilterVersion::GetVersionByFilter(const std::string &type, bool bForceUpdate)
{
  if (type.empty())
    return;

  if (bForceUpdate)
  {  
    CFGLoader *pLoader = new CFGLoader();
    pLoader->LoadConfig(INTERNALFILTERS);
  }

  CFGFilter *pFilter = NULL;
  if (pFilter = CFilterCoreFactory::GetFilterFromName(type))
  {
    CStdString sPath = static_cast<CFGFilterFile *>(pFilter)->GetPath();
    FilterVersion filterVersion;
    GetVersionByPath(AToW(sPath), filterVersion);
    m_FilterVersions[type] = filterVersion;
  }
}

void CDSFilterVersion::GetVersionByPath(const CStdStringW &path, FilterVersion &filterVersion)
{
  if (path.empty())
    return;

  std::wstring szVersionFile = path;
  DWORD  verHandle = NULL;
  UINT   size = 0;
  LPBYTE lpBuffer = NULL;
  DWORD  verSize = GetFileVersionInfoSize(szVersionFile.c_str(), &verHandle);

  if (verSize != NULL)
  {
    LPSTR verData = new char[verSize];

    if (GetFileVersionInfo(szVersionFile.c_str(), verHandle, verSize, verData))
    {
      if (VerQueryValue(verData, L"\\", (VOID FAR* FAR*)&lpBuffer, &size))
      {
        if (size)
        {
          VS_FIXEDFILEINFO *verInfo = (VS_FIXEDFILEINFO *)lpBuffer;
          if (verInfo->dwSignature == 0xfeef04bd)
          {
            filterVersion.sVersion = StringUtils::Format("v{}.{}.{}.{}",
              (verInfo->dwFileVersionMS >> 16) & 0xffff,
              (verInfo->dwFileVersionMS >> 0) & 0xffff,
              (verInfo->dwFileVersionLS >> 16) & 0xffff,
              (verInfo->dwFileVersionLS >> 0) & 0xffff
            );
            int iMajor = (verInfo->dwFileVersionMS >> 16) & 0xffff;
            int iMinor = (verInfo->dwFileVersionMS >> 0) & 0xffff;
            int iBuild = (verInfo->dwFileVersionLS >> 16) & 0xffff;
            int iRevision = (verInfo->dwFileVersionLS >> 0) & 0xffff;
            filterVersion.iVersion = (iMajor << 24 | iMinor << 16 | iBuild << 8 | iRevision);
          }
        }
      }
    }
    delete[] verData;
  }
}

std::wstring CDSFilterVersion::GetMadvrFilePath()
{
  HKEY hKey;
  std::wstring sPath = L"";
  if (RegOpenKeyExW(HKEY_CLASSES_ROOT, L"CLSID\\{E1A8B82A-32CE-4B0D-BE0D-AA68C772E423}\\InprocServer32", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
  {
    wchar_t buf[MAX_PATH];
    DWORD bufSize = sizeof(buf);
    DWORD valType;
    if (RegQueryValueExW(hKey, NULL, 0, &valType, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS && valType == REG_SZ)
    {
      //g_charsetConverter.wToUTF8(std::wstring(buf, bufSize / sizeof(wchar_t)), sPath);
      sPath = std::wstring(buf, bufSize / sizeof(wchar_t));
      //g_charsetConverter.wToUTF8(std::wstring(buf, bufSize / sizeof(wchar_t)), sPath);
    }
    RegCloseKey(hKey);
  }

  return sPath;
}

#endif