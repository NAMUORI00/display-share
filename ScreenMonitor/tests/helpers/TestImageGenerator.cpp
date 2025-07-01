#define _USE_MATH_DEFINES
#include <cmath>
#include "TestImageGenerator.h"
#include <random>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

cv::Mat TestImageGenerator::createSolidColorImage(int width, int height, const cv::Scalar& color) {
    cv::Mat image = cv::Mat::zeros(height, width, CV_8UC3);
    image.setTo(color);
    return image;
}

std::pair<cv::Mat, std::vector<cv::Rect>> TestImageGenerator::createHSVTestImage(
    int width, int height, int hue_min, int hue_max, int num_objects) {
    
    cv::Mat image = cv::Mat::zeros(height, width, CV_8UC3);
    std::vector<cv::Rect> objects;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> hue_dist(hue_min, hue_max);
    std::uniform_int_distribution<> sat_dist(180, 255);
    std::uniform_int_distribution<> val_dist(180, 255);
    
    for (int i = 0; i < num_objects; ++i) {
        cv::Rect obj_rect = generateRandomRect(width, height, 30, 80);
        
        // HSV 색상 생성
        int h = hue_dist(gen);
        int s = sat_dist(gen);
        int v = val_dist(gen);
        
        cv::Mat hsv_color(1, 1, CV_8UC3, cv::Scalar(h, s, v));
        cv::Mat bgr_color;
        cv::cvtColor(hsv_color, bgr_color, cv::COLOR_HSV2BGR);
        cv::Scalar color = cv::Scalar(bgr_color.at<cv::Vec3b>(0, 0));
        
        cv::rectangle(image, obj_rect, color, -1);
        objects.push_back(obj_rect);
    }
    
    return std::make_pair(image, objects);
}

cv::Mat TestImageGenerator::createNoisyImage(int width, int height, double noise_level) {
    cv::Mat image = cv::Mat::zeros(height, width, CV_8UC3);
    cv::Mat noise = cv::Mat::zeros(height, width, CV_8UC3);
    
    cv::randu(noise, cv::Scalar::all(0), cv::Scalar::all(255));
    
    cv::addWeighted(image, 1.0 - noise_level, noise, noise_level, 0, image);
    
    return image;
}

std::pair<cv::Mat, std::vector<cv::Rect>> TestImageGenerator::createMultiSizeObjectImage(
    int width, int height, const cv::Scalar& target_color, const std::vector<int>& sizes) {
    
    cv::Mat image = cv::Mat::zeros(height, width, CV_8UC3);
    std::vector<cv::Rect> objects;
    
    for (int size : sizes) {
        cv::Point center = generateRandomPoint(width - size, height - size);
        cv::Rect obj_rect(center.x, center.y, size, size);
        
        cv::rectangle(image, obj_rect, target_color, -1);
        objects.push_back(obj_rect);
    }
    
    return std::make_pair(image, objects);
}

cv::Mat TestImageGenerator::createCheckerboardImage(int width, int height, int square_size) {
    cv::Mat image = cv::Mat::zeros(height, width, CV_8UC3);
    
    for (int y = 0; y < height; y += square_size) {
        for (int x = 0; x < width; x += square_size) {
            bool is_white = ((x / square_size) + (y / square_size)) % 2 == 0;
            cv::Scalar color = is_white ? cv::Scalar(255, 255, 255) : cv::Scalar(0, 0, 0);
            
            cv::Rect rect(x, y, 
                         std::min(square_size, width - x), 
                         std::min(square_size, height - y));
            cv::rectangle(image, rect, color, -1);
        }
    }
    
    return image;
}

cv::Mat TestImageGenerator::createGradientImage(int width, int height, 
    const cv::Scalar& start_color, const cv::Scalar& end_color, int direction) {
    
    cv::Mat image = cv::Mat::zeros(height, width, CV_8UC3);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double ratio;
            if (direction == 0) { // 수평
                ratio = static_cast<double>(x) / width;
            } else { // 수직
                ratio = static_cast<double>(y) / height;
            }
            
            cv::Scalar pixel_color = start_color * (1.0 - ratio) + end_color * ratio;
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(
                static_cast<uchar>(pixel_color[0]),
                static_cast<uchar>(pixel_color[1]),
                static_cast<uchar>(pixel_color[2])
            );
        }
    }
    
    return image;
}

std::pair<cv::Mat, std::vector<cv::Rect>> TestImageGenerator::createCircleObjectsImage(
    int width, int height, const cv::Scalar& color, int radius, int count) {
    
    cv::Mat image = cv::Mat::zeros(height, width, CV_8UC3);
    std::vector<cv::Rect> objects;
    
    for (int i = 0; i < count; ++i) {
        cv::Point center = generateRandomPoint(width - 2 * radius, height - 2 * radius);
        center.x += radius;
        center.y += radius;
        
        cv::circle(image, center, radius, color, -1);
        
        cv::Rect bounding_box(center.x - radius, center.y - radius, 2 * radius, 2 * radius);
        objects.push_back(bounding_box);
    }
    
    return std::make_pair(image, objects);
}

