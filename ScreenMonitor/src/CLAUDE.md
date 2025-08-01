# CLAUDE.md - 소스 코드 아키텍처 가이드

ScreenMonitor 소스 코드 구조 및 개발 가이드

## 소스 코드 아키텍처 개요

ScreenMonitor는 **모듈식 C++17 아키텍처**를 기반으로 한 전문적인 화면 캡처 및 컴퓨터 비전 시스템입니다. 각 모듈은 명확한 책임 분리와 인터페이스 기반 설계를 통해 높은 확장성과 유지보수성을 제공합니다.

### 핵심 설계 원칙
- **인터페이스 분리**: 추상 인터페이스를 통한 느슨한 결합
- **팩토리 패턴**: 런타임 알고리즘 선택 및 의존성 주입
- **이벤트 기반**: ImGui 즉시 모드 GUI와 성능 관찰자 패턴
- **한국어 주석**: 코드 가독성 및 유지보수를 위한 한국어 주석 표준

## 모듈별 아키텍처

### 📁 src/ 디렉토리 구조
```
src/
├── main.cpp                     # 🚀 WinMain 진입점
├── core/                        # 🔧 핵심 시스템 로직
│   └── ConfigManager.cpp        # JSON 설정 관리 및 스키마 검증
├── capture/                     # 📹 화면 캡처 구현체
│   └── ScreenCaptureLiteDevice.cpp # screen_capture_lite 래퍼
├── detection/                   # 🔍 컴퓨터 비전 알고리즘
│   ├── ColorDetector.cpp        # 기본 색상 감지
│   ├── HSVColorDetection.cpp    # HSV 기반 색상 추적
│   ├── ObjectDetector.cpp       # 템플릿 매칭
│   └── YOLOv11TensorRTInference.cpp # YOLO v11 객체 검출
├── factories/                   # 🏭 객체 생성 및 의존성 주입
│   └── ComponentFactory.cpp     # 알고리즘 및 캡처 장치 팩토리
├── gui/                         # 🖥️ ImGui 기반 사용자 인터페이스
│   └── MainInterface.cpp        # 메인 GUI 및 도킹 시스템
└── monitoring/                  # 📊 성능 모니터링 (구현 예정)
```

## 핵심 모듈 상세 분석

### 🚀 main.cpp - 애플리케이션 진입점

**역할**: Windows GUI 애플리케이션 초기화 및 메인 루프 관리

```cpp
// WinMain 기반 Windows GUI 애플리케이션
// ImGui 컨텍스트 초기화 및 MainInterface 생성
// 이벤트 루프 및 리소스 정리
```

**핵심 책임**:
- Windows 애플리케이션 초기화 (WinMain 진입점)
- ImGui 컨텍스트 및 OpenGL 렌더링 설정
- MainInterface 인스턴스 생성 및 실행
- 애플리케이션 종료 시 리소스 정리

**개발 가이드**:
- main.cpp는 최소한의 초기화 로직만 포함
- 모든 GUI 로직은 MainInterface로 위임
- 예외 처리 및 안전한 종료 보장

### 🔧 core/ - 핵심 시스템 로직

#### ConfigManager.cpp
**역할**: JSON 기반 설정 관리 및 런타임 설정 변경 지원

```cpp
// JSON 스키마 검증 및 타입 안전성 보장
// 실시간 설정 변경 및 GUI 반영
// 설정 섹션: production_system, vision_algorithms, analytics, gui, performance
```

**주요 기능**:
- **JSON 스키마 검증**: nlohmann_json을 통한 타입 안전성
- **실시간 설정 변경**: GUI에서 변경된 설정 즉시 반영
- **다계층 설정 구조**: 모듈별 설정 그룹화 및 관리
- **기본값 처리**: 누락된 설정에 대한 안전한 기본값 제공

**개발 패턴**:
```cpp
// 새로운 설정 추가 시
1. config/config.json에 기본값 추가
2. ConfigManager에 getter/setter 메서드 추가
3. MainInterface에 GUI 컨트롤 추가
4. 설정 변경 시 즉시 반영 로직 구현
```

### 📹 capture/ - 화면 캡처 구현체

#### ScreenCaptureLiteDevice.cpp
**역할**: screen_capture_lite 라이브러리 래퍼 및 캡처 인터페이스 구현

