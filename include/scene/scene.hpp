#ifndef SCENE_CLASS
#define SCENE_CLASS

#define IMGUI_ENABLE_FREETYPE
//#define IMGUI_ENABLE_FREETYPE_LUNASVG

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "misc/freetype/imgui_freetype.h"

#define GL_SILENCE_DEPRECATION
#define IMGUI_DEFINE_MATH_OPERATORS


#include <ImGuizmo.h>
#include <imoguizmo.hpp>

#include <lib_opengl/shader.h>
#include <lib_opengl/shader_compute.h>

#include <texture_loader.hpp>
#include <object_loader.hpp>

#include <lib_opengl/camera_quat.h>

#include <iostream>

#include <vector>
#include <memory>
#include <algorithm>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/mutex.hpp>

#include <thread>

#include <solver.hpp>

#include <atomic>
#include <chrono>

const unsigned int SCR_WIDTH_V = 800;
const unsigned int SCR_HEIGHT_V = 600;

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

namespace FluidSim {

struct floor_cfg{
    glm::vec2 floor_size;
    glm::vec3 tileCol1;
    glm::vec3 tileCol2;
    glm::vec3 tileCol3;
    glm::vec3 tileCol4;
    glm::vec3 tileColVariation;
    float tileDarkOffset;
    float tileSize;
    bool view_floor;
};

enum shader_type{
    DEFAULT,   
    WTH_GEOMETRY,
    COMPUTE
};

struct shaderConfig{
    std::string shader_name;
    std::string vertex_path;
    std::string fragment_path;
    std::string geometry_path;
    std::string compute_path;
    shader_type type;
};


struct ViewportCamConfig{

    glm::vec3 pivot_position;
    float pivot_distance;
    glm::vec2 yaw_pitch;
};

struct sceneObject{

    public:
    std::shared_ptr<Object> object;

    void AddTexture(const unsigned int &texture_index, const std::string &texture_name){
        Object_textures[texture_name] = texture_index;
    }

    void RemoveTexture(const std::string &texture_name){
        Object_textures.erase(texture_name);
    }

    std::map<std::string,bool> cameras_for_render;

    bool texture_transform_support;
    bool auto_render;

