#pragma once
/*
*      Copyright (C) 2005-2008 Team XBMC
*      http://www.xbmc.org
*
*      Copyright (C) 2015 Romank
*      https://github.com/Romank1
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

#if TODO
#ifndef HAS_DS_PLAYER
#error DSPlayer's header file included without HAS_DS_PLAYER defined
#endif

#include "DSPVRBackend.h"

class CDSArgusTV : public CDSPVRBackend
{
public:
  CDSArgusTV(const std::string& strBackendBaseAddress, const std::string& strBackendName);
  virtual ~CDSArgusTV();
  virtual bool        ConvertStreamURLToTimeShiftFilePath(const std::string& strUrl, std::string& strTimeShiftFile);
  virtual bool        SupportsStreamConversion(const std::string& strUrl) const { return StringUtils::EqualsNoCase(StringUtils::Left(strUrl, 7), "rtsp://"); };
  virtual bool        SupportsFastChannelSwitch() const { return true; };

private:          
  bool                SendCommandToArgusTV_GET(const std::string& strCommand, CVariant &json_response);
  bool                SendCommandToArgusTV_POST(const std::string& strCommand, const std::string& strArguments, CVariant &json_response);
};
#endif