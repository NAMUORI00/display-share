#include "gui/MainInterface.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <algorithm>

// Capture interface (CaptureSettings 정의 포함)
#include "interfaces/ICaptureDevice.h"

// ImGui core (always available)
#include <imgui.h>
#include <imgui_internal.h>

#if GUI_ENABLED
// ImGui with full backend support
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#include <GL/gl.h>
// Windows OpenGL 확장 상수 정의
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#else
#include <OpenGL/gl.h>
#endif

// Error callback for GLFW
static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}
#endif

MainInterface::MainInterface() 
    : m_initialized(false)
#if GUI_ENABLED
    , m_window(nullptr)
    , m_imguiContext(nullptr)
    , m_frameTexture(0)
    , m_textureWidth(0)
    , m_textureHeight(0)
#endif
    , m_centerCapture(std::make_unique<CenterRegionCapture>())
    , m_metrics(std::make_unique<ScreenMonitor::SimpleMetrics>())
    , m_captureEnabled(false)
    , m_hsvEnabled(true)
    , m_yoloEnabled(true)
    , m_yolov11Enabled(true)
    , m_showAboutDialog(false)
    , m_showROIOverlay(true)
    , m_showDetectionStats(true)
    , m_currentFPS(0.0f)
    , m_targetFPS(60.0f)
    , m_autoScrollConsole(true)
    , m_coutRedirect(nullptr)
    , m_cerrRedirect(nullptr)
    , m_originalCout(nullptr)
    , m_originalCerr(nullptr)
    , m_configManager()
    , m_configFilePath("config/config.json")
{
    // 기본 YOLO v11 설정
    std::strncpy(m_yolov11ModelPath, "models/yolo11n.onnx", sizeof(m_yolov11ModelPath) - 1);
    m_yolov11ModelPath[sizeof(m_yolov11ModelPath) - 1] = '\0';
    
    std::strncpy(m_yolov11ClassNamesPath, "models/coco.names", sizeof(m_yolov11ClassNamesPath) - 1);
    m_yolov11ClassNamesPath[sizeof(m_yolov11ClassNamesPath) - 1] = '\0';
    
    // FPS 히스토리 초기화
    m_fpsHistory.reserve(FPS_HISTORY_SIZE);

    // 캡처 디바이스 초기화 지연 생성 (StartCapture에서 생성)
}

MainInterface::~MainInterface() {
    Cleanup();
}

bool MainInterface::Initialize() {
#if GUI_ENABLED
    // 설정 파일 로드
    if (m_configManager.loadConfig(m_configFilePath)) {
        std::cout << "Configuration loaded from " << m_configFilePath << std::endl;
        ApplyConfiguration();
    } else {
        std::cerr << "Failed to load configuration, using defaults." << std::endl;
    }

    std::cout << "Initializing Modern 320x320 ROI GUI System..." << std::endl;
    
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

    // 현대적인 윈도우 생성 (더 큰 해상도)
    m_window = glfwCreateWindow(1600, 900, "Professional 320x320 ROI Detection System", nullptr, nullptr);
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
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;    // 도킹 활성화
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // 멀티 뷰포트 활성화

    // 현대적인 테마 적용
    ApplyModernTheme();

    // 백엔드 초기화
    if (!ImGui_ImplGlfw_InitForOpenGL(m_window, true)) {
        std::cerr << "Failed to initialize ImGui GLFW backend" << std::endl;
        return false;
    }
    
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        std::cerr << "Failed to initialize ImGui OpenGL3 backend" << std::endl;
        return false;
    }

    // OpenGL 텍스처 생성
    if (!CreateGLTexture()) {
        std::cerr << "Failed to create OpenGL texture" << std::endl;
        return false;
    }

    // 스트림 리다이렉션 설정
    SetupStreamRedirection();

    m_initialized = true;
    std::cout << "Modern 320x320 ROI GUI System initialized successfully!" << std::endl;
    // 자동 캡처 시작을 원하면 다음 줄을 활성화
    // StartCapture(0);
    
    return true;
#else
    std::cout << "GUI disabled, running in headless mode" << std::endl;
    m_initialized = true;
    return true;
#endif
}