```cpp
// ICaptureDevice 인터페이스 구현
// 크로스플랫폼 화면 캡처 기능
// 실시간 프레임 버퍼 관리 및 성능 최적화
```

**핵심 기능**:
- **멀티 모니터 지원**: 다중 디스플레이 환경에서 선택적 캡처
- **고성능 캡처**: 하드웨어 가속 및 메모리 최적화
- **프레임 버퍼 관리**: 효율적인 메모리 사용 및 캐싱
- **에러 핸들링**: 캡처 실패 시 안정적인 복구

**새로운 캡처 장치 추가**:
```cpp
1. ICaptureDevice 인터페이스 구현
2. ComponentFactory에 생성 로직 추가
3. config.json에 장치 설정 추가
4. MainInterface에 선택 UI 추가
```

### 🔍 detection/ - 컴퓨터 비전 알고리즘

모든 검출 알고리즘은 `IDetectionAlgorithm` 인터페이스를 구현하여 일관된 API 제공

#### ColorDetector.cpp
**역할**: 기본 색상 기반 객체 감지

```cpp
// RGB 색상 공간에서 색상 범위 매칭
// 단순하고 빠른 실시간 검출
// 조명 변화에 민감한 기본 알고리즘
```

#### HSVColorDetection.cpp  
**역할**: HSV 색상 공간 기반 고급 색상 추적

```cpp
// HSV 색상 공간에서 더 안정적인 색상 검출
// 조명 변화에 강인한 알고리즘
// 색조(H), 채도(S), 밝기(V) 독립적 조정
```

#### ObjectDetector.cpp
**역할**: 템플릿 매칭 기반 객체 검출

```cpp
// OpenCV 템플릿 매칭 알고리즘 활용
// 정적 객체 검출에 최적화
// 다양한 매칭 메서드 지원 (SQDIFF, CCORR, CCOEFF)
```

#### YOLOv11TensorRTInference.cpp
**역할**: 딥러닝 기반 고급 객체 검출

```cpp
// YOLOv11 모델 + TensorRT 가속
// 실시간 다중 객체 검출 및 분류
// GPU 가속을 통한 고성능 추론
```

**새로운 검출 알고리즘 추가**:
```cpp
1. IDetectionAlgorithm 인터페이스 구현
   - detect() 메서드: 검출 수행
   - configure() 메서드: 파라미터 설정
   - getResults() 메서드: 결과 반환

2. ComponentFactory에 알고리즘 등록
   - createDetectionAlgorithm() 메서드에 case 추가

3. config.json에 알고리즘 설정 추가
   - vision_algorithms 섹션에 새 알고리즘 파라미터

4. MainInterface에 GUI 컨트롤 추가
   - Detection Settings 패널에 설정 UI
```

### 🏭 factories/ - 객체 생성 및 의존성 주입

#### ComponentFactory.cpp
**역할**: 설정 기반 컴포넌트 생성 및 의존성 관리

```cpp
// 팩토리 패턴 구현
// 런타임 알고리즘 선택
// 의존성 주입 및 객체 생명주기 관리
```

**핵심 기능**:
- **런타임 알고리즘 선택**: 설정에 따른 동적 객체 생성
- **의존성 주입**: 인터페이스 기반 느슨한 결합
- **객체 생명주기 관리**: 스마트 포인터 활용한 메모리 관리
- **확장성**: 새로운 알고리즘 추가 시 최소 코드 변경

### 🖥️ gui/ - ImGui 기반 사용자 인터페이스

#### MainInterface.cpp
**역할**: 전문적인 5패널 도킹 인터페이스 및 통합 콘솔

```cpp
// ImGui 도킹 시스템 활용
// 5패널 전문 레이아웃: Capture Settings, Detection Settings, 
// Screen Preview, Performance Monitor, Console
// cout/cerr 스트림 리다이렉션을 통한 통합 콘솔
```

