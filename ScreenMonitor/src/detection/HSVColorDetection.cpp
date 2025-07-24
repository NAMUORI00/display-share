#include "../../include/detection/HSVColorDetection.h"
#include "../../include/core/Constants.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>

HSVColorDetection::HSVColorDetection()
    : is_initialized_(false)
    , total_detections_(0)
    , average_processing_time_ms_(0.0)
    , total_processing_time_ms_(0.0) {
    
    // 기본 설정 초기화
    hsv_settings_.lower_bound = cv::Scalar(
        Constants::Vision::HSV::DEFAULT_LOWER_H,
        Constants::Vision::HSV::DEFAULT_LOWER_S,
        Constants::Vision::HSV::DEFAULT_LOWER_V
    );
    hsv_settings_.upper_bound = cv::Scalar(
        Constants::Vision::HSV::DEFAULT_UPPER_H,
        Constants::Vision::HSV::DEFAULT_UPPER_S,
        Constants::Vision::HSV::DEFAULT_UPPER_V
    );
    hsv_settings_.min_area = Constants::Vision::HSV::MIN_AREA;
    hsv_settings_.max_area = Constants::Vision::HSV::MAX_AREA;
    hsv_settings_.confidence_threshold = Constants::Vision::HSV::DEFAULT_CONFIDENCE_THRESHOLD;
}

HSVColorDetection::~HSVColorDetection() {
    Cleanup();
}

bool HSVColorDetection::Initialize(const AlgorithmSettings& settings) {
    try {
        // 설정을 HSVSettings로 캐스팅
        const HSVSettings* hsv_settings = dynamic_cast<const HSVSettings*>(&settings);
        if (hsv_settings) {
            if (!ValidateSettings(*hsv_settings)) {
                std::cerr << "[HSVColorDetection] Invalid settings provided" << std::endl;
                return false;
            }
            hsv_settings_ = *hsv_settings;
        } else {
            // 기본 설정 사용
            std::cout << "[HSVColorDetection] Using default HSV settings" << std::endl;
        }
        
        // 성능 메트릭 초기화
        total_detections_ = 0;
        average_processing_time_ms_ = 0.0;
        total_processing_time_ms_ = 0.0;
        last_detection_time_ = std::chrono::high_resolution_clock::now();
        
        is_initialized_ = true;
        
        std::cout << "[HSVColorDetection] Initialized successfully" << std::endl;
        std::cout << "  - HSV Range: [" 
                  << hsv_settings_.lower_bound[0] << ", " << hsv_settings_.lower_bound[1] << ", " << hsv_settings_.lower_bound[2] << "] ~ "
                  << "[" << hsv_settings_.upper_bound[0] << ", " << hsv_settings_.upper_bound[1] << ", " << hsv_settings_.upper_bound[2] << "]" << std::endl;
        std::cout << "  - Area Range: " << hsv_settings_.min_area << " ~ " << hsv_settings_.max_area << std::endl;
        std::cout << "  - Confidence Threshold: " << hsv_settings_.confidence_threshold << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[HSVColorDetection] Initialization failed: " << e.what() << std::endl;
        is_initialized_ = false;
        return false;
    }
}

