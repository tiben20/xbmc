set(SOURCES GUIDialogDSFilters.cpp
            GUIDialogDSManager.cpp
            GUIDialogDSPlayercoreFactory.cpp
            GUIDialogDSPlayerProcessInfo.cpp
            GUIDialogDSRules.cpp
            GUIDialogLAVAudio.cpp
            GUIDialogLAVSplitter.cpp
            GUIDialogLAVVideo.cpp
            GUIDialogMadvrSettings.cpp
            GUIDialogMadvrSettingsBase.cpp
            GUIDialogSanear.cpp)

set(HEADERS GUIDialogDSFilters.h
            GUIDialogDSManager.h
            GUIDialogDSPlayercoreFactory.h
            GUIDialogDSPlayerProcessInfo.h
            GUIDialogDSRules.h
            GUIDialogLAVAudio.h
            GUIDialogLAVSplitter.h
            GUIDialogLAVVideo.h
            GUIDialogMadvrSettings.h
            GUIDialogMadvrSettingsBase.h
            GUIDialogSanear.h)

core_add_library(Dialogs)
