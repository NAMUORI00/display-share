# Deployment Notes

C_capture 프로젝트의 배포 관련 실무 노하우 및 주의사항 모음

## 🚀 배포 환경 구성

### Windows 환경 배포 체크리스트

#### 시스템 요구사항 검증
```bash
# GPU 지원 확인
nvidia-smi
# CUDA 버전 확인  
nvcc --version
# Visual C++ 런타임 확인
dir "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist"
```

#### 필수 런타임 라이브러리
1. **Visual C++ Redistributable 2022** (x64)
2. **CUDA Runtime 11.8+** (GPU 사용 시)
3. **TensorRT 8.6+** (AI 추론 사용 시)

#### 환경 변수 설정
```batch
# TensorRT 경로 설정
set TENSORRT_ROOT=C:\TensorRT-8.6.1.6
set PATH=%PATH%;%TENSORRT_ROOT%\lib

# CUDA 경로 설정 (보통 자동 설정됨)
set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8
```

---

## 📦 의존성 패키징 전략

### Git Submodule 배포 가이드

#### 프로덕션 빌드를 위한 Submodule 준비
```bash
# 모든 submodule을 특정 커밋으로 고정
cd ScreenMonitor/external/opencv
git checkout 4.8.1  # 안정적인 릴리즈 버전

cd ../imgui
git checkout docking # 도킹 브랜치의 안정 커밋

cd ../googletest
git checkout release-1.12.1

# 변경사항 커밋
git add ScreenMonitor/external/
git commit -m "Pin submodules to stable versions for production"
```

#### 오프라인 배포를 위한 완전한 소스 패키징
```bash
# 모든 submodule을 포함한 완전한 소스 아카이브 생성
git archive --format=tar.gz --prefix=C_capture/ HEAD > C_capture_source.tar.gz

# Submodule도 포함하여 패키징
git submodule foreach --recursive 'git archive --format=tar --prefix=C_capture/$sm_path/ HEAD' | tar -xf - -C temp/
cd temp && tar -czf ../C_capture_complete.tar.gz C_capture/
```

### 바이너리 의존성 관리

#### DLL 수집 스크립트
```python
# collect_dependencies.py
import os
import shutil
import subprocess
from pathlib import Path

def collect_dll_dependencies(executable_path, output_dir):
    """실행 파일의 DLL 의존성을 수집"""
    
    # Dependency Walker 또는 dumpbin 사용
    result = subprocess.run([
        'dumpbin', '/dependents', executable_path
    ], capture_output=True, text=True)
    
    dll_list = parse_dependencies(result.stdout)
    
    for dll in dll_list:
        dll_path = find_dll_path(dll)
        if dll_path and not is_system_dll(dll):
            shutil.copy2(dll_path, output_dir)
            print(f"Copied: {dll}")

def find_dll_path(dll_name):
    """시스템 PATH에서 DLL 찾기"""
    for path in os.environ['PATH'].split(';'):
        dll_path = Path(path) / dll_name
        if dll_path.exists():
            return dll_path
    return None

# 사용법
collect_dll_dependencies('SmartScreenCapture.exe', './dist/bin/')
```

---

## 🔧 빌드 시스템 최적화

### CMake 프로덕션 설정

#### 최적화된 릴리즈 빌드
```cmake
# 프로덕션 빌드 설정
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # 최대 최적화
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    
    # Link Time Optimization
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
    
    # 정적 링킹 (배포 단순화)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -static-libgcc -static-libstdc++")
    
    # 디버그 정보 제거
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
endif()
```

#### 자동화된 배포 빌드 스크립트
```batch
@echo off
echo Building C_capture for production deployment...

REM 빌드 디렉토리 정리
if exist build rmdir /s /q build
mkdir build

REM CMake 구성 (정적 라이브러리 우선)
cmake -B build -S ScreenMonitor ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DOPENCV_STATIC=ON ^
    -DWITH_CUDA=ON ^
    -DWITH_TENSORRT=ON

REM 병렬 빌드
cmake --build build --config Release --parallel %NUMBER_OF_PROCESSORS%

REM 테스트 실행
ctest -C Release --test-dir build --output-on-failure

REM 배포 패키지 생성
call package_for_deployment.bat

echo Build and packaging completed!
```

### 배포 패키징 자동화

