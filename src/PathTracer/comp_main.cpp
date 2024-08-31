#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "opengl/Shader.h"
#include "camera.h"
#include "quad.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void glfw_error_callback(int error, const char* description);
void processInput(GLFWwindow* window);

const unsigned int SCREEN_WIDTH = 400;
const unsigned int SCREEN_HEIGHT = 400;

struct Material {
    alignas(16) glm::vec4 albedo;
    alignas(4)  unsigned int type;
};

#define MATERIAL_LIGHT 0
#define MATERIAL_LAMBERTIAN 1
#define MATERIAL_METAL 2
#define MATERIAL_GLASS 3

struct triangle {
    glm::vec3 v0;
    glm::vec3 v1;
    glm::vec3 v2;
    unsigned int materialIndex;
};

struct gpu_quad {
    alignas(16) glm::vec4 Q;
    alignas(16) glm::vec4 u;
    alignas(16) glm::vec4 v;
    alignas(4)  unsigned int materialIndex;
};

void GLAPIENTRY MessageCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam
) {
    std::string SEVERITY = "";
    switch (severity) {
    case GL_DEBUG_SEVERITY_LOW:
        SEVERITY = "LOW";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        SEVERITY = "MEDIUM";
        break;
    case GL_DEBUG_SEVERITY_HIGH:
        SEVERITY = "HIGH";
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        SEVERITY = "NOTIFICATION";
        break;
    }
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = %s, message = %s\n",
        type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "",
        type, SEVERITY.c_str(), message);
}

float deltaTime = 0.0f;
float lastFrame = 0.0f;

