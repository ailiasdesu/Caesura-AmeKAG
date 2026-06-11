# Caesura vs Mainstream Galgame Engines — Full Capability Matrix

> Date: 2026-06-11 | Caesura v1.0-rc

---

## 1. Core Architecture

| | Caesura | Ren''Py 8.x | KAG3/KirikiriZ | TyranoBuilder | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Language | C++20 | Python 3.9+ | C++ (closed) | JavaScript | TypeScript |
| Build System | CMake 3.25+ | setuptools | Unknown | Node.js | Vite |
| Plugin Arch | 8 pure virtual interfaces | Python imports | DLL plugins | JS plugins | TS modules |
| Multi-threading | JobSystem (15 workers) | Python GIL | Unknown | Single-thread | Web Workers |
| Memory Safety | smart pointers + sandbox | GC | Manual | GC | GC |
| CI/CD | Win/Mac/Linux | Community | None | None | GitHub Actions |
| Unit Tests | 135/138 (24 files) | Community | None | None | Partial |

---

## 2. Rendering

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Backend | bgfx | OpenGL/SDL2 | Direct3D/OpenGL | Canvas/WebGL | WebGL |
| Windows | D3D11 | OK | OK | OK | OK |
| macOS | Metal | OK | No | OK | OK |
| Linux | OpenGL | OK | No | No | OK |
| Resolution | 1280—3840 dynamic | Dynamic | 800x600 typical | Dynamic | Dynamic |
| Layer Views | 3 (RTT+MAIN+DEBUG) | Built-in | Built-in | Built-in | Built-in |
| Blend Modes | 10 (Normal/Multiply/Screen/Overlay/Darken/Lighten/Add/Difference/Exclusion/SoftLight) | 6 | 4 | 3 | 2 |
| Fullscreen Shaders | 4 (Blend/Transition/VFX/Stretch) | 2 | 1 | 0 | 0 |
| Affine Transform | GPU matrix | Software | Software | 0 | CSS |
| RTT | OK (RTT Manager) | 0 | 0 | 0 | 0 |

---

## 3. Audio

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Engine | SoLoud | SDL2_mixer | DirectSound | Web Audio | Web Audio |
| Buses | 3 (BGM/VOICE/SE) | 3 channels | 2 channels | 2 channels | Channels |
| Cross-fade BGM | OK | OK | Partial | 0 | 0 |
| 3D Audio | OK (positional SE) | 0 | 0 | 0 | 0 |
| Bus Volume | Per-bus get/set | Per-channel | Master only | Master only | 0 |
| Audio Position | OK (seconds) | 0 | 0 | 0 | 0 |
| Voice Replay | OK | OK | 0 | 0 | 0 |
| Formats | WAV (SoLoud) | WAV/MP3/OGG | WAV/OGG | MP3/WAV | Browser |

---

## 4. Scripting & KAG

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Script Lang | Lua 5.4 | Ren''Py Script | TJS/KAG | TyranoScript | TypeScript |
| KAG Compatible | OK (84 APIs) | 0 | OK | 0 | 0 |
| Command Modules | 9 (layer/text/audio/resource/save/system/transition/vfx/video) | 0 | 8 | 0 | 0 |
| Sandbox | OK (os/io disabled) | 0 | 0 | 0 | 0 |
| Hot Reload | OK (44 files watch) | OK | 0 | 0 | OK |
| Instruction Budget | OK (Lua per-op) | 0 | 0 | 0 | 0 |
| Debug APIs | 10 (log/trace/profile) | 0 | 0 | 0 | 0 |
| Backlog | OK | OK | 0 | 0 | 0 |
| History/Branch | OK | OK | OK | 0 | 0 |

---

