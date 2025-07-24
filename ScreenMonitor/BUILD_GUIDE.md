# 🚀 Single EXE Build Guide

**Professional Screen Capture & Computer Vision System** - 의존성 없는 단일 실행파일 빌드 가이드

## 📋 개요

이 가이드는 모든 종속성이 정적으로 링크된 단일 실행파일을 생성하는 방법을 설명합니다. 생성된 실행파일은 별도의 DLL이나 라이브러리 설치 없이 독립적으로 실행됩니다.

## 🎯 빌드 목표

- ✅ **단일 EXE 파일**: 모든 종속성 내장
- ✅ **의존성 제로**: 외부 DLL 불필요
- ✅ **이식성**: 다른 시스템으로 복사만으로 실행
- ✅ **프로덕션 준비**: 엔터프라이즈 배포 가능

## 🛠️ 사전 요구사항

### Windows
- **Visual Studio 2022** (Community 이상)
- **CMake 3.20+**
- **Git** (서브모듈용)
- **vcpkg** (선택사항 - GLFW용)

### Linux/macOS
- **GCC 7+** 또는 **Clang 6+**
- **CMake 3.20+**
- **Git** (서브모듈용)
- **OpenGL 개발 라이브러리**

## 🚀 빌드 프로세스

### 방법 1: 자동 빌드 스크립트 (권장)

#### Windows
```batch
# ScreenMonitor 디렉토리에서 실행
build-static.bat
```

#### Linux/macOS
```bash
# ScreenMonitor 디렉토리에서 실행
./build-static.sh
```

### 방법 2: 수동 빌드

#### 1. 서브모듈 초기화
```bash
git submodule update --init --recursive
```

#### 2. 빌드 디렉토리 생성
```bash
mkdir build-static
cd build-static
```

#### 3. CMake 구성 (Windows)
```batch
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded" ^
    ..
```

#### 4. CMake 구성 (Linux/macOS)
```bash
cmake -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -static-libgcc -static-libstdc++" \
    ..
```

#### 5. 빌드 실행
```bash
cmake --build . --config Release --parallel
```

## 📦 정적 링킹 설정

### CMake 설정 주요 포인트

```cmake
# 전역 정적 라이브러리 빌드 강제
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)

# MSVC 런타임 정적 링킹 (Windows)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 정적 링킹 검색 우선순위
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_SEARCH_START_STATIC ON
    LINK_SEARCH_END_STATIC ON
)
```

### 주요 종속성 정적 링킹

1. **OpenCV**: 핵심 모듈만 정적 빌드
   - `opencv_core`, `opencv_imgproc`, `opencv_imgcodecs`
   
2. **ImGui**: 완전 정적 링킹
   - GLFW + OpenGL3 백엔드 포함
   
3. **screen_capture_lite**: 정적 라이브러리로 빌드

4. **nlohmann/json**: 헤더 전용 라이브러리

## 🔍 의존성 검증

### Windows - DLL 종속성 확인
```batch
# Visual Studio Developer Command Prompt에서
dumpbin /dependents SmartScreenCapture.exe
```

### Linux - 공유 라이브러리 확인
```bash
ldd SmartScreenCapture
```

### macOS - 동적 라이브러리 확인
```bash
otool -L SmartScreenCapture
```

## 📁 결과물 구조

빌드 완료 후 생성되는 파일 구조:

```
build-static/
├── bin/
│   └── SmartScreenCapture.exe    # 메인 실행파일
└── deploy/                       # 배포 패키지
    ├── SmartScreenCapture.exe    # 실행파일
    ├── config/
    │   └── config.json           # 설정 파일
    ├── models/                   # AI 모델 (선택사항)
    └── README.txt               # 배포 가이드
```

## ⚡ 성능 최적화

### 컴파일러 최적화 플래그

#### Windows (MSVC)
```cmake
set(CMAKE_CXX_FLAGS_RELEASE "/MT /O2 /DNDEBUG")
```

#### Linux/macOS (GCC/Clang)
```cmake
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -static-libgcc -static-libstdc++")
```

### 크기 최적화 (선택사항)

링크 타임 최적화 활성화:
```cmake
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
```

## 🐛 일반적인 문제 해결

### 1. "DLL not found" 오류
**원인**: 정적 링킹이 완전하지 않음  
**해결**: CMake 설정에서 `BUILD_SHARED_LIBS=OFF` 확인

### 2. 런타임 라이브러리 오류 (Windows)
**원인**: MSVC 런타임 불일치  
**해결**: `CMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded"` 설정

### 3. OpenGL 관련 오류
**원인**: OpenGL 드라이버 또는 라이브러리 누락  
**해결**: 최신 그래픽 드라이버 설치

### 4. 대용량 실행파일
**원인**: 디버그 정보 포함  
**해결**: Release 모드 빌드 확인

## 📊 빌드 결과 예상 크기

| 구성 요소 | 예상 크기 |
|-----------|-----------|
| 기본 실행파일 | ~15-25 MB |
| OpenCV 포함 | +10-15 MB |
| ImGui 포함 | +2-3 MB |
| **총 크기** | **~30-45 MB** |

## 🎯 배포 가이드

### 1. 단순 배포
생성된 `SmartScreenCapture.exe`만 복사하여 배포 가능

### 2. 완전 배포 패키지
`build-static/deploy/` 폴더 전체를 압축하여 배포

### 3. 설치 관리자 (선택사항)
NSIS 또는 Inno Setup을 사용하여 설치 관리자 생성

## ✅ 테스트 검증

### 기본 실행 테스트
```batch
SmartScreenCapture.exe --help
```

### 의존성 독립성 테스트
1. 다른 시스템으로 실행파일 복사
2. 라이브러리 설치 없이 실행
3. 정상 작동 확인

## 🔧 고급 설정

### 추가 최적화 옵션

#### 링크 타임 코드 생성 (LTCG)
```cmake
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
```

#### 불필요한 기호 제거
```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
endif()
```

## 📞 지원 및 문제 해결

빌드 관련 문제가 발생하면:

1. **로그 확인**: 빌드 스크립트의 상세 출력 검토
2. **환경 검증**: 사전 요구사항 재확인
3. **클린 빌드**: 빌드 디렉토리 삭제 후 재빌드
4. **이슈 리포트**: GitHub Issues에 문제 상황 보고

---

**Professional Screen Capture & Computer Vision System** - 엔터프라이즈급 프로덕션 준비 완료 🚀