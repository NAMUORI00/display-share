# display-share Wiki / Docs

이 브랜치(`wiki`)는 **문서 전용**이며 **GitHub Pages** 소스로 사용됩니다.

- **문서 사이트**: https://NAMUORI00.github.io/display-share/
- **소스 코드**: [`main` 브랜치](https://github.com/NAMUORI00/display-share/tree/main)

## 구성

| 경로 | 역할 |
|------|------|
| `index.html` | 문서 사이트 셸 (사이드바 + 마크다운 렌더) |
| `pages/*.md` | 페이지 본문 (ASCII 파일명) |
| `*.md` (한글 파일명) | GitHub Wiki 호환 원본 |
| `.nojekyll` | Jekyll 비활성 (정적 호스팅) |

## 로컬 미리보기

`wiki` 브랜치 루트에서 정적 서버를 띄우면 됩니다.

```powershell
# 예: Python
python -m http.server 8080
# 브라우저: http://localhost:8080/
```
