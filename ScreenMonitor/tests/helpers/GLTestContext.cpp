#include "GLTestContext.h"
#include <iostream>
#include <string>
#include <GL/gl.h>

bool GLTestContext::s_glfw_initialized = false;
int GLTestContext::s_instance_count = 0;

GLTestContext::GLTestContext() {
    s_instance_count++;
}

GLTestContext::~GLTestContext() {
    Cleanup();
    s_instance_count--;
    
    // 마지막 인스턴스가 파괴될 때 GLFW 정리
    if (s_instance_count == 0 && s_glfw_initialized) {
        glfwTerminate();
        s_glfw_initialized = false;
    }
}

bool GLTestContext::Initialize(bool headless) {
    if (m_initialized) {
        return true;
    }
    
    m_headless = headless;
    
    if (!InitializeGLFW()) {
        return false;
    }
    
    if (!CreateWindow()) {
        return false;
    }
    
    if (!InitializeGL()) {
        return false;
    }
    
    m_initialized = true;
    return true;
}

void GLTestContext::Cleanup() {
    if (m_initialized) {
        if (m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        m_initialized = false;
    }
}

void GLTestContext::MakeCurrent() {
    if (m_window) {
        glfwMakeContextCurrent(m_window);
    }
}

std::string GLTestContext::GetOpenGLVersion() const {
    if (!m_initialized) return "";
    
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    return version ? std::string(version) : "";
}

std::string GLTestContext::GetRenderer() const {
    if (!m_initialized) return "";
    
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    return renderer ? std::string(renderer) : "";
}

void GLTestContext::SetFramebufferSize(int width, int height) {
    m_framebuffer_width = width;
    m_framebuffer_height = height;
    
    if (m_window) {
        glfwSetWindowSize(m_window, width, height);
    }
}

std::pair<int, int> GLTestContext::GetFramebufferSize() const {
    if (m_window) {
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);
        return {width, height};
    }
    return {m_framebuffer_width, m_framebuffer_height};
}

std::string GLTestContext::CheckGLError() const {
    GLenum error = glGetError();
    switch (error) {
        case GL_NO_ERROR:
            return "";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
        default:
            return "Unknown OpenGL error: " + std::to_string(error);
    }
}

void GLTestContext::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void GLTestContext::Clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLTestContext::SwapBuffers() {
    if (m_window) {
        glfwSwapBuffers(m_window);
    }
}

unsigned int GLTestContext::CreateTestTexture(int width, int height, unsigned int format) {
    unsigned int texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    
    // 텍스처 파라미터 설정
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // 빈 텍스처 생성
    std::vector<unsigned char> data(width * height * 3, 128); // 회색으로 초기화
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data.data());
    
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture_id;
}

void GLTestContext::DeleteTexture(unsigned int texture_id) {
    glDeleteTextures(1, &texture_id);
}

GLTestContext& GLTestContext::GetInstance() {
    static GLTestContext instance;
    return instance;
}

bool GLTestContext::IsGLFWInitialized() {
    return s_glfw_initialized;
}

void GLTestContext::ErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void GLTestContext::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

bool GLTestContext::InitializeGLFW() {
    if (s_glfw_initialized) {
        return true;
    }
    
    glfwSetErrorCallback(ErrorCallback);
    
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    
    s_glfw_initialized = true;
    return true;
}

bool GLTestContext::CreateWindow() {
    // OpenGL 3.3 Core Profile 설정
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    if (m_headless) {
        // 헤드리스 모드: 보이지 않는 윈도우 생성
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    m_window = glfwCreateWindow(m_framebuffer_width, m_framebuffer_height, 
                               "GLTestContext", nullptr, nullptr);
    
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        return false;
    }
    
    glfwMakeContextCurrent(m_window);
    glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
    
    if (!m_headless) {
        glfwSwapInterval(1); // VSync 활성화
    }
    
    return true;
}

bool GLTestContext::InitializeGL() {
    // OpenGL 버전 확인
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if (!version) {
        std::cerr << "Failed to get OpenGL version" << std::endl;
        return false;
    }
    
    std::cout << "OpenGL Version: " << version << std::endl;
    
    // 기본 OpenGL 설정
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // 블렌딩 설정 (투명도 지원)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // 뷰포트 설정
    SetViewport(0, 0, m_framebuffer_width, m_framebuffer_height);
    
    return true;
}