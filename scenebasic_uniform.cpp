#include "scenebasic_uniform.h"
#include "helper/scenerunner.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>
#include <vector>

using std::string;
using std::cerr;
using std::endl;
using std::vector;

#include "helper/glutils.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "thirdparty/tinygltf/tiny_gltf.h"

using glm::vec3;
using glm::mat4;
using glm::mat3;

SceneBasic_Uniform::SceneBasic_Uniform() : 
    angle(0.0f), modelLoaded(false), modelIndexCount(0),
    modelVAO(0), modelVBO(0), modelNBO(0), modelEBO(0),
    modelUVBO(0), modelTangentBO(0),
    baseColorTex(0), normalMapTex(0),
    skyboxVAO(0), skyboxVBO(0), cubemapTexture(0),
    fbo(0), fboColorTex(0), fboDepthRBO(0), quadVAO(0), quadVBO(0),
    sphereVAO(0), sphereVBO(0), sphereEBO(0), sphereIndexCount(0) {}

void SceneBasic_Uniform::compile()
{
    try {
        progBlinnPhong.compileShader("shader/blinn_phong.vert");
        progBlinnPhong.compileShader("shader/blinn_phong.frag");
        progBlinnPhong.link();

        progSkybox.compileShader("shader/skybox.vert");
        progSkybox.compileShader("shader/skybox.frag");
        progSkybox.link();

        progPostProcess.compileShader("shader/postprocess.vert");
        progPostProcess.compileShader("shader/postprocess.frag");
        progPostProcess.link();
    } catch (GLSLProgramException &e) {
        cerr << e.what() << endl;
        exit(EXIT_FAILURE);
    }
}

