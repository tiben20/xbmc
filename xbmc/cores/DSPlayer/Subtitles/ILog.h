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

#define _LOGDEBUG   0
#define _LOGINFO    1
#define _LOGWARNING 2
#define _LOGERROR   3
#define _LOGFATAL   4
#define _LOGNONE    5

/**
 * ILog: Allow the subtitles dll to log message within XBMC
 */
class ILog
{
public:
  virtual void Log(int loglevel, const char *format, ...) = 0;
};

extern ILog* g_log;