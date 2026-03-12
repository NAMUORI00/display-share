#include "gui/MainInterface.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>

// Capture interface (CaptureSettings 정의 포함)
#include "interfaces/ICaptureDevice.h"

// ImGui core (always available)
#include <imgui.h>
#include <imgui_internal.h>

#if GUI_ENABLED
    // Windows 헤더를 먼저 포함하여 GLFW에서 재정의 경고(APIENTRY 등) 최소화
    #ifdef _WIN32
        #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
        #endif
        #ifndef NOMINMAX
        #define NOMINMAX
        #endif
        #include <windows.h>
    #endif

// ImGui with full backend support
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

// OpenGL types
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
    , m_yolo26Enabled(true)
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
    std::strncpy(m_yolo26ModelPath, "models/yolo26n.onnx", sizeof(m_yolo26ModelPath) - 1);
    m_yolo26ModelPath[sizeof(m_yolo26ModelPath) - 1] = '\0';

    std::strncpy(m_yolo26ClassNamesPath, "models/coco_classes.txt", sizeof(m_yolo26ClassNamesPath) - 1);
    m_yolo26ClassNamesPath[sizeof(m_yolo26ClassNamesPath) - 1] = '\0';
    
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
        RefreshYOLO26Providers();
        ReloadYOLO26Detector();
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

    // 폰트 & 테마 초기화
    InitFonts();
    InitTheme();

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
    // 모니터 목록 초기화 후 자동 캡처 시작
    RefreshMonitorList();
    if (!m_monitors.empty()) {
        int monitorIndex = (m_selectedMonitor < static_cast<int>(m_monitors.size())) 
            ? m_monitors[m_selectedMonitor].index : m_monitors[0].index;
        StartCapture(monitorIndex);  // 선택된 모니터에서 실시간 캡처 시작
        AddLogMessage("Auto-started capture on monitor: " + std::to_string(monitorIndex), 0);
    }
    
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
    
    // 첫 실행 시 레이아웃 초기화 (간소화된 2-패널 레이아웃)
    static bool first_time = true;
    if (first_time || m_resetLayout) {
        first_time = false;
        m_resetLayout = false;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        // 간소화된 2-패널 레이아웃: 중앙(ROI) + 우측(Control)
        auto dock_id_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.30f, nullptr, &dockspace_id);

        ImGui::DockBuilderDockWindow("ROI View", dockspace_id);          // 중앙: ROI 시각화
        ImGui::DockBuilderDockWindow("Control Panel", dock_id_right);    // 우측: 제어 패널

        ImGui::DockBuilderFinish(dockspace_id);
    }

    // 메뉴바 렌더링
    RenderMenuBar();

    // 필수 패널만 렌더링
    RenderROIVisualizationPanel();    // 320x320 ROI 시각화 + 검출 요약
    RenderControlPanel();             // 제어 패널
    if (m_showLogConsole) {
        RenderLogConsolePanel();      // 로그 콘솔 (메뉴 토글)
    }

    // 프레임 렌더링 후 캡처 프레임 폴링
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
    if (ImGui::Begin("ROI View")) {

        // ROI 프레임 표시
        if (!m_roiFrame.empty() && m_frameTexture != 0) {
            const ImVec2 content_region = ImGui::GetContentRegionAvail();
            float display_size = (std::min)(content_region.x - 10.0f, content_region.y - 80.0f);
            if (display_size < 64.0f) display_size = 64.0f;

            // 중앙 정렬
            const float center_x = (content_region.x - display_size) * 0.5f;
            if (center_x > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + center_x);

            // 320x320 ROI 이미지
            ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(m_frameTexture)),
                        ImVec2(display_size, display_size));

            // 검출 오버레이
            if (m_showROIOverlay) {
                RenderDetectionOverlay(m_roiFrame);
            }

            // --- 간결한 검출 요약 (Detection Results 패널 대체) ---
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f),
                "HSV: %zu pts | YOLO: %zu objs",
                m_hsvDetections.size(), m_yoloDetections.size());

            if (!m_yoloConfidences.empty()) {
                float avg_conf = 0.0f;
                for (float c : m_yoloConfidences) avg_conf += c;
                avg_conf /= m_yoloConfidences.size();
                ImGui::SameLine();
                ImGui::Text("(avg %.0f%%)", avg_conf * 100.0f);
            }

            // ROI 정보 (한 줄 축약)
            ImGui::TextDisabled("ROI (%d,%d) %dx%d from %dx%d",
                m_regionInfo.x, m_regionInfo.y,
                m_regionInfo.width, m_regionInfo.height,
                m_regionInfo.source_width, m_regionInfo.source_height);

        } else {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 40.0f);
            ImGui::TextDisabled("Waiting for ROI capture...");
        }

        // 오버레이 토글
        ImGui::Checkbox("Overlay", &m_showROIOverlay);
    }
    ImGui::End();
