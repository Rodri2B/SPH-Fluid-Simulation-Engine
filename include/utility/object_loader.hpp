#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/gtx/euler_angles.hpp>
#endif

#include <lib_opengl/shader.h>
#include <lib_opengl/model_original.h>
#include <atomic>


//std::string default_shader_path = "../shaders/default/";
//Shader default_shader((default_shader_path+"default_sh.vs").c_str(), (default_shader_path+"default_sh.fs").c_str());

/* one pissibles solution would be flip the x axis or render from the other side because that orietation was thought to
be looked from below and not from above
namespace Plane{

    float Vertices[32] = {
    // positions      //normals     //coords
   -0.5,  0.0, -0.5,  0.0, 1.0, 0.0 ,0.0, 0.0, // bottom left
    0.5,  0.0, -0.5,  0.0, 1.0, 0.0 ,1.0, 0.0, // bottom right
    0.5,  0.0,  0.5,  0.0, 1.0, 0.0 ,1.0, 1.0, // top right
   -0.5,  0.0,  0.5,  0.0, 1.0, 0.0 ,0.0, 1.0, // top left
							
    };

    unsigned int Indices[6] = {3, 2, 0, 1, 0, 2};
};

*/

namespace Plane{

    extern float Vertices[32];

    extern float Vertices_vertical[32];

    extern unsigned int Indices[6];
};

namespace Cube{

    extern float Vertices[192];
    

    extern unsigned int Indices[36];
};

namespace EdgeCube{

    extern unsigned int Indices[24];
};

struct object_config_t 
{

    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    glm::vec2 position_tex;
    float rotation_tex; 
    glm::vec2 scale_tex;
    glm::mat4 model_m_ref;
    Shader &dshader;

};


enum draw_mode {
    DRAW_UNTEXTURED,
    DRAW_TEXTURED,
    DRAW_UNTEXTURED_INSTANCED,
    DRAW_TEXTURED_INSTANCED,
    DRAW_MODEL,
    DRAW_MODEL_INSTANCED,
    DRAW_INFO,
    DRAW_MODEL_INFO,
    DRAW_OUTLINE,
    DRAW_MODEL_OUTLINE,
    DRAW_EDGE_ONLY
};

namespace ObjTypes
{
enum Object_types {
    Plane,
    Circle,
    Cube,
    EdgeCube,
    Sphere,
    Elipse,
    Icosphere,
    Cylinder,
    Cone,
    Torus,
    Mesh
};

}

namespace object_loader{
struct Buffers
{
    unsigned int ObjectVAO;
    unsigned int ObjectVBO;
    unsigned int ObjectEBO;
    unsigned int ObjectABO;
    unsigned int ObjectEBSize; 
};

}
class Object
{

public:
    Object(Shader &dshader, unsigned int dmode = DRAW_UNTEXTURED);
    Object(const glm::vec3 &pos, Shader &dshader, unsigned int dmode = DRAW_UNTEXTURED);
    Object(const glm::vec3 &pos, const glm::vec3 &rot, Shader &dshader, unsigned int dmode = DRAW_UNTEXTURED);
    Object(const glm::vec3 &pos, const glm::vec3 &rot, const glm::vec3 &scale, Shader &dshader, unsigned int dmode = DRAW_UNTEXTURED);
    Object(const object_config_t &config, unsigned int dmode = DRAW_UNTEXTURED);
    
    void Draw();
    void DrawInfo(const unsigned int &index);
    void DrawOutline();

    void setInstancePosBuffer(const unsigned int &buffer, const unsigned int &amount);

