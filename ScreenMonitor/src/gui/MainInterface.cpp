#include "gui/MainInterface.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cstring>

// ImGui with full backend support
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

// Error callback for GLFW
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

MainInterface::MainInterface() 
    : m_initialized(false)
    , m_window(nullptr)
    , m_imguiContext(nullptr)
    , m_frameTexture(0)
    , m_textureWidth(0)
    , m_textureHeight(0)
    , m_captureEnabled(false)
    , m_hsvEnabled(false)
    , m_yoloEnabled(false)
    , m_yolov11Enabled(false)
    , m_selectedMonitor(0)
    , m_targetFPS(60.0f)
    , m_currentFPS(0.0f)
    , m_yolov11Confidence(0.5f)
    , m_yolov11NMS(0.4f)
    , m_yolov11Backend(0)
{
    // HSV 기본값 설정
    m_hsvLower[0] = 100; m_hsvLower[1] = 50;  m_hsvLower[2] = 50;
    m_hsvUpper[0] = 130; m_hsvUpper[1] = 255; m_hsvUpper[2] = 255;
    
    // YOLO v11 기본 경로 설정 (cross-platform compatible)
    std::strncpy(m_yolov11ModelPath, "models/yolo11n.onnx", sizeof(m_yolov11ModelPath) - 1);
    m_yolov11ModelPath[sizeof(m_yolov11ModelPath) - 1] = '\0';
    std::strncpy(m_yolov11ClassNamesPath, "models/coco.names", sizeof(m_yolov11ClassNamesPath) - 1);
    m_yolov11ClassNamesPath[sizeof(m_yolov11ClassNamesPath) - 1] = '\0';
}

MainInterface::~MainInterface() {
    Cleanup();
}

bool MainInterface::Initialize() {
    std::cout << "Initializing GUI with GLFW + OpenGL3..." << std::endl;
    
    // GLFW 에러 콜백 설정
    glfwSetErrorCallback(glfw_error_callback);
    
    // GLFW 초기화
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // OpenGL 3.3 Core Profile 설정
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // 윈도우 생성
    m_window = glfwCreateWindow(1280, 720, "Smart Screen Capture - Educational Computer Vision", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // VSync 활성화

    // ImGui 초기화
    IMGUI_CHECKVERSION();
    m_imguiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_imguiContext);
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // ImGui 스타일 설정
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 16.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 3.0f;
    
    // 색상 커스터마이징
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.95f);
    colors[ImGuiCol_Header] = ImVec4(0.2f, 0.3f, 0.4f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.4f, 0.5f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.25f, 0.35f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.2f, 0.3f, 0.4f, 0.9f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.4f, 0.5f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.25f, 0.35f, 1.0f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.9f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 0.9f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.2f, 0.2f, 0.2f, 0.9f);

    // ImGui 백엔드 초기화
    const char* glsl_version = "#version 330";
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // OpenGL 텍스처 생성
    if (!CreateGLTexture()) {
        std::cerr << "Failed to create OpenGL texture" << std::endl;
        return false;
    }

    // FPS 히스토리 초기화
    m_fpsHistory.reserve(FPS_HISTORY_SIZE);

    m_initialized = true;
    std::cout << "MainInterface initialized successfully with GUI enabled" << std::endl;
    return true;
}