IDetectionAlgorithm::DetectionResult HSVColorDetection::DetectSingle(const cv::Mat& image) {
    if (!is_initialized_ || image.empty()) {
        return DetectionResult();
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        std::vector<DetectionResult> results = DetectMultiple(image);
        
        if (!results.empty()) {
            // 신뢰도가 가장 높은 결과 반환
            auto best_result = *std::max_element(results.begin(), results.end(),
                [](const DetectionResult& a, const DetectionResult& b) {
                    return a.confidence < b.confidence;
                });
            
            auto end_time = std::chrono::high_resolution_clock::now();
            double processing_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
            UpdatePerformanceStats(processing_time);
            
            return best_result;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[HSVColorDetection] Detection failed: " << e.what() << std::endl;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double processing_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    UpdatePerformanceStats(processing_time);
    
    return DetectionResult();
}

std::vector<IDetectionAlgorithm::DetectionResult> HSVColorDetection::DetectMultiple(const cv::Mat& image) {
    std::vector<DetectionResult> results;
    
    if (!is_initialized_ || image.empty()) {
        return results;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // 이미지 전처리
        cv::Mat preprocessed;
        PreprocessImage(image, preprocessed);
        
        // HSV 변환
        cv::Mat hsv_image;
        cv::cvtColor(preprocessed, hsv_image, cv::COLOR_BGR2HSV);
        
        // HSV 마스크 생성
        cv::Mat mask;
        CreateHSVMask(hsv_image, mask);
        
        // 모폴로지 연산 적용
        cv::Mat morphology_result;
        ApplyMorphology(mask, morphology_result);
        
        // 캐시 업데이트 (디버깅용)
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            last_mask_ = mask.clone();
            last_morphology_result_ = morphology_result.clone();
        }
        
        // 컨투어 찾기
        std::vector<std::vector<cv::Point>> contours;
        FindAndFilterContours(morphology_result, contours);
        
        // 컨투어를 DetectionResult로 변환
        results.reserve(contours.size());
        for (const auto& contour : contours) {
            DetectionResult result = ContourToDetectionResult(contour, image.size());
            if (result.confidence >= hsv_settings_.confidence_threshold) {
                results.push_back(result);
            }
        }
        
        // 신뢰도 순으로 정렬
        std::sort(results.begin(), results.end(),
            [](const DetectionResult& a, const DetectionResult& b) {
                return a.confidence > b.confidence;
            });
        
        // 최대 결과 수 제한
        if (results.size() > Constants::Vision::MAX_DETECTION_RESULTS) {
            results.resize(Constants::Vision::MAX_DETECTION_RESULTS);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[HSVColorDetection] Multiple detection failed: " << e.what() << std::endl;
        results.clear();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double processing_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    UpdatePerformanceStats(processing_time);
    
    return results;
}

std::vector<IDetectionAlgorithm::DetectionResult> HSVColorDetection::DetectInROI(
    const cv::Mat& image, const cv::Rect& roi) {
    
    if (!is_initialized_ || image.empty()) {
        return std::vector<DetectionResult>();
    }
    
    // ROI 유효성 검사
    cv::Rect safe_roi = roi & cv::Rect(0, 0, image.cols, image.rows);
    if (safe_roi.width <= 0 || safe_roi.height <= 0) {
        std::cerr << "[HSVColorDetection] Invalid ROI provided" << std::endl;
        return std::vector<DetectionResult>();
    }
    
    try {
        // ROI 영역만 추출
        cv::Mat roi_image = image(safe_roi);
        
        // ROI에서 감지 수행
        std::vector<DetectionResult> roi_results = DetectMultiple(roi_image);
        
        // 좌표를 원본 이미지 기준으로 변환
        for (auto& result : roi_results) {
            result.bounding_box.x += safe_roi.x;
            result.bounding_box.y += safe_roi.y;
            result.center.x += safe_roi.x;
            result.center.y += safe_roi.y;
        }
        
        return roi_results;
        
    } catch (const std::exception& e) {
        std::cerr << "[HSVColorDetection] ROI detection failed: " << e.what() << std::endl;
        return std::vector<DetectionResult>();
    }
}

bool HSVColorDetection::UpdateSettings(const AlgorithmSettings& settings) {
    const HSVSettings* hsv_settings = dynamic_cast<const HSVSettings*>(&settings);
    if (!hsv_settings) {
        std::cerr << "[HSVColorDetection] Invalid settings type provided" << std::endl;
        return false;
    }
    
    if (!ValidateSettings(*hsv_settings)) {
        std::cerr << "[HSVColorDetection] Invalid settings values provided" << std::endl;
        return false;
    }
    
    hsv_settings_ = *hsv_settings;
    
    std::cout << "[HSVColorDetection] Settings updated successfully" << std::endl;
    return true;
}

std::string HSVColorDetection::GetAlgorithmName() const {
    return "HSV Color Detection";
}

std::string HSVColorDetection::GetVersion() const {
    return Constants::Version::VERSION_STRING;
}

std::vector<std::string> HSVColorDetection::GetSupportedClasses() const {
    return {"colored_object"};  // HSV는 특정 색상 객체만 감지
}

std::string HSVColorDetection::GetPerformanceStats() const {
    std::ostringstream stats;
    stats << "HSV Color Detection Performance:\n";
    stats << "  Total Detections: " << total_detections_.load() << "\n";
    stats << "  Average Processing Time: " << average_processing_time_ms_.load() << " ms\n";
    stats << "  Total Processing Time: " << total_processing_time_ms_.load() << " ms\n";
    
    if (total_detections_ > 0) {
        double avg_fps = 1000.0 / average_processing_time_ms_.load();
        stats << "  Average FPS: " << avg_fps << "\n";
    }
    
    return stats.str();
}

void HSVColorDetection::Reset() {
    total_detections_ = 0;
    average_processing_time_ms_ = 0.0;
    total_processing_time_ms_ = 0.0;
    last_detection_time_ = std::chrono::high_resolution_clock::now();
    
    // 캐시된 이미지 정리
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        last_mask_.release();
        last_morphology_result_.release();
    }
    
    std::cout << "[HSVColorDetection] Performance statistics reset" << std::endl;
}

void HSVColorDetection::Cleanup() {
    Reset();
    is_initialized_ = false;
    std::cout << "[HSVColorDetection] Cleanup completed" << std::endl;
}

void HSVColorDetection::SetHSVRange(const cv::Scalar& lower, const cv::Scalar& upper) {
    hsv_settings_.lower_bound = lower;
    hsv_settings_.upper_bound = upper;
    std::cout << "[HSVColorDetection] HSV range updated: [" 
              << lower[0] << ", " << lower[1] << ", " << lower[2] << "] ~ "
              << "[" << upper[0] << ", " << upper[1] << ", " << upper[2] << "]" << std::endl;
}

void HSVColorDetection::GetHSVRange(cv::Scalar& lower, cv::Scalar& upper) const {
    lower = hsv_settings_.lower_bound;
    upper = hsv_settings_.upper_bound;
}

void HSVColorDetection::SetAreaConstraints(int min_area, int max_area) {
    hsv_settings_.min_area = std::max(1, min_area);
    hsv_settings_.max_area = std::max(hsv_settings_.min_area, max_area);
    std::cout << "[HSVColorDetection] Area constraints updated: " 
              << hsv_settings_.min_area << " ~ " << hsv_settings_.max_area << std::endl;
}

cv::Mat HSVColorDetection::GetLastMask() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return last_mask_.clone();
}

// Private 메서드 구현들

void HSVColorDetection::PreprocessImage(const cv::Mat& image, cv::Mat& preprocessed) const {
    if (hsv_settings_.enable_gaussian_blur) {
        cv::GaussianBlur(image, preprocessed, 
                        cv::Size(hsv_settings_.gaussian_kernel_size, hsv_settings_.gaussian_kernel_size),
                        hsv_settings_.gaussian_sigma);
    } else {
        preprocessed = image.clone();
    }
}

void HSVColorDetection::CreateHSVMask(const cv::Mat& hsv_image, cv::Mat& mask) const {
    cv::inRange(hsv_image, hsv_settings_.lower_bound, hsv_settings_.upper_bound, mask);
}

void HSVColorDetection::ApplyMorphology(const cv::Mat& mask, cv::Mat& result) const {
    if (hsv_settings_.enable_morphology) {
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
            cv::Size(hsv_settings_.morphology_kernel_size, hsv_settings_.morphology_kernel_size));
        
        cv::morphologyEx(mask, result, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(result, result, cv::MORPH_CLOSE, kernel);
    } else {
        result = mask.clone();
    }
}

void HSVColorDetection::FindAndFilterContours(const cv::Mat& mask, 
                                             std::vector<std::vector<cv::Point>>& contours) const {
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(mask, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    // 면적 기준 필터링
    contours.erase(std::remove_if(contours.begin(), contours.end(),
        [this](const std::vector<cv::Point>& contour) {
            double area = cv::contourArea(contour);
            return area < hsv_settings_.min_area || area > hsv_settings_.max_area;
        }), contours.end());
}

IDetectionAlgorithm::DetectionResult HSVColorDetection::ContourToDetectionResult(
    const std::vector<cv::Point>& contour, const cv::Size& image_size) const {
    
    DetectionResult result;
    
    // 바운딩 박스 계산
    result.bounding_box = cv::boundingRect(contour);
    
    // 중심점 계산
    result.center = cv::Point2f(
        result.bounding_box.x + result.bounding_box.width / 2.0f,
        result.bounding_box.y + result.bounding_box.height / 2.0f
    );
    
    // 신뢰도 계산
    result.confidence = CalculateConfidence(contour, image_size);
    
    // 라벨 설정
    result.label = "colored_object";
    result.class_id = 0;
    
    return result;
}

double HSVColorDetection::CalculateConfidence(const std::vector<cv::Point>& contour, 
                                            const cv::Size& image_size) const {
    double area = cv::contourArea(contour);
    double perimeter = cv::arcLength(contour, true);
    
    if (perimeter == 0) return 0.0;
    
    // 형태 기반 신뢰도 (원형도)
    double circularity = 4 * CV_PI * area / (perimeter * perimeter);
    circularity = std::min(1.0, circularity);
    
    // 크기 기반 신뢰도 (적정 크기 범위)
    double size_confidence = 1.0;
    if (area < hsv_settings_.min_area * 2) {
        size_confidence = area / (hsv_settings_.min_area * 2);
    } else if (area > hsv_settings_.max_area / 2) {
        size_confidence = (hsv_settings_.max_area / 2) / area;
    }
    
    // 전체 신뢰도 계산 (가중 평균)
    double confidence = (circularity * 0.6 + size_confidence * 0.4);
    
    return std::max(0.0, std::min(1.0, confidence));
}

void HSVColorDetection::UpdatePerformanceStats(double processing_time) const {
    uint64_t current_detections = total_detections_.fetch_add(1) + 1;
    
    // For atomic<double>, we need to use load/store pattern instead of fetch_add
    double current_total = total_processing_time_ms_.load() + processing_time;
    total_processing_time_ms_.store(current_total);
    
    double new_average = current_total / current_detections;
    average_processing_time_ms_.store(new_average);
    last_detection_time_ = std::chrono::high_resolution_clock::now();
}

bool HSVColorDetection::ValidateSettings(const HSVSettings& settings) const {
    // HSV 범위 유효성 검사
    if (settings.lower_bound[0] < 0 || settings.lower_bound[0] > 179 ||
        settings.upper_bound[0] < 0 || settings.upper_bound[0] > 179 ||
        settings.lower_bound[0] >= settings.upper_bound[0]) {
        return false;
    }
    
    if (settings.lower_bound[1] < 0 || settings.lower_bound[1] > 255 ||
        settings.upper_bound[1] < 0 || settings.upper_bound[1] > 255 ||
        settings.lower_bound[1] >= settings.upper_bound[1]) {
        return false;
    }
    
    if (settings.lower_bound[2] < 0 || settings.lower_bound[2] > 255 ||
        settings.upper_bound[2] < 0 || settings.upper_bound[2] > 255 ||
        settings.lower_bound[2] >= settings.upper_bound[2]) {
        return false;
    }
    
    // 영역 크기 유효성 검사
    if (settings.min_area <= 0 || settings.max_area <= settings.min_area) {
        return false;
    }
    
    // 신뢰도 임계값 유효성 검사
    if (settings.confidence_threshold < Constants::Vision::MIN_CONFIDENCE ||
        settings.confidence_threshold > Constants::Vision::MAX_CONFIDENCE) {
        return false;
    }
    
    // 커널 크기 유효성 검사
    if (settings.morphology_kernel_size <= 0 || settings.morphology_kernel_size % 2 == 0) {
        return false;
    }
    
    if (settings.gaussian_kernel_size <= 0 || settings.gaussian_kernel_size % 2 == 0) {
        return false;
    }
    
    return true;
}