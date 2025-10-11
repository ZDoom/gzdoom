cmake_minimum_required(VERSION 3.11)
project(zwidget)

if (UNIX AND NOT APPLE)
	include(FindPkgConfig)
	pkg_check_modules(DBUS dbus-1)
	if (!DBUS_FOUND)
		pkg_check_modules(DBUS REQUIRED dbus) # Fedora Linux looks for dbus instead
	endif()
	pkg_check_modules(WAYLANDPP
		wayland-client
		wayland-client++
		wayland-client-extra++
		wayland-client-unstable++
		wayland-cursor++
		xkbcommon>=0.5.0
	)
endif()

# SDL2 finding stuff
# Optional on all platforms
find_package(SDL2 QUIET)
if(NOT ${SDL2_FOUND})
	include(FindPkgConfig)
	pkg_search_module(SDL2 sdl2)
endif()

set(ZWIDGET_SOURCES
	src/core/canvas.cpp
	src/core/font.cpp
	src/core/image.cpp
	src/core/span_layout.cpp
	src/core/timer.cpp
	src/core/widget.cpp
	src/core/theme.cpp
	src/core/utf8reader.cpp
	src/core/pathfill.cpp
	src/core/truetypefont.cpp
	src/core/truetypefont.h
	src/core/picopng/picopng.cpp
	src/core/picopng/picopng.h
	src/core/nanosvg/nanosvg.cpp
	src/core/nanosvg/nanosvg.h
	src/core/nanosvg/nanosvgrast.h
	src/widgets/lineedit/lineedit.cpp
	src/widgets/mainwindow/mainwindow.cpp
	src/widgets/menubar/menubar.cpp
	src/widgets/scrollbar/scrollbar.cpp
	src/widgets/statusbar/statusbar.cpp
	src/widgets/textedit/textedit.cpp
	src/widgets/toolbar/toolbar.cpp
	src/widgets/toolbar/toolbarbutton.cpp
	src/widgets/imagebox/imagebox.cpp
	src/widgets/textlabel/textlabel.cpp
	src/widgets/pushbutton/pushbutton.cpp
	src/widgets/checkboxlabel/checkboxlabel.cpp
	src/widgets/dropdown/dropdown.cpp
	src/widgets/listview/listview.cpp
	src/widgets/tabwidget/tabwidget.cpp
	src/window/window.cpp
	src/window/stub/stub_open_folder_dialog.cpp
	src/window/stub/stub_open_folder_dialog.h
	src/window/stub/stub_open_file_dialog.cpp
	src/window/stub/stub_open_file_dialog.h
	src/window/stub/stub_save_file_dialog.cpp
	src/window/stub/stub_save_file_dialog.h
	src/window/ztimer/ztimer.h
	src/window/ztimer/ztimer.cpp
	src/systemdialogs/open_folder_dialog.cpp
	src/systemdialogs/open_file_dialog.cpp
	src/systemdialogs/save_file_dialog.cpp
)

set(ZWIDGET_INCLUDES
	include/zwidget/core/canvas.h
	include/zwidget/core/colorf.h
	include/zwidget/core/font.h
	include/zwidget/core/image.h
	include/zwidget/core/rect.h
	include/zwidget/core/pathfill.h
	include/zwidget/core/span_layout.h
	include/zwidget/core/timer.h
	include/zwidget/core/widget.h
	include/zwidget/core/theme.h
	include/zwidget/core/utf8reader.h
	include/zwidget/core/resourcedata.h
	include/zwidget/widgets/lineedit/lineedit.h
	include/zwidget/widgets/mainwindow/mainwindow.h
	include/zwidget/widgets/menubar/menubar.h
	include/zwidget/widgets/scrollbar/scrollbar.h
	include/zwidget/widgets/statusbar/statusbar.h
	include/zwidget/widgets/textedit/textedit.h
	include/zwidget/widgets/toolbar/toolbar.h
	include/zwidget/widgets/toolbar/toolbarbutton.h
	include/zwidget/widgets/imagebox/imagebox.h
	include/zwidget/widgets/textlabel/textlabel.h
	include/zwidget/widgets/pushbutton/pushbutton.h
	include/zwidget/widgets/checkboxlabel/checkboxlabel.h
	include/zwidget/widgets/listview/listview.h
	include/zwidget/widgets/tabwidget/tabwidget.h
	include/zwidget/window/window.h
	include/zwidget/window/x11nativehandle.h
	include/zwidget/window/waylandnativehandle.h
	include/zwidget/window/win32nativehandle.h
	include/zwidget/window/sdl2nativehandle.h
	include/zwidget/systemdialogs/open_folder_dialog.h
	include/zwidget/systemdialogs/open_file_dialog.h
	include/zwidget/systemdialogs/save_file_dialog.h
)

