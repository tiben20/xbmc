/*
 *      Copyright (C) 2013 Arne Morten Kvarving
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
#pragma once

#include "addons/binary-addons/AddonDll.h"
#include "addons/kodi-addon-dev-kit/include/kodi/kodi_audiodec_types.h"
#include "cores/paplayer/ICodec.h"
#include "music/tags/ImusicInfoTagLoader.h"
#include "filesystem/MusicFileDirectory.h"

namespace MUSIC_INFO
{
  class CMusicInfoTag;
  class EmbeddedArt;
}

namespace ADDON
{
  class CAudioDecoder : public CAddonDll,
                        public ICodec,
                        public MUSIC_INFO::IMusicInfoTagLoader,
                        public XFILE::CMusicFileDirectory
  {
  public:

    static std::unique_ptr<CAudioDecoder> FromExtension(CAddonInfo addonInfo, const cp_extension_t* ext);

    explicit CAudioDecoder(CAddonInfo addonInfo)
      : CAddonDll(std::move(addonInfo))
      , m_context{nullptr}
      , m_tags{false}
      , m_tracks{false}
      , m_channel{nullptr}
    {}

    CAudioDecoder(CAddonInfo addonInfo, std::string extension, std::string mimetype, bool tags,
        bool tracks, std::string codecName, std::string strExt);

    ~CAudioDecoder() override;

    // Things that MUST be supplied by the child classes
    bool Create();
    bool Init(const CFileItem& file, unsigned int filecache) override;
    int ReadPCM(uint8_t* buffer, int size, int* actualsize) override;
    bool Seek(int64_t time) override;
    bool CanInit() override { return true; }
    void DeInit();
    void Destroy();
    bool Load(const std::string& strFileName,
              MUSIC_INFO::CMusicInfoTag& tag,
              MUSIC_INFO::EmbeddedArt *art = NULL) override;
    int GetTrackCount(const std::string& strPath) override;

    const std::string& GetExtensions() const { return m_extension; }
    const std::string& GetMimetypes() const { return m_mimetype; }
    bool HasTags() const { return m_tags; }
    bool HasTracks() const { return m_tracks; }
  protected:
    std::string m_extension;
    std::string m_mimetype;
    void* m_context;
    bool m_tags;
    bool m_tracks;
    const AEChannel* m_channel;
    AddonInstance_AudioDecoder m_struct;
  };

} /*namespace ADDON*/
