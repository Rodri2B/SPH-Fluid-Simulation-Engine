cmake_minimum_required (VERSION 3.16)

project ("sim_fluid" VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp -Ofast -Wall")

cmake_policy(SET CMP0072 NEW)
find_package(OpenGL REQUIRED)

##################### PCL ###################################
# Make sure CMake will search your local PCL install
# (uses the HOME environment variable so it works on any machine/user)
list(APPEND CMAKE_PREFIX_PATH
     "$ENV{HOME}/Libraries/eigen3.4"
     "$ENV{HOME}/Libraries/VTK9.4.2"
     "$ENV{HOME}/Libraries/flann"
     "$ENV{HOME}/Libraries/boost1.88"
     "$ENV{HOME}/Libraries/qhull"
     "$ENV{HOME}/Libraries/OpenCV"
     "$ENV{HOME}/Libraries/yaml_cpp"
)   

find_package( Eigen3 3.4 REQUIRED )

#############################################################

### OpenCV

find_package(OpenCV REQUIRED )

############################################################

############################################################

### Leaven

add_subdirectory($ENV{HOME}/Libraries/Leaven/lib ${CMAKE_BINARY_DIR}/leaven-build)
include_directories($ENV{HOME}/Libraries/Leaven/lib)

############################################################

set(CURRENT_USER $ENV{USER})

set(LIB_DIR "/home/${CURRENT_USER}/Libraries")

add_subdirectory("${LIB_DIR}/glfw" "${CMAKE_BINARY_DIR}/glfw")

include_directories("${LIB_DIR}/glfw/include")

add_subdirectory("${LIB_DIR}/glm" "${CMAKE_BINARY_DIR}/glm")

include_directories("${LIB_DIR}")

set(GLAD_DIR "${LIB_DIR}/Glad")
set(IMGUI_DIR "${LIB_DIR}/imgui")

add_library(
    glad
    ${GLAD_DIR}/glad/glad.h
    ${GLAD_DIR}/glad/glad.c
    ${GLAD_DIR}/KHR/khrplatform.h
)

file(GLOB IMGUI_SOURCES "${IMGUI_DIR}/*.cpp")
file(GLOB IMGUI_HEADERS "${IMGUI_DIR}/*.h")

set(VHACD_DIR "${LIB_DIR}/v-hacd/include")

add_library(
    imgui
    ${IMGUI_HEADERS}
    ${IMGUI_SOURCES}
    ${IMGUI_DIR}/backends/imgui_impl_glfw.h
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.h
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    
)

target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

add_library(
    camera
    include/camera/lib_opengl/quaternion_custom.h
    include/camera/lib_opengl/camera_quat.h
    include/camera/lib_opengl/camera_quat.cpp
    include/camera/lib_opengl/camera_quat_arc.cpp
    include/camera/lib_opengl/camera_quat_virtual.cpp


)

add_library(
    mesh
    include/mesh/lib_opengl/mesh_original.h

)

add_library(
    model
    include/model/lib_opengl/model_original.h
    include/model/lib_opengl/texture_from_file.cpp

)

add_library(
    shader
    include/shader/lib_opengl/shader.h
    include/shader/lib_opengl/shader_compute.h
    include/shader/lib_opengl/shader_compute.cpp
)

add_library(
    utility
    include/utility/binding_vector.hpp
    include/utility/object_loader.hpp
    include/utility/object_loader.cpp
    
)


add_library(stb STATIC include/utility/stb_image.cpp)
set(STB_INCLUDE_PATH "${LIB_DIR}/stb")
target_include_directories(stb PUBLIC ${STB_INCLUDE_PATH})

add_library(
    texture_loader
    include/texture_loader/texture_loader.hpp
)

add_library(
    grid
    include/grid/grid.hpp
    include/grid/grid.cpp
)


add_library(
    main_scene
    include/scene/scene.hpp
    include/scene/scene.cpp
)

add_library(
    ImGuizmo
    include/utility/Imguizmo/ImGuizmo.h
    include/utility/Imguizmo/imoguizmo.hpp
    include/utility/Imguizmo/ImGuizmo.cpp
)

file(GLOB FLUIDSIM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/include/fluidsim/*.cpp")
file(GLOB FLUIDSIM_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/fluidsim/*.hpp")

add_library(
    Fluidsolver
    ${FLUIDSIM_HEADERS}
    ${FLUIDSIM_SOURCES}
)


target_include_directories(glad PUBLIC "${GLAD_DIR}")
target_include_directories(camera PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/camera")
target_include_directories(mesh PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/mesh")
target_include_directories(model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/model")
target_include_directories(shader PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/shader")

target_include_directories(utility PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/utility")
target_include_directories(texture_loader PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/texture_loader")
target_include_directories(grid PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/grid")


target_include_directories(ImGuizmo PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/utility/Imguizmo")

target_include_directories(Fluidsolver PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/fluidsim")

set_target_properties(camera PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(mesh PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(model PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(shader PROPERTIES LINKER_LANGUAGE CXX)

set_target_properties(utility PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(texture_loader PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(grid PROPERTIES LINKER_LANGUAGE CXX)



set_target_properties(ImGuizmo PROPERTIES LINKER_LANGUAGE CXX)

set_target_properties(main_scene PROPERTIES LINKER_LANGUAGE CXX)

set_target_properties(Fluidsolver PROPERTIES LINKER_LANGUAGE CXX)

# Set the path to Assimp's local installation
set(ASSIMP_DIR "${LIB_DIR}/assimp")

# Add the Assimp include directory
include_directories(${ASSIMP_DIR}/include)

# Add the Assimp library directory
link_directories(${ASSIMP_DIR}/lib)

# Set the path to Freetype's local installation
set(FREETYPE_DIR "${LIB_DIR}/freetype")

# Add the FreeType include directory
include_directories(${FREETYPE_DIR}/include/freetype2/)

# Add the FreeType library directory
link_directories(${FREETYPE_DIR}/lib)

# Link against the FreeType library
find_library(FREETYPE_LIBRARY freetype PATHS ${FREETYPE_DIR}/lib)

add_executable (fluidsim
    src/fluidsim_gui.cpp
)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(fluidsim PUBLIC stb)

set(glm_DIR "${LIB_DIR}/glm") # if necessary

#find_package(glm CONFIG REQUIRED)

target_link_libraries(shader PUBLIC glad glm::glm-header-only)

target_link_libraries(camera PUBLIC glad glm::glm-header-only shader)

target_link_libraries(model PUBLIC glad glm::glm-header-only stb assimp mesh shader)

target_link_libraries(utility PUBLIC shader model mesh stb)

target_link_libraries(grid PUBLIC glad glm::glm-header-only shader)

target_link_libraries(ImGuizmo PUBLIC imgui)

target_link_libraries(Fluidsolver PUBLIC glm::glm-header-only utility Eigen3::Eigen LeavenLib ${Boost_LIBRARIES})


############################## BOOST #####################################

#set(Boost_USE_STATIC_LIBS OFF) 
#set(Boost_USE_MULTITHREADED ON)  
#set(Boost_USE_STATIC_RUNTIME OFF)
#cmake_policy(SET CMP0167 OLD)
#find_package(Boost 1.88 REQUIRED COMPONENTS thread)
find_package(Boost CONFIG REQUIRED thread)

include_directories(${Boost_INCLUDE_DIRS}) 

##########################################################################

####################### yaml ###########################################

find_package(yaml-cpp REQUIRED)

########################################################################

find_package(OpenMP REQUIRED)

target_include_directories(main_scene
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include/scene"
)

target_link_libraries(main_scene
    #PRIVATE
    PUBLIC
        glad
        stb
        shader
        grid
        utility
        texture_loader
        camera
        mesh
        model
        glfw
        ${GLFW_LIBRARIES}
        OpenGL::GL
        ${FREETYPE_LIBRARY}
        imgui
        ImGuizmo
        ${Boost_LIBRARIES}
        glm::glm-header-only
        Fluidsolver
)

########################################################

target_link_libraries(fluidsim
    PUBLIC
        stb
        main_scene
        assimp
        OpenMP::OpenMP_CXX
)
