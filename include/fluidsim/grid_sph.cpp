
#include "solver.hpp"

uint32_t FluidSim::SPHsolver::nextPowerOfTwo(uint32_t v)
{
    if (v == 0) return 1;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

uint32_t FluidSim::SPHsolver::ZigZag(int v)
{
    return (uint32_t(v) << 1) ^ uint32_t(v >> 31);
}

uint32_t FluidSim::SPHsolver::GetFlatCellIndex(glm::int3 c)
{
    uint32_t x = ZigZag(c.x);
    uint32_t y = ZigZag(c.y);
    uint32_t z = ZigZag(c.z);

    uint32_t h = x * 0x9e3779b9u
           ^ y * 0x85ebca6bu
           ^ z * 0xc2b2ae35u;

    h ^= h >> 16;
    return h & (total_grid_cell_count - 1);
}

void FluidSim::SPHsolver::CreateUnsortedList(){ //parallel

    const float inv_h = 1.0f / fluid_h;

    //std::for_each(std::execution::par,
    //              particleIndexBuffer.begin(),
    //              particleIndexBuffer.end(),
    //              [&](uint32_t particleIndex)
    //{
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < (int)particleIndexBuffer.size(); ++i)
    {
        uint32_t particleIndex = particleIndexBuffer[i];
        const fluid_particle& particle = fluid_particles[particleIndex];

        glm::int3 cellIndex;
        cellIndex.x = (int)std::floor(particle.position.x * inv_h);
        cellIndex.y = (int)std::floor(particle.position.y * inv_h);
        cellIndex.z = (int)std::floor(particle.position.z * inv_h);

        cellIndexBuffer[particleIndex] = GetFlatCellIndex(cellIndex);
    }
    //});

}

void FluidSim::SPHsolver::SortList(){ //parallel
    std::sort(std::execution::par,
              particleIndexBuffer.begin(),
              particleIndexBuffer.end(),
              [&](uint32_t a, uint32_t b)
              {
                  return cellIndexBuffer[a] < cellIndexBuffer[b];
              });

}

void FluidSim::SPHsolver::BuildOffsetList(){ 
    //uint32_t particleIndex = particleIndexBuffer[dispatchThreadID.x];
    //uint32_t cellIndex = cellIndexBuffer[particleIndex];

    //InterlockedMin(cellOffsetBuffer[cellIndex], dispatchThreadID.x);
    //std::fill(cellOffsetBuffer.begin(), cellOffsetBuffer.end(), UINT32_MAX);
    std::memset(cellOffsetBuffer.data(), 0xFF,cellOffsetBuffer.size() * sizeof(uint32_t));
    
    //#pragma omp parallel for schedule(static)
    for (uint32_t i = 0; i < particles_number; ++i)
    {
        uint32_t cell = cellIndexBuffer[particleIndexBuffer[i]];

        //uint32_t cell_offset_idx = cellOffsetBuffer[cell];//

        if (cellOffsetBuffer[cell] == UINT32_MAX )//cell_offset_idx == UINT32_MAX || i < cell_offset_idx)
            cellOffsetBuffer[cell] = i;
    }
}

void FluidSim::SPHsolver::InitGrid()
{

    particles_number = fluid_particles.size();

    color_gradient_vector.resize(particles_number);

    particleIndexBuffer.resize(particles_number);

    #ifdef USE_XPBF
    delta_position_buffer.resize(particles_number);
    #endif

    total_grid_cell_count = nextPowerOfTwo(4*particles_number); //or 4*

    cellIndexBuffer.resize(particles_number);

    cellOffsetBuffer.resize(total_grid_cell_count);

    neighborParticlesIndexBuffer.resize(particles_number);

    #pragma omp parallel for schedule(static)
    for (uint32_t i = 0; i < particles_number; ++i) particleIndexBuffer[i] = i;

    //gpuParticleBuffer.reserve(particles_number);
    gpuParticleBuffer.resize(particles_number);

}