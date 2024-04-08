set(SOURCES DX11SubPic.cpp
			ISubPic.cpp
			MemSubPic.cpp
			CoordGeom.cpp
			DX9SubPic.cpp)

set(HEADERS DX9SubPic.h
			DX11SubPic.h
			ISubPic.h
			MemSubPic.h
			CoordGeom.h)

core_add_library(Subpic)
