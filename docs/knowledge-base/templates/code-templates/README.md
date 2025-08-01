# Code Templates

C_capture 프로젝트 개발을 위한 표준화된 코드 템플릿 모음

## 📁 템플릿 구성

### C++17 클래스 템플릿
- **interface-template.h** - 추상 인터페이스 정의 표준
- **concrete-class-template.h/.cpp** - 구체 클래스 구현 표준
- **singleton-template.h** - 싱글톤 패턴 구현
- **raii-wrapper-template.h** - RAII 래퍼 클래스

### 성능 최적화 템플릿
- **performance-timer.h** - 성능 측정 유틸리티
- **memory-pool-template.h** - 메모리 풀 구현
- **thread-safe-queue.h** - 스레드 안전 큐 구현
- **cuda-helper-template.h** - CUDA 메모리 관리 헬퍼

### 테스트 템플릿
- **unit-test-template.cpp** - GoogleTest 단위 테스트
- **mock-class-template.h** - Mock 객체 구현
- **performance-test-template.cpp** - 성능 벤치마크 테스트
- **integration-test-template.cpp** - 통합 테스트

## 🎯 사용 지침

### 템플릿 활용 방법
1. 해당 템플릿 파일을 복사
2. `Template` 키워드를 실제 클래스명으로 치환
3. TODO 주석을 참조하여 구체적 구현 추가
4. 프로젝트 스타일 가이드 준수 확인

### 명명 규칙
- **클래스**: PascalCase (예: ObjectDetector)
- **메소드**: camelCase (예: captureScreen)
- **변수**: snake_case (예: frame_count_)
- **상수**: UPPER_SNAKE_CASE (예: MAX_RETRY_COUNT)

## 🚀 빠른 시작

### 새로운 인터페이스 생성
```bash
# 템플릿 복사 및 이름 변경
cp interface-template.h ../../../ScreenMonitor/include/interfaces/INewInterface.h

# 템플릿 내용을 실제 인터페이스로 수정
sed -i 's/ITemplate/INewInterface/g' INewInterface.h
```

### 성능 측정 코드 추가
```cpp
#include "performance-timer.h"

void myFunction() {
    PerformanceTimer timer("myFunction");
    // 측정할 코드
}
```

## 📊 품질 기준

모든 템플릿은 다음 기준을 만족합니다:
- **C++17 표준 준수**
- **예외 안전성 보장**
- **메모리 누수 방지**
- **스레드 안전성 고려**
- **성능 최적화 적용**