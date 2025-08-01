# Knowledge Base 사용 가이드

C_capture 프로젝트의 **중앙집중식 지식 저장소**입니다. 외부 리서치 에이전트들이 수집한 모든 기술 자료를 체계적으로 관리하며, 구현 에이전트들이 필요할 때 즉시 접근할 수 있도록 최적화되었습니다.

## 📁 디렉토리 구조

### `/research/` - 외부 리서치 자료
기술 도메인별로 분류된 외부 연구 자료 및 최신 기술 동향
- **yolo-v11/**: YOLO v11 최적화 기법 및 TensorRT 통합
- **opencv-hsv/**: OpenCV HSV 색상 검출 최적화
- **screen-capture/**: 실시간 화면 캡처 최적화 기법
- **tensorrt/**: TensorRT 엔진 최적화 및 메모리 관리
- **imgui/**: ImGui 도킹 시스템 성능 최적화

### `/quick-reference/` - 빠른 참조 자료
즉시 사용 가능한 실용적 정보
- **api-cheatsheets/**: API 참조 치트시트
- **code-snippets/**: 재사용 가능한 코드 조각
- **troubleshooting/**: 문제 해결 가이드
- **performance-targets/**: 성능 목표 및 벤치마크

### `/project-specific/` - 프로젝트 특화 지식
C_capture 프로젝트만의 고유한 기술적 결정사항
- 아키텍처 결정 사항 및 근거
- 최적화 이력 및 성능 개선 추적
- 테스트 전략 및 품질 보증 방법론
- 배포 관련 주의사항 및 노하우

### `/templates/` - 템플릿 및 보일러플레이트
개발 효율성을 위한 재사용 가능한 템플릿
- **code-templates/**: C++17 코드 템플릿
- **documentation-templates/**: 문서화 표준 템플릿
- **testing-templates/**: 테스트 코드 템플릿

## 🔍 사용 방법

### 에이전트별 접근 패턴

#### knowledge-manager
```bash
# 새로운 리서치 자료 저장
Write -> /docs/knowledge-base/research/{domain}/{topic}.md

# 인덱스 업데이트
Edit -> /docs/knowledge-base/index.md
```

#### technical-advisor
```bash
# 빠른 정보 검색
Grep -> pattern: "optimization" path: "/docs/knowledge-base/"

# 특정 도메인 자료 탐색
Glob -> pattern: "*/yolo-v11/*.md"
```

#### documentation-curator
```bash
# 프로젝트 문서와 지식 베이스 동기화
Read -> 기존 CLAUDE.md 파일들
Write -> 통합된 최신 문서
```

#### context-provider
```bash
# 에이전트별 맞춤 컨텍스트 제공
Read -> /docs/knowledge-base/project-specific/
Task -> 상황별 컨텍스트 분석
```

## 📊 품질 관리 기준

### 자료 등급 시스템
- **Grade A**: 공식 문서, 검증된 벤치마크, 프로덕션 검증
- **Grade B**: 커뮤니티 베스트 프랙티스, 실험적 최적화
- **Grade C**: 이론적 접근, 미검증 최적화 기법

### 메타데이터 표준
```yaml
---
title: 문서 제목
domain: [yolo-v11, opencv-hsv, screen-capture, tensorrt, imgui]
grade: [A, B, C]
source_url: 원본 URL
collected_date: YYYY-MM-DD
last_verified: YYYY-MM-DD
relevance_score: [1-10]
tags: [최적화, 실시간, 성능, 메모리]
---
```

## 🚀 최적화 가이드라인

### 검색 최적화
- 일관된 태그 시스템 사용
- 키워드 기반 파일명 지정
- 크로스 레퍼런스 링크 활용

### 접근성 최적화
- 빠른 참조용 요약 섹션 필수
- 코드 예제 우선 제공
- 실행 가능한 명령어 포함

### 유지보수 최적화
- 정기적인 외부 소스 검증
- 중복 정보 통합 관리
- 버전 히스토리 추적

---

**Knowledge Base 운영 원칙**
1. **정확성**: 검증된 정보만 저장
2. **접근성**: 빠른 검색과 즉시 활용 가능
3. **일관성**: 표준화된 형식과 구조
4. **현재성**: 최신 정보 유지 및 업데이트
5. **실무성**: 실제 개발에 직접 적용 가능한 내용 중심