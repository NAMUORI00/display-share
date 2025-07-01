# Windows 빌드 가이드 - 완전한 GUI 활성화

이 문서는 **완전히 활성화된 GUI**와 **서브모듈 기반 의존성 관리**, **정적 링킹**을 사용하여 Windows에서 DLL 없는 단일 실행 파일을 빌드하는 완전한 가이드입니다.

## 🎯 빌드 목표

- **완전한 GUI 기능**: ImGui + GLFW + OpenGL3으로 완전 활성화된 교육용 인터페이스
- **실시간 화면 캡처**: screen_capture_lite 통합으로 고성능 멀티모니터 캡처
- **컴퓨터 비전 통합**: HSV 색상 검출 + YOLO v11 객체 검출 실시간 처리
- **단일 실행 파일**: DLL 의존성 없는 독립적인 exe 생성
- **vcpkg 불필요**: Git 서브모듈로 모든 라이브러리 관리
- **정적 링킹**: 모든 라이브러리가 exe에 포함
- **배포 최적화**: exe 파일 하나만 복사하면 실행

## 📋 시스템 요구사항

### 필수 소프트웨어
```
- Windows 10/11 (버전 1903 이상)
- Visual Studio 2019 이상 또는 Visual Studio Build Tools
- CMake 3.20 이상
- Git (서브모듈용)
- OpenGL 3.3 지원 그래픽 카드 (GUI 기능용)
- 화면 캡처 권한 (Windows 보안 설정에서 허용 필요)
```

### 권장 하드웨어
```
- CPU: 멀티코어 (실시간 캡처 및 빌드 속도 향상)
- RAM: 8GB 이상 (실시간 캡처 시 16GB 권장)
- 저장공간: 5GB 이상 (서브모듈 + 빌드 파일)
- GPU: DirectX 11/12 지원 (실시간 화면 캡처 최적화)
- 멀티모니터: 여러 모니터 환경에서 선택적 캡처 가능
```

### 실행 환경 권한
```
- 관리자 권한 (화면 캡처 시 필요할 수 있음)
- Windows Defender 실시간 보호 예외 등록 (성능 향상)
- 화면 녹화 권한 (개인정보 보호 설정)
```

## 🚀 빌드 단계별 가이드

### 1단계: 프로젝트 클론 및 서브모듈 초기화

```powershell
# 프로젝트 클론 (서브모듈 포함)
git clone --recursive <repository-url> EducationalComputerVision
cd EducationalComputerVision/ScreenMonitor

# 이미 클론한 경우 서브모듈 초기화
git submodule update --init --recursive

# 서브모듈 상태 확인
git submodule status
```

**예상 결과:**
```
c67de117379f4d1c889c7581a0a76aa0979c2083 external/googletest (release-1.8.0-3569-gc67de117)
8c61ee5498f1761c057f6aa8f87222c11b4c8eff external/imgui (v1.62-4681-g8c61ee54)
98ac4d85bedfaecded9e073b7370daab85ee7275 external/nlohmann_json (v3.11.2-272-g98ac4d85)
43112409ef0b711b18c2dc12433ad5e2403aea71 external/opencv (4.11.0-548-g43112409ef)
b77bc46c53fd01d4678cacd4766a22de2b7ab3d4 external/screen_capture_lite (14.0.6-383-gb77bc46)
```

### 2단계: CMake 구성 (정적 링킹 모드)

```powershell
# Release 모드 구성 (정적 링킹 자동 적용)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 17 2022"

# 또는 MinGW 사용 시
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
```

**구성 성공 확인:**
```
-- OpenGL found - GUI features enabled
-- Building GLFW from: external/screen_capture_lite/Example_OpenGL/glfw
-- ImGui GUI fully enabled with GLFW + OpenGL3
-- MSVC Runtime: Static linking enabled
-- Enabling minimal OpenCV core modules for reliable build
-- Configuring done
-- Generating done
-- Build files have been written to: /path/to/build
```

### 3단계: 정적 링킹 빌드 실행

```powershell
# 병렬 빌드 실행 (4개 코어 사용)
cmake --build build --config Release --parallel 4

# 빌드 진행 상황 모니터링
# 예상 빌드 시간: 10-20분 (첫 빌드), 2-5분 (증분 빌드)
```

