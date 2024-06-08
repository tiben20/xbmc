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

#include "GUIDialogLibplacebo.h"
#include "Application/Application.h"
#include "URL.h"
#include "dialogs/GUIDialogYesNo.h"
#include "filesystem/File.h"
#include "guilib/LocalizeStrings.h"
#include "profiles/ProfileManager.h"
#include "settings/Settings.h"
#include "settings/lib/Setting.h"
#include "settings/lib/SettingsManager.h"
#include "utils/LangCodeExpander.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "guilib/GUIWindowManager.h"
#include "dialogs/GUIDialogSelect.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "input/keyboard/Key.h"
#include "utils/XMLUtils.h"
#include "Filters/RendererSettings.h"
#include "PixelShaderList.h"
#include "cores/playercorefactory/PlayerCoreFactory.h"
#include "Filters/LAVAudioSettings.h"
#include "Filters/LAVVideoSettings.h"
#include "Filters/LAVSplitterSettings.h"
#include "utils/CharsetConverter.h"
#include "settings/MediaSettings.h"
#include "settings/Settings.h"
#include "settings/lib/Setting.h"
#include "settings/lib/SettingsManager.h"
#include "addons/Skin.h"
#include "settings/SettingUtils.h"
#include "guilib/GUIComponent.h"
#include "Filters/MPCVideoRenderer/Include/libplacebo/options.h"
#include "VideoRenderers/MPCVRRenderer.h"

#define SETTING_LIBPLACEBO_DEBAND_ITERATIONS       "video.libplacebo.beband.iterations"
#define SETTING_LIBPLACEBO_DEBAND_THRESHOLD        "video.libplacebo.beband.threshold"
#define SETTING_LIBPLACEBO_DEBAND_RADIUS        "video.libplacebo.beband.radius"
#define SETTING_LIBPLACEBO_DEBAND_GRAIN        "video.libplacebo.beband.grain"
#define SETTING_LIBPLACEBO_SIGMOID_CENTER        "video.libplacebo.sigmoid.center"
#define SETTING_LIBPLACEBO_SIGMOID_SLOPE        "video.libplacebo.sigmoid.slope"

#define CreateGroup(thegroup,thecategory) std::shared_ptr<CSettingGroup> thegroup = AddGroup(thecategory); if (thegroup == NULL) {CLog::Log(LOGERROR, "CGUIDialogLibplacebo: unable to setup settings");  return; }

using namespace std;

CGUIDialogLibplacebo::CGUIDialogLibplacebo()
  : CGUIDialogSettingsManualBase(WINDOW_DIALOG_LIBPLACEBO_OPTIONS, "DialogSettings.xml")
{
}

CGUIDialogLibplacebo::~CGUIDialogLibplacebo()
{ 
}

void CGUIDialogLibplacebo::ShowLibplaceboOptions()
{
  
}

void CGUIDialogLibplacebo::SetupView()
{
  CGUIDialogSettingsManualBase::SetupView();

  SetHeading(55200);

  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_OKAY_BUTTON);
  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_CUSTOM_BUTTON);
  SET_CONTROL_LABEL(CONTROL_SETTINGS_CANCEL_BUTTON, 15067);
}

