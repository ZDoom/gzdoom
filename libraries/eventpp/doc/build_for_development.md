# Build eventpp for development

The library itself is header only and doesn't need building.  
If you want to improve eventpp, you need to build the test code.  

There are three parts of code to test the library,

- unittests: tests the library. They require C++17 since it uses generic lambda and `std::any` (the library itself only requires C++11).
- tutorials: sample code to demonstrate how to use the library. They require C++11. If you want to have a quick study on how to use the library, you can look at the tutorials.
- benchmarks: measure the library performance.

All parts are in the `tests` folder.

All three parts require CMake to build, and there is a makefile to ease the building.  
Go to folder `tests/build`, then run `make` with different target.
- `make vc19` #generate solution files for Microsoft Visual Studio 2019, then open eventpptest.sln in folder project_vc19
- `make vc17` #generate solution files for Microsoft Visual Studio 2017, then open eventpptest.sln in folder project_vc17
- `make vc15` #generate solution files for Microsoft Visual Studio 2015, then open eventpptest.sln in folder project_vc15
- `make mingw` #build using MinGW
- `make linux` #build on Linux
- `make mingw_coverage` #build using MinGW and generate code coverage report