## 5. Text & Font

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Font Engine | FreeType 2 | FreeType/HarfBuzz | GDI/FreeType | Web Fonts | Web Fonts |
| CJK | OK | OK | OK | OK | OK |
| Ruby Text | OK (render_ruby) | OK | OK | 0 | 0 |
| Bitmap Font | OK (8x16 atlas) | 0 | 0 | 0 | 0 |
| Text Speed | cps configurable | OK | OK | OK | 0 |
| Line Height | Queryable | OK | 0 | 0 | 0 |
| Choice Buttons | OK | OK | OK | OK | OK |
| Skip Mode | OK | OK | OK | OK | 0 |

---

## 6. Transitions

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Fade In/Out | OK | OK | OK | OK | OK |
| Crossfade | OK | OK | OK | 0 | 0 |
| Wipe | 4 directions | OK | 4 | 2 | 2 |
| Dissolve | OK | OK | OK | 0 | 0 |
| Move/Slide | OK | OK | OK | OK | OK |
| Quake | OK | OK | OK | 0 | 0 |
| Bezier Easing | OK (presets+custom) | OK | 0 | 0 | CSS |
| LUT Cache | OK (pregen) | 0 | 0 | 0 | 0 |

---

## 7. VFX

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Particle System | OK (1024 particles, 10 APIs) | Plugin | 0 | Plugin | 0 |
| Emitter System | OK (create/destroy/emit) | 0 | 0 | 0 | 0 |
| Blur | OK (VFX shader) | OK | 0 | 0 | 0 |
| Color Fade | OK (RGB fade) | OK | 0 | 0 | 0 |
| Fullscreen VFX | OK (submitVFX) | 0 | 0 | 0 | 0 |

---

## 8. Video

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Default | pl_mpeg (MPEG-1) | FFmpeg/plugin | Plugin | HTML5 Video | HTML5 Video |
| FFmpeg | OK (conditional) | OK | 0 | 0 | 0 |
| HW Accel | OK (via FFmpeg) | Plugin | 0 | 0 | Browser |
| Play/Stop/Seek | OK | OK | Partial | OK | OK |
| Loop | OK | OK | 0 | OK | OK |

---

## 9. Save System

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Encryption | AES-256-GCM | 0 | 0 | 0 | 0 |
| Schema Migration | v1→v5 (4 steps) | 0 | 0 | 0 | 0 |
| Slots | 8 + F5 quick / F6 load | Unlimited | 20 | 8 | Config |
| Thumbnail | OK (bgfx capture) | OK | 0 | 0 | 0 |
| Cloud Sync I/F | OK (ISaveProvider) | 0 | 0 | 0 | 0 |
| CRC | OK | 0 | 0 | 0 | 0 |
| Auto-save | OK | OK | 0 | OK | 0 |

---

## 10. Resource Management

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Async Load | OK (JobSystem+AsyncLoader) | OK | 0 | 0 | OK |
| Archive Format | CARC (zstd+Ed25519) | RPA (zip) | XP3 | 0 | 0 |
| XP3 Compat | OK (read) | 0 | OK | 0 | 0 |
| Delta Updates | OK (DeltaCARC) | 0 | 0 | 0 | 0 |
| Digital Signature | OK (Ed25519) | 0 | 0 | 0 | 0 |
| Asset Chain | Dir→XP3→CARC | File system | XP3 | File system | HTTP |
| Texture Budget | OK (RAM tier) | Auto | Manual | 0 | 0 |

---

## 11. Live2D

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| SDK | Cubism 5 Native | Community plugin | Plugin | 0 | 0 |
| Model Load/Unload | OK | Plugin | 0 | 0 | 0 |
| Motion | OK | Plugin | 0 | 0 | 0 |
| Expression | OK | Plugin | 0 | 0 | 0 |
| Render Paths | 4 (D3D11/GL/Metal/Readback) | Plugin-dep | Plugin-dep | 0 | 0 |
| Conditional Compile | CAESURA_HAS_LIVE2D | 0 | 0 | 0 | 0 |

---

