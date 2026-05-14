#include "scene.hpp"



FluidSim::Scene::Scene(GLFWwindow *window,const int &screenWidth,const int &screenHeight,
                            ImFont *m_font,ImFont *m_font_mono,ImFont *m_font_mono_off,ImFont *m_icons,
                            GLFWcursor* m_arrowCursor,
                            GLFWcursor* m_dragCursor,
                            GLFWcursor* m_invisible_cursor,
                            ImWchar *m_icons_ranges,
                            ImGuiIO& io,ImGuiStyle& style, ImGuiWindowFlags &m_windowFlags_global_reference,
                            std::shared_ptr<std::vector<unsigned int>> m_global_uniform_binding_indexes): 
                            io_reference(io), global_style_reference(style), windowFlags_global_reference(m_windowFlags_global_reference),
                            InfoShader("../shaders/gui_test/uniforms/pick.vs", "../shaders/gui_test/uniforms/pick.fs"),
                            gridShader("../shaders/gui_test/grid.vs", "../shaders/gui_test/grid2.fs"),
                            OutlineShader("../shaders/gui_test/uniforms/outline_mask.vs", "../shaders/gui_test/uniforms/outline_mask.fs"),
                            DrawOutlineShader("../shaders/gui_test/uniforms/draw_outline.vs", "../shaders/gui_test/uniforms/draw_outline.fs"),
                            fluidsim_sh("../shaders/fluid/fluidsim_sh.vs", "../shaders/fluid/fluidsim_sh.fs"),
                            fluidsim_p_sh("../shaders/fluid/fluidsimp_sh.vs", "../shaders/fluid/fluidsimp_sh.fs"),
                            fluidsim_shade_sh("../shaders/fluid/fluidsim_shade_sh.vs", "../shaders/fluid/fluidsim_shade_sh.fs"),
                            cube_edge_shader("../shaders/fluid/domain_sh.vs", "../shaders/fluid/domain_sh.fs")
{

    main_window = window;
    font = m_font;
    font_mono = m_font_mono;
    font_mono_offset = m_font_mono_off;
    icons = m_icons;

    icons_ranges = m_icons_ranges;

    global_style_copy = style;

    monitorScreenHeight = screenHeight;
    monitorScreenWidth = screenWidth;

    arrowCursor = m_arrowCursor;
    dragCursor = m_dragCursor;
    invisible_cursor = m_invisible_cursor;

    global_uniform_binding_indexes = m_global_uniform_binding_indexes;

    glm::mat4 guizmoview_proj = glm::perspective(glm::radians(140.0f), 1.0f, 0.1f, 1000.0f);
    std::memcpy(imoguizmo_proj, glm::value_ptr(guizmoview_proj), sizeof(float) * 16);

    glGenVertexArrays(1, &dummyVAO);

    int width;
    int height;

    glfwGetWindowSize(window, &width, &height);

    window_viewport_current_width = static_cast<unsigned int>(width/2);
    window_viewport_current_height = static_cast<unsigned int>(height/2);

    /////create ubo

    glGenBuffers(1, &uboViewportCamera);
    glBindBuffer(GL_UNIFORM_BUFFER, uboViewportCamera);
    unsigned long ubuffer_size =  2 * sizeof(glm::mat4);
    glBufferData(GL_UNIFORM_BUFFER, ubuffer_size, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    if (global_uniform_binding_indexes->size() >= 3)
    {   
        ProjViewBindingViewportCamera = global_uniform_binding_indexes->back();

        glBindBufferRange(GL_UNIFORM_BUFFER, ProjViewBindingViewportCamera, uboViewportCamera, 0, 2 * sizeof(glm::mat4));

        global_uniform_binding_indexes->pop_back();
    }
    else 
        throw std::runtime_error("There is no uniform buffer biding availiable!");

    unsigned int shID = OutlineShader.ID;
    glBindBuffer(GL_UNIFORM_BUFFER, uboViewportCamera);
    OutlineShader.use();
    glUniformBlockBinding(shID, glGetUniformBlockIndex(shID, "Matrices_cam"), ProjViewBindingViewportCamera);
    shID = InfoShader.ID;
    InfoShader.use();
    glUniformBlockBinding(shID, glGetUniformBlockIndex(shID, "Matrices_cam"), ProjViewBindingViewportCamera);
    shID = gridShader.ID;
    gridShader.use();
    glUniformBlockBinding(shID, glGetUniformBlockIndex(shID, "Matrices_cam"), ProjViewBindingViewportCamera);
    shID = fluidsim_sh.ID;
    fluidsim_sh.use();
    glUniformBlockBinding(shID, glGetUniformBlockIndex(shID, "Matrices_cam"), ProjViewBindingViewportCamera);
    shID = fluidsim_p_sh.ID;
    fluidsim_p_sh.use();
    glUniformBlockBinding(shID, glGetUniformBlockIndex(shID, "Matrices_cam"), ProjViewBindingViewportCamera);
    shID = fluidsim_shade_sh.ID;
    fluidsim_shade_sh.use();
    glUniformBlockBinding(shID, glGetUniformBlockIndex(shID, "Matrices_cam"), ProjViewBindingViewportCamera);
    shID = cube_edge_shader.ID;
    cube_edge_shader.use();
    glUniformBlockBinding(shID, glGetUniformBlockIndex(shID, "Matrices_cam"), ProjViewBindingViewportCamera);

    DrawOutlineShader.use();
    glUniform1i(glGetUniformLocation(DrawOutlineShader.ID, "bufferMask"), 0);
    DrawOutlineShader.setVec2("viewportSize",glm::vec2(
        static_cast<float>(window_viewport_current_width),
        static_cast<float>(window_viewport_current_height)));


    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    Create_viewport_framebuffer();
    Create_info_framebuffer();
    Create_mask_framebuffer();

    //create lidar and camera context
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    //glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);

    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FluidSim::ParticleGpu), (void*)0);

    glEnableVertexAttribArray(1); // velocity
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FluidSim::ParticleGpu), (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(2); // density
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(FluidSim::ParticleGpu), (void*)(6 * sizeof(float)));

    glEnableVertexAttribArray(3); // boundary
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(FluidSim::ParticleGpu), (void*)(7 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    //setting fluidsim_p instances
    glGenVertexArrays(1, &particle_P_VAO);
    glBindVertexArray(particle_P_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FluidSim::ParticleGpu), (void*)0);

    glEnableVertexAttribArray(1); // velocity
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(FluidSim::ParticleGpu), (void*)(3 * sizeof(float)));

    glEnableVertexAttribArray(2); // density
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(FluidSim::ParticleGpu), (void*)(6 * sizeof(float)));

    glEnableVertexAttribArray(3); // boundary
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(FluidSim::ParticleGpu), (void*)(7 * sizeof(float)));

    glVertexAttribDivisor(0, 1);
    glVertexAttribDivisor(1, 1);
    glVertexAttribDivisor(2, 1);
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);

}

void FluidSim::Scene::InitCameras(const ViewportCamConfig &view_cfg){

    viewport_camera  = std::make_unique<ArcBallCamera>(window_viewport_current_width, window_viewport_current_height,
                        view_cfg.pivot_position, view_cfg.pivot_distance,
                        view_cfg.yaw_pitch.x,view_cfg.yaw_pitch.y);

    viewport_camera->setConstrainAngle(false);

    viewport_camera_projection_matrix = glm::perspective(glm::radians(ViewportCamVfov), (float)window_viewport_current_width / (float)window_viewport_current_height, 0.1f, 100.0f);
    viewport_camera_view_matrix = viewport_camera->GetViewMatrix();

    update_viewport_cam_view_matrix_ubo();
}

void FluidSim::Scene::Init_fluid_solver(const FluidSim::solver_init_conf &init_conf, const simulation_param_conf &param_conf){
    
    fluid_solver = std::make_shared<SPHsolver>
    (
        init_conf.b_conf,
        init_conf.fluid_h_conf,
        init_conf.particle_spacing_conf,
        init_conf.particle_radius_conf,
        init_conf.rest_density_conf,
        init_conf.fluid_Rv_conf
    );

    //fluid_solver->SetSimulationParm(param_conf);
    fluid_solver->ChangeSimulationParm(param_conf);
    fluid_solver->UploadSimulationParm();

    scene_simulation_param = param_conf;

}

void FluidSim::Scene::Upload_fluid_particles(){

    fluid_solver->GPU_buffer_mutex.lock();

    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    glBufferData(GL_ARRAY_BUFFER, fluid_solver->particles_number * sizeof(ParticleGpu), fluid_solver->gpuParticleBuffer.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    fluid_solver->GPU_buffer_mutex.unlock();

}
void FluidSim::Scene::Render_fluid_particles(){

    switch(current_renderfmode_item){
        case 0:
            fluidsim_sh.use();

            glBindVertexArray(particleVAO);
            glBindBuffer(GL_ARRAY_BUFFER, particleVBO);

            glDrawArrays(GL_POINTS, 0, fluid_solver->particles_number);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
            break;
        case 1:
            fluidsim_p_sh.use();
            glBindVertexArray(particle_P_VAO);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, fluid_solver->particles_number);
            glBindVertexArray(0);
            break;
        case 2:
            fluidsim_shade_sh.use();
            glBindVertexArray(particle_P_VAO);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, fluid_solver->particles_number);
            glBindVertexArray(0);
            break;
    }
}

void FluidSim::Scene::SetDomain(std::shared_ptr<cube> domain){

    std::shared_ptr<edge_cube> d_cube = std::make_shared<edge_cube>(cube_edge_shader);
 
    d_cube->Change_position(domain->Get_position());
    d_cube->Change_scale(domain->Get_scale());
    d_cube->Change_rotation(domain->Get_rotation());

    d_cube->SetInfoShader(&InfoShader);
    d_cube->SetOutlineShader(&OutlineShader);

    AddObject(d_cube,"Domain");
    ObjectsVector["Domain"]->auto_render = true;
    ObjectsVector["Domain"]->texture_transform_support = false;
    ObjectsVector["Domain"]->cameras_for_render["viewport_cam"] = false;    

}

void FluidSim::Scene::SetDomain(cube &domain){

    std::shared_ptr<edge_cube> d_cube = std::make_shared<edge_cube>(cube_edge_shader);
 
    d_cube->Change_position(domain.Get_position());
    d_cube->Change_scale(domain.Get_scale());
    d_cube->Change_rotation(domain.Get_rotation());

    d_cube->SetInfoShader(&InfoShader);
    d_cube->SetOutlineShader(&OutlineShader);

    AddObject(d_cube,"Domain");
    ObjectsVector["Domain"]->auto_render = true;
    ObjectsVector["Domain"]->texture_transform_support = false;
    ObjectsVector["Domain"]->cameras_for_render["viewport_cam"] = false;    

}

