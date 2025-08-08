# CMakeList.txt : CMake project for UE-SDK-Body-Generator, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (UE-SDK-Body-Generator "UE-SDK-Body-Generator.cpp" "UE-SDK-Body-Generator.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET UE-SDK-Body-Generator PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
