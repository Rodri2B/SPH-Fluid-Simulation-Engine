#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <object_loader.hpp>
#include "volumeSampler.h"
#include "surfaceSampler.h"
#include <vector>
#include <memory>
#include <optional>
#include <execution>
//#include <tbb/parallel_for_each.h>

#include <boost/thread/mutex.hpp>

#include "types_definitions.hpp"

namespace FluidSim {

inline float simple_pos_hash(uint32_t x) {
    return static_cast<float>((x * 2654435761u)%1000u)/1000.0f; 
}

class SPHsolver {

public:

SPHsolver(const baundary_config &b_conf, const float &fluid_h_conf, 
    const  float &particle_spacing_conf, const float &particle_radius_conf, 
    const float &rest_density_conf, const float &fluid_Rv_conf):
dummy_b_shader("../shaders/default/dummy.vs", "../shaders/default/dummy.fs"),
fluid_boundary(b_conf.b_position,b_conf.b_rotation,b_conf.b_size,dummy_b_shader)
{

    baundary_conf = b_conf;
    fluid_h = fluid_h_conf;
    particle_spacing = particle_spacing_conf;
    particle_radius = particle_radius_conf;
    rest_density = rest_density_conf;
    fluid_Rv = fluid_Rv_conf;

    //particle_mass = rest_density_conf*(particle_spacing_conf*particle_spacing_conf*particle_spacing_conf);
    //particle_mass = rest_density_conf/24000.0f;
    float n_p = 1.0f/particle_spacing+1.0f;
    particle_mass = rest_density/(n_p*n_p*n_p)*1.35f;

    //int search_count = static_cast<int>(std::floor(fluid_h/particle_spacing)) +1;
    //float N =0.0f;
    //for (int i = -search_count; i <= search_count; ++i)
    //for (int j = -search_count; j <= search_count; ++j)
    //for (int k = -search_count; k <= search_count; ++k)
    //{
    //    float dist = glm::length(particle_spacing*glm::vec3(static_cast<float>(i),static_cast<float>(j),static_cast<float>(k)));

    //    if(dist < fluid_h) N+=1.0f;
    //}
    //particle_mass = rest_density_conf*(315.0f / (64.0f * FLUID_PI * std::powf(fluid_h, 9)))/N;
    

    #ifdef USE_XPBF
    glm::vec3 deltaq = glm::vec3(fluid_h*delta_q,0.0f,0.0f);
    poly6_delta_q = poly6_kernel(deltaq);
    #endif

}


void ChangeSimulationParm(const simulation_param_conf &param_conf);

void UploadSimulationParm();

void AddObject(std::shared_ptr<Object> object_ptr,const std::string &object_name, 
    bool sample_volume = true, bool uniform_volume_sampling = true,
    bool sample_surface = false);

void RemoveObject(const std::string &object_name);

void InitParticles();

void InitGrid();

void ResetSim();

void CreateUnsortedList();

void SortList();

void BuildOffsetList();

uint32_t nextPowerOfTwo(uint32_t v);

uint32_t ZigZag(int v);

uint32_t GetFlatCellIndex(glm::int3 c);

void find_neighbors(const uint32_t &particleIndex,const glm::int3 &cellIndex,const fluid_particle& particle,std::vector<uint32_t> &neighborParticleIndexBuffer);

void solve_test(float &time_window);

#ifdef USE_XPBF
void XPBF_solve(float &time_window);
#elifdef USE_VISCOEL
void Viscoelastic_solve(float &time_window);
#else 
void PBF_solve(float &time_window);
void Kens_solve(float &time_window);
#endif

void format_particle_buffer();

void calc_density(const uint32_t &neighbors_count, const std::vector<uint32_t> &neighborParticleIndexBuffer,fluid_particle &particle);
void calc_density2(const uint32_t &neighbors_count, const std::vector<uint32_t> &neighborParticleIndexBuffer,fluid_particle &particle);

float poly6_kernel(const glm::vec3 &distance_vec);

glm::vec3 poly6_kernel_gradient(const glm::vec3 &distance_vec);

float poly6_kernel_laplacian(const glm::vec3 &distance_vec);

float spiky_kernel(const glm::vec3 &distance_vec);

glm::vec3 spiky_kernel_gradient(const glm::vec3 &distance_vec);

float viscosity_kernel_laplacian(const glm::vec3 &distance_vec);
float viscosity_kernel(const glm::vec3 &distance_vec);

////////////new///////////////////////////

float spiky_kernel_new(const glm::vec3 &distance_vec);

float spiky_kernel_near_d(const glm::vec3 &distance_vec);


glm::vec3 spiky_kernel_gradient_new(const glm::vec3 &distance_vec);
glm::vec3 spiky_kernel_gradient_near_d(const glm::vec3 &distance_vec);

baundary_config baundary_conf;
float fluid_h;
float particle_spacing;
float particle_radius;
float particle_mass;
float rest_density;
float fluid_Rv;
glm::vec3 gravity = glm::vec3(0.0f,-9.81f,0.0f);

float cr_coefficient_b = 1.0f;
float friction_coefficient_b = 1.0f;


#ifdef USE_XPBF

///////// XPBF specific /////////////

float epsilon_lambda = 0.1; // 0 - 1 //relaxation parameter
float epsilon_vorticity = 0.1;// 0 - 1 //vorticity parameter
float c_viscosity = 0.01;
float delta_q = 0.2; //0.1 - 0.3
float poly6_delta_q;

std::vector<glm::vec3> delta_position_buffer;

////////////////////////////////////

#elifdef USE_VISCOEL

float k_gas_constant;
float k_near_gas_constant;
float viscosity_linear;
float viscosity_quadratic;
#else

float k_gas_constant;
float k_near_gas_constant;
float viscosity;
float l_tension;
float surface_tension;

#endif


float timestep = 1.0f/60.0f;
float subtimestep;

float friction_multiplier = 15.0f;

uint32_t substep_num;

uint32_t solver_iterations;

uint32_t particles_number;

uint32_t total_grid_cell_count;

Boundary fluid_boundary;
Shader dummy_b_shader;

std::map<std::string,std::shared_ptr<SamplingObject>> ObjectsVector;
std::vector<fluid_particle> fluid_particles;
std::vector<unsigned int> particleIndexBuffer;
std::vector<unsigned int> cellIndexBuffer;
std::vector<unsigned int> cellOffsetBuffer;


std::vector<ParticleGpu> gpuParticleBuffer;

std::vector<glm::vec3> color_gradient_vector;

std::vector<std::vector<unsigned int>> neighborParticlesIndexBuffer;

simulation_param_conf copy_param_conf;

boost::mutex Baundary_model_mutex;
boost::mutex GPU_buffer_mutex;
boost::mutex change_param_mutex;
boost::mutex solver_func_mutex;

//boost::mutex Ball_model_mutex;

std::atomic<bool> solveAbortCondition{false};


private:


};




}
