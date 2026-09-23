# CMake generated Testfile for 
# Source directory: C:/Users/STACJO~1/Desktop/ZAKADY~1
# Build directory: C:/Users/STACJO~1/Desktop/ZAKADY~1/build-r75
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test("simulation" "C:/Users/STACJO~1/Desktop/ZAKADY~1/build-r75/orbital_tests.exe" "C:/Users/STACJO~1/Desktop/ZAKADY~1/data/catalog.json")
set_tests_properties("simulation" PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/STACJO~1/Desktop/ZAKADY~1/CMakeLists.txt;53;add_test;C:/Users/STACJO~1/Desktop/ZAKADY~1/CMakeLists.txt;0;")
subdirs("_deps/raylib-build")
subdirs("_deps/box2d-build")
subdirs("_deps/json-build")
