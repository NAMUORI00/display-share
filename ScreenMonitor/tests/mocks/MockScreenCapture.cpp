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
        {"Primary Monitor", 1920, 1080, true},
        {"Secondary Monitor", 1280, 720, false},
        {"Third Monitor", 1440, 900, false}
    };
    
    // 기본 설정
    m_mock_settings.selectedMonitorIndex = 0;
    m_mock_settings.targetFPS = 60.0;
    m_mock_settings.enableFPSLimiting = false;
    
    SetupDefaultBehavior();
}

void MockScreenCapture::SetMockFrame(const cv::Mat& frame) {
    m_mock_frame = frame.clone();
}

void MockScreenCapture::SetMockMonitors(const std::vector<HighSpeedCapture::MonitorInfo>& monitors) {
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
    ON_CALL(*this, Initialize())
        .WillByDefault(Invoke([this]() {
            SimulateProcessingDelay();
            m_is_initialized = !m_should_fail;
            return m_is_initialized;
        }));
    
    ON_CALL(*this, Initialize(_))
        .WillByDefault(Invoke([this](const HighSpeedCapture::CaptureSettings& settings) {
            SimulateProcessingDelay();
            m_mock_settings = settings;
            m_is_initialized = !m_should_fail;
            return m_is_initialized;
        }));
    
    // 캡처 성공
    ON_CALL(*this, CaptureFrame())
        .WillByDefault(Invoke([this]() {
            SimulateProcessingDelay();
            m_capture_call_count++;
            return ShouldSucceed() && m_is_initialized;
        }));
    
    ON_CALL(*this, CaptureScreen())
        .WillByDefault(Invoke([this]() {
            return CaptureFrame();
        }));
    
    // 프레임 반환
    ON_CALL(*this, GetLatestFrame())
        .WillByDefault(Invoke([this]() -> cv::Mat {
            if (!m_is_initialized) {
                return cv::Mat();
            }
            return GetNextFrame();
        }));
    
    // 모니터 관리
    ON_CALL(*this, SetTargetMonitor(_))
        .WillByDefault(Invoke([this](int monitorIndex) {
            if (monitorIndex >= 0 && monitorIndex < static_cast<int>(m_mock_monitors.size())) {
                m_mock_settings.selectedMonitorIndex = monitorIndex;
                return true;
            }
            return false;
        }));
    
    ON_CALL(*this, GetCurrentMonitorIndex())
        .WillByDefault(Return(m_mock_settings.selectedMonitorIndex));
    
    ON_CALL(*this, GetAvailableMonitors())
        .WillByDefault(Return(m_mock_monitors));
    
    // 설정 관리
    ON_CALL(*this, UpdateSettings(_))
        .WillByDefault(Invoke([this](const HighSpeedCapture::CaptureSettings& settings) {
            m_mock_settings = settings;
        }));
    
    ON_CALL(*this, GetSettings())
        .WillByDefault(Return(m_mock_settings));
    
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
    
    ON_CALL(*this, Initialize())
        .WillByDefault(Return(false));
    
    ON_CALL(*this, Initialize(_))
        .WillByDefault(Return(false));
    
    ON_CALL(*this, CaptureFrame())
        .WillByDefault(Return(false));
    
    ON_CALL(*this, CaptureScreen())
        .WillByDefault(Return(false));
    
    ON_CALL(*this, GetLatestFrame())
        .WillByDefault(Return(cv::Mat()));
    
    ON_CALL(*this, SetTargetMonitor(_))
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
    m_mock_settings.targetFPS = 120.0;
    m_mock_settings.enableFPSLimiting = false;
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