void FluidSim::Scene::StartFluidThread() {
    bool expected = false;
    if (!fluid_thread_running.compare_exchange_strong(expected, true)) {
        return; // already running
    }
    fluid_solver_thread = std::thread([this]() { FluidSolverLoop(); });
}

void FluidSim::Scene::StopFluidThread() {
    fluid_thread_running.store(false);
    if (fluid_solver_thread.joinable()) {
        fluid_solver_thread.join();
    }
}

void FluidSim::Scene::FluidSolverLoop() {
    while (fluid_thread_running.load()) {
        if(fluid_thread_start.load()){

            float time_window;
            
            // Get current time point
            auto start = std::chrono::high_resolution_clock::now();

            #ifdef USE_XPBF

            fluid_solver->XPBF_solve(time_window);

            #elifdef USE_VISCOEL

            fluid_solver->Viscoelastic_solve(time_window);

            #else

            //fluid_solver->PBF_solve(time_window);
            fluid_solver->Kens_solve(time_window);

            #endif


            // Get the end time point
            auto end = std::chrono::high_resolution_clock::now();

            // Calculate the duration in microseconds
            int64_t duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

            int64_t remaining_time = static_cast<int64_t>(time_window*1000000.0f) - duration;

            if(remaining_time <= 0) {
                //std::cout << "time old: " <<static_cast<int64_t>(duration*1000000.0f)<<"\n";
                simulation_time.store(duration);
                continue;
            }
            else {
                std::this_thread::sleep_for(std::chrono::microseconds(remaining_time));
                end = std::chrono::high_resolution_clock::now();
                duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                //std::cout << "time: " <<static_cast<int64_t>(duration*1000000.0f)<<"\n";
                simulation_time.store(duration);
            }

        }else std::this_thread::sleep_for(std::chrono::milliseconds(10)); // prevent 100% CPU spin
    }
}

void FluidSim::Scene::AddShader(const shaderConfig &shader_cfg){

    switch(shader_cfg.type){
        case FluidSim::shader_type::DEFAULT:
            ShadersVector[shader_cfg.shader_name] = std::make_shared<Shader>(shader_cfg.vertex_path.c_str(),shader_cfg.fragment_path.c_str());
            break;
        case FluidSim::shader_type::WTH_GEOMETRY:
            ShadersVector[shader_cfg.shader_name] = std::make_shared<Shader>(shader_cfg.vertex_path.c_str(),shader_cfg.geometry_path.c_str(),shader_cfg.fragment_path.c_str());
            break;
        case FluidSim::shader_type::COMPUTE:
            ComputeShadersVector[shader_cfg.shader_name] = std::make_shared<Shader_compute>(shader_cfg.compute_path.c_str());
            break;
    }
}

void FluidSim::Scene::RemoveShader(const std::string &shader_name){

    ShadersVector.erase(shader_name);

}

void FluidSim::Scene::AddModel(const std::string &model_path, const std::string &model_name){

    ModelsVector[model_name] = std::make_shared<Model>(model_path);

}

void FluidSim::Scene::RemoveModel(const std::string &model_name){

    ModelsVector.erase(model_name);

}

void FluidSim::Scene::AddObject(std::shared_ptr<Object> object_ptr,const std::string &object_name){
    ObjectsVector[object_name] = std::make_shared<sceneObject>();
    ObjectsVector[object_name]->object = object_ptr;
}

void FluidSim::Scene::RemoveObject(const std::string &object_name){
    ObjectsVector.erase(object_name);
}

void FluidSim::Scene::InitFloor(const glm::vec3 &position, const glm::vec3 &scale){

    FluidSim::shaderConfig floor_shader_cfg;
    floor_shader_cfg.type = FluidSim::shader_type::DEFAULT;
    floor_shader_cfg.vertex_path = "../shaders/fluid/floor.vs";
    floor_shader_cfg.fragment_path =  "../shaders/fluid/floor.fs";
    floor_shader_cfg.shader_name = "floorShader";
    AddShader(floor_shader_cfg);


    std::shared_ptr<plane> s_floor = std::make_shared<plane>(position,
                                                            *(main_scene_class->ShadersVector["floorShader"].get()),
                                                            DRAW_UNTEXTURED);
    s_floor->SetInfoShader(&main_scene_class->InfoShader);
    s_floor->SetOutlineShader(&main_scene_class->OutlineShader);
    s_floor->SetDrawInfoMode(DRAW_INFO);
    s_floor->SetDrawOutlineMode(DRAW_OUTLINE);
    s_floor->Change_scale(scale);

    main_scene_class->AddObject(s_floor,"Floor");
    //main_scene_class->ObjectsVector["scene_floor"]->AddTexture(scene_cube_texture,"scene_floor_texture");
    main_scene_class->ObjectsVector["Floor"]->auto_render = true;
    main_scene_class->ObjectsVector["Floor"]->texture_transform_support = false;
    main_scene_class->ObjectsVector["Floor"]->cameras_for_render["viewport_cam"] = true;

    floor_config = {
        .floor_size = glm::vec2(scale.z,scale.x),
        .tileCol1 = glm::vec3(0.204f,0.318f,0.776f),
        .tileCol2 = glm::vec3(0.608f,0.369f,0.858f),
        .tileCol3 = glm::vec3(0.302f,0.736f,0.458f),
        .tileCol4 = glm::vec3(0.802f,0.643f,0.367f),
        .tileColVariation = glm::vec3(0.33f,0.0f,0.47f),
        .tileDarkOffset = -0.35f,
        .tileSize = 1.0f,
        .view_floor = true
    };

    ShadersVector["floorShader"]->use();
    ShadersVector["floorShader"]->setVec2("floor_dimension",floor_config.floor_size);
    ShadersVector["floorShader"]->setVec3("tileCol1",floor_config.tileCol1);
    ShadersVector["floorShader"]->setVec3("tileCol2",floor_config.tileCol2);
    ShadersVector["floorShader"]->setVec3("tileCol3",floor_config.tileCol3);
    ShadersVector["floorShader"]->setVec3("tileCol4",floor_config.tileCol4);
    ShadersVector["floorShader"]->setVec3("tileColVariation",floor_config.tileColVariation);
    ShadersVector["floorShader"]->setFloat("tileDarkOffset",floor_config.tileDarkOffset);
    ShadersVector["floorShader"]->setFloat("tileScale",floor_config.tileSize);



}

void FluidSim::Scene::Verify_clicked_object(){
    
    
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left,true) && scene_hovered && !isUsingGuizmo){
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_info_framebuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0);

        unsigned int Pixel[3];
        ImVec2 mouse_position  = ImGui::GetMousePos();
        mouse_position.x -= ViewportPos.x;
        mouse_position.y -= ViewportPos.y;
        mouse_position.y = (ViewportSize.y - 1) - mouse_position.y;
        glReadPixels(static_cast<unsigned int>(mouse_position.x),static_cast<unsigned int>(mouse_position.y),1,1,GL_RGB_INTEGER,GL_UNSIGNED_INT,&Pixel);

        //std::cout << "("<<Pixel[0]<<","<<Pixel[1]<<","<<Pixel[2]<<")" << std::endl;

        if(Pixel[0] == 0) {
            object_selected = "";
        }
        else {
            auto it = ObjectsVector.begin();
            std::advance(it, Pixel[0] - 1);
            object_selected  = it->first;
        }

        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    }
}


void FluidSim::Scene::draw_outline(){

    if(object_selected != ""){
        if(ObjectsVector[object_selected]->cameras_for_render["viewport_cam"]){
            glBindFramebuffer(GL_FRAMEBUFFER, m_mask_framebuffer);
            glViewport(0, 0, static_cast<unsigned int>(window_viewport_current_width), static_cast<unsigned int>(window_viewport_current_height));
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClear(GL_COLOR_BUFFER_BIT);
            //glEnable(GL_DEPTH_TEST);
            glBindBuffer(GL_UNIFORM_BUFFER, uboViewportCamera);
            //glEnable(GL_STENCIL_TEST);
            //glStencilFunc(GL_NOTEQUAL, 0x01, 0xFF);
            //glStencilMask(0x00); 
            glDisable(GL_DEPTH_TEST);
            //float distance = glm::length(ObjectsVector[object_selected]->object->position - viewport_camera->m_pos);
            ObjectsVector[object_selected]->object->SetOutlineTransform(); 
            //OutlineShader.setFloat("distant",distance);
            if(ObjectsVector[object_selected]->object->object_type == ObjTypes::Object_types::Plane){

                std::shared_ptr<plane> PlaneObj = std::static_pointer_cast<plane>(ObjectsVector[object_selected]->object);
                if(PlaneObj->back_face_culling)
                    PlaneObj->DrawOutline();
                else{
                    glDisable(GL_CULL_FACE);
                    PlaneObj->DrawOutline();
                    glEnable(GL_CULL_FACE);
                }

            }
            else ObjectsVector[object_selected]->object->DrawOutline();
            //else if(ObjectsVector[object_selected]->object->object_type != ObjTypes::Object_types::EdgeCube) ObjectsVector[object_selected]->object->DrawOutline();
            //glStencilMask(0xFF);
            //glStencilFunc(GL_ALWAYS, 0, 0xFF);
            //glDisable(GL_STENCIL_TEST);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);

            glBindFramebuffer(GL_FRAMEBUFFER, viewport_framebuffer);     

            glViewport(0, 0, static_cast<unsigned int>(window_viewport_current_width), static_cast<unsigned int>(window_viewport_current_height));

            //glDisable(GL_DEPTH_TEST);
            //glDepthMask(GL_TRUE);
            glDisable(GL_CULL_FACE);

            //draw

            DrawOutlineShader.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_mask_texture);

            //glActiveTexture(GL_TEXTURE1);
            //glUniform1i(glGetUniformLocation(DrawOutlineShader.ID, "bufferDepthMask"), 1);
            //glBindTexture(GL_TEXTURE_2D, m_mask_depth_texture);
            ////glTexParameteri(GL_TEXTURE_2D,
            ////        GL_TEXTURE_COMPARE_MODE,
            ////        GL_NONE);

            //glActiveTexture(GL_TEXTURE2);
            //glUniform1i(glGetUniformLocation(DrawOutlineShader.ID, "bufferDepth"), 2);
            //glBindTexture(GL_TEXTURE_2D, viewport_rbo);
            ////glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);

            glBindVertexArray(dummyVAO);
            glDrawArrays(GL_TRIANGLES,0,6);

            glBindVertexArray(0);

            glActiveTexture(GL_TEXTURE0);

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
        }
        else{
            object_selected = "";
        }

    }

    
}