**빌드 성공 확인:**
```powershell
# 생성된 실행 파일 확인
dir build\bin\SmartScreenCapture.exe

# 파일 크기 확인 (약 150-300MB 예상 - GUI 포함으로 증가)
Get-ChildItem build\bin\SmartScreenCapture.exe | Format-Table Name, Length

# GUI 기능 테스트 실행
.\build\bin\SmartScreenCapture.exe
```

**GUI 기능 확인:**
실행 후 다음 GUI 구성 요소들이 표시되는지 확인:
- 메인 윈도우: "Smart Screen Capture - Educational Computer Vision"
- 메뉴바: File, View, Help 메뉴
- 패널: Capture Settings, Detection Settings, Performance Monitor
- Live Capture 윈도우: 실시간 비디오 피드 (데모 그라데이션)
- 상태바: FPS 및 캡처 상태 표시

### 4단계: 정적 링킹 검증

```powershell
# DLL 의존성 확인 (최소한의 시스템 DLL만 표시되어야 함)
dumpbin /dependents build\bin\SmartScreenCapture.exe

# 또는 Dependency Walker 사용
# https://www.dependencywalker.com/
```

**예상 의존성 (정적 링킹 성공 시):**
```
KERNEL32.dll    (Windows 핵심)
USER32.dll      (윈도우 관리)
GDI32.dll       (그래픽 인터페이스)
ADVAPI32.dll    (고급 API)
OLE32.dll       (OLE 지원)
OLEAUT32.dll    (OLE 자동화)
OPENGL32.dll    (OpenGL 그래픽 - GUI용)
```

**GUI 의존성 참고:** OpenGL32.dll은 Windows 시스템 기본 라이브러리로, GUI 기능을 위해 필요합니다.

## 🔧 빌드 옵션 및 최적화

### 고급 CMake 옵션

```powershell
# OpenCV 활성화 빌드 (더 오래 걸림)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENCV=ON

# DirectML GPU 가속 활성화 (ONNX Runtime 필요)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DENABLE_GPU=ON

# 디버그 빌드 (개발용)
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
```

### 성능 최적화 빌드

```powershell
# 최대 최적화 빌드
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="/O2 /DNDEBUG"

# 병렬 빌드 코어 수 조정
cmake --build build --config Release --parallel 8
```

## ✅ 빌드 검증 테스트

### 기본 실행 테스트

```powershell
# 메인 애플리케이션 실행 (GUI 포함)
.\build\bin\SmartScreenCapture.exe

# 콘솔 출력 확인 (정상 실행 시)
# === SmartScreenCapture v1.0 ===
# High-Performance Screen Capture & Real-time Computer Vision
# ============================================
# Initializing Smart Screen Capture System...
# ConfigManager created successfully
# HighSpeedCapture created and initialized successfully
# ColorDetector created successfully
# ObjectDetector created successfully
# MainInterface created successfully
# === System Initialization Complete ===

# 테스트 실행 (빌드된 경우)
ctest -C Release --test-dir build --verbose
```

### GUI 기능 상세 테스트

```powershell
# 1. 기본 GUI 실행 테스트
.\build\bin\SmartScreenCapture.exe

# 실행 후 확인사항:
# - 윈도우 생성: 1280x720 해상도
# - 메뉴바 동작: File/View/Help 메뉴 클릭
# - 패널 배치: 도킹 가능한 패널들
# - 실시간 화면 캡처: 실제 모니터 화면 표시
# - 모니터 선택: 드롭다운에서 사용 가능한 모니터 목록
# - 캡처 제어: Start/Stop Capture 버튼 동작
# - FPS 모니터링: 실시간 성능 그래프
```

### 실시간 화면 캡처 기능 테스트

