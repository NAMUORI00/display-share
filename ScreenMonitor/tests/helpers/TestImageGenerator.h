#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

/**
 * @brief 테스트용 이미지 생성 헬퍼 클래스
 */
class TestImageGenerator {
public:
    /**
     * @brief 단색 이미지 생성
     * @param width 이미지 너비
     * @param height 이미지 높이
     * @param color BGR 색상
     * @return 생성된 이미지
     */
    static cv::Mat createSolidColorImage(int width, int height, const cv::Scalar& color);
    
    /**
     * @brief HSV 색상 범위에 맞는 테스트 이미지 생성
     * @param width 이미지 너비
     * @param height 이미지 높이
     * @param hue_min 최소 색조값
     * @param hue_max 최대 색조값
     * @param num_objects 생성할 객체 수
     * @return 생성된 이미지와 객체 위치들
     */
    static std::pair<cv::Mat, std::vector<cv::Rect>> createHSVTestImage(
        int width, int height, int hue_min, int hue_max, int num_objects = 3);
    
    /**
     * @brief 노이즈가 포함된 이미지 생성
     * @param width 이미지 너비
     * @param height 이미지 높이
     * @param noise_level 노이즈 레벨 (0.0-1.0)
     * @return 노이즈가 포함된 이미지
     */
    static cv::Mat createNoisyImage(int width, int height, double noise_level = 0.1);
    
    /**
     * @brief 다양한 크기의 객체가 포함된 이미지 생성
     * @param width 이미지 너비
     * @param height 이미지 높이
     * @param target_color 대상 색상 (HSV)
     * @param sizes 객체 크기들
     * @return 생성된 이미지와 객체 위치들
     */
    static std::pair<cv::Mat, std::vector<cv::Rect>> createMultiSizeObjectImage(
        int width, int height, const cv::Scalar& target_color, const std::vector<int>& sizes);
    
    /**
     * @brief 체크보드 패턴 이미지 생성
     * @param width 이미지 너비
     * @param height 이미지 높이
     * @param square_size 체크보드 사각형 크기
     * @return 체크보드 이미지
     */
    static cv::Mat createCheckerboardImage(int width, int height, int square_size = 50);
    
    /**
     * @brief 그라디언트 이미지 생성
     * @param width 이미지 너비
     * @param height 이미지 높이
     * @param start_color 시작 색상
     * @param end_color 끝 색상
     * @param direction 그라디언트 방향 (0: 수평, 1: 수직)
     * @return 그라디언트 이미지
     */
    static cv::Mat createGradientImage(int width, int height, 
        const cv::Scalar& start_color, const cv::Scalar& end_color, int direction = 0);
    
    /**
     * @brief 원형 객체들이 포함된 이미지 생성
     * @param width 이미지 너비
     * @param height 이미지 높이
     * @param color 원의 색상
     * @param radius 원의 반지름
     * @param count 원의 개수
     * @return 생성된 이미지와 원들의 바운딩 박스
     */
    static std::pair<cv::Mat, std::vector<cv::Rect>> createCircleObjectsImage(
        int width, int height, const cv::Scalar& color, int radius, int count = 5);
    
    /**
     * @brief 임의의 다각형 객체들이 포함된 이미지 생성
     * @param width 이미지 너비
     * @param height 이미지 높이
     * @param color 다각형 색상
     * @param count 다각형 개수
     * @return 생성된 이미지와 바운딩 박스들
     */
    static std::pair<cv::Mat, std::vector<cv::Rect>> createPolygonObjectsImage(
        int width, int height, const cv::Scalar& color, int count = 3);
    
    /**
     * @brief YOLO 테스트용 COCO 스타일 이미지 생성
     * @param width 이미지 너비
     * @param height 이미지 높이
     * @return COCO 데이터셋 스타일의 테스트 이미지
     */
    static cv::Mat createYOLOTestImage(int width, int height);
    
    /**
     * @brief 빈 이미지 생성
     * @param width 이미지 너비
     * @param height 이미지 높이
     * @return 검은색 빈 이미지
     */
    static cv::Mat createEmptyImage(int width, int height);
    
    /**
     * @brief 임의의 색상 생성
     * @param hsv_range HSV 색상 범위 내에서 생성할지 여부
     * @return 임의의 BGR 색상
     */
    static cv::Scalar generateRandomColor(bool hsv_range = false);
    
private:
    // 내부 헬퍼 함수들
    static cv::Point generateRandomPoint(int max_width, int max_height);
    static cv::Rect generateRandomRect(int max_width, int max_height, int min_size = 20, int max_size = 100);
    static std::vector<cv::Point> generateRandomPolygon(const cv::Point& center, int radius, int vertices);
};