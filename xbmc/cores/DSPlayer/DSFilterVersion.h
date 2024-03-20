#pragma once
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

#include "GraphFilters.h"

#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

typedef struct FilterVersion
{
  std::string sVersion;
  unsigned int iVersion;
}  FilterVersion;

class CDSFilterVersion
{
public:

  /// Retrieve singleton instance
  static CDSFilterVersion* Get();
  /// Destroy singleton instance
  static void Destroy()
  {
    delete m_pSingleton;
    m_pSingleton = nullptr;
  }

  bool IsRegisteredFilter(const std::string filter);
  void InitVersion();
  std::string GetStringVersion(const std::string &type);
  int GetIntVersion(const std::string &type);

private:
  CDSFilterVersion();
  ~CDSFilterVersion();

  static CDSFilterVersion* m_pSingleton;
  void GetVersionByPath(const std::string &path, FilterVersion &filterVersion );
  void GetVersionByFilter(const std::string &type, bool bForceUpdate = false);
  std::string GetMadvrFilePath();
  std::map<std::string, FilterVersion> m_FilterVersions;
};