**GUI 아키텍처**:
```
┌─────────────────────────────────────────────────────────────┐
│                      메인 메뉴 바                           │
├──────────────┬──────────────────────────┬──────────────────┤
│   Capture    │                          │   Performance    │
│   Settings   │     Screen Preview       │   Monitor        │
│              │    (실시간 비디오)       │  (FPS, Metrics)  │
├──────────────┼──────────────────────────┼──────────────────┤
│  Detection   │                          │                  │
│  Settings    │                          │                  │
├──────────────┴──────────────────────────────────────────────┤
│                    Console Output                           │
│                 (통합 로그 및 메시지)                       │
└─────────────────────────────────────────────────────────────┘
```

**핵심 기능**:
- **도킹 시스템**: imgui_internal.h의 DockBuilder API 활용
- **스트림 리다이렉션**: GuiStreamBuf 클래스로 cout/cerr 캡처
- **실시간 업데이트**: 성능 지표, 검출 결과 실시간 표시
- **설정 통합**: 모든 설정을 GUI에서 실시간 변경 가능

**GUI 확장 가이드**:
```cpp
1. 새로운 패널 추가:
   - Render() 메서드에 ImGui::Begin()/End() 블록 추가
   - 도킹 레이아웃에 DockBuilderDockWindow() 호출 추가

2. 새로운 설정 컨트롤 추가:
   - 해당 패널에 ImGui 컨트롤 위젯 추가
   - ConfigManager를 통한 설정 읽기/쓰기 연결

3. 실시간 데이터 표시:
   - 성능 관찰자 패턴 활용
   - 매 프레임 업데이트되는 데이터 소스 연결
```

## 모듈 간 상호작용 패턴

### 데이터 흐름 아키텍처
```
[main.cpp] 
    ↓ 초기화
[MainInterface] 
    ↓ 설정 로드
[ConfigManager] 
    ↓ 컴포넌트 생성
[ComponentFactory] 
    ↓ 객체 생성
[ICaptureDevice] ← [IDetectionAlgorithm]
    ↓ 결과
[MainInterface] 
    ↓ GUI 표시
[사용자]
```

### 이벤트 처리 패턴
1. **GUI 이벤트** → MainInterface → ConfigManager → 설정 업데이트
2. **캡처 이벤트** → ICaptureDevice → 프레임 데이터 → IDetectionAlgorithm
3. **검출 결과** → MainInterface → GUI 업데이트 → 사용자 표시

## 개발 가이드라인

### 코딩 컨벤션

#### 한국어 주석 표준
```cpp
// 🟢 권장: 명확한 한국어 주석
class ScreenCaptureLiteDevice : public ICaptureDevice {
private:
    std::unique_ptr<sc_lite::Monitor> monitor_;  // 현재 선택된 모니터
    std::vector<uint8_t> frame_buffer_;          // 프레임 데이터 버퍼
    
public:
    // 화면 캡처를 시작하고 성공 여부를 반환
    bool StartCapture() override;
    
    // 최신 프레임을 가져와서 버퍼에 저장
    void CaptureFrame() override;
};

// ❌ 지양: 불명확하거나 영어 혼재
// Get frame -> 프레임 가져오기 (혼재)
// 프레임버퍼 (띄어쓰기 누락)
```

#### C++17 모던 패턴
```cpp
// 🟢 스마트 포인터 활용
std::unique_ptr<IDetectionAlgorithm> algorithm = 
    ComponentFactory::CreateDetectionAlgorithm(config);

// 🟢 auto 키워드 적극 활용 (타입이 명확한 경우)
auto config = ConfigManager::GetInstance().GetVisionConfig();

// 🟢 범위 기반 for 루프
for (const auto& result : detection_results) {
    ProcessDetectionResult(result);
}

// 🟢 이니셜라이저 리스트
std::vector<cv::Point> points{
    {100, 200}, {150, 250}, {200, 300}
};
```

### 새로운 기능 추가 워크플로우

