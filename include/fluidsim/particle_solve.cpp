
#include "solver.hpp"

void FluidSim::SPHsolver::solve_test(float &time_window){


    solver_func_mutex.lock();

    UploadSimulationParm();
    
    time_window = timestep;

    const float inv_h = 1.0f / fluid_h;
    const float inv_rest_density = 1.0f / rest_density;
    const float inv_subtimestep = 1.0f / subtimestep;

    float k2 = 1e-6f;
    float k3 = 2.5f*k2*particle_spacing;
    

    for(uint32_t substep = 0;substep < substep_num;++substep)
    {

        CreateUnsortedList();
        SortList();
        BuildOffsetList();

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];


            glm::int3 cellIndex;
            cellIndex.x = (int)std::floor(particle.position.x * inv_h);
            cellIndex.y = (int)std::floor(particle.position.y * inv_h);
            cellIndex.z = (int)std::floor(particle.position.z * inv_h);

            std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
            // IMPORTANT: we reuse this buffer every step.
            // If we don't clear it, we'll keep stale neighbors forever and can even include self,
            // which causes r==0 in spiky_kernel_gradient -> NaNs.
            neighborParticleIndexBuffer.clear();
            neighborParticleIndexBuffer.reserve(64);

            find_neighbors(particleIndex,cellIndex,particle,neighborParticleIndexBuffer);

        }

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
            uint32_t neighbors_count = neighborParticleIndexBuffer.size();
            // solve SPH

            //calculate internal forces
            glm::vec3 internal_f(0.0f);
            for(uint32_t i = 0; i < neighbors_count;++i){
                fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
                glm::vec3 distance_vec = (particle.old_position-particleB.old_position);
                float distance = glm::length(distance_vec);
                if(distance < 1e-9f)distance = 1e-9f;
                internal_f += distance_vec*((k3-k2*distance)/(distance*distance*distance*distance));
            }

            //apply  external forces
            //particle.velocity += subtimestep*((-glm::length(gravity)*glm::normalize(particle.old_position)) + (internal_f/particle_mass));
            particle.velocity += subtimestep*(gravity + (internal_f/particle_mass));
            particle.position += subtimestep*particle.velocity;

        }

        // get mutex
        Baundary_model_mutex.lock();
        glm::mat3 boundary_R = glm::mat3(fluid_boundary.model_m);
        glm::mat3 boundary_inverse_R = glm::inverse(boundary_R);
        glm::vec3 boundary_translation = glm::vec3(fluid_boundary.model_m[3]);

        // free mutex
        Baundary_model_mutex.unlock();

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            //detect collision and response
            glm::vec3 transformed_position = boundary_inverse_R*(particle.position - boundary_translation);

            float penetration_depth;

            if(transformed_position.x > 0.5f){
                penetration_depth = transformed_position.x - 0.5f;
                glm::vec3 normal_local(-1.0f,0.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }else if(transformed_position.x < -0.5f){
                penetration_depth = -transformed_position.x - 0.5f;
                glm::vec3 normal_local(1.0f,0.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }

            if(transformed_position.y > 0.5f){
                penetration_depth = transformed_position.y - 0.5f;
                glm::vec3 normal_local(0.0f,-1.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }else if(transformed_position.y < -0.5f){
                penetration_depth = -transformed_position.y - 0.5f;
                glm::vec3 normal_local(0.0f,1.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }

            if(transformed_position.z > 0.5f){
                penetration_depth = transformed_position.z - 0.5f;
                glm::vec3 normal_local(0.0f,0.0f,-1.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }else if(transformed_position.z < -0.5f){
                penetration_depth = -transformed_position.z - 0.5f;
                glm::vec3 normal_local(0.0f,0.0f,1.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }

            particle.old_position = particle.position;

        }
    
    }

    format_particle_buffer();

    //std::cout<<"solve end\n";
    solver_func_mutex.unlock();
    
}








void FluidSim::SPHsolver::Kens_solve(float &time_window){

    solver_func_mutex.lock();

    UploadSimulationParm();

    time_window = timestep;

    const float smoothingRadius = fluid_h;
    const float sqrSmoothingRadius = smoothingRadius * smoothingRadius;
    const float inv_h = 1.0f / smoothingRadius;
    // Keep the same prediction horizon used by the HLSL kernel.
    const float inv_subtimestep = 1.0f / subtimestep;
    const float eps = 1e-20f;

    const float h2 = smoothingRadius * smoothingRadius;
    const float h4 = h2*h2;
    const float h5 = std::pow(smoothingRadius, 5.0f);
    const float h6 = h5 * smoothingRadius;
    //const float h7 = h6 * smoothingRadius;
    const float h9 = std::pow(smoothingRadius, 9.0f);
    //const float h11 = std::pow(smoothingRadius, 11.0f);

    const float kSpikyPow2 = 15.0f / (2.0f * FLUID_PI * h5);
    const float kSpikyPow3 = 15.0f / (FLUID_PI * h6);
    //const float kSpikyPow4 = 105.0f / (4.0f*FLUID_PI * h7);
    const float kSpikyPow2Grad = 15.0f / (FLUID_PI * h5);
    const float kSpikyPow3Grad = 45.0f / (FLUID_PI * h6);
    //const float kSpikyPow4Grad = 105.0f / (FLUID_PI * h7);
    const float poly6Scale = 315.0f / (64.0f * FLUID_PI * h9);//
    //const float poly6PScale = 3465.0f / (512.0f * FLUID_PI * h11);//

    const float poly6ScaleLap = (6.0f*315.0f) / (64.0f * FLUID_PI * h9);
    const float poly6ScaleGrad = poly6ScaleLap;


    auto signNotZero = [](const float &v) -> float {
        return (v < 0.0f) ? -1.0f : 1.0f;
    };

///////////////////
    auto densityKernel = [&](const float &dst) -> float {
        //if (dst < smoothingRadius) {
            const float v = smoothingRadius - dst;
            return v * v * kSpikyPow2;
        //}
        //return 0.0f;
    };

    auto nearDensityKernel = [&](const float &dst) -> float {
        //if (dst < smoothingRadius) {
            const float v = smoothingRadius - dst;
            return v * v * v * kSpikyPow3;
        //}
        //return 0.0f;
    };

    auto densityDerivative = [&](const float &dst) -> float {
        //if (dst <= smoothingRadius) {
            const float v = smoothingRadius - dst;
            return -v * kSpikyPow2Grad;
        //}
        //return 0.0f;
    };

    auto nearDensityDerivative = [&](const float &dst) -> float {
        //if (dst <= smoothingRadius) {
            const float v = smoothingRadius - dst;
            return -v * v * kSpikyPow3Grad;
        //}
        //return 0.0f;
    };
//////////
    //auto densityKernel = [&](float dst) -> float {
    //    if (dst < smoothingRadius) {
    //        const float v = smoothingRadius - dst;
    //        return v * v * v * kSpikyPow3;
    //    }
    //    return 0.0f;
    //};

    //auto nearDensityKernel = [&](float dst) -> float {
    //    if (dst < smoothingRadius) {
    //        const float v = smoothingRadius - dst;
    //        return v * v * v * v * kSpikyPow4;
    //    }
    //    return 0.0f;
    //};

    //auto densityKernel = [&](float dst) -> float {
    //    if (dst < smoothingRadius) {
    //        const float v = h2 - dst * dst;
    //        return v * v * v * poly6Scale;
    //    }
    //    return 0.0f;
    //};

    //auto nearDensityKernel = [&](float dst) -> float {
    //    if (dst < smoothingRadius) {
    //        const float v = h2 - dst * dst;
    //        return v * v * v * v * poly6PScale;
    //    }
    //    return 0.0f;
    //};

    //auto densityDerivative = [&](float dst) -> float {
    //    if (dst <= smoothingRadius) {
    //        const float v = smoothingRadius - dst;
    //        return -v * v * kSpikyPow3Grad;
    //    }
    //    return 0.0f;
    //};

    //auto nearDensityDerivative = [&](float dst) -> float {
    //    if (dst <= smoothingRadius) {
    //        const float v = smoothingRadius - dst;
    //        return -v * v * v * kSpikyPow4Grad;
    //    }
    //    return 0.0f;
    //};

    //auto poly6Kernel = [&](const float &dst) -> float {
    //    //if (dst < smoothingRadius) {
    //        const float v = h2 - dst * dst;
    //        return v * v * v * poly6Scale;
    //    //}
    //    return 0.0f;
    //};

    auto poly6KernelDerivate = [&](const float &dst) -> float {
        //if (dst < smoothingRadius) {
            const float v = h2 - dst * dst;
            return -poly6ScaleGrad * v * v;
        //}
        return 0.0f;
    };

    auto poly6KernelLaplacian = [&](const float &dst) -> float {
        //if (dst < smoothingRadius) {
            const float r2 = dst * dst;
            const float diff = 3.0f*h4 -10.0f*h2*r2+7.0f*r2*r2;
            return -poly6ScaleLap * diff;
        //}
        //return 0.0f;
    };

    auto ViscosityKernel = [&](const float &dst) -> float {
        //if (dst < smoothingRadius) {
            return (smoothingRadius - dst) * kSpikyPow3Grad;
        //}
        //return 0.0f;
    };

    //auto pressureFromDensity = [&](float density) -> float {
    //    return (density - rest_density) * k_gas_constant;
    //};

    //auto nearPressureFromDensity = [&](float nearDensity) -> float {
    //    return nearDensity * k_near_gas_constant;
    //};

    for(uint32_t substep = 0; substep < substep_num; ++substep)
    {
        // ExternalForces kernel
        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            const uint32_t particleIndex = particleIndexBuffer[i];
            fluid_particle& particle = fluid_particles[particleIndex];

            particle.old_position = particle.position;

            //apply  external forces

            //particle.position += particle.velocity * subtimestep;

            particle.velocity += gravity * subtimestep;
            particle.old_velocity = particle.velocity;
            particle.position += particle.velocity * subtimestep;
        }

        // UpdateSpatialHash + Reorder kernels
        CreateUnsortedList();
        SortList();
        BuildOffsetList();
///////////////////////////////////////////////////////////////
/*
        // Read boundary transform once for this substep.
        Baundary_model_mutex.lock();
        glm::mat3 boundary_R = glm::mat3(fluid_boundary.model_m);
        boundary_R[0] = glm::normalize(boundary_R[0]);
        boundary_R[1] = glm::normalize(boundary_R[1]);
        boundary_R[2] = glm::normalize(boundary_R[2]);
        const glm::mat3 boundary_inverse_R = glm::transpose(boundary_R);
        const glm::vec3 boundary_translation = glm::vec3(fluid_boundary.model_m[3]);
        const glm::vec3 boundary_scale = fluid_boundary.Get_scale();
        Baundary_model_mutex.unlock();

        //Ball_model_mutex.lock();
        glm::vec3 Ball_position = ObjectsVector["Ball"]->object->Get_position();
        float Ball_Radius = std::static_pointer_cast<sphere>(ObjectsVector["Ball"]->object)->getRadius();
        float Ball_Radius2 = Ball_Radius*Ball_Radius;
        //std::cout << Ball_position.x <<","<< Ball_position.y <<","<< Ball_position.z <<"\n";
        //Ball_model_mutex.unlock();
*/
//////////////////////////////////////////////////////////////////////
        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        // CalculateDensities kernel
        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            const uint32_t particleIndex = particleIndexBuffer[i];
            fluid_particle& particle = fluid_particles[particleIndex];

            const glm::vec3 pos = particle.position;

            float particle_density = densityKernel(0.0f);
            float particle_near_density = nearDensityKernel(0.0f);
////////////////////////////////////
            glm::int3 cellIndex;
            cellIndex.x = (int)std::floor(pos.x * inv_h);
            cellIndex.y = (int)std::floor(pos.y * inv_h);
            cellIndex.z = (int)std::floor(pos.z * inv_h);

////////////////////////////

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
                        glm::vec3 distance_vec = particleB.position-pos;

                        const float sqrDst = glm::dot(distance_vec,distance_vec);

                        if(sqrDst < h2){
                            
                            const float dst = std::sqrt(sqrDst);
                            particle_density += densityKernel(dst);
                            particle_near_density += nearDensityKernel(dst);
                        }
                    }


                    ++it;
                }
            }
///////////////////

/*
// density
            glm::vec3 posLocal = boundary_inverse_R * (pos - boundary_translation);

            const glm::vec3 halfSize = 0.5f*boundary_scale;
            const glm::vec3 edgeDst(
                halfSize.x - std::abs(posLocal.x),
                halfSize.y - std::abs(posLocal.y),
                halfSize.z - std::abs(posLocal.z)
            );
            

            if (edgeDst.x >= -FLT_MAX && edgeDst.x < fluid_h) {
                
                const float dst = 2.0f*(edgeDst.x);
                particle_density += densityKernel(dst);
                particle_near_density += nearDensityKernel(dst);
                
            }
            if (edgeDst.y >= -FLT_MAX && edgeDst.y < fluid_h) {
                
                const float dst = 2.0f*(edgeDst.y);
                particle_density += densityKernel(dst);
                particle_near_density += nearDensityKernel(dst);
            }
            if (edgeDst.z >= -FLT_MAX && edgeDst.z < fluid_h) {
                
                const float dst = 2.0f*(edgeDst.z);
                particle_density += densityKernel(dst);
                particle_near_density += nearDensityKernel(dst);                
            }

/////////////////////////////////////////////////
*/
            particle_density *= particle_mass;
            particle_near_density *= particle_mass;

            //calculate pressure
            particle.pressure = k_gas_constant*(particle_density-rest_density);
            particle.near_pressure = k_near_gas_constant*(particle_near_density);

            //store density
            particle.density = particle_density;
            particle.near_density = particle_near_density;


        }

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        // CalculatePressureForce kernel
        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            const uint32_t particleIndex = particleIndexBuffer[i];
            fluid_particle& particle = fluid_particles[particleIndex];
            const glm::vec3 pos = particle.position;

            const float pressure = particle.pressure;
            const float nearPressure = particle.near_pressure;

            uint32_t neighbourCount=0;

            glm::vec3 f_pressure(0.0f);

            glm::vec3 color_gradient(0.0f);
