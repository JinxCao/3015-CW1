#ifndef SCENEBASIC_UNIFORM_H
#define SCENEBASIC_UNIFORM_H

#include "helper/scene.h"

#define ENABLE_OPENGL
#include <glad/glad.h>
#include "helper/glslprogram.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>

class SceneBasic_Uniform : public Scene
{
private:
    GLuint vaoHandle;
    float angle;

    GLSLProgram progBlinnPhong;
    GLSLProgram progSkybox;
    GLSLProgram progPostProcess;

    GLuint modelVAO;
    GLuint modelVBO, modelNBO, modelEBO;
    GLuint modelUVBO, modelTangentBO;
    GLsizei modelIndexCount;
    bool modelLoaded;
    
    GLuint baseColorTex;
    GLuint normalMapTex;

    GLuint skyboxVAO, skyboxVBO;
    GLuint cubemapTexture;

    GLuint fbo;
    GLuint fboColorTex;
    GLuint fboDepthRBO;
    GLuint quadVAO, quadVBO;

    GLuint sphereVAO, sphereVBO, sphereEBO;
    GLsizei sphereIndexCount;

    void compile();
    void loadModel(const std::string& path);
    void createSphere(float radius, int sectors, int stacks);
    GLuint loadCubemap(const std::vector<std::string>& faces);
    void setupSkybox();
    void setupFramebuffer();
    void setupQuad();
    void renderModel();
    void renderSphere();
    void renderSkybox();
    void renderQuad();

public:
    SceneBasic_Uniform();

    void initScene();
    void update( float t );
    void render();
    void resize(int, int);
};

#endif // SCENEBASIC_UNIFORM_H