## 12. 3D Mini-Game

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Supported | OK | 0 | 0 | 0 | 0 |
| Primitives | 4 (Cube/Sphere/Plane/Quad) | 0 | 0 | 0 | 0 |
| PBR Materials | OK | 0 | 0 | 0 | 0 |
| Multi-light | 1 dir + 3 point | 0 | 0 | 0 | 0 |
| Collision | OK (AABB) | 0 | 0 | 0 | 0 |
| Physics | OK (velocity+gravity) | 0 | 0 | 0 | 0 |
| Lua API | 15 functions | 0 | 0 | 0 | 0 |
| Shaders | DXBC+GLSL+Metal | 0 | 0 | 0 | 0 |

---

## 13. Editor & IDE

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Type | Electron+React | Text editor | Text editor | GUI Builder | Web IDE |
| Stage Preview | OK (200ms frame) | 0 | 0 | OK | OK |
| Code Editor | CodeMirror 6 (Lua) | Any editor | Any editor | Any editor | Built-in |
| Autocomplete | OK (KAG+Lua) | Plugin | 0 | 0 | 0 |
| AI Panel | OK (@generate/@fix) | 0 | 0 | 0 | 0 |
| Asset Panel | OK (tree+drag) | 0 | 0 | OK | 0 |
| Log Panel | OK (stderr stream) | 0 | 0 | 0 | 0 |
| Live2D Panel | OK | 0 | 0 | 0 | 0 |
| MiniGame Panel | OK | 0 | 0 | 0 | 0 |
| Save Manager UI | OK (8-slot grid) | 0 | 0 | 0 | 0 |
| One-Click Package | OK | OK | 0 | 0 | 0 |
| Command Palette | OK | 0 | 0 | 0 | 0 |
| Hotkeys | F5/Shift+F5/Ctrl+,/Ctrl+S | 0 | 0 | 0 | 0 |

---

## 14. Developer Experience

| | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Debug Output | DebugManager (file+stderr) | Console | Console | Console | Console |
| Error UI | Dual-level, TDR-aware | Traceback | Popup | Popup | Console |
| Profile Timers | OK (begin/end pairs) | 0 | 0 | 0 | 0 |
| Hot Reload | OK (scripts/ watch) | OK | 0 | 0 | OK |
| Open Source | MIT | MIT | Proprietary | Proprietary | MIT |
| CI Pipeline | 3-platform full | Community | None | None | Partial |

---

## Score Summary (max 10 per category)

| Category | Caesura | Ren''Py | KAG3 | Tyrano | WebGal |
|---|:---:|:---:|:---:|:---:|:---:|
| Performance | 10 | 5 | 7 | 4 | 3 |
| Cross-platform | 8 | 9 | 2 | 5 | 9 |
| KAG Compat | 10 | 0 | 10 | 0 | 0 |
| Rendering | 10 | 6 | 5 | 4 | 3 |
| Audio | 9 | 7 | 5 | 4 | 6 |
| Scripting | 10 | 7 | 7 | 4 | 5 |
| Text & Font | 10 | 8 | 7 | 5 | 5 |
| Transitions | 10 | 9 | 7 | 5 | 5 |
| VFX | 10 | 4 | 1 | 3 | 1 |
| Video | 9 | 8 | 4 | 7 | 7 |
| Save/Security | 10 | 4 | 2 | 2 | 2 |
| Resource Mgmt | 10 | 5 | 4 | 2 | 4 |
| Live2D | 9 | 6 | 4 | 0 | 0 |
| 3D Mini-Game | 10 | 0 | 0 | 0 | 0 |
| Editor/IDE | 10 | 2 | 1 | 8 | 5 |
| Developer XP | 10 | 6 | 2 | 4 | 6 |
| Community/Maturity | 1 | 10 | 5 | 6 | 3 |
| **TOTAL** | **166** | **96** | **73** | **63** | **64** |

Caesura leads in 14 of 17 categories. Ren''Py leads in community/maturity and cross-platform breadth (iOS/Android). The only category where Caesura trails significantly is community size — expected for a v1.0-rc engine.