////////////////////////////////////
            glm::int3 cellIndex;
            cellIndex.x = (int)std::floor(pos.x * inv_h);
            cellIndex.y = (int)std::floor(pos.y * inv_h);
            cellIndex.z = (int)std::floor(pos.z * inv_h);

////////////////////////////

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
                        glm::vec3 distance_vec = particleB.position-pos;

                        const float sqrDst = glm::dot(distance_vec,distance_vec);

                        if(sqrDst < h2){
                            
                            const float dst = std::sqrt(sqrDst);
                            const glm::vec3 dir = (dst > eps) ? (distance_vec / dst) : glm::vec3(0.0f, 1.0f, 0.0f);

                            const float densityNeighbour = std::max(particleB.density, eps);
                            const float nearDensityNeighbour = std::max(particleB.near_density, eps);
                            const float sharedPressure = 0.5f * (pressure + particleB.pressure);
                            const float sharedNearPressure = 0.5f * (nearPressure + particleB.near_pressure);

                            f_pressure += dir * densityDerivative(dst) * sharedPressure / densityNeighbour;
                            f_pressure += dir * nearDensityDerivative(dst) * sharedNearPressure / nearDensityNeighbour;
                            neighbourCount++;

                            
                            if(surface_tension > 0.0f)color_gradient -= (particle_mass/particleB.density)*poly6KernelDerivate(dst)*distance_vec;

                        
                        }
                    }


                    ++it;
                }
            }
///////////////////

            if(surface_tension > 0.0f) color_gradient_vector[particleIndex] = color_gradient;