void MainInterface::ApplyModernTheme() {
#if GUI_ENABLED
    ImGuiStyle& style = ImGui::GetStyle();
    
    // 현대적인 색상 스키마 (다크 테마 기반)
    ImVec4* colors = style.Colors;
    
    // 메인 배경색들
    colors[ImGuiCol_WindowBg]        = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    colors[ImGuiCol_ChildBg]         = ImVec4(0.16f, 0.17f, 0.18f, 1.00f);
    colors[ImGuiCol_PopupBg]         = ImVec4(0.20f, 0.21f, 0.22f, 0.92f);
    
    // 프레임 배경
    colors[ImGuiCol_FrameBg]         = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.30f, 0.31f, 0.32f, 1.00f);
    colors[ImGuiCol_FrameBgActive]   = ImVec4(0.35f, 0.36f, 0.37f, 1.00f);
    
    // 버튼 색상
    colors[ImGuiCol_Button]          = ImVec4(0.20f, 0.51f, 0.91f, 1.00f);
    colors[ImGuiCol_ButtonHovered]   = ImVec4(0.25f, 0.56f, 0.96f, 1.00f);
    colors[ImGuiCol_ButtonActive]    = ImVec4(0.15f, 0.46f, 0.86f, 1.00f);
    
    // 헤더 색상
    colors[ImGuiCol_Header]          = ImVec4(0.20f, 0.51f, 0.91f, 0.31f);
    colors[ImGuiCol_HeaderHovered]   = ImVec4(0.25f, 0.56f, 0.96f, 0.80f);
    colors[ImGuiCol_HeaderActive]    = ImVec4(0.20f, 0.51f, 0.91f, 1.00f);
    
    // 텍스트 색상
    colors[ImGuiCol_Text]            = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled]    = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
    
    // 도킹 관련
    colors[ImGuiCol_DockingPreview]  = ImVec4(0.20f, 0.51f, 0.91f, 0.78f);
    colors[ImGuiCol_DockingEmptyBg]  = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    
    // 테두리 및 분리선
    colors[ImGuiCol_Border]          = ImVec4(0.30f, 0.31f, 0.32f, 1.00f);
    colors[ImGuiCol_Separator]       = ImVec4(0.30f, 0.31f, 0.32f, 1.00f);
    
    // 전문적인 스타일 설정
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;
    
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize  = 1.0f;
    style.PopupBorderSize  = 1.0f;
    style.FrameBorderSize  = 0.0f;
    
    style.WindowPadding    = ImVec2(8.0f, 8.0f);
    style.FramePadding     = ImVec2(6.0f, 4.0f);
    style.ItemSpacing      = ImVec2(6.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.IndentSpacing    = 25.0f;
    style.ScrollbarSize    = 15.0f;
    style.GrabMinSize      = 10.0f;
#endif
}

void MainInterface::Render() {
#if GUI_ENABLED
    if (!m_initialized || !m_window) return;

    // 새 ImGui 프레임 시작
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // 메인 도킹 공간 설정
    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    
    // 첫 실행 시 레이아웃 초기화
    static bool first_time = true;
    if (first_time) {
        first_time = false;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        // 현대적인 3패널 레이아웃
        auto dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.35f, nullptr, &dockspace_id);
        auto dock_id_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.35f, nullptr, &dockspace_id);
        auto dock_id_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);

        // 패널 배치
        ImGui::DockBuilderDockWindow("320x320 ROI Visualization", dockspace_id);          // 중앙: ROI 시각화
        ImGui::DockBuilderDockWindow("Detection Results", dock_id_left);                  // 좌측: 검출 결과
        ImGui::DockBuilderDockWindow("Control Panel", dock_id_right);                     // 우측: 제어 패널
        ImGui::DockBuilderDockWindow("Performance Dashboard", dock_id_bottom);            // 하단: 성능 대시보드

        ImGui::DockBuilderFinish(dockspace_id);
    }

    // 메뉴바 렌더링
    RenderMenuBar();

    // 현대화된 패널들 렌더링
    RenderROIVisualizationPanel();    // 320x320 ROI 시각화
    RenderDetectionResultsPanel();    // 검출 결과 표시
    RenderControlPanel();             // 간소화된 제어
    RenderPerformanceDashboard();     // 성능 모니터링

    // 프레임 렌더링 후 캡처 프레임 폴링 (UI 이벤트와 분리)
    PollCaptureFrame();

    // 상태바 렌더링
    RenderStatusBar();

    // About 다이얼로그
    RenderAboutDialog();

    // ImGui 렌더링
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // 멀티 뷰포트 업데이트
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(m_window);
#endif
}

