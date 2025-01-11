/*
 *  Copyright (C) 2016-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PVRGUIActionsEPG.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "dialogs/GUIDialogFileBrowser.h"
#include "dialogs/GUIDialogYesNo.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIKeyboardFactory.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "guilib/WindowIDs.h"
#include "pvr/PVRItem.h"
#include "pvr/PVRManager.h"
#include "pvr/dialogs/GUIDialogPVRChannelGuide.h"
#include "pvr/dialogs/GUIDialogPVRGuideInfo.h"
#include "pvr/epg/EpgContainer.h"
#include "pvr/epg/EpgInfoTag.h"
#include "pvr/epg/EpgSearchFilter.h"
#include "pvr/guilib/PVRGUIActionsParentalControl.h"
#include "pvr/windows/GUIWindowPVRSearch.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "storage/MediaManager.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <memory>
#include <string>

using namespace PVR;

namespace
{
PVR::CGUIWindowPVRSearchBase* GetSearchWindow(bool bRadio)
{
  const int windowSearchId = bRadio ? WINDOW_RADIO_SEARCH : WINDOW_TV_SEARCH;

  PVR::CGUIWindowPVRSearchBase* windowSearch;

  CGUIWindowManager& windowMgr = CServiceBroker::GetGUI()->GetWindowManager();
  if (bRadio)
    windowSearch = windowMgr.GetWindow<PVR::CGUIWindowPVRRadioSearch>(windowSearchId);
  else
    windowSearch = windowMgr.GetWindow<PVR::CGUIWindowPVRTVSearch>(windowSearchId);

  if (!windowSearch)
    CLog::LogF(LOGERROR, "Unable to get {}!", bRadio ? "WINDOW_RADIO_SEARCH" : "WINDOW_TV_SEARCH");

  return windowSearch;
}
} // unnamed namespace

bool CPVRGUIActionsEPG::ShowEPGInfo(const CFileItem& item) const
{
  const std::shared_ptr<const CPVRChannel> channel(CPVRItem(item).GetChannel());
  if (channel && CServiceBroker::GetPVRManager().Get<PVR::GUI::Parental>().CheckParentalLock(
                     channel) != ParentalCheckResult::SUCCESS)
    return false;

  const std::shared_ptr<CPVREpgInfoTag> epgTag(CPVRItem(item).GetEpgInfoTag());
  if (!epgTag)
  {
    CLog::LogF(LOGERROR, "No epg tag!");
    return false;
  }

  CGUIDialogPVRGuideInfo* pDlgInfo =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogPVRGuideInfo>(
          WINDOW_DIALOG_PVR_GUIDE_INFO);
  if (!pDlgInfo)
  {
    CLog::LogF(LOGERROR, "Unable to get WINDOW_DIALOG_PVR_GUIDE_INFO!");
    return false;
  }

  pDlgInfo->SetProgInfo(std::make_shared<CFileItem>(epgTag));
  pDlgInfo->Open();
  return true;
}

bool CPVRGUIActionsEPG::ShowChannelEPG(const CFileItem& item) const
{
  const std::shared_ptr<const CPVRChannel> channel(CPVRItem(item).GetChannel());
  if (channel && CServiceBroker::GetPVRManager().Get<PVR::GUI::Parental>().CheckParentalLock(
                     channel) != ParentalCheckResult::SUCCESS)
    return false;

  CGUIDialogPVRChannelGuide* pDlgInfo =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogPVRChannelGuide>(
          WINDOW_DIALOG_PVR_CHANNEL_GUIDE);
  if (!pDlgInfo)
  {
    CLog::LogF(LOGERROR, "Unable to get WINDOW_DIALOG_PVR_CHANNEL_GUIDE!");
    return false;
  }

  pDlgInfo->Open(channel);
  return true;
}

bool CPVRGUIActionsEPG::FindSimilar(const CFileItem& item) const
{
  CGUIWindowPVRSearchBase* windowSearch = GetSearchWindow(CPVRItem(item).IsRadio());
  if (!windowSearch)
    return false;

  //! @todo If we want dialogs to spawn program search in a clean way - without having to force-close any
  //        other dialogs - we must introduce a search dialog with functionality similar to the search window.

  for (int iId = CServiceBroker::GetGUI()->GetWindowManager().GetTopmostModalDialog(
           true /* ignoreClosing */);
       iId != WINDOW_INVALID;
       iId = CServiceBroker::GetGUI()->GetWindowManager().GetTopmostModalDialog(
           true /* ignoreClosing */))
  {
    CLog::LogF(LOGWARNING,
               "Have to close modal dialog with id {} before search window can be opened.", iId);

    CGUIWindow* window = CServiceBroker::GetGUI()->GetWindowManager().GetWindow(iId);
    if (window)
    {
      window->Close();
    }
    else
    {
      CLog::LogF(LOGERROR, "Unable to get window instance {}! Cannot open search window.", iId);
      return false; // return, otherwise we run into an endless loop
    }
  }

  windowSearch->SetItemToSearch(item);
  CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(windowSearch->GetID());
  return true;
};

bool CPVRGUIActionsEPG::ExecuteSavedSearch(const CFileItem& item)
{
  const auto searchFilter = item.GetEPGSearchFilter();

  if (!searchFilter)
  {
    CLog::LogF(LOGERROR, "Wrong item type. No EPG search filter present.");
    return false;
  }

  CGUIWindowPVRSearchBase* windowSearch = GetSearchWindow(searchFilter->IsRadio());
  if (!windowSearch)
    return false;

  windowSearch->SetItemToSearch(item);
  CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(windowSearch->GetID());
  return true;
}