camera cam;
float lastX = SCREEN_WIDTH / 2.0f;
float lastY = SCREEN_HEIGHT / 2.0f;
bool firstMouse = true;

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Path Tracer", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwSetErrorCallback(glfw_error_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Enable debug output
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, 0);

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    Shader shader("../../../../../data/shaders/pathtracer.glsl");
	Shader quadShader("../../../../../data/shaders/quad_vert.glsl", "../../../../../data/shaders/quad_frag.glsl");

    // Quad SSBO
    auto red_lol = make_shared<lambertian>(color(.65, .05, .05));

    quad q0 = { point3(555, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red_lol };
    quad q1 = { point3(0, 0, 0), vec3(0, 555, 0), vec3(0, 0, 555), red_lol };
    quad q2 = { point3(343, 554, 332), vec3(-130, 0, 0), vec3(0, 0, -105), red_lol };
    quad q3 = { point3(0, 0, 0), vec3(555, 0, 0), vec3(0, 0, 555), red_lol };
    quad q4 = { point3(555, 555, 555), vec3(-555, 0, 0), vec3(0, 0, -555), red_lol };
    quad q5 = { point3(0, 0, 555), vec3(555, 0, 0), vec3(0, 555, 0), red_lol };

    gpu_quad gq0 = {
        glm::vec4(q0.Q.x(), q0.Q.y(), q0.Q.z(), 0.0),
        glm::vec4(q0.u.x(), q0.u.y(), q0.u.z(), 0.0),
        glm::vec4(q0.v.x(), q0.v.y(), q0.v.z(), 0.0),
        0
    };
	gpu_quad gq1 = {
		glm::vec4(q1.Q.x(), q1.Q.y(), q1.Q.z(), 0.0),
		glm::vec4(q1.u.x(), q1.u.y(), q1.u.z(), 0.0),
		glm::vec4(q1.v.x(), q1.v.y(), q1.v.z(), 0.0),
		2
	};
	gpu_quad gq2 = {
		glm::vec4(q2.Q.x(), q2.Q.y(), q2.Q.z(), 0.0),
		glm::vec4(q2.u.x(), q2.u.y(), q2.u.z(), 0.0),
		glm::vec4(q2.v.x(), q2.v.y(), q2.v.z(), 0.0),
		1
	};
	gpu_quad gq3 = {
		glm::vec4(q3.Q.x(), q3.Q.y(), q3.Q.z(), 0.0),
		glm::vec4(q3.u.x(), q3.u.y(), q3.u.z(), 0.0),
		glm::vec4(q3.v.x(), q3.v.y(), q3.v.z(), 0.0),
		1
	};
	gpu_quad gq4 = {
		glm::vec4(q4.Q.x(), q4.Q.y(), q4.Q.z(), 0.0),
		glm::vec4(q4.u.x(), q4.u.y(), q4.u.z(), 0.0),
		glm::vec4(q4.v.x(), q4.v.y(), q4.v.z(), 0.0),
		1
	};
	gpu_quad gq5 = {
		glm::vec4(q5.Q.x(), q5.Q.y(), q5.Q.z(), 0.0),
		glm::vec4(q5.u.x(), q5.u.y(), q5.u.z(), 0.0),
		glm::vec4(q5.v.x(), q5.v.y(), q5.v.z(), 0.0),
		1
	};

	gpu_quad quads[] = { gq0, gq1, gq2, gq3, gq4, gq5 };

	GLuint quadBuffer;
	glCreateBuffers(1, &quadBuffer);

	glNamedBufferStorage(quadBuffer, sizeof(quads), quads, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, quadBuffer);

    // Material SSBO
    Material red = { glm::vec4(.65, .05, .05, 0), MATERIAL_LAMBERTIAN };
    Material white = { glm::vec4(.73, .73, .73, 0), MATERIAL_LAMBERTIAN };
    Material green = { glm::vec4(.12, .45, .15, 0), MATERIAL_LAMBERTIAN };
    Material light = { glm::vec4(15.0f, 15.0f, 15.0f, 0), MATERIAL_LIGHT };

    Material materials[] = { red, white, green, light };

    GLuint materialBuffer;
    glCreateBuffers(1, &materialBuffer);

    glNamedBufferStorage(materialBuffer, sizeof(materials), materials, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, materialBuffer);

    // outputImage image2D
	GLuint outputImage;
	glGenTextures(1, &outputImage);
	glBindTexture(GL_TEXTURE_2D, outputImage);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, SCREEN_WIDTH, SCREEN_HEIGHT);
	glBindImageTexture(0, outputImage, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	// Screen quad VAO
	float quadVertices[] = {
		-1.0f, -1.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 1.0f, 0.0f,
		 1.0f,  1.0f, 1.0f, 1.0f,

		-1.0f, -1.0f, 0.0f, 0.0f,
		 1.0f,  1.0f, 1.0f, 1.0f,
		-1.0f,  1.0f, 0.0f, 1.0f
	};

	GLuint quadVBO;
	glCreateBuffers(1, &quadVBO);
	glNamedBufferStorage(quadVBO, sizeof(quadVertices), quadVertices, 0);

	GLuint quadVAO;
	glCreateVertexArrays(1, &quadVAO);

	glVertexArrayVertexBuffer(quadVAO, 0, quadVBO, 0, sizeof(float) * 4);
	glEnableVertexArrayAttrib(quadVAO, 0);
	glVertexArrayAttribFormat(quadVAO, 0, 2, GL_FLOAT, GL_FALSE, 0);
	glVertexArrayAttribBinding(quadVAO, 0, 0);
	glEnableVertexArrayAttrib(quadVAO, 1);
	glVertexArrayAttribFormat(quadVAO, 1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2);
	glVertexArrayAttribBinding(quadVAO, 1, 0);


    // Camera uniforms
    cam.aspect_ratio = 1.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 1;
    cam.max_depth = 5;
    cam.background = color(0, 0, 0);

    cam.vfov = 40;
    cam.lookfrom = point3(278, 278, -800);
    cam.lookat = point3(278, 278, 0);
    cam.vup = vec3(0, 1, 0);

    cam.defocus_angle = 0;

    cam.initialize();

	while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

		processInput(window);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

        shader.use();
        shader.setVec3("camPos", glm::vec3(cam.lookfrom.x(), cam.lookfrom.y(), cam.lookfrom.z()));
        shader.setVec3("camDir", glm::vec3(cam.lookat.x(), cam.lookat.y(), cam.lookat.z()));
        shader.setVec3("w", glm::vec3(cam.w.x(), cam.w.y(), cam.w.z()));
        shader.setVec3("u", glm::vec3(cam.u.x(), cam.u.y(), cam.u.z()));
        shader.setVec3("v", glm::vec3(cam.v.x(), cam.v.y(), cam.v.z()));
        shader.setVec3("pixel00_loc", glm::vec3(cam.pixel00_loc.x(), cam.pixel00_loc.y(), cam.pixel00_loc.z()));
        shader.setVec3("pixel_delta_u", glm::vec3(cam.pixel_delta_u.x(), cam.pixel_delta_u.y(), cam.pixel_delta_u.z()));
        shader.setVec3("pixel_delta_v", glm::vec3(cam.pixel_delta_v.x(), cam.pixel_delta_v.y(), cam.pixel_delta_v.z()));
        shader.setVec3("defocus_disk_u", glm::vec3(cam.defocus_disk_u.x(), cam.defocus_disk_u.y(), cam.defocus_disk_u.z()));
        shader.setVec3("defocus_disk_v", glm::vec3(cam.defocus_disk_v.x(), cam.defocus_disk_v.y(), cam.defocus_disk_v.z()));
        shader.setFloat("defocus_angle", cam.defocus_angle);
        shader.setFloat("focus_dist", cam.focus_dist);

		glDispatchCompute(SCREEN_WIDTH / 16, SCREEN_HEIGHT / 16, 1);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		// Draw the output image
		glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT);

		quadShader.use();
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);

        // Update title
        std::string title = "Path Tracer | FPS: " + std::to_string((int)(1.0f / deltaTime));
        glfwSetWindowTitle(window, title.c_str());

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        cam.lookfrom += cam.w * 1000.0f * deltaTime;
        cam.calculateParameters();
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        cam.lookfrom += -(cam.w * 1000.0f * deltaTime);
        cam.calculateParameters();
    }
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		cam.lookfrom += -(cam.u * 1000.0f * deltaTime);
		cam.calculateParameters();
	}
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
		cam.lookfrom += cam.u * 1000.0f * deltaTime;
		cam.calculateParameters();
    }
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    // cam.processMouse(xoffset, yoffset, 0);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    // camera.processMouse(0, 0, static_cast<float>(yoffset));
}

void glfw_error_callback(int error, const char *description) {
    fprintf(stderr, "GLFW error %d: %s\n", error, description);
}
