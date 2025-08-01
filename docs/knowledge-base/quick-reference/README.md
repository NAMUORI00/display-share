# Quick Reference Materials

즉시 사용 가능한 실용적 참조 자료 모음

## 📁 디렉토리 구성

### 📚 API 치트시트 (`/api-cheatsheets/`)
각 기술 스택별 핵심 API 빠른 참조
- OpenCV HSV API 핵심 함수들
- YOLO 추론 API 표준 호출 패턴
- TensorRT 엔진 API 필수 메소드
- ImGui 도킹 API 핵심 함수

### 💻 코드 스니펫 (`/code-snippets/`)
즉시 적용 가능한 최적화된 코드 조각
- HSV 색상 검출 최적화 코드
- YOLO TensorRT 추론 고성능 구현
- 320x320 ROI 추출 최적화 코드
- ImGui 도킹 레이아웃 설정

### 🔧 문제 해결 (`/troubleshooting/`)
자주 발생하는 문제의 해결책
- TensorRT 엔진 생성 실패 대응
- OpenCV 성능 저하 문제 해결
- 화면 캡처 안정성 문제 대응
- 빌드 시스템 의존성 충돌 해결

### 🎯 성능 목표 (`/performance-targets/`)
프로젝트 성능 기준점 및 벤치마크
- 실시간 처리 목표 (30+ FPS)
- 메모리 사용량 상한선
- 추론 속도 기준점
- UI 반응성 목표

## 🚀 사용 가이드

### 빠른 검색 패턴
```bash
# API 참조 검색
Grep pattern: "cv::inRange" path: "/docs/knowledge-base/quick-reference/"

# 코드 스니펫 찾기  
Glob pattern: "**/code-snippets/*.cpp"

# 문제 해결책 검색
Grep pattern: "error.*tensorrt" path: "**/troubleshooting/"
```

### 성능 목표 확인
```bash
# 특정 성능 메트릭 조회
Grep pattern: "fps.*target" path: "**/performance-targets/"

# 메모리 사용량 기준 확인
Grep pattern: "memory.*limit" path: "**/performance-targets/"
```

## ⚡ 우선순위 자료

### 최우선 참조 (Grade A)
1. **opencv-hsv-api.md** - HSV 처리 핵심 API
2. **yolo-tensorrt-snippet.cpp** - YOLO 추론 최적화 코드
3. **real-time-targets.md** - 실시간 처리 성능 목표

### 자주 참조 (Grade B)  
1. **tensorrt-troubleshooting.md** - TensorRT 문제 해결
2. **imgui-docking-setup.cpp** - 도킹 시스템 설정
3. **roi-extraction-snippet.cpp** - ROI 추출 최적화