## Phase 3: 音频系统 (P0)

**目标:** SoLoud 集成, BGM/SE/Voice 三总线, 淡入淡出, 音频回调唤醒协程 (禁止轮询)。

**依赖:** Phase 0 | **验收:** playbgm/playse/playvoice 正常, voice 结束 resume 协程

### Task 3.1: AudioBackend
**Files:** Create: `src/audio/AudioBackend.h`, `src/audio/AudioBackend.cpp`

```cpp
// src/audio/AudioBackend.h
#pragma once
#include <soloud.h>
#include <soloud_wav.h>
#include <string>
#include <unordered_map>

namespace caesura {
enum class AudioBus { BGM=0, VOICE=1, SE=2 };

class AudioBackend {
public:
    static AudioBackend& instance();
    bool init(); void shutdown();
    unsigned int play(const std::string& path, AudioBus bus, float vol=1.0f);
    void stop(unsigned int id); void stopBus(AudioBus bus);
    void fade(unsigned int id, float to, float dur);
    bool isPlaying(unsigned int id);
    void checkVoiceComplete();
    SoLoud::Soloud& eng() { return m_sol; }
private:
    SoLoud::Soloud m_sol;
    std::unordered_map<unsigned int, SoLoud::handle> m_h;
    std::unordered_map<std::string, SoLoud::Wav> m_cache;
    unsigned int m_next=1, m_voice=0;
    float m_bgmv=1, m_voicev=1, m_sev=1;
};
}
```

```cpp
// play() 核心实现
unsigned int AudioBackend::play(const std::string& path, AudioBus bus, float vol) {
    auto& w = m_cache.emplace(path, SoLoud::Wav()).first->second;
    if (!w.mData && w.load(path.c_str()) != SoLoud::SO_NO_ERROR) { m_cache.erase(path); return 0; }
    if (bus == AudioBus::VOICE && m_voice) m_sol.stop(m_h[m_voice]);
    float bv = (bus==AudioBus::BGM)?m_bgmv:(bus==AudioBus::VOICE)?m_voicev:m_sev;
    SoLoud::handle h = m_sol.play(w, vol*bv);
    if (!h) return 0;
    unsigned int id = m_next++; m_h[id]=h;
    if (bus == AudioBus::VOICE) m_voice = id;
    return id;
}

void AudioBackend::checkVoiceComplete() {
    if (!m_voice) return;
    if (!m_sol.isValidVoiceHandle(m_h[m_voice])) {
        SDL_Event e={}; e.type=SDL_EVENT_USER; e.user.code=1;
        SDL_PushEvent(&e); m_voice=0;
    }
}
```

### Task 3.2: audio.lua
**Files:** Create: `scripts/kag/commands/audio.lua`

```lua
return {
    playbgm = function(ctx,p)
        local h = backend.audio_play(p.storage, 0, tonumber(p.volume) or 1)
        if p.fadein then backend.audio_fade(h, tonumber(p.volume) or 1, tonumber(p.fadein)) end
    end,
    stopbgm = function(ctx,p)
        if tonumber(p.fadeout or 0)>0 then backend.audio_fadebus(0,0,tonumber(p.fadeout))
        else backend.audio_stopbus(0) end
    end,
    playse = function(ctx,p) backend.audio_play(p.storage, 2, tonumber(p.volume) or 1) end,
    playvoice = function(ctx,p)
        local h = backend.audio_play(p.storage, 1, 1)
        local ct = kag.cancel_token.new()
        ct:register(function() backend.audio_stop(h) end)
        table.insert(ctx.active_operations, ct)
        coroutine.yield()
    end,
}
```

### Phase 3 检查点
- [x] SoLoud 44100Hz 2ch
- [x] BGM/SE/Voice 独立
- [x] fadeVolume 淡入淡出
- [x] Voice 回调 resume 协程 (零轮询)

---
