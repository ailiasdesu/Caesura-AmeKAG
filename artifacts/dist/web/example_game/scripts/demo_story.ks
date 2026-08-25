; ===========================================================================
;  Caesura (AmeKAG) — KAG Demo Story
;  演示 KAG 标签语法
; ===========================================================================

[font face="default" size=24 color="#ffffff"]
[pt speed=50]

[bg storage="demo_bg.png"]
[wait time=300]

*start

[ch name="Narrator" text="Welcome to Caesura (AmeKAG)."]
[p]

[ch name="Narrator" text="This is a demonstration of the KAG scripting language."]
[p]

[ch name="Narrator" text="You can use [bg] to change backgrounds..."]
[bg storage="demo_bg2.png"]
[wait time=500]

[ch name="Narrator" text="...[fg] to add foreground characters..."]
[fg storage="demo_chara.png"]
[wait time=400]

[ch name="Narrator" text="...[position] to place them on screen..."]
[position layer="fg0" x=0.5 y=0.5 scale=1.0]
[wait time=300]

[ch name="Narrator" text="...and [layopt] to adjust their properties."]
[layopt layer="fg0" opacity=0.8]

[p]

[ch name="Character" text="Dialog can include character names!"]
[p]

[text text="Or plain text without a speaker label."]
[p]

[ruby text="Ruby" ruby="ルビ"]
[ch name="Narrator" text="Ruby annotations add furigana above kanji."]
[p]

[font face="Noto Serif" size=28 color="#ffcc00"]
[ch name="Narrator" text="Fonts, sizes, and colors are all configurable."]
[reset]
[p]

[ch name="Narrator" text="Audio is supported too:"]
[playbgm storage="bgm/demo.ogg" volume=0.8]
[ch name="Narrator" text="Background music is now playing."]
[p]

[playse storage="se/click.wav"]
[ch name="Narrator" text="Sound effects play instantly."]
[stopse]
[p]

[ch name="Narrator" text="Transitions make scene changes smooth:"]
[cl layer="fg"]
[ch name="Narrator" text="Goodbye foreground character."]
[wait time=300]

[ch name="Narrator" text="That's the end of the demo."]

[l]
[ch name="Narrator" text="Thanks for trying Caesura (AmeKAG)!"]

[p]
[er]

; ===========================================================================
;  End of demo
; ===========================================================================
