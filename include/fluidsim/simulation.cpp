
#include "solver.hpp"








void FluidSim::SPHsolver::ChangeSimulationParm(const simulation_param_conf &param_conf){

    change_param_mutex.lock();

    copy_param_conf.cr_coefficient_b = param_conf.cr_coefficient_b;
    copy_param_conf.friction_coefficient_b = param_conf.friction_coefficient_b;
    copy_param_conf.friction_multiplier = param_conf.friction_multiplier;

    #ifdef USE_XPBF
    copy_param_conf.c_viscosity = param_conf.c_viscosity;
    copy_param_conf.delta_q = param_conf.delta_q;
    copy_param_conf.epsilon_lambda = param_conf.epsilon_lambda;
    copy_param_conf.epsilon_vorticity = param_conf.epsilon_vorticity;

    #elifdef USE_VISCOEL

    k_gas_constant        = param_conf.k_gas_constant     ;
    k_near_gas_constant   = param_conf.k_near_gas_constant;
    viscosity_linear      = param_conf.viscosity_linear   ;
    viscosity_quadratic   = param_conf.viscosity_quadratic;

    #else

    copy_param_conf.k_gas_constant = param_conf.k_gas_constant;
    copy_param_conf.k_near_gas_constant = param_conf.k_near_gas_constant;
    copy_param_conf.viscosity = param_conf.viscosity;
    copy_param_conf.l_tension = param_conf.l_tension;
    copy_param_conf.surface_tension = param_conf.surface_tension;

    #endif
    copy_param_conf.rest_density = param_conf.rest_density;
    copy_param_conf.gravity = param_conf.gravity;
    copy_param_conf.timestep = param_conf.timestep;
    copy_param_conf.substep_num = param_conf.substep_num;
    copy_param_conf.solver_iterations = param_conf.solver_iterations;

    change_param_mutex.unlock();

}











void FluidSim::SPHsolver::UploadSimulationParm(){

    change_param_mutex.lock();

    cr_coefficient_b = copy_param_conf.cr_coefficient_b;
    friction_coefficient_b = copy_param_conf.friction_coefficient_b;
    friction_multiplier = copy_param_conf.friction_multiplier;

    #ifdef USE_XPBF
    c_viscosity = copy_param_conf.c_viscosity;
    delta_q = copy_param_conf.delta_q;
    epsilon_lambda = copy_param_conf.epsilon_lambda;
    epsilon_vorticity = copy_param_conf.epsilon_vorticity;
    glm::vec3 deltaq = glm::vec3(fluid_h*copy_param_conf.delta_q,0.0f,0.0f);
    poly6_delta_q = poly6_kernel(deltaq);

    #elifdef USE_VISCOEL

    k_gas_constant        = copy_param_conf.k_gas_constant     ;
    k_near_gas_constant   = copy_param_conf.k_near_gas_constant;
    viscosity_linear      = copy_param_conf.viscosity_linear   ;
    viscosity_quadratic   = copy_param_conf.viscosity_quadratic;
    #else

    k_gas_constant =  copy_param_conf.k_gas_constant;
    k_near_gas_constant =  copy_param_conf.k_near_gas_constant;
    viscosity =       copy_param_conf.viscosity;
    l_tension =       copy_param_conf.l_tension;
    surface_tension = copy_param_conf.surface_tension;

    #endif

    //particle_mass = (particle_mass/rest_density)*copy_param_conf.rest_density;

    //particle_mass = 1.0f;

    rest_density = copy_param_conf.rest_density;

    float n_p = 1.0f/particle_spacing+1.0f;
    particle_mass = copy_param_conf.rest_density/(n_p*n_p*n_p)*1.35f;

    //particle_mass = (rest_density)/24000.0f;
    gravity = copy_param_conf.gravity;
    timestep = copy_param_conf.timestep;
    substep_num = copy_param_conf.substep_num;
    solver_iterations = copy_param_conf.solver_iterations;

    change_param_mutex.unlock();

    subtimestep = timestep/static_cast<float>(substep_num);

}













