
find_path(VPX_INCLUDE_DIR NAMES vpx/vp8dx.h vpx/vpx_decoder.h)
find_library(VPX_LIBRARIES NAMES vpx)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VPX DEFAULT_MSG VPX_LIBRARIES VPX_INCLUDE_DIR)

mark_as_advanced(VPX_INCLUDE_DIR VPX_LIBRARIES)