/*
//pressure


            glm::vec3 posLocal = boundary_inverse_R * (pos - boundary_translation);

            const glm::vec3 halfSize = 0.5f*boundary_scale;
            const glm::vec3 edgeDst(
                halfSize.x - std::abs(posLocal.x),
                halfSize.y - std::abs(posLocal.y),
                halfSize.z - std::abs(posLocal.z)
            );
            
            if (edgeDst.x >= -FLT_MAX && edgeDst.x < fluid_h) {

                glm::vec3 dir = boundary_R*glm::vec3(signNotZero(posLocal.x),0.0f,0.0f);
                
                const float dst = 2.0f*edgeDst.x;

                const float densityNeighbour = std::max(particle.density, eps);
                const float nearDensityNeighbour = std::max(particle.near_density, eps);
                const float sharedPressure = pressure;
                const float sharedNearPressure = nearPressure;

                f_pressure += dir * densityDerivative(dst) * sharedPressure / densityNeighbour;
                f_pressure += dir * nearDensityDerivative(dst) * sharedNearPressure / nearDensityNeighbour;
                neighbourCount++;

                
                //if(surface_tension > 0.0f)color_gradient -= (particle_mass/particle.density)*poly6KernelDerivate(dst)*dir*dst;
                
            }
            if (edgeDst.y >= -FLT_MAX && edgeDst.y < fluid_h) {
                glm::vec3 dir = boundary_R*glm::vec3(0.0f,signNotZero(posLocal.y),0.0f);
                
                const float dst = 2.0f*edgeDst.y;

                const float densityNeighbour = std::max(particle.density, eps);
                const float nearDensityNeighbour = std::max(particle.near_density, eps);
                const float sharedPressure = pressure;
                const float sharedNearPressure = nearPressure;

                f_pressure += dir * densityDerivative(dst) * sharedPressure / densityNeighbour;
                f_pressure += dir * nearDensityDerivative(dst) * sharedNearPressure / nearDensityNeighbour;
                neighbourCount++;

                
                //if(surface_tension > 0.0f)color_gradient -= (particle_mass/particle.density)*poly6KernelDerivate(dst)*dir*dst;
            }
            if (edgeDst.z >= -FLT_MAX && edgeDst.z < fluid_h) {
                glm::vec3 dir = boundary_R*glm::vec3(0.0f,0.0f,signNotZero(posLocal.z));
                
                const float dst = 2.0f*edgeDst.z;

                const float densityNeighbour = std::max(particle.density, eps);
                const float nearDensityNeighbour = std::max(particle.near_density, eps);
                const float sharedPressure = pressure;
                const float sharedNearPressure = nearPressure;

                f_pressure += dir * densityDerivative(dst) * sharedPressure / densityNeighbour;
                f_pressure += dir * nearDensityDerivative(dst) * sharedNearPressure / nearDensityNeighbour;
                neighbourCount++;

                
                //if(surface_tension > 0.0f)color_gradient -= (particle_mass/particle.density)*poly6KernelDerivate(dst)*dir*dst;             
            }


////////////////////////////////////
*/
            f_pressure *= particle_mass;
            const glm::vec3 acceleration = f_pressure / std::max(particle.density, eps);
            particle.velocity += acceleration * subtimestep;

            // Keep the same low-neighbour drag from the shader path.
            if (neighbourCount < 8) {
                particle.velocity -= particle.velocity * subtimestep * 0.75f/particle_mass;
            }
        }

        //std::cout<<particle_mass<<"\n";

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i) {
            const uint32_t particleIndex = particleIndexBuffer[i];
            fluid_particles[particleIndex].old_velocity = fluid_particles[particleIndex].velocity;
        }

        // CalculateViscosity kernel
        if(viscosity > 0.0f){
        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            const uint32_t particleIndex = particleIndexBuffer[i];
            fluid_particle& particle = fluid_particles[particleIndex];

            const glm::vec3 pos = particle.position;
            const glm::vec3 velocity = particle.old_velocity;

            glm::vec3 f_viscosity(0.0f);

////////////////////////////////////
            glm::int3 cellIndex;
            cellIndex.x = (int)std::floor(pos.x * inv_h);
            cellIndex.y = (int)std::floor(pos.y * inv_h);
            cellIndex.z = (int)std::floor(pos.z * inv_h);

////////////////////////////

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
                        glm::vec3 distance_vec = particleB.position-pos;

                        const float sqrDst = glm::dot(distance_vec,distance_vec);

                        if(sqrDst < h2){
                            
                            const float dst = std::sqrt(sqrDst);
                            f_viscosity += ((particleB.old_velocity - velocity)/particleB.density) * ViscosityKernel(dst);

                        }
                    }


                    ++it;
                }
            }
/*
///////////////////

            //viscosity


            glm::vec3 posLocal = boundary_inverse_R * (pos - boundary_translation);
            glm::vec3 velocityLocal = boundary_inverse_R * velocity;

            const glm::vec3 halfSize = 0.5f*boundary_scale;
            const glm::vec3 edgeDst(
                halfSize.x - std::abs(posLocal.x),
                halfSize.y - std::abs(posLocal.y),
                halfSize.z - std::abs(posLocal.z)
            );
            
            if (edgeDst.x >= -FLT_MAX && edgeDst.x < fluid_h) {

                glm::vec3 normal_local(-signNotZero(posLocal.x),0.0f,0.0f);
                glm::vec3 velocityLocal_normal = (glm::dot(velocityLocal,normal_local))*normal_local;

                glm::vec3 tan = velocityLocal-velocityLocal_normal;
                glm::vec3 delta_tan;
                glm::vec3 tan_abs = glm::abs(tan);
                delta_tan.x = std::clamp(friction_coefficient_b*(tan.x)*subtimestep*friction_multiplier,-tan_abs.x,tan_abs.x);
                delta_tan.y = std::clamp(friction_coefficient_b*(tan.y)*subtimestep*friction_multiplier,-tan_abs.y,tan_abs.y);
                delta_tan.z = std::clamp(friction_coefficient_b*(tan.z)*subtimestep*friction_multiplier,-tan_abs.z,tan_abs.z);

                glm::vec3 mirrored_velLocal = -velocityLocal_normal + tan -2.0f*delta_tan; 
                
                const float dst = 2.0f*edgeDst.x;
                f_viscosity += ((boundary_R*mirrored_velLocal - velocity)/particle.density) * ViscosityKernel(dst);
                
            }
            if (edgeDst.y >= -FLT_MAX && edgeDst.y < fluid_h) {
                glm::vec3 normal_local(0.0f,-signNotZero(posLocal.y),0.0f);
                glm::vec3 velocityLocal_normal = (glm::dot(velocityLocal,normal_local))*normal_local;

                glm::vec3 tan = velocityLocal-velocityLocal_normal;
                glm::vec3 delta_tan;
                glm::vec3 tan_abs = glm::abs(tan);
                delta_tan.x = std::clamp(friction_coefficient_b*(tan.x)*subtimestep*friction_multiplier,-tan_abs.x,tan_abs.x);
                delta_tan.y = std::clamp(friction_coefficient_b*(tan.y)*subtimestep*friction_multiplier,-tan_abs.y,tan_abs.y);
                delta_tan.z = std::clamp(friction_coefficient_b*(tan.z)*subtimestep*friction_multiplier,-tan_abs.z,tan_abs.z);

                glm::vec3 mirrored_velLocal = -velocityLocal_normal + tan -2.0f*delta_tan; 
                
                const float dst = 2.0f*edgeDst.y;
                f_viscosity += ((boundary_R*mirrored_velLocal - velocity)/particle.density) * ViscosityKernel(dst);
            }
            if (edgeDst.z >= -FLT_MAX && edgeDst.z < fluid_h) {
                glm::vec3 normal_local(0.0f,0.0f,-signNotZero(posLocal.z));
                glm::vec3 velocityLocal_normal = (glm::dot(velocityLocal,normal_local))*normal_local;

                glm::vec3 tan = velocityLocal-velocityLocal_normal;
                glm::vec3 delta_tan;
                glm::vec3 tan_abs = glm::abs(tan);
                delta_tan.x = std::clamp(friction_coefficient_b*(tan.x)*subtimestep*friction_multiplier,-tan_abs.x,tan_abs.x);
                delta_tan.y = std::clamp(friction_coefficient_b*(tan.y)*subtimestep*friction_multiplier,-tan_abs.y,tan_abs.y);
                delta_tan.z = std::clamp(friction_coefficient_b*(tan.z)*subtimestep*friction_multiplier,-tan_abs.z,tan_abs.z);

                glm::vec3 mirrored_velLocal = -velocityLocal_normal + tan -2.0f*delta_tan; 
                
                const float dst = 2.0f*edgeDst.z;
                f_viscosity += ((boundary_R*mirrored_velLocal - velocity)/particle.density) * ViscosityKernel(dst);               
            }

            ///////////////////////////
*/
            f_viscosity *= particle_mass;
            particle.velocity = velocity + f_viscosity * viscosity * subtimestep;
        }
        }


        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        // CalculateSurface kernel
        if(surface_tension > 0.0f){
        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            const uint32_t particleIndex = particleIndexBuffer[i];
            fluid_particle& particle = fluid_particles[particleIndex];

            const glm::vec3 pos = particle.position;
            const glm::vec3 velocity = particle.velocity;

            glm::vec3 f_surface(0.0f);

////////////////////////////////////
            glm::int3 cellIndex;
            cellIndex.x = (int)std::floor(pos.x * inv_h);
            cellIndex.y = (int)std::floor(pos.y * inv_h);
            cellIndex.z = (int)std::floor(pos.z * inv_h);

////////////////////////////
            glm::vec3 color_gradient = color_gradient_vector[particleIndex];

            float color_gradient_length = glm::length(color_gradient);

            float lapla_t = 0.0f;

            if(color_gradient_length > l_tension){
                
            color_gradient = color_gradient/color_gradient_length;

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
                        glm::vec3 distance_vec = particleB.position-pos;

                        const float sqrDst = glm::dot(distance_vec,distance_vec);

                        if(sqrDst < h2){
                            
                            const float dst = std::sqrt(sqrDst);
                    
                            //float distance = glm::length(distance_vec);

                            lapla_t += (particle_mass/particleB.density)*poly6KernelLaplacian(dst);
                            

                        }
                    }


                    ++it;
                }
            }