void FluidSim::Scene::draw_grid(){

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);   
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    glBindBuffer(GL_UNIFORM_BUFFER, uboViewportCamera);

    gridShader.use();
    //glUniformBlockBinding(gridShader.ID, glGetUniformBlockIndex(gridShader.ID, "Matrices_cam"), ProjViewBindingViewportCamera);
    gridShader.setVec3("cameraPos",viewport_camera->m_pos);
    glBindVertexArray(dummyVAO);
    glDrawArrays(GL_TRIANGLES,0,6);
    
    glEnable(GL_CULL_FACE);

    glBindVertexArray(0);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void FluidSim::Scene::RenderInfoBuffer(){

    //rendering picking buffer

    glViewport(0, 0, window_viewport_current_width, window_viewport_current_height);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_info_framebuffer);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBindBuffer(GL_UNIFORM_BUFFER, uboViewportCamera);

    unsigned int index = 1;
    for (auto it = ObjectsVector.begin(); it != ObjectsVector.end(); it++) {

        if(it->second->cameras_for_render.find("viewport_cam") != it->second->cameras_for_render.end()){
            if(it->second->cameras_for_render["viewport_cam"]){
                //glm::mat4 view_matrix = camera.GetViewMatrix();
                //glm::mat4 projection;

                it->second->object->SetInfoTransform();

                switch (it->second->object->object_type)
                {
                case ObjTypes::Object_types::Plane:{
                    std::shared_ptr<plane> PlaneObj = std::static_pointer_cast<plane>(it->second->object);
                    if(PlaneObj->back_face_culling)
                        PlaneObj->DrawInfo(index);
                    else{
                        glDisable(GL_CULL_FACE);
                        PlaneObj->DrawInfo(index);
                        glEnable(GL_CULL_FACE);
                    }

                    break;
                }
                case ObjTypes::Object_types::Cube:
                    std::static_pointer_cast<cube>(it->second->object)->DrawInfo(index);
                    break;
                case ObjTypes::Object_types::Sphere:
                    std::static_pointer_cast<sphere>(it->second->object)->DrawInfo(index);
                    break;
                case ObjTypes::Object_types::Mesh:
                    std::static_pointer_cast<object_model>(it->second->object)->DrawInfo(index);
                    break;
                case ObjTypes::Object_types::EdgeCube:
                    std::static_pointer_cast<edge_cube>(it->second->object)->DrawInfo(index);
                break;
                }
            }
        }
        ++index; // Increment the index
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void FluidSim::Scene::RenderStartMainViewport(){
        glBindFramebuffer(GL_FRAMEBUFFER, viewport_framebuffer);

        glViewport(0, 0, window_viewport_current_width , window_viewport_current_height);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        //glEnable(GL_DEPTH_TEST); // enable depth testing (is disabled for rendering screen-space quad)
        // render
        // ------
        glClearColor(background_scene_color.x, background_scene_color.y, background_scene_color.z, background_scene_color.w);
        //glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

        //glEnable(GL_STENCIL_TEST);
        glDisable(GL_STENCIL_TEST);
        //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        //stencil
        //glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        //glStencilFunc(GL_ALWAYS, 0x01, 0xFF); 
        //glStencilMask(0xFF); 

        glBindBuffer(GL_UNIFORM_BUFFER, uboViewportCamera);

}

void FluidSim::Scene::RenderEndMainViewport(){
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FluidSim::Scene::RenderMainViewportAuto(){



        for (auto it = ObjectsVector.begin(); it != ObjectsVector.end(); it++) {

            if(it->second->cameras_for_render.find("viewport_cam") != it->second->cameras_for_render.end() && it->second->auto_render){
                if(it->second->cameras_for_render["viewport_cam"]){

                    //glDisable(GL_STENCIL_TEST);

                    //if(it->first == object_selected) glEnable(GL_STENCIL_TEST);

                    
                    it->second->object->SetViewProjUBuffer(ProjViewBindingViewportCamera);
                    it->second->object->SetTransform();
                    //glm::mat4 *model_matrix;
                    //glm::mat3 *model_matrix_tex;

                    if(it->second->texture_transform_support){
                        it->second->object->SetTextureTransform();
                    }

                    switch (it->second->object->object_type)
                    {
                    case ObjTypes::Object_types::Plane:{
                        std::shared_ptr<plane> PlaneObj = std::static_pointer_cast<plane>(it->second->object);
                        if(PlaneObj->back_face_culling)
                            PlaneObj->Draw();
                        else{
                            glDisable(GL_CULL_FACE);
                            PlaneObj->Draw();
                            glEnable(GL_CULL_FACE);
                        }

                        break;
                    }
                    case ObjTypes::Object_types::Cube:
                        std::static_pointer_cast<cube>(it->second->object)->Draw();
                        break;
                    case ObjTypes::Object_types::Sphere:
                        std::static_pointer_cast<sphere>(it->second->object)->Draw();
                        break;
                    case ObjTypes::Object_types::Mesh:
                        std::static_pointer_cast<object_model>(it->second->object)->Draw();
                        //model_matrix = &(std::static_pointer_cast<object_model>(it->second->object))->model_m;
                        //model_matrix_tex = &(std::static_pointer_cast<object_model>(it->second->object))->model_m_tex;
                        //glBufferSubData(GL_UNIFORM_BUFFER, 0,sizeof(glm::mat4), glm::value_ptr(*model_matrix));
                        break;
                    case ObjTypes::Object_types::EdgeCube:
                        if(object_selected == "Domain") cube_edge_shader.setBool("selected",true);
                        else cube_edge_shader.setBool("selected",false);
                        std::static_pointer_cast<edge_cube>(it->second->object)->Draw();
                        break;
                    }


                    //glBufferSubData(GL_UNIFORM_BUFFER, 2*sizeof(glm::mat4),sizeof(float), &near);
                    //glBufferSubData(GL_UNIFORM_BUFFER, 2*sizeof(glm::mat4) + sizeof(float),sizeof(float), &far);
                
                }
                    
            }
        }
        //glDisable(GL_STENCIL_TEST); 

}


void FluidSim::Scene::update_viewport_cam_view_matrix_ubo(){
        glBindBuffer(GL_UNIFORM_BUFFER, uboViewportCamera);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(viewport_camera_view_matrix));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(viewport_camera_projection_matrix));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

}

void FluidSim::Scene::update_viewport_cam_projection_matrix_ubo(){
        glBindBuffer(GL_UNIFORM_BUFFER, uboViewportCamera);
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(viewport_camera_projection_matrix));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

}

void FluidSim::Scene::Create_viewport_framebuffer()
{   

    // framebuffer configuration
    // -------------------------
    glGenFramebuffers(1, &viewport_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, viewport_framebuffer);
    // create a color attachment texture
    glGenTextures(1, &viewport_textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, viewport_textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, window_viewport_current_width, window_viewport_current_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, viewport_textureColorbuffer, 0);


    // create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
    glGenRenderbuffers(1, &viewport_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, viewport_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, window_viewport_current_width, window_viewport_current_height); // use a single renderbuffer object for both a depth AND stencil buffer.
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, viewport_rbo); // now actually attach it

    //glGenTextures(1, &viewport_rbo);
    //glBindTexture(GL_TEXTURE_2D, viewport_rbo);

    //glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, window_viewport_current_width, window_viewport_current_height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);

    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, viewport_rbo, 0);

    // now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    previousImGuiSceneWindowSize = ImVec2(window_viewport_current_width, window_viewport_current_height);
}

// and we rescale the buffer, so we're able to resize the window
void FluidSim::Scene::Rescale_viewport_framebuffer(int width, int height)
{
	glBindTexture(GL_TEXTURE_2D, viewport_textureColorbuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, viewport_textureColorbuffer, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, viewport_rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, viewport_rbo);
}

void FluidSim::Scene::Create_info_framebuffer()
{   

    // framebuffer configuration
    // -------------------------
    glGenFramebuffers(1, &m_info_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_info_framebuffer);
    // create a color attachment texture
    glGenTextures(1, &m_picking_texture);
    glBindTexture(GL_TEXTURE_2D, m_picking_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32UI, window_viewport_current_width, window_viewport_current_height, 0, GL_RGB_INTEGER, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_picking_texture, 0);
    // create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
    //glGenTextures(1, &m_picking_depth_tex);
    //glBindTexture(GL_TEXTURE_2D, m_picking_depth_tex);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, window_viewport_current_width, window_viewport_current_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    //glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,m_picking_depth_tex,0);

    glGenRenderbuffers(1, &m_picking_depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_picking_depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, window_viewport_current_width, window_viewport_current_height); // use a single renderbuffer object for both a depth AND stencil buffer.
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_picking_depth_rbo); // now actually attach it

    // now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    previousPickingImGuiSceneWindowSize = ImVec2(window_viewport_current_width, window_viewport_current_height);
}


void FluidSim::Scene::Rescale_info_framebuffer(int width, int height)
{
    glBindTexture(GL_TEXTURE_2D, m_picking_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32UI, width, height, 0, GL_RGB_INTEGER, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_picking_texture, 0);

    //glBindTexture(GL_TEXTURE_2D, m_picking_depth_tex);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    //glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,m_picking_depth_tex,0);


	glBindRenderbuffer(GL_RENDERBUFFER, m_picking_depth_rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_picking_depth_rbo);
}

void FluidSim::Scene::Create_mask_framebuffer()
{   

    // framebuffer configuration
    // -------------------------
    glGenFramebuffers(1, &m_mask_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_mask_framebuffer);
    // create a color attachment texture
    glGenTextures(1, &m_mask_texture);
    glBindTexture(GL_TEXTURE_2D, m_mask_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, window_viewport_current_width, window_viewport_current_height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_mask_texture, 0);

    //glGenTextures(1, &m_mask_depth_texture);
    //glBindTexture(GL_TEXTURE_2D, m_mask_depth_texture);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, window_viewport_current_width, window_viewport_current_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    //glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,m_mask_depth_texture,0);

    // now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //previousPickingImGuiSceneWindowSize = ImVec2(window_viewport_current_width, window_viewport_current_height);
}


void FluidSim::Scene::Rescale_mask_framebuffer(int width, int height)
{
    glBindTexture(GL_TEXTURE_2D, m_mask_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_mask_texture, 0);

    //glBindTexture(GL_TEXTURE_2D, m_mask_depth_texture);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    //glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,m_mask_depth_texture,0);

}


