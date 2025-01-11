/*
 *  Copyright (C) 2023 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoSelectActionProcessor.h"

#include "ContextMenuManager.h"
#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "dialogs/GUIDialogSelect.h"
#include "filesystem/Directory.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/guilib/GUIBuiltinsUtils.h"
#include "utils/guilib/GUIContentUtils.h"
#include "video/VideoFileItemClassify.h"
#include "video/guilib/VideoGUIUtils.h"

namespace KODI::VIDEO::GUILIB
{

Action CVideoSelectActionProcessor::GetDefaultSelectAction()
{
  return static_cast<Action>(CServiceBroker::GetSettingsComponent()->GetSettings()->GetInt(
      CSettings::SETTING_MYVIDEOS_SELECTACTION));
}

Action CVideoSelectActionProcessor::GetDefaultAction()
{
  return GetDefaultSelectAction();
}

bool CVideoSelectActionProcessor::Process(Action action)
{
  if (CVideoPlayActionProcessor::Process(action))
    return true;

  switch (action)
  {
    case ACTION_CHOOSE:
      return OnChooseSelected();

    case ACTION_PLAYPART:
    {
      const unsigned int part = ChooseStackItemPartNumber();
      if (part < 1) // part numbers are 1-based
        return false;

      return OnPlayPartSelected(part);
    }

    case ACTION_QUEUE:
      return OnQueueSelected();

    case ACTION_INFO:
    {
      if (GetDefaultAction() == ACTION_INFO && !KODI::VIDEO::IsVideoDb(*m_item) &&
          !m_item->IsPlugin() && !m_item->IsScript() && !m_item->IsPVR() &&
          !KODI::VIDEO::UTILS::HasItemVideoDbInformation(*m_item))
      {
        // for items without info fall back to default play action
        return Process(CVideoPlayActionProcessor::GetDefaultAction());
      }

      return OnInfoSelected();
    }

    default:
      break;
  }
  return false; // We did not handle the action.
}

bool CVideoSelectActionProcessor::OnPlayPartSelected(unsigned int part)
{
  //! @todo implement different (not using builtins function)
  KODI::UTILS::GUILIB::CGUIBuiltinsUtils::ExecutePlayMediaPart(m_item, part);
  return true;
}

bool CVideoSelectActionProcessor::OnQueueSelected()
{
  VIDEO::UTILS::QueueItem(m_item, VIDEO::UTILS::QueuePosition::POSITION_END);
  return true;
}

bool CVideoSelectActionProcessor::OnInfoSelected()
{
  return KODI::UTILS::GUILIB::CGUIContentUtils::ShowInfoForItem(*m_item);
}

bool CVideoSelectActionProcessor::OnChooseSelected()
{
  CONTEXTMENU::ShowFor(m_item, CContextMenuManager::MAIN);
  return true;
}

unsigned int CVideoSelectActionProcessor::ChooseStackItemPartNumber() const
{
  CFileItemList parts;
  XFILE::CDirectory::GetDirectory(m_item->GetDynPath(), parts, "", XFILE::DIR_FLAG_DEFAULTS);

  for (int i = 0; i < parts.Size(); ++i)
    parts[i]->SetLabel(StringUtils::Format(g_localizeStrings.Get(23051), i + 1)); // Part #

  CGUIDialogSelect* dialog =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(
          WINDOW_DIALOG_SELECT);

  dialog->Reset();
  dialog->SetHeading(CVariant{20324}); // Play part...
  dialog->SetItems(parts);
  dialog->Open();

  if (!dialog->IsConfirmed())
    return 0; // User cancelled the dialog.

  return dialog->GetSelectedItem() + 1; // part numbers are 1-based
}

} // namespace KODI::VIDEO::GUILIB