```powershell
# 2. 화면 캡처 기능 상세 테스트
.\build\bin\SmartScreenCapture.exe

# 단계별 테스트:
# 1단계: 모니터 선택
#   - Capture Settings 패널에서 Monitor 드롭다운 확인
#   - 사용 가능한 모니터 목록 표시 확인
#   - 다른 모니터 선택 시 Live Capture 창 변화 없음 (캡처 시작 전)

# 2단계: 캡처 시작
#   - "Start Capture" 버튼 클릭
#   - 버튼이 "Stop Capture"로 변경되는지 확인
#   - Live Capture 창에 실제 화면이 표시되는지 확인
#   - 선택한 모니터의 화면이 실시간으로 업데이트되는지 확인

# 3단계: 성능 확인
#   - Performance Monitor 패널에서 실시간 FPS 확인
#   - FPS 그래프가 실시간으로 업데이트되는지 확인
#   - Target FPS 슬라이더 조정 시 성능 변화 확인

# 4단계: 검출 기능 테스트
#   - Detection Settings에서 HSV Color Detection 체크
#   - HSV 슬라이더 조정으로 색상 검출 확인
#   - 검출된 객체가 Live Capture에 표시되는지 확인
```

### 배포 테스트

```powershell
# 1. 다른 폴더에 exe 복사
mkdir test_deployment
copy build\bin\SmartScreenCapture.exe test_deployment\

# 2. 해당 폴더에서 실행 테스트
cd test_deployment
.\SmartScreenCapture.exe

# 3. 다른 Windows 컴퓨터에서 테스트 (권장)
```

## 🐛 문제 해결

### 일반적인 빌드 오류

#### 1. **CMake 구성 실패**
```powershell
# 해결방법: 빌드 캐시 클리어
rmdir /s build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
```

#### 2. **화면 캡처 관련 오류**
실시간 화면 캡처 기능에서 발생할 수 있는 오류들:

**A. "Failed to initialize screen capture system" 오류:**
```powershell
# 해결방법:
# 1. 관리자 권한으로 실행
runas /user:Administrator ".\build\bin\SmartScreenCapture.exe"

# 2. Windows 보안 설정 확인
# Settings > Privacy & Security > Screen recording
# SmartScreenCapture.exe에 권한 부여

# 3. 바이러스 백신 예외 처리
# Windows Defender > Virus & threat protection
# 실행 파일 경로를 예외 목록에 추가
```

**B. "No monitors detected" 오류:**
```powershell
# 해결방법:
# 1. 그래픽 드라이버 업데이트
# 2. 모니터 연결 상태 확인
# 3. 디스플레이 설정에서 멀티모니터 감지 확인
```

**C. GUI 관련 컴파일 오류 (완전 해결됨):**
```
오류 해결 방법:
- OpenGL 드라이버 업데이트
- Visual Studio 재시작
- 빌드 디렉터리 클리어 후 재구성
- GLFW 서브모듈 상태 확인: git submodule status
```

**현재 시스템 상태:**
- MainInterface.h: 완전한 GLFW + OpenGL3 + ImGui 통합
- PerformanceMonitor: 실시간 화면 캡처 완전 활성화
- HighSpeedCapture: screen_capture_lite 통합 완료
- CMakeLists.txt: 모든 백엔드 완전 활성화

#### 3. **서브모듈 누락**
```powershell
# 해결방법: 서브모듈 강제 업데이트
git submodule update --init --recursive --force
```

#### 4. **메모리 부족 오류**
```powershell
# 해결방법: 병렬 빌드 제한
cmake --build build --config Release --parallel 2
```

#### 5. **링커 오류**
```powershell
# 해결방법: 정적 링킹 설정 확인
# CMakeLists.txt에서 BUILD_SHARED_LIBS=OFF 확인
```

#### 6. **성능 관련 문제**
실시간 화면 캡처 시 성능 최적화:

**A. 낮은 FPS 문제:**
```powershell
# 해결방법:
# 1. Target FPS 설정 조정 (GUI에서)
# 2. 불필요한 백그라운드 애플리케이션 종료
# 3. 전용 GPU 사용 설정 (NVIDIA/AMD 제어판)
# 4. 전원 계획을 "고성능"으로 설정
```

**B. 높은 메모리 사용량:**
```powershell
# 해결방법:
# 1. 해상도가 낮은 모니터 선택
# 2. FPS 제한 활성화
# 3. 검출 알고리즘 일시 비활성화
# 4. Windows 페이지 파일 크기 증가
```

**C. GPU 메모리 부족:**
```powershell
# 해결방법:
# 1. YOLO v11에서 OpenCV DNN 백엔드 사용
# 2. 다른 GPU 사용 애플리케이션 종료
# 3. 모니터 해상도 임시 감소
```

### Visual Studio 관련 문제