void CGUIDialogLibplacebo::InitializeSettings()
{
  CGUIDialogSettingsManualBase::InitializeSettings();

  bool usePopup = g_SkinInfo->HasSkinFile("DialogSlider.xml");

  std::shared_ptr<CSettingCategory> category = AddCategory("libplacebosettings", 55201);
  if (category == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogLibplacebo: unable to setup settings");
    return;
  }
  // get all necessary setting groups
  CreateGroup(groupDeband, category);
  CreateGroup(groupSigmoid, category);
  CreateGroup(groupColorAjustment, category);
  CreateGroup(groupPeakDetect, category);
  CreateGroup(groupColorMap, category);
  CreateGroup(groupDither, category);
  CreateGroup(groupIcc, category);
  CreateGroup(groupCone, category);
  CreateGroup(groupBlend, category);
  CreateGroup(groupDeinterlace, category);
  CreateGroup(groupDistort, category);
  pl_options placeboOptions = pl_options_alloc(NULL);
  //CMPCVRRenderer::Get()->GetPlHelper().
  pl_options_reset(placeboOptions, &pl_render_default_params);
  //value, 21468 minimum step maximum
  AddSlider(groupDeband, SETTING_LIBPLACEBO_DEBAND_ITERATIONS, 55202, SettingLevel::Basic, placeboOptions->deband_params.iterations, 21469, 0, 1, 8);
  AddSlider(groupDeband, SETTING_LIBPLACEBO_DEBAND_THRESHOLD, 55203, SettingLevel::Basic, placeboOptions->deband_params.threshold, 21469, (float)0, (float)1, (float)256);
  AddSlider(groupDeband, SETTING_LIBPLACEBO_DEBAND_RADIUS, 55204, SettingLevel::Basic, placeboOptions->deband_params.radius, 21469, (float)0, (float)1, (float)256);
  AddSlider(groupDeband, SETTING_LIBPLACEBO_DEBAND_GRAIN, 55205, SettingLevel::Basic, placeboOptions->deband_params.grain, 21469, (float)0, (float)1, (float)512);

  AddSlider(groupSigmoid, SETTING_LIBPLACEBO_SIGMOID_CENTER, 55206, SettingLevel::Basic, placeboOptions->sigmoid_params.center, 21469, 0.0, 0.1, 1.0);
  AddSlider(groupSigmoid, SETTING_LIBPLACEBO_SIGMOID_SLOPE, 55207, SettingLevel::Basic, placeboOptions->sigmoid_params.slope, 21469, 0.0, 1.0, 100.0);
  /* 
      // Backing storage for all of the various rendering parameters. Whether
    // or not these params are active is determined by whether or not
    // `params.*_params` is set to this address or NULL.
    done struct pl_deband_params deband_params;
    struct pl_sigmoid_params sigmoid_params;
    struct pl_color_adjustment color_adjustment;
    struct pl_peak_detect_params peak_detect_params;
    struct pl_color_map_params color_map_params;
    struct pl_dither_params dither_params;
    struct pl_icc_params icc_params PL_DEPRECATED_IN(v6.327);
    struct pl_cone_params cone_params;
    struct pl_blend_params blend_params;
    struct pl_deinterlace_params deinterlace_params;
    struct pl_distort_params distort_params;

    // Backing storage for "custom" scalers. `params.upscaler` etc. will
    // always be a pointer either to a built-in pl_filter_config, or one of
    // these structs. `name`, `description` and `allowed` will always be
    // valid for the respective type of filter config.
    struct pl_filter_config upscaler;
    struct pl_filter_config downscaler;
    struct pl_filter_config plane_upscaler;
    struct pl_filter_config plane_downscaler;
    struct pl_filter_config frame_mixer;
  
  
  
  void nk_property_int(struct nk_context* ctx, const char* name, int min, int* val, int max, int step, float inc_per_pixel);
  * sigmoid_params
     nk_property_float(nk, "Sigmoid center", 0, &spar->center, 1, 0.1, 0.01);
     nk_property_float(nk, "Sigmoid slope", 0, &spar->slope, 100, 1, 0.1);
     
     color_adjustment
     nk_property_float(nk, "Brightness", -1, &adj->brightness, 1, 0.1, 0.005);
     nk_property_float(nk, "Contrast", 0, &adj->contrast, 10, 0.1, 0.005);
     nk_property_float(nk, "Saturation", 0, &adj->saturation, 10, 0.1, 0.005);
     nk_property_float(nk, "Gamma", 0, &adj->gamma, 10, 0.1, 0.005);
     Convert to human-friendly temperature values for display
     int temp = (int) roundf(adj->temperature * 3500) + 6500;
     nk_property_int(nk, "Temperature (K)", 3000, &temp, 10000, 10, 5);
     adj->temperature = (temp - 6500) / 3500.0;

     deinterlace_params
     static const char *deint_algos[PL_DEINTERLACE_ALGORITHM_COUNT] = {
                [PL_DEINTERLACE_WEAVE]  = "Field weaving (no-op)",
                [PL_DEINTERLACE_BOB]    = "Naive bob (line doubling)",
                [PL_DEINTERLACE_YADIF]  = "Yadif (\"yet another deinterlacing filter\")",
            };

            nk_label(nk, "Deinterlacing algorithm", NK_TEXT_LEFT);
            dpar->algo = nk_combo(nk, deint_algos, PL_DEINTERLACE_ALGORITHM_COUNT,
                                  dpar->algo, 16, nk_vec2(nk_widget_width(nk), 300));
    

    struct pl_distort_params *dpar = &opts->distort_params;
            nk_layout_row_dynamic(nk, 24, 2);
            par->distort_params = nk_check_label(nk, "Enable", par->distort_params) ? dpar : NULL;
            if (nk_button_label(nk, "Reset settings"))
                *dpar = pl_distort_default_params;

            static const char *address_modes[PL_TEX_ADDRESS_MODE_COUNT] = {
                [PL_TEX_ADDRESS_CLAMP]  = "Clamp edges",
                [PL_TEX_ADDRESS_REPEAT] = "Repeat edges",
                [PL_TEX_ADDRESS_MIRROR] = "Mirror edges",
            };

            nk_checkbox_label(nk, "Constrain bounds", &dpar->constrain);
            dpar->address_mode = nk_combo(nk, address_modes, PL_TEX_ADDRESS_MODE_COUNT,
                                          dpar->address_mode, 16, nk_vec2(nk_widget_width(nk), 100));
            bool alpha = nk_check_label(nk, "Transparent background", dpar->alpha_mode);
            dpar->alpha_mode = alpha ? PL_ALPHA_INDEPENDENT : PL_ALPHA_NONE;
            nk_checkbox_label(nk, "Bicubic interpolation", &dpar->bicubic);

            struct pl_transform2x2 *tf = &dpar->transform;
            nk_property_float(nk, "Scale X", -10.0, &tf->mat.m[0][0], 10.0, 0.1, 0.005);
            nk_property_float(nk, "Shear X", -10.0, &tf->mat.m[0][1], 10.0, 0.1, 0.005);
            nk_property_float(nk, "Shear Y", -10.0, &tf->mat.m[1][0], 10.0, 0.1, 0.005);
            nk_property_float(nk, "Scale Y", -10.0, &tf->mat.m[1][1], 10.0, 0.1, 0.005);
            nk_property_float(nk, "Offset X", -10.0, &tf->c[0], 10.0, 0.1, 0.005);
            nk_property_float(nk, "Offset Y", -10.0, &tf->c[1], 10.0, 0.1, 0.005);

            float zoom_ref = fabsf(tf->mat.m[0][0] * tf->mat.m[1][1] -
                                   tf->mat.m[0][1] * tf->mat.m[1][0]);
            zoom_ref = logf(fmaxf(zoom_ref, 1e-4));
            float zoom = zoom_ref;
            nk_property_float(nk, "log(Zoom)", -10.0, &zoom, 10.0, 0.1, 0.005);
            pl_transform2x2_scale(tf, expf(zoom - zoom_ref));

            float angle_ref = (atan2f(tf->mat.m[1][0], tf->mat.m[1][1]) -
                               atan2f(tf->mat.m[0][1], tf->mat.m[0][0])) / 2;
            angle_ref = fmodf(angle_ref * 180/M_PI + 540, 360) - 180;
            float angle = angle_ref;
            nk_property_float(nk, "Rotate (°)", -200, &angle, 200, -5, -0.2);
            float angle_delta = (angle - angle_ref) * M_PI / 180;
            const pl_matrix2x2 rot = pl_matrix2x2_rotation(angle_delta);
            pl_matrix2x2_rmul(&rot, &tf->mat);

            bool flip_ox = nk_button_label(nk, "Flip output X");
            bool flip_oy = nk_button_label(nk, "Flip output Y");
            bool flip_ix = nk_button_label(nk, "Flip input X");
            bool flip_iy = nk_button_label(nk, "Flip input Y");
            if (flip_ox ^ flip_ix)
                tf->mat.m[0][0] = -tf->mat.m[0][0];
            if (flip_ox ^ flip_iy)
                tf->mat.m[0][1] = -tf->mat.m[0][1];
            if (flip_oy ^ flip_ix)
                tf->mat.m[1][0] = -tf->mat.m[1][0];
            if (flip_oy ^ flip_iy)
                tf->mat.m[1][1] = -tf->mat.m[1][1];
            if (flip_ox)
                tf->c[0] = -tf->c[0];
            if (flip_oy)
                tf->c[1] = -tf->c[1];

      struct pl_cone_params *cpar = &opts->cone_params;
            nk_layout_row_dynamic(nk, 24, 2);
            par->cone_params = nk_check_label(nk, "Color blindness", par->cone_params) ? cpar : NULL;
            if (nk_button_label(nk, "Default values"))
                *cpar = pl_vision_normal;
            nk_layout_row(nk, NK_DYNAMIC, 24, 5, (float[]){ 0.25, 0.25/3, 0.25/3, 0.25/3, 0.5 });
            nk_label(nk, "Cone model:", NK_TEXT_LEFT);
            unsigned int cones = cpar->cones;
            nk_checkbox_flags_label(nk, "L", &cones, PL_CONE_L);
            nk_checkbox_flags_label(nk, "M", &cones, PL_CONE_M);
            nk_checkbox_flags_label(nk, "S", &cones, PL_CONE_S);
            cpar->cones = cones;
            nk_property_float(nk, "Sensitivity", 0.0, &cpar->strength, 5.0, 0.1, 0.01);
  */

}

void CGUIDialogLibplacebo::OnSettingChanged(const std::shared_ptr<const CSetting>& setting)
{
  if (setting == NULL)
    return;

  
  CGUIDialogSettingsManualBase::OnSettingChanged(setting);
  const std::string &settingId = setting->GetId();

}

void CGUIDialogLibplacebo::OnSettingAction(const std::shared_ptr<const CSetting>& setting)
{
  if (setting == NULL)
    return;

  CGUIDialogSettingsManualBase::OnSettingAction(setting);
  const std::string &settingId = setting->GetId();

  
}