# Testing Templates

C_capture 프로젝트의 체계적인 테스트 작성을 위한 표준 템플릿 모음

## 📁 템플릿 구성

### 단위 테스트 템플릿
- **unit-test-class-template.cpp** - 클래스 단위 테스트 표준
- **unit-test-function-template.cpp** - 함수 단위 테스트 표준
- **mock-object-template.h** - Mock 객체 구현 표준
- **test-fixture-template.h** - 테스트 픽스처 표준

### 통합 테스트 템플릿
- **integration-test-template.cpp** - 모듈 간 통합 테스트
- **pipeline-test-template.cpp** - 전체 파이프라인 테스트
- **performance-integration-template.cpp** - 성능 통합 테스트

### 성능 테스트 템플릿
- **benchmark-test-template.cpp** - 마이크로 벤치마크 테스트
- **stress-test-template.cpp** - 스트레스 테스트
- **memory-test-template.cpp** - 메모리 누수 테스트
- **concurrency-test-template.cpp** - 동시성 테스트

## 🎯 테스트 작성 원칙

### 테스트 품질 기준
- **독립성**: 각 테스트는 다른 테스트에 의존하지 않음
- **반복성**: 동일한 결과를 보장하는 결정적 테스트
- **명확성**: 테스트 목적과 검증 내용이 명확
- **성능**: 빠른 실행 속도와 효율적 자원 사용

### 네이밍 컨벤션
```cpp
// 테스트 클래스: {TargetClass}Test
class ObjectDetectorTest : public ::testing::Test { ... };

// 테스트 메소드: {Method}_{Scenario}_{ExpectedBehavior}
TEST_F(ObjectDetectorTest, Detect_ValidImage_ReturnsDetections) { ... }
TEST_F(ObjectDetectorTest, Detect_EmptyImage_ReturnsEmptyResult) { ... }
TEST_F(ObjectDetectorTest, Detect_LargeImage_CompletesWithinTimeLimit) { ... }
```

## 🚀 템플릿 활용 가이드

### 새 테스트 클래스 생성
```bash
# 템플릿 복사
cp unit-test-class-template.cpp ../../../ScreenMonitor/tests/test_NewClass.cpp

# 클래스명 치환
sed -i 's/Template/NewClass/g' test_NewClass.cpp
```

### 테스트 데이터 준비
```cpp
// 테스트용 이미지 생성
cv::Mat createTestImage(int width = 320, int height = 320) {
    cv::Mat image(height, width, CV_8UC3);
    cv::randu(image, cv::Scalar::all(0), cv::Scalar::all(255));
    return image;
}
```

## 📊 테스트 커버리지 목표

### 커버리지 기준
- **단위 테스트**: 90% 이상 라인 커버리지
- **통합 테스트**: 주요 시나리오 100% 커버
- **성능 테스트**: 모든 성능 크리티컬 경로

### 필수 테스트 시나리오
- **정상 케이스**: 예상된 입력에 대한 정상 동작
- **경계 케이스**: 최소/최대 값 처리
- **예외 케이스**: 잘못된 입력에 대한 적절한 처리
- **성능 케이스**: 응답 시간 및 자원 사용량 검증