void FluidSim::SPHsolver::ResetSim()
{   

    if(!solver_func_mutex.try_lock()){

        // abort solve
        solveAbortCondition.store(true);

        solver_func_mutex.lock();

        solveAbortCondition.store(false);
    }

    //GPU_buffer_mutex.lock();
    //change_param_mutex.lock();

    fluid_particles.clear();
    color_gradient_vector.clear();
    particleIndexBuffer.clear();
    cellIndexBuffer.clear();
    cellOffsetBuffer.clear();
    neighborParticlesIndexBuffer.clear();
    InitParticles();
    
    particles_number = fluid_particles.size();
    color_gradient_vector.resize(particles_number);

    particleIndexBuffer.resize(particles_number);

    #ifdef USE_XPBF
    delta_position_buffer.clear();
    delta_position_buffer.resize(particles_number);
    #endif

    total_grid_cell_count = nextPowerOfTwo(2*particles_number); //or 4*

    cellIndexBuffer.resize(particles_number);

    cellOffsetBuffer.resize(total_grid_cell_count);

    neighborParticlesIndexBuffer.resize(particles_number);

    #pragma omp parallel for schedule(static)
    for (uint32_t i = 0; i < particles_number; ++i) particleIndexBuffer[i] = i;
    
    
    // get mutex
    GPU_buffer_mutex.lock();

    gpuParticleBuffer.clear();
    gpuParticleBuffer.resize(particles_number);

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < (int)fluid_particles.size(); ++i)
    {
        const fluid_particle& p = fluid_particles[i];

        gpuParticleBuffer[i] = ParticleGpu{
            p.position.x, p.position.y, p.position.z,
            p.velocity.x,p.velocity.y,p.velocity.z,
            p.density,
            p.is_boundary ? 1.0f : 0.0f
        };
    }
    // free mutex
    GPU_buffer_mutex.unlock();
    //GPU_buffer_mutex.unlock();
    //change_param_mutex.unlock();

    solver_func_mutex.unlock();
}












void FluidSim::SPHsolver::find_neighbors(const uint32_t &particleIndex,const glm::int3 &cellIndex,const fluid_particle& particle,std::vector<uint32_t> &neighborParticleIndexBuffer){

        for (int i = -1; i <= 1; ++i)
        for (int j = -1; j <= 1; ++j)
        for (int k = -1; k <= 1; ++k)
        {
            glm::int3 neighborIndex = cellIndex + glm::int3(i, j, k);
            uint32_t flatNeighborIndex = GetFlatCellIndex(neighborIndex);

            uint32_t it = cellOffsetBuffer[flatNeighborIndex];

            while (it != UINT32_MAX && it < particles_number)
            {
                uint32_t particleIndexB = particleIndexBuffer[it];

                if (cellIndexBuffer[particleIndexB] != flatNeighborIndex)
                    break;

                if(particleIndexB != particleIndex){
                    // SPH evaluation here
                    const fluid_particle& particleB = fluid_particles[particleIndexB];
                    glm::vec3 distance_vec = particle.position-particleB.position;

                    if(glm::dot(distance_vec,distance_vec) <= fluid_h*fluid_h){
                        neighborParticleIndexBuffer.push_back(particleIndexB);
                    }
                }


                ++it;
            }
        }
}










void FluidSim::SPHsolver::format_particle_buffer(){
    // get mutex
    GPU_buffer_mutex.lock();

    //std::transform(std::execution::par,
    //               fluid_particles.begin(), fluid_particles.end(),
    //               gpuParticleBuffer.begin(),
    //               [](const fluid_particle& p) {
    //                   return ParticleGpu{
    //                       p.position.x, p.position.y, p.position.z,
    //                       p.density, p.is_boundary ? 1.0f : 0.0f
    //                   };
    //               });

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < (int)fluid_particles.size(); ++i)
    {
        const fluid_particle& p = fluid_particles[i];

        gpuParticleBuffer[i] = ParticleGpu{
            p.position.x, p.position.y, p.position.z,
            p.velocity.x,p.velocity.y,p.velocity.z,
            p.density,
            p.is_boundary ? 1.0f : 0.0f
        };
    }
    // free mutex
    GPU_buffer_mutex.unlock();
}