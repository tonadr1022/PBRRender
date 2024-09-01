project(pbr_render)

find_package(SDL2 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(unofficial-shaderc CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)

add_compile_definitions(TRACY_ENABLE)
option(TRACY_ENABLE "" ON)

set(SOURCES
    main.cpp
    EAssert.cpp
    pch.cpp
    App.cpp
    MeshLoader.cpp
    Renderer.cpp
    Window.cpp
    ResourceManager.cpp

    gl/OpenGLDebug.cpp
    gl/Texture.cpp
    gl/ShaderManager.cpp
    gl/VertexArray.cpp
    gl/Shader.cpp
    Player.cpp
    camera/FPSCamera.cpp
    camera/OrbitCamera.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
    endif()
endif()

target_precompile_headers(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/pch.hpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    imgui::imgui
    GLEW::GLEW
    unofficial::shaderc::shaderc
    fastgltf::fastgltf
    glm::glm
    Tracy::TracyClient
    spdlog::spdlog
)