#### 1. **컴파일러 찾기 실패**
```powershell
# Visual Studio Developer Command Prompt 사용
# "Developer Command Prompt for VS 2022" 실행 후 빌드
```

#### 2. **런타임 라이브러리 충돌**
```powershell
# 해결방법: CMake에서 런타임 라이브러리 명시
cmake -B build -S . -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded"
```

## 📊 빌드 결과 예상치

### 성공적인 빌드 결과

```
파일 크기: 150-300MB (GUI 포함 정적 링킹으로 인해 증가)
DLL 의존성: 시스템 기본 DLL만 (6-9개, OpenGL 포함)
실행 방식: 독립 실행 (추가 설치 불필요)
성능: 정적 링킹으로 최적화됨
GUI 기능: 완전 활성화 (ImGui + GLFW + OpenGL3)
```

### GUI 기능 세부사항

```
주요 GUI 구성요소:
- 메인 윈도우: 1280x720 기본 해상도
- 도킹 시스템: 자유로운 패널 배치
- 실시간 비디오: OpenGL 텍스처 기반 렌더링
- 성능 모니터: FPS 그래프 및 실시간 통계
- 교육용 패널: 컴퓨터 비전 알고리즘 설정
- 설정 관리: JSON 기반 구성 시스템
```

### 빌드 시간 가이드

```
첫 빌드: 15-30분 (모든 라이브러리 컴파일)
증분 빌드: 1-3분 (변경사항만 컴파일)
클린 빌드: 10-20분 (캐시 활용)
```

## 🎯 배포 가이드

### 단일 파일 배포

```powershell
# 1. 최종 exe 파일 위치
build\bin\SmartScreenCapture.exe

# 2. 배포용 폴더 생성
mkdir release
copy build\bin\SmartScreenCapture.exe release\

# 3. 선택사항: config 파일 포함
copy config\config.json release\

# 4. 배포 패키지 생성
7z a SmartScreenCapture_v1.0.zip release\*
```

### 시스템 요구사항 (최종 사용자)

```
- Windows 10/11 (x64)
- 메모리: 4GB RAM 이상 (GUI 사용 시 6GB 권장)
- OpenGL 3.3 지원 그래픽 카드 (GUI 필수)
- DirectX 12 (GPU 가속 사용 시)
- 관리자 권한 (화면 캡처 시 필요할 수 있음)
- 화면 해상도: 1024x768 이상 (GUI 표시용)
```

## 🔄 지속적 통합 (CI) 설정

### GitHub Actions 예제

```yaml
name: Windows Build
on: [push, pull_request]
jobs:
  build:
    runs-on: windows-latest
    steps:
    - uses: actions/checkout@v3
      with:
        submodules: recursive
    - name: Configure CMake
      run: cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
    - name: Build
      run: cmake --build build --config Release --parallel 4
    - name: Test
      run: ctest -C Release --test-dir build
    - name: Upload Artifact
      uses: actions/upload-artifact@v3
      with:
        name: SmartScreenCapture-Windows
        path: build/bin/SmartScreenCapture.exe
```

## 📚 추가 리소스

### 관련 문서
- `README.md`: 프로젝트 전체 개요
- `CLAUDE.md`: 개발 환경 설정
- `.gitmodules`: 서브모듈 구성 정보

### 개발 도구
- Visual Studio: https://visualstudio.microsoft.com/
- CMake: https://cmake.org/download/
- Git: https://git-scm.com/download/win

---

## 💡 프롬프트 사용 가이드

이 문서를 AI 어시스턴트에게 제공할 때:

```
"다음 Windows 빌드 가이드를 참조하여 서브모듈 기반 정적 링킹 빌드를 도와주세요. 
특히 DLL 없는 단일 실행 파일 생성에 초점을 맞춰 진행해주세요."

[이 문서 전체 내용 첨부]
```

**주요 키워드:**
- 완전한 GUI 활성화 (ImGui + GLFW + OpenGL3)
- 서브모듈 기반 빌드
- 정적 링킹 (Static Linking)
- DLL 없는 실행 파일
- vcpkg 제거
- Windows CMake 빌드
- 교육용 컴퓨터 비전 프레임워크

## 🎨 GUI 기능 상세 설명

### 완전히 구현된 GUI 구성요소

