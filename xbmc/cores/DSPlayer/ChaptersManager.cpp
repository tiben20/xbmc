/*
 *      Copyright (C) 2005-2010 Team XBMC
 *      http://www.xbmc.org
 *
 *		Copyright (C) 2010-2013 Eduard Kytmanov
 *		http://www.avmedia.su
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

#include "ChaptersManager.h"
#include "threads/SingleLock.h"

CChaptersManager *CChaptersManager::m_pSingleton = NULL;

CChaptersManager::CChaptersManager(void)
  : m_currentChapter(-1),
  m_bCheckForChapters(true)
{
}

CChaptersManager::~CChaptersManager(void)
{
  FlushChapters();
}

void CChaptersManager::FlushChapters()
{
  CSingleExit lock(m_lock);
  for (std::map<long, SChapterInfos *>::iterator it = m_chapters.begin(); it != m_chapters.end(); ++it)
    delete it->second;
  m_chapters.clear();
  m_currentChapter = -1;
  CLog::Log(LOGDEBUG, "%s Ressources released", __FUNCTION__);
}
CChaptersManager *CChaptersManager::Get()
{
  return (m_pSingleton) ? m_pSingleton : (m_pSingleton = new CChaptersManager());
}

void CChaptersManager::Destroy()
{
  delete m_pSingleton;
  m_pSingleton = NULL;
}

int CChaptersManager::GetChapterCount()
{
  CSingleExit lock(m_lock);
  return m_chapters.size();
}

int CChaptersManager::GetChapter()
{
  CSingleExit lock(m_lock);

  if (m_chapters.empty())
    return -1;
  else
    return m_currentChapter;
}

int CChaptersManager::GetChapterPos(int iChapter)
{
  CSingleExit lock(m_lock);

  if (GetChapterCount() > 0)
    return m_chapters[iChapter]->starttime / 1000;
  else
    return 0;
}

void CChaptersManager::GetChapterName(std::string& strChapterName, int chapterIdx)
{
  CSingleExit lock(m_lock);
  if (m_currentChapter == -1)
    return;

  if (chapterIdx == -1)
    chapterIdx = m_currentChapter;

  strChapterName = m_chapters[chapterIdx]->name;
}

void CChaptersManager::UpdateChapters(int64_t current_time)
{
  if (m_chapters.empty() && m_bCheckForChapters)
    LoadChapters();

  if (!m_chapters.empty())
  {
    int64_t current_converted_time = (int64_t)current_time / 10000;
    for (std::map<long, SChapterInfos *>::iterator it = m_chapters.begin(); it != m_chapters.end(); it++)
    {
      if (it->second->starttime > current_converted_time)
      {
        m_currentChapter = it->first > 1 ? it->first - 1 : 1;
        return;
      }
    }
    m_currentChapter = GetChapterCount();
  }
}

bool CChaptersManager::LoadInterface()
{
  if (!CGraphFilters::Get()->Splitter.pBF)
    return false;
  std::string splitterName = CGraphFilters::Get()->Splitter.osdname;

  //CLog::Log(LOGDEBUG, "%s Looking for chapters in \"%s\"", __FUNCTION__, splitterName.c_str());

  m_pIAMExtendedSeeking = CGraphFilters::Get()->Splitter.pBF;
  if (m_pIAMExtendedSeeking)
    return true;
  return false;
}

bool CChaptersManager::LoadChapters()
{
  CSingleExit lock(m_lock);
  if (!m_chapters.empty())
    FlushChapters();
  
  if (!m_pIAMExtendedSeeking)
    LoadInterface();

  if (m_pIAMExtendedSeeking)
  {
    long chaptersCount = -1;
    if (FAILED(m_pIAMExtendedSeeking->get_MarkerCount(&chaptersCount)) || chaptersCount <= 0)
    {
      m_bCheckForChapters = false;
      return false;
    }

    SChapterInfos *infos = nullptr;
    for (int i = 1; i <= chaptersCount; i++)
    {
      double starttime = 0;
      if (FAILED(m_pIAMExtendedSeeking->GetMarkerTime(i, &starttime)))
        continue;

      BSTR chapterName = nullptr;
      if (FAILED(m_pIAMExtendedSeeking->GetMarkerName(i, &chapterName)))
        continue;

      infos = new SChapterInfos();
      infos->starttime = (uint64_t)(starttime * 1000.0); // To ms; 
      infos->endtime = 0;
      infos->name = "";
      if (chapterName != nullptr)
        g_charsetConverter.wToUTF8(chapterName, infos->name);

      SysFreeString(chapterName);

      CLog::Log(LOGINFO, "%s Chapter \"%s\" found. Start time: %" PRId64, __FUNCTION__, infos->name.c_str(), infos->starttime);
      m_chapters.insert(std::pair<long, SChapterInfos *>(i, infos));
    }

    m_currentChapter = 1;

    return true;
  }
  else
  {
    std::string splitterName = CGraphFilters::Get()->Splitter.osdname;
    CLog::Log(LOGERROR, "%s The splitter \"%s\" doesn't support chapters", __FUNCTION__, splitterName.c_str());
    return false;
  }
}

int CChaptersManager::SeekChapter(int iChapter)
{

  if (GetChapterCount() > 0)
  {
    if (iChapter < 1)
      iChapter = 1;
    if (iChapter > GetChapterCount())
      return -1;

    // Seek to the chapter.
    CLog::Log(LOGDEBUG, "%s Seeking to chapter %d at %llu", __FUNCTION__, iChapter, m_chapters[iChapter]->starttime);
    g_dsGraph->SeekInMilliSec(m_chapters[iChapter]->starttime);
  }
  else
  {
    // Do a regular big jump.
    if (iChapter > GetChapter())
      g_dsGraph->Seek(true, true);
    else
      g_dsGraph->Seek(false, true);
  }
  return iChapter;
}

#endif