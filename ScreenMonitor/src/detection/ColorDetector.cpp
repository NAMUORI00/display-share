#include "detection/ColorDetector.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <sstream>

ColorDetector::ColorDetector()
    : morph_kernel_size_(3)
    , min_contour_area_(100)
    , min_contour_size_(4)
    , processing_time_ms_(0.0)
    , frame_count_(0)
    , avg_fps_(0.0)
    , configured_(false)
{    
    // 기본 HSV 색상 범위 설정 (파란색 계열)
    color_range_ = HSVRange(140, 120, 180, 160, 200, 255);
    configured_ = true;
}

cv::Rect ColorDetector::detectTarget(const cv::Mat& image)
{
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (image.empty() || !configured_) {
        return cv::Rect(); // 빈 Rect 반환
    }
    
    cv::Mat hsv_image;
    cv::cvtColor(image, hsv_image, cv::COLOR_BGR2HSV);
    
    cv::Mat mask = createColorMask(hsv_image);
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    auto targets = extractTargetsFromContours(contours, image.size());
    
    updatePerformanceStats();
    
    if (targets.empty()) {
        return cv::Rect();
    }
    
    // 가장 큰 객체 반환
    return targets[0].bbox;
}

std::vector<ColorDetector::DetectionResult> ColorDetector::detectMultipleTargets(const cv::Mat& image)
{
    auto start_time = std::chrono::high_resolution_clock::now();
    
    if (image.empty() || !configured_) {
        return std::vector<DetectionResult>();
    }
    
    cv::Mat hsv_image;
    cv::cvtColor(image, hsv_image, cv::COLOR_BGR2HSV);
    
    cv::Mat mask = createColorMask(hsv_image);
    
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    
    auto targets = extractTargetsFromContours(contours, image.size());
    
    updatePerformanceStats();
    
    return targets;
}

void ColorDetector::setColorRange(const HSVRange& range)
{
    color_range_ = range;
    configured_ = true;
}

void ColorDetector::setColorRange(const cv::Scalar& lower, const cv::Scalar& upper)
{
    color_range_.h_min = static_cast<int>(lower[0]);
    color_range_.s_min = static_cast<int>(lower[1]);
    color_range_.v_min = static_cast<int>(lower[2]);
    color_range_.h_max = static_cast<int>(upper[0]);
    color_range_.s_max = static_cast<int>(upper[1]);
    color_range_.v_max = static_cast<int>(upper[2]);
    configured_ = true;
}

bool ColorDetector::initialize()
{
    configured_ = true;
    frame_count_ = 0;
    avg_fps_ = 0.0;
    processing_time_ms_ = 0.0;
    last_process_time_ = std::chrono::high_resolution_clock::now();
    return true;
}

void ColorDetector::reset()
{
    frame_count_ = 0;
    avg_fps_ = 0.0;
    processing_time_ms_ = 0.0;
    last_process_time_ = std::chrono::high_resolution_clock::now();
}

std::string ColorDetector::getPerformanceStats() const
{
    std::stringstream ss;
    ss << "ColorDetector Performance: " 
       << "FPS=" << avg_fps_ 
       << ", ProcessTime=" << processing_time_ms_ << "ms"
       << ", Frames=" << frame_count_;
    return ss.str();
}

cv::Mat ColorDetector::createColorMask(const cv::Mat& hsv_image) const
{
    cv::Mat mask;
    cv::inRange(hsv_image, 
               cv::Scalar(color_range_.h_min, color_range_.s_min, color_range_.v_min),
               cv::Scalar(color_range_.h_max, color_range_.s_max, color_range_.v_max),
               mask);
    
    // 모폴로지 연산으로 노이즈 제거
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, 
                                              cv::Size(morph_kernel_size_, morph_kernel_size_));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
    
    return mask;
}

std::vector<ColorDetector::DetectionResult> ColorDetector::extractTargetsFromContours(
    const std::vector<std::vector<cv::Point>>& contours,
    const cv::Size& original_size) const
{
    std::vector<DetectionResult> results;
    
    for (const auto& contour : contours) {
        if (contour.size() < min_contour_size_) continue;
        
        double area = cv::contourArea(contour);
        if (area < min_contour_area_) continue;
        
        cv::Rect bbox = cv::boundingRect(contour);
        
        // 경계 상자가 이미지 범위 내에 있는지 확인
        if (bbox.x >= 0 && bbox.y >= 0 && 
            bbox.x + bbox.width <= original_size.width &&
            bbox.y + bbox.height <= original_size.height) {
            
            double confidence = std::min(1.0, area / (bbox.width * bbox.height));
            results.emplace_back(bbox, confidence, "color_object");
        }
    }
    
    // 면적 기준으로 정렬 (큰 것부터)
    std::sort(results.begin(), results.end(), 
              [](const DetectionResult& a, const DetectionResult& b) {
                  return (a.bbox.width * a.bbox.height) > (b.bbox.width * b.bbox.height);
              });
    
    return results;
}

void ColorDetector::updatePerformanceStats() const
{
    auto current_time = std::chrono::high_resolution_clock::now();
    
    // 처리 시간 계산
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        current_time - last_process_time_);
    processing_time_ms_ = duration.count() / 1000.0;
    
    // FPS 계산
    frame_count_++;
    if (frame_count_ > 1) {
        double time_diff_sec = duration.count() / 1000000.0;
        if (time_diff_sec > 0) {
            double current_fps = 1.0 / time_diff_sec;
            avg_fps_ = (avg_fps_ * (frame_count_ - 1) + current_fps) / frame_count_;
        }
    }
    
    last_process_time_ = current_time;
}