void MainInterface::Cleanup() {
    if (m_initialized) {
        std::cout << "Cleaning up GUI..." << std::endl;
        
        // OpenGL 텍스처 정리
        if (m_frameTexture != 0) {
            glDeleteTextures(1, &m_frameTexture);
            m_frameTexture = 0;
        }

        // ImGui 정리
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        if (m_imguiContext) {
            ImGui::DestroyContext(m_imguiContext);
            m_imguiContext = nullptr;
        }

        // GLFW 정리
        if (m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        glfwTerminate();

        m_initialized = false;
        std::cout << "GUI cleanup completed" << std::endl;
    }
}

void MainInterface::Render() {
    if (!m_initialized || !m_window) return;

    // 새 ImGui 프레임 시작
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // 메인 메뉴바
    RenderMenuBar();
    
    // 메인 도킹 공간 설정
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    // SetNextWindowViewport는 docking branch에서만 사용 가능
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // 도킹 스페이스 (간단한 레이아웃으로 대체)
    // ImGui::DockSpace는 특별한 docking branch에서만 사용 가능
    ImGui::Text("SmartScreenCapture Dashboard");
    
    // 각 패널 렌더링
    RenderCapturePanel();
    RenderDetectionPanel();
    RenderPerformancePanel();
    RenderFrame();
    
    ImGui::End();
    
    // 상태바
    RenderStatusBar();

    // 렌더링 완료
    ImGui::Render();
    
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glfwSwapBuffers(m_window);
}

void MainInterface::HandleEvents() {
    if (!m_initialized || !m_window) return;
    
    glfwPollEvents();
}

void MainInterface::UpdateFrame(const cv::Mat& frame) {
    if (!m_initialized) return;
    
    // Frame update with OpenGL texture
    if (!frame.empty()) {
        m_textureWidth = frame.cols;
        m_textureHeight = frame.rows;
        m_currentFrame = frame.clone();
        UpdateGLTexture(frame);
    }
}

void MainInterface::UpdateDetections(const std::vector<cv::Rect>& detections) {
    if (!m_initialized) return;
    
    m_detections = detections;
}

void MainInterface::SetMonitorList(const std::vector<std::string>& monitors) {
    m_monitorList = monitors;
    if (m_selectedMonitor >= static_cast<int>(monitors.size())) {
        m_selectedMonitor = 0;
    }
}

void MainInterface::UpdateFPS(float fps) {
    m_currentFPS = fps;
    
    // FPS 히스토리 업데이트
    m_fpsHistory.push_back(fps);
    if (m_fpsHistory.size() > FPS_HISTORY_SIZE) {
        m_fpsHistory.erase(m_fpsHistory.begin());
    }
}

bool MainInterface::ShouldClose() const {
    return m_window ? glfwWindowShouldClose(m_window) : false;
}

// Private helper methods
bool MainInterface::CreateGLTexture() {
    glGenTextures(1, &m_frameTexture);
    if (m_frameTexture == 0) {
        std::cerr << "Failed to generate OpenGL texture" << std::endl;
        return false;
    }
    
    glBindTexture(GL_TEXTURE_2D, m_frameTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F); // GL_CLAMP_TO_EDGE
    
    // 기본 크기로 초기화 (1x1 픽셀)
    unsigned char pixel[4] = {0, 0, 0, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void MainInterface::UpdateGLTexture(const cv::Mat& frame) {
    if (m_frameTexture == 0 || frame.empty()) return;
    
    // OpenCV Mat을 OpenGL 텍스처로 업로드
    glBindTexture(GL_TEXTURE_2D, m_frameTexture);
    
    // BGR을 RGB로 변환 (OpenCV는 BGR, OpenGL은 RGB)
    cv::Mat rgb_frame;
    cv::cvtColor(frame, rgb_frame, cv::COLOR_BGR2RGB);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb_frame.cols, rgb_frame.rows, 
                 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_frame.data);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

void MainInterface::RenderFrame() {
    ImGui::Begin("Live Capture");
    
    if (m_frameTexture != 0 && m_textureWidth > 0 && m_textureHeight > 0) {
        // 사용 가능한 영역 크기
        ImVec2 region = ImGui::GetContentRegionAvail();
        
        // 종횡비 유지하면서 크기 조정
        float aspect = static_cast<float>(m_textureWidth) / static_cast<float>(m_textureHeight);
        float display_w = region.x;
        float display_h = display_w / aspect;
        
        if (display_h > region.y) {
            display_h = region.y;
            display_w = display_h * aspect;
        }
        
        // 중앙 정렬
        ImVec2 cursor_pos = ImGui::GetCursorPos();
        cursor_pos.x += (region.x - display_w) * 0.5f;
        cursor_pos.y += (region.y - display_h) * 0.5f;
        ImGui::SetCursorPos(cursor_pos);
        
        // 텍스처 렌더링
        ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(m_frameTexture)), 
                     ImVec2(display_w, display_h));
        
        // 검출 결과 오버레이 (TODO: 구현)
        if (!m_detections.empty()) {
            ImGui::Text("Detections: %zu objects", m_detections.size());
        }
    } else {
        ImGui::Text("No video feed available");
        ImGui::Text("Start capture to see live video");
    }
    
    ImGui::End();
}

void MainInterface::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Config")) {
                // TODO: 설정 저장
            }
            if (ImGui::MenuItem("Load Config")) {
                // TODO: 설정 로드
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(m_window, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset Layout")) {
                // TODO: 레이아웃 리셋
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                // TODO: About 다이얼로그
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void MainInterface::RenderCapturePanel() {
    ImGui::Begin("Capture Settings");
    
    // 캡처 시작/중지 버튼
    if (m_captureEnabled) {
        if (ImGui::Button("Stop Capture", ImVec2(-1, 30))) {
            m_captureEnabled = false;
        }
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    } else {
        if (ImGui::Button("Start Capture", ImVec2(-1, 30))) {
            m_captureEnabled = true;
        }
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    }
    ImGui::PopStyleColor();
    
    ImGui::Separator();
    
    // 모니터 선택
    if (!m_monitorList.empty()) {
        ImGui::Text("Monitor:");
        if (ImGui::BeginCombo("##monitor", m_monitorList[m_selectedMonitor].c_str())) {
            for (int i = 0; i < static_cast<int>(m_monitorList.size()); i++) {
                bool is_selected = (m_selectedMonitor == i);
                if (ImGui::Selectable(m_monitorList[i].c_str(), is_selected)) {
                    m_selectedMonitor = i;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
    
    // FPS 설정
    ImGui::Text("Target FPS:");
    ImGui::SliderFloat("##target_fps", &m_targetFPS, 1.0f, 120.0f, "%.1f");
    
    ImGui::Separator();
    
    // 현재 상태
    ImGui::Text("Status: %s", m_captureEnabled ? "Capturing" : "Stopped");
    ImGui::Text("Current FPS: %.1f", m_currentFPS);
    
    ImGui::End();
}

void MainInterface::RenderDetectionPanel() {
    ImGui::Begin("Detection Settings");
    
    // HSV 검출 설정
    ImGui::Checkbox("HSV Color Detection", &m_hsvEnabled);
    if (m_hsvEnabled) {
        ImGui::Text("HSV Lower Bound:");
        ImGui::SliderInt3("##hsv_lower", m_hsvLower, 0, 255);
        ImGui::Text("HSV Upper Bound:");
        ImGui::SliderInt3("##hsv_upper", m_hsvUpper, 0, 255);
    }
    
    ImGui::Separator();
    
    // YOLO v11 검출 설정
    ImGui::Checkbox("YOLO v11 Detection", &m_yolov11Enabled);
    if (m_yolov11Enabled) {
        ImGui::Text("Model Path:");
        ImGui::InputText("##yolo_model", m_yolov11ModelPath, sizeof(m_yolov11ModelPath));
        
        ImGui::Text("Class Names:");
        ImGui::InputText("##yolo_classes", m_yolov11ClassNamesPath, sizeof(m_yolov11ClassNamesPath));
        
        ImGui::Text("Confidence Threshold:");
        ImGui::SliderFloat("##confidence", &m_yolov11Confidence, 0.1f, 1.0f, "%.2f");
        
        ImGui::Text("NMS Threshold:");
        ImGui::SliderFloat("##nms", &m_yolov11NMS, 0.1f, 1.0f, "%.2f");
        
        ImGui::Text("Backend:");
        const char* backends[] = {"OpenCV DNN", "ONNX Runtime GPU"};
        ImGui::Combo("##backend", &m_yolov11Backend, backends, 2);
    }
    
    ImGui::Separator();
    
    // 검출 결과
    ImGui::Text("Detections: %zu", m_detections.size());
    
    ImGui::End();
}

void MainInterface::RenderPerformancePanel() {
    ImGui::Begin("Performance Monitor");
    
    // FPS 그래프
    if (!m_fpsHistory.empty()) {
        ImGui::Text("FPS History");
        ImGui::PlotLines("##fps_plot", m_fpsHistory.data(), static_cast<int>(m_fpsHistory.size()),
                        0, nullptr, 0.0f, 150.0f, ImVec2(0, 80));
    }
    
    // 성능 통계
    ImGui::Separator();
    ImGui::Text("Performance Statistics:");
    ImGui::Text("Current FPS: %.1f", m_currentFPS);
    
    if (!m_fpsHistory.empty()) {
        float avg_fps = 0.0f;
        for (float fps : m_fpsHistory) {
            avg_fps += fps;
        }
        avg_fps /= m_fpsHistory.size();
        ImGui::Text("Average FPS: %.1f", avg_fps);
    }
    
    // 메모리 사용량 (추후 구현)
    ImGui::Text("Memory Usage: N/A");
    ImGui::Text("GPU Usage: N/A");
    
    ImGui::End();
}

void MainInterface::RenderStatusBar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 work_size = viewport->WorkSize;
    
    ImGui::SetNextWindowPos(ImVec2(work_pos.x, work_pos.y + work_size.y - 25));
    ImGui::SetNextWindowSize(ImVec2(work_size.x, 25));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoSavedSettings;
    
    ImGui::Begin("StatusBar", nullptr, flags);
    
    ImGui::Text("Ready | FPS: %.1f | Status: %s", 
                m_currentFPS, 
                m_captureEnabled ? "Capturing" : "Stopped");
    
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    ImGui::Text("GUI: Enabled");
    
    ImGui::End();
}