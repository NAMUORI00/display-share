#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <filesystem>

using json = nlohmann::json;

/**
 * @brief 테스트용 설정 관리 헬퍼 클래스
 */
class ConfigTestHelper {
public:
    /**
     * @brief 기본 테스트 설정 JSON 생성
     * @return 기본 테스트 설정
     */
    static json createDefaultTestConfig();
    
    /**
     * @brief 최소한의 유효한 설정 생성
     * @return 최소 유효 설정
     */
    static json createMinimalValidConfig();
    
    /**
     * @brief 잘못된 설정 생성
     * @return 스키마 검증에 실패하는 설정
     */
    static json createInvalidConfig();
    
    /**
     * @brief HSV 색상 범위가 다른 설정 생성
     * @param h_min 최소 색조
     * @param h_max 최대 색조
     * @param s_min 최소 채도
     * @param s_max 최대 채도
     * @param v_min 최소 명도
     * @param v_max 최대 명도
     * @return HSV 설정이 포함된 JSON
     */
    static json createHSVTestConfig(int h_min, int h_max, int s_min, int s_max, int v_min, int v_max);
    
    /**
     * @brief YOLO 설정이 포함된 테스트 설정 생성
     * @param model_path YOLO 모델 경로
     * @param config_path YOLO 설정 경로
     * @param confidence 신뢰도 임계값
     * @return YOLO 설정이 포함된 JSON
     */
    static json createYOLOTestConfig(const std::string& model_path, 
                                    const std::string& config_path, 
                                    double confidence = 0.5);
    
    /**
     * @brief 성능 설정이 다른 테스트 설정 생성
     * @param target_fps 목표 FPS
     * @param multithreading 멀티스레딩 사용 여부
     * @param max_threads 최대 스레드 수
     * @return 성능 설정이 포함된 JSON
     */
    static json createPerformanceTestConfig(int target_fps, bool multithreading, int max_threads);
    
    /**
     * @brief 임시 설정 파일 생성
     * @param config 저장할 설정 JSON
     * @return 생성된 임시 파일 경로
     */
    static std::string createTempConfigFile(const json& config);
    
    /**
     * @brief 빈 설정 파일 생성
     * @return 빈 설정 파일 경로
     */
    static std::string createEmptyConfigFile();
    
    /**
     * @brief 잘못된 JSON 형식의 설정 파일 생성
     * @return 잘못된 JSON 파일 경로
     */
    static std::string createInvalidJSONFile();
    
    /**
     * @brief 접근할 수 없는 설정 파일 경로 생성
     * @return 접근 불가능한 파일 경로
     */
    static std::string createInaccessibleFilePath();
    
    /**
     * @brief 필수 섹션이 누락된 설정 생성
     * @param missing_section 누락시킬 섹션 이름
     * @return 섹션이 누락된 설정
     */
    static json createConfigWithMissingSection(const std::string& missing_section);
    
    /**
     * @brief 잘못된 데이터 타입이 포함된 설정 생성
     * @return 잘못된 타입의 설정
     */
    static json createConfigWithWrongTypes();
    
    /**
     * @brief 범위를 벗어난 값이 포함된 설정 생성
     * @return 범위 초과 값이 포함된 설정
     */
    static json createConfigWithOutOfRangeValues();
    
    /**
     * @brief 기본 스키마 JSON 생성
     * @return 기본 스키마
     */
    static json createDefaultSchema();
    
    /**
     * @brief 설정 비교
     * @param config1 첫 번째 설정
     * @param config2 두 번째 설정
     * @param ignore_keys 무시할 키들
     * @return 설정이 같은지 여부
     */
    static bool compareConfigs(const json& config1, const json& config2, 
                              const std::vector<std::string>& ignore_keys = {});
    
    /**
     * @brief 임시 파일 정리
     * @param file_path 정리할 파일 경로
     */
    static void cleanupTempFile(const std::string& file_path);
    
    /**
     * @brief 모든 임시 파일 정리
     */
    static void cleanupAllTempFiles();
    
private:
    static std::vector<std::string> temp_files_;
    static std::string generateTempFileName();
};