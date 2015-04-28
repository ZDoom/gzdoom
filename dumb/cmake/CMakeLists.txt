cmake_minimum_required(VERSION 2.6)
project(libdumb C)

set(CMAKE_C_FLAGS "-Wall -DDUMB_DECLARE_DEPRECATED -D_USE_SSE -msse -Wno-unused-variable -Wno-unused-but-set-variable")
set(CMAKE_C_FLAGS_DEBUG "-ggdb -DDEBUGMODE=1 -D_DEBUG")
set(CMAKE_C_FLAGS_RELEASE "-ffast-math -O2 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-ffast-math -g -O2 -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL "-ffast-math -Os -DNDEBUG")

link_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(../include/)

SET(SOURCES
    ../src/core/unload.c
    ../src/core/rendsig.c
    ../src/core/rendduh.c
    ../src/core/register.c
    ../src/core/readduh.c
    ../src/core/rawsig.c
    ../src/core/makeduh.c
    ../src/core/loadduh.c
    ../src/core/dumbfile.c
    ../src/core/duhtag.c
    ../src/core/duhlen.c
    ../src/core/atexit.c
    ../src/helpers/stdfile.c
    ../src/helpers/silence.c
    ../src/helpers/sampbuf.c
    ../src/helpers/riff.c
    ../src/helpers/resample.c
    ../src/helpers/memfile.c
    ../src/helpers/clickrem.c
    ../src/helpers/barray.c
    ../src/helpers/tarray.c
    ../src/it/xmeffect.c
    ../src/it/readxm2.c
    ../src/it/readxm.c
    ../src/it/readstm2.c
    ../src/it/readstm.c
    ../src/it/reads3m2.c
    ../src/it/reads3m.c
    ../src/it/readriff.c
    ../src/it/readptm.c
    ../src/it/readpsm.c
    ../src/it/readoldpsm.c
    ../src/it/readokt2.c
    ../src/it/readokt.c
    ../src/it/readmtm.c
    ../src/it/readmod2.c
    ../src/it/readmod.c
    ../src/it/readdsmf.c
    ../src/it/readasy.c
    ../src/it/readamf2.c
    ../src/it/readamf.c
    ../src/it/readam.c
    ../src/it/read6692.c
    ../src/it/read669.c
    ../src/it/ptmeffect.c
    ../src/it/loadxm2.c
    ../src/it/loadxm.c
    ../src/it/loadstm2.c
    ../src/it/loadstm.c
    ../src/it/loads3m2.c
    ../src/it/loads3m.c
    ../src/it/loadriff2.c
    ../src/it/loadriff.c
    ../src/it/loadptm2.c
    ../src/it/loadptm.c
    ../src/it/loadpsm2.c
    ../src/it/loadpsm.c
    ../src/it/loadoldpsm2.c
    ../src/it/loadoldpsm.c
    ../src/it/loadokt2.c
    ../src/it/loadokt.c
    ../src/it/loadmtm2.c
    ../src/it/loadmtm.c
    ../src/it/loadmod2.c
    ../src/it/loadmod.c
    ../src/it/loadasy2.c
    ../src/it/loadasy.c
    ../src/it/loadamf2.c
    ../src/it/loadamf.c
    ../src/it/load6692.c
    ../src/it/load669.c
    ../src/it/itunload.c
    ../src/it/itrender.c
    ../src/it/itread2.c
    ../src/it/itread.c
    ../src/it/itorder.c
    ../src/it/itmisc.c
    ../src/it/itload2.c
    ../src/it/itload.c
    ../src/it/readany.c
    ../src/it/loadany2.c
    ../src/it/loadany.c
    ../src/it/readany2.c
    ../src/helpers/resampler.c
    ../src/helpers/lpc.c
)

set(INSTALL_HEADERS
    ../include/dumb.h
)

add_library(dumb ${SOURCES})
set_target_properties(dumb PROPERTIES DEBUG_POSTFIX d)

# Make sure the dylib install name path is set on OSX so you can include dumb in app bundles
IF(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_target_properties(dumb PROPERTIES INSTALL_NAME_DIR ${CMAKE_INSTALL_PREFIX}/lib)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL "Darwin")

INSTALL(FILES ${INSTALL_HEADERS} DESTINATION include/)
INSTALL(TARGETS dumb
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
