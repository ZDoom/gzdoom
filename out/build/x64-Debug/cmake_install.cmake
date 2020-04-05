# Install script for directory: C:/Users/Sahil/Documents/New folder (3)/gzdoom

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/install/x64-Debug")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xDocumentationx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/docs" TYPE DIRECTORY FILES "C:/Users/Sahil/Documents/New folder (3)/gzdoom/docs/")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/libraries/glslang/glslang/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/libraries/glslang/spirv/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/libraries/glslang/OGLCompilersDLL/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/libraries/zlib/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/libraries/asmjit/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/libraries/jpeg/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/libraries/bzip2/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/libraries/lzma/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/tools/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/libraries/gdtoa/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/wadsrc/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/wadsrc_bm/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/wadsrc_lights/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/wadsrc_extra/cmake_install.cmake")
  include("C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/src/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "C:/Users/Sahil/Documents/New folder (3)/gzdoom/out/build/x64-Debug/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