void SceneBasic_Uniform::loadModel(const std::string& path)
{
    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = loader.LoadBinaryFromFile(&gltf, &err, &warn, path);
    if (!warn.empty()) cerr << "GLTF Warning: " << warn << endl;
    if (!err.empty()) cerr << "GLTF Error: " << err << endl;
    if (!ret) {
        cerr << "Failed to load GLB: " << path << endl;
        return;
    }

    if (gltf.meshes.empty()) return;

    vector<float> positions, normals, uvs, tangents;
    vector<unsigned int> indices;

    for (const auto& mesh : gltf.meshes) {
        for (const auto& prim : mesh.primitives) {
            unsigned int vertexOffset = positions.size() / 3;

            auto extractAttr = [&](const char* name, int compCount) -> vector<float> {
                vector<float> result;
                if (!prim.attributes.count(name)) return result;
                const tinygltf::Accessor& acc = gltf.accessors[prim.attributes.at(name)];
                const tinygltf::BufferView& bv = gltf.bufferViews[acc.bufferView];
                const tinygltf::Buffer& buf = gltf.buffers[bv.buffer];
                const float* data = reinterpret_cast<const float*>(&buf.data[bv.byteOffset + acc.byteOffset]);
                for (size_t i = 0; i < acc.count * compCount; i++)
                    result.push_back(data[i]);
                return result;
            };

            auto pos = extractAttr("POSITION", 3);
            auto norm = extractAttr("NORMAL", 3);
            auto uv = extractAttr("TEXCOORD_0", 2);
            auto tan = extractAttr("TANGENT", 4);

            positions.insert(positions.end(), pos.begin(), pos.end());
            normals.insert(normals.end(), norm.begin(), norm.end());
            uvs.insert(uvs.end(), uv.begin(), uv.end());
            tangents.insert(tangents.end(), tan.begin(), tan.end());

            if (prim.indices >= 0) {
                const tinygltf::Accessor& acc = gltf.accessors[prim.indices];
                const tinygltf::BufferView& bv = gltf.bufferViews[acc.bufferView];
                const tinygltf::Buffer& buf = gltf.buffers[bv.buffer];
                const unsigned char* dataPtr = &buf.data[bv.byteOffset + acc.byteOffset];
                for (size_t i = 0; i < acc.count; i++) {
                    unsigned int idx = 0;
                    if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        idx = reinterpret_cast<const unsigned short*>(dataPtr)[i];
                    else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                        idx = reinterpret_cast<const unsigned int*>(dataPtr)[i];
                    else
                        idx = dataPtr[i];
                    indices.push_back(idx + vertexOffset);
                }
            }

            if (baseColorTex == 0 && prim.material >= 0 && prim.material < (int)gltf.materials.size()) {
                const tinygltf::Material& mat = gltf.materials[prim.material];
                
                auto loadTexFromGLTF = [&](int texIndex) -> GLuint {
                    if (texIndex < 0) return 0;
                    const tinygltf::Texture& tex = gltf.textures[texIndex];
                    if (tex.source < 0) return 0;
                    const tinygltf::Image& img = gltf.images[tex.source];
                    
                    GLuint texID;
                    glGenTextures(1, &texID);
                    glBindTexture(GL_TEXTURE_2D, texID);
                    
                    GLenum format = (img.component == 4) ? GL_RGBA : GL_RGB;
                    glTexImage2D(GL_TEXTURE_2D, 0, format, img.width, img.height, 0, format, GL_UNSIGNED_BYTE, img.image.data());
                    glGenerateMipmap(GL_TEXTURE_2D);
                    
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    
                    return texID;
                };

                if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
                    baseColorTex = loadTexFromGLTF(mat.pbrMetallicRoughness.baseColorTexture.index);
                    std::cout << "Loaded baseColor texture" << endl;
                }
                if (mat.normalTexture.index >= 0) {
                    normalMapTex = loadTexFromGLTF(mat.normalTexture.index);
                    std::cout << "Loaded normal map" << endl;
                }
            }
        }
    }

    modelIndexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &modelVAO);
    glBindVertexArray(modelVAO);

    glGenBuffers(1, &modelVBO);
    glBindBuffer(GL_ARRAY_BUFFER, modelVBO);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &modelNBO);
    glBindBuffer(GL_ARRAY_BUFFER, modelNBO);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(float), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1);

    if (!uvs.empty()) {
        glGenBuffers(1, &modelUVBO);
        glBindBuffer(GL_ARRAY_BUFFER, modelUVBO);
        glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(float), uvs.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(2);
    }

    if (!tangents.empty()) {
        glGenBuffers(1, &modelTangentBO);
        glBindBuffer(GL_ARRAY_BUFFER, modelTangentBO);
        glBufferData(GL_ARRAY_BUFFER, tangents.size() * sizeof(float), tangents.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(3);
    }

    glGenBuffers(1, &modelEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, modelEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    modelLoaded = true;
}

GLuint SceneBasic_Uniform::loadCubemap(const std::vector<std::string>& faces)
{
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texID);

    int w, h, ch;
    for (int i = 0; i < 6; i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &w, &h, &ch, 0);
        if (data) {
            GLenum format = (ch == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            cerr << "Cubemap load failed: " << faces[i] << endl;
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return texID;
}

void SceneBasic_Uniform::setupSkybox()
{
    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    vector<string> faces = {
        "textures/right.jpg", "textures/left.jpg",
        "textures/top.jpg", "textures/bottom.jpg",
        "textures/front.jpg", "textures/back.jpg"
    };
    cubemapTexture = loadCubemap(faces);
}

void SceneBasic_Uniform::setupFramebuffer()
{
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &fboColorTex);
    glBindTexture(GL_TEXTURE_2D, fboColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboColorTex, 0);

    glGenRenderbuffers(1, &fboDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, fboDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fboDepthRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        cerr << "Framebuffer not complete!" << endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneBasic_Uniform::setupQuad()
{
    float quadVerts[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void SceneBasic_Uniform::createSphere(float radius, int sectors, int stacks)
{
    vector<float> vertices;
    vector<unsigned int> indices;

    float sectorStep = 2 * 3.14159265f / sectors;
    float stackStep = 3.14159265f / stacks;

    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = 3.14159265f / 2 - i * stackStep;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = j * sectorStep;
            float x = xy * cosf(sectorAngle);
            float y = xy * sinf(sectorAngle);

            vertices.push_back(x);
            vertices.push_back(z);
            vertices.push_back(y);

            float nx = x / radius;
            float ny = z / radius;
            float nz = y / radius;
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);

            float s = (float)j / sectors;
            float t = (float)i / stacks;
            vertices.push_back(s);
            vertices.push_back(t);
        }
    }

    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    sphereIndexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &sphereVAO);
    glBindVertexArray(sphereVAO);

    glGenBuffers(1, &sphereVBO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glGenBuffers(1, &sphereEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void SceneBasic_Uniform::renderSphere()
{
    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void SceneBasic_Uniform::renderModel()
{
    if (!modelLoaded) return;
    glBindVertexArray(modelVAO);
    glDrawElements(GL_TRIANGLES, modelIndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void SceneBasic_Uniform::renderSkybox()
{
    glDepthFunc(GL_LEQUAL);
    progSkybox.use();

    mat4 skyView = mat4(mat3(view));
    progSkybox.setUniform("view", skyView);
    progSkybox.setUniform("projection", projection);

    glBindVertexArray(skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
    progSkybox.setUniform("skybox", 0);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}

void SceneBasic_Uniform::renderQuad()
{
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void SceneBasic_Uniform::initScene()
{
    compile();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    loadModel("media/model.glb");
    createSphere(0.8f, 36, 18);
    setupSkybox();
    setupFramebuffer();
    setupQuad();

    projection = glm::perspective(glm::radians(45.0f), (float)width / height, 0.1f, 100.0f);
}

void SceneBasic_Uniform::update(float t)
{
    angle = t;
}

void SceneBasic_Uniform::render()
{
    view = glm::lookAt(g_camera.position, g_camera.position + g_camera.front, g_camera.up);
    model = mat4(1.0f);

    if (g_input.autoRotate) {
        model = glm::rotate(model, angle * 0.5f, vec3(0.0f, 1.0f, 0.0f));
    }

    if (g_input.postProcessOn) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    progBlinnPhong.use();
    progBlinnPhong.setUniform("model", model);
    progBlinnPhong.setUniform("view", view);
    progBlinnPhong.setUniform("projection", projection);
    progBlinnPhong.setUniform("viewPos", g_camera.position);

    progBlinnPhong.setUniform("dirLight.on", g_input.dirLightOn);
    progBlinnPhong.setUniform("dirLight.direction", vec3(-0.5f, -0.8f, -1.0f));
    progBlinnPhong.setUniform("dirLight.ambient", vec3(0.25f));
    progBlinnPhong.setUniform("dirLight.diffuse", vec3(1.2f));
    progBlinnPhong.setUniform("dirLight.specular", vec3(0.6f));

    vec3 spotColor;
    if (g_input.autoRotate) {
        float t = fmod(angle * 0.3f, 6.283185f) / 6.283185f;
        float phase = t * 3.0f;
        int idx = (int)phase;
        float frac = phase - idx;
        
        vec3 colors[4] = { vec3(1.0f, 0.2f, 0.2f), vec3(0.2f, 1.0f, 0.2f), 
                           vec3(0.2f, 0.2f, 1.0f), vec3(1.0f, 0.2f, 0.2f) };
        spotColor = glm::mix(colors[idx], colors[idx + 1], frac);
    } else {
        spotColor = vec3(1.5f, 0.2f, 0.2f);
    }

    progBlinnPhong.setUniform("spotLight.on", g_input.spotLightOn);
    progBlinnPhong.setUniform("spotLight.position", g_camera.position);
    progBlinnPhong.setUniform("spotLight.direction", g_camera.front);
    progBlinnPhong.setUniform("spotLight.ambient", spotColor * 0.1f);
    progBlinnPhong.setUniform("spotLight.diffuse", spotColor * 1.2f);
    progBlinnPhong.setUniform("spotLight.specular", spotColor * 0.8f);
    progBlinnPhong.setUniform("spotLight.cutOff", glm::cos(glm::radians(15.0f)));
    progBlinnPhong.setUniform("spotLight.outerCutOff", glm::cos(glm::radians(25.0f)));
    progBlinnPhong.setUniform("spotLight.constant", 1.0f);
    progBlinnPhong.setUniform("spotLight.linear", 0.027f);
    progBlinnPhong.setUniform("spotLight.quadratic", 0.0028f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, baseColorTex);
    progBlinnPhong.setUniform("baseColorMap", 0);
    progBlinnPhong.setUniform("hasBaseColor", baseColorTex != 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalMapTex);
    progBlinnPhong.setUniform("normalMap", 1);
    progBlinnPhong.setUniform("hasNormalMap", normalMapTex != 0);

    if (g_input.showSphere) {
        progBlinnPhong.setUniform("hasBaseColor", false);
        progBlinnPhong.setUniform("hasNormalMap", false);
        renderSphere();
    } else {
        renderModel();
    }
    renderSkybox();

    if (g_input.postProcessOn) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        progPostProcess.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fboColorTex);
        progPostProcess.setUniform("screenTexture", 0);
        renderQuad();

        glEnable(GL_DEPTH_TEST);
    }
}

void SceneBasic_Uniform::resize(int w, int h)
{
    width = w;
    height = h;
    glViewport(0, 0, w, h);
    projection = glm::perspective(glm::radians(45.0f), (float)w / h, 0.1f, 100.0f);

    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &fboColorTex);
        glDeleteRenderbuffers(1, &fboDepthRBO);
        setupFramebuffer();
    }
}