    void SetDrawMode(unsigned int mode);
    void SetDrawInfoMode(unsigned int mode);
    void SetDrawOutlineMode(unsigned int mode);
    void SetInfoShader(Shader *info_shader);
    void SetOutlineShader(Shader *outline_shader);
    void SetTexture(unsigned int TexIndex);
    void SetTransform();
    void SetProjection(const glm::mat4 &projection);
    void SetView(const glm::mat4 &projection);
    void SetInfoProjection(const glm::mat4 &projection);
    void SetOutlineProjection(const glm::mat4 &projection);
    void SetInfoView(const glm::mat4 &projection);
    void SetOutlineView(const glm::mat4 &view);
    void SetTextureTransform();
    void SetInfoTransform();
    void SetOutlineTransform();
    void SetTextureInfoTransform();
    void SetViewProjUBuffer(unsigned int biding);
    void ScaleTexture(const glm::vec2 &scale_vector);
    void RotateTexture(const float &angle);
    void PositionateTexture(const glm::vec2 &position_new);
    void ApplyRotationTexture();
    void GlobalScale(const glm::vec3 &scale_vector);
    void GlobalRotate(const float roll, const float pitch, const float yaw);
    void Scale(const glm::vec3 &scale_vector);
    void Scale(glm::mat4 &delta_scale);
    void Rotate(float angle,const glm::vec3 &rot_vector);
    void Rotate(const float roll, const float pitch, const float yaw);
    void Rotate(const glm::vec3 rot);
    void Rotate(glm::mat4 &delta_rot);
    void Tanslate(const glm::vec3 &translation);
    void ApplyAllTransfoms();
    void ApplyRotation();
    void ApplyTranslation();
    void ApplyScale();
    glm::vec3 Get_rotation();
    glm::vec3 Get_position();
    glm::vec3 Get_scale();
    glm::mat4 Get_model_matrix();
    glm::mat4 Get_reference_model_matrix();
    void Change_rotation(const glm::vec3 &rotation_new);
    void Change_position(const glm::vec3 &position_new);
    void Change_scale(const glm::vec3 &scale_vector);
    void Change_reference_model_matrix(const glm::mat4 &new_model);
    void Reset_reference_model_matrix();
    void Reset_reference_model_texture_matrix();

    object_loader::Buffers getBuffers();

    unsigned int Texture;
    Shader &localShader;
    Shader *localShader_info;
    Shader *localShader_outline;
    Model *localModel = nullptr;

    ObjTypes::Object_types object_type;

    glm::mat4 model_m = glm::mat4(1.0f);
    glm::mat3 model_m_tex = glm::mat3(1.0f);

    glm::vec3 position = glm::vec3(0.0f,0.0f,0.0f);
    glm::vec3 rotation = glm::vec3(0.0f,0.0f,0.0f); //roll, pith, yaw or x,y,z
    glm::vec3 scale = glm::vec3(1.0f,1.0f,1.0f);

protected:

    virtual void Init() = 0;

    void draw_untextured();
    void draw_textured();
    void draw_untextured_instanced();
    void draw_textured_instanced();
    void draw_model();
    void draw_model_instanced();
    void draw_info(const unsigned int &index);
    void draw_model_info(const unsigned int &index);
    void draw_outline();
    void draw_model_outline();

    void draw_edge_only();
    void draw_edge_only_info(const unsigned int &index);
    void draw_edge_only_outline();

    //use (p.*Draw)(); out of the class
    void (Object::*Draw_method)();
    void (Object::*Draw_method_info)(const unsigned int &index);
    void (Object::*Draw_method_outline)();

    unsigned int ObjectVAO;
    unsigned int ObjectVBO;
    unsigned int ObjectEBO;

    unsigned int ObjectABO;
    unsigned int amount_i;

    unsigned int element_buffer_size;

    //glm::mat4 model_m = glm::mat4(1.0f);
    glm::mat4 model_m_ref = glm::mat4(1.0f);

    //glm::vec3 position = glm::vec3(0.0f,0.0f,0.0f);
    glm::mat4 local_pos_matrix = glm::mat4(1.0f);

    //glm::vec3 rotation = glm::vec3(0.0f,0.0f,0.0f); //roll, pith, yaw or x,y,z
    glm::mat4 local_rot_matrix = glm::mat4(1.0f);

    //glm::vec3 scale = glm::vec3(1.0f,1.0f,1.0f);
    glm::mat4 local_scale_matrix = glm::mat4(1.0f);

    //glm::mat3 model_m_tex = glm::mat3(1.0f);
    glm::mat3 model_m_tex_ref = glm::mat3(1.0f);

    glm::vec2 position_tex = glm::vec2(0.0f,0.0f);
    glm::mat3 local_pos_matrix_tex = glm::mat3(1.0f);

