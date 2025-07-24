#pragma once

#include <memory>
#include <vector>
#include <string>
#include <streambuf>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

// Conditional GUI compilation
#ifndef DISABLE_GUI
    // GUI fully enabled with GLFW + OpenGL3
    #include <GLFW/glfw3.h>
    
    // OpenGL types
    #if defined(_WIN32)
        #include <windows.h>
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

    /**
     * @brief 화면 업데이트
     * @param frame 캡처된 프레임
     */
    void UpdateFrame(const cv::Mat& frame);

    /**
     * @brief 검출 결과 업데이트
     * @param detections 검출된 객체들
     */
    void UpdateDetections(const std::vector<cv::Rect>& detections);

    /**
     * @brief 로그 메시지 추가
     * @param message 로그 메시지
     * @param level 로그 레벨 (0=Info, 1=Warning, 2=Error)
     */
    void AddLogMessage(const std::string& message, int level = 0);

    /**
     * @brief 모니터 목록 설정
     * @param monitors 사용 가능한 모니터 목록
     */
    void SetMonitorList(const std::vector<std::string>& monitors);

    /**
     * @brief FPS 업데이트
     * @param fps 현재 FPS
     */
    void UpdateFPS(float fps);

    // GUI 상태 접근자
    bool IsCaptureEnabled() const { return m_captureEnabled; }
    bool IsHSVDetectionEnabled() const { return m_hsvEnabled; }
    bool IsYOLODetectionEnabled() const { return m_yoloEnabled; }
    bool IsYOLOv11DetectionEnabled() const { return m_yolov11Enabled; }
    int GetSelectedMonitorIndex() const { return m_selectedMonitor; }
    cv::Scalar GetHSVLowerBound() const { return cv::Scalar(m_hsvLower[0], m_hsvLower[1], m_hsvLower[2]); }
    cv::Scalar GetHSVUpperBound() const { return cv::Scalar(m_hsvUpper[0], m_hsvUpper[1], m_hsvUpper[2]); }
    float GetTargetFPS() const { return m_targetFPS; }
    
    // YOLO v11 설정 접근자
    std::string GetYOLOv11ModelPath() const { return std::string(m_yolov11ModelPath); }
    std::string GetYOLOv11ClassNamesPath() const { return std::string(m_yolov11ClassNamesPath); }
    float GetYOLOv11Confidence() const { return m_yolov11Confidence; }
    float GetYOLOv11NMSThreshold() const { return m_yolov11NMS; }
    int GetYOLOv11Backend() const { return m_yolov11Backend; }

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
    
    // GUI 패널 함수
    void RenderMenuBar();
    void RenderCapturePanel();
    void RenderDetectionPanel();
    void RenderPerformancePanel();
    void RenderConsolePanel();
    void RenderStatusBar();

private:
    bool m_initialized = false;
    
#if GUI_ENABLED
    GLFWwindow* m_window = nullptr;
    ImGuiContext* m_imguiContext = nullptr;
    
    // OpenGL 리소스
    GLuint m_frameTexture = 0;
    int m_textureWidth = 0;
    int m_textureHeight = 0;
#endif
    
    // 프레임 데이터
    cv::Mat m_currentFrame;
    std::vector<cv::Rect> m_detections;
    
    // GUI 상태
    bool m_captureEnabled = false;
    bool m_hsvEnabled = true;
    bool m_yoloEnabled = false;
    int m_selectedMonitor = 0;
    std::vector<std::string> m_monitorList;
    
    // HSV 설정
    int m_hsvLower[3] = {140, 120, 180};
    int m_hsvUpper[3] = {160, 200, 255};
    
    // YOLO 설정 (레거시)
    char m_yoloModelPath[256] = "models/yolo.weights";
    char m_yoloConfigPath[256] = "models/yolo.cfg";
    float m_yoloConfidence = 0.5f;
    
    // YOLO v11 설정
    bool m_yolov11Enabled = true;
    char m_yolov11ModelPath[256] = "models/yolo11n.onnx";
    char m_yolov11ClassNamesPath[256] = "models/coco_classes.txt";
    float m_yolov11Confidence = 0.25f;
    float m_yolov11NMS = 0.45f;
#ifdef ONNX_GPU_ONLY
    int m_yolov11Backend = 1;  // GPU 전용: ONNX Runtime GPU만 사용
#else
    int m_yolov11Backend = 0;  // 0: OpenCV DNN, 1: ONNX Runtime GPU
#endif
    
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
};