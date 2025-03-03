get_filename_component(eventpp_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
if (NOT TARGET eventpp::eventpp)
  include("${eventpp_CMAKE_DIR}/eventppTargets.cmake")
endif ()

set(eventpp_LIBRARIES eventpp::eventpp)
