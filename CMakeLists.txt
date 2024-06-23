cmake_minimum_required(VERSION 3.5)

project(TesselTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

add_executable(TesselTest WIN32 main.cpp)

include(GNUInstallDirs)
install(TARGETS TesselTest
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)


target_include_directories(TesselTest PUBLIC "${Vulkan_INCLUDE_DIR}")
target_link_libraries(TesselTest PUBLIC "Shcore.lib")