#### 완전한 배포 패키지 생성
```batch
REM package_for_deployment.bat
@echo off
set DIST_DIR=dist
set VERSION=1.0.0

REM 배포 디렉토리 구조 생성
mkdir %DIST_DIR%\bin
mkdir %DIST_DIR%\config
mkdir %DIST_DIR%\models
mkdir %DIST_DIR%\docs

REM 실행 파일 및 DLL 복사
copy build\bin\Release\SmartScreenCapture.exe %DIST_DIR%\bin\
copy build\bin\Release\*.dll %DIST_DIR%\bin\

REM 설정 파일 복사
copy ScreenMonitor\config\default_config.json %DIST_DIR%\config\

REM 사용자 가이드 복사
copy ScreenMonitor\README.md %DIST_DIR%\docs\
copy BUILD_GUIDE.md %DIST_DIR%\docs\

REM 압축 패키지 생성
powershell Compress-Archive -Path %DIST_DIR%\* -DestinationPath C_capture_v%VERSION%_windows_x64.zip

echo Package created: C_capture_v%VERSION%_windows_x64.zip
```

---

## 🛡️ 보안 및 안정성 고려사항

### 실행 환경 보안

#### 코드 서명 (선택사항)
```batch
REM 코드 서명을 통한 신뢰성 확보
signtool sign /f "certificate.p12" /p "password" /t "http://timestamp.digicert.com" SmartScreenCapture.exe
```

#### 바이러스 스캔 예외 설정 안내
```text
# 사용자 가이드에 포함할 내용
일부 백신 프로그램이 화면 캡처 기능을 의심스러운 활동으로 감지할 수 있습니다.
다음 디렉토리를 예외 목록에 추가하세요:
- C:\Program Files\C_capture\
- %APPDATA%\C_capture\
```

### 안정성 향상 방법

#### 크래시 리포팅 시스템
```cpp
// 크래시 덤프 생성 설정
#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>

LONG WINAPI unhandled_exception_filter(EXCEPTION_POINTERS* exception_info) {
    // 미니덤프 파일 생성
    HANDLE dump_file = CreateFile(
        L"crash.dmp",
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (dump_file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dump_info;
        dump_info.ThreadId = GetCurrentThreadId();
        dump_info.ExceptionPointers = exception_info;
        dump_info.ClientPointers = FALSE;
        
        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            dump_file,
            MiniDumpNormal,
            &dump_info,
            nullptr,
            nullptr
        );
        
        CloseHandle(dump_file);
    }
    
    return EXCEPTION_EXECUTE_HANDLER;
}

// main 함수에서 설정
int main() {
    SetUnhandledExceptionFilter(unhandled_exception_filter);
    // ... 나머지 코드
}
#endif
```

#### 자동 복구 메커니즘
```cpp
class SafeApplication {
    int restart_count_ = 0;
    static constexpr int MAX_RESTART_COUNT = 3;
    
public:
    int run() {
        while (restart_count_ < MAX_RESTART_COUNT) {
            try {
                return run_main_loop();
            }
            catch (const std::exception& e) {
                log_error("Application crashed: " + std::string(e.what()));
                ++restart_count_;
                
                if (restart_count_ < MAX_RESTART_COUNT) {
                    log_info("Attempting automatic restart...");
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }
        }
        
        log_fatal("Maximum restart attempts exceeded");
        return -1;
    }
};
```

---

## 📊 배포 후 모니터링

### 성능 텔레메트리

#### 기본 성능 메트릭 수집
```cpp
class PerformanceTelemetry {
    struct Metrics {
        double avg_fps = 0.0;
        double avg_latency_ms = 0.0;
        size_t memory_usage_mb = 0;
        size_t gpu_memory_usage_mb = 0;
        int crash_count = 0;
    };
    
    Metrics metrics_;
    std::ofstream log_file_;
    
public:
    PerformanceTelemetry() : log_file_("performance.log", std::ios::app) {
        log_file_ << "timestamp,fps,latency_ms,ram_mb,gpu_mb,crashes\n";
    }
    
    void record_frame(double fps, double latency_ms) {
        // 이동 평균 계산
        metrics_.avg_fps = 0.9 * metrics_.avg_fps + 0.1 * fps;
        metrics_.avg_latency_ms = 0.9 * metrics_.avg_latency_ms + 0.1 * latency_ms;
        
        // 주기적으로 로그 파일에 기록
        static int frame_count = 0;
        if (++frame_count % 300 == 0) {  // 10초마다 (30fps 기준)
            log_metrics();
        }
    }
    
private:
    void log_metrics() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        log_file_ << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                 << "," << metrics_.avg_fps
                 << "," << metrics_.avg_latency_ms
                 << "," << get_ram_usage_mb()
                 << "," << get_gpu_memory_usage_mb()
                 << "," << metrics_.crash_count << "\n";
        log_file_.flush();
    }
};
```

### 사용자 피드백 수집

