# 🎯 320x320 중앙 영역 실시간 감지 시스템 개선 계획

## 📋 프로젝트 현황 요약
- **현재**: 범용 화면 캡처 & 컴퓨터 비전 시스템 (과도하게 복잡)
- **목표**: 화면 중앙 320x320 영역 HSV/YOLO 실시간 감지 + GUI 표시
- **문제**: 불필요한 코드, 미완성 YOLO, 전체화면 캡처, 복잡한 아키텍처

## 🏗️ STEP 0: 에이전트 팀 구성 (최우선 작업)

### 📊 리서치팀
- **general-purpose**: 프로젝트 전체 분석 및 현황 파악
- **cpp-architect**: 현재 아키텍처 분석 및 리팩토링 방향 수립
- **vision-expert**: HSV/YOLO 알고리즘 현황 분석 및 개선점 도출
- **🌐 외부자료 수집 에이전트**: 
  - **WebSearch/WebFetch**: YOLO v11 최신 구현 사례, OpenCV HSV 최적화 기법 조사
  - **mcp__exa__deep_researcher**: 실시간 화면 캡처 최적화 방법론, 320x320 ROI 처리 기법 심층 연구
  - **mcp__context7**: OpenCV, ImGui, TensorRT 최신 문서 및 API 레퍼런스 수집

### 🔨 구현팀
- **cpp-architect**: 불필요한 코드 제거 및 아키텍처 최적화
- **vision-expert**: HSV 감지 최적화 및 YOLO v11 완성
- **performance-optimizer**: 320x320 캡처 로직 및 실시간 처리 최적화
- **gui-developer**: ImGui 인터페이스 개선 (ROI 표시, 감지 결과)
- **cmake-specialist**: 빌드 시스템 정리 및 최적화

### 🧪 QA팀
- **test-engineer**: 핵심 기능 테스트 및 품질 검증
- **performance-optimizer**: 성능 벤치마크 및 최적화 검증
- **general-purpose**: 통합 테스트 및 최종 검증

## 🚀 실행 단계별 계획 (에이전트 팀 기반)

### Phase 0: 팀 구성 및 분석 (리서치팀 + 외부자료 수집)
**에이전트 팀**: general-purpose, cpp-architect, vision-expert + 외부자료 수집팀
1. **🌐 외부 기술 자료 수집** (외부자료 수집 에이전트들)
   - **WebSearch/WebFetch**: "YOLO v11 real-time inference optimization", "OpenCV HSV color detection performance"
   - **mcp__exa__deep_researcher**: "320x320 ROI screen capture techniques", "real-time computer vision pipeline optimization"
   - **mcp__context7**: OpenCV 4.x HSV 함수 최신 문서, ImGui docking 최적화 가이드, TensorRT ONNX 추론 Best Practices
2. **프로젝트 전체 현황 분석** (general-purpose)
3. **코드 아키텍처 분석** (cpp-architect) 
4. **HSV/YOLO 구현 상태 분석** (vision-expert)
5. **외부 자료 기반 개선 방향 수립** (전체 리서치팀)
6. **제거할 코드 및 개선점 목록 작성** (전체 리서치팀)

### Phase 1: 아키텍처 정리 (구현팀 + QA팀)
**에이전트 팀**: cpp-architect, cmake-specialist, test-engineer
1. **불필요한 .cpp 파일 제거/단순화** (cpp-architect)
   - ColorDetector.cpp 제거
   - ObjectDetector.cpp 단순화 
   - 복잡한 인터페이스 정리
2. **CMakeLists.txt 빌드 대상 정리** (cmake-specialist)
3. **빌드 테스트 및 검증** (test-engineer)

### Phase 2: 핵심 기능 완성 (구현팀 + QA팀)
**에이전트 팀**: vision-expert, performance-optimizer, test-engineer
1. **320x320 중앙 캡처 로직 구현** (performance-optimizer)
   - ScreenCaptureLiteDevice 수정
   - ROI 기반 캡처 최적화 (외부 자료 기반)
2. **HSV 감지 ROI 최적화** (vision-expert)
   - 외부 조사 자료 기반 최적화 적용
3. **YOLO v11 추론 완성** (vision-expert)
   - YOLOv11TensorRTInference.cpp 완성
   - 외부 Best Practice 적용한 실제 객체 감지 구현
