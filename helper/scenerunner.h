#include <glad/glad.h>
#include "scene.h"
#include <GLFW/glfw3.h>
#include "glutils.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define WIN_WIDTH 800
#define WIN_HEIGHT 600

#include <map>
#include <string>
#include <fstream>
#include <iostream>

struct CameraState {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 2.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    float distance = 2.0f;
    float lastX = WIN_WIDTH / 2.0f;
    float lastY = WIN_HEIGHT / 2.0f;
    bool dragging = false;
    float sensitivity = 0.3f;
};

struct InputState {
    bool dirLightOn = true;
    bool spotLightOn = true;
    bool postProcessOn = false;
    bool showSphere = false;
    bool autoRotate = false;
    bool key1Pressed = false;
    bool key2Pressed = false;
    bool key3Pressed = false;
    bool key4Pressed = false;
    bool keyFPressed = false;
};

inline CameraState g_camera;
inline InputState g_input;
inline float g_deltaTime = 0.0f;
inline float g_lastFrame = 0.0f;

inline void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_camera.dragging = true;
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            g_camera.lastX = static_cast<float>(x);
            g_camera.lastY = static_cast<float>(y);
        } else if (action == GLFW_RELEASE) {
            g_camera.dragging = false;
        }
    }
}

inline void cursorPosCallback(GLFWwindow* window, double xposIn, double yposIn) {
    if (!g_camera.dragging) return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    float xoffset = (xpos - g_camera.lastX) * g_camera.sensitivity;
    float yoffset = (g_camera.lastY - ypos) * g_camera.sensitivity;
    g_camera.lastX = xpos;
    g_camera.lastY = ypos;

    g_camera.yaw += xoffset;
    g_camera.pitch += yoffset;

    if (g_camera.pitch > 89.0f) g_camera.pitch = 89.0f;
    if (g_camera.pitch < -89.0f) g_camera.pitch = -89.0f;
}

inline void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    g_camera.distance -= static_cast<float>(yoffset) * 0.2f;
    if (g_camera.distance < 0.5f) g_camera.distance = 0.5f;
    if (g_camera.distance > 10.0f) g_camera.distance = 10.0f;
}

inline void updateCameraPosition() {
    float x = g_camera.distance * cos(glm::radians(g_camera.pitch)) * cos(glm::radians(g_camera.yaw));
    float y = g_camera.distance * sin(glm::radians(g_camera.pitch));
    float z = g_camera.distance * cos(glm::radians(g_camera.pitch)) * sin(glm::radians(g_camera.yaw));
    g_camera.position = glm::vec3(x, y, z);
    g_camera.front = glm::normalize(-g_camera.position);
}

class SceneRunner {
private:
    GLFWwindow * window;
    int fbw, fbh;
	bool debug;           // Set true to enable debug messages

public:
    SceneRunner(const std::string & windowTitle, int width = WIN_WIDTH, int height = WIN_HEIGHT, int samples = 0) : debug(true) {
        // Initialize GLFW
        if( !glfwInit() ) exit( EXIT_FAILURE );

#ifdef __APPLE__
        // Select OpenGL 4.1
        glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
        glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 1 );
#else
        // Select OpenGL 4.6
        glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );
        glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 6 );
#endif
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
        if(debug) 
			glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
        if(samples > 0) {
            glfwWindowHint(GLFW_SAMPLES, samples);
        }

        // Open the window
        window = glfwCreateWindow( WIN_WIDTH, WIN_HEIGHT, windowTitle.c_str(), NULL, NULL );
        if( ! window ) {
			std::cerr << "Unable to create OpenGL context." << std::endl;
            glfwTerminate();
            exit( EXIT_FAILURE );
        }
        glfwMakeContextCurrent(window);

        // Get framebuffer size
        glfwGetFramebufferSize(window, &fbw, &fbh);

        // Load the OpenGL functions.
        if(!gladLoadGL()) { exit(-1); }

        GLUtils::dumpGLInfo();

        // Initialization
        glClearColor(0.5f,0.5f,0.5f,1.0f);

        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetScrollCallback(window, scrollCallback);

#ifndef __APPLE__
		if (debug) {
			glDebugMessageCallback(GLUtils::debugCallback, nullptr);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
			glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
				GL_DEBUG_SEVERITY_NOTIFICATION, -1, "Start debugging");
		}
#endif
    }

    int run(Scene & scene) {
        scene.setDimensions(fbw, fbh);
        scene.initScene();
        scene.resize(fbw, fbh);

        // Enter the main loop
        mainLoop(window, scene);

#ifndef __APPLE__
		if( debug )
			glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 1,
				GL_DEBUG_SEVERITY_NOTIFICATION, -1, "End debug");
#endif

		// Close window and terminate GLFW
		glfwTerminate();

        // Exit program
        return EXIT_SUCCESS;
    }

    static std::string parseCLArgs(int argc, char ** argv, std::map<std::string, std::string> & sceneData) {
        if( argc < 2 ) {
            printHelpInfo(argv[0], sceneData);
            exit(EXIT_FAILURE);
        }

        std::string recipeName = argv[1];
        auto it = sceneData.find(recipeName);
        if( it == sceneData.end() ) {
            printf("Unknown recipe: %s\n\n", recipeName.c_str());
            printHelpInfo(argv[0], sceneData);
            exit(EXIT_FAILURE);
        }

        return recipeName;
    }

private:
    static void printHelpInfo(const char * exeFile,  std::map<std::string, std::string> & sceneData) {
        printf("Usage: %s recipe-name\n\n", exeFile);
        printf("Recipe names: \n");
        for( auto it : sceneData ) {
            printf("  %11s : %s\n", it.first.c_str(), it.second.c_str());
        }
    }

    void processInput(GLFWwindow* window) {
        updateCameraPosition();

        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !g_input.key1Pressed) {
            g_input.dirLightOn = !g_input.dirLightOn;
            g_input.key1Pressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE)
            g_input.key1Pressed = false;

        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !g_input.key2Pressed) {
            g_input.spotLightOn = !g_input.spotLightOn;
            g_input.key2Pressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE)
            g_input.key2Pressed = false;

        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && !g_input.key3Pressed) {
            g_input.showSphere = !g_input.showSphere;
            g_input.key3Pressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_RELEASE)
            g_input.key3Pressed = false;

        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS && !g_input.key4Pressed) {
            g_input.autoRotate = !g_input.autoRotate;
            g_input.key4Pressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_RELEASE)
            g_input.key4Pressed = false;

        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !g_input.keyFPressed) {
            g_input.postProcessOn = !g_input.postProcessOn;
            g_input.keyFPressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE)
            g_input.keyFPressed = false;
    }

    void mainLoop(GLFWwindow * window, Scene & scene) {
        while( ! glfwWindowShouldClose(window) && !glfwGetKey(window, GLFW_KEY_ESCAPE) ) {
            GLUtils::checkForOpenGLError(__FILE__,__LINE__);
            
            processInput(window);
            scene.update(float(glfwGetTime()));
            scene.render();
            glfwSwapBuffers(window);

            glfwPollEvents();
			int state = glfwGetKey(window, GLFW_KEY_SPACE);
			if (state == GLFW_PRESS)
				scene.animate(!scene.animating());
        }
    }
};
