
#include "solver.hpp"

void FluidSim::SPHsolver::calc_density(const uint32_t &neighbors_count, const std::vector<uint32_t> &neighborParticleIndexBuffer,fluid_particle &particle){

    float particle_density = 0.0f;
    float particle_near_density = 0.0f;
    for(uint32_t i = 0; i < neighbors_count;++i){
        fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
        particle_density += particle_mass*poly6_kernel(particle.position - particleB.position);
        particle_near_density += particle_mass*spiky_kernel_near_d(particle.position - particleB.position);
    }
    particle_density += particle_mass*poly6_kernel(glm::vec3(0.0f));
    particle.density = particle_density;
    particle_near_density += particle_mass*spiky_kernel_near_d(glm::vec3(0.0f));
    particle.near_density = particle_near_density; 
}

void FluidSim::SPHsolver::calc_density2(const uint32_t &neighbors_count, const std::vector<uint32_t> &neighborParticleIndexBuffer,fluid_particle &particle){

    float particle_density = 0.0f;
    float particle_near_density = 0.0f;
    for(uint32_t i = 0; i < neighbors_count;++i){
        fluid_particle &particleB = fluid_particles[neighborParticleIndexBuffer[i]];
        
        glm::vec3 distance_vec = (particleB.position-particle.position);
        float distance = glm::length(distance_vec);
                
        if(distance < fluid_h){
            float kernel = (1.0f-(distance/fluid_h));
            particle_density += kernel*kernel;
            particle_near_density += kernel*kernel*kernel;
        }
    }

    //particle_density += 1.0f;
    particle.density = particle_density;
    //particle_near_density += 1.0f;
    particle.near_density = particle_near_density; 
}

float FluidSim::SPHsolver::poly6_kernel(const glm::vec3 &distance_vec){
    // 3D Poly6 kernel:
    // W(r,h) = 315/(64*pi*h^9) * (h^2 - r^2)^3   for 0 <= r <= h, else 0
    const float r2 = glm::dot(distance_vec, distance_vec);
    const float h2 = fluid_h * fluid_h;
    if (r2 >= h2) return 0.0f;

    const float diff = h2 - r2;
    return (315.0f / (64.0f * FLUID_PI * std::powf(fluid_h, 9))) * (diff * diff * diff);
}

float FluidSim::SPHsolver::poly6_kernel_laplacian(const glm::vec3 &distance_vec){
    // 3D Poly6 kernel:
    const float r2 = glm::dot(distance_vec, distance_vec);
    const float h2 = fluid_h * fluid_h;
    if (r2 >= h2) return 0.0f;

    const float diff = 3.0f*h2*h2 -10.0f*h2*r2+7.0f*r2*r2;
    return (-6.0f*315.0f / (64.0f * FLUID_PI * std::powf(fluid_h, 9))) * diff;
}

glm::vec3 FluidSim::SPHsolver::poly6_kernel_gradient(const glm::vec3 &distance_vec){

    const float r2 = glm::dot(distance_vec,distance_vec);
    const float h2 = fluid_h * fluid_h;
    if (r2 >= h2) return glm::vec3(0.0f);

    const float diff = h2 - r2;
    return (-6.0f*315.0f / (64.0f * FLUID_PI * std::powf(fluid_h, 9))) * diff*diff*distance_vec;
}

float FluidSim::SPHsolver::spiky_kernel(const glm::vec3 &distance_vec){
    const float r = glm::length(distance_vec);
    // r == 0 is undefined due to (r_vec / r); treat as zero to avoid NaNs.
    if (r >= fluid_h) return 0.0f;

    const float diff = fluid_h - r;
    return (15.0f / (FLUID_PI * std::powf(fluid_h, 6))) * (diff * diff * diff);
    
}

glm::vec3 FluidSim::SPHsolver::spiky_kernel_gradient(const glm::vec3 &distance_vec){
    // 3D Spiky kernel gradient:
    // ∇W(r,h) = -45/(pi*h^6) * (h - r)^2 * (r_vec / r)   for 0 < r <= h, else 0
    const float r = glm::length(distance_vec);
    // r == 0 is undefined due to (r_vec / r); treat as zero to avoid NaNs.
    //if (r <= 1e-6f || r >= fluid_h) return glm::vec3(0.0f);
    if (r >= fluid_h) return glm::vec3(0.0f);

    if(r==0.0f){
        const float coeff = (-45.0f / (FLUID_PI * std::powf(fluid_h, 6))) * (fluid_h * fluid_h);
        return coeff * glm::vec3(0.0f,1.0f,0.0f);
    }else{
        const float diff = fluid_h - r;
        const float coeff = (-45.0f / (FLUID_PI * std::powf(fluid_h, 6))) * (diff * diff / r);
        return coeff * distance_vec;
    }
}