    std::map<std::string,unsigned int> Object_textures;
};



class Scene {

public:


//main contex
GLFWwindow* main_window;

bool enable_vsync = 1;
//float swap_interval = 1.0f;

ImGuiWindowFlags &windowFlags_global_reference;
ImGuiIO &io_reference;
ImGuiStyle &global_style_reference;
ImGuiStyle global_style_copy;

ImWchar *icons_ranges;

ImFont *font;
ImFont *font_mono;
ImFont *font_mono_offset;
ImFont *icons;

GLFWcursor* arrowCursor;
GLFWcursor* dragCursor;
GLFWcursor* invisible_cursor;

double global_cursor_xpos = 0.0f;
double global_cursor_ypos = 0.0f;

unsigned int window_viewport_current_width = SCR_WIDTH_V;
unsigned int window_viewport_current_height = SCR_HEIGHT_V;
ImVec2 previousImGuiSceneWindowSize = ImVec2(0, 0);
ImVec2 previousPickingImGuiSceneWindowSize = ImVec2(0, 0);


int monitorScreenHeight;
int monitorScreenWidth;

ImVec2 ViewportSize;
ImVec2 ViewportPos;
ImVec2 ViewportMin;
ImVec2 ViewportMax;



bool scene_hovered = false;
bool isDragging = false;

bool capture_mouse = false;
bool keyPressed = false;
bool scroll_callback_installed = false;
bool shift_pressed = false;

std::string object_selected = ""; 

bool isUsingGuizmo = false;

ImGuizmo::OPERATION currentTransformOp = ImGuizmo::TRANSLATE;
ImGuizmo::MODE currentTransformMode = ImGuizmo::WORLD;

bool button_state_vector[6] = {true,false,false,true,false,true};
bool want_to_toggle_proj = false;

std::unique_ptr<ArcBallCamera> viewport_camera;
glm::mat4 viewport_camera_view_matrix;
glm::mat4 viewport_camera_projection_matrix;
float ViewportCamVfov = 45.0f;
float ViewportCamVfovTemp;
bool last_frame_view_fov_input_txt_active = false;


float imoguizmo_proj[16];

GLuint dummyVAO;

Shader gridShader;
Shader InfoShader;
Shader OutlineShader;
Shader DrawOutlineShader;
Shader fluidsim_sh;
Shader fluidsim_p_sh;
Shader fluidsim_shade_sh;

Shader cube_edge_shader;

unsigned int viewport_framebuffer;
unsigned int viewport_textureColorbuffer;
unsigned int viewport_rbo;

unsigned int m_info_framebuffer;
unsigned int m_picking_texture;
unsigned int m_picking_depth_rbo;

unsigned int m_mask_framebuffer;
unsigned int m_mask_texture;
//unsigned int m_mask_depth_texture;

glm::mat4 guizmoview_proj;

std::map<std::string,std::shared_ptr<sceneObject>> ObjectsVector;
std::map<std::string,std::shared_ptr<Model>> ModelsVector;
std::map<std::string,std::shared_ptr<Shader>> ShadersVector;
std::map<std::string,std::shared_ptr<Shader_compute>> ComputeShadersVector;

std::shared_ptr<std::vector<unsigned int>> global_uniform_binding_indexes;

unsigned int uboViewportCamera;

unsigned int ProjViewBindingViewportCamera;

glm::vec4 background_scene_color = glm::vec4(0.69f, 0.69f, 0.93f, 1.0f);


bool lidar_board_render_mode[4] = {false,false,false,false};
bool camera_board_render_mode[4] = {false,false,false,false};

unsigned int particleVAO;
unsigned int particleVBO;

unsigned int particle_P_VAO;

std::shared_ptr<FluidSim::SPHsolver> fluid_solver;
std::thread fluid_solver_thread;

simulation_param_conf scene_simulation_param;

Scene(GLFWwindow *window,const int &screenWidth,const int &screenHeight,
        ImFont *m_font,ImFont *m_font_mono,ImFont *m_font_mono_off,ImFont *m_icons,
        GLFWcursor* m_arrowCursor,
        GLFWcursor* m_dragCursor,
        GLFWcursor* m_invisible_cursor,
        ImWchar *m_icons_ranges,
        ImGuiIO& io,ImGuiStyle& style, ImGuiWindowFlags &m_windowFlags_global_reference,
        std::shared_ptr<std::vector<unsigned int>> m_global_uniform_binding_indexes);

~Scene() {
    StopFluidThread(); // make sure we don't leave a thread running
}

void AddShader(const shaderConfig &shader_cfg);
void RemoveShader(const std::string &shader_name);

void AddModel(const std::string &model_path, const std::string &model_name);
void RemoveModel(const std::string &model_name);

void AddObject(std::shared_ptr<Object> object_ptr,const std::string &object_name);
void RemoveObject(const std::string &object_name);

void InitCameras(const ViewportCamConfig &view_cfg);//******************** 

void Verify_clicked_object();

void RenderStartMainViewport();
void RenderEndMainViewport();
void RenderMainViewportAuto();

void draw_outline();

void draw_grid();

void RenderInfoBuffer();

void Create_viewport_framebuffer();
void Rescale_viewport_framebuffer(int width, int height);
void Create_info_framebuffer();
void Rescale_info_framebuffer(int width, int height);
void Create_mask_framebuffer();
void Rescale_mask_framebuffer(int width, int height);

void update_viewport_cam_view_matrix_ubo();
void update_viewport_cam_projection_matrix_ubo();

void draw_statistics_viewport();
void draw_simulation_settings_viewport();

void draw_guizmo_toolbar();
void draw_imoguizmo();

void draw_viewport();
void draw_transform_settings();

void processInput(const float &deltaTime);

void Init_fluid_solver(const FluidSim::solver_init_conf &init_conf, const simulation_param_conf &param_conf);
void Upload_fluid_particles();
void Render_fluid_particles();

void SetDomain(std::shared_ptr<cube> domain);
void SetDomain(cube &domain);

void StartFluidThread();
void StopFluidThread();

void InitFloor(const glm::vec3 &position, const glm::vec3 &scale);

float particle_density = 600.0f;

floor_cfg floor_config;

glm::vec3 ball_color = glm::vec3(1.0,0.0,0.0);

private:

std::atomic<int64_t> simulation_time{0};

std::atomic<bool> fluid_thread_running{false};
std::atomic<bool> fluid_thread_start{false};

void FluidSolverLoop();

bool view_boundary = false;

bool view_velocity_field = true;
float particles_color[3] = {34.0f/255.f,87.0f/255.f,185.0f/255.f};

uint32_t current_renderfmode_item = 1;
const char* renderfmode_items[3] = {
    "Raw",
    "Instanced",
    "Shaded"
};


bool render_raw = false;
bool render_instanced = true;
bool render_shaded = false;

bool instanced_c_shape = true;
float particle_render_size = 0.08f;

};

}

extern std::shared_ptr<FluidSim::Scene> main_scene_class;

#endif