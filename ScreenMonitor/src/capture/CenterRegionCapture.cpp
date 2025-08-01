#include "capture/CenterRegionCapture.h"
#include <algorithm>
#include <iostream>

CenterRegionCapture::CenterRegionCapture() 
    : total_extractions_(0)
    , total_extraction_time_ms_(0.0)
    , allocated_memory_bytes_(0)
    , last_extraction_time_(std::chrono::high_resolution_clock::now()) {
}

bool CenterRegionCapture::ExtractCenterRegion(const cv::Mat& source_image, cv::Mat& output_region) {
    if (source_image.empty()) {
        std::cerr << "[CenterRegionCapture] Source image is empty." << std::endl;
        return false;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // 중심 영역 정보 계산
        RegionInfo region_info = CalculateRegionInfo(source_image.cols, source_image.rows);
        
        // 영역 유효성 검사
        if (region_info.x < 0 || region_info.y < 0 || 
            region_info.x + region_info.width > source_image.cols || 
            region_info.y + region_info.height > source_image.rows) {
            std::cerr << "[CenterRegionCapture] Calculated region exceeds image boundaries: "
                      << "x=" << region_info.x << ", y=" << region_info.y 
                      << ", width=" << region_info.width << ", height=" << region_info.height
                      << ", source_size=" << source_image.cols << "x" << source_image.rows << std::endl;
            return false;
        }

        // ROI 영역 정의 및 추출 (메모리 복사 최소화)
        cv::Rect roi(region_info.x, region_info.y, region_info.width, region_info.height);
        cv::Mat roi_image = source_image(roi);
        
        // 크기가 정확히 320x320인지 확인 후 복사
        if (roi_image.cols == TARGET_WIDTH && roi_image.rows == TARGET_HEIGHT) {
            // 직접 복사 (최적화)
            roi_image.copyTo(output_region);
        } else {
            // 리사이즈 필요 (예외적인 경우)
            cv::resize(roi_image, output_region, cv::Size(TARGET_WIDTH, TARGET_HEIGHT), 0, 0, cv::INTER_LINEAR);
        }

        // 성능 메트릭 업데이트
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double extraction_time_ms = duration.count() / 1000.0;
        
        UpdatePerformanceMetrics(extraction_time_ms);

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[CenterRegionCapture] Error during center region extraction: " << e.what() << std::endl;
        return false;
    }
}

CenterRegionCapture::RegionInfo CenterRegionCapture::CalculateRegionInfo(int source_width, int source_height) const {
    RegionInfo info;
    
    // 중심 좌표 계산
    int center_x = source_width / 2;
    int center_y = source_height / 2;
    
    // 320x320 영역의 시작 좌표 계산
    info.x = std::max(0, center_x - TARGET_WIDTH / 2);
    info.y = std::max(0, center_y - TARGET_HEIGHT / 2);
    
    // 영역이 이미지 경계를 벗어나지 않도록 조정
    if (info.x + TARGET_WIDTH > source_width) {
        info.x = std::max(0, source_width - TARGET_WIDTH);
    }
    if (info.y + TARGET_HEIGHT > source_height) {
        info.y = std::max(0, source_height - TARGET_HEIGHT);
    }
    
    // 실제 추출될 영역 크기 계산 (경계 조정 후)
    info.width = std::min(TARGET_WIDTH, source_width - info.x);
    info.height = std::min(TARGET_HEIGHT, source_height - info.y);
    
    // 원본 이미지 정보
    info.source_width = source_width;
    info.source_height = source_height;
    
    // 좌표 변환을 위한 스케일 팩터 계산
    info.scale_x = static_cast<double>(source_width) / TARGET_WIDTH;
    info.scale_y = static_cast<double>(source_height) / TARGET_HEIGHT;
    
    return info;
}

void CenterRegionCapture::TransformToScreenCoordinates(int region_x, int region_y, 
                                                     const RegionInfo& region_info,
                                                     int& screen_x, int& screen_y) const {
    // 320x320 영역 내 좌표를 전체 화면 좌표로 변환
    screen_x = region_info.x + region_x;
    screen_y = region_info.y + region_y;
    
    // 경계 검사
    screen_x = std::max(0, std::min(screen_x, region_info.source_width - 1));
    screen_y = std::max(0, std::min(screen_y, region_info.source_height - 1));
}

bool CenterRegionCapture::TransformToRegionCoordinates(int screen_x, int screen_y,
                                                      const RegionInfo& region_info,
                                                      int& region_x, int& region_y) const {
    // 전체 화면 좌표를 320x320 영역 좌표로 변환
    region_x = screen_x - region_info.x;
    region_y = screen_y - region_info.y;
    
    // 320x320 영역 내에 있는지 확인
    bool is_inside = (region_x >= 0 && region_x < region_info.width && 
                     region_y >= 0 && region_y < region_info.height);
    
    if (!is_inside) {
        region_x = std::max(0, std::min(region_x, region_info.width - 1));
        region_y = std::max(0, std::min(region_y, region_info.height - 1));
    }
    
    return is_inside;
}

CenterRegionCapture::PerformanceMetrics CenterRegionCapture::GetPerformanceMetrics() const {
    PerformanceMetrics metrics;
    
    uint64_t total_extractions = total_extractions_.load();
    double total_time = total_extraction_time_ms_.load();
    
    metrics.total_extractions = total_extractions;
    metrics.extraction_time_ms = total_time;
    metrics.average_time_ms = (total_extractions > 0) ? (total_time / total_extractions) : 0.0;
    metrics.memory_usage_bytes = allocated_memory_bytes_.load();
    
    return metrics;
}

void CenterRegionCapture::ResetPerformanceMetrics() {
    total_extractions_ = 0;
    total_extraction_time_ms_ = 0.0;
    last_extraction_time_ = std::chrono::high_resolution_clock::now();
}

void CenterRegionCapture::OptimizeMemoryAllocation(int expected_width, int expected_height) {
    try {
        // 예상 크기에 따른 임시 버퍼 사전 할당
        size_t expected_bytes = expected_width * expected_height * 3; // BGR 3채널
        
        if (expected_width >= TARGET_WIDTH && expected_height >= TARGET_HEIGHT) {
            // 320x320 결과 버퍼 사전 할당
            temp_buffer_.create(TARGET_HEIGHT, TARGET_WIDTH, CV_8UC3);
            allocated_memory_bytes_ = TARGET_WIDTH * TARGET_HEIGHT * 3;
            
            std::cout << "[CenterRegionCapture] Memory optimization complete: "
                      << TARGET_WIDTH << "x" << TARGET_HEIGHT 
                      << " (" << allocated_memory_bytes_.load() << " bytes)" << std::endl;
        } else {
            std::cerr << "[CenterRegionCapture] Warning: Expected image size is smaller than 320x320: "
                      << expected_width << "x" << expected_height << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[CenterRegionCapture] Memory allocation optimization failed: " << e.what() << std::endl;
    }
}

void CenterRegionCapture::UpdatePerformanceMetrics(double extraction_time_ms) const {
    total_extractions_++;
    
    // 원자적 연산으로 누적 시간 업데이트
    double current_total = total_extraction_time_ms_.load();
    while (!total_extraction_time_ms_.compare_exchange_weak(current_total, current_total + extraction_time_ms)) {
        // CAS(Compare-And-Swap) 재시도
    }
    
    last_extraction_time_ = std::chrono::high_resolution_clock::now();
}