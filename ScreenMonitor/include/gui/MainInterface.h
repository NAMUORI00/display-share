#pragma once

#include <memory>

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

private:
    bool m_initialized = false;
};