void MainInterface::RenderROIVisualizationPanel() {
#if GUI_ENABLED
    if (ImGui::Begin("320x320 ROI Visualization")) {
        
        // 패널 헤더
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
        ImGui::Text("🎯 Center Region Capture (320x320)");
        ImGui::PopStyleColor();
        ImGui::Separator();

        // ROI 프레임 표시
        if (!m_roiFrame.empty() && m_frameTexture != 0) {
            // 사용 가능한 영역 크기 계산
            const ImVec2 content_region = ImGui::GetContentRegionAvail();
            const float aspect_ratio = 1.0f; // 320x320 = 1:1 비율
            
            // 320x320 이미지에 맞는 크기 계산
            float display_width = (std::min)(content_region.x - 20.0f, content_region.y - 100.0f);
            float display_height = display_width / aspect_ratio;
            
            if (display_height > content_region.y - 100.0f) {
                display_height = content_region.y - 100.0f;
                display_width = display_height * aspect_ratio;
            }

            // 중앙 정렬을 위한 커서 위치 조정
            const float center_x = (content_region.x - display_width) * 0.5f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + center_x);

            // 320x320 ROI 이미지 표시
            ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(m_frameTexture)), 
                        ImVec2(display_width, display_height));

            // 검출 오버레이 렌더링
            if (m_showROIOverlay) {
                RenderDetectionOverlay(m_roiFrame);
            }

            // ROI 정보 표시
            ImGui::Spacing();
            ImGui::Text("Region Info:");
            ImGui::Text("  Position: (%d, %d)", m_regionInfo.x, m_regionInfo.y);
            ImGui::Text("  Size: %dx%d", m_regionInfo.width, m_regionInfo.height);
            ImGui::Text("  Source: %dx%d", m_regionInfo.source_width, m_regionInfo.source_height);
            
        } else {
            // 프레임이 없을 때 플레이스홀더
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - 200.0f) * 0.5f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 50.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::Text("Waiting for 320x320 ROI capture...");
            ImGui::PopStyleColor();
        }

        // 제어 버튼들
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Checkbox("Show Detection Overlay", &m_showROIOverlay)) {
            // 설정 저장
        }
    }
    ImGui::End();
#endif
}