/*
            // tension

            glm::vec3 posLocal = boundary_inverse_R * (pos - boundary_translation);

            const glm::vec3 halfSize = 0.5f*boundary_scale;
            const glm::vec3 edgeDst(
                halfSize.x - std::abs(posLocal.x),
                halfSize.y - std::abs(posLocal.y),
                halfSize.z - std::abs(posLocal.z)
            );
            
            if (edgeDst.x >= -FLT_MAX && edgeDst.x < fluid_h) {
                
                const float dst = 2.0f*edgeDst.x;

                lapla_t += (particle_mass/particle.density)*poly6KernelLaplacian(dst);
                
            }
            if (edgeDst.y >= -FLT_MAX && edgeDst.y < fluid_h) {
                const float dst = 2.0f*edgeDst.y;

                lapla_t += (particle_mass/particle.density)*poly6KernelLaplacian(dst);
            }
            if (edgeDst.z >= -FLT_MAX && edgeDst.z < fluid_h) {
                const float dst = 2.0f*edgeDst.z;

                lapla_t += (particle_mass/particle.density)*poly6KernelLaplacian(dst);              
            }


///////////////////
*/      }
            f_surface = -surface_tension*lapla_t*color_gradient;
            particle.velocity = velocity + f_surface * subtimestep;
        }
        }



        // Read boundary transform once for this substep.
        Baundary_model_mutex.lock();
        glm::mat3 boundary_R = glm::mat3(fluid_boundary.model_m);
        boundary_R[0] = glm::normalize(boundary_R[0]);
        boundary_R[1] = glm::normalize(boundary_R[1]);
        boundary_R[2] = glm::normalize(boundary_R[2]);
        const glm::mat3 boundary_inverse_R = glm::transpose(boundary_R);
        const glm::vec3 boundary_translation = glm::vec3(fluid_boundary.model_m[3]);
        const glm::vec3 boundary_scale = fluid_boundary.Get_scale();
        Baundary_model_mutex.unlock();

        //Ball_model_mutex.lock();
        glm::vec3 Ball_position = ObjectsVector["Ball"]->object->Get_position();
        float Ball_Radius = std::static_pointer_cast<sphere>(ObjectsVector["Ball"]->object)->getRadius();
        float Ball_Radius2 = Ball_Radius*Ball_Radius;
        //std::cout << Ball_position.x <<","<< Ball_position.y <<","<< Ball_position.z <<"\n";
        //Ball_model_mutex.unlock();

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        // UpdatePositions kernel + ResolveCollisions helper
        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            const uint32_t particleIndex = particleIndexBuffer[i];
            fluid_particle& particle = fluid_particles[particleIndex];

            glm::vec3 pos = particle.position; //+ particle.velocity * subtimestep;
            glm::vec3 vel = particle.velocity;
            
            //collide with sphere

            glm::vec3 ball_to_p = pos - Ball_position;
            float ball_to_p_dist2 = glm::dot(ball_to_p,ball_to_p);
            if (ball_to_p_dist2 <= Ball_Radius2) {
                float ball_to_p_dist = std::sqrt(ball_to_p_dist2);
                (ball_to_p_dist == 0.0f)?ball_to_p = glm::vec3(0.0f,1.0f,0.0f):ball_to_p = ball_to_p/ball_to_p_dist;

                pos += ball_to_p*(Ball_Radius - ball_to_p_dist + 1e-6f);
                glm::vec3 velocity_normal = (glm::dot(vel,ball_to_p ))*ball_to_p;

                glm::vec3 tan = vel-velocity_normal;
                vel += -(1.0f+cr_coefficient_b + 1e-6f)*velocity_normal;
                glm::vec3 delta_tan;
                glm::vec3 tan_abs = glm::abs(tan);
                delta_tan.x = std::clamp(friction_coefficient_b*(tan.x)*subtimestep*friction_multiplier,-tan_abs.x,tan_abs.x);
                delta_tan.y = std::clamp(friction_coefficient_b*(tan.y)*subtimestep*friction_multiplier,-tan_abs.y,tan_abs.y);
                delta_tan.z = std::clamp(friction_coefficient_b*(tan.z)*subtimestep*friction_multiplier,-tan_abs.z,tan_abs.z);
                vel -= delta_tan;                 
                
            }

            glm::vec3 posLocal = boundary_inverse_R * (pos - boundary_translation);
            glm::vec3 velocityLocal = boundary_inverse_R * vel;

            const glm::vec3 halfSize = 0.5f*boundary_scale;
            const glm::vec3 edgeDst(
                halfSize.x - std::abs(posLocal.x),
                halfSize.y - std::abs(posLocal.y),
                halfSize.z - std::abs(posLocal.z)
            );
            //bool special_condition = false;
            //if (edgeDst.x <= 0.0f && edgeDst.y <= 0.0f && edgeDst.z <= 0.0f){
            //    posLocal.x = (halfSize.x-simple_pos_hash(particleIndex)*1e-2f-1e-6f) * signNotZero(posLocal.x);
            //    posLocal.y = (halfSize.y-simple_pos_hash(particleIndex+1)*1e-2f-1e-6f) * signNotZero(posLocal.y);
            //    posLocal.z = (halfSize.z-simple_pos_hash(particleIndex+2)*1e-2f-1e-6f) * signNotZero(posLocal.z);
            //    special_condition = true;
 
            //}

            if (edgeDst.x <= 0.0f) {
                posLocal.x = (halfSize.x-1e-6f) * signNotZero(posLocal.x);
                glm::vec3 normal_local(-signNotZero(posLocal.x),0.0f,0.0f);
                //glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                //float vn = glm::dot(particle.velocity, normal_world);
                //if (vn < 0.0f)
                //    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
                
                //velocityLocal.x *= -cr_coefficient_b;

                glm::vec3 velocityLocal_normal = (glm::dot(velocityLocal,normal_local))*normal_local;
                //velocityLocal = -cr_coefficient_b*velocityLocal_normal+
                //                (1.0f-friction_coefficient_b)*(velocityLocal-velocityLocal_normal);

                glm::vec3 tan = velocityLocal-velocityLocal_normal;
                velocityLocal += -(1.0f+cr_coefficient_b+1e-6f)*velocityLocal_normal;
                glm::vec3 delta_tan;
                glm::vec3 tan_abs = glm::abs(tan);
                delta_tan.x = std::clamp(friction_coefficient_b*(tan.x)*subtimestep*friction_multiplier,-tan_abs.x,tan_abs.x);
                delta_tan.y = std::clamp(friction_coefficient_b*(tan.y)*subtimestep*friction_multiplier,-tan_abs.y,tan_abs.y);
                delta_tan.z = std::clamp(friction_coefficient_b*(tan.z)*subtimestep*friction_multiplier,-tan_abs.z,tan_abs.z);
                velocityLocal -= delta_tan;                 
                
            }
            if (edgeDst.y <= 0.0f) {
                posLocal.y = (halfSize.y-1e-6f) * signNotZero(posLocal.y);
                glm::vec3 normal_local(0.0f,-signNotZero(posLocal.y),0.0f);
                //glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                //float vn = glm::dot(particle.velocity, normal_world);
                //if (vn < 0.0f)
                //    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
                
                //velocityLocal.y *= -cr_coefficient_b;

                glm::vec3 velocityLocal_normal = (glm::dot(velocityLocal,normal_local))*normal_local;
                //velocityLocal = -cr_coefficient_b*velocityLocal_normal+
                //                (1.0f-friction_coefficient_b)*(velocityLocal-velocityLocal_normal);

                glm::vec3 tan = velocityLocal-velocityLocal_normal;
                velocityLocal += -(1.0f+cr_coefficient_b+1e-6f)*velocityLocal_normal;
                glm::vec3 delta_tan;
                glm::vec3 tan_abs = glm::abs(tan);
                delta_tan.x = std::clamp(friction_coefficient_b*(tan.x)*subtimestep*friction_multiplier,-tan_abs.x,tan_abs.x);
                delta_tan.y = std::clamp(friction_coefficient_b*(tan.y)*subtimestep*friction_multiplier,-tan_abs.y,tan_abs.y);
                delta_tan.z = std::clamp(friction_coefficient_b*(tan.z)*subtimestep*friction_multiplier,-tan_abs.z,tan_abs.z);
                velocityLocal -= delta_tan;  
            }
            if (edgeDst.z <= 0.0f) {
                posLocal.z = (halfSize.z-1e-6f) * signNotZero(posLocal.z);
                glm::vec3 normal_local(0.0f,0.0f,-signNotZero(posLocal.z));
                //glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                //float vn = glm::dot(particle.velocity, normal_world);
                //if (vn < 0.0f)
                //    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
                
                //velocityLocal.z *= -cr_coefficient_b;

                glm::vec3 velocityLocal_normal = (glm::dot(velocityLocal,normal_local))*normal_local;
                //velocityLocal = -cr_coefficient_b*velocityLocal_normal+
                //                (1.0f-friction_coefficient_b)*(velocityLocal-velocityLocal_normal);

                glm::vec3 tan = velocityLocal-velocityLocal_normal;
                velocityLocal += -(1.0f+cr_coefficient_b+1e-6f)*velocityLocal_normal;
                glm::vec3 delta_tan;
                glm::vec3 tan_abs = glm::abs(tan);
                delta_tan.x = std::clamp(friction_coefficient_b*(tan.x)*subtimestep*friction_multiplier,-tan_abs.x,tan_abs.x);
                delta_tan.y = std::clamp(friction_coefficient_b*(tan.y)*subtimestep*friction_multiplier,-tan_abs.y,tan_abs.y);
                delta_tan.z = std::clamp(friction_coefficient_b*(tan.z)*subtimestep*friction_multiplier,-tan_abs.z,tan_abs.z);
                velocityLocal -= delta_tan;                 
            }

            if(edgeDst.x <= 0.0f || edgeDst.y <= 0.0f || edgeDst.z <= 0.0f ){
                pos = boundary_R * posLocal + boundary_translation;
                vel = boundary_R * velocityLocal;
            }

            particle.position = pos;
            particle.velocity = vel;
            particle.old_position = pos;
            particle.old_velocity = vel;

        }
    }

    format_particle_buffer();
    solver_func_mutex.unlock();
}


