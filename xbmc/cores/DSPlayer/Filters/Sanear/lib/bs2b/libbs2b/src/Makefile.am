lib_LTLIBRARIES = \
	libbs2b.la

bin_PROGRAMS = \
	bs2bconvert \
	bs2bstream


bs2b_HEADERS = \
	bs2b.h \
	bs2bclass.h \
	bs2btypes.h \
	bs2bversion.h

bs2bdir = $(includedir)/bs2b

libbs2b_la_DEPENDENCIES = \
    $(bs2b_HEADERS)

libbs2b_la_LDFLAGS = \
	-lm -version-info 0:0:0

libbs2b_la_SOURCES = \
	bs2b.c \
	bs2bclass.cpp

bs2bconvert_LDADD = \
	libbs2b.la

bs2bconvert_LDFLAGS = \
	-lsndfile

bs2bconvert_SOURCES = \
	bs2bconvert.c

bs2bstream_LDADD = \
	libbs2b.la

bs2bstream_SOURCES = \
	bs2bstream.c