#### 간단한 피드백 시스템
```cpp
class FeedbackCollector {
public:
    void show_feedback_dialog() {
        // ImGui를 통한 간단한 피드백 폼
        if (ImGui::Begin("Feedback")) {
            static char feedback_text[1000] = "";
            ImGui::InputTextMultiline("Your feedback:", feedback_text, sizeof(feedback_text));
            
            if (ImGui::Button("Send Feedback")) {
                save_feedback(feedback_text);
                memset(feedback_text, 0, sizeof(feedback_text));
            }
        }
        ImGui::End();
    }
    
private:
    void save_feedback(const std::string& feedback) {
        std::ofstream file("feedback.log", std::ios::app);
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        file << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
             << " | " << feedback << "\n";
    }
};
```

---

## 🔄 업데이트 및 유지보수

### 자동 업데이트 체크

#### 버전 확인 시스템
```cpp
class UpdateChecker {
    std::string current_version_ = "1.0.0";
    std::string update_url_ = "https://api.github.com/repos/user/C_capture/releases/latest";
    
public:
    struct VersionInfo {
        std::string version;
        std::string download_url;
        std::string changelog;
        bool is_newer = false;
    };
    
    VersionInfo check_for_updates() {
        // HTTP 요청을 통해 최신 버전 정보 조회
        auto json_response = fetch_latest_release_info();
        return parse_version_info(json_response);
    }
    
private:
    bool is_version_newer(const std::string& remote_version) {
        // 의미론적 버전 비교 (semantic versioning)
        return compare_versions(remote_version, current_version_) > 0;
    }
};
```

### 설정 파일 마이그레이션

#### 버전별 설정 호환성
```cpp
class ConfigMigrator {
public:
    bool migrate_config(const std::string& config_path) {
        nlohmann::json config = load_json(config_path);
        
        std::string version = config.value("config_version", "1.0.0");
        
        if (version == "1.0.0") {
            migrate_from_v1_0_to_v1_1(config);
            version = "1.1.0";
        }
        
        if (version == "1.1.0") {
            migrate_from_v1_1_to_v1_2(config);
            version = "1.2.0";
        }
        
        config["config_version"] = version;
        return save_json(config_path, config);
    }
    
private:
    void migrate_from_v1_0_to_v1_1(nlohmann::json& config) {
        // v1.0에서 v1.1로의 설정 변경사항 처리
        if (config.contains("old_setting")) {
            config["new_setting"] = config["old_setting"];
            config.erase("old_setting");
        }
    }
};
```

---

## 📋 배포 후 검증 체크리스트

### 기능 검증
- [ ] 애플리케이션이 정상적으로 시작됨
- [ ] 화면 캡처가 작동함 (권한 확인)
- [ ] YOLO 추론이 정상 작동함 (GPU/CPU 환경별)
- [ ] GUI 인터페이스가 올바르게 표시됨
- [ ] 설정 파일 로드/저장이 작동함

### 성능 검증
- [ ] 30 FPS 이상 달성 (대상 하드웨어에서)
- [ ] 메모리 사용량이 예상 범위 내
- [ ] CPU 사용률이 적정 수준
- [ ] 장시간 실행 시 안정성 확인

### 사용자 경험 검증
- [ ] 설치 과정이 간단함
- [ ] 에러 메시지가 이해하기 쉬움
- [ ] 도움말 문서가 포함됨
- [ ] 제거 과정이 깨끗함

---

## 🚨 일반적인 배포 문제 및 해결책

### DLL 의존성 문제
```text
문제: "xxx.dll을 찾을 수 없습니다"
해결: 
1. Dependency Walker로 누락된 DLL 확인
2. 해당 DLL을 실행 파일과 같은 디렉토리에 복사
3. 정적 링킹으로 재빌드 고려
```

### GPU 드라이버 호환성
```text
문제: TensorRT 초기화 실패
해결:
1. NVIDIA 드라이버 버전 확인 (최소 요구사항: 472.84+)
2. CUDA 설치 상태 확인
3. GPU 메모리 부족 여부 확인
4. CPU 폴백 모드로 전환
```

### 권한 관련 문제
```text
문제: 화면 캡처 실패 (접근 거부)
해결:
1. 관리자 권한으로 실행
2. Windows 개인정보 보호 설정 확인
3. 백신 프로그램 예외 설정
4. UAC 설정 조정
```

---

**관련 문서**
- [빌드 가이드](../../ScreenMonitor/BUILD_GUIDE.md)
- [아키텍처 결정사항](architecture-decisions.md)
- [성능 목표](../quick-reference/performance-targets/)