#ifndef USE_XPBF 
#ifndef USE_VISCOEL
void FluidSim::SPHsolver::PBF_solve(float &time_window){


    solver_func_mutex.lock();

    UploadSimulationParm();
    
    time_window = timestep;

    const float inv_h = 1.0f / fluid_h;
    const float inv_rest_density = 1.0f / rest_density;
    const float inv_subtimestep = 1.0f / subtimestep;
    

    for(uint32_t substep = 0;substep < substep_num;++substep)
    {

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            //apply  external forces
            particle.old_velocity += subtimestep*(gravity);
            particle.old_position += subtimestep*particle.old_velocity;

            particle.velocity = particle.old_velocity;
            particle.position = particle.old_position;


        }

        CreateUnsortedList();
        SortList();
        BuildOffsetList();

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }




        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];


            glm::int3 cellIndex;
            cellIndex.x = (int)std::floor(particle.position.x * inv_h);
            cellIndex.y = (int)std::floor(particle.position.y * inv_h);
            cellIndex.z = (int)std::floor(particle.position.z * inv_h);

            std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
            // IMPORTANT: we reuse this buffer every step.
            // If we don't clear it, we'll keep stale neighbors forever and can even include self,
            // which causes r==0 in spiky_kernel_gradient -> NaNs.
            neighborParticleIndexBuffer.clear();
            neighborParticleIndexBuffer.reserve(64);

            find_neighbors(particleIndex,cellIndex,particle,neighborParticleIndexBuffer);

        }

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
                uint32_t particleIndex = particleIndexBuffer[i];

                fluid_particle& particle = fluid_particles[particleIndex];

                std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
                uint32_t neighbors_count = neighborParticleIndexBuffer.size();

                //calculate density
                calc_density(neighbors_count,neighborParticleIndexBuffer,particle);
                //calculate pressure
                particle.pressure = k_gas_constant*(particle.density-rest_density);

                particle.near_pressure = k_near_gas_constant*(particle.near_density);

        }

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }
        //compute pressure

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
            uint32_t neighbors_count = neighborParticleIndexBuffer.size();
            // solve SPH

            //calculate internal forces
            glm::vec3 internal_f(0.0f);
            glm::vec3 f_pressure(0.0f);

            for(uint32_t i = 0; i < neighbors_count;++i){
                fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
                glm::vec3 distance_vec = (particle.old_position-particleB.old_position);
                //float distance = glm::length(distance_vec);

                f_pressure += particle_mass*
                ((particle.pressure+particleB.pressure)/(2.0f*particleB.density))*
                spiky_kernel_gradient_new(distance_vec);

                f_pressure += particle_mass*
                ((particle.near_pressure+particleB.near_pressure)/(2.0f*particleB.near_density))*
                //((particle.near_pressure/(particle.near_density*particle.near_density))+(particleB.near_pressure)/(particleB.near_density*particleB.near_density))*
                spiky_kernel_gradient_near_d(distance_vec);

            }

            f_pressure = -f_pressure;

            internal_f = f_pressure;

            particle.velocity += subtimestep*((internal_f/particle.density));
        }


        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
            uint32_t neighbors_count = neighborParticleIndexBuffer.size();
            // solve SPH

            //calculate internal forces
            glm::vec3 internal_f(0.0f);
            glm::vec3 f_viscosity(0.0f);

            for(uint32_t i = 0; i < neighbors_count;++i){
                fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
                glm::vec3 distance_vec = (particle.old_position-particleB.old_position);

                //viscosity

                f_viscosity += particle_mass*
                (particleB.velocity-particle.velocity)*(1.f/(particleB.density*particleB.density))*
                poly6_kernel(distance_vec);


            }

            f_viscosity = viscosity*f_viscosity;


            internal_f = f_viscosity;

            particle.velocity += subtimestep*((internal_f/particle.density));

        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
            uint32_t neighbors_count = neighborParticleIndexBuffer.size();
            // solve SPH

            //calculate internal forces
            glm::vec3 internal_f(0.0f);
            glm::vec3 color_gradient(0.0f);

            for(uint32_t i = 0; i < neighbors_count;++i){
                fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
                glm::vec3 distance_vec = (particle.old_position-particleB.old_position);
                //surface tension
                
                color_gradient += (particle_mass/particleB.density)*poly6_kernel_gradient(distance_vec);

            }

            float color_gradient_length = glm::length(color_gradient);

            glm::vec3 f_surface(0.0f);

            if(color_gradient_length > l_tension){
                color_gradient = color_gradient/color_gradient_length;
                float lapla_t = 0.0f;
                for(uint32_t i = 0; i < neighbors_count;++i){
                    fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
                    glm::vec3 distance_vec = (particle.old_position-particleB.old_position);
                    //float distance = glm::length(distance_vec);

                    lapla_t += (particle_mass/particleB.density)*poly6_kernel_laplacian(distance_vec);
                }

                f_surface = -surface_tension*lapla_t*color_gradient;
            }

            internal_f = f_surface;

            particle.velocity += subtimestep*((internal_f/particle.density));

        }