void FluidSim::Scene::draw_guizmo_toolbar(){
    ImGui::PushFont(icons);
    ImVec2 toolbarPos = ImVec2(ViewportMin.x + 10, ViewportMin.y + 10); // 10 px padding
    ImGui::SetCursorScreenPos(toolbarPos);

    // Begin a transparent child window for the button bar
    ImGuiWindowFlags toolbarFlagsWindow =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        windowFlags_global_reference;

    ImGuiChildFlags toolbarFlagsChild = 
        ImGuiChildFlags_AlwaysAutoResize | 
        ImGuiChildFlags_AutoResizeX | 
        ImGuiChildFlags_AutoResizeY;


    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f)); 
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);    

    float button_size = static_cast<float>(monitorScreenHeight)*0.03;

    ImGui::BeginChild("GizmoToolbar", ImVec2(3*button_size +4*2, button_size), toolbarFlagsChild, toolbarFlagsWindow);


    //global_style_reference.Colors[ImGuiCol_Button] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f); // Default button color
    //global_style_reference.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.75f, 0.75f, 0.75f, 1.0f); // Hover color
    //global_style_reference.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Active color

    ImVec4 default_color_def = ImVec4(0.85f, 0.85f, 0.85f, 1.0f); // Default button color
    ImVec4 default_color_hover = ImVec4(0.75f, 0.75f, 0.75f, 1.0f); // Hover color

    ImVec4 selected_color_def = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // selected Default button color
    ImVec4 selected_color_hover = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); //selected Hover color

    if(ImGui::InvisibleButton("##hiddenb0", ImVec2(button_size, button_size))){
        currentTransformOp = ImGuizmo::TRANSLATE;
        button_state_vector[0] = true;
        button_state_vector[1] = false;
        button_state_vector[2] = false;
    }
    ImGui::SetCursorScreenPos(toolbarPos);

    bool active = ImGui::IsItemActive();
    bool hovered = ImGui::IsItemHovered();

    if(button_state_vector[0]){
        global_style_reference.Colors[ImGuiCol_Button] = selected_color_def; // Default button color
        //global_style_reference.Colors[ImGuiCol_ButtonHovered] = selected_color_hover; // Hover color
    }else if(hovered && !active){
        global_style_reference.Colors[ImGuiCol_Button] = default_color_hover; // Default button color
    }else if(hovered && active){
        global_style_reference.Colors[ImGuiCol_Button] = selected_color_def;
    }else{
        global_style_reference.Colors[ImGuiCol_Button] = default_color_def;
    }

    ImGui::Button("\ue900", ImVec2(button_size, button_size));
        
    ImGui::SameLine();

    ImVec2 button_pos  = ImGui::GetCursorScreenPos();
    
    if(ImGui::InvisibleButton("##hiddenb1", ImVec2(button_size, button_size))){
        currentTransformOp = ImGuizmo::ROTATE;
        button_state_vector[0] = false;
        button_state_vector[1] = true;
        button_state_vector[2] = false;
    }
    ImGui::SetCursorScreenPos(button_pos);

    active = ImGui::IsItemActive();
    hovered = ImGui::IsItemHovered();

    if(button_state_vector[1]){
        global_style_reference.Colors[ImGuiCol_Button] = selected_color_def; // Default button color
        //global_style_reference.Colors[ImGuiCol_ButtonHovered] = selected_color_hover; // Hover color
    }else if(hovered && !active){
        global_style_reference.Colors[ImGuiCol_Button] = default_color_hover; // Default button color
    }else if(hovered && active){
        global_style_reference.Colors[ImGuiCol_Button] = selected_color_def;
    }else{
        global_style_reference.Colors[ImGuiCol_Button] = default_color_def;
    }

    ImGui::Button("\ue901", ImVec2(button_size, button_size));

    ImGui::SameLine();

    button_pos  = ImGui::GetCursorScreenPos();
    
    if(ImGui::InvisibleButton("##hiddenb2", ImVec2(button_size, button_size))){
        currentTransformOp = ImGuizmo::SCALE;
        button_state_vector[0] = false;
        button_state_vector[1] = false;
        button_state_vector[2] = true;
    }
    ImGui::SetCursorScreenPos(button_pos);

    active = ImGui::IsItemActive();
    hovered = ImGui::IsItemHovered();

    if(button_state_vector[2]){
        global_style_reference.Colors[ImGuiCol_Button] = selected_color_def; // Default button color
        //global_style_reference.Colors[ImGuiCol_ButtonHovered] = selected_color_hover; // Hover color
    }else if(hovered && !active){
        global_style_reference.Colors[ImGuiCol_Button] = default_color_hover; // Default button color
    }else if(hovered && active){
        global_style_reference.Colors[ImGuiCol_Button] = selected_color_def;
    }else{
        global_style_reference.Colors[ImGuiCol_Button] = default_color_def;
    }

    ImGui::Button("\ue902", ImVec2(button_size, button_size));

    global_style_reference.Colors[ImGuiCol_Button] = default_color_def; // Default button color
    global_style_reference.Colors[ImGuiCol_ButtonHovered] = default_color_hover; // Hover color
    global_style_reference.Colors[ImGuiCol_ButtonActive] = selected_color_def; // Active color

    ImGui::EndChild();


    toolbarPos = ImVec2(ViewportMin.x + 20 + 4*button_size, ViewportMin.y + 10); // 10 px padding
    ImGui::SetCursorScreenPos(toolbarPos);

    ImGui::BeginChild("world_transform_fly_mode", ImVec2(2*button_size+3*2, button_size), toolbarFlagsChild, toolbarFlagsWindow);

    if(ImGui::InvisibleButton("##hiddenb6", ImVec2(button_size, button_size))){
        button_state_vector[3] = !button_state_vector[3];
        currentTransformMode = (button_state_vector[3])?ImGuizmo::WORLD:ImGuizmo::LOCAL;
    }
    ImGui::SetCursorScreenPos(toolbarPos);

    active = ImGui::IsItemActive();
    hovered = ImGui::IsItemHovered();

    if(button_state_vector[3]){
        global_style_reference.Colors[ImGuiCol_Button] = selected_color_def; // Default button color
        //global_style_reference.Colors[ImGuiCol_ButtonHovered] = selected_color_hover; // Hover color
    }else if(hovered && !active){
        global_style_reference.Colors[ImGuiCol_Button] = default_color_hover; // Default button color
    }else if(hovered && active){
        global_style_reference.Colors[ImGuiCol_Button] = selected_color_def;
    }else{
        global_style_reference.Colors[ImGuiCol_Button] = default_color_def;
    }
    
    ImGui::Button("\ue906", ImVec2(button_size, button_size));

    ImGui::SameLine();

    button_pos  = ImGui::GetCursorScreenPos();

    if(ImGui::InvisibleButton("##hiddenbfly", ImVec2(button_size, button_size))){
        button_state_vector[4] = !button_state_vector[4];
        viewport_camera->fly_mode = button_state_vector[4];
    }
    ImGui::SetCursorScreenPos(button_pos);

    active = ImGui::IsItemActive();
    hovered = ImGui::IsItemHovered();

    if(button_state_vector[4]){
        global_style_reference.Colors[ImGuiCol_Button] = selected_color_def; // Default button color
        //global_style_reference.Colors[ImGuiCol_ButtonHovered] = selected_color_hover; // Hover color
    }else if(hovered && !active){
        global_style_reference.Colors[ImGuiCol_Button] = default_color_hover; // Default button color
    }else if(hovered && active){
        global_style_reference.Colors[ImGuiCol_Button] = selected_color_def;
    }else{
        global_style_reference.Colors[ImGuiCol_Button] = default_color_def;
    }
    
    ImGui::Button("\ue909", ImVec2(button_size, button_size));

    global_style_reference.Colors[ImGuiCol_Button] = default_color_def; // Default button color
    global_style_reference.Colors[ImGuiCol_ButtonHovered] = default_color_hover; // Hover color
    global_style_reference.Colors[ImGuiCol_ButtonActive] = selected_color_def; // Active color

    ImGui::EndChild();

    toolbarPos = ImVec2(ViewportMin.x + 22 + 8*button_size, ViewportMin.y + 10); // 10 px padding
    ImGui::SetCursorScreenPos(toolbarPos);

    ImGui::BeginChild("camera_mode", ImVec2(button_size, button_size), toolbarFlagsChild, toolbarFlagsWindow);

    if(button_state_vector[5]){
        if (ImGui::Button("\ue904", ImVec2(button_size, button_size)) || want_to_toggle_proj){
            button_state_vector[5] = !button_state_vector[5];
            want_to_toggle_proj = false;

        }
    }else{
        if (ImGui::Button("\ue905", ImVec2(button_size, button_size)) || want_to_toggle_proj){
            button_state_vector[5] = !button_state_vector[5];
            want_to_toggle_proj = false;
            viewport_camera_projection_matrix = glm::perspective(glm::radians(ViewportCamVfov), 
                (float)window_viewport_current_width / (float)window_viewport_current_height, 
                0.1f, 100.0f);
        }
    }
    
    ImGui::EndChild();

    toolbarPos = ImVec2(ViewportMin.x + 20 + 9.3*button_size, ViewportMin.y + 10); // 10 px padding
    ImGui::SetCursorScreenPos(toolbarPos);
    ImGui::PopFont();

    ImGui::BeginChild("perspective_angle", ImVec2(2.6f*button_size,button_size), toolbarFlagsChild, toolbarFlagsWindow);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 7));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(default_color_def.x,default_color_def.y,default_color_def.z,0.6f)); // normal bg
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(default_color_hover.x,default_color_hover.y,default_color_hover.z, 0.6f)); // hover bg
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(selected_color_def.x,selected_color_def.y,selected_color_def.z, 0.6f)); // active bg;
    ImGui::PushStyleColor(ImGuiCol_Text,           ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    //global_style_reference.Colors[ImGuiCol_Button] = ImVec4(0.85f, 0.85f, 0.85f, 1.0f); // Default button color
    //global_style_reference.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.75f, 0.75f, 0.75f, 1.0f); // Hover color
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
    ImGui::PushItemWidth(2.6f*button_size);

    float vfov_tmp = ViewportCamVfov;

    ImGui::PushFont(font_mono_offset);
    ImGui::DragFloat("##DragViewFov", &vfov_tmp,0.1f,0.1f, 89.0f, "\ue907:%.1f°",ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopFont();

    if(ImGui::IsItemActive()){
        if (ImGui::TempInputIsActive(ImGui::GetItemID())) {
            ViewportCamVfovTemp = vfov_tmp;
            last_frame_view_fov_input_txt_active = true;

        }else if(ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            ViewportCamVfov = std::clamp(vfov_tmp, 0.1f, 89.0f);
            last_frame_view_fov_input_txt_active = false;
            if(button_state_vector[5])
                viewport_camera_projection_matrix = glm::perspective(glm::radians(ViewportCamVfov), 
                    (float)window_viewport_current_width / (float)window_viewport_current_height, 
                    0.1f, 100.0f);
            
        }else last_frame_view_fov_input_txt_active = false;
        //std::cout<<"changed"<<std::endl;

    }else if(last_frame_view_fov_input_txt_active){

        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)){
            ViewportCamVfov = std::clamp(ViewportCamVfovTemp, 0.1f, 89.0f);
            if(button_state_vector[5])
                viewport_camera_projection_matrix = glm::perspective(glm::radians(ViewportCamVfov), 
                    (float)window_viewport_current_width / (float)window_viewport_current_height, 
                    0.1f, 100.0f);
        }
        last_frame_view_fov_input_txt_active = false;
    }
    
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    ImGui::PopItemWidth();

    ImGui::EndChild();


    global_style_reference.Colors[ImGuiCol_Button] = global_style_copy.Colors[ImGuiCol_Button];
    global_style_reference.Colors[ImGuiCol_ButtonHovered] = global_style_copy.Colors[ImGuiCol_ButtonHovered];
    global_style_reference.Colors[ImGuiCol_ButtonActive] = global_style_copy.Colors[ImGuiCol_ButtonActive];

    ImGui::PopStyleVar(6);

    //ImGui::PopFont();
}

