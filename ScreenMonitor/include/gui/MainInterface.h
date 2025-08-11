#pragma once

#include <memory>
#include <vector>
#include <string>
#include <streambuf>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include "core/ConfigManager.h"
#include "capture/CenterRegionCapture.h"
#include "monitoring/SimpleMetrics.h"
#include "capture/ScreenCaptureLiteDevice.h"
 
// Conditional GUI compilation
#ifndef DISABLE_GUI
    // Windows 헤더를 먼저 포함하여 GLFW에서 재정의 경고(APIENTRY 등) 최소화
    #if defined(_WIN32)
        #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
        #endif
        #ifndef NOMINMAX
        #define NOMINMAX
        #endif
        #include <windows.h>
    #endif
    // GUI fully enabled with GLFW + OpenGL3
    #include <GLFW/glfw3.h>

    // OpenGL types
    #if defined(_WIN32)
        #include <GL/gl.h>
    #elif defined(__APPLE__)
        #include <OpenGL/gl.h>
    #else
        #include <GL/gl.h>
    #endif

    #define GUI_ENABLED 1
#else
    #define GUI_ENABLED 0
    // Dummy types for non-GUI build
    typedef void* GLFWwindow;
    typedef unsigned int GLuint;
#endif

// Forward declarations
struct ImGuiContext;
class HighSpeedCapture;

/**
 * @brief GUI 스트림 버퍼 - cout/cerr을 ImGui 콘솔로 리다이렉션
 */
class GuiStreamBuf : public std::streambuf {
public:
    GuiStreamBuf(class MainInterface* gui, int logLevel) : m_gui(gui), m_logLevel(logLevel) {}
    
protected:
    int overflow(int c) override;
    
private:
    class MainInterface* m_gui;
    int m_logLevel; // 0=Info, 1=Warning, 2=Error
    std::string m_buffer;
};

/**
 * @brief 메인 GUI 인터페이스
 * 
 * ImGui 기반 사용자 인터페이스 관리
 */
class MainInterface {
public:
    /**
     * @brief MainInterface 생성자
     */
    MainInterface();
    
    /**
     * @brief 소멸자
     */
    ~MainInterface();

    /**
     * @brief GUI 초기화
     * @return 초기화 성공 여부
     */
    bool Initialize();
    
    /**
     * @brief GUI 정리
     */
    void Cleanup();
    
    /**
     * @brief GUI 렌더링
     */
    void Render();
    
    /**
     * @brief 이벤트 처리
     */
    void HandleEvents();

    // 캡처 제어 (GUI 버튼/메뉴에서 사용)
    bool StartCapture(int monitorIndex = 0);
    void StopCapture();
    void PollCaptureFrame();
    void RefreshMonitorList();

    /**
     * @brief 화면 업데이트
     * @param frame 캡처된 프레임
     */
    void UpdateFrame(const cv::Mat& frame);

    /**
     * @brief 320x320 ROI 프레임 업데이트
     * @param roi_frame 320x320 중심 영역 프레임
     * @param region_info 영역 정보
     */
    void UpdateROIFrame(const cv::Mat& roi_frame, const CenterRegionCapture::RegionInfo& region_info);

    /**
     * @brief HSV 검출 결과 업데이트
     * @param hsv_detections HSV 검출된 좌표들 (320x320 영역 기준)
     */
    void UpdateHSVDetections(const std::vector<cv::Point>& hsv_detections);

    /**
     * @brief YOLO 검출 결과 업데이트
     * @param yolo_detections YOLO 검출된 객체들 (320x320 영역 기준)
     * @param confidence_scores 각 검출의 신뢰도 점수
     * @param class_names 각 검출의 클래스 이름
     */
    void UpdateYOLODetections(const std::vector<cv::Rect>& yolo_detections, 
                             const std::vector<float>& confidence_scores,
                             const std::vector<std::string>& class_names);

    /**
     * @brief 로그 메시지 추가
     * @param message 로그 메시지
     * @param level 로그 레벨 (0=Info, 1=Warning, 2=Error)
     */
    void AddLogMessage(const std::string& message, int level = 0);


    /**
     * @brief FPS 업데이트
     * @param fps 현재 FPS
     */
    void UpdateFPS(float fps);

    // 현대화된 GUI 상태 접근자 (320x320 ROI 최적화)
    bool IsCaptureEnabled() const { return m_captureEnabled; }
    bool IsHSVDetectionEnabled() const { return m_hsvEnabled; }
    bool IsYOLODetectionEnabled() const { return m_yoloEnabled; }
    bool IsYOLOv11DetectionEnabled() const { return m_yolov11Enabled; }
    bool ShouldShowROIOverlay() const { return m_showROIOverlay; }
    bool ShouldShowDetectionStats() const { return m_showDetectionStats; }
    
    // HSV 설정 접근자
    cv::Scalar GetHSVLowerBound() const { return cv::Scalar(m_hsvLower[0], m_hsvLower[1], m_hsvLower[2]); }
    cv::Scalar GetHSVUpperBound() const { return cv::Scalar(m_hsvUpper[0], m_hsvUpper[1], m_hsvUpper[2]); }
    