/*
        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
            uint32_t neighbors_count = neighborParticleIndexBuffer.size();
            // solve SPH

            //calculate internal forces
            glm::vec3 internal_f(0.0f);
            glm::vec3 f_pressure(0.0f);
            glm::vec3 f_viscosity(0.0f);
            glm::vec3 color_gradient(0.0f);

            for(uint32_t i = 0; i < neighbors_count;++i){
                fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
                glm::vec3 distance_vec = (particle.old_position-particleB.old_position);
                //float distance = glm::length(distance_vec);


                //if(distance < 1e-9f)distance = 1e-9f;

                //pressure

                
                //f_pressure += particle_mass*particle_mass*
                //((particle.pressure/(particle.density*particle.density))+(particleB.pressure)/(particleB.density*particleB.density))*
                //spiky_kernel_gradient(distance_vec);
                f_pressure += particle_mass*
                ((particle.pressure+particleB.pressure)/(2.0f*particleB.density))*
                //((particle.pressure/(particle.density*particle.density))+(particleB.pressure)/(particleB.density*particleB.density))*
                spiky_kernel_gradient_new(distance_vec);

                f_pressure += particle_mass*
                ((particle.near_pressure+particleB.near_pressure)/(2.0f*particleB.near_density))*
                //((particle.near_pressure/(particle.near_density*particle.near_density))+(particleB.near_pressure)/(particleB.near_density*particleB.near_density))*
                spiky_kernel_gradient_near_d(distance_vec);

                //viscosity
                
                //f_viscosity += particle_mass*
                //((particleB.old_velocity-particle.old_velocity)/(particleB.density*particleB.density))*
                //viscosity_kernel_laplacian(distance_vec);

                f_viscosity += particle_mass*
                (particleB.old_velocity-particle.old_velocity)*(1.f/(particleB.density*particleB.density))*
                poly6_kernel(distance_vec);

                //surface tension
                
                color_gradient += (particle_mass/particleB.density)*poly6_kernel_gradient(distance_vec);

            }

            //f_pressure = -f_pressure;
            f_pressure = -f_pressure;
            f_viscosity = viscosity*f_viscosity;

            float color_gradient_length = glm::length(color_gradient);

            glm::vec3 f_surface(0.0f);

            if(color_gradient_length > l_tension){
                color_gradient = color_gradient/color_gradient_length;
                float lapla_t = 0.0f;
                for(uint32_t i = 0; i < neighbors_count;++i){
                    fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
                    glm::vec3 distance_vec = (particle.old_position-particleB.old_position);
                    //float distance = glm::length(distance_vec);

                    lapla_t += (particle_mass/particleB.density)*poly6_kernel_laplacian(distance_vec);
                }

                f_surface = -surface_tension*lapla_t*color_gradient;
            }

            internal_f = f_pressure+f_viscosity+f_surface;

            //apply  external forces
            //particle.velocity += subtimestep*((-glm::length(gravity)*glm::normalize(particle.old_position)) + (internal_f/particle_mass));
            particle.velocity += subtimestep*((internal_f/particle.density));
            particle.position += subtimestep*particle.velocity;

        }
*/

        // get mutex
        Baundary_model_mutex.lock();
        glm::mat3 boundary_R = glm::mat3(fluid_boundary.model_m);
        glm::mat3 boundary_inverse_R = glm::inverse(boundary_R);
        glm::vec3 boundary_translation = glm::vec3(fluid_boundary.model_m[3]);

        // free mutex
        Baundary_model_mutex.unlock();

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            particle.position += subtimestep*particle.velocity;

            //detect collision and response
            glm::vec3 transformed_position = boundary_inverse_R*(particle.position - boundary_translation);

            float penetration_depth;

            if(transformed_position.x > 0.5f){
                penetration_depth = transformed_position.x - 0.5f;
                glm::vec3 normal_local(-1.0f,0.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }else if(transformed_position.x < -0.5f){
                penetration_depth = -transformed_position.x - 0.5f;
                glm::vec3 normal_local(1.0f,0.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }

            if(transformed_position.y > 0.5f){
                penetration_depth = transformed_position.y - 0.5f;
                glm::vec3 normal_local(0.0f,-1.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }else if(transformed_position.y < -0.5f){
                penetration_depth = -transformed_position.y - 0.5f;
                glm::vec3 normal_local(0.0f,1.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }

            if(transformed_position.z > 0.5f){
                penetration_depth = transformed_position.z - 0.5f;
                glm::vec3 normal_local(0.0f,0.0f,-1.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }else if(transformed_position.z < -0.5f){
                penetration_depth = -transformed_position.z - 0.5f;
                glm::vec3 normal_local(0.0f,0.0f,1.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }

            particle.old_velocity = particle.velocity;
            particle.old_position = particle.position;

        }
    
    }

    format_particle_buffer();

    //std::cout<<"solve end\n";
    solver_func_mutex.unlock();
    
}

#endif
#endif

#ifdef USE_VISCOEL

void FluidSim::SPHsolver::Viscoelastic_solve(float &time_window){


    solver_func_mutex.lock();

    UploadSimulationParm();
    
    time_window = timestep;

    const float inv_h = 1.0f / fluid_h;
    const float inv_rest_density = 1.0f / rest_density;
    const float inv_subtimestep = 1.0f / subtimestep;
    

    for(uint32_t substep = 0;substep < substep_num;++substep)
    {

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            //apply  external forces

            particle.velocity += subtimestep*(gravity);



        }

        CreateUnsortedList();
        SortList();
        BuildOffsetList();

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }




        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];


            glm::int3 cellIndex;
            cellIndex.x = (int)std::floor(particle.position.x * inv_h);
            cellIndex.y = (int)std::floor(particle.position.y * inv_h);
            cellIndex.z = (int)std::floor(particle.position.z * inv_h);

            std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
            // IMPORTANT: we reuse this buffer every step.
            // If we don't clear it, we'll keep stale neighbors forever and can even include self,
            // which causes r==0 in spiky_kernel_gradient -> NaNs.
            neighborParticleIndexBuffer.clear();
            neighborParticleIndexBuffer.reserve(64);

            find_neighbors(particleIndex,cellIndex,particle,neighborParticleIndexBuffer);

        }

////////////////////////////////// viscosity //////////////////////////////////////////////////

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
            uint32_t neighbors_count = neighborParticleIndexBuffer.size();
            // solve SPH

            //calculate internal forces
            glm::vec3 impulse(0.0f);

            for(uint32_t i = 0; i < neighbors_count;++i){
                fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
                glm::vec3 distance_vec = (particleB.old_position-particle.old_position);
                float distance = glm::length(distance_vec);
                //viscosity
                if(distance < fluid_h){
                    (distance == 0)?distance_vec = glm::vec3(0.0f,1.0f,0.0f):distance_vec = distance_vec/distance;
                    
                    float u  = glm::dot((particle.old_velocity - particleB.old_velocity),distance_vec);
                    if(u > 0){
                        impulse += 0.5f*subtimestep*(1.0f-(distance/fluid_h))*(viscosity_linear*u+viscosity_quadratic*u*u)*distance_vec;
                    }
                }
                


            }

            particle.velocity -= impulse;

        }
