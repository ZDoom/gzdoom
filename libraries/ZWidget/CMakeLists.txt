cmake_minimum_required(VERSION 3.11)
project(zwidget)

set(ZWIDGET_SOURCES
	src/core/canvas.cpp
	src/core/font.cpp
	src/core/image.cpp
	src/core/span_layout.cpp
	src/core/timer.cpp
	src/core/widget.cpp
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
	src/widgets/listview/listview.cpp
	src/widgets/tabwidget/tabwidget.cpp
	src/window/window.cpp
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
)

set(ZWIDGET_WIN32_SOURCES
	src/window/win32/win32window.cpp
	src/window/win32/win32window.h
)

set(ZWIDGET_COCOA_SOURCES
)

set(ZWIDGET_SDL2_SOURCES
	src/window/sdl2/sdl2displaywindow.cpp
	src/window/sdl2/sdl2displaywindow.h
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
source_group("src\\widgets\\listview" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/listview/.+")
source_group("src\\widgets\\tabwidget" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/widgets/tabwidget/.+")
source_group("src\\window" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/src/window/.+")
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
source_group("include\\window\\win32" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/window/win32/.+")
source_group("include\\window\\sdl2" REGULAR_EXPRESSION "${CMAKE_CURRENT_SOURCE_DIR}/include/zwidget/window/sdl2/.+")

include_directories(include include/zwidget src)

if(WIN32)
	set(ZWIDGET_SOURCES ${ZWIDGET_SOURCES} ${ZWIDGET_WIN32_SOURCES})
	add_definitions(-DUNICODE -D_UNICODE)
	if(MINGW)
		add_definitions(-DMINGW)
	endif()
elseif(APPLE)
	set(ZWIDGET_SOURCES ${ZWIDGET_SOURCES} ${ZWIDGET_COCOA_SOURCES})
	set(ZWIDGET_LIBS ${CMAKE_DL_LIBS} -ldl)
	add_definitions(-DUNIX -D_UNIX)
	add_link_options(-pthread)
else()
	set(ZWIDGET_SOURCES ${ZWIDGET_SOURCES} ${ZWIDGET_SDL2_SOURCES})
	if(NOT HAIKU)
		set(ZWIDGET_LIBS ${CMAKE_DL_LIBS} -ldl)
	else()
		set(ZWIDGET_LIBS ${CMAKE_DL_LIBS})
	endif()
	add_definitions(-DUNIX -D_UNIX)
	add_link_options(-pthread)
endif()

if(MSVC)
	# Use all cores for compilation
	set(CMAKE_CXX_FLAGS "/MP ${CMAKE_CXX_FLAGS}")

	# Ignore warnings in third party code
	#set_source_files_properties(${ZWIDGET_SOURCES} PROPERTIES COMPILE_FLAGS "/wd4244 /wd4267 /wd4005 /wd4018 -D_CRT_SECURE_NO_WARNINGS")
endif()

add_library(zwidget STATIC ${ZWIDGET_SOURCES} ${ZWIDGET_INCLUDES})
target_link_libraries(zwidget ${ZWIDGET_LIBS})
set_target_properties(zwidget PROPERTIES CXX_STANDARD 17)

if(MSVC)
	set_property(TARGET zwidget PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
