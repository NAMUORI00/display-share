# Knowledge Base Master Index

C_capture 프로젝트 지식 저장소의 **중앙 인덱스**입니다. 모든 기술 자료를 빠르게 찾을 수 있도록 키워드와 카테고리별로 정리되어 있습니다.

## 🔍 빠른 검색 키워드

### 성능 최적화 관련
- **실시간 처리**: `real-time`, `performance`, `optimization`
- **메모리 관리**: `memory`, `allocation`, `deallocation`
- **GPU 가속**: `cuda`, `tensorrt`, `gpu-acceleration`
- **멀티스레딩**: `threading`, `concurrency`, `parallel`

### 기술별 키워드
- **YOLO v11**: `yolo`, `object-detection`, `inference`, `onnx`
- **OpenCV**: `opencv`, `hsv`, `color-detection`, `roi`
- **Screen Capture**: `screen-capture`, `320x320`, `roi-extraction`
- **TensorRT**: `tensorrt`, `engine-optimization`, `fp16`, `int8`
- **ImGui**: `imgui`, `docking`, `rendering`, `ui-performance`

## 📂 카테고리별 자료 맵

### 🎯 YOLO v11 최적화 (`/research/yolo-v11/`)
| 파일명 | 주제 | 키워드 | 등급 |
|--------|------|--------|------|
| `optimization-techniques.md` | YOLO v11 최적화 기법 | `performance`, `inference` | A |
| `tensorrt-integration.md` | TensorRT 통합 방법 | `tensorrt`, `onnx` | A |
| `performance-benchmarks.md` | 성능 벤치마크 | `benchmark`, `fps` | B |
| `best-practices.md` | 베스트 프랙티스 | `best-practice`, `production` | A |

### 🖼️ OpenCV HSV 처리 (`/research/opencv-hsv/`)
| 파일명 | 주제 | 키워드 | 등급 |
|--------|------|--------|------|
| `color-detection-optimization.md` | HSV 색상 검출 최적화 | `hsv`, `color-detection` | A |
| `roi-processing.md` | ROI 처리 최적화 | `roi`, `320x320` | A |
| `performance-tips.md` | 성능 향상 팁 | `performance`, `opencv` | B |
| `api-reference.md` | API 참조 가이드 | `api`, `reference` | A |

### 📺 화면 캡처 (`/research/screen-capture/`)
| 파일명 | 주제 | 키워드 | 등급 |
|--------|------|--------|------|
| `320x320-roi-techniques.md` | 320x320 ROI 기법 | `roi`, `screen-capture` | A |
| `real-time-optimization.md` | 실시간 최적화 | `real-time`, `optimization` | A |
| `windows-specific.md` | Windows 특화 기법 | `windows`, `platform` | B |

### ⚡ TensorRT 엔진 (`/research/tensorrt/`)
| 파일명 | 주제 | 키워드 | 등급 |
|--------|------|--------|------|
| `onnx-optimization.md` | ONNX 모델 최적화 | `onnx`, `optimization` | A |
| `fp16-int8-conversion.md` | 정밀도 변환 | `fp16`, `int8`, `quantization` | A |
| `memory-management.md` | 메모리 관리 | `memory`, `tensorrt` | A |

### 🖥️ ImGui 인터페이스 (`/research/imgui/`)
| 파일명 | 주제 | 키워드 | 등급 |
|--------|------|--------|------|
| `docking-optimization.md` | 도킹 시스템 최적화 | `docking`, `imgui` | A |
| `real-time-rendering.md` | 실시간 렌더링 | `rendering`, `real-time` | B |
| `performance-tips.md` | 성능 최적화 팁 | `performance`, `ui` | B |

## ⚡ 빠른 참조 자료 (`/quick-reference/`)

### API 치트시트
- `opencv-hsv-api.md` - OpenCV HSV API 빠른 참조
- `yolo-inference-api.md` - YOLO 추론 API 가이드
- `tensorrt-engine-api.md` - TensorRT 엔진 API 참조
- `imgui-docking-api.md` - ImGui 도킹 API 치트시트

### 코드 스니펫
- `hsv-color-detection.cpp` - HSV 색상 검출 코드
- `yolo-tensorrt-inference.cpp` - YOLO TensorRT 추론 코드
- `roi-extraction.cpp` - ROI 추출 최적화 코드
- `imgui-docking-setup.cpp` - ImGui 도킹 설정 코드

### 문제 해결
- `tensorrt-common-issues.md` - TensorRT 일반적 문제
- `opencv-performance-issues.md` - OpenCV 성능 문제
- `screen-capture-troubleshooting.md` - 화면 캡처 문제 해결
- `build-system-issues.md` - 빌드 시스템 문제

### 성능 목표
- `real-time-targets.md` - 실시간 처리 목표 (30+ FPS)
- `memory-usage-targets.md` - 메모리 사용량 목표
- `inference-speed-targets.md` - 추론 속도 목표
- `ui-responsiveness-targets.md` - UI 반응성 목표

## 🏗️ 프로젝트 특화 지식 (`/project-specific/`)

### 아키텍처 결정사항
- 모듈식 설계 채택 근거
- 인터페이스 기반 추상화 전략
- 의존성 주입 패턴 적용 이유

### 최적화 이력
- 화면 캡처 성능 개선 추적 (v1.0 → v2.0)
- YOLO 추론 속도 최적화 과정
- 메모리 사용량 감소 히스토리

### 테스트 전략
- GoogleTest 기반 단위 테스트 전략
- 성능 테스트 및 벤치마킹 방법론
- 통합 테스트 시나리오 설계

### 배포 노하우
- Windows 환경 배포 최적화
- 의존성 패키징 전략
- 사용자 환경 호환성 보장

## 🛠️ 템플릿 활용 가이드 (`/templates/`)

### 코드 템플릿
- **C++17 클래스 템플릿**: 모던 C++ 표준 준수
- **인터페이스 구현 템플릿**: 추상화 계층 구현
- **성능 측정 템플릿**: 벤치마킹 코드 표준

### 문서 템플릿
- **기술 분석 보고서**: 외부 리서치 자료 정리 형식
- **최적화 제안서**: 성능 개선 제안 표준 형식
- **API 문서 템플릿**: 일관된 API 문서화 기준

### 테스트 템플릿
- **단위 테스트 템플릿**: GoogleTest 기반 표준
- **성능 테스트 템플릿**: 벤치마킹 테스트 구조
- **통합 테스트 템플릿**: 모듈 간 연동 테스트

## 🔄 업데이트 히스토리

### 최근 업데이트
- **2024-01**: YOLO v11 TensorRT 최적화 자료 추가
- **2024-01**: OpenCV HSV 실시간 처리 기법 업데이트
- **2024-01**: ImGui 도킹 성능 최적화 가이드 추가

### 정기 검증 일정
- **매주 월요일**: 외부 소스 링크 유효성 검증
- **매월 첫째 주**: 성능 벤치마크 업데이트
- **분기별**: 전체 지식 베이스 구조 최적화

---

**검색 팁**
- Grep 도구 사용: `pattern: "optimization" path: "/docs/knowledge-base/"`
- 특정 파일 탐색: `Glob pattern: "*/yolo-v11/*.md"`
- 키워드 조합 검색: `"real-time AND optimization"`