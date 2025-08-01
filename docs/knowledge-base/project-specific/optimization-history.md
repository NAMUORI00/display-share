# Optimization History

C_capture 프로젝트의 성능 최적화 이력을 추적하고 개선 과정을 문서화합니다.

## 📊 성능 개선 타임라인

### Phase 1: 기본 구현 (2024년 1월 1주차)
**초기 성능 기준선 설정**

#### 초기 성능 측정
- **전체 파이프라인**: ~200ms (5 FPS)
- **화면 캡처**: ~50ms
- **YOLO 추론**: ~120ms (CPU)
- **GUI 렌더링**: ~30ms

#### 주요 병목 지점
1. 전체 화면 캡처 후 ROI 추출
2. CPU 기반 YOLO 추론
3. 비효율적인 메모리 할당/해제

---

### Phase 2: ROI 최적화 (2024년 1월 2주차)
**화면 캡처 성능 대폭 개선**

#### 최적화 내용
```cpp
// Before: 전체 화면 캡처 후 ROI 추출
cv::Mat fullScreen = captureFullScreen();  // ~50ms
cv::Mat roi = fullScreen(cv::Rect(x, y, 320, 320));

// After: 직접 ROI 캡처
cv::Mat roi = captureROI(x, y, 320, 320);  // ~5ms
```

#### 성능 개선 결과
- **화면 캡처**: 50ms → 5ms (10배 개선)
- **메모리 사용량**: ~12MB → ~1MB
- **전체 파이프라인**: 200ms → 155ms

#### 핵심 기법
- Screen Capture Lite 라이브러리의 영역별 캡처 활용
- 메모리 재사용을 통한 할당 오버헤드 제거
- 픽셀 포맷 변환 최적화 (BGRA → RGB)

---

### Phase 3: TensorRT 통합 (2024년 1월 3주차)
**YOLO 추론 속도 혁신적 개선**

#### TensorRT 엔진 최적화
```cpp
// 모델 양자화 설정
config->setFlag(nvinfer1::BuilderFlag::kFP16);
config->setFlag(nvinfer1::BuilderFlag::kSTRICT_TYPES);

// 최적 배치 크기 설정
profile->setDimensions("input", OptProfileSelector::kMIN, dims_min);
profile->setDimensions("input", OptProfileSelector::kOPT, dims_opt);
profile->setDimensions("input", OptProfileSelector::kMAX, dims_max);
```

#### 성능 개선 결과
- **YOLO 추론**: 120ms → 8ms (15배 개선)
- **GPU 메모리**: ~1.5GB 사용
- **전체 파이프라인**: 155ms → 43ms

#### 주요 최적화 기법
- FP16 정밀도로 모델 양자화 (정확도 손실 < 2%)
- 동적 형태(Dynamic Shapes) 대신 고정 형태 사용
- CUDA 스트림을 활용한 비동기 추론

---

### Phase 4: 메모리 최적화 (2024년 1월 4주차)
**메모리 효율성 및 안정성 개선**

#### 메모리 풀링 구현
```cpp
class MemoryPool {
    std::vector<cv::Mat> available_buffers_;
    std::mutex mutex_;
    
public:
    cv::Mat acquire(int rows, int cols, int type);
    void release(cv::Mat& buffer);
};
```

#### 성능 개선 결과
- **메모리 할당 시간**: ~2ms → ~0.1ms
- **메모리 단편화**: 95% 감소
- **안정성**: 장시간 실행 시 메모리 누수 제거

#### 핵심 개선사항
- 고정 크기 메모리 풀 구현
- RAII 패턴을 통한 자동 메모리 관리
- 스마트 포인터 활용으로 메모리 안전성 보장

---

### Phase 5: 멀티스레딩 최적화 (2024년 1월 5주차)
**파이프라인 병렬화를 통한 처리량 향상**

#### 생산자-소비자 패턴 구현
```cpp
// 캡처 스레드
void captureThread() {
    while (running_) {
        cv::Mat frame = capture_->captureROI();
        frame_queue_.push(std::move(frame));
    }
}

// 추론 스레드  
void inferenceThread() {
    while (running_) {
        cv::Mat frame = frame_queue_.pop();
        auto results = detector_->detect(frame);
        result_queue_.push(std::move(results));
    }
}
```

#### 성능 개선 결과
- **전체 파이프라인**: 43ms → 28ms
- **CPU 사용률**: 균등한 부하 분산
- **처리량**: 23 FPS → 35 FPS

#### 핵심 기법
- 잠금 없는 큐(Lock-free Queue) 구현
- 스레드 안전한 메모리 풀 적용
- 작업 부하 균등 분산

