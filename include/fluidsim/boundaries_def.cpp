#include "solver.hpp"


namespace FluidSim {

namespace SamplingShapes {

float Cube_vertices[192] = {
    // Back face
    -0.5f, -0.5f, -0.5f, // Bottom-left
     0.5f,  0.5f, -0.5f, // top-right
     0.5f, -0.5f, -0.5f, // bottom-right         
    -0.5f,  0.5f, -0.5f, // top-left
    // Front face
    -0.5f, -0.5f,  0.5f, // bottom-left 
     0.5f, -0.5f,  0.5f, // bottom-right
     0.5f,  0.5f,  0.5f, // top-right
    -0.5f,  0.5f,  0.5f, // top-left  
    // Left face
    -0.5f,  0.5f,  0.5f, // top-right 
    -0.5f,  0.5f, -0.5f, // top-left
    -0.5f, -0.5f, -0.5f, // bottom-left
    -0.5f, -0.5f,  0.5f, // bottom-right 
    // Right face
     0.5f,  0.5f,  0.5f, // top-left 
     0.5f, -0.5f, -0.5f, // bottom-right
     0.5f,  0.5f, -0.5f, // top-right         
     0.5f, -0.5f,  0.5f, // bottom-left      
    // Bottom face
    -0.5f, -0.5f, -0.5f, // top-right 
     0.5f, -0.5f, -0.5f, // top-left
     0.5f, -0.5f,  0.5f, // bottom-left
    -0.5f, -0.5f,  0.5f, // bottom-right 
    // Top face
    -0.5f,  0.5f, -0.5f, // top-left 
     0.5f,  0.5f,  0.5f, // bottom-right
     0.5f,  0.5f, -0.5f, // top-right     
    -0.5f,  0.5f,  0.5f // bottom-left        
};

unsigned int Cube_indices[36] = {
    // Back face
    0, 1, 2,
    1, 0, 3,
    // Front face 
    4, 5, 6,
    6, 7, 4,
    // Left face
    8, 9, 10,
    10, 11, 8,
    // Right face
    12, 13, 14,
    13, 12, 15,
    // Top face
    16, 17, 18,
    18, 19, 16,
    // Bottom face
    20, 21, 22,
    21, 20, 23
};

float Plane_vertices[12] = {
    // positions   
   -0.5,  0.0, -0.5, // bottom left
    0.5,  0.0, -0.5, // bottom right
    0.5,  0.0,  0.5, // top right
   -0.5,  0.0,  0.5  // top left
							
};

unsigned int Plane_indices[6] = {3, 2, 0, 1, 0, 2};

//create function to generate sphere

void Generate_sphere_mesh(const unsigned int &X_SEGMENTS, const unsigned int &Y_SEGMENTS,std::vector<float> &data,std::vector<unsigned int> &indices)
{

    std::vector<glm::vec3> positions;

    const float PI = 3.14159265359f;
    for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
    {
        for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
        {
            float xSegment = (float)x / (float)X_SEGMENTS;
            float ySegment = (float)y / (float)Y_SEGMENTS;
            float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
            float yPos = std::cos(ySegment * PI);
            float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

            positions.push_back(glm::vec3(xPos, yPos, zPos));
        }
    }

    bool oddRow = false;
    for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
    {
        if (!oddRow) // even rows: y == 0, y == 2; and so on
        {
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
            {
                indices.push_back(y * (X_SEGMENTS + 1) + x);
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
            }
        }
        else
        {
            for (int x = X_SEGMENTS; x >= 0; --x)
            {
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + x);
            }
        }
        oddRow = !oddRow;
    }

    
    for (unsigned int i = 0; i < positions.size(); ++i)
    {
        data.push_back(positions[i].x);
        data.push_back(positions[i].y);
        data.push_back(positions[i].z);
    }

}




}
}