float FluidSim::SPHsolver::viscosity_kernel_laplacian(const glm::vec3 &distance_vec){
    const float r = glm::length(distance_vec);
    if (r >= fluid_h) return 0.0f;

    const float diff = fluid_h - r;
    return (45.0f / (FLUID_PI * std::powf(fluid_h, 6))) * diff;
}

float FluidSim::SPHsolver::viscosity_kernel(const glm::vec3 &distance_vec){
    float r = glm::length(distance_vec);
    if (r > fluid_h) return 0.0f;
    if (r <= 1e-6f) r = 1e-6f;
    const float el1 = -(r*r*r)/(2*fluid_h*fluid_h*fluid_h); 
    const float el2 = (r*r)/(fluid_h*fluid_h);
    const float el3 = (fluid_h)/(2*r);
    return (15.0f / (2.0f*FLUID_PI * fluid_h*fluid_h*fluid_h)) *(el1+el2+el3-1.0f);
}

//////////////new////////////////////////////


float FluidSim::SPHsolver::spiky_kernel_new(const glm::vec3 &distance_vec){
    const float r = glm::length(distance_vec);
    // r == 0 is undefined due to (r_vec / r); treat as zero to avoid NaNs.
    if (r >= fluid_h) return 0.0f;

    const float diff = fluid_h - r;
    return (15.0f / (2.0f*FLUID_PI * std::powf(fluid_h, 5))) * (diff * diff);
    
}

float FluidSim::SPHsolver::spiky_kernel_near_d(const glm::vec3 &distance_vec){
    const float r = glm::length(distance_vec);
    // r == 0 is undefined due to (r_vec / r); treat as zero to avoid NaNs.
    if (r >= fluid_h) return 0.0f;

    const float diff = fluid_h - r;
    return (15.0f / (FLUID_PI * std::powf(fluid_h, 6))) * (diff * diff * diff);
    
}


glm::vec3 FluidSim::SPHsolver::spiky_kernel_gradient_new(const glm::vec3 &distance_vec){
    // 3D Spiky kernel gradient:
    // ∇W(r,h) = -45/(pi*h^6) * (h - r)^2 * (r_vec / r)   for 0 < r <= h, else 0
    const float r = glm::length(distance_vec);
    // r == 0 is undefined due to (r_vec / r); treat as zero to avoid NaNs.
    //if (r <= 1e-6f || r >= fluid_h) return glm::vec3(0.0f);
    if (r >= fluid_h) return glm::vec3(0.0f);

    if(r==0.0f){
        const float coeff = (-15.0f / (FLUID_PI * std::powf(fluid_h, 5))) * fluid_h;
        return coeff * glm::vec3(0.0f,1.0f,0.0f);
    }else{
        const float diff = fluid_h - r;
        const float coeff = (-15.0f / (FLUID_PI * std::powf(fluid_h, 5))) * diff;
        return coeff * distance_vec / r;
    }
}

glm::vec3 FluidSim::SPHsolver::spiky_kernel_gradient_near_d(const glm::vec3 &distance_vec){
    // 3D Spiky kernel gradient:
    // ∇W(r,h) = -45/(pi*h^6) * (h - r)^2 * (r_vec / r)   for 0 < r <= h, else 0
    const float r = glm::length(distance_vec);
    // r == 0 is undefined due to (r_vec / r); treat as zero to avoid NaNs.
    //if (r <= 1e-6f || r >= fluid_h) return glm::vec3(0.0f);
    if (r >= fluid_h) return glm::vec3(0.0f);

    if(r==0.0f){
        const float coeff = (-45.0f / (FLUID_PI * std::powf(fluid_h, 6))) * fluid_h*fluid_h;
        return coeff * glm::vec3(0.0f,1.0f,0.0f);
    }else{
        const float diff = fluid_h - r;
        const float coeff = (-45.0f / (FLUID_PI * std::powf(fluid_h, 6))) * diff*diff;
        return coeff * distance_vec / r;
    }
}