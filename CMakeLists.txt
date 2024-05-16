project(fr)

add_library(fr STATIC "./src/fr.cpp" "./include/fr/fr.hpp")
target_link_libraries(fr glfw Vulkan::Vulkan)
target_include_directories(fr PUBLIC "./include/")