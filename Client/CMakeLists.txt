find_package(Vulkan  REQUIRED)

add_library(MineClone_Client STATIC
        src/Game/MineCloneGame.cpp
        src/GFX/Game.cpp
        src/GFX/Graphics.cpp
        src/Client.cpp
)

target_include_directories(MineClone_Client
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(MineClone_Client 
    PUBLIC
        glfw
        Vulkan::Vulkan
)