void FluidSim::Scene::draw_imoguizmo(){

    ImOGuizmo::config.axisLengthScale = 1.0f;
    const float square_size = 120.0f;
    // specify position and size of gizmo (and its window when using ImOGuizmo::BeginFrame())
    ImOGuizmo::SetRect(ViewportPos.x +ViewportSize.x-square_size -5 /* x */, ViewportPos.y + 5 /* y */, square_size /* square size */);
    //ImOGuizmo::BeginFrame(); // to use you own window remove this call 
    // and wrap everything in between ImGui::Begin() and ImGui::End() instead

    float viewMatrix[16];
    viewport_camera_view_matrix = viewport_camera->GetViewMatrix();
    memcpy(viewMatrix, glm::value_ptr(viewport_camera_view_matrix), sizeof(float) * 16);
    // optional: set distance to pivot (-> activates interaction)
    if(ImOGuizmo::DrawGizmo(viewMatrix, imoguizmo_proj, viewport_camera->m_pivot_distance /* optional: default = 0.0f */))
    {   
    	// in case of user interaction viewMatrix gets updated
        glm::vec3 right_v(viewMatrix[0],viewMatrix[4],viewMatrix[8]);
        glm::vec3 up_v(viewMatrix[1],viewMatrix[5],viewMatrix[9]);
        glm::vec3 front_v(-viewMatrix[2],-viewMatrix[6],-viewMatrix[10]);
        viewport_camera->SetVectors(right_v,up_v,front_v);
        viewport_camera_view_matrix = viewport_camera->GetViewMatrix();
    }

}

