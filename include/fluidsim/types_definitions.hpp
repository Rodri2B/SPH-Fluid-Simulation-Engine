#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <object_loader.hpp>
#include <Eigen/Dense>
#include <vector>
#include <memory>

#define FLUID_PI 3.14159265359F

//#define USE_VISCOEL

//#define USE_XPBF

namespace FluidSim {


    typedef cube Boundary;

    namespace SamplingShapes {
        extern float Cube_vertices[192];
        extern unsigned int Cube_indices[36];
        extern float Plane_vertices[12];
        extern unsigned int Plane_indices[6];
        void Generate_sphere_mesh(const unsigned int &X_SEGMENTS, const unsigned int &Y_SEGMENTS,std::vector<float> &data,std::vector<unsigned int> &indices);
    }

    struct ParticleGpu {
        float px, py, pz, vx, vy, vz, density;
        float is_boundary;
    };

    struct particle_boundary_sector{

        particle_boundary_sector(const uint32_t &b_offset,const uint32_t &b_size){
            boundary_offset = b_offset;
            boundary_size = b_size;
        }
        uint32_t boundary_offset;
        uint32_t boundary_size;
    };

    struct SamplingObject{

        std::shared_ptr<Object> object;

        bool sample_surface;
        bool sample_volume;
        bool uniform_volume_sampling;

        std::vector<particle_boundary_sector> particle_boundaries;

    };


    struct baundary_config{
        glm::vec3 b_position;
        glm::vec3 b_rotation;
        glm::vec3 b_size;
    };

    struct fluid_particle{
        glm::vec3 position;
        glm::vec3 old_position;
        #ifdef USE_VISCOEL
        glm::vec3 position_temp;
        #endif
        glm::vec3 velocity;

        #ifdef USE_XPBF
        glm::vec3 vorticity;
        #else
        glm::vec3 old_velocity;
        #endif

        #ifdef USE_XPBF
        float lambda;
        #else
        float pressure;
        float near_pressure;
        float near_density;
        #endif

        float density;
        bool is_boundary;
    };

    using VerticesMap = Eigen::Map<
        Eigen::Matrix<float, 3, Eigen::Dynamic>,
        0,
        Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic>
    >;

    using ConstVerticesMap = Eigen::Map<
        const Eigen::Matrix<float, 3, Eigen::Dynamic>,
        0,
        Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic>
    >;

    using IndicesMap =
        Eigen::Map<Eigen::Matrix<unsigned int, 3, Eigen::Dynamic>>;


    struct simulation_param_conf{

        glm::vec3 gravity;
        float cr_coefficient_b;
        float friction_coefficient_b;
        float friction_multiplier;

        #ifdef USE_XPBF
        float epsilon_lambda;
        float epsilon_vorticity;
        float c_viscosity;
        float delta_q;

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

        float timestep;

        float rest_density;

        uint32_t substep_num;
        uint32_t solver_iterations;

    };

    struct solver_init_conf{

        baundary_config b_conf;
        float fluid_h_conf;
        float particle_spacing_conf; 
        float particle_radius_conf;
        float rest_density_conf;
        float fluid_Rv_conf;

    };

}