set(ZWIDGET_WIN32_SOURCES
	src/window/win32/win32_display_backend.cpp
	src/window/win32/win32_display_backend.h
	src/window/win32/win32_display_window.cpp
	src/window/win32/win32_display_window.h
	src/window/win32/win32_open_folder_dialog.cpp
	src/window/win32/win32_open_folder_dialog.h
	src/window/win32/win32_open_file_dialog.cpp
	src/window/win32/win32_open_file_dialog.h
	src/window/win32/win32_save_file_dialog.cpp
	src/window/win32/win32_save_file_dialog.h
	src/window/win32/win32_util.h
)

set(ZWIDGET_DBUS_SOURCES
	src/window/dbus/dbus_open_folder_dialog.cpp
	src/window/dbus/dbus_open_folder_dialog.h
	src/window/dbus/dbus_open_file_dialog.cpp
	src/window/dbus/dbus_open_file_dialog.h
	src/window/dbus/dbus_save_file_dialog.cpp
	src/window/dbus/dbus_save_file_dialog.h
)

set(ZWIDGET_COCOA_SOURCES
)

set(ZWIDGET_SDL2_SOURCES
	src/window/sdl2/sdl2_display_backend.cpp
	src/window/sdl2/sdl2_display_backend.h
	src/window/sdl2/sdl2_display_window.cpp
	src/window/sdl2/sdl2_display_window.h
)

set(ZWIDGET_X11_SOURCES
	src/window/x11/x11_display_backend.cpp
	src/window/x11/x11_display_backend.h
	src/window/x11/x11_display_window.cpp
	src/window/x11/x11_display_window.h
	src/window/x11/x11_connection.cpp
	src/window/x11/x11_connection.h
)

set(ZWIDGET_WAYLAND_SOURCES
	src/window/wayland/wayland_display_backend.cpp
	src/window/wayland/wayland_display_backend.h
	src/window/wayland/wayland_display_window.cpp
	src/window/wayland/wayland_display_window.h
	src/window/wayland/wl_fractional_scaling_protocol.cpp
	src/window/wayland/wl_fractional_scaling_protocol.hpp
	src/window/wayland/wl_cursor_shape.cpp
	src/window/wayland/wl_cursor_shape.hpp
	src/window/wayland/wl_xdg_toplevel_icon.cpp
	src/window/wayland/wl_xdg_toplevel_icon.hpp
)

