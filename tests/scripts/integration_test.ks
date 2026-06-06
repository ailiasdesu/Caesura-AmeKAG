; =============================================================================
;  Caesura (AmeKAG) — integration_test.ks
;  集成测试用 KAG 脚本
;  覆盖标签: [ch], [bg], [wait], [saveplace], [loadplace], [save], [load]
; =============================================================================

[ch name="主角" text="这是集成测试的第一句对话。"]
[ch name="旁白" text="测试背景切换和存档功能。"]

; 测试1: 背景切换
[bg path="tests/fixtures/test_bg.png"]
[wait time=500]

; 测试2: 临时书签
[saveplace]
[ch name="主角" text="已保存临时书签，继续对话..."]
[wait time=300]
[loadplace]
[ch name="主角" text="从书签恢复成功。"]

; 测试3: 存档/读档
[save slot=1]
[ch name="旁白" text="存档1已保存。"]
[load slot=1]

; 测试4: 有参数的表情/对话
[ch name="主角" text="测试嵌套引号: \"hello world\"。"]
[ch name="旁白" text="测试结束。所有集成测试标签已覆盖。"]
