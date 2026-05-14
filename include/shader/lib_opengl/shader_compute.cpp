#include "shader_compute.h"

void Query_compute_limitations(uint32_t max_compute_work_group_count[3],
                               uint32_t max_compute_work_group_size[3],
                               uint32_t max_compute_work_group_invocations)
{

	// query limitations
	// -----------------

    int max_compute_work_group_counti[3];
    int max_compute_work_group_sizei[3];
    int max_compute_work_group_invocationsi;

	for (int idx = 0; idx < 3; idx++) {
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, idx, &max_compute_work_group_counti[idx]);
		glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, idx, &max_compute_work_group_sizei[idx]);
	}	
	glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &max_compute_work_group_invocationsi);

    max_compute_work_group_count[0] = static_cast<uint32_t>(max_compute_work_group_counti[0]); 
    max_compute_work_group_count[1] = static_cast<uint32_t>(max_compute_work_group_counti[1]);
    max_compute_work_group_count[2] = static_cast<uint32_t>(max_compute_work_group_counti[2]);

    max_compute_work_group_size[0] = static_cast<uint32_t>(max_compute_work_group_sizei[0]); 
    max_compute_work_group_size[1] = static_cast<uint32_t>(max_compute_work_group_sizei[1]);
    max_compute_work_group_size[2] = static_cast<uint32_t>(max_compute_work_group_sizei[2]);

    max_compute_work_group_invocations = static_cast<uint32_t>(max_compute_work_group_invocationsi);

}

void Print_compute_limitations(uint32_t max_compute_work_group_count[3],
                               uint32_t max_compute_work_group_size[3],
                               uint32_t max_compute_work_group_invocations)
{

	std::cout << "OpenGL Limitations: " << std::endl;
	std::cout << "maximum number of work groups in X dimension " << max_compute_work_group_count[0] << std::endl;
	std::cout << "maximum number of work groups in Y dimension " << max_compute_work_group_count[1] << std::endl;
	std::cout << "maximum number of work groups in Z dimension " << max_compute_work_group_count[2] << std::endl;

	std::cout << "maximum size of a work group in X dimension " << max_compute_work_group_size[0] << std::endl;
	std::cout << "maximum size of a work group in Y dimension " << max_compute_work_group_size[1] << std::endl;
	std::cout << "maximum size of a work group in Z dimension " << max_compute_work_group_size[2] << std::endl;

	std::cout << "Number of invocations in a single local work group that may be dispatched to a compute shader " << max_compute_work_group_invocations << std::endl;
}
