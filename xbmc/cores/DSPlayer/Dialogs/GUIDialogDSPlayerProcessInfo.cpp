/*
 *      Copyright (C) 2005-2016 Team Kodi
 *      http://kodi.tv
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

#include "GUIDialogDSPlayerProcessInfo.h"
#include "GraphFilters.h"

#include "input/keyboard/Key.h"
#include "input/actions/Action.h"
#include "input/actions/ActionIDs.h"
#include "guilib/GUIMessage.h"

CGUIDialogDSPlayerProcessInfo ::CGUIDialogDSPlayerProcessInfo (void)
    : CGUIDialog(WINDOW_DIALOG_DSPLAYER_PROCESS_INFO, "DialogPlayerProcessInfo.xml")
{
  m_loadType = KEEP_IN_MEMORY;
}

CGUIDialogDSPlayerProcessInfo ::~CGUIDialogDSPlayerProcessInfo (void)
{
}

bool CGUIDialogDSPlayerProcessInfo ::OnAction(const CAction &action)
{
  if (action.GetID() == ACTION_PLAYER_PROCESS_INFO)
  {
    Close();
    return true;
  }
  return CGUIDialog::OnAction(action);
}

bool CGUIDialogDSPlayerProcessInfo::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
  case GUI_MSG_WINDOW_INIT:
  {
    CGraphFilters::Get()->SetDialogProcessInfo(true);
    break;
  }
  case GUI_MSG_WINDOW_DEINIT:
  {
    CGraphFilters::Get()->SetDialogProcessInfo(false);
    break;
  }
  }

  return CGUIDialog::OnMessage(message);
}
