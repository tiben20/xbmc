set(SOURCES ChaptersManager.cpp
            DSFilterVersion.cpp
            DSGraph.cpp
            DSInputStreamPVRManager.cpp
            DSPlayer.cpp
            DSPlayerDatabase.cpp
            DSPropertyPage.cpp
            ExternalPixelShader.cpp
            FGFilter.cpp
            FGLoader.cpp
            FGManager.cpp
            FGManager2.cpp
            GraphFilters.cpp
            GUIDialogShaderList.cpp
            PixelShaderCompiler.cpp
            PixelShaderList.cpp
            ShadersSelectionRule.cpp
            StreamsManager.cpp
            subtitles/ILogImpl.cpp)

set(HEADERS ChaptersManager.h
            DSFilterVersion.h
            DSGraph.h
            DSInputStreamPVRManager.h
            DSMessage.h
            DSPlayer.h
            DSPlayerDatabase.h
            DSPropertyPage.h
            ExternalPixelShader.h
            FGFilter.h
            FGLoader.h
            FGManager.h
            FGManager2.h
            GraphFilters.h
            GUIDialogShaderList.h
            IDSPlayer.h
            moreuuids.h
            PixelShaderCompiler.h
            PixelShaderList.h
            ShadersSelectionRule.h
            StreamsManager.h
            subtitles/ILogImpl.h
            subtitles/DllLibSubs.h
            subtitles/ILog.h
			SComCli.h)

core_add_library(DSPlayer)