4. **성능 테스트 및 최적화** (test-engineer + performance-optimizer)

### Phase 3: GUI 개선 (구현팀 + QA팀)
**에이전트 팀**: gui-developer, test-engineer
1. **320x320 ROI 표시 추가** (gui-developer)
2. **감지된 좌표/바운딩박스 실시간 표시** (gui-developer)
3. **불필요한 GUI 패널 정리** (gui-developer)
4. **GUI 기능 테스트** (test-engineer)

### Phase 4: 문서 정리 (전체 팀 협업)
**에이전트 팀**: general-purpose + 모든 전문 에이전트
1. **CLAUDE.md 파일별 재작성** (각 담당 에이전트)
   - 루트 CLAUDE.md (general-purpose)
   - ScreenMonitor/CLAUDE.md (cpp-architect)
   - src/CLAUDE.md (vision-expert)
   - external/CLAUDE.md (cmake-specialist)
   - tests/CLAUDE.md (test-engineer)
2. **통합 문서 검토** (general-purpose)

### Phase 5: 최종 QA 및 검증 (QA팀)
**에이전트 팀**: test-engineer, performance-optimizer, general-purpose
1. **전체 시스템 통합 테스트** (test-engineer)
2. **성능 벤치마크 측정** (performance-optimizer)
3. **최종 품질 검증** (general-purpose)

## 🔧 각 팀이 활용할 전문 도구들
- **리서치팀**: 
  - 내부 분석: Read, Grep, Glob, Task
  - **외부 자료**: WebSearch, WebFetch, mcp__exa__deep_researcher, mcp__context7
- **구현팀**: Write, Edit, MultiEdit, Bash (코드 작성 중심)
- **QA팀**: Bash, Task (테스트 및 검증 중심)

## 📈 예상 결과
- **최신 기술 적용**: 외부 자료 기반 최적화 기법 도입
- **코드 라인 수**: 현재 대비 30-40% 감소
- **빌드 시간**: 불필요한 모듈 제거로 단축
- **실행 성능**: 320x320 영역 전용 + 외부 최적화 기법으로 대폭 향상
- **유지보수성**: 목적 특화 + 최신 Best Practice 적용

## ⚡ 실행 방식
1. **Step 0**: 모든 필요한 에이전트 팀을 Task 도구로 구성
2. **외부 자료 수집**: WebSearch, mcp__exa__, mcp__context7 활용한 기술 조사
3. **Step 1-5**: 각 Phase별로 해당 에이전트 팀이 병렬/순차 작업 진행
4. **품질 관리**: 각 Phase 완료 시 QA팀의 검증 과정 필수

## 📊 현재 진행 상황

### Phase 0: 외부 자료 수집 결과 (진행 중)

#### 🔍 YOLO v11 최적화 조사 결과
- **성능 개선**: 이전 버전 대비 22% 적은 파라미터로 2% 높은 mAP 달성
- **TensorRT 최적화**: FP16에서 35% 성능 향상, INT8에서 46% 성능 향상
- **320x320 입력 최적화**: ~120 FPS 달성 가능 (소형 모델 기준)
- **배치 처리**: 32개 배치로 45 FPS → 200+ FPS 달성 가능

#### 🎨 HSV 색상 감지 최적화 결과
- **OpenCV 4.x 개선**: color conversion 함수 성능 향상
- **SIMD 최적화**: SSE2, AVX 자동 활용으로 2배 성능 향상
- **ROI 처리**: 320x320 영역 전용 처리로 메모리 사용량 최소화
- **모폴로지 최적화**: 작은 커널(3x3, 5x5) 사용으로 빠른 연산

#### 🚀 실시간 캡처 최적화 (진행 중)
- Windows 320x320 중앙 영역 캡처 기법 연구 중
- 60FPS+ 고성능 파이프라인 설계 방안 조사 중

## 다음 단계
- 외부 자료 수집 완료 대기
- 리서치팀 구성 및 프로젝트 현황 분석 시작
- 구현팀 준비 및 작업 계획 수립

---

**작성일**: 2025-01-31  
**상태**: Phase 0 진행 중  
**다음 업데이트**: 외부 자료 수집 완료 후