#### 1. **메인 인터페이스**
- **윈도우**: "Smart Screen Capture - Educational Computer Vision"
- **해상도**: 1280x720 (사용자 조정 가능)
- **스타일**: 다크 테마 + 커스텀 색상 스키마

#### 2. **메뉴 시스템**
- **File**: Save Config, Load Config, Exit
- **View**: Reset Layout
- **Help**: About 다이얼로그

#### 3. **패널 구성**
- **Capture Settings**: 모니터 선택, FPS 설정, 캡처 제어
- **Detection Settings**: HSV 색상 검출, YOLO v11 객체 검출
- **Performance Monitor**: FPS 그래프, 성능 통계
- **Live Capture**: 실시간 비디오 표시 (OpenGL 텍스처)
- **상태바**: 현재 상태 및 FPS 표시

#### 4. **실시간 기능**
- **비디오 스트림**: OpenGL 기반 실시간 렌더링
- **FPS 모니터링**: 120개 샘플 히스토리 그래프
- **도킹 시스템**: 자유로운 패널 배치 및 크기 조정
- **알고리즘 시각화**: 컴퓨터 비전 결과 오버레이

#### 5. **교육용 특화 기능**
- **알고리즘 비교**: HSV vs YOLO 검출 결과
- **성능 분석**: 실시간 처리 속도 측정
- **설정 실험**: 파라미터 조정 및 결과 관찰
- **시각적 피드백**: 검출 결과 색상 오버레이

## 🎮 애플리케이션 사용 가이드

### 기본 사용법

#### 1. **애플리케이션 시작**
```
1. SmartScreenCapture.exe 실행
2. 콘솔 창에서 초기화 메시지 확인
3. GUI 창이 열릴 때까지 대기 (약 3-5초)
4. 모든 패널이 정상적으로 표시되는지 확인
```

#### 2. **화면 캡처 시작**
```
1. Capture Settings 패널 찾기
2. Monitor 드롭다운에서 캡처할 모니터 선택
3. Target FPS 슬라이더로 원하는 성능 설정 (권장: 30-60 FPS)
4. "Start Capture" 버튼 클릭
5. Live Capture 창에서 실시간 화면 확인
```

#### 3. **HSV 색상 검출 사용**
```
1. Detection Settings 패널에서 "HSV Color Detection" 체크
2. HSV Lower Bound 슬라이더로 검출할 색상 범위 설정
3. HSV Upper Bound 슬라이더로 색상 범위 상한 설정
4. Live Capture 창에서 검출 결과 확인
5. 실시간으로 슬라이더 조정하며 최적 값 찾기
```

#### 4. **YOLO v11 객체 검출 사용**
```
1. Detection Settings 패널에서 "YOLO v11 Detection" 체크
2. Model Path에 YOLO 모델 파일 경로 입력 (예: models/yolo11n.onnx)
3. Confidence Threshold 조정 (권장: 0.25-0.5)
4. NMS Threshold 조정 (권장: 0.45)
5. Backend 선택 (OpenCV DNN 또는 ONNX Runtime GPU)
6. Live Capture 창에서 객체 검출 결과 확인
```

#### 5. **성능 모니터링**
```
1. Performance Monitor 패널에서 실시간 FPS 확인
2. FPS History 그래프로 성능 안정성 확인
3. Average FPS로 전체 성능 평가
4. 시스템 리소스 사용량 모니터링
```

### 고급 사용법

#### 멀티모니터 환경 최적화
```
1. 고해상도 모니터: Target FPS를 30-45로 설정
2. 듀얼 모니터: 주 모니터에서 캡처, 보조 모니터에서 결과 확인
3. 트리플 모니터: 성능에 따라 FPS 제한 활성화
```

#### 검출 성능 최적화
```
1. HSV 검출: 조명 조건에 따라 색상 범위 조정
2. YOLO 검출: Confidence를 낮춰 더 많은 객체 검출
3. 실시간 처리: 불필요한 검출 알고리즘 비활성화
```

#### 교육 활용 방안
```
1. 알고리즘 비교: HSV와 YOLO를 동시에 활성화하여 결과 비교
2. 성능 분석: 다양한 FPS 설정으로 성능 영향 관찰
3. 파라미터 실험: 실시간으로 설정 조정하며 결과 변화 확인
4. 시각적 학습: 검출 결과를 실시간으로 관찰하며 알고리즘 이해
```