bool CPVRGUIActionsEPG::EditSavedSearch(const CFileItem& item)
{
  const auto searchFilter = item.GetEPGSearchFilter();

  if (!searchFilter)
  {
    CLog::LogF(LOGERROR, "Wrong item type. No EPG search filter present.");
    return false;
  }

  CGUIWindowPVRSearchBase* windowSearch = GetSearchWindow(searchFilter->IsRadio());
  if (!windowSearch)
    return false;

  if (windowSearch->OpenDialogSearch(item) == CGUIDialogPVRGuideSearch::Result::SEARCH)
    CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(windowSearch->GetID());

  return true;
}

bool CPVRGUIActionsEPG::RenameSavedSearch(const CFileItem& item)
{
  const auto searchFilter = item.GetEPGSearchFilter();

  if (!searchFilter)
  {
    CLog::LogF(LOGERROR, "Wrong item type. No EPG search filter present.");
    return false;
  }

  std::string title = searchFilter->GetTitle();
  if (CGUIKeyboardFactory::ShowAndGetInput(title,
                                           CVariant{g_localizeStrings.Get(528)}, // "Enter title"
                                           false))
  {
    searchFilter->SetTitle(title);
    CServiceBroker::GetPVRManager().EpgContainer().PersistSavedSearch(*searchFilter);
    return true;
  }
  return false;
}

bool CPVRGUIActionsEPG::ChooseIconForSavedSearch(const CFileItem& item)
{
  const auto searchFilter{item.GetEPGSearchFilter()};

  if (!searchFilter)
  {
    CLog::LogF(LOGERROR, "Wrong item type. No EPG search filter present.");
    return false;
  }

  // setup our icon list
  CFileItemList items;

  // Add the current icon, if available.
  const std::string iconPath{searchFilter->GetIconPath()};
  auto current{std::make_shared<CFileItem>("icon://Current", false)};
  current->SetArt("icon", iconPath.empty() ? "DefaultPVRSearch.png" : iconPath);
  current->SetLabel(g_localizeStrings.Get(19282)); // Current icon
  items.Add(std::move(current));

  // And add a "No icon" entry as well.
  auto nothumb{std::make_shared<CFileItem>("icon://None", false)};
  nothumb->SetArt("icon", "DefaultPVRSearch.png");
  nothumb->SetLabel(g_localizeStrings.Get(19283)); // No icon
  items.Add(std::move(nothumb));

  std::string icon;
  std::vector<CMediaSource> sources;
  CServiceBroker::GetMediaManager().GetLocalDrives(sources);
  if (!CGUIDialogFileBrowser::ShowAndGetImage(items, sources,
                                              g_localizeStrings.Get(19285), // Browse for icon
                                              icon))
    return false;

  if (icon == "icon://Current")
    return true;

  if (icon == "icon://None")
    icon.clear();

  searchFilter->SetIconPath(icon);
  CServiceBroker::GetPVRManager().EpgContainer().PersistSavedSearch(*searchFilter);
  return true;
}

bool CPVRGUIActionsEPG::DuplicateSavedSearch(const CFileItem& item)
{
  const auto searchFilter{item.GetEPGSearchFilter()};

  if (!searchFilter)
  {
    CLog::LogF(LOGERROR, "Wrong item type. No EPG search filter present.");
    return false;
  }

  const auto dupedSearchFilter{std::make_shared<CPVREpgSearchFilter>(*searchFilter)};
  dupedSearchFilter->SetDatabaseId(PVR_EPG_SEARCH_INVALID_DATABASE_ID); // force new db entry
  dupedSearchFilter->SetTitle(StringUtils::Format(g_localizeStrings.Get(19356), // Copy of '<title>'
                                                  searchFilter->GetTitle()));
  CServiceBroker::GetPVRManager().EpgContainer().PersistSavedSearch(*dupedSearchFilter);
  return true;
}

bool CPVRGUIActionsEPG::DeleteSavedSearch(const CFileItem& item)
{
  const auto searchFilter = item.GetEPGSearchFilter();

  if (!searchFilter)
  {
    CLog::LogF(LOGERROR, "Wrong item type. No EPG search filter present.");
    return false;
  }

  if (CGUIDialogYesNo::ShowAndGetInput(CVariant{122}, // "Confirm delete"
                                       CVariant{19338}, // "Delete this saved search?"
                                       CVariant{""}, CVariant{item.GetLabel()}))
  {
    return CServiceBroker::GetPVRManager().EpgContainer().DeleteSavedSearch(*searchFilter);
  }
  return false;
}

std::string CPVRGUIActionsEPG::GetTitleForEpgTag(const std::shared_ptr<const CPVREpgInfoTag>& tag)
{
  if (tag)
  {
    if (CServiceBroker::GetPVRManager().IsParentalLocked(tag))
      return g_localizeStrings.Get(19266); // Parental locked
    else if (!tag->Title().empty())
      return tag->Title();
  }

  if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
          CSettings::SETTING_EPG_HIDENOINFOAVAILABLE))
    return g_localizeStrings.Get(19055); // no information available

  return {};
}
