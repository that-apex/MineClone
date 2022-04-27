# find vulcan
find_package(Vulkan REQUIRED)

# find shaderc
if (CMAKE_BUILD_TYPE EQUAL "DEBUG")
    find_library(shaderc_LIBRARY NAMES shaderc_combinedd HINTS "$ENV{VULKAN_SDK}/lib")
else ()
    find_library(shaderc_LIBRARY NAMES shaderc_combined HINTS "$ENV{VULKAN_SDK}/lib")
endif ()

# shaders
add_subdirectory("shader")

# library
add_library(MineClone_Client STATIC
        src/Game/MineCloneGame.cpp
        src/GFX/Game.cpp
        src/GFX/Graphics.cpp
        src/GFX/Shader.cpp
        src/GFX/SwapChain.cpp
        src/GFX/VulkanContext.cpp
        src/Client.cpp
)

add_dependencies(MineClone_Client MineClone_Client_Shaders)

target_include_directories(MineClone_Client
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(MineClone_Client
    PUBLIC
        glfw
        Vulkan::Vulkan
        "${shaderc_LIBRARY}"
    PRIVATE
        MineClone_Client_Shaders
)