void FluidSim::Scene::draw_viewport(){

    global_style_reference.WindowPadding = ImVec2(0.0f, 0.0f);
    global_style_reference.WindowBorderSize = 0.0f;  

    ImGui::Begin("Viewport",nullptr,ImGuiWindowFlags_None | windowFlags_global_reference);

    //bool scene_focused = ImGui::IsWindowFocused();
    scene_hovered = ImGui::IsWindowHovered() && glfwGetWindowAttrib(main_window, GLFW_HOVERED);  
    if (scene_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
    {
        ImGui::SetNextWindowFocus();
    }   
    // we access the ImGui window size
    ViewportSize = ImGui::GetContentRegionAvail();
    ViewportMin = ImGui::GetCursorScreenPos();
    ViewportMax = ImVec2(ViewportMin.x + ViewportSize.x,ViewportMin.y + ViewportSize.y);

    ViewportPos = ViewportMin;
    // Here we can render into the ImGui window
    // ImGui Buttons, Drop Downs, etc. and later our framebuffer    
    // Check if the ImGui window size has changed
    if (ViewportSize.x != previousImGuiSceneWindowSize.x || ViewportSize.y != previousImGuiSceneWindowSize.y) {
        // Update framebuffer size
        window_viewport_current_width = static_cast<unsigned int>(ViewportSize.x);
        window_viewport_current_height = static_cast<unsigned int>(ViewportSize.y);
        // we rescale the framebuffer to the actual window size here and reset the glViewport 
        Rescale_viewport_framebuffer(static_cast<int>(window_viewport_current_width), 
                                    static_cast<int>(window_viewport_current_height));
        Rescale_info_framebuffer(static_cast<int>(window_viewport_current_width), 
                                static_cast<int>(window_viewport_current_height));
        Rescale_mask_framebuffer(static_cast<int>(window_viewport_current_width), 
                                static_cast<int>(window_viewport_current_height));
                                
        viewport_camera->ScreenResize(window_viewport_current_width,window_viewport_current_height);

        DrawOutlineShader.use();
        DrawOutlineShader.setVec2("viewportSize",glm::vec2(ViewportSize.x,ViewportSize.y));

        if(button_state_vector[5])
            viewport_camera_projection_matrix = glm::perspective(glm::radians(ViewportCamVfov), (float)window_viewport_current_width / (float)window_viewport_current_height, 0.1f, 100.0f);
        previousImGuiSceneWindowSize = ViewportSize;
    }   
    // and here we can add our created texture as image to ImGui
    // unfortunately we need to use the cast to void* or I didn't find another way tbh
    //ImGui::Image((ImTextureID)textureColorbuffer, ImVec2(framebufferWidth, framebufferHeight));   
    //ImGui::SetCursorPos(ViewportMin);
    // Draw the texture (OpenGL)
    ImGui::GetWindowDrawList()->AddImage(
        (ImTextureID)(intptr_t)viewport_textureColorbuffer,
        ViewportMin,
        ImVec2(ViewportMin.x + ViewportSize.x, ViewportMin.y + ViewportSize.y),
        ImVec2(0, 1), // UVs
        ImVec2(1, 0)  // Flip vertically for OpenGL
    );  
    // Use your camera/view/proj matrices
    draw_imoguizmo();  
    // Tell ImGuizmo where the viewport is  
    draw_guizmo_toolbar(); 

    ImGuizmo::BeginFrame();
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetOrthographic(!button_state_vector[5]);
    ImGuizmo::SetRect(ViewportPos.x, ViewportPos.y, ViewportSize.x, ViewportSize.y);

    float viewMatrix[16];
    float projMatrix[16];
    float modelMatrix[16];
    float deltaMatrix[16];  
    float translation[3],rotation[3],scale[3];  
    memcpy(viewMatrix, glm::value_ptr(viewport_camera_view_matrix), sizeof(float) * 16);
    memcpy(projMatrix, glm::value_ptr(viewport_camera_projection_matrix), sizeof(float) * 16); 

    //ImGuizmo::Manipulate(viewMatrix, projMatrix,ImGuizmo::TRANSLATE, ImGuizmo::LOCAL, modelMatrix,deltaMatrix);
    if(object_selected != ""){
        
        memcpy(modelMatrix, glm::value_ptr(ObjectsVector[object_selected]->object->model_m), sizeof(float) * 16);
        ImGuizmo::ManipulateForward(viewMatrix, projMatrix,currentTransformOp, currentTransformMode, modelMatrix,deltaMatrix);
    } 
    //cube.Rotate(glm::radians(1.0f),glm::vec3(1.0f,0.0f,0.0f));
    //cube.Change_rotation(glm::radians(glm::vec3(0.0f,45.0f,45.0f)));
    //cube.SetTransform();  
    //glm::vec3 rot = cube.Get_rotation();
    //std::cout<<rot.y<<","<<rot.z<<","<<rot.x<<","<<std::endl;
    if (ImGuizmo::IsUsing()){
        isUsingGuizmo = true;
        //cube.Tanslate(glm::vec3(deltaMatrix[12],deltaMatrix[13],deltaMatrix[14]));    
        //ImGuizmo::DecomposeMatrixToComponents(deltaMatrix,translation,rotation,scale);
        //std::cout<<rotation[0]<<","<<rotation[1]<<","<<rotation[2]<<","<<std::endl;
        //cube.Rotate(glm::radians(glm::vec3(rotation[2],rotation[0],rotation[1])));

        glm::mat4 delta_transform(1.0f);

        switch (currentTransformOp)
        {
        case ImGuizmo::TRANSLATE:
            delta_transform[3] = glm::vec4(deltaMatrix[12],deltaMatrix[13],deltaMatrix[14],1.0f); 
            ObjectsVector[object_selected]->object->Tanslate(glm::vec3(deltaMatrix[12],deltaMatrix[13],deltaMatrix[14]));
            break;
        case ImGuizmo::ROTATE:
            memcpy(glm::value_ptr(delta_transform),deltaMatrix, sizeof(float) * 16);
            delta_transform[3] = glm::vec4(0.0f,0.0f,0.0f,1.0f);
            ObjectsVector[object_selected]->object->Rotate(delta_transform);
            break;
        case ImGuizmo::SCALE:
            memcpy(glm::value_ptr(delta_transform),deltaMatrix, sizeof(float) * 16);
            ObjectsVector[object_selected]->object->Scale(delta_transform);
            break;        
        }
        
    }else isUsingGuizmo = false;

    //}

    ImGui::End();  
    
    if(isUsingGuizmo && object_selected == "Domain"){
        fluid_solver->fluid_boundary.Change_position(ObjectsVector[object_selected]->object->Get_position());
        fluid_solver->fluid_boundary.Change_scale(ObjectsVector[object_selected]->object->Get_scale());
        fluid_solver->fluid_boundary.Change_rotation(ObjectsVector[object_selected]->object->Get_rotation());
    }
    
    //global_style_reference.WindowPadding = ImVec2(8.0f, 8.0f);
    //global_style_reference.WindowBorderSize = 2.0f;
    global_style_reference.WindowPadding = global_style_copy.WindowPadding;
    global_style_reference.WindowBorderSize = global_style_copy.WindowBorderSize;

    if(!button_state_vector[5]){

        float ortho_gain_half = std::clamp(viewport_camera->m_pivot_distance*0.6f*0.5f,0.01f,MAXFLOAT);
        float hor_ratio_half = ortho_gain_half*static_cast<float>(window_viewport_current_width)/
                               static_cast<float>(window_viewport_current_height);

        viewport_camera_projection_matrix = glm::ortho(-hor_ratio_half,
            hor_ratio_half,
            -ortho_gain_half,
            ortho_gain_half, 
            -100.0f, 100.0f);
    } 

    update_viewport_cam_view_matrix_ubo();

}


void FluidSim::Scene::draw_transform_settings(){

    ImGui::Begin("Object Transform",nullptr,ImGuiWindowFlags_None | windowFlags_global_reference);
    float modfied = false;
    if(object_selected != ""){

    glm::vec3 position = ObjectsVector[object_selected]->object->position; 
    glm::vec3 scale = ObjectsVector[object_selected]->object->scale;
    glm::vec3 orientation;
    orientation.z = glm::degrees(ObjectsVector[object_selected]->object->rotation.x);
    orientation.x = glm::degrees(ObjectsVector[object_selected]->object->rotation.y);
    orientation.y = glm::degrees(ObjectsVector[object_selected]->object->rotation.z);

    ImGui::Text(("Object: " + object_selected).c_str());
    ImGui::NewLine();
    // Draw your viewport content (OpenGL/Vulkan texture, etc.)
    ImGui::Text("Translation");
    // Create sliders for X and Y coordinates
    //ImGui::SliderFloat("Z Position", &board_position.z, -pos_amplitude, pos_amplitude, "Z: %.1f");
    if(ImGui::DragFloat("X Position", &position.x, 0.01f,-FLT_MAX,FLT_MAX,"X: %.2f m",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    if(ImGui::DragFloat("Y Position", &position.y, 0.01f,-FLT_MAX,FLT_MAX,"Y: %.2f m",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    if(ImGui::DragFloat("Z Position", &position.z, 0.01f,-FLT_MAX,FLT_MAX,"Z: %.2f m",ImGuiSliderFlags_AlwaysClamp))modfied = true;

    ImGui::Text("Rotation");

    if(ImGui::DragFloat("X Rotation", &orientation.x,0.1f,-FLT_MAX, FLT_MAX, "X: %.1f°",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    if(ImGui::DragFloat("Y Rotation", &orientation.y,0.1f,-FLT_MAX, FLT_MAX, "Y: %.1f°",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    if(ImGui::DragFloat("Z Rotation", &orientation.z,0.1f,-FLT_MAX, FLT_MAX, "Z: %.1f°",ImGuiSliderFlags_AlwaysClamp))modfied = true;

    ImGui::Text("Scale");

    if(ImGui::DragFloat("X Scale", &scale.x,0.01f,-FLT_MAX, FLT_MAX, "X: %.3f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    if(ImGui::DragFloat("Y Scale", &scale.y,0.01f,-FLT_MAX, FLT_MAX, "Y: %.3f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    if(ImGui::DragFloat("Z Scale", &scale.z,0.01f,-FLT_MAX, FLT_MAX, "Z: %.3f",ImGuiSliderFlags_AlwaysClamp))modfied = true;

    if(modfied){
    ObjectsVector[object_selected]->object->Change_position(position);
    ObjectsVector[object_selected]->object->Change_scale(scale);
    glm::vec3 rot_change(glm::radians(glm::vec3(orientation.z,orientation.x,orientation.y)));
    ObjectsVector[object_selected]->object->Change_rotation(rot_change);

        if(object_selected == "Domain"){
            fluid_solver->fluid_boundary.Change_position(position);
            fluid_solver->fluid_boundary.Change_scale(scale);
            fluid_solver->fluid_boundary.Change_rotation(rot_change);
        }else if(object_selected == "Floor"){

            floor_config.floor_size = glm::vec2(scale.z,scale.x);
            ShadersVector["floorShader"]->use();
            ShadersVector["floorShader"]->setVec2("floor_dimension",floor_config.floor_size);
        }

    }
    }else{
        ImGui::Text("Object: No Object Selected!");
    }

    ImGui::End();
     
    ImGui::Begin("Object Model Matrix",nullptr,ImGuiWindowFlags_None | windowFlags_global_reference);
    if(object_selected != ""){

    ImGui::Text(("Object: " + object_selected).c_str());
    ImGui::NewLine();
    ImGui::NewLine();

    float windowWidth = ImGui::GetWindowSize().x;
    float textWidth   = ImGui::CalcTextSize("MODEL MATRIX").x;
    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
    ImGui::Text("MODEL MATRIX");
    // Draw your viewport content (OpenGL/Vulkan texture, etc.)
        float line1[4] = {
            ObjectsVector[object_selected]->object->model_m[0][0],
            ObjectsVector[object_selected]->object->model_m[1][0],
            ObjectsVector[object_selected]->object->model_m[2][0],
            ObjectsVector[object_selected]->object->model_m[3][0]
        };
        float line2[4] = {
            ObjectsVector[object_selected]->object->model_m[0][1],
            ObjectsVector[object_selected]->object->model_m[1][1],
            ObjectsVector[object_selected]->object->model_m[2][1],
            ObjectsVector[object_selected]->object->model_m[3][1]
        };
        float line3[4] = {
            ObjectsVector[object_selected]->object->model_m[0][2],
            ObjectsVector[object_selected]->object->model_m[1][2],
            ObjectsVector[object_selected]->object->model_m[2][2],
            ObjectsVector[object_selected]->object->model_m[3][2]
        };
        float line4[4] = {
            ObjectsVector[object_selected]->object->model_m[0][3],
            ObjectsVector[object_selected]->object->model_m[1][3],
            ObjectsVector[object_selected]->object->model_m[2][3],
            ObjectsVector[object_selected]->object->model_m[3][3]
        };
        ImGui::SetCursorPosX((windowWidth) * 0.175f);
        ImGui::InputFloat4("##l1m", line1, "%.3f",ImGuiInputTextFlags_ReadOnly);
        ImGui::SetCursorPosX((windowWidth) * 0.175f);
        ImGui::InputFloat4("##l2m", line2, "%.3f",ImGuiInputTextFlags_ReadOnly);
        ImGui::SetCursorPosX((windowWidth) * 0.175f);
        ImGui::InputFloat4("##l3m", line3, "%.3f",ImGuiInputTextFlags_ReadOnly);
        ImGui::SetCursorPosX((windowWidth) * 0.175f);
        ImGui::InputFloat4("##l4m", line4, "%.3f",ImGuiInputTextFlags_ReadOnly);
    }
    else{
        ImGui::Text("Object: No Object Selected!");
    }
    ImGui::End();
}

void FluidSim::Scene::draw_statistics_viewport(){

    ImGui::Begin("Statistics");

    bool modfied = false;

    // Draw your viewport content (OpenGL/Vulkan texture, etc.)
    ImVec2 size = ImGui::GetContentRegionAvail();
    float spacing_x = global_style_reference.ItemSpacing.x;

    ImGui::Text("Viewport");
    ImGui::PushFont(font_mono);
    ImGui::SetWindowFontScale(0.9f);
    ImGui::Text("Framerate: %.1f FPS (%.3f ms/frame) ", io_reference.Framerate, 1000.0f / io_reference.Framerate);
    ImGui::PopFont();
    ImGui::SetWindowFontScale(1.0f);
    if(ImGui::Checkbox("Enable Vsync##statistics",&enable_vsync))(enable_vsync)?glfwSwapInterval(1):glfwSwapInterval(0);
    ImGui::NewLine();
    ImGui::Text("Simulation");
    ImGui::PushFont(font_mono);
    ImGui::SetWindowFontScale(0.9f);
    (simulation_time.load() == 0 || !fluid_thread_start.load())
    ?ImGui::Text("Framerate: Simulation Stopped")
    :ImGui::Text("Framerate: %.1f FPS (%.3f ms/frame) ", 1.0f/(static_cast<float>(simulation_time.load())/1000000.0f), static_cast<float>(simulation_time.load())/1000.0f);
    ImGui::PopFont();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::NewLine();
    ImGui::Text("Number of Particles: %u",fluid_solver->particles_number);
    
    
    ImGui::End();
}

void FluidSim::Scene::draw_simulation_settings_viewport(){


    ImGui::Begin("Render Settings");

    bool modfied = false;
    bool modfiedf = false;

    // Draw your viewport content (OpenGL/Vulkan texture, etc.)
    ImVec2 size = ImGui::GetContentRegionAvail();
    float spacing_x = global_style_reference.ItemSpacing.x;
    float button_size = static_cast<float>(monitorScreenHeight)*0.03;

//render_instanced

    ImGui::PushItemWidth(size.x);

    ImGui::Text("Render Fluid Mode");

    bool isSelected = false;

    if (ImGui::BeginCombo("##RenderFluidMode", renderfmode_items[current_renderfmode_item]))
    {
        for (int n = 0; n < IM_ARRAYSIZE(renderfmode_items); n++)
        {
            bool is_selected = (current_renderfmode_item == n);
            if (ImGui::Selectable(renderfmode_items[n], is_selected))
                current_renderfmode_item = n;

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();

        switch(current_renderfmode_item){
            case 0:
                render_raw = true;
                render_instanced = false;
                render_shaded = false;
                break;
            case 1:
                render_raw = false;
                render_instanced = true;
                render_shaded = false;
                break;
            case 2:
                render_raw = false;
                render_instanced = false;
                render_shaded = true;
                break;
        }
    }

    if(!render_raw){
        if(render_instanced){
            if(ImGui::Checkbox("Circular Shape",&instanced_c_shape)){
                fluidsim_p_sh.use();
                fluidsim_p_sh.setBool("render_as_circle",instanced_c_shape);
            }
        }

        ImGui::Text("Particle Render Size");
        if(ImGui::DragFloat("##Prendersize", &particle_render_size,0.01f, 0.01f,5.0f,"Size: %.2f (m)",ImGuiSliderFlags_AlwaysClamp)){
            if(render_instanced){
                fluidsim_p_sh.use();
                fluidsim_p_sh.setFloat("square_size",particle_render_size);
            }else{
                fluidsim_shade_sh.use();
                fluidsim_shade_sh.setFloat("square_size",particle_render_size);
            }
        }
    }

    ImGui::NewLine();

    ImGui::Text("Floor Settings");

    ShadersVector["floorShader"]->use();

    if(ImGui::Checkbox("View Floor",&floor_config.view_floor)){
        ObjectsVector["Floor"]->cameras_for_render["viewport_cam"] = floor_config.view_floor;
    }

    ImGui::Text("Size");

    ImGui::PushItemWidth((size.x-spacing_x)/2.0f);
    if(ImGui::DragFloat("##FloorH", &floor_config.floor_size.x,0.1f, 0.1f,100.0f,"Horizontal: %.2f (m)",ImGuiSliderFlags_AlwaysClamp))modfiedf = true;
    ImGui::SameLine();
    if(ImGui::DragFloat("##FlooV", &floor_config.floor_size.y,0.1f, 0.1f,100.0f,"Vertical: %.2f (m)",ImGuiSliderFlags_AlwaysClamp))modfiedf = true;

    ImGui::PushItemWidth(size.x);
    ImGui::Text("Tile Color 1");
    if(ImGui::ColorEdit3("##TileColor1",glm::value_ptr(floor_config.tileCol1))) 
        ShadersVector["floorShader"]->setVec3("tileCol1",floor_config.tileCol1);
    ImGui::Text("Tile Color 2");
    if(ImGui::ColorEdit3("##TileColor2",glm::value_ptr(floor_config.tileCol2)))
        ShadersVector["floorShader"]->setVec3("tileCol2",floor_config.tileCol2);
    ImGui::Text("Tile Color 3");
    if(ImGui::ColorEdit3("##TileColor3",glm::value_ptr(floor_config.tileCol3)))
        ShadersVector["floorShader"]->setVec3("tileCol3",floor_config.tileCol3);
    ImGui::Text("Tile Color 4");
    if(ImGui::ColorEdit3("##TileColor4",glm::value_ptr(floor_config.tileCol4)))
        ShadersVector["floorShader"]->setVec3("tileCol4",floor_config.tileCol4);

    ImGui::Text("Tile Color Variation");
    if(ImGui::ColorEdit3("##TileColorVar",glm::value_ptr(floor_config.tileColVariation)))
        ShadersVector["floorShader"]->setVec3("tileColVariation",floor_config.tileColVariation);

    ImGui::Text("Tile Dark Offset");
    if(ImGui::DragFloat("##TileDarkOff", &floor_config.tileDarkOffset,0.01f, -1.00f,1.00f,"Offset: %.2f",ImGuiSliderFlags_AlwaysClamp))
        ShadersVector["floorShader"]->setFloat("tileDarkOffset",floor_config.tileDarkOffset);
    ImGui::Text("Tile Size");
    if(ImGui::DragFloat("##TileSize", &floor_config.tileSize,0.01f, 0.01f,10.0f,"Size: %.2f (m)",ImGuiSliderFlags_AlwaysClamp))
        ShadersVector["floorShader"]->setFloat("tileScale",floor_config.tileSize);

    if(modfiedf) {
        ShadersVector["floorShader"]->setVec2("floor_dimension",floor_config.floor_size);
        ObjectsVector["Floor"]->object->Change_scale(glm::vec3(floor_config.floor_size.y,1.0f,floor_config.floor_size.x));
    }

    ImGui::NewLine();

    ImGui::PushItemWidth(size.x);

    ImGui::Text("Ball Settings");

    ImGui::Checkbox("View Ball",&ObjectsVector["Ball"]->cameras_for_render["viewport_cam"]);

    
    ImGui::Text("Ball Color");
    if(ImGui::ColorEdit3("##BallColor1",glm::value_ptr(ball_color))){
        ShadersVector["ballShader"]->use();
        ShadersVector["ballShader"]->setVec3("color",ball_color);
    }   

    ImGui::End();

    ImGui::Begin("Simulation Settings");

    modfied = false;
    size = ImGui::GetContentRegionAvail();

    ImGui::Text("Fluid Parameters");
    
    #ifdef USE_XPBF 
    //ImGui::PushItemWidth((size.x-spacing_x)/2.0f);
    //if(ImGui::DragFloat("##Vorticity", &scene_simulation_param.epsilon_vorticity,0.01f, 0.0f,1.0f,"Vorticity: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    //ImGui::SameLine();
    //if(ImGui::DragFloat("##Viscosity", &scene_simulation_param.c_viscosity,0.01f, 0.0f,1.0f,"Viscosity: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    //ImGui::PushItemWidth(size.x);
    //if(ImGui::DragFloat("##Relaxation", &scene_simulation_param.epsilon_lambda,0.01f, 0.01f,1.0f,"Relaxation: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    //if(ImGui::DragFloat("##delta_q", &scene_simulation_param.delta_q,0.01f, 0.1f,0.3f,"Delta q: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    #elifdef USE_VISCOEL

    ImGui::PushItemWidth(size.x);
    if(ImGui::DragFloat("##Stiffness", &scene_simulation_param.k_gas_constant,1.0f, 0.0f,20000.0f,"Stiffness: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    if(ImGui::DragFloat("##NearStiffness", &scene_simulation_param.k_near_gas_constant,1.0f, 0.0f,20000.0f,"Near Stiffness: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    ImGui::PushItemWidth((size.x-spacing_x)/2.0f);
    if(ImGui::DragFloat("##LViscosity", &scene_simulation_param.viscosity_linear,0.01f, 0.01f,50.0f,"LViscosity: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    ImGui::SameLine();
    if(ImGui::DragFloat("##QViscosity", &scene_simulation_param.viscosity_quadratic,0.01f, 0.01f,50.0f,"QViscosity: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    //ImGui::PushItemWidth((size.x-spacing_x)/2.0f);
    
    //if(ImGui::DragFloat("##Surface_Tension", &scene_simulation_param.surface_tension,0.01f, 0.01f,1.0f,"Surface Tension: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    //ImGui::Text("Surface Threshold");
    //if(ImGui::DragFloat("##SurfaceThres", &scene_simulation_param.l_tension,0.001f, 0.001f,1.0f,"%.3f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    ImGui::Text("Density");
    if(ImGui::DragFloat("##Density", &scene_simulation_param.rest_density,1.0f, 1.0f,50000.0f,"%.0f (Kg/m³)",ImGuiSliderFlags_AlwaysClamp))modfied = true;


    #else

    ImGui::PushItemWidth((size.x-spacing_x)/2.0f);
    if(ImGui::DragFloat("##Stiffness", &scene_simulation_param.k_gas_constant,1.0f, 0.0f,20000.0f,"Stiffness: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    ImGui::SameLine();
    if(ImGui::DragFloat("##Viscosity", &scene_simulation_param.viscosity,0.01f, 0.00f,1.3f,"Viscosity: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    ImGui::PushItemWidth(size.x);
    if(ImGui::DragFloat("##NearStiffness", &scene_simulation_param.k_near_gas_constant,1.0f, 0.0f,20000.0f,"Near Stiffness: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    if(ImGui::DragFloat("##Surface_Tension", &scene_simulation_param.surface_tension,0.01f, 0.00f,1.0f,"Surface Tension: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    ImGui::Text("Surface Threshold");
    if(ImGui::DragFloat("##SurfaceThres", &scene_simulation_param.l_tension,0.001f, 0.001f,1.0f,"%.3f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    ImGui::Text("Density");
    if(ImGui::DragFloat("##Density", &scene_simulation_param.rest_density,1.0f, 1.0f,50000.0f,"%.0f (Kg/m³)",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    
    #endif

    ImGui::Text("Boundary Settings");
    ImGui::PushItemWidth(size.x);
    if(ImGui::DragFloat("##RestituitionCR", &scene_simulation_param.cr_coefficient_b,0.01f, 0.00f,1.0f,"Restituition CR: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    if(ImGui::DragFloat("##FrictionB", &scene_simulation_param.friction_coefficient_b,0.01f, 0.0f,1.0f,"Wall Friction: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    if(ImGui::DragFloat("##FrictionMul", &scene_simulation_param.friction_multiplier,0.01f, 0.0f,180.0f,"Friction Multiplier: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    if(ImGui::Checkbox("View Boundary",&view_boundary)){
        ObjectsVector["Domain"]->cameras_for_render["viewport_cam"] = view_boundary;
    }
    if(!view_boundary){
        ImGui::BeginDisabled();
        if(ImGui::Button("Select Boundary",ImVec2((size.x-spacing_x)/2.0f,button_size)))object_selected="Domain";
        ImGui::EndDisabled();
    }
    else if(ImGui::Button("Select Boundary",ImVec2((size.x-spacing_x)/2.0f,button_size)))object_selected="Domain";
    //ImGui::NewLine();
    
    ImGui::NewLine();
    ImGui::PushItemWidth((size.x-spacing_x)/2.0f);
    ImGui::Text("Particles Density");
    ImGui::DragFloat("##PDensity", &particle_density,1.0f, 30.0f,3000.0f,"%.0f (p/m³)",ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    if(ImGui::Button("Apply",ImVec2((size.x-spacing_x)/2.0f,button_size))){

        fluid_thread_start.store(false);

        if(!fluid_solver->solver_func_mutex.try_lock()){

            // abort solve
            fluid_solver->solveAbortCondition.store(true);

            fluid_solver->solver_func_mutex.lock();

            fluid_solver->solveAbortCondition.store(false);
        }

        float particle_spacing = 1.0f/(std::round(std::cbrtf(particle_density))-1.0f);//0.035;

        fluid_solver->fluid_h = 2.f*particle_spacing;
        fluid_solver->particle_spacing = particle_spacing;
        fluid_solver->particle_radius = particle_spacing/2.f;
        fluid_solver->fluid_Rv = 1.5f*particle_spacing/2.f;


        float n_p = 1.0f/particle_spacing+1.0f;
        fluid_solver->particle_mass = fluid_solver->rest_density/(n_p*n_p*n_p)*1.35f;
    

        #ifdef USE_XPBF
        glm::vec3 deltaq = glm::vec3(fluid_solver->fluid_h*fluid_solver->delta_q,0.0f,0.0f);
        fluid_solver->poly6_delta_q = poly6_kernel(deltaq);
        #endif

        fluid_solver->solver_func_mutex.unlock();
        fluid_solver->ResetSim();
        simulation_time.store(0);


    }

    ImGui::PushItemWidth(size.x);

    ImGui::NewLine();
    ImGui::Text("Particles Render");
    if(ImGui::Checkbox("View Velocity Field",&view_velocity_field)){

        switch(current_renderfmode_item){
            case 0:
                fluidsim_sh.use();
                fluidsim_sh.setBool("render_vel_field",view_velocity_field);
                break;
            case 1:
                fluidsim_p_sh.use();
                fluidsim_p_sh.setBool("render_vel_field",view_velocity_field);
                break;
            case 2:
                fluidsim_shade_sh.use();
                fluidsim_shade_sh.setBool("render_vel_field",view_velocity_field);
                break;
        }


    }
    if(view_velocity_field){
        ImGui::BeginDisabled();
        ImGui::Text("Fluid Color");
        ImGui::ColorEdit3("##Color",particles_color);
        ImGui::EndDisabled();
    }
    else {
        ImGui::Text("Fluid Color");

        switch(current_renderfmode_item){
            case 0:
                fluidsim_sh.use();
                if(ImGui::ColorEdit3("##Color",particles_color))fluidsim_sh.setVec3("fluid_color",glm::vec3(particles_color[0],particles_color[1],particles_color[2]));
                break;
            case 1:
                fluidsim_p_sh.use();
                if(ImGui::ColorEdit3("##Color",particles_color))fluidsim_p_sh.setVec3("fluid_color",glm::vec3(particles_color[0],particles_color[1],particles_color[2]));
                break;
            case 2:
                fluidsim_shade_sh.use();
                if(ImGui::ColorEdit3("##Color",particles_color))fluidsim_shade_sh.setVec3("fluid_color",glm::vec3(particles_color[0],particles_color[1],particles_color[2]));
                break;
        }


    }

    ImGui::NewLine();
    ImGui::Text("Gravity");
    ImGui::PushItemWidth((size.x-2.0f*spacing_x)/3.0f);
    if(ImGui::DragFloat("##GravityX", &scene_simulation_param.gravity.x,0.01f, -100.0f,100.0f,"X: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    ImGui::SameLine();
    if(ImGui::DragFloat("##GravityY", &scene_simulation_param.gravity.y,0.01f, -100.0f,100.0f,"Y: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    ImGui::SameLine();
    if(ImGui::DragFloat("##GravityZ", &scene_simulation_param.gravity.z,0.01f, -100.0f,100.0f,"Z: %.2f",ImGuiSliderFlags_AlwaysClamp))modfied = true;

    #ifdef USE_XPBF 


    ImGui::PushItemWidth((size.x-spacing_x)/2.0f);
    float substep_num = static_cast<float>(scene_simulation_param.substep_num);
    float solver_iterations = static_cast<float>(scene_simulation_param.solver_iterations);
    if(ImGui::DragFloat("##Substeps", &substep_num,0.1f, 1.0f,FLT_MAX,"Substeps: %.0f",ImGuiSliderFlags_AlwaysClamp))modfied = true;
    ImGui::SameLine();
    if(ImGui::DragFloat("##SolverItr", &solver_iterations,0.1f, 1.0f,FLT_MAX,"Solver Itr: %.0f",ImGuiSliderFlags_AlwaysClamp))modfied = true;

    #else

    ImGui::PushItemWidth(size.x);
    float substep_num = static_cast<float>(scene_simulation_param.substep_num);
    if(ImGui::DragFloat("##Substeps", &substep_num,0.1f, 1.0f,FLT_MAX,"Substeps: %.0f",ImGuiSliderFlags_AlwaysClamp))modfied = true;

    #endif

    ImGui::PushItemWidth(size.x);
    float timestepsize = scene_simulation_param.timestep*1000.0f;
    if(ImGui::DragFloat("##Timestepsize", &timestepsize,0.001f, 0.1f,FLT_MAX,"Step Time (ms): %.3f",ImGuiSliderFlags_AlwaysClamp))modfied = true;

    if(ImGui::Button("Start",ImVec2((size.x-spacing_x)/2.0f,button_size)))fluid_thread_start.store(true);
    ImGui::SameLine();
    if(ImGui::Button("Stop",ImVec2((size.x-spacing_x)/2.0f,button_size))){
        fluid_thread_start.store(false);
        //simulation_time.store(0);
    }
    if(ImGui::Button("Reset",ImVec2(size.x,button_size))){
        fluid_thread_start.store(false);
        fluid_solver->ResetSim();
        simulation_time.store(0);
    }

    if(modfied){
        scene_simulation_param.substep_num = static_cast<uint32_t>(substep_num);
        #ifdef USE_XPBF
        scene_simulation_param.solver_iterations = static_cast<uint32_t>(solver_iterations);
        #endif
        scene_simulation_param.timestep = timestepsize/1000.0f;
        fluid_solver->ChangeSimulationParm(scene_simulation_param);
    }
    ImGui::End();


}


void FluidSim::Scene::processInput(const float &deltaTime)
{
    if (glfwGetKey(main_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(main_window, true);

    if(scene_hovered && button_state_vector[4]){
        if (glfwGetKey(main_window, GLFW_KEY_W) == GLFW_PRESS)
            (button_state_vector[5])?
            viewport_camera->ProcessKeyboard(FORWARD, deltaTime):
            viewport_camera->ProcessKeyboardPivotDistance(FORWARD, deltaTime);
        if (glfwGetKey(main_window, GLFW_KEY_S) == GLFW_PRESS)
            (button_state_vector[5])?
            viewport_camera->ProcessKeyboard(BACKWARD, deltaTime):
            viewport_camera->ProcessKeyboardPivotDistance(BACKWARD, deltaTime);
        if (glfwGetKey(main_window, GLFW_KEY_A) == GLFW_PRESS)
            viewport_camera->ProcessKeyboard(LEFT, deltaTime);
        if (glfwGetKey(main_window, GLFW_KEY_D) == GLFW_PRESS)
            viewport_camera->ProcessKeyboard(RIGHT, deltaTime);
        if (glfwGetKey(main_window, GLFW_KEY_Q) == GLFW_PRESS)
            viewport_camera->ProcessKeyboard(CAMERA_YAW_NEG, deltaTime);
        if (glfwGetKey(main_window, GLFW_KEY_E) == GLFW_PRESS)
            viewport_camera->ProcessKeyboard(CAMERA_YAW_POS, deltaTime);
        if (glfwGetKey(main_window, GLFW_KEY_R) == GLFW_PRESS)
            viewport_camera->ProcessKeyboard(CAMERA_PITCH_POS, deltaTime);
        if (glfwGetKey(main_window, GLFW_KEY_F) == GLFW_PRESS)
            viewport_camera->ProcessKeyboard(CAMERA_PITCH_NEG, deltaTime);
        if(ImGui::IsKeyPressed(ImGuiKey_P,false) && !isUsingGuizmo)
            want_to_toggle_proj = true;
                
        if (ImGui::IsKeyPressed(ImGuiKey_M,false) && !isUsingGuizmo && !isDragging){
            button_state_vector[4] = !button_state_vector[4];
            viewport_camera->fly_mode = button_state_vector[4];
        }
    }
    else if(scene_hovered && !isUsingGuizmo){

        
        //if (glfwGetKey(main_window, GLFW_KEY_G) == GLFW_PRESS)
        if(ImGui::IsKeyPressed(ImGuiKey_G,false))
        {
            currentTransformOp = ImGuizmo::TRANSLATE;
            button_state_vector[0] = true;
            button_state_vector[1] = false;
            button_state_vector[2] = false; 
        } 
        //if (glfwGetKey(main_window, GLFW_KEY_R) == GLFW_PRESS)
        if(ImGui::IsKeyPressed(ImGuiKey_R,false))
        {
            currentTransformOp = ImGuizmo::ROTATE;
            button_state_vector[0] = false;
            button_state_vector[1] = true;
            button_state_vector[2] = false;
        }
        //if (glfwGetKey(main_window, GLFW_KEY_S) == GLFW_PRESS)
        if(ImGui::IsKeyPressed(ImGuiKey_S,false))
        {
            currentTransformOp = ImGuizmo::SCALE;
            button_state_vector[0] = false;
            button_state_vector[1] = false;
            button_state_vector[2] = true;   
        }        
        //if (glfwGetKey(main_window, GLFW_KEY_W) == GLFW_PRESS)
        if(ImGui::IsKeyPressed(ImGuiKey_W,false))
        {
            button_state_vector[3] = !button_state_vector[3];
            currentTransformMode = (button_state_vector[3])?ImGuizmo::WORLD:ImGuizmo::LOCAL;
        }

        //if (glfwGetKey(main_window, GLFW_KEY_P) == GLFW_PRESS)
        if(ImGui::IsKeyPressed(ImGuiKey_P,false))
            want_to_toggle_proj = true;
        
        if (ImGui::IsKeyPressed(ImGuiKey_M,false) && !isDragging){
            button_state_vector[4] = !button_state_vector[4];
            viewport_camera->fly_mode = button_state_vector[4];
        }
            

    }

    if (glfwGetKey(main_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS && !isDragging)
        shift_pressed = true;
    else if(glfwGetKey(main_window, GLFW_KEY_LEFT_SHIFT) == GLFW_RELEASE && !isDragging)
        shift_pressed = false; 
        
    if (glfwGetMouseButton(main_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS && !keyPressed && scene_hovered){

        keyPressed = true;
        viewport_camera->firstMouse = true;

        glfwGetCursorPos(main_window, &global_cursor_xpos, &global_cursor_ypos);

        glfwSetCursorPosCallback(main_window, mouse_callback);
        glfwSetMouseButtonCallback(main_window, nullptr);

        ImGui::GetIO().MouseDown[0] = 0;
        ImGui::GetIO().MouseDown[1] = 0;

        //change cursor icon
        isDragging = true;
        glfwSetInputMode(main_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        (button_state_vector[4] && !shift_pressed)?
        glfwSetCursor(main_window, invisible_cursor):
        glfwSetCursor(main_window, dragCursor);
    } 
    //or just consider processing only at each frame 
    //else if (glfwGetKey(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS && keyPressed && scene_hovered){
    //    //camera.ProcessMouseMovement();
    //}
    else if (glfwGetMouseButton(main_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_RELEASE || !scene_hovered){

        keyPressed=false;
        glfwSetCursorPosCallback(main_window, ImGui_ImplGlfw_CursorPosCallback);
        glfwSetMouseButtonCallback(main_window, ImGui_ImplGlfw_MouseButtonCallback);

        //restore cursor icon
        isDragging = false;
        if(glfwGetInputMode(main_window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) glfwSetCursor(main_window, arrowCursor);
        glfwSetInputMode(main_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    

    if(scene_hovered && !scroll_callback_installed){
        glfwSetScrollCallback(main_window, scroll_callback);
        scroll_callback_installed = true;
    }
    else if(!scene_hovered && scroll_callback_installed){
        glfwSetScrollCallback(main_window, ImGui_ImplGlfw_ScrollCallback);
        scroll_callback_installed = false;

    }


}
// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);


    main_scene_class->viewport_camera->ProcessMouseMovement(xpos, ypos, main_scene_class->shift_pressed);
    //if out of bounds teleport && camera set initial position


    if(xpos < main_scene_class->ViewportPos.x){
        glfwSetCursorPos(window, xpos + main_scene_class->ViewportSize.x, ypos);
        xpos += main_scene_class->ViewportSize.x;
        main_scene_class->viewport_camera->firstMouse = true;
    } else if(xpos > (main_scene_class->ViewportPos.x+main_scene_class->ViewportSize.x)){
        glfwSetCursorPos(window, xpos - main_scene_class->ViewportSize.x, ypos);
        xpos -= main_scene_class->ViewportSize.x;
        main_scene_class->viewport_camera->firstMouse = true;
    }
    if(ypos < main_scene_class->ViewportPos.y){
        glfwSetCursorPos(window, xpos, ypos + main_scene_class->ViewportSize.y);
        main_scene_class->viewport_camera->firstMouse = true;
    } else if(ypos > (main_scene_class->ViewportPos.y+main_scene_class->ViewportSize.y)){
        glfwSetCursorPos(window, xpos, ypos - main_scene_class->ViewportSize.y);
        main_scene_class->viewport_camera->firstMouse = true;
    }
    
    main_scene_class->global_cursor_xpos = static_cast<double>(xpos);
    main_scene_class->global_cursor_ypos = static_cast<double>(ypos);

}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    main_scene_class->viewport_camera->ProcessMouseScroll(static_cast<float>(yoffset));
}
