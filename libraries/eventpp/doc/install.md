# Install and use eventpp in your project

- [Install and use eventpp in your project](#install-and-use-eventpp-in-your-project)
	- [Include the source code in your project directly](#include-the-source-code-in-your-project-directly)
	- [Use CMake FetchContent](#use-cmake-fetchcontent)
	- [Use Vcpkg package manager](#use-vcpkg-package-manager)
	- [Use Conan package manager](#use-conan-package-manager)
	- [Use Hunter package manager](#use-hunter-package-manager)
	- [Use Homebrew on macOS (or Linux)](#use-homebrew-on-macos-or-linux)
	- [Install using CMake locally and use it in CMake](#install-using-cmake-locally-and-use-it-in-cmake)

`eventpp` package is available in C++ package managers Vcpkg, Conan, Hunter, and Homebrew.  
`eventpp` is header only and not requires building. There are various methods to use `eventpp`.  
Here lists all possible methods to use `eventpp`.  

## Include the source code in your project directly

eventpp is header only library. Just clone the source code, or use git submodule, then add the 'include' folder inside eventpp to your project include directory, then you can use the library.  
You don't need to link to any source code.  

## Use CMake FetchContent

Add below code to your CMake file

```
include(FetchContent)
FetchContent_Declare(
    eventpp
    GIT_REPOSITORY https://github.com/wqking/eventpp
    GIT_TAG        d2db8af2a46c79f8dc75759019fba487948e9442 # v0.1.3
)
FetchContent_MakeAvailable(eventpp)
```

Then `eventpp` is available to your project. If `GIT_TAG` is omitted, the latest code on master branch will be used.

## Use Vcpkg package manager

```
vcpkg install eventpp
```

Then in your CMake project CMakeLists.txt file, put below code, remember to replace ${TARGET} with your target,

```
find_package(eventpp CONFIG REQUIRED)
target_link_libraries(${TARGET} PRIVATE eventpp::eventpp)
find_path(EVENTPP_INCLUDE_DIR eventpp/eventqueue.h)
include_directories(${EVENTPP_INCLUDE_DIR})
```

Then run cmake, note you need -DCMAKE_TOOLCHAIN_FILE to specify vcpkg, and replace -G with your generator
```
cmake .. -DCMAKE_TOOLCHAIN_FILE=VCPKGDIR/vcpkg/scripts/buildsystems/vcpkg.cmake -G"MinGW Makefiles"
```

Note with vcpkg, only the released version can be used, which may be behind the latest update. You can't use the minor update on the master branch.

## Use Conan package manager

In your CMake project, add a conanlist.txt file that should contain,

```
[requires]
eventpp/0.1.3

[generators]
CMakeDeps
CMakeToolchain
```

Then in your CMakeLists, add,

```
find_package(eventpp CONFIG REQUIRED)
target_link_libraries(${TARGET} PRIVATE eventpp::eventpp)
```

Then run `conan install . --output-folder=build --build=missing` in your project folder (assuming the default conan profile exists and is okay).  
Then cd into folder build, run `cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -G "Whatever generator you want" -DCMAKE_BUILD_TYPE=Release`, alternatively it can also use cmake presets instead of passing the toolchain file on cmake >=3.23

There are [more information here](https://github.com/wqking/eventpp/issues/70).

## Use Hunter package manager

You may follow the example code on [Hunter document](https://hunter.readthedocs.io/en/latest/packages/pkg/eventpp.html). Below is the code snippet from that document,  

```
hunter_add_package(eventpp)
find_package(eventpp CONFIG REQUIRED)

add_executable(main main.cpp)
target_link_libraries(main eventpp::eventpp)
include_directories(${EVENTPP_INCLUDE_DIR})
```

## Use Homebrew on macOS (or Linux)

```
brew install eventpp
```

## Install using CMake locally and use it in CMake

Note: this is only an alternative, you should use the FetchContent method instead of this.  
If you are going to use eventpp in CMake managed project, you can install eventpp then use it in CMake.  
In eventpp root folder, run the commands,  
```
mkdir build
cd build
cmake ..
sudo make install
```

Then in the project CMakeLists.txt,   
```
# the project target is mytest, just for example
add_executable(mytest test.cpp)

find_package(eventpp)
if(eventpp_FOUND)
    target_link_libraries(mytest eventpp::eventpp)
else(eventpp_FOUND)
    message(FATAL_ERROR "eventpp library is not found")
endif(eventpp_FOUND)
```

Note: when using this method with MingW on Windows, by default CMake will install eventpp in non-writable system folder and get error. You should specify another folder to install. To do so, replace `cmake ..` with `cmake .. -DCMAKE_INSTALL_PREFIX="YOUR_NEW_LIB_FOLDER"`.
