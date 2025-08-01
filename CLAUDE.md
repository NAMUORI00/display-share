# CLAUDE.md

Claude Code (claude.ai/code) 개발자를 위한 C_capture 프로젝트 가이드

## 프로젝트 개요

**C_capture**는 전문적인 **화면 캡처 및 컴퓨터 비전 시스템**을 위한 통합 개발 환경으로, ScreenMonitor라는 고성능 C++17 애플리케이션을 중심으로 구성된 단일 저장소입니다.

### 핵심 특징
- **전문 C++ 시스템**: Windows GUI 기반 실시간 화면 캡처 및 컴퓨터 비전 처리
- **모듈식 아키텍처**: ImGui 기반 전문 인터페이스, OpenCV 컴퓨터 비전, TensorRT AI 추론
- **복합 의존성 관리**: 5개 Git submodule을 통한 체계적 라이브러리 관리
- **완전한 개발 생태계**: 테스트, 빌드, 문서화가 통합된 전문 개발 환경

## 프로젝트 구조

```
C_capture/
├── CLAUDE.md                    # 📍 이 파일 - 프로젝트 진입점
├── .gitmodules                  # Git submodule 구성
├── .gitignore                   # 빌드 결과물 및 임시 파일 제외
├── build/                       # CMake 빌드 결과물 (자동 생성)
│
└── ScreenMonitor/               # 🎯 메인 C++ 애플리케이션
    ├── CLAUDE.md               # ScreenMonitor 상세 가이드 (223줄)
    ├── src/CLAUDE.md           # 소스 코드 아키텍처 가이드
    ├── external/CLAUDE.md      # Git submodule 관리 가이드  
    ├── tests/CLAUDE.md         # 테스트 전략 가이드
    │
    ├── CMakeLists.txt          # 루트 빌드 설정
    ├── BUILD_GUIDE.md          # 빌드 상세 가이드
    ├── README.md               # 프로젝트 소개
    │
    ├── src/                    # 소스 코드 (모듈식 구조)
    │   ├── main.cpp            # WinMain 진입점
    │   ├── core/               # 핵심 로직 (ConfigManager)
    │   ├── capture/            # 화면 캡처 구현
    │   ├── detection/          # 컴퓨터 비전 알고리즘
    │   └── gui/                # ImGui 인터페이스
    │
    ├── include/                # 헤더 파일 (인터페이스 중심)
    │   ├── interfaces/         # 추상 인터페이스
    │   ├── core/               # 핵심 컴포넌트 헤더
    │   ├── capture/            # 캡처 시스템 헤더
    │   ├── detection/          # 비전 알고리즘 헤더
    │   └── gui/                # GUI 컴포넌트 헤더
    │
    ├── external/               # Git submodule 의존성
    │   ├── opencv/             # 컴퓨터 비전 라이브러리
    │   ├── imgui/              # GUI 프레임워크
    │   ├── googletest/         # 테스트 프레임워크
    │   ├── nlohmann_json/      # JSON 처리
    │   └── screen_capture_lite/ # 크로스플랫폼 캡처
    │
    ├── tests/                  # GoogleTest 기반 테스트
    │   ├── helpers/            # 테스트 유틸리티
    │   ├── mocks/              # Mock 객체
    │   └── test_*.cpp          # 단위 및 통합 테스트
    │
    ├── config/                 # JSON 설정 파일
    ├── models/                 # AI 모델 파일
    └── scripts/                # 개발 스크립트
```

## 빠른 시작 가이드

### 1. 저장소 복제 및 초기화
```bash
# 저장소 복제
git clone <repository-url> C_capture
cd C_capture

# 모든 Git submodule 초기화 (필수)
git submodule update --init --recursive

# Submodule 상태 확인
git submodule status
```

### 2. 개발 환경 요구사항
- **운영체제**: Windows 10/11 (64-bit)
- **빌드 도구**: Visual Studio 2019+ 또는 Build Tools
- **CMake**: 3.16+ (최신 버전 권장)
- **GPU**: CUDA 11.0+ (TensorRT 지원용, 선택사항)

### 3. 빌드 및 실행
```bash
# 프로젝트 구성 (Release 권장)
cmake -B build -S ScreenMonitor -DCMAKE_BUILD_TYPE=Release

# 병렬 빌드 실행
cmake --build build --config Release --parallel

# 메인 애플리케이션 실행
./build/bin/Release/SmartScreenCapture.exe

# 테스트 실행
ctest -C Release --test-dir build --verbose
```

## Git Submodule 관리

### 핵심 Submodule 정보
| 모듈 | 목적 | 특징 |
|------|------|------|
| **opencv** | 컴퓨터 비전 | core, imgproc, imgcodecs 모듈만 사용 |
| **imgui** | GUI 프레임워크 | docking 브랜치 필수 |
| **googletest** | 테스트 프레임워크 | gtest, gmock 포함 |
| **nlohmann_json** | JSON 처리 | 헤더 전용 라이브러리 |
| **screen_capture_lite** | 화면 캡처 | 크로스플랫폼 고성능 |

