#pragma once

#include <GLFW/glfw3.h>
#include <memory>

/**
 * @brief 테스트용 OpenGL 컨텍스트 헬퍼 클래스
 * 
 * GUI 테스트를 위한 헤드리스 OpenGL 컨텍스트 생성 및 관리
 */
class GLTestContext {
public:
    /**
     * @brief GLTestContext 생성자
     */
    GLTestContext();
    
    /**
     * @brief 소멸자
     */
    ~GLTestContext();
    
    /**
     * @brief OpenGL 컨텍스트 초기화
     * @param headless 헤드리스 모드 여부 (기본값: true)
     * @return 초기화 성공 여부
     */
    bool Initialize(bool headless = true);
    
    /**
     * @brief 컨텍스트 정리
     */
    void Cleanup();
    
    /**
     * @brief 컨텍스트가 초기화되었는지 확인
     * @return 초기화 상태
     */
    bool IsInitialized() const { return m_initialized; }
    
    /**
     * @brief GLFW 윈도우 핸들 반환
     * @return GLFW 윈도우 포인터
     */
    GLFWwindow* GetWindow() const { return m_window; }
    
    /**
     * @brief 컨텍스트를 현재 스레드에 바인딩
     */
    void MakeCurrent();
    
    /**
     * @brief OpenGL 버전 정보 반환
     * @return OpenGL 버전 문자열
     */
    std::string GetOpenGLVersion() const;
    
    /**
     * @brief OpenGL 렌더러 정보 반환
     * @return 렌더러 문자열
     */
    std::string GetRenderer() const;
    
    /**
     * @brief 프레임버퍼 크기 설정
     * @param width 너비
     * @param height 높이
     */
    void SetFramebufferSize(int width, int height);
    
    /**
     * @brief 프레임버퍼 크기 반환
     * @return {width, height} 쌍
     */
    std::pair<int, int> GetFramebufferSize() const;
    
    /**
     * @brief OpenGL 오류 확인
     * @return 오류가 있으면 오류 문자열, 없으면 빈 문자열
     */
    std::string CheckGLError() const;
    
    /**
     * @brief 뷰포트 설정
     * @param x X 좌표
     * @param y Y 좌표
     * @param width 너비
     * @param height 높이
     */
    void SetViewport(int x, int y, int width, int height);
    
    /**
     * @brief 화면 클리어
     * @param r 빨간색 (0.0-1.0)
     * @param g 녹색 (0.0-1.0)
     * @param b 파란색 (0.0-1.0)
     * @param a 알파 (0.0-1.0)
     */
    void Clear(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
    
    /**
     * @brief 버퍼 스왑 (더블 버퍼링)
     */
    void SwapBuffers();
    
    /**
     * @brief 테스트용 텍스처 생성
     * @param width 텍스처 너비
     * @param height 텍스처 높이
     * @param format 픽셀 포맷 (기본값: GL_RGB)
     * @return 텍스처 ID
     */
    unsigned int CreateTestTexture(int width, int height, unsigned int format = 0x1907); // GL_RGB
    
    /**
     * @brief 텍스처 삭제
     * @param texture_id 삭제할 텍스처 ID
     */
    void DeleteTexture(unsigned int texture_id);
    
    /**
     * @brief 정적 인스턴스 반환
     * @return 전역 GLTestContext 인스턴스
     */
    static GLTestContext& GetInstance();
    
    /**
     * @brief GLFW 초기화 상태 확인
     * @return GLFW 초기화 여부
     */
    static bool IsGLFWInitialized();
    
private:
    bool m_initialized = false;
    bool m_headless = true;
    GLFWwindow* m_window = nullptr;
    int m_framebuffer_width = 800;
    int m_framebuffer_height = 600;
    
    static bool s_glfw_initialized;
    static int s_instance_count;
    
    // GLFW 콜백 함수들
    static void ErrorCallback(int error, const char* description);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    
    // 내부 헬퍼 함수들
    bool InitializeGLFW();
    bool CreateWindow();
    bool InitializeGL();
};