# KAG API Reference — 完整版

> Caesura (AmeKAG) 引擎 KAG 脚本 API。61 个命令，6 个子模块。
> 所有 C++ 访问通过 `backend.lua → BackendRegistry → I*Backend` 抽象接口。

## Layer（图层操作）

| 函数 | 参数 | 说明 |
|------|------|------|
| `KAG.show_image(layer, path, x, y, opts?)` | layer:"bg"/"fg"/"char", path:string, x,y:number, opts:{scaleX?,scaleY?,opacity?,blend?} | 在指定图层显示图像 |
| `KAG.hide_image(layer)` | layer:string | 隐藏图层图像 |
| `KAG.move_image(layer, x, y, duration?)` | layer:string, x,y:number, duration:number(0) | 平滑移动图像 |
| `KAG.set_image_scale(layer, sx, sy)` | layer:string, sx,sy:number | 设置图像缩放 |
| `KAG.set_image_opacity(layer, value)` | layer:string, value:number(0-1) | 设置图像透明度 |
| `KAG.set_image_blend(layer, mode)` | layer:string, mode:number | 设置混合模式 |
| `KAG.clear_screen(layer?)` | layer:string? | 清除指定图层（nil=全部） |
| `KAG.clear_text_layer()` | — | 清除消息层 |
| `KAG.clear_text()` | — | 清除消息文本 |

## Text（文本/对话）

| 函数 | 参数 | 说明 |
|------|------|------|
| `KAG.show_text(name, text)` | name:string?, text:string | 显示对话文本 |
| `KAG.hide_text()` | — | 隐藏消息框 |
| `KAG.wait_click()` | — | 暂停等待用户点击 |
| `KAG.set_font(id)` | id:number | 0=小字体, 1=大字体, 2=TTF |
| `KAG.set_text_speed(cps)` | cps:number | 字符/秒显示速度 |
| `KAG.set_text_color(r, g, b)` | r,g,b:number(0-255) | 文本颜色 |
| `KAG.render_text(text, x, y, r, g, b, a)` | text:string, x,y:number, r,g,b,a:number | 直接渲染文本在坐标 |
| `KAG.render_ruby(text, ruby, x, y, r, g, b, a)` | text:string, ruby:string, x,y:number, r,g,b,a:number | Ruby注音文本 |
| `KAG.line_height()` | — | 返回当前字体行高(px) |

## Audio（音频）

| 函数 | 参数 | 说明 |
|------|------|------|
| `KAG.play_bgm(path, volume?, loop?)` | path:string, volume:number(0.6), loop:bool(true) | 播放背景音乐 |
| `KAG.stop_bgm(fade?)` | fade:number(1.0) | 淡出停止 BGM |
| `KAG.play_voice(path, volume?)` | path:string, volume:number(1.0) | 播放语音（绝对中断） |
| `KAG.stop_voice()` | — | 停止语音 |
| `KAG.play_se(path, volume?)` | path:string, volume:number(1.0) | 播放音效（2D） |
| `KAG.play_se_3d(path, x, y, z)` | path:string, x,y,z:number | 播放空间音效 |
| `KAG.stop_se()` | — | 停止所有音效 |
| `KAG.set_se_volume(handle, volume)` | handle:number, volume:number(0-1) | 单音效音量 |
| `KAG.set_bgm_volume(volume)` | volume:number(0-1) | BGM 总线音量 |
| `KAG.set_voice_volume(volume)` | volume:number(0-1) | 语音总线音量 |
| `KAG.set_se_bus_volume(volume)` | volume:number(0-1) | SE 总线音量 |
| `KAG.set_bus_volume(bus, volume)` | bus:string("bgm"/"voice"/"se"), volume:number | 设置总线音量 |
| `KAG.get_bus_volume(bus)` | bus:string | 查询总线音量 |
| `KAG.set_global_volume(volume)` | volume:number(0-1) | 主音量 |
| `KAG.get_global_volume()` | — | 查询主音量 |
| `KAG.audio_get_position(bus)` | bus:string | 播放位置(秒) |
| `KAG.audio_get_length(bus)` | bus:string | 音轨长度(秒) |
| `KAG.audio_fade_volume(bus, target, time)` | bus:string, target:number, time:number | 平滑音量过渡 |
| `KAG.flush_wave_cache()` | — | 释放波形内存 |
| `KAG.replay_voice()` | — | 重播最后语音 |
| `KAG.is_voice_playing()` | — | 语音播放中？ |
| `KAG.is_bgm_playing()` | — | BGM 播放中？ |
| `KAG.get_active_voices()` | — | 活跃语音数 |
| `KAG.set_listener(posX,posY,posZ, atX,atY,atZ)` | posX,posY,posZ,atX,atY,atZ:number | 更新3D监听器位置 |

## System（系统控制）

