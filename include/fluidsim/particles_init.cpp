
#include "solver.hpp"

void FluidSim::SPHsolver::AddObject(
    std::shared_ptr<Object> object_ptr,const std::string &object_name, 
    bool sample_volume, bool uniform_volume_sampling,
    bool sample_surface)
{
    ObjectsVector[object_name] = std::make_shared<SamplingObject>();
    ObjectsVector[object_name]->object = object_ptr;
    ObjectsVector[object_name]->sample_volume = sample_volume;
    ObjectsVector[object_name]->uniform_volume_sampling = uniform_volume_sampling;
    ObjectsVector[object_name]->sample_surface = sample_surface;
    
}

void FluidSim::SPHsolver::RemoveObject(const std::string &object_name){
    if(ObjectsVector.find(object_name) == ObjectsVector.end()) {
        ObjectsVector.erase(object_name);
    }
    else std::cout << "Object "<<object_name<<" does not exist!\n";
}

void FluidSim::SPHsolver::InitParticles()
{
    for (auto it = ObjectsVector.begin(); it != ObjectsVector.end(); ++it)
    {
        if (!it->second->sample_volume && !it->second->sample_surface)
            continue;

        // --- GLM -> Eigen transform ---
        Eigen::Map<const Eigen::Matrix4f> model_eigen(
            glm::value_ptr(it->second->object->model_m)
        );

        const Eigen::Matrix3f rot = model_eigen.topLeftCorner<3,3>();
        const Eigen::Vector3f translate = model_eigen.topRightCorner<3,1>();

        std::shared_ptr<object_model> modelObj;

        uint32_t iterations = 1;
        if (it->second->object->object_type == ObjTypes::Object_types::Mesh)
        {
            modelObj = std::static_pointer_cast<object_model>(it->second->object);
            iterations = modelObj->localModel->meshes.size();
        }

        for (uint32_t index = 0; index < iterations; ++index)
        {
            std::optional<VerticesMap> vertices;
            std::optional<IndicesMap>  indices;
            Eigen::Matrix<float, 3, Eigen::Dynamic> vertices_storage;

            // storage that must stay alive (sphere case)
            std::shared_ptr<std::vector<float>> sphere_data;
            std::shared_ptr<std::vector<unsigned int>> sphere_ind;

            switch (it->second->object->object_type)
            {
                // -------- Cube --------
                case ObjTypes::Object_types::Cube:
                {
                    const ConstVerticesMap base(
                        SamplingShapes::Cube_vertices,
                        3,
                        sizeof(SamplingShapes::Cube_vertices) / (3 * sizeof(float)),
                        // VerticesMap uses a dynamic Eigen::Stride, so we must provide it.
                        // This array is tightly packed as xyz xyz xyz ...
                        Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic>(3, 1)
                    );

                    vertices_storage = (rot * base).colwise() + translate;

                    vertices.emplace(
                        vertices_storage.data(),
                        3,
                        vertices_storage.cols(),
                        Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic>(3, 1)
                    );

                    indices.emplace(
                        SamplingShapes::Cube_indices,
                        3,
                        sizeof(SamplingShapes::Cube_indices) / (3 * sizeof(unsigned int))
                    );
                    break;
                }

                // -------- Plane --------
                case ObjTypes::Object_types::Plane:
                {
                    const ConstVerticesMap base(
                        SamplingShapes::Plane_vertices,
                        3,
                        sizeof(SamplingShapes::Plane_vertices) / (3 * sizeof(float)),
                        // VerticesMap uses a dynamic Eigen::Stride, so we must provide it.
                        // This array is tightly packed as xyz xyz xyz ...
                        Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic>(3, 1)
                    );

                    vertices_storage = (rot * base).colwise() + translate;

                    vertices.emplace(
                        vertices_storage.data(),
                        3,
                        vertices_storage.cols(),
                        Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic>(3, 1)
                    );

                    indices.emplace(
                        SamplingShapes::Plane_indices,
                        3,
                        sizeof(SamplingShapes::Plane_indices) / (3 * sizeof(unsigned int))
                    );
                    break;
                }

                // -------- Sphere --------
                case ObjTypes::Object_types::Sphere:
                {
                    auto sphereObj =
                        std::static_pointer_cast<sphere>(it->second->object);

                    sphere_data = std::make_shared<std::vector<float>>();
                    sphere_ind  = std::make_shared<std::vector<unsigned int>>();

                    SamplingShapes::Generate_sphere_mesh(
                        sphereObj->x_segments,
                        sphereObj->y_segments,
                        *sphere_data,
                        *sphere_ind
                    );

                    const ConstVerticesMap base(
                        sphere_data->data(),
                        3,
                        sphere_data->size() / 3,
                        // VerticesMap uses a dynamic Eigen::Stride, so we must provide it.
                        // sphere_data is tightly packed as xyz xyz xyz ...
                        Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic>(3, 1)
                    );

                    vertices_storage = (rot * base).colwise() + translate;

                    vertices.emplace(
                        vertices_storage.data(),
                        3,
                        vertices_storage.cols(),
                        Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic>(3, 1)
                    );


                    indices.emplace(
                        sphere_ind->data(),
                        3,
                        sphere_ind->size() / 3
                    );
                    break;
                }

                // -------- Mesh --------
                case ObjTypes::Object_types::Mesh:
                {
                    auto& mesh = modelObj->localModel->meshes[index];
                    auto& verts = mesh.vertices;

                    const ConstVerticesMap base(
                        reinterpret_cast<float*>(&verts[0].Position),
                        3,
                        verts.size(),
                        Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic>(
                            sizeof(Vertex) / sizeof(float),
                            1
                        )
                    );

                    vertices_storage = (rot * base).colwise() + translate;

                    vertices.emplace(
                        vertices_storage.data(),
                        3,
                        vertices_storage.cols(),
                        Eigen::Stride<Eigen::Dynamic, Eigen::Dynamic>(3, 1)
                    );

                    indices.emplace(
                        mesh.indices.data(),
                        3,
                        mesh.indices.size() / 3
                    );
                    break;
                }
            }

            // ---------- Volume sampling ----------
            if (it->second->sample_volume)
            {   
                //std::cout << "val1\n";
                std::vector<Eigen::Vector3f> sampling =
                    it->second->uniform_volume_sampling
                    ? VolumeSampler::sampleMeshDense(
                        *vertices, *indices,
                        0.5f * particle_spacing,
                        particle_spacing
                      )
                    : VolumeSampler::sampleMeshRandom(
                        *vertices, *indices,
                        0.5f * particle_spacing
                      );
                //std::cout << "val2\n";
                fluid_particles.reserve(
                    fluid_particles.size() + sampling.size()
                );

                for (const auto& v : sampling)
                {
                    fluid_particles.emplace_back(fluid_particle{
                        glm::vec3(v.x(), v.y(), v.z()),
                        glm::vec3(v.x(), v.y(), v.z()),
                        #ifdef USE_VISCOEL
                        glm::vec3(0.0f,0.0f,0.0f),
                        #endif
                        glm::vec3(0.0f,0.0f,0.0f),
                        glm::vec3(0.0f,0.0f,0.0f),
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        false
                    });
                }
            }

            // ---------- Surface sampling ----------
            if (it->second->sample_surface)
            {
                auto sampling = SurfaceSampler::sampleMesh(
                    *vertices, *indices, particle_spacing
                );

                it->second->particle_boundaries.emplace_back(
                    fluid_particles.size(),
                    sampling.size()
                );

                fluid_particles.reserve(
                    fluid_particles.size() + sampling.size()
                );

                for (const auto& v : sampling)
                {
                    fluid_particles.emplace_back(fluid_particle{
                        glm::vec3(v.x(), v.y(), v.z()),
                        glm::vec3(v.x(), v.y(), v.z()),
                        #ifdef USE_VISCOEL
                        glm::vec3(0.0f,0.0f,0.0f),
                        #endif
                        glm::vec3(0.0f,0.0f,0.0f),
                        glm::vec3(0.0f,0.0f,0.0f),
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        true
                    });
                }
            }
        }
    }
}