#endif
}

void MainInterface::RenderDetectionResultsPanel() {
#if GUI_ENABLED
    // 검출 결과는 ROI Visualization 패널에 통합됨 — 이 함수는 더 이상 호출되지 않음
#endif
}

void MainInterface::RenderControlPanel() {
#if GUI_ENABLED
    if (ImGui::Begin("Control Panel")) {

        // ── 캡처 제어 (항상 표시) ──
        if (ImGui::Button("Refresh")) {
            RefreshMonitorList();
        }
        ImGui::SameLine();
        if (!m_monitors.empty()) {
            ImGui::SetNextItemWidth(-1);
            std::string currentLabel;
            if (m_selectedMonitor < static_cast<int>(m_monitors.size())) {
                const auto& mon = m_monitors[m_selectedMonitor];
                currentLabel = std::to_string(mon.index) + ": " + mon.name;
            } else {
                currentLabel = "Select Monitor";
            }
            if (ImGui::BeginCombo("##monitor", currentLabel.c_str())) {
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
            ImGui::TextDisabled("No monitors");
        }

        const bool allowCaptureToggle = m_captureEnabled || !m_monitors.empty();
        if (!allowCaptureToggle) ImGui::BeginDisabled(true);
        if (ImGui::Button(m_captureEnabled ? "Stop" : "Start", ImVec2(-1, 32))) {
            if (!m_captureEnabled) {
                int monIndex = (m_selectedMonitor < static_cast<int>(m_monitors.size())) ? m_monitors[m_selectedMonitor].index : 0;
                if (!StartCapture(monIndex)) {
                    AddLogMessage("Failed to start capture", 2);
                }
            } else {
                StopCapture();
            }
        }
        if (!allowCaptureToggle) ImGui::EndDisabled();

        ImGui::Separator();

        // ── 검출 토글 ──
        ImGui::Checkbox("HSV Detection", &m_hsvEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("YOLO26", &m_yolo26Enabled);

        // ── HSV 설정 (기본 접힘) ──
        if (ImGui::CollapsingHeader("HSV Settings")) {
            ImGui::SliderInt("H Min", &m_hsvLower[0], 0, 179);
            ImGui::SliderInt("S Min", &m_hsvLower[1], 0, 255);
            ImGui::SliderInt("V Min", &m_hsvLower[2], 0, 255);
            ImGui::SliderInt("H Max", &m_hsvUpper[0], 0, 179);
            ImGui::SliderInt("S Max", &m_hsvUpper[1], 0, 255);
            ImGui::SliderInt("V Max", &m_hsvUpper[2], 0, 255);
        }

        // ── YOLO26 설정 (기본 접힘) ──
        if (ImGui::CollapsingHeader("YOLO26 Settings")) {
            ImGui::SliderFloat("Confidence", &m_yolo26Confidence, 0.0f, 1.0f, "%.2f");
            ImGui::InputInt("GPU ID", &m_yolo26SelectedGpuId);
            if (m_yolo26SelectedGpuId < 0) m_yolo26SelectedGpuId = 0;

            std::string current_gpu_label = "Custom GPU ID";
            for (const auto& device : m_yolo26Providers) {
                if (device.device_id == m_yolo26SelectedGpuId) {
                    current_gpu_label = std::to_string(device.device_id) + ": " + device.name;
                    break;
                }
            }
            if (m_yolo26Providers.empty()) current_gpu_label = "No GPUs";

            if (ImGui::BeginCombo("GPU", current_gpu_label.c_str())) {
                for (const auto& device : m_yolo26Providers) {
                    const bool selected = device.device_id == m_yolo26SelectedGpuId;
                    const std::string label = std::to_string(device.device_id) + ": " + device.name;
                    if (ImGui::Selectable(label.c_str(), selected)) {
                        m_yolo26SelectedGpuId = device.device_id;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::InputText("Model", m_yolo26ModelPath, sizeof(m_yolo26ModelPath));
            ImGui::InputText("Classes", m_yolo26ClassNamesPath, sizeof(m_yolo26ClassNamesPath));

            if (ImGui::Button("Reload Session", ImVec2(-1, 0))) {
                ReloadYOLO26Detector();
            }
        }

        // ── 저장 ──
        ImGui::Separator();
        if (ImGui::Button("Save Settings", ImVec2(-1, 0))) {
            SaveConfiguration();
        }
    }
    ImGui::End();
#endif
}

void MainInterface::RenderPerformanceDashboard() {
#if GUI_ENABLED
    // 성능 정보는 상태바에 통합됨 — 이 함수는 더 이상 호출되지 않음
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
    if (m_yolo26Enabled && !m_yoloDetections.empty()) {
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
            if (ImGui::MenuItem("Start", "Ctrl+S", nullptr, !m_captureEnabled)) {
                const int monitor_index =
                    (m_selectedMonitor < static_cast<int>(m_monitors.size())) ? m_monitors[m_selectedMonitor].index : 0;
                StartCapture(monitor_index);
            }
            if (ImGui::MenuItem("Stop", "Ctrl+Q", nullptr, m_captureEnabled)) {
                StopCapture();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Config")) {
                SaveConfiguration();
            }
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(m_window, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("ROI Overlay", nullptr, &m_showROIOverlay);
            ImGui::MenuItem("Log Console", nullptr, &m_showLogConsole);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                m_resetLayout = true;
            }
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

        double proc_ms = 0.0;
        if (m_metrics) proc_ms = m_metrics->getCurrentProcessTime();

        ImGui::Text("%s | FPS: %.1f | Proc: %.1fms | HSV: %zu | YOLO: %zu",
                   m_captureEnabled ? "Running" : "Stopped",
                   m_currentFPS, proc_ms,
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
        ImGui::BulletText("YOLO26 object detection with ONNX Runtime");
        ImGui::BulletText("Professional ImGui interface with docking support");
        ImGui::BulletText("Performance monitoring and metrics");
        
        ImGui::Spacing();
        ImGui::Text("System Version: 3.0 (Modernized)");
        ImGui::Text("Built with OpenCV, ImGui, and ONNX Runtime");
    }
    ImGui::End();
#endif
}

// ================== 신규: 로그 콘솔 패널 ==================
void MainInterface::RenderLogConsolePanel() {
#if GUI_ENABLED
    if (ImGui::Begin("Log Console")) {
        // 필터 영역
        ImGui::Checkbox("Info", &m_logFilterInfo); ImGui::SameLine();
        ImGui::Checkbox("Warn", &m_logFilterWarn); ImGui::SameLine();
        ImGui::Checkbox("Error", &m_logFilterError);
        ImGui::SetNextItemWidth(180);
        ImGui::InputTextWithHint("##logsearch","Search...", m_logSearch, sizeof(m_logSearch));
        ImGui::SameLine();
        ImGui::Checkbox("AutoScroll", &m_autoScrollConsole);
        ImGui::Separator();

        ImGui::BeginChild("LogScroll", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& e : m_logMessages) {
            if ((e.level==0 && !m_logFilterInfo) || (e.level==1 && !m_logFilterWarn) || (e.level==2 && !m_logFilterError)) continue;
            if (m_logSearch[0] != '\0') {
                if (e.message.find(m_logSearch) == std::string::npos) continue;
            }
            ImVec4 col = (e.level==0)?ImVec4(0.85f,0.85f,0.85f,1.f):(e.level==1?ImVec4(1.f,0.85f,0.3f,1.f):ImVec4(1.f,0.4f,0.3f,1.f));
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(e.message.c_str());
            ImGui::PopStyleColor();
        }
        if (m_autoScrollConsole && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }
    ImGui::End();
#endif
}

// 도움말 오버레이 — 간소화에 따라 기본 숨김, 메뉴의 About으로 대체됨
void MainInterface::RenderHelpOverlay() {
#if GUI_ENABLED
    // 간소화된 UI에서는 더 이상 표시하지 않음
#endif
}

// ================== 신규: 폰트/테마 & 유틸 ==================
void MainInterface::InitFonts() {
#if GUI_ENABLED
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig cfg; cfg.OversampleH = 2; cfg.OversampleV = 2; cfg.PixelSnapH = true;
    // 기본 한글 폰트 (없는 경우 fallback - 예외 처리 생략)
    io.Fonts->AddFontDefault();
    io.FontDefault = io.Fonts->Fonts.back();
#endif
}

void MainInterface::InitTheme() {
#if GUI_ENABLED
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.f; style.FrameRounding = 6.f; style.GrabRounding = 5.f; style.TabRounding = 5.f;
    style.ItemSpacing = ImVec2(8,6); style.FramePadding = ImVec2(10,6);
    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f,0.11f,0.13f,0.98f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f,0.36f,0.55f,1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f,0.45f,0.70f,0.85f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f,0.55f,0.85f,0.90f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.18f,0.42f,0.65f,1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.19f,0.42f,0.66f,0.80f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f,0.50f,0.80f,0.90f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.18f,0.42f,0.65f,1.00f);
#endif
}

void MainInterface::DrawMetricCard(const char* id, const char* label, const std::string& value, unsigned int bg, unsigned int border) {
#if GUI_ENABLED
    ImGui::PushID(id);
    ImVec2 size(140,72);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 p2(p.x + size.x, p.y + size.y);
    dl->AddRectFilled(p, p2, bg, 8.f);
    dl->AddRect(p, p2, border, 8.f, 0, 2.f);
    ImGui::InvisibleButton("metric_btn", size);
    ImGui::SetCursorScreenPos(ImVec2(p.x + 10, p.y + 8));
    ImGui::TextUnformatted(label);
    ImGui::SetCursorScreenPos(ImVec2(p.x + 10, p.y + 36));
    ImGui::TextColored(ImVec4(0.95f,0.97f,1.f,1.f), "%s", value.c_str());
    ImGui::SameLine();
    ImGui::PopID();
    ImGui::SameLine();
#endif
}

void MainInterface::DrawHSVRangePreview() {
#if GUI_ENABLED
    ImVec2 start = ImGui::GetCursorScreenPos();
    ImVec2 size(ImGui::GetContentRegionAvail().x, 18);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int hMin=m_hsvLower[0], hMax=m_hsvUpper[0];
    const int total = 179;
    for(int h=hMin; h<=hMax; ++h){
        float t0 = (float)(h - hMin)/(float)std::max(1,(hMax-hMin));
        ImU32 col = ImColor::HSV(h/179.f,1.f,1.f);
        float x0 = start.x + t0 * size.x;
        float x1 = start.x + ((float)(h - hMin + 1)/(float)std::max(1,(hMax-hMin))) * size.x;
        dl->AddRectFilled(ImVec2(x0,start.y), ImVec2(x1,start.y+size.y), col);
    }
    dl->AddRect(start, ImVec2(start.x + size.x, start.y + size.y), IM_COL32(255,255,255,180), 3.f, 0, 1.5f);
    ImGui::Dummy(size);
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
            
            // ROI 프레임 업데이트 (실시간 GUI 표시)
            UpdateROIFrame(roi, m_regionInfo);
            
            // HSV 색상 검출 실행 (옵션)
            if (m_hsvEnabled && !roi.empty()) {
                std::vector<cv::Point> hsv_points;
                PerformHSVDetection(roi, hsv_points);
                UpdateHSVDetections(hsv_points);
            }
            
            // YOLO 객체 검출 실행 (옵션)  
            if (m_yolo26Enabled && !roi.empty()) {
                std::vector<cv::Rect> yolo_boxes;
                std::vector<float> yolo_confs;
                std::vector<std::string> yolo_names;
                PerformYOLODetection(roi, yolo_boxes, yolo_confs, yolo_names);
                UpdateYOLODetections(yolo_boxes, yolo_confs, yolo_names);
            }
            
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
    if (!m_captureDevice) {
        m_captureDevice = std::make_unique<ScreenCaptureLiteDevice>();
    }
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
    std::string normalized_message = message;
    while (!normalized_message.empty() &&
           (normalized_message.back() == '\n' || normalized_message.back() == '\r')) {
        normalized_message.pop_back();
    }

    if (normalized_message.empty()) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#if defined(_WIN32)
    localtime_s(&local_time, &now_time);
#else
    localtime_r(&now_time, &local_time);
#endif

    std::ostringstream timestamp;
    timestamp << std::put_time(&local_time, "%H:%M:%S");
    m_logMessages.push_back({normalized_message, level, timestamp.str()});
    if (m_logMessages.size() > MAX_LOG_ENTRIES) {
        m_logMessages.erase(m_logMessages.begin());
    }

    std::cout << "[LOG" << level << "] " << normalized_message << std::endl;
}

void MainInterface::SaveConfiguration() {
    if (!m_configManager.isLoaded()) {
        m_configManager.resetToDefaults();
    }

    m_configManager.setValue("/vision_algorithms/hsv_tracking/enabled", m_hsvEnabled);
    m_configManager.setValue("/vision_algorithms/yolo26_detection/enabled", m_yolo26Enabled);
    m_configManager.setValue("/vision_algorithms/yolo26_detection/onnx_model_path", std::string(m_yolo26ModelPath));
    m_configManager.setValue("/vision_algorithms/yolo26_detection/class_names_path", std::string(m_yolo26ClassNamesPath));
    m_configManager.setValue("/vision_algorithms/yolo26_detection/confidence_threshold", m_yolo26Confidence);
    m_configManager.setValue("/vision_algorithms/yolo26_detection/max_detections", m_yolo26MaxDetections);
    m_configManager.setValue("/vision_algorithms/yolo26_detection/input_size/0", m_yolo26InputWidth);
    m_configManager.setValue("/vision_algorithms/yolo26_detection/input_size/1", m_yolo26InputHeight);
    m_configManager.setValue("/vision_algorithms/yolo26_detection/selected_gpu_id", m_yolo26SelectedGpuId);
    m_configManager.setValue("/vision_algorithms/yolo26_detection/execution_providers", m_yolo26ExecutionProviders);
    
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
    m_hsvEnabled = m_configManager.getValue("/vision_algorithms/hsv_tracking/enabled", true);
    m_yolo26Enabled = m_configManager.getValue("/vision_algorithms/yolo26_detection/enabled", true);
    m_yolo26Confidence = m_configManager.getValue("/vision_algorithms/yolo26_detection/confidence_threshold", 0.25f);
    m_yolo26MaxDetections = m_configManager.getValue("/vision_algorithms/yolo26_detection/max_detections", 100);
    m_yolo26InputWidth = m_configManager.getValue("/vision_algorithms/yolo26_detection/input_size/0", 640);
    m_yolo26InputHeight = m_configManager.getValue("/vision_algorithms/yolo26_detection/input_size/1", 640);
    m_yolo26SelectedGpuId = m_configManager.getValue("/vision_algorithms/yolo26_detection/selected_gpu_id", 0);
    m_yolo26ExecutionProviders = m_configManager.getValue(
        "/vision_algorithms/yolo26_detection/execution_providers",
        std::vector<std::string>{"cuda", "cpu"});

    const std::string model_path = m_configManager.getValue("/vision_algorithms/yolo26_detection/onnx_model_path",
                                                            std::string("models/yolo26n.onnx"));
    const std::string class_names_path = m_configManager.getValue("/vision_algorithms/yolo26_detection/class_names_path",
                                                                  std::string("models/coco_classes.txt"));

    std::strncpy(m_yolo26ModelPath, model_path.c_str(), sizeof(m_yolo26ModelPath) - 1);
    m_yolo26ModelPath[sizeof(m_yolo26ModelPath) - 1] = '\0';
    std::strncpy(m_yolo26ClassNamesPath, class_names_path.c_str(), sizeof(m_yolo26ClassNamesPath) - 1);
    m_yolo26ClassNamesPath[sizeof(m_yolo26ClassNamesPath) - 1] = '\0';

    ReloadYOLO26Detector();
}

void MainInterface::UpdateConfigurationFromGui() {
    // GUI에서 설정 업데이트시 자동 저장
}

void MainInterface::AutoSaveConfiguration() {
    SaveConfiguration();
}

bool MainInterface::InitializeYOLO26Detector() {
    m_yolo26LastError.clear();
    m_yolo26ProviderStatus = "disabled";
    m_yolo26CpuFallback = false;

    if (!m_yolo26Enabled) {
        m_yolo26Detector.reset();
        return true;
    }

    auto detector = std::make_unique<YOLO26OnnxRuntimeInference>();
    YOLO26OnnxRuntimeInference::Settings settings;
    settings.onnx_model_path = m_yolo26ModelPath;
    settings.class_names_path = m_yolo26ClassNamesPath;
    settings.confidence_threshold = m_yolo26Confidence;
    settings.max_detections = m_yolo26MaxDetections;
    settings.input_width = m_yolo26InputWidth;
    settings.input_height = m_yolo26InputHeight;
    settings.selected_gpu_id = m_yolo26SelectedGpuId;
    settings.execution_providers = m_yolo26ExecutionProviders;

    if (!detector->Initialize(settings)) {
        m_yolo26LastError = detector->GetLastError();
        m_yolo26ProviderStatus = "initialization failed";
        AddLogMessage("YOLO26 init failed: " + m_yolo26LastError, 2);
        m_yolo26Detector = std::move(detector);
        return false;
    }

    m_yolo26Providers = detector->GetAvailableProviders();
    m_yolo26ProviderStatus = detector->GetActiveProviderName();
    m_yolo26CpuFallback = detector->IsUsingCpuFallback();
    m_yolo26LastError = detector->GetLastError();
    AddLogMessage("YOLO26 session ready with provider: " + m_yolo26ProviderStatus, 0);
    m_yolo26Detector = std::move(detector);
    return true;
}

bool MainInterface::ReloadYOLO26Detector() {
    RefreshYOLO26Providers();
    return InitializeYOLO26Detector();
}

void MainInterface::RefreshYOLO26Providers() {
    if (!m_yolo26Detector) {
        m_yolo26Detector = std::make_unique<YOLO26OnnxRuntimeInference>();
    }
    m_yolo26Providers = m_yolo26Detector->GetAvailableProviders();
}

void MainInterface::SetupStreamRedirection() {
    // 스트림 리다이렉션 간소화
}

void MainInterface::CleanupStreamRedirection() {
    // 스트림 리다이렉션 정리
}

// 검출 헬퍼 함수 구현
void MainInterface::PerformHSVDetection(const cv::Mat& roi, std::vector<cv::Point>& detections) {
    if (roi.empty()) return;
    
    detections.clear();
    
    try {
        // HSV 변환
        cv::Mat hsv;
        cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);
        
        // HSV 범위로 마스크 생성
        cv::Scalar lower(m_hsvLower[0], m_hsvLower[1], m_hsvLower[2]);
        cv::Scalar upper(m_hsvUpper[0], m_hsvUpper[1], m_hsvUpper[2]);
        cv::Mat mask;
        cv::inRange(hsv, lower, upper, mask);
        
        // 노이즈 제거를 위한 모폴로지 연산
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
        
        // 컨투어 찾기
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        // 각 컨투어의 중심점 계산
        for (const auto& contour : contours) {
            double area = cv::contourArea(contour);
            if (area > 100 && area < 10000) { // 적절한 크기의 객체만
                cv::Moments moments = cv::moments(contour);
                if (moments.m00 != 0) {
                    cv::Point center(
                        static_cast<int>(moments.m10 / moments.m00),
                        static_cast<int>(moments.m01 / moments.m00)
                    );
                    detections.push_back(center);
                }
            }
        }
    } catch (const cv::Exception& e) {
        AddLogMessage("HSV Detection error: " + std::string(e.what()), 2);
    }
}

void MainInterface::PerformYOLODetection(const cv::Mat& roi, std::vector<cv::Rect>& boxes, 
                                          std::vector<float>& confidences, std::vector<std::string>& class_names) {
    boxes.clear();
    confidences.clear();
    class_names.clear();

    if (roi.empty() || !m_yolo26Enabled) {
        return;
    }

    if (!m_yolo26Detector || !m_yolo26Detector->IsInitialized()) {
        if (!ReloadYOLO26Detector()) {
            return;
        }
    }

    const auto detections = m_yolo26Detector->DetectMultiple(roi);
    m_yolo26ProviderStatus = m_yolo26Detector->GetActiveProviderName();
    m_yolo26CpuFallback = m_yolo26Detector->IsUsingCpuFallback();
    m_yolo26LastError = m_yolo26Detector->GetLastError();

    for (const auto& detection : detections) {
        boxes.push_back(detection.bounding_box);
        confidences.push_back(static_cast<float>(detection.confidence));
        class_names.push_back(detection.label);
    }
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
