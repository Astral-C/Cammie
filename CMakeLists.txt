cmake_minimum_required(VERSION 3.8)
project(cammie)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(lib)

file(GLOB_RECURSE CAMMIE_SRC
    # cammie
    "src/*.cpp"
    "src/*.c"
    "include/*.hpp"
    "include/*.h"
    
    # ImGuiFileDialog
    "lib/ImGuiFileDialog/ImGuiFileDialog/*.cpp"
)
find_package(ICU 61.0 COMPONENTS uc i18n REQUIRED)
add_executable(cammie ${CAMMIE_SRC})
target_include_directories(cammie PUBLIC include include/util lib/glfw/include lib/ImGuiFileDialog/ImGuiFileDialog/ lib/libgctools/include lib/fmt/include)
target_link_libraries(cammie PUBLIC j3dultra imgui glfw gctools ICU::uc ICU::i18n fmt)