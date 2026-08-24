; =============================================================================
; W3 — Web CJK / Font / Asset smoke scene (Track W)
; Exercises Chinese / Japanese / English / mixed punctuation rendering,
; the shipped CJK font (CaesuraNoto via @font-face) and a genuine fallback
; path (unknown face -> system font stack). Strings below are asserted by
; scripts/web_browser_smoke.mjs --cjk against the PACKAGED dist.
; =============================================================================
[font face="CaesuraNoto" size=22]
[bg storage="assets/bg/classroom.png"]
[ch name="Title" text="Caesura W3 — CJK / Font / Asset smoke"]
[p]
[ch name="中文" text="你好，世界！这是凯苏拉（Caesura）引擎的中文渲染测试。——「引号」、省略号……、破折号、全角，。！？"]
[p]
[ch name="日本語" text="こんにちは、これはケースラエンジンの日本語レンダリングテストです。「括弧」・中黒・──ダッシュ──"]
[p]
[ch name="English" text="The quick brown fox jumps over the lazy dog. "Quotes" & (parens) — em dash…"]
[p]
[ch name="Mixed" text="中文English混排…「日本語」！？— ＆ ￥ ％ ＠"]
[p]
[font face="UnknownFace" size=18]
[ch name="Fallback" text="フォールバック：システムフォントに落ちます。System font fallback works too."]
[p]
[end]