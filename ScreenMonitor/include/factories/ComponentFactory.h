#pragma once

#include "../interfaces/ICaptureDevice.h"
#include "../interfaces/IDetectionAlgorithm.h"
#include <memory>
#include <string>
#include <map>
#include <functional>

/**
 * @brief 컴포넌트 팩토리 클래스
 * 
 * Factory 패턴을 구현하여 각종 컴포넌트의 생성을 담당합니다.
 */
class ComponentFactory {
public:
    /**
     * @brief 캡처 디바이스 타입 열거형
     */
    enum class CaptureDeviceType {
        SCREEN_CAPTURE_LITE,    // screen_capture_lite 기반
        DIRECTX_CAPTURE,        // DirectX 기반 (Windows)
        XLIB_CAPTURE,           // X11 기반 (Linux)
        CORE_GRAPHICS_CAPTURE   // Core Graphics 기반 (macOS)
    };

    /**
     * @brief 감지 알고리즘 타입 열거형
     */
    enum class DetectionAlgorithmType {
        HSV_COLOR_DETECTION,    // HSV 색상 기반 감지
        YOLO_OBJECT_DETECTION,  // YOLO 객체 감지
        TEMPLATE_MATCHING,      // 템플릿 매칭
        FEATURE_MATCHING        // 특징점 매칭
    };

    ComponentFactory() = default;
    ~ComponentFactory() = default;

    // 싱글톤 패턴
    static ComponentFactory& GetInstance();

    /**
     * @brief 캡처 디바이스 생성
     * @param type 캡처 디바이스 타입
     * @return 생성된 캡처 디바이스 인스턴스
     */
    std::unique_ptr<ICaptureDevice> CreateCaptureDevice(CaptureDeviceType type);

    /**
     * @brief 감지 알고리즘 생성
     * @param type 감지 알고리즘 타입
     * @return 생성된 감지 알고리즘 인스턴스
     */
    std::unique_ptr<IDetectionAlgorithm> CreateDetectionAlgorithm(DetectionAlgorithmType type);

    /**
     * @brief 문자열로 캡처 디바이스 생성
     * @param type_name 디바이스 타입 이름
     * @return 생성된 캡처 디바이스 인스턴스
     */
    std::unique_ptr<ICaptureDevice> CreateCaptureDevice(const std::string& type_name);

    /**
     * @brief 문자열로 감지 알고리즘 생성
     * @param type_name 알고리즘 타입 이름
     * @return 생성된 감지 알고리즘 인스턴스
     */
    std::unique_ptr<IDetectionAlgorithm> CreateDetectionAlgorithm(const std::string& type_name);

    /**
     * @brief 사용 가능한 캡처 디바이스 타입 목록
     * @return 타입 이름 목록
     */
    std::vector<std::string> GetAvailableCaptureDeviceTypes() const;

    /**
     * @brief 사용 가능한 감지 알고리즘 타입 목록
     * @return 타입 이름 목록
     */
    std::vector<std::string> GetAvailableDetectionAlgorithmTypes() const;

    /**
     * @brief 현재 플랫폼에서 권장되는 캡처 디바이스 타입
     * @return 권장 타입
     */
    CaptureDeviceType GetRecommendedCaptureDeviceType() const;

    // 복사 생성자 및 대입 연산자 삭제 (싱글톤)
    ComponentFactory(const ComponentFactory&) = delete;
    ComponentFactory& operator=(const ComponentFactory&) = delete;

private:
    // 팩토리 함수 타입 정의
    using CaptureDeviceCreator = std::function<std::unique_ptr<ICaptureDevice>()>;
    using DetectionAlgorithmCreator = std::function<std::unique_ptr<IDetectionAlgorithm>()>;

    // 등록된 생성자 맵
    std::map<std::string, CaptureDeviceCreator> capture_device_creators_;
    std::map<std::string, DetectionAlgorithmCreator> detection_algorithm_creators_;

    /**
     * @brief 기본 생성자들 등록
     */
    void RegisterDefaultCreators();

    /**
     * @brief 타입을 문자열로 변환
     */
    std::string CaptureDeviceTypeToString(CaptureDeviceType type) const;
    std::string DetectionAlgorithmTypeToString(DetectionAlgorithmType type) const;
};