source_group("src" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/.+")
source_group("src\\core" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/core/.+")
source_group("src\\core\\picopng" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/core/picopng/.+")
source_group("src\\core\\nanosvg" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/core/nanosvg/.+")
source_group("src\\widgets" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/.+")
source_group("src\\widgets\\lineedit" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/lineedit/.+")
source_group("src\\widgets\\mainwindow" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/mainwindow/.+")
source_group("src\\widgets\\menubar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/menubar/.+")
source_group("src\\widgets\\scrollbar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/scrollbar/.+")
source_group("src\\widgets\\statusbar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/statusbar/.+")
source_group("src\\widgets\\textedit" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/textedit/.+")
source_group("src\\widgets\\toolbar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/toolbar/.+")
source_group("src\\widgets\\imagebox" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/imagebox/.+")
source_group("src\\widgets\\textlabel" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/textlabel/.+")
source_group("src\\widgets\\pushbutton" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/pushbutton/.+")
source_group("src\\widgets\\checkboxlabel" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/checkboxlabel/.+")
source_group("src\\widgets\\dropdown" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/dropdown/.+")
source_group("src\\widgets\\listview" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/listview/.+")
source_group("src\\widgets\\tabwidget" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/tabwidget/.+")
source_group("src\\window" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/.+")
source_group("src\\window\\stub" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/stub/.+")
source_group("src\\window\\win32" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/win32/.+")
source_group("src\\window\\sdl2" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/sdl2/.+")
source_group("src\\window\\x11" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/x11/.+")
source_group("src\\window\\wayland" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/wayland/.+")
source_group("src\\window\\dbus" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/dbus/.+")
source_group("src\\systemdialogs" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/systemdialogs/.+")
source_group("include" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/.+")
source_group("include\\core" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/core/.+")
source_group("include\\widgets" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/.+")
source_group("include\\widgets\\lineedit" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/lineedit/.+")
source_group("include\\widgets\\mainwindow" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/mainwindow/.+")
source_group("include\\widgets\\menubar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/menubar/.+")
source_group("include\\widgets\\scrollbar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/scrollbar/.+")
source_group("include\\widgets\\statusbar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/statusbar/.+")
source_group("include\\widgets\\textedit" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/textedit/.+")
source_group("include\\widgets\\toolbar" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/toolbar/.+")
source_group("include\\widgets\\imagebox" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/imagebox/.+")
source_group("include\\widgets\\textlabel" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/textlabel/.+")
source_group("include\\widgets\\pushbutton" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/pushbutton/.+")
source_group("include\\widgets\\checkboxlabel" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/checkboxlabel/.+")
source_group("include\\widgets\\listview" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/listview/.+")
source_group("include\\widgets\\tabwidget" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/widgets/tabwidget/.+")
source_group("include\\window" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/window/.+")
source_group("include\\systemdialogs" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/systemdialogs/.+")

# Include directory to external projects using zwidget
include_directories(include)

# Internal include dirs for building zwidget
set(ZWIDGET_INCLUDE_DIRS include/zwidget src)

set(ZWIDGET_COMPILE_OPTIONS)

if(WIN32)
	set(ZWIDGET_SOURCES ${ZWIDGET_SOURCES} ${ZWIDGET_WIN32_SOURCES})
	set(ZWIDGET_DEFINES -DUNICODE -D_UNICODE)
	set(ZWIDGET_LINK_OPTIONS)
	if(MSVC)
		# Use all cores for compilation
		set(ZWIDGET_COMPILE_OPTIONS ${ZWIDGET_COMPILE_OPTIONS} /MP)

		# Ignore specific warnings
		#set(ZWIDGET_COMPILE_OPTIONS ${ZWIDGET_COMPILE_OPTIONS} /wd4244 /wd4267 /wd4005 /wd4018)

		# Don't slow down std containers in debug builds
		set(ZWIDGET_DEFINES ${ZWIDGET_DEFINES})

		# Ignore warning about legacy CRT functions
		#set(ZWIDGET_DEFINES ${ZWIDGET_DEFINES} -D_CRT_SECURE_NO_WARNINGS)
	endif()
elseif(APPLE)
	set(ZWIDGET_SOURCES ${ZWIDGET_SOURCES} ${ZWIDGET_COCOA_SOURCES})
	set(ZWIDGET_LIBS ${CMAKE_DL_LIBS} -ldl)
	set(ZWIDGET_DEFINES -DUNIX -D_UNIX)
	set(ZWIDGET_LINK_OPTIONS -pthread)
else()
	set(ZWIDGET_SOURCES ${ZWIDGET_SOURCES} ${ZWIDGET_X11_SOURCES})
	set(ZWIDGET_LIBS ${CMAKE_DL_LIBS} -lX11 -lXi)
	set(ZWIDGET_DEFINES -DUNIX -D_UNIX -DUSE_X11)
	set(ZWIDGET_LINK_OPTIONS -pthread)
	if (DBUS_FOUND)
		include_directories("/usr/lib64/dbus-1.0/include") # Bazzite (and probably Fedora 42) keeps the platform-specific dbus headers in this folder
		set(ZWIDGET_SOURCES ${ZWIDGET_SOURCES} ${ZWIDGET_DBUS_SOURCES})
		set(ZWIDGET_INCLUDE_DIRS ${ZWIDGET_INCLUDE_DIRS} ${DBUS_INCLUDE_DIRS})
		set(ZWIDGET_LIBS ${ZWIDGET_LIBS} ${DBUS_LDFLAGS})
		set(ZWIDGET_DEFINES ${ZWIDGET_DEFINES} -DUSE_DBUS)
	endif()
	if (WAYLANDPP_FOUND)
		set(ZWIDGET_SOURCES ${ZWIDGET_SOURCES} ${ZWIDGET_WAYLAND_SOURCES})
		set(ZWIDGET_INCLUDE_DIRS ${ZWIDGET_INCLUDE_DIRS} ${WAYLANDPP_INCLUDE_DIRS})
		set(ZWIDGET_LIBS ${ZWIDGET_LIBS} ${WAYLANDPP_LDFLAGS})
		set(ZWIDGET_DEFINES ${ZWIDGET_DEFINES} -DUSE_WAYLAND)
	endif()
endif()

if(SDL2_FOUND AND NOT WIN32)
	set(ZWIDGET_SOURCES ${ZWIDGET_SOURCES} ${ZWIDGET_SDL2_SOURCES})
	set(ZWIDGET_LIBS ${ZWIDGET_LIBS} ${SDL2_LIBRARIES} -lSDL2)
	set(ZWIDGET_DEFINES ${ZWIDGET_DEFINES} -DUSE_SDL2)
endif()

if(MSVC)
	set(CXX_WARNING_FLAGS /W3)
else()
	set(CXX_WARNING_FLAGS -Wall -Wpedantic)
endif()

add_library(zwidget STATIC ${ZWIDGET_SOURCES} ${ZWIDGET_INCLUDES})
target_compile_options(zwidget PRIVATE ${ZWIDGET_COMPILE_OPTIONS})
target_compile_definitions(zwidget PRIVATE ${ZWIDGET_DEFINES})
target_include_directories(zwidget PRIVATE ${ZWIDGET_INCLUDE_DIRS})
target_link_options(zwidget PRIVATE ${ZWIDGET_LINK_OPTIONS})
target_link_libraries(zwidget ${ZWIDGET_LIBS})
set_target_properties(zwidget PROPERTIES CXX_STANDARD 20)
target_compile_options(zwidget PRIVATE ${CXX_WARNING_FLAGS})
if(SDL2_FOUND)
	target_include_directories(zwidget PRIVATE ${SDL2_INCLUDE_DIRS})
endif()

if(MSVC)
	set_property(TARGET zwidget PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(ZWIDGET_BUILD_EXAMPLE "Build the zwidget example application" ON)

if(ZWIDGET_BUILD_EXAMPLE)
	add_executable(zwidget_example WIN32
		example/example.cpp
		example/picopng.cpp
		example/picopng.h
	)
	target_compile_options(zwidget_example PRIVATE ${CXX_WARNING_FLAGS})

	add_custom_command(
		TARGET zwidget_example POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
			"${CMAKE_CURRENT_SOURCE_DIR}/example/banner.png"
			"${CMAKE_CURRENT_BINARY_DIR}/banner.png"
		COMMAND ${CMAKE_COMMAND} -E copy
			"${CMAKE_CURRENT_SOURCE_DIR}/example/OpenSans.ttf"
			"${CMAKE_CURRENT_BINARY_DIR}/OpenSans.ttf"
	)

	target_include_directories(zwidget_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/example)
	target_link_libraries(zwidget_example PRIVATE zwidget)

	if(WIN32)
		target_compile_definitions(zwidget_example PRIVATE UNICODE _UNICODE)
		target_link_libraries(zwidget_example PRIVATE gdi32 user32 shell32 comdlg32)
	elseif(APPLE)
		target_link_libraries(zwidget_example PRIVATE "-framework Cocoa -framework OpenGL")
	else()
		target_link_libraries(zwidget_example PRIVATE ${ZWIDGET_LIBS})
	endif()

	set_target_properties(zwidget_example PROPERTIES CXX_STANDARD 20)

	if(MSVC)
		set_property(TARGET zwidget_example PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
endif()