    float rotation_tex = 0.0f; 
    glm::mat3 local_rot_matrix_tex = glm::mat3(1.0f);

    glm::vec2 scale_tex = glm::vec2(1.0f,1.0f);
    glm::mat3 local_scale_matrix_tex = glm::mat3(1.0f);


};




class plane : public Object
{
    public:

    plane(Shader &dshader, unsigned int dmode = DRAW_UNTEXTURED);
    plane(const glm::vec3 &pos, Shader &dshader, unsigned int dmode = DRAW_UNTEXTURED);
    plane(const glm::vec3 &pos, Shader &dshader, unsigned int dmode, bool vertical);
    plane(const glm::vec3 &pos, const glm::vec3 &rot, Shader &dshader, unsigned int dmode = DRAW_UNTEXTURED);
    plane(const glm::vec3 &pos, const glm::vec3 &rot, const glm::vec3 &scale, Shader &dshader, unsigned int dmode = DRAW_UNTEXTURED);
    plane(const object_config_t &config, unsigned int dmode = DRAW_UNTEXTURED);

    bool back_face_culling = true;

    void Init(/* args */) override;
    void InitVertical();
};



class sphere : public Object
{
    public:

    sphere(Shader &dshader, const float &Radius, unsigned int dmode = DRAW_UNTEXTURED);
    sphere(const glm::vec3 &pos, Shader &dshader, const float &Radius, unsigned int dmode = DRAW_UNTEXTURED);
    sphere(const glm::vec3 &pos, const glm::vec3 &rot, Shader &dshader, const float &Radius, unsigned int dmode = DRAW_UNTEXTURED);
    sphere(const glm::vec3 &pos, const glm::vec3 &rot, const glm::vec3 &scale, Shader &dshader, const float &Radius, unsigned int dmode = DRAW_UNTEXTURED);
    sphere(const object_config_t &config, const float &Radius, unsigned int dmode = DRAW_UNTEXTURED);

    void Init(/* args */) override;

    float getRadius();

    unsigned int x_segments = 64;
    unsigned int y_segments = 64;

    // ***
    std::atomic<glm::vec3> position;

    private:

    //float radius;
    std::atomic<float> radius;
};



class cube : public Object
{
    public:

    cube(Shader &dshader, unsigned int dmode = DRAW_UNTEXTURED);
    cube(const glm::vec3 &pos, Shader &dshader, unsigned int dmode = DRAW_UNTEXTURED);
    cube(const glm::vec3 &pos, const glm::vec3 &rot, Shader &dshader, unsigned int dmode = DRAW_UNTEXTURED);
    cube(const glm::vec3 &pos, const glm::vec3 &rot, const glm::vec3 &scale, Shader &dshader, unsigned int dmode = DRAW_UNTEXTURED);
    cube(const object_config_t &config, unsigned int dmode = DRAW_UNTEXTURED);

    void Init(/* args */) override;
};

class edge_cube : public Object
{
    public:

    edge_cube(Shader &dshader);
    edge_cube(const glm::vec3 &pos, Shader &dshader);
    edge_cube(const glm::vec3 &pos, const glm::vec3 &rot, Shader &dshader);
    edge_cube(const glm::vec3 &pos, const glm::vec3 &rot, const glm::vec3 &scale, Shader &dshader);
    edge_cube(const object_config_t &config);

    void Init(/* args */) override;
};


class object_model : public Object
{
    public:

    object_model(Shader &dshader, Model &dmodel, unsigned int dmode = DRAW_MODEL);
    object_model(const glm::vec3 &pos, Shader &dshader, Model &dmodel, unsigned int dmode = DRAW_MODEL);
    object_model(const glm::vec3 &pos, const glm::vec3 &rot, Shader &dshader, Model &dmodel, unsigned int dmode = DRAW_MODEL);
    object_model(const glm::vec3 &pos, const glm::vec3 &rot, const glm::vec3 &scale, Shader &dshader, Model &dmodel, unsigned int dmode = DRAW_MODEL);
    object_model(const object_config_t &config, Model &dmodel, unsigned int dmode = DRAW_MODEL);

    void Init() override;

};

