#pragma once

/*
 *      Copyright (C) 2024 Team XBMC
 *      http://xbmc.org
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

#include "settings/dialogs/GUIDialogSettingsManualBase.h"

class CGUIDialogLibplacebo : public CGUIDialogSettingsManualBase
{
public:
  CGUIDialogLibplacebo();
  virtual ~CGUIDialogLibplacebo();
  static void ShowLibplaceboOptions();
protected:

  // implementations of ISettingCallback
  virtual void OnSettingChanged(const std::shared_ptr<const CSetting>& setting);
  virtual void OnSettingAction(const std::shared_ptr<const CSetting>& setting);

  // specialization of CGUIDialogSettingsBase
  virtual bool AllowResettingSettings() const { return false; }
  virtual bool Save() { return true; };

  // specialization of CGUIDialogSettingsManualBase
  virtual void InitializeSettings();
  virtual void SetupView();
};