| 函数 | 参数 | 说明 |
|------|------|------|
| `KAG.wait(ms)` | ms:number | 等待毫秒 |
| `KAG.jump(label)` | label:string | 跳转到场景标签 |
| `KAG.call(scene)` | scene:string | 调用子场景（可return） |
| `KAG.return()` | — | 从子场景返回 |
| `KAG.set_variable(name, value)` | name:string, value:any | 设置全局变量 |
| `KAG.get_variable(name)` | name:string | 获取全局变量 |
| `KAG.if_var(name, op, value, label)` | name:string, op:string("<"/">"/"=="/"!="), value:any, label:string | 条件跳转 |
| `KAG.rand(min, max)` | min:number, max:number | 随机整数 |
| `KAG.save(slot)` | slot:number | 存档到槽位 |
| `KAG.load(slot)` | slot:number | 从槽位读档 |
| `KAG.quick_save(slot?)` | slot:number(1) | 快速存档(F5) |
| `KAG.quick_load(slot?)` | slot:number(1) | 快速读档(F6) |
| `KAG.auto_save()` | — | 自动存档 |
| `KAG.get_save_info(slot)` | slot:number | 获取存档信息 |
| `KAG.set_encryption_key(key)` | key:string | 设置存档加密密钥 |
| `KAG.clear_encryption_key()` | — | 清除加密密钥 |
| `KAG.log(level, msg)` | level:string("trace"/"debug"/"info"/"warn"/"error"/"fatal"), msg:string | 引擎日志 |

## Transition（转场效果）

| 函数 | 参数 | 说明 |
|------|------|------|
| `KAG.transition(type, time)` | type:string("fade_in"/"fade_out"/"crossfade"/"wipe_left"/"wipe_right"/"dissolve"), time:number | 执行转场 |
| `KAG.set_transition_rule(from, to, type)` | from:string, to:string, type:string | 设置自动转场规则 |

## VFX（视觉效果）

| 函数 | 参数 | 说明 |
|------|------|------|
| `KAG.quake(intensity, duration?)` | intensity:number, duration:number(0.5) | 屏幕震动 |
| `KAG.render("submit_vfx", type, ...)` | type:string, ...:any | 提交视觉特效 |
| `KAG.render("submit_transition", type, time)` | type:string, time:number | 提交转场渲染 |

## Video（视频播放）

| 函数 | 参数 | 说明 |
|------|------|------|
| `KAG.play_video(path, opts?)` | path:string, opts:{loop?:bool, volume?:number} | 播放视频 (pl_mpeg/FFmpeg) |
| `KAG.stop_video()` | — | 停止视频 |
| `KAG.pause_video()` | — | 暂停视频 |
| `KAG.resume_video()` | — | 恢复视频 |
| `KAG.seek_video(ms)` | ms:number | 跳转到时间点 |
| `KAG.wait_video_end()` | — | 等待视频播放完毕 |
| `KAG.is_video_playing()` | — | 视频播放中？ |
| `KAG.get_video_position()` | — | 当前播放位置(ms) |

## MiniGame（3D 小游戏）

| 函数 | 参数 | 说明 |
|------|------|------|
| `mini_game.init(opts)` | opts:{width,height} | 初始化3D场景 |
| `mini_game.spawn_cube(opts)` | opts:{size:{x,y,z}, pos:{x,y,z}, color:{r,g,b}?} | 生成立方体 |
| `mini_game.spawn_sphere(opts)` | opts:{pos:{x,y,z}, radius?, color?} | 生成球体 |
| `mini_game.spawn_plane(opts)` | opts:{size:{w,h}, pos:{x,y,z}, color?} | 生成平面 |
| `mini_game.spawn_floor(opts)` | opts:{size:{x,y,z}, pos:{x,y,z}, color?} | 生成地板 |
| `mini_game.set_camera(opts)` | opts:{pos:{x,y,z}, lookAt:{x,y,z}, fov?} | 设置摄像机 |
| `mini_game.set_material(opts)` | opts:{roughness, metallic, specStr} | 设置PBR材质 |
| `mini_game.set_lighting(opts)` | opts:{dir:{x,y,z,w}, points?:[{pos,color,intensity,range}]} | 设置光照 |
| `mini_game.check_collision(pos, radius)` | pos:{x,y,z}, radius:number | 碰撞检测 |
| `mini_game.cleanup()` | — | 清理3D资源 |

## Live2D（条件编译）

| 函数 | 参数 | 说明 |
|------|------|------|
| `KAG.live2d_load(id, path)` | id:string, path:string | 加载Live2D模型 |
| `KAG.live2d_remove(id)` | id:string | 移除模型 |
| `KAG.live2d_set_animation(id, name, loop?)` | id:string, name:string, loop:bool(false) | 设置动画 |
| `KAG.live2d_set_expression(id, name)` | id:string, name:string | 设置表情 |

## 沙箱约束

禁用: `loadfile`, `dofile`, `os.execute`, `os.remove`, `os.rename`, `os.exit`, `io.open`, `io.popen`
`require()` 仅返回预加载模块。
Debug 库只读子集。
AI strict 模式: Render/DevCore/Debug 白名单代理。
完整规则: `scripts/sandbox.lua`