/////////////////////////////////////////////////////////////////////////////////

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            //apply  external forces

            particle.position += subtimestep*(particle.velocity);



        }
////////////////// springs ////////////////////////////////////

// ajust springs

// apply springs displacement

///////////////////////////////////////////////////////////////

/////////////////////// pressure ///////////////////////////////////////////////
        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
                uint32_t particleIndex = particleIndexBuffer[i];

                fluid_particle& particle = fluid_particles[particleIndex];

                std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
                uint32_t neighbors_count = neighborParticleIndexBuffer.size();

                //calculate density
                calc_density2(neighbors_count,neighborParticleIndexBuffer,particle);
                //calculate pressure
                particle.pressure = k_gas_constant*(particle.density-rest_density);

                particle.near_pressure = k_near_gas_constant*(particle.near_density);

        }

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }
        //compute pressure

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
            uint32_t neighbors_count = neighborParticleIndexBuffer.size();
            // solve SPH

            //calculate internal forces
            glm::vec3 displacement(0.0f);

            for(uint32_t i = 0; i < neighbors_count;++i){
                fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
                glm::vec3 distance_vec = (particleB.position-particle.position);
                float distance = glm::length(distance_vec);

                (distance == 0)? distance_vec = glm::vec3(0.0f,1.0f,0.0f):distance_vec =  distance_vec/distance;

                if(distance < fluid_h){

                    float kernel = (1.0f-(distance/fluid_h));

                    displacement += subtimestep*subtimestep*
                    (particle.pressure*kernel + particle.near_pressure*kernel*kernel)*distance_vec;
                }

            }

            particle.position_temp = particle.position-0.5f*displacement;
        }



        // get mutex
        Baundary_model_mutex.lock();
        glm::mat3 boundary_R = glm::mat3(fluid_boundary.model_m);
        glm::mat3 boundary_inverse_R = glm::inverse(boundary_R);
        glm::vec3 boundary_translation = glm::vec3(fluid_boundary.model_m[3]);

        // free mutex
        Baundary_model_mutex.unlock();

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            particle.position = particle.position_temp;

            //detect collision and response
            glm::vec3 transformed_position = boundary_inverse_R*(particle.position - boundary_translation);

            float penetration_depth;

            if(transformed_position.x > 0.5f){
                penetration_depth = transformed_position.x - 0.5f;
                glm::vec3 normal_local(-1.0f,0.0f,0.0f);
                particle.position += boundary_R * (penetration_depth * normal_local);
            }else if(transformed_position.x < -0.5f){
                penetration_depth = -transformed_position.x - 0.5f;
                glm::vec3 normal_local(1.0f,0.0f,0.0f);
                particle.position += boundary_R * (penetration_depth * normal_local);
            }

            if(transformed_position.y > 0.5f){
                penetration_depth = transformed_position.y - 0.5f;
                glm::vec3 normal_local(0.0f,-1.0f,0.0f);
                particle.position += boundary_R * (penetration_depth * normal_local);
            }else if(transformed_position.y < -0.5f){
                penetration_depth = -transformed_position.y - 0.5f;
                glm::vec3 normal_local(0.0f,1.0f,0.0f);
                particle.position += boundary_R * (penetration_depth * normal_local);
            }

            if(transformed_position.z > 0.5f){
                penetration_depth = transformed_position.z - 0.5f;
                glm::vec3 normal_local(0.0f,0.0f,-1.0f);
                particle.position += boundary_R * (penetration_depth * normal_local);
            }else if(transformed_position.z < -0.5f){
                penetration_depth = -transformed_position.z - 0.5f;
                glm::vec3 normal_local(0.0f,0.0f,1.0f);
                particle.position += boundary_R * (penetration_depth * normal_local);
            }


            particle.velocity = (particle.position-particle.old_position)/subtimestep;
            transformed_position = boundary_inverse_R*(particle.position - boundary_translation);

            float d_stick = fluid_h*0.2;

/*
            if(transformed_position.x > 0.5f-){
                penetration_depth = transformed_position.x - 0.5f;
                glm::vec3 normal_local(-1.0f,0.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                //float vn = glm::dot(particle.velocity, normal_world);
                //if (vn < 0.0f)
                    ////particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    //particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }else if(transformed_position.x < -0.5f){
                penetration_depth = -transformed_position.x - 0.5f;
                glm::vec3 normal_local(1.0f,0.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                //if (vn < 0.0f)
                    ////particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    //particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }

            if(transformed_position.y > 0.5f){
                penetration_depth = transformed_position.y - 0.5f;
                glm::vec3 normal_local(0.0f,-1.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                //if (vn < 0.0f)
                //    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                //    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }else if(transformed_position.y < -0.5f){
                penetration_depth = -transformed_position.y - 0.5f;
                glm::vec3 normal_local(0.0f,1.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                //if (vn < 0.0f)
                //    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                //    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }

            if(transformed_position.z > 0.5f){
                penetration_depth = transformed_position.z - 0.5f;
                glm::vec3 normal_local(0.0f,0.0f,-1.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                //if (vn < 0.0f)
                //    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                //    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }else if(transformed_position.z < -0.5f){
                penetration_depth = -transformed_position.z - 0.5f;
                glm::vec3 normal_local(0.0f,0.0f,1.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                //if (vn < 0.0f)
                //    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                //    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }

*/
            particle.old_velocity = particle.velocity;
            particle.old_position = particle.position;

        }
    
    }

    format_particle_buffer();

    //std::cout<<"solve end\n";
    solver_func_mutex.unlock();
    
}

#endif

///////////////////////// XPBF //////////////////////////////////////////////////////////////////////

#ifdef USE_XPBF

