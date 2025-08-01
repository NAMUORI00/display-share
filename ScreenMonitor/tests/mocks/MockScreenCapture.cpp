#include "MockScreenCapture.h"
#include "../helpers/TestImageGenerator.h"
#include <thread>
#include <chrono>
#include <random>

using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::AtLeast;

MockScreenCapture::MockScreenCapture() {
    // 기본 mock 프레임 생성
    m_mock_frame = TestImageGenerator::createSolidColorImage(640, 480, cv::Scalar(100, 150, 200));
    
    // 기본 모니터 정보 생성
    m_mock_monitors = {
        MonitorInfo(0, "Primary Monitor", 1920, 1080, 0, 0, true),
        MonitorInfo(1, "Secondary Monitor", 1280, 720, 1920, 0, false),
        MonitorInfo(2, "Third Monitor", 1440, 900, 0, 1080, false)
    };
    
    // 기본 설정
    m_mock_settings.target_fps = 60;
    m_mock_settings.enable_cursor = false;
    m_mock_settings.enable_border = false;
    m_mock_settings.capture_area = cv::Rect(0, 0, 0, 0);
    
    SetupDefaultBehavior();
}

void MockScreenCapture::SetMockFrame(const cv::Mat& frame) {
    m_mock_frame = frame.clone();
}

void MockScreenCapture::SetMockMonitors(const std::vector<MonitorInfo>& monitors) {
    m_mock_monitors = monitors;
}

void MockScreenCapture::SetCaptureSuccessRate(double success_rate) {
    m_success_rate = std::max(0.0, std::min(1.0, success_rate));
}

void MockScreenCapture::SetFrameSequence(const std::vector<cv::Mat>& frames) {
    m_frame_sequence = frames;
    m_current_frame_index = 0;
}

void MockScreenCapture::SetupDefaultBehavior() {
    // 초기화 성공
    ON_CALL(*this, Initialize(_))
        .WillByDefault(Invoke([this](const CaptureSettings& settings) {
            SimulateProcessingDelay();
            m_mock_settings = settings;
            m_is_initialized = !m_should_fail;
            return m_is_initialized;
        }));
    
    // 캡처 시작
    ON_CALL(*this, StartCapture(_))
        .WillByDefault(Invoke([this](int monitor_index) {
            SimulateProcessingDelay();
            if (monitor_index >= 0 && monitor_index < static_cast<int>(m_mock_monitors.size()) && m_is_initialized) {
                return ShouldSucceed();
            }
            return false;
        }));
    
    // 캡처 중지
    ON_CALL(*this, StopCapture())
        .WillByDefault(Invoke([this]() {
            // 캡처 중지 로직
        }));
    
    // 프레임 캡처
    ON_CALL(*this, CaptureFrame(_))
        .WillByDefault(Invoke([this](cv::Mat& output_frame) {
            SimulateProcessingDelay();
            m_capture_call_count++;
            
            if (!m_is_initialized || !ShouldSucceed()) {
                return false;
            }
            
            cv::Mat frame = GetNextFrame();
            if (!frame.empty()) {
                frame.copyTo(output_frame);
                return true;
            }
            return false;
        }));
    
    // 모니터 목록 반환
    ON_CALL(*this, GetAvailableMonitors())
        .WillByDefault(Return(m_mock_monitors));
    
    // 캡처 상태 확인
    ON_CALL(*this, IsCapturing())
        .WillByDefault(Return(m_is_initialized));
    
    // 현재 모니터 인덱스
    ON_CALL(*this, GetCurrentMonitorIndex())
        .WillByDefault(Return(0));
    
    // 설정 업데이트
    ON_CALL(*this, UpdateSettings(_))
        .WillByDefault(Invoke([this](const CaptureSettings& settings) {
            m_mock_settings = settings;
            return true;
        }));
    
    // 성능 통계
    ON_CALL(*this, GetPerformanceStats())
        .WillByDefault(Invoke([this]() {
            return "Mock Performance Stats: FPS=60.0, Calls=" + std::to_string(m_capture_call_count);
        }));
    
    // 정리
    ON_CALL(*this, Cleanup())
        .WillByDefault(Invoke([this]() {
            m_is_initialized = false;
            m_current_frame_index = 0;
            m_capture_call_count = 0;
        }));
}

void MockScreenCapture::SetupFailureScenario() {
    m_should_fail = true;
    
    ON_CALL(*this, Initialize(_))
        .WillByDefault(Return(false));
    
    ON_CALL(*this, StartCapture(_))
        .WillByDefault(Return(false));
    
    ON_CALL(*this, CaptureFrame(_))
        .WillByDefault(Return(false));
    
    ON_CALL(*this, IsCapturing())
        .WillByDefault(Return(false));
    
    ON_CALL(*this, UpdateSettings(_))
        .WillByDefault(Return(false));
}

void MockScreenCapture::SetupPerformanceScenario() {
    // 고성능 시나리오: 지연 최소화, 높은 성공률
    m_delay_ms = 1;
    m_success_rate = 0.99;
    
    // 다양한 해상도의 프레임 시퀀스 생성
    std::vector<cv::Mat> frames;
    frames.push_back(TestImageGenerator::createSolidColorImage(1920, 1080, cv::Scalar(255, 0, 0)));
    frames.push_back(TestImageGenerator::createSolidColorImage(1280, 720, cv::Scalar(0, 255, 0)));
    frames.push_back(TestImageGenerator::createSolidColorImage(640, 480, cv::Scalar(0, 0, 255)));
    
    SetFrameSequence(frames);
    
    // 높은 FPS 설정
    m_mock_settings.target_fps = 120;
}

bool MockScreenCapture::ShouldSucceed() {
    if (m_should_fail) return false;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    return dis(gen) < m_success_rate;
}

void MockScreenCapture::SimulateProcessingDelay() {
    if (m_delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(m_delay_ms));
    }
}

cv::Mat MockScreenCapture::GetNextFrame() {
    if (m_frame_sequence.empty()) {
        return m_mock_frame.clone();
    }
    
    cv::Mat frame = m_frame_sequence[m_current_frame_index].clone();
    m_current_frame_index = (m_current_frame_index + 1) % m_frame_sequence.size();
    
    return frame;
}