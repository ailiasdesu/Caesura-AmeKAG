; Caesura Galgame Demo — version 3 (minimal, incrementally tested)
[font face="default" size=22]
[pt speed=60]

[bg storage="assets/bg/classroom.png"]
[wait time=300]

[playbgm storage="assets/bgm/daily.wav" volume=0.8]

[ch name="Narrator" text="Welcome to Caesura AmeKAG Engine Demo."]
[p]

[ch name="Narrator" text="This is a test of the KAG scripting system."]
[p]

; Scene 1 — Classroom
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=400]

[ch name="Narrator" text="Afternoon sunlight filters through the window."]

[playse storage="assets/se/click.wav"]

[ch name="Teacher" text="Welcome to our class. Today we learn something interesting."
     voice="assets/voice/line01.wav" sprite="assets/fg/girl_uniform.png"]
[p]

; Scene 2 — Hallway
[cl]
[bg storage="assets/bg/hana.png"]
[wait time=400]

[ch name="Narrator" text="After class, you walk to the hallway."]

[fg storage="assets/fg/girl_uniform.png"]
[position layer="fg" pos="right"]

[playvoice storage="assets/voice/line01.wav"]
[ch name="Sakura" text="Hello! Are you a new student? My name is Sakura."]
[p]

; Scene 3 — Library
[cl]
[bg storage="assets/bg/classroom.png"]
[wait time=400]

[ch name="Narrator" text="After school, you go to the library with Sakura."]
[ch name="Sakura" text="Look, there is an old notebook here."]
[p]

; Ending
[cl]
[bg storage="assets/bg/hana.png"]

[ch name="Narrator" text="The day ends. You return home, looking forward to tomorrow."]

[wait time=1000]
[stopbgm]

[ch name="Narrator" text="Thank you for watching the Caesura AmeKAG demo."]
[p]

[end]