### Submodule 관리 명령어
```bash
# 모든 submodule을 최신 버전으로 업데이트
git submodule update --remote

# 특정 submodule 업데이트
git submodule update --remote ScreenMonitor/external/opencv

# Submodule 변경사항 커밋
git add .gitmodules ScreenMonitor/external/
git commit -m "Submodule 업데이트: opencv, imgui"

# Submodule 문제 해결 (초기화 재실행)
git submodule deinit --all
git submodule update --init --recursive
```

## 개발 워크플로우

### 일반적인 개발 사이클
```bash
# 1. 최신 코드 동기화
git pull origin main
git submodule update --recursive

# 2. 기능 브랜치 생성
git checkout -b feature/새로운-기능

# 3. 코드 수정 및 테스트
cmake --build build --config Release
ctest -C Release --test-dir build

# 4. 변경사항 커밋
git add .
git commit -m "feat: 새로운 기능 구현"

# 5. 브랜치 푸시 및 PR 생성
git push origin feature/새로운-기능
```

### 빌드 시 자주 발생하는 문제

#### 1. Submodule 누락 오류
```bash
# 증상: "external/opencv not found" 등의 오류
# 해결: Submodule 초기화 재실행
git submodule update --init --recursive
```

#### 2. ImGui DockBuilder 오류
```bash
# 증상: "DockBuilder API not found"
# 해결: imgui docking 브랜치 확인
cd ScreenMonitor/external/imgui
git checkout docking
```

#### 3. CMake 캐시 문제
```bash
# 증상: 설정 변경이 반영되지 않음
# 해결: 빌드 캐시 삭제 후 재구성
rm -rf build/
cmake -B build -S ScreenMonitor -DCMAKE_BUILD_TYPE=Release
```

## 프로젝트별 특화 가이드

각 주요 구성요소에 대한 상세한 가이드는 다음 CLAUDE.md 파일들을 참조하세요:

- **📖 [ScreenMonitor/CLAUDE.md](ScreenMonitor/CLAUDE.md)** - 메인 애플리케이션 완전 가이드 (223줄)
- **🔧 [ScreenMonitor/src/CLAUDE.md](ScreenMonitor/src/CLAUDE.md)** - 소스 코드 아키텍처 및 개발 가이드
- **📦 [ScreenMonitor/external/CLAUDE.md](ScreenMonitor/external/CLAUDE.md)** - Git submodule 관리 전문 가이드
- **🧪 [ScreenMonitor/tests/CLAUDE.md](ScreenMonitor/tests/CLAUDE.md)** - 테스트 전략 및 품질 보증 가이드

## SuperClaude Framework 통합

이 프로젝트는 **SuperClaude Framework v1.0**과 완전히 통합되어 한국어 최적화된 개발 경험을 제공합니다.

### 추천 Claude Code 명령어
```bash
# 프로젝트 전체 분석
/analyze --scope project --focus architecture

# 빌드 시스템 최적화  
/build --target release --validate

# 코드 품질 개선
/improve --focus quality --scope module

# 테스트 전략 수립
/test --type integration --coverage

# 성능 최적화 분석
/analyze --focus performance --think-hard
```

### 한국어 개발 지원
- **한국어 주석**: 모든 소스 코드에 한국어 주석 사용
- **한국 시간대**: 빌드 및 로그 시간이 KST 기준
- **문화적 맥락**: 한국 개발자 워크플로우에 최적화

## 프로젝트 상태 및 성숙도

- **🟢 Production Ready**: 안정적인 화면 캡처 및 GUI 시스템
- **🟢 완전한 문서화**: 모든 주요 구성요소에 대한 상세 가이드
- **🟢 테스트 커버리지**: GoogleTest 기반 포괄적 테스트
- **🟢 전문 아키텍처**: 모듈식 설계 및 인터페이스 분리
- **🟡 AI 기능**: YOLOv11 TensorRT 통합 (GPU 환경 의존적)

## 기여 및 협업

새로운 개발자를 위한 진입 경로:
1. **프로젝트 이해**: 이 CLAUDE.md → ScreenMonitor/CLAUDE.md 순서로 읽기
2. **환경 설정**: Git submodule 초기화 → CMake 빌드 → 테스트 실행
3. **코드 탐색**: src/CLAUDE.md를 통한 모듈별 학습
4. **기능 개발**: 인터페이스 기반 확장 및 테스트 작성

---

**C_capture 프로젝트** - 전문적인 화면 캡처 및 컴퓨터 비전 통합 시스템  
SuperClaude Framework v1.0 호환 | 한국어 최적화 개발 환경