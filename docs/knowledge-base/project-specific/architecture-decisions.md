# Architecture Decisions Record (ADR)

C_capture 프로젝트의 주요 아키텍처 결정사항과 그 근거를 기록합니다.

## ADR-001: 모듈식 아키텍처 채택

### 결정사항
인터페이스 기반의 모듈식 아키텍처를 채택하여 각 컴포넌트를 독립적으로 개발하고 테스트할 수 있도록 설계

### 근거
- **확장성**: 새로운 검출 알고리즘이나 캡처 방식을 쉽게 추가 가능
- **테스트 용이성**: Mock 객체를 통한 독립적인 단위 테스트 가능
- **유지보수성**: 각 모듈의 책임이 명확하여 수정 영향 범위 최소화
- **성능**: 컴파일 타임 다형성을 통한 런타임 오버헤드 최소화

### 구현 방식
```cpp
// 인터페이스 정의
class IScreenCapture {
public:
    virtual ~IScreenCapture() = default;
    virtual bool captureScreen(cv::Mat& output) = 0;
};

// 구체 구현
class ScreenCaptureLiteDevice : public IScreenCapture {
    bool captureScreen(cv::Mat& output) override;
};
```

### 상태
✅ **Accepted** - 2024년 1월 구현 완료

---

## ADR-002: 의존성 주입 패턴 적용

### 결정사항
생성자 주입을 통한 의존성 주입 패턴을 적용하여 컴포넌트 간 결합도를 낮춤

### 근거
- **테스트 용이성**: Mock 객체 주입을 통한 독립적 테스트
- **유연성**: 런타임에 구현체를 변경할 수 있는 구조
- **SOLID 원칙**: 의존성 역전 원칙(DIP) 준수

### 구현 방식
```cpp
class ObjectDetector {
private:
    std::unique_ptr<IYOLOInference> inference_;
    
public:
    ObjectDetector(std::unique_ptr<IYOLOInference> inference)
        : inference_(std::move(inference)) {}
};
```

### 상태
✅ **Accepted** - 모든 주요 컴포넌트에 적용

---

## ADR-003: Git Submodule 기반 의존성 관리

### 결정사항
외부 라이브러리를 Git submodule로 관리하여 버전 일관성과 빌드 재현성 보장

### 근거
- **버전 고정**: 특정 커밋으로 고정하여 빌드 재현성 보장
- **오프라인 빌드**: 인터넷 연결 없이도 빌드 가능
- **수정 추적**: 외부 라이브러리 수정사항을 Git으로 추적 가능
- **팀 협업**: 모든 개발자가 동일한 라이브러리 버전 사용

### 채택된 라이브러리
- **OpenCV**: 컴퓨터 비전 처리
- **ImGui**: GUI 프레임워크 (docking 브랜치)
- **GoogleTest**: 테스트 프레임워크
- **nlohmann/json**: JSON 파싱
- **screen_capture_lite**: 크로스플랫폼 화면 캡처

### 상태
✅ **Accepted** - 5개 주요 의존성 submodule로 관리

---

## ADR-004: 320x320 고정 ROI 처리

### 결정사항
YOLO 입력을 위해 화면의 특정 영역(320x320)만 추출하여 처리하는 ROI 기반 접근 방식 채택

### 근거
- **성능 최적화**: 전체 화면 대신 작은 영역만 처리하여 성능 향상
- **YOLO 최적화**: 320x320은 YOLO v11의 최적 입력 크기
- **실시간 처리**: 작은 영역 처리로 실시간 추론 가능
- **메모리 효율**: 메모리 사용량 최소화

### 구현 세부사항
```cpp
// ROI 추출 최적화
cv::Rect roi(x, y, 320, 320);
cv::Mat roiImage = fullScreen(roi).clone();
```

### 성능 목표
- ROI 추출: < 2ms
- 전체 파이프라인: < 33ms (30 FPS)

### 상태
✅ **Accepted** - 성능 목표 달성

---

## ADR-005: ImGui 도킹 브랜치 사용

### 결정사항
ImGui의 공식 도킹 브랜치를 사용하여 전문적인 GUI 레이아웃 구현

### 근거
- **전문 인터페이스**: 도킹 기능으로 IDE 스타일의 전문적 UI 제공
- **사용자 경험**: 사용자가 레이아웃을 자유롭게 조정 가능
- **개발 효율성**: 복잡한 레이아웃을 쉽게 구현
- **향후 호환성**: 도킹 기능이 메인 브랜치에 병합될 예정

### 기술적 고려사항
- 도킹 브랜치는 실험적 기능이지만 안정성 검증 완료
- 성능상 오버헤드 최소 (< 1ms per frame)

### 상태
✅ **Accepted** - 안정적 운영 중

---

## ADR-006: TensorRT 선택적 지원

### 결정사항
CUDA/TensorRT 지원을 선택적으로 구현하여 GPU가 없는 환경에서도 동작 가능하도록 설계

### 근거
- **호환성**: GPU가 없는 개발 환경에서도 빌드/테스트 가능
- **성능 확장성**: GPU가 있는 환경에서는 고성능 추론 제공
- **유연성**: 런타임에 CPU/GPU 추론 엔진 선택 가능

### 구현 방식
```cpp
#ifdef TENSORRT_AVAILABLE
    return std::make_unique<YOLOv11TensorRTInference>();
#else
    return std::make_unique<YOLOv11CPUInference>();
#endif
```

### 성능 비교
- **CPU 추론**: ~100ms (개발/테스트용)
- **TensorRT 추론**: ~8ms (프로덕션용)

### 상태
✅ **Accepted** - 양쪽 환경 모두 지원

---

## 결정 과정 가이드라인

### 새로운 ADR 작성 시 고려사항
1. **성능 영향**: 실시간 처리 목표(30+ FPS)에 미치는 영향
2. **메모리 효율성**: 메모리 사용량 증가 여부
3. **테스트 가능성**: 단위 테스트 작성 용이성
4. **유지보수성**: 장기간 유지보수 관점에서의 복잡도
5. **팀 개발**: 여러 개발자가 협업하기 쉬운 구조인지

### 결정 승인 프로세스
1. **제안**: 아키텍처 변경 제안서 작성
2. **검토**: 성능 테스트 및 코드 리뷰
3. **승인**: 팀 합의 후 ADR 문서화
4. **구현**: 단계적 구현 및 테스트
5. **검증**: 성능 목표 달성 여부 확인

---

**관련 문서**
- [최적화 이력](optimization-history.md)
- [테스트 전략](testing-strategies.md)
- [배포 노하우](deployment-notes.md)