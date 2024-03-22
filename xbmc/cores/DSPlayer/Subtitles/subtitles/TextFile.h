/* 
 *  Copyright (C) 2003-2006 Gabest
 *  http://www.gabest.org
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
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "atlwfile.h"
/*#include "utils\DownloadQueue.h"
#include "utils\Event.h"*/

class CTextFile : protected ATL::CStdioFile
{
public:
  typedef enum {ASCII, UTF8, LE16, BE16, ANSI} enc;

private:
  enc m_encoding, m_defaultencoding;
  int m_offset;
  std::wstring m_strFileName;

public:
  CTextFile(enc e = ASCII);

  virtual bool Open(LPCTSTR lpszFileName);
  virtual bool Save(LPCTSTR lpszFileName, enc e /*= ASCII*/);

  void SetEncoding(enc e);
  enc GetEncoding();
  bool IsUnicode();

  // CFile

  std::wstring GetFilePath() const;

  // CStdioFile

  ULONGLONG GetPosition();
  ULONGLONG GetLength();
  ULONGLONG Seek(LONGLONG lOff, UINT nFrom);

  void WriteString(LPCSTR lpsz/*std::string str*/);
  void WriteString(LPCWSTR lpsz/*std::wstring str*/);
  BOOL ReadString(std::string& str);
  BOOL ReadString(std::wstring& str);

};

class CWebTextFile : public CTextFile/*, IDownloadQueueObserver*/
{
  LONGLONG m_llMaxSize;
  std::wstring m_tempfn;
  /*CEvent m_downloadEvent;
  TICKET m_dlTicket;
  bool m_dlSucceeded;*/

public:
  CWebTextFile(LONGLONG llMaxSize = 1024*1024);

  //void OnFileComplete(TICKET aTicket, std::string& aFilePath, INT aByteRxCount, Result aResult);
  bool Open(LPCTSTR lpszFileName);
  bool Save(LPCTSTR lpszFileName, enc e /*= ASCII*/);
  void Close();
};

extern std::wstring  AToT(std::string str);
extern std::wstring  WToT(std::wstring str);
extern std::string TToA(std::wstring str);
extern std::wstring TToW(std::wstring str);