    // YOLO v11 설정 접근자
    std::string GetYOLOv11ModelPath() const { return std::string(m_yolov11ModelPath); }
    std::string GetYOLOv11ClassNamesPath() const { return std::string(m_yolov11ClassNamesPath); }
    float GetYOLOv11Confidence() const { return m_yolov11Confidence; }
    float GetYOLOv11NMSThreshold() const { return m_yolov11NMS; }
    
    // 성능 및 ROI 정보 접근자
    float GetTargetFPS() const { return m_targetFPS; }
    const CenterRegionCapture::RegionInfo& GetRegionInfo() const { return m_regionInfo; }

    // 윈도우 상태
    bool ShouldClose() const;
    
    // 스트림 리다이렉션 관리
    void SetupStreamRedirection();
    void CleanupStreamRedirection();

private:
    // OpenGL 헬퍼 함수
    bool CreateGLTexture();
    void UpdateGLTexture(const cv::Mat& frame);
    void RenderFrame();
    
    // 현대화된 GUI 패널 함수
    void RenderMenuBar();
    void RenderROIVisualizationPanel();      // 320x320 ROI 시각화 패널
    void RenderDetectionResultsPanel();      // 검출 결과 표시 패널
    void RenderPerformanceDashboard();       // 성능 모니터링 대시보드
    void RenderControlPanel();               // 간소화된 제어 패널
    void RenderStatusBar();
    void RenderAboutDialog();
    
    // 현대화된 GUI 스타일링
    void ApplyModernTheme();
    void RenderDetectionOverlay(const cv::Mat& frame);

    // 설정 관리 헬퍼
    void SaveConfiguration();
    void LoadConfiguration();
    void ApplyConfiguration();
    void UpdateConfigurationFromGui();
    void AutoSaveConfiguration();
    
    // 검출 헬퍼 함수들
    void PerformHSVDetection(const cv::Mat& roi, std::vector<cv::Point>& detections);
    void PerformYOLODetection(const cv::Mat& roi, std::vector<cv::Rect>& boxes, 
                              std::vector<float>& confidences, std::vector<std::string>& class_names);

private:
    bool m_initialized = false;
    
#if GUI_ENABLED
    bool m_resetLayout = false;
    GLFWwindow* m_window = nullptr;
    ImGuiContext* m_imguiContext = nullptr;
    
    // OpenGL 리소스
    GLuint m_frameTexture = 0;
    int m_textureWidth = 0;
    int m_textureHeight = 0;
#endif
    
    // 320x320 ROI 시스템
    cv::Mat m_currentFrame;              // 전체 화면 프레임
    cv::Mat m_roiFrame;                  // 320x320 중심 영역 프레임
    CenterRegionCapture::RegionInfo m_regionInfo;  // 영역 정보
    std::unique_ptr<CenterRegionCapture> m_centerCapture;
    
    // 검출 결과 데이터
    std::vector<cv::Point> m_hsvDetections;          // HSV 검출 좌표들
    std::vector<cv::Rect> m_yoloDetections;          // YOLO 바운딩 박스들
    std::vector<float> m_yoloConfidences;            // YOLO 신뢰도 점수들
    std::vector<std::string> m_yoloClassNames;       // YOLO 클래스 이름들
    
    // 성능 모니터링
    std::unique_ptr<ScreenMonitor::SimpleMetrics> m_metrics;
    
    // 현대화된 GUI 상태
    bool m_captureEnabled = false;
    bool m_hsvEnabled = true;
    bool m_yoloEnabled = true;       // YOLO 기본 활성화
    bool m_showAboutDialog = false;
    bool m_showROIOverlay = true;    // ROI 오버레이 표시
    bool m_showDetectionStats = true; // 검출 통계 표시
    
    // HSV 설정
    int m_hsvLower[3] = {140, 120, 180};
    int m_hsvUpper[3] = {160, 200, 255};
    
    // 현대화된 YOLO v11 설정 (320x320 최적화)
    char m_yolov11ModelPath[256] = "models/yolo11n.onnx";
    char m_yolov11ClassNamesPath[256] = "models/coco.names";
    float m_yolov11Confidence = 0.25f;
    float m_yolov11NMS = 0.45f;
    bool m_yolov11Enabled = true;
    
    // 성능 데이터
    float m_currentFPS = 0.0f;
    float m_targetFPS = 60.0f;
    std::vector<float> m_fpsHistory;
    static constexpr size_t FPS_HISTORY_SIZE = 120;
    
    // 로그 시스템
    struct LogEntry {
        std::string message;
        int level; // 0=Info, 1=Warning, 2=Error
        std::string timestamp;
    };
    std::vector<LogEntry> m_logMessages;
    bool m_autoScrollConsole = true;
    static constexpr size_t MAX_LOG_ENTRIES = 1000;
    
    // 스트림 리다이렉션
    std::unique_ptr<GuiStreamBuf> m_coutRedirect;
    std::unique_ptr<GuiStreamBuf> m_cerrRedirect;
    std::streambuf* m_originalCout;
    std::streambuf* m_originalCerr;

    // 설정 관리
    ConfigManager m_configManager;
    std::string m_configFilePath = "config/config.json";

    // 캡처 디바이스
    std::unique_ptr<ScreenCaptureLiteDevice> m_captureDevice;
    std::chrono::steady_clock::time_point m_lastFrameTime{std::chrono::steady_clock::now()};
    // 모니터 선택/관리
    std::vector<ICaptureDevice::MonitorInfo> m_monitors;
    int m_selectedMonitor = 0;
};