#### 1. 새로운 검출 알고리즘 추가
```cpp
// Step 1: 인터페이스 구현
class NewDetectionAlgorithm : public IDetectionAlgorithm {
public:
    // 검출 수행 - 필수 구현
    std::vector<DetectionResult> detect(const cv::Mat& frame) override;
    
    // 알고리즘 설정 - 필수 구현  
    void configure(const nlohmann::json& config) override;
    
    // 성능 지표 반환 - 선택적 구현
    PerformanceMetrics getMetrics() const override;
};

// Step 2: 팩토리에 등록
// ComponentFactory.cpp의 createDetectionAlgorithm()에 추가
case AlgorithmType::NEW_ALGORITHM:
    return std::make_unique<NewDetectionAlgorithm>();

// Step 3: 설정 스키마 추가
// config/config.json에 알고리즘 설정 추가
"vision_algorithms": {
    "new_algorithm": {
        "enabled": true,
        "parameter1": 0.5,
        "parameter2": 100
    }
}

// Step 4: GUI 컨트롤 추가
// MainInterface.cpp의 Detection Settings 패널에 UI 추가
if (ImGui::CollapsingHeader("New Algorithm Settings")) {
    ImGui::SliderFloat("Parameter 1", &param1, 0.0f, 1.0f);
    ImGui::SliderInt("Parameter 2", &param2, 0, 200);
}
```

#### 2. 새로운 캡처 장치 추가
```cpp
// Step 1: 인터페이스 구현
class NewCaptureDevice : public ICaptureDevice {
public:
    bool initialize() override;
    void startCapture() override;
    cv::Mat getLatestFrame() override;
    void cleanup() override;
};

// Step 2: 팩토리에 등록 (ComponentFactory.cpp)
case CaptureDeviceType::NEW_DEVICE:
    return std::make_unique<NewCaptureDevice>();

// Step 3: GUI에서 장치 선택 옵션 추가
// MainInterface.cpp의 Capture Settings에 선택 UI 추가
```

### 성능 최적화 가이드

#### 메모리 관리
```cpp
// 🟢 프레임 버퍼 재사용
class FrameBufferPool {
private:
    std::queue<std::vector<uint8_t>> available_buffers_;
    std::mutex buffer_mutex_;
    
public:
    std::vector<uint8_t> getBuffer(size_t size);
    void returnBuffer(std::vector<uint8_t>&& buffer);
};

// 🟢 OpenCV Mat 메모리 최적화
cv::Mat frame(height, width, CV_8UC3, buffer.data()); // 복사 없이 래핑
```

#### 멀티스레딩 패턴
```cpp
// 🟢 캡처와 처리 분리
std::thread capture_thread(&CaptureWorker::run, this);
std::thread detection_thread(&DetectionWorker::run, this);

// 🟢 스레드 안전한 큐 사용
thread_safe_queue<cv::Mat> frame_queue_;
```

### 디버깅 가이드

#### 로깅 시스템 활용
```cpp
// ImGui 콘솔에 출력되는 로그 활용
std::cout << "[캡처] 프레임 캡처 시작: " << width << "x" << height << std::endl;
std::cerr << "[오류] 캡처 장치 초기화 실패: " << error_msg << std::endl;

// 성능 측정
auto start = std::chrono::high_resolution_clock::now();
// ... 작업 수행 ...
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
std::cout << "[성능] 검출 시간: " << duration.count() << "ms" << std::endl;
```

#### Visual Studio 디버깅
```cpp
// 디버그 빌드에서 유용한 assert 활용
#ifdef _DEBUG
    assert(frame.cols > 0 && frame.rows > 0);
    assert(detection_results.size() <= MAX_DETECTIONS);
#endif

// 조건부 컴파일을 통한 디버그 출력
#ifdef DEBUG_VERBOSE
    std::cout << "[디버그] 검출된 객체 수: " << results.size() << std::endl;
#endif
```

## 아키텍처 확장 포인트

### 플러그인 아키텍처로 확장
```cpp
// 향후 플러그인 시스템을 위한 인터페이스 설계
class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual std::string getName() const = 0;
    virtual Version getVersion() const = 0;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
};

// 동적 로딩을 위한 팩토리 함수
typedef std::unique_ptr<IPlugin> (*CreatePluginFunc)();
```

### 분산 처리 지원
```cpp
// 원격 처리 노드와의 통신을 위한 인터페이스
class IRemoteProcessor {
public:
    virtual void sendFrame(const cv::Mat& frame) = 0;
    virtual std::vector<DetectionResult> getResults() = 0;
    virtual bool isConnected() const = 0;
};
```

---

**ScreenMonitor 소스 코드 아키텍처** - 모듈식 C++17 설계  
한국어 주석 표준 | 인터페이스 기반 확장성 | 전문적인 GUI 통합