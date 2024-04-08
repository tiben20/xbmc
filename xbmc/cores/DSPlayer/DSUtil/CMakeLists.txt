set(SOURCES DSUtil.cpp
            MediaTypeEx.cpp
			Geometry.cpp
			DSMPropertyBag.cpp
			FontInstaller.cpp
			GolombBuffer.cpp
			H264Nalu.cpp
			HdmvClipInfo.cpp
			MediaTypes.cpp
			NullRenderers.cpp
			vd.cpp
			text.cpp)

set(HEADERS DSUtil.h
			MediaTypeEx.h
			Geometry.h
			DSMPropertyBag.h
			DShowCommon.h
			FontInstaller.h
			HdmvClipInfo.h
			GolombBuffer.h
			H264Nalu.h
			MediaTypes.h
			Mpeg2Def.h
			NullRenderers.h
			simd_common.h
			vd.h
			text.h)

core_add_library(DSUtil)
