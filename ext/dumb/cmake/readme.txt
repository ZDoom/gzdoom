Howto build libdumb with cmake
==============================

A quick example
---------------

In libdumb cmake directory (dumb/cmake/), run:
```
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS:BOOL=ON ..
make
make install
```

Steps
-----

1. Create a new temporary build directory and cd into it
2. Run libdumb cmake file with cmake (eg. `cmake -DCMAKE_INSTALL_PREFIX=/install/dir -DBUILD_SHARED_LIBS:BOOL=OFF -DCMAKE_BUILD_TYPE=Release path/to/dumb/cmake/dir`).
3. Run make (eg. just `make` or `mingw32-make` or something).
4. If needed, run make install.

Flags
-----

* CMAKE_INSTALL_PREFIX sets the installation path prefix
* CMAKE_BUILD_TYPE sets the build type (eg. Release, Debug, RelWithDebInfo, MinSizeRel). Debug libraries will be named libdumbd, release libraries libdumb.
* BUILD_SHARED_LIBS selects whether cmake should build dynamic or static library (On=shared, OFF=static)
* You may also need to tell cmake what kind of makefiles to create with the "-G" flag. Eg. for MSYS one would say something like `cmake -G "MSYS Makefiles" .`.
