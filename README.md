# display-share Wiki / Docs

문서 전용 브랜치. **GitHub Pages** 소스.

- **사이트**: https://NAMUORI00.github.io/display-share/
- **소스 코드**: [`main`](https://github.com/NAMUORI00/display-share/tree/main)

## 구성

| 경로 | 역할 |
|------|------|
| `index.html` | 문서 셸 (base path + Mermaid 내구성) |
| `pages/*.md` | **본문 SSOT** (ASCII 파일명) |
| 한글 `*.md` | GitHub blob/wiki 호환 미러 |
| `.nojekyll` | Jekyll 비활성 |

## 로컬 미리보기

```powershell
# wiki 브랜치 루트
python -m http.server 8080
# http://localhost:8080/
```

## 페이지

설계: 홈 · 아키텍처 · 파이프라인 · 설계안 · 개선점  
사용: 시작하기 · 재현 · 설정 · 모델  
기타: 라이선스 · 개발 현황