std::pair<cv::Mat, std::vector<cv::Rect>> TestImageGenerator::createPolygonObjectsImage(
    int width, int height, const cv::Scalar& color, int count) {
    
    cv::Mat image = cv::Mat::zeros(height, width, CV_8UC3);
    std::vector<cv::Rect> objects;
    
    for (int i = 0; i < count; ++i) {
        cv::Point center = generateRandomPoint(width - 100, height - 100);
        center.x += 50;
        center.y += 50;
        
        std::vector<cv::Point> polygon = generateRandomPolygon(center, 30, 6);
        
        std::vector<std::vector<cv::Point>> polygons = {polygon};
        cv::fillPoly(image, polygons, color);
        
        cv::Rect bounding_box = cv::boundingRect(polygon);
        objects.push_back(bounding_box);
    }
    
    return std::make_pair(image, objects);
}

cv::Mat TestImageGenerator::createYOLOTestImage(int width, int height) {
    cv::Mat image = createSolidColorImage(width, height, cv::Scalar(128, 128, 128));
    
    // 자동차 모양 생성
    cv::Rect car_body(width/4, height/2, width/2, height/4);
    cv::rectangle(image, car_body, cv::Scalar(0, 0, 255), -1); // 빨간색 자동차
    
    // 바퀴 생성
    cv::circle(image, cv::Point(width/4 + 20, height/2 + height/4), 15, cv::Scalar(0, 0, 0), -1);
    cv::circle(image, cv::Point(width*3/4 - 20, height/2 + height/4), 15, cv::Scalar(0, 0, 0), -1);
    
    // 사람 모양 생성
    cv::circle(image, cv::Point(width/8, height/3), 20, cv::Scalar(255, 200, 150), -1); // 머리
    cv::rectangle(image, cv::Rect(width/8 - 15, height/3 + 20, 30, 50), cv::Scalar(0, 255, 0), -1); // 몸통
    
    return image;
}

cv::Mat TestImageGenerator::createEmptyImage(int width, int height) {
    return cv::Mat::zeros(height, width, CV_8UC3);
}

cv::Scalar TestImageGenerator::generateRandomColor(bool hsv_range) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    if (hsv_range) {
        std::uniform_int_distribution<> h_dist(0, 179);
        std::uniform_int_distribution<> s_dist(100, 255);
        std::uniform_int_distribution<> v_dist(100, 255);
        
        cv::Mat hsv_color(1, 1, CV_8UC3, cv::Scalar(h_dist(gen), s_dist(gen), v_dist(gen)));
        cv::Mat bgr_color;
        cv::cvtColor(hsv_color, bgr_color, cv::COLOR_HSV2BGR);
        return cv::Scalar(bgr_color.at<cv::Vec3b>(0, 0));
    } else {
        std::uniform_int_distribution<> color_dist(0, 255);
        return cv::Scalar(color_dist(gen), color_dist(gen), color_dist(gen));
    }
}

cv::Point TestImageGenerator::generateRandomPoint(int max_width, int max_height) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> x_dist(0, max_width);
    std::uniform_int_distribution<> y_dist(0, max_height);
    
    return cv::Point(x_dist(gen), y_dist(gen));
}

cv::Rect TestImageGenerator::generateRandomRect(int max_width, int max_height, int min_size, int max_size) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> size_dist(min_size, max_size);
    
    int width = size_dist(gen);
    int height = size_dist(gen);
    
    std::uniform_int_distribution<> x_dist(0, std::max(0, max_width - width));
    std::uniform_int_distribution<> y_dist(0, std::max(0, max_height - height));
    
    return cv::Rect(x_dist(gen), y_dist(gen), width, height);
}

std::vector<cv::Point> TestImageGenerator::generateRandomPolygon(const cv::Point& center, int radius, int vertices) {
    std::vector<cv::Point> polygon;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> angle_dist(0, 2 * M_PI);
    std::uniform_real_distribution<> radius_dist(radius * 0.7, radius * 1.3);
    
    for (int i = 0; i < vertices; ++i) {
        double angle = (2 * M_PI * i) / vertices + angle_dist(gen) * 0.2;
        double r = radius_dist(gen);
        
        cv::Point vertex(
            center.x + static_cast<int>(r * cos(angle)),
            center.y + static_cast<int>(r * sin(angle))
        );
        polygon.push_back(vertex);
    }
    
    return polygon;
}