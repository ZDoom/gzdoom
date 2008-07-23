cmake_minimum_required( VERSION 2.6 )
project( snes_spc )

# I don't plan on debugging this, so make it a release build.
set( CMAKE_BUILD_TYPE "RelWithDebInfo" )

if( CMAKE_COMPILER_IS_GNUCXX )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fomit-frame-pointer" )
endif( CMAKE_COMPILER_IS_GNUCXX )

add_library( snes_spc
    snes_spc/dsp.cpp
    snes_spc/SNES_SPC.cpp
    snes_spc/SNES_SPC_misc.cpp
    snes_spc/SNES_SPC_state.cpp
    snes_spc/spc.cpp
    snes_spc/SPC_DSP.cpp
    snes_spc/SPC_Filter.cpp )
target_link_libraries( snes_spc )