void MainInterface::RenderDetectionResultsPanel() {
#if GUI_ENABLED
    if (ImGui::Begin("Detection Results")) {
        
        // 패널 헤더
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        ImGui::Text("🔍 Real-time Detection Results");
        ImGui::PopStyleColor();
        ImGui::Separator();

        // HSV 검출 결과
        if (ImGui::CollapsingHeader("HSV Color Detection", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("Status: %s", m_hsvEnabled ? "Active" : "Inactive");
            ImGui::PopStyleColor();
            
            if (m_hsvEnabled && !m_hsvDetections.empty()) {
                ImGui::Text("Detected Points: %zu", m_hsvDetections.size());
                
                // 최근 검출된 좌표들 표시 (최대 10개)
                const size_t max_display = (std::min)(m_hsvDetections.size(), size_t(10));
                for (size_t i = 0; i < max_display; ++i) {
                    const cv::Point& pt = m_hsvDetections[i];
                    ImGui::Text("  Point %zu: (%d, %d)", i + 1, pt.x, pt.y);
                }
                if (m_hsvDetections.size() > max_display) {
                    ImGui::Text("  ... and %zu more points", m_hsvDetections.size() - max_display);
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::Text("No HSV detections");
                ImGui::PopStyleColor();
            }
        }

        // YOLO v11 검출 결과
        if (ImGui::CollapsingHeader("YOLO v11 Object Detection", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("Status: %s", m_yoloEnabled ? "Active" : "Inactive");
            ImGui::PopStyleColor();
            
            if (m_yoloEnabled && !m_yoloDetections.empty()) {
                ImGui::Text("Detected Objects: %zu", m_yoloDetections.size());
                
                // 검출된 객체들 표시
                for (size_t i = 0; i < m_yoloDetections.size() && i < 10; ++i) {
                    const cv::Rect& rect = m_yoloDetections[i];
                    const float confidence = (i < m_yoloConfidences.size()) ? m_yoloConfidences[i] : 0.0f;
                    const std::string& class_name = (i < m_yoloClassNames.size()) ? m_yoloClassNames[i] : "unknown";
                    
                    ImGui::Text("  %s: %.2f%% [%d,%d,%dx%d]", 
                               class_name.c_str(), confidence * 100.0f,
                               rect.x, rect.y, rect.width, rect.height);
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::Text("No YOLO detections");
                ImGui::PopStyleColor();
            }
        }

        // 검출 통계
        if (m_showDetectionStats && ImGui::CollapsingHeader("Detection Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Total HSV Points: %zu", m_hsvDetections.size());
            ImGui::Text("Total YOLO Objects: %zu", m_yoloDetections.size());
            
            // 평균 신뢰도 계산
            if (!m_yoloConfidences.empty()) {
                float avg_confidence = 0.0f;
                for (float conf : m_yoloConfidences) {
                    avg_confidence += conf;
                }
                avg_confidence /= m_yoloConfidences.size();
                ImGui::Text("Avg YOLO Confidence: %.1f%%", avg_confidence * 100.0f);
            }
        }
    }
    ImGui::End();
#endif
}

void MainInterface::RenderControlPanel() {
#if GUI_ENABLED
    if (ImGui::Begin("Control Panel")) {
        
        // 패널 헤더
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
        ImGui::Text("⚙️ Detection Control");
        ImGui::PopStyleColor();
        ImGui::Separator();

        // 모니터 목록 새로고침
        if (ImGui::Button("🔄 Refresh Monitors")) {
            RefreshMonitorList();
        }
        if (!m_monitors.empty()) {
            std::string currentLabel;
            if (m_selectedMonitor < static_cast<int>(m_monitors.size())) {
                const auto& mon = m_monitors[m_selectedMonitor];
                currentLabel = std::to_string(mon.index) + ": " + mon.name;
            } else {
                currentLabel = "Select Monitor";
            }
            if (ImGui::BeginCombo("Monitor", currentLabel.c_str())) {
                for (int i = 0; i < static_cast<int>(m_monitors.size()); ++i) {
                    bool selected = (i == m_selectedMonitor);
                    const auto& mon = m_monitors[i];
                    std::string label = std::to_string(mon.index) + ": " + mon.name + " (" + std::to_string(mon.width) + "x" + std::to_string(mon.height) + ")";
                    if (ImGui::Selectable(label.c_str(), selected)) {
                        m_selectedMonitor = i;
                        if (m_captureEnabled) {
                            StopCapture();
                            StartCapture(mon.index);
                        }
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::TextDisabled("No monitors detected");
        }

        bool canStart = !m_captureEnabled && !m_monitors.empty();
        if (!canStart) ImGui::BeginDisabled(true);
        if (ImGui::Button(m_captureEnabled ? "🛑 Stop Capture" : "▶️ Start Capture", ImVec2(-1, 40))) {
            if (!m_captureEnabled) {
                int monIndex = (m_selectedMonitor < static_cast<int>(m_monitors.size())) ? m_monitors[m_selectedMonitor].index : 0;
                if (!StartCapture(monIndex)) {
                    AddLogMessage("Failed to start capture", 2);
                }
            } else {
                StopCapture();
            }
        }
        if (!canStart) ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();

        // HSV 설정
        if (ImGui::CollapsingHeader("HSV Color Detection", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enable HSV Detection", &m_hsvEnabled);
            
            ImGui::Text("HSV Lower Bound:");
            ImGui::SliderInt("H Min", &m_hsvLower[0], 0, 179);
            ImGui::SliderInt("S Min", &m_hsvLower[1], 0, 255);
            ImGui::SliderInt("V Min", &m_hsvLower[2], 0, 255);
            
            ImGui::Text("HSV Upper Bound:");
            ImGui::SliderInt("H Max", &m_hsvUpper[0], 0, 179);
            ImGui::SliderInt("S Max", &m_hsvUpper[1], 0, 255);
            ImGui::SliderInt("V Max", &m_hsvUpper[2], 0, 255);
        }

        // YOLO v11 설정
        if (ImGui::CollapsingHeader("YOLO v11 Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enable YOLO Detection", &m_yolov11Enabled);
            
            ImGui::SliderFloat("Confidence Threshold", &m_yolov11Confidence, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("NMS Threshold", &m_yolov11NMS, 0.0f, 1.0f, "%.2f");
            
            if (ImGui::Button("Load YOLO Model", ImVec2(-1, 25))) {
                // 모델 로드 트리거
            }
        }

        // 성능 설정
        if (ImGui::CollapsingHeader("Performance Settings")) {
            ImGui::SliderFloat("Target FPS", &m_targetFPS, 30.0f, 120.0f, "%.0f");
            ImGui::Checkbox("Show Detection Statistics", &m_showDetectionStats);
            ImGui::Checkbox("Show ROI Overlay", &m_showROIOverlay);
        }

        // 설정 저장/로드
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Save Settings", ImVec2(-1, 25))) {
            SaveConfiguration();
        }
    }
    ImGui::End();
#endif
}

void MainInterface::RenderPerformanceDashboard() {
#if GUI_ENABLED
    if (ImGui::Begin("Performance Dashboard")) {
        
        // 패널 헤더
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.6f, 1.0f));
        ImGui::Text("📊 System Performance Metrics");
        ImGui::PopStyleColor();
        ImGui::Separator();

        // 실시간 메트릭 표시
        if (m_metrics) {
            const double current_fps = m_metrics->getCurrentFPS();
            const double process_time = m_metrics->getCurrentProcessTime();
            
            // FPS 표시 (색상 코딩)
            ImVec4 fps_color = (current_fps >= m_targetFPS * 0.9f) ? 
                               ImVec4(0.2f, 1.0f, 0.4f, 1.0f) :   // 녹색: 좋음
                               (current_fps >= m_targetFPS * 0.7f) ? 
                               ImVec4(1.0f, 0.8f, 0.2f, 1.0f) :   // 노랑: 보통
                               ImVec4(1.0f, 0.3f, 0.2f, 1.0f);    // 빨강: 나쁨
            
            ImGui::PushStyleColor(ImGuiCol_Text, fps_color);
            ImGui::Text("FPS: %.1f / %.0f", current_fps, m_targetFPS);
            ImGui::PopStyleColor();
            
            ImGui::Text("Process Time: %.2f ms", process_time);
            
            // FPS 그래프
            if (ImGui::CollapsingHeader("FPS History", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (!m_fpsHistory.empty()) {
                    ImGui::PlotLines("FPS", m_fpsHistory.data(), static_cast<int>(m_fpsHistory.size()),
                                    0, nullptr, 0.0f, m_targetFPS * 1.2f, ImVec2(0, 80));
                }
            }
        }

        // 320x320 ROI 성능 메트릭
        if (ImGui::CollapsingHeader("ROI Processing Metrics", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (m_centerCapture) {
                const auto perf_metrics = m_centerCapture->GetPerformanceMetrics();
                ImGui::Text("ROI Extractions: %llu", perf_metrics.total_extractions);
                ImGui::Text("Avg Extraction Time: %.3f ms", perf_metrics.average_time_ms);
                ImGui::Text("Memory Usage: %.1f KB", perf_metrics.memory_usage_bytes / 1024.0);
                
                if (ImGui::Button("Reset ROI Metrics", ImVec2(-1, 25))) {
                    m_centerCapture->ResetPerformanceMetrics();
                }
            }
        }

        // 시스템 상태
        if (ImGui::CollapsingHeader("System Status")) {
            ImGui::Text("Capture Status: %s", m_captureEnabled ? "Running" : "Stopped");
            ImGui::Text("HSV Detection: %s", m_hsvEnabled ? "Enabled" : "Disabled");
            ImGui::Text("YOLO Detection: %s", m_yolov11Enabled ? "Enabled" : "Disabled");
            ImGui::Text("ROI Size: 320x320 pixels");
        }
    }
    ImGui::End();
#endif
}

void MainInterface::RenderDetectionOverlay(const cv::Mat& frame) {
#if GUI_ENABLED
    if (frame.empty()) return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 canvas_pos = ImGui::GetItemRectMin();
    const ImVec2 canvas_size = ImGui::GetItemRectSize();

    // HSV 검출 포인트 표시
    if (m_hsvEnabled && !m_hsvDetections.empty()) {
        for (const auto& point : m_hsvDetections) {
            const float x = canvas_pos.x + (point.x / 320.0f) * canvas_size.x;
            const float y = canvas_pos.y + (point.y / 320.0f) * canvas_size.y;
            
            draw_list->AddCircleFilled(ImVec2(x, y), 3.0f, IM_COL32(0, 255, 0, 255));
            draw_list->AddCircle(ImVec2(x, y), 5.0f, IM_COL32(0, 255, 0, 180), 0, 1.5f);
        }
    }

    // YOLO 바운딩 박스 표시
    if (m_yoloEnabled && !m_yoloDetections.empty()) {
        for (size_t i = 0; i < m_yoloDetections.size(); ++i) {
            const cv::Rect& rect = m_yoloDetections[i];
            
            const float x1 = canvas_pos.x + (rect.x / 320.0f) * canvas_size.x;
            const float y1 = canvas_pos.y + (rect.y / 320.0f) * canvas_size.y;
            const float x2 = canvas_pos.x + ((rect.x + rect.width) / 320.0f) * canvas_size.x;
            const float y2 = canvas_pos.y + ((rect.y + rect.height) / 320.0f) * canvas_size.y;
            
            // 바운딩 박스
            draw_list->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(255, 100, 0, 255), 0.0f, 0, 2.0f);
            
            // 클래스 이름과 신뢰도 표시
            if (i < m_yoloClassNames.size() && i < m_yoloConfidences.size()) {
                const std::string label = m_yoloClassNames[i] + 
                                         " (" + std::to_string(static_cast<int>(m_yoloConfidences[i] * 100)) + "%)";
                
                draw_list->AddRectFilled(ImVec2(x1, y1 - 20), ImVec2(x1 + label.length() * 7, y1), 
                                        IM_COL32(255, 100, 0, 200));
                draw_list->AddText(ImVec2(x1 + 2, y1 - 18), IM_COL32(255, 255, 255, 255), label.c_str());
            }
        }
    }
#endif
}

// 업데이트 함수들 구현
void MainInterface::UpdateROIFrame(const cv::Mat& roi_frame, const CenterRegionCapture::RegionInfo& region_info) {
    if (!roi_frame.empty()) {
        m_roiFrame = roi_frame.clone();
        m_regionInfo = region_info;
        UpdateGLTexture(roi_frame);
    }
}

void MainInterface::UpdateHSVDetections(const std::vector<cv::Point>& hsv_detections) {
    m_hsvDetections = hsv_detections;
}

void MainInterface::UpdateYOLODetections(const std::vector<cv::Rect>& yolo_detections, 
                                        const std::vector<float>& confidence_scores,
                                        const std::vector<std::string>& class_names) {
    m_yoloDetections = yolo_detections;
    m_yoloConfidences = confidence_scores;
    m_yoloClassNames = class_names;
}

void MainInterface::UpdateFPS(float fps) {
    m_currentFPS = fps;
    
    // FPS 히스토리 업데이트
    m_fpsHistory.push_back(fps);
    if (m_fpsHistory.size() > FPS_HISTORY_SIZE) {
        m_fpsHistory.erase(m_fpsHistory.begin());
    }
    
    // SimpleMetrics 업데이트
    if (m_metrics) {
        m_metrics->updateFPS(fps);
    }
}

// 기존 함수들 유지 (축약된 버전으로 구현)
void MainInterface::RenderMenuBar() {
#if GUI_ENABLED
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("System")) {
            if (ImGui::MenuItem("Start Detection", "Ctrl+S", nullptr, !m_captureEnabled)) {
                m_captureEnabled = true;
            }
            if (ImGui::MenuItem("Stop Detection", "Ctrl+Q", nullptr, m_captureEnabled)) {
                m_captureEnabled = false;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Configuration", "Ctrl+Shift+S")) {
                SaveConfiguration();
            }
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(m_window, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("ROI Overlay", nullptr, &m_showROIOverlay);
            ImGui::MenuItem("Detection Stats", nullptr, &m_showDetectionStats);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                m_showAboutDialog = true;
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
#endif
}

void MainInterface::RenderStatusBar() {
#if GUI_ENABLED
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 work_size = viewport->WorkSize;
    
    ImGui::SetNextWindowPos(ImVec2(work_pos.x, work_pos.y + work_size.y - 25));
    ImGui::SetNextWindowSize(ImVec2(work_size.x, 25));
    
    if (ImGui::Begin("StatusBar", nullptr, 
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
        
        ImGui::Text("Status: %s | FPS: %.1f | HSV: %zu points | YOLO: %zu objects", 
                   m_captureEnabled ? "Running" : "Stopped",
                   m_currentFPS,
                   m_hsvDetections.size(),
                   m_yoloDetections.size());
    }
    ImGui::End();
#endif
}

void MainInterface::RenderAboutDialog() {
#if GUI_ENABLED
    if (!m_showAboutDialog) return;
    
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("About 320x320 ROI Detection System", &m_showAboutDialog, 
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
        ImGui::Text("🎯 Professional 320x320 ROI Detection System");
        ImGui::PopStyleColor();
        
        ImGui::Separator();
        ImGui::Text("A modern, high-performance computer vision system optimized for");
        ImGui::Text("320x320 center region detection and analysis.");
        
        ImGui::Spacing();
        ImGui::Text("Features:");
        ImGui::BulletText("Real-time 320x320 center region capture");
        ImGui::BulletText("HSV color detection with coordinate tracking");
        ImGui::BulletText("YOLO v11 object detection with TensorRT optimization");
        ImGui::BulletText("Professional ImGui interface with docking support");
        ImGui::BulletText("Performance monitoring and metrics");
        
        ImGui::Spacing();
        ImGui::Text("System Version: 3.0 (Modernized)");
        ImGui::Text("Built with OpenCV, ImGui, and TensorRT");
    }
    ImGui::End();
#endif
}

// 나머지 필수 함수들 (간소화)
void MainInterface::Cleanup() {
#if GUI_ENABLED
    CleanupStreamRedirection();
    
    if (m_frameTexture != 0) {
        glDeleteTextures(1, &m_frameTexture);
        m_frameTexture = 0;
    }
    
    if (m_imguiContext) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(m_imguiContext);
        m_imguiContext = nullptr;
    }
    
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    
    glfwTerminate();
#endif
    StopCapture();
    m_initialized = false;
}

bool MainInterface::CreateGLTexture() {
#if GUI_ENABLED
    glGenTextures(1, &m_frameTexture);
    if (m_frameTexture == 0) return false;
    
    glBindTexture(GL_TEXTURE_2D, m_frameTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    return true;
#else
    return true;
#endif
}

void MainInterface::UpdateGLTexture(const cv::Mat& frame) {
#if GUI_ENABLED
    if (frame.empty() || m_frameTexture == 0) return;
    
    cv::Mat display_frame;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, display_frame, cv::COLOR_BGR2RGB);
    } else {
        display_frame = frame;
    }
    
    glBindTexture(GL_TEXTURE_2D, m_frameTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, display_frame.cols, display_frame.rows, 
                 0, GL_RGB, GL_UNSIGNED_BYTE, display_frame.data);
    
    m_textureWidth = display_frame.cols;
    m_textureHeight = display_frame.rows;
#endif
}

void MainInterface::HandleEvents() {
#if GUI_ENABLED
    if (m_window) {
        glfwPollEvents();
    }
#endif
}

bool MainInterface::StartCapture(int monitorIndex) {
    if (m_captureEnabled) return true;
    try {
        if (!m_captureDevice) {
            m_captureDevice = std::make_unique<ScreenCaptureLiteDevice>();
        }
        if (m_monitors.empty()) {
            RefreshMonitorList();
        }
        if (monitorIndex >= static_cast<int>(m_monitors.size())) {
            monitorIndex = 0;
        }
        ICaptureDevice::CaptureSettings settings;
        settings.target_fps = static_cast<int>(m_targetFPS);
        if (!m_captureDevice->Initialize(settings)) {
            AddLogMessage("Capture device init failed", 2);
            return false;
        }
        m_captureDevice->SetCenterRegionMode(true);
        if (!m_captureDevice->StartCapture(monitorIndex)) {
            AddLogMessage("Capture start failed", 2);
            return false;
        }
        m_captureEnabled = true;
        m_lastFrameTime = std::chrono::steady_clock::now();
        AddLogMessage("Capture started (monitor index: " + std::to_string(monitorIndex) + ")", 0);
        return true;
    } catch (const std::exception& e) {
        AddLogMessage(std::string("Capture start exception: ") + e.what(), 2);
        return false;
    }
}

void MainInterface::StopCapture() {
    if (!m_captureEnabled) return;
    try {
        if (m_captureDevice) {
            m_captureDevice->StopCapture();
        }
    } catch (...) {}
    m_captureEnabled = false;
    AddLogMessage("Capture stopped", 0);
}

void MainInterface::PollCaptureFrame() {
    if (!m_captureEnabled || !m_captureDevice) return;
    cv::Mat full, roi;
    if (m_captureDevice->CaptureFrame(full)) {
        if (m_captureDevice->GetCenterRegion(roi) && !roi.empty()) {
            if (m_centerCapture) {
                m_regionInfo = m_centerCapture->CalculateRegionInfo(full.cols, full.rows);
            }
            UpdateROIFrame(roi, m_regionInfo);
            auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - m_lastFrameTime).count();
            if (dt > 0) {
                double fps = 1.0 / dt;
                UpdateFPS(static_cast<float>(fps));
            }
            m_lastFrameTime = now;
        }
    }
}

void MainInterface::RefreshMonitorList() {
    m_monitors.clear();
    if (m_captureDevice) {
        m_monitors = m_captureDevice->GetAvailableMonitors();
        if (m_monitors.empty()) {
            AddLogMessage("No monitors detected", 1);
        } else {
            AddLogMessage("Monitors refreshed: " + std::to_string(m_monitors.size()), 0);
            if (m_selectedMonitor >= static_cast<int>(m_monitors.size())) {
                m_selectedMonitor = 0;
            }
        }
    }
}

bool MainInterface::ShouldClose() const {
#if GUI_ENABLED
    return m_window ? glfwWindowShouldClose(m_window) : false;
#else
    return false;
#endif
}

// 간소화된 구현들
void MainInterface::UpdateFrame(const cv::Mat& frame) {
    if (!frame.empty()) {
        m_currentFrame = frame.clone();
    }
}

void MainInterface::AddLogMessage(const std::string& message, int level) {
    // 로그 시스템 간소화 (콘솔 출력으로 대체)
    std::cout << "[LOG" << level << "] " << message << std::endl;
}

void MainInterface::SaveConfiguration() {
    m_configManager.setValue("/vision_algorithms/hsv_tracking/enabled", m_hsvEnabled);
    m_configManager.setValue("/vision_algorithms/yolo_v11/enabled", m_yolov11Enabled);
    m_configManager.setValue("/vision_algorithms/yolo_v11/confidence_threshold", m_yolov11Confidence);
    m_configManager.setValue("/vision_algorithms/yolo_v11/nms_threshold", m_yolov11NMS);
    
    if (m_configManager.saveConfig(m_configFilePath)) {
        std::cout << "Configuration saved to " << m_configFilePath << std::endl;
    }
}

void MainInterface::LoadConfiguration() {
    if (m_configManager.loadConfig(m_configFilePath)) {
        ApplyConfiguration();
    }
}

void MainInterface::ApplyConfiguration() {
    // 설정 적용 로직 간소화
    m_hsvEnabled = m_configManager.getValue("/vision_algorithms/hsv_tracking/enabled", true);
    m_yolov11Enabled = m_configManager.getValue("/vision_algorithms/yolo_v11/enabled", true);
    m_yolov11Confidence = m_configManager.getValue("/vision_algorithms/yolo_v11/confidence_threshold", 0.25f);
    m_yolov11NMS = m_configManager.getValue("/vision_algorithms/yolo_v11/nms_threshold", 0.45f);
}

void MainInterface::UpdateConfigurationFromGui() {
    // GUI에서 설정 업데이트시 자동 저장
}

void MainInterface::AutoSaveConfiguration() {
    SaveConfiguration();
}

void MainInterface::SetupStreamRedirection() {
    // 스트림 리다이렉션 간소화
}

void MainInterface::CleanupStreamRedirection() {
    // 스트림 리다이렉션 정리
}

// GuiStreamBuf 구현
int GuiStreamBuf::overflow(int c) {
    if (c != EOF) {
        m_buffer += static_cast<char>(c);
        if (c == '\n') {
            if (m_gui) {
                m_gui->AddLogMessage(m_buffer, m_logLevel);
            }
            m_buffer.clear();
        }
    }
    return c;
}