void FluidSim::SPHsolver::XPBF_solve(float &time_window){

    solver_func_mutex.lock();

    UploadSimulationParm();
    
    time_window = timestep;

    const float inv_h = 1.0f / fluid_h;
    const float inv_rest_density = 1.0f / rest_density;
    const float inv_subtimestep = 1.0f / subtimestep;

    for(uint32_t substep = 0;substep < substep_num;++substep)
    {

    if (solveAbortCondition.load()) {
        solver_func_mutex.unlock();
        return;
    }

    #pragma omp parallel for schedule(static)
    for (uint32_t i = 0; i < particles_number; ++i)
    {
        uint32_t particleIndex = particleIndexBuffer[i];

        fluid_particle& particle = fluid_particles[particleIndex];

        //apply  external forces
        particle.velocity += subtimestep*gravity;
        particle.position += subtimestep*particle.velocity;

    }

    if (solveAbortCondition.load()) {
        solver_func_mutex.unlock();
        return;
    }
    
    CreateUnsortedList();
    SortList();
    BuildOffsetList();

    if (solveAbortCondition.load()) {
        solver_func_mutex.unlock();
        return;
    }    


    #pragma omp parallel for schedule(static)
    for (uint32_t i = 0; i < particles_number; ++i)
    {
        uint32_t particleIndex = particleIndexBuffer[i];

        fluid_particle& particle = fluid_particles[particleIndex];


        glm::int3 cellIndex;
        cellIndex.x = (int)std::floor(particle.position.x * inv_h);
        cellIndex.y = (int)std::floor(particle.position.y * inv_h);
        cellIndex.z = (int)std::floor(particle.position.z * inv_h);

        std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
        // IMPORTANT: we reuse this buffer every step.
        // If we don't clear it, we'll keep stale neighbors forever and can even include self,
        // which causes r==0 in spiky_kernel_gradient -> NaNs.
        neighborParticleIndexBuffer.clear();
        neighborParticleIndexBuffer.reserve(64);

        find_neighbors(particleIndex,cellIndex,particle,neighborParticleIndexBuffer);

    }



    for(uint32_t i = 0; i < solver_iterations;++i){

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
                uint32_t particleIndex = particleIndexBuffer[i];

                fluid_particle& particle = fluid_particles[particleIndex];

                std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
                uint32_t neighbors_count = neighborParticleIndexBuffer.size();
                // solve SPH



                //calculate lambda_i
                calc_density(neighbors_count,neighborParticleIndexBuffer,particle);
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
                uint32_t particleIndex = particleIndexBuffer[i];

                fluid_particle& particle = fluid_particles[particleIndex];

                std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
                uint32_t neighbors_count = neighborParticleIndexBuffer.size();

                float constrain = particle.density*inv_rest_density - 1.0f;
                glm::vec3 sum_ki(0.0f);
                float sum_kj = 0.0f;

                for(uint32_t i = 0; i < neighbors_count;++i){
                    fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
                    glm::vec3 gradient = spiky_kernel_gradient(particle.position - particleB.position);
                    sum_ki += gradient;
                    sum_kj += glm::dot(gradient,gradient);
                }

                float sum_constrain_grad = (glm::dot(sum_ki,sum_ki) + sum_kj)*inv_rest_density*inv_rest_density;

                particle.lambda = -(constrain/(sum_constrain_grad + epsilon_lambda));
            


        }

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
            uint32_t neighbors_count = neighborParticleIndexBuffer.size();

            // solve SPH

            //calculating scorr

            glm::vec3 delta_position(0.0f);

            for(uint32_t i = 0; i < neighbors_count;++i){
                fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
                float scorr_basis = poly6_kernel(particle.position - particleB.position)/poly6_delta_q;
                float scorr = -0.1f*std::pow(scorr_basis,4);

                delta_position += (particle.lambda+particleB.lambda+scorr)*spiky_kernel_gradient(particle.position - particleB.position);
                
            }
            delta_position *= inv_rest_density;

            delta_position_buffer[particleIndex] = delta_position;
        }
    
///////////////////////////////////////////////////////////////////////////////

        // get mutex
        Baundary_model_mutex.lock();
        glm::mat3 boundary_R = glm::mat3(fluid_boundary.model_m);
        glm::mat3 boundary_inverse_R = glm::inverse(boundary_R);
        glm::vec3 boundary_translation = glm::vec3(fluid_boundary.model_m[3]);

        // free mutex
        Baundary_model_mutex.unlock();

        if (solveAbortCondition.load()) {
            solver_func_mutex.unlock();
            return;
        }

        #pragma omp parallel for schedule(static)
        for (uint32_t i = 0; i < particles_number; ++i)
        {
            uint32_t particleIndex = particleIndexBuffer[i];

            fluid_particle& particle = fluid_particles[particleIndex];

            //detect collision and response
            glm::vec3 transformed_position = boundary_inverse_R*(particle.position - boundary_translation);

            float penetration_depth;

            if(transformed_position.x > 0.5f){
                penetration_depth = transformed_position.x - 0.5f;
                glm::vec3 normal_local(-1.0f,0.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }else if(transformed_position.x < -0.5f){
                penetration_depth = -transformed_position.x - 0.5f;
                glm::vec3 normal_local(1.0f,0.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }

            if(transformed_position.y > 0.5f){
                penetration_depth = transformed_position.y - 0.5f;
                glm::vec3 normal_local(0.0f,-1.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }else if(transformed_position.y < -0.5f){
                penetration_depth = -transformed_position.y - 0.5f;
                glm::vec3 normal_local(0.0f,1.0f,0.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }

            if(transformed_position.z > 0.5f){
                penetration_depth = transformed_position.z - 0.5f;
                glm::vec3 normal_local(0.0f,0.0f,-1.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }else if(transformed_position.z < -0.5f){
                penetration_depth = -transformed_position.z - 0.5f;
                glm::vec3 normal_local(0.0f,0.0f,1.0f);
                glm::vec3 normal_world = glm::normalize(boundary_R * normal_local);
                particle.position += boundary_R * (penetration_depth * normal_local);
                float vn = glm::dot(particle.velocity, normal_world);
                if (vn < 0.0f)
                    //particle.velocity -= (1.0f + cr_coefficient_b*(penetration_depth/(subtimestep*glm::length(particle.velocity)))) * vn * normal_world;
                    particle.velocity -= (1.0f + cr_coefficient_b) * vn * normal_world;
            }

            // update position
            particle.position += delta_position_buffer[particleIndex];
            std::cout<<"x: "<<delta_position_buffer[particleIndex].z<<" y: "<<delta_position_buffer[particleIndex].y<<" z: "<<delta_position_buffer[particleIndex].z<<"\n";

        }
    }
///////////////////////////////////////////////////////////////////////////////

    if (solveAbortCondition.load()) {
        solver_func_mutex.unlock();
        return;
    }

    #pragma omp parallel for schedule(static)
    for (uint32_t i = 0; i < particles_number; ++i)
    {
        int32_t particleIndex = particleIndexBuffer[i];

        fluid_particle& particle = fluid_particles[particleIndex];

        //update velocity
        //particle.velocity = inv_subtimestep*(particle.position-particle.old_position);
        //particle.velocity = inv_subtimestep*(particle.position-particle.old_position);

    }
    if (solveAbortCondition.load()) {
        solver_func_mutex.unlock();
        return;
    }

    #pragma omp parallel for schedule(static)
    for (uint32_t i = 0; i < particles_number; ++i)
    {
        int32_t particleIndex = particleIndexBuffer[i];

        fluid_particle& particle = fluid_particles[particleIndex];


        std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
        uint32_t neighbors_count = neighborParticleIndexBuffer.size();


        //XSPH viscosity
        glm::vec3 vpoly6_sum(0.0f);

        for(uint32_t i = 0; i < neighbors_count;++i){
            fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
            glm::vec3 relative_velocity = particleB.velocity - particle.velocity;
            vpoly6_sum += relative_velocity*poly6_kernel(particle.position - particleB.position);
        }

        //particle.velocity = particle.velocity + c_viscosity*vpoly6_sum;
    }
    if (solveAbortCondition.load()) {
        solver_func_mutex.unlock();
        return;
    }

    #pragma omp parallel for schedule(static)
    for (uint32_t i = 0; i < particles_number; ++i)
    {
        int32_t particleIndex = particleIndexBuffer[i];

        fluid_particle& particle = fluid_particles[particleIndex];

        std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
        uint32_t neighbors_count = neighborParticleIndexBuffer.size();

        glm::vec3 vorticity(0.0f);

        for(uint32_t i = 0; i < neighbors_count;++i){
            fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
            glm::vec3 relative_velocity = particleB.velocity - particle.velocity;
            glm::vec3 gradient = -spiky_kernel_gradient(particle.position - particleB.position);
            vorticity += glm::cross(relative_velocity,gradient);
        }

        particle.vorticity = vorticity;
    }
    if (solveAbortCondition.load()) {
        solver_func_mutex.unlock();
        return;
    }

    #pragma omp parallel for schedule(static)
    for (uint32_t i = 0; i < particles_number; ++i)
    {
        uint32_t particleIndex = particleIndexBuffer[i];

        fluid_particle& particle = fluid_particles[particleIndex];

       //vorticity confinement

        std::vector<uint32_t> &neighborParticleIndexBuffer = neighborParticlesIndexBuffer[particleIndex];
        uint32_t neighbors_count = neighborParticleIndexBuffer.size();
 
        glm::vec3 vorticity_norm_gradient(0.0f);

        for(uint32_t i = 0; i < neighbors_count;++i){
            fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
            vorticity_norm_gradient += (glm::length(particleB.vorticity) - glm::length(particle.vorticity))*(-spiky_kernel_gradient(particle.position - particleB.position));
        }

        glm::vec3 location_vector = glm::normalize(vorticity_norm_gradient);

        //particle.velocity = particle.velocity+epsilon_vorticity*glm::cross(location_vector,particle.vorticity)*subtimestep;

        //update old position

        particle.old_position = particle.position;

    }
    }

    format_particle_buffer();

    //std::cout<<"solve end\n";
    solver_func_mutex.unlock();
}

#endif