---

### Phase 6: GUI 렌더링 최적화 (현재 진행)

#### 현재 최적화 중인 영역
1. **ImGui 드로우 콜 최소화**
   - 텍스처 아틀라스 최적화
   - 불필요한 위젯 렌더링 제거
   
2. **실시간 이미지 업데이트**
   - GPU 텍스처 직접 업데이트
   - 더블 버퍼링을 통한 깜빡임 제거

#### 예상 개선 목표
- **GUI 렌더링**: 15ms → 8ms
- **전체 파이프라인**: 28ms → 20ms (50 FPS 달성)

---

## 📈 성능 메트릭 추적

### 버전별 성능 비교
| 버전 | 전체 파이프라인 | 화면 캡처 | YOLO 추론 | GUI 렌더링 | FPS |
|------|----------------|-----------|-----------|------------|-----|
| v0.1 | 200ms | 50ms | 120ms | 30ms | 5 |
| v0.2 | 155ms | 5ms | 120ms | 30ms | 6.5 |
| v0.3 | 43ms | 5ms | 8ms | 30ms | 23 |
| v0.4 | 43ms | 5ms | 8ms | 30ms | 23 |
| v0.5 | 28ms | 5ms | 8ms | 15ms | 35 |
| v0.6 | 20ms (목표) | 5ms | 8ms | 7ms | 50 |

### 메모리 사용량 추적  
| 버전 | 시스템 RAM | GPU 메모리 | 최대 메모리 | 메모리 증가율 |
|------|-----------|-----------|-------------|--------------|
| v0.1 | ~50MB | 0MB | 50MB | N/A |
| v0.2 | ~15MB | 0MB | 15MB | -70% |
| v0.3 | ~20MB | 1.5GB | 1.52GB | 시스템 +33% |
| v0.4 | ~18MB | 1.5GB | 1.518GB | -0.1% |
| v0.5 | ~22MB | 1.5GB | 1.522GB | +0.3% |

---

## 🎯 최적화 기법 라이브러리

### 검증된 최적화 패턴

#### 1. 메모리 관리 최적화
```cpp
// 메모리 풀 기반 버퍼 관리
template<typename T>
class ObjectPool {
    std::stack<std::unique_ptr<T>> available_;
    std::mutex mutex_;
    
public:
    std::unique_ptr<T> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!available_.empty()) {
            auto obj = std::move(available_.top());
            available_.pop();
            return obj;
        }
        return std::make_unique<T>();
    }
    
    void release(std::unique_ptr<T> obj) {
        std::lock_guard<std::mutex> lock(mutex_);
        available_.push(std::move(obj));
    }
};
```

#### 2. CUDA 메모리 최적화
```cpp
// 통합 메모리 사용으로 CPU-GPU 복사 최소화
void* unified_memory;
cudaMallocManaged(&unified_memory, size);
```

#### 3. 컴파일러 최적화 플래그
```cmake
# Release 모드 최적화 설정
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
```

### 성능 측정 도구 모음

#### 1. 마이크로 벤치마킹
```cpp
class Timer {
    std::chrono::high_resolution_clock::time_point start_;
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsed() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }
};
```

#### 2. 메모리 사용량 모니터링
```cpp
class MemoryMonitor {
    size_t peak_usage_ = 0;
public:
    void record_usage() {
        auto current = getCurrentMemoryUsage();
        peak_usage_ = std::max(peak_usage_, current);
    }
    
    size_t getPeakUsage() const { return peak_usage_; }
};
```

---

## 🔮 향후 최적화 계획

### 단기 목표 (1-2주)
1. **GPU 직접 렌더링**: OpenGL 텍스처 직접 업데이트
2. **배치 처리**: 여러 ROI 동시 처리
3. **캐시 최적화**: CPU 캐시 친화적 데이터 구조

### 중기 목표 (1-2개월)
1. **모델 최적화**: INT8 양자화 도입
2. **알고리즘 개선**: 더 효율적인 후처리 알고리즘
3. **하드웨어 활용**: AVX/SSE 명령어 활용

### 장기 목표 (3-6개월)  
1. **분산 처리**: 다중 GPU 활용
2. **적응적 최적화**: 실행 환경에 따른 자동 최적화
3. **예측 최적화**: 다음 프레임 예측을 통한 선제적 처리

---

**관련 문서**
- [아키텍처 결정사항](architecture-decisions.md)
- [성능 목표](../quick-reference/performance-targets/)
- [벤치마크 결과](../research/yolo-v11/performance-benchmarks.md)