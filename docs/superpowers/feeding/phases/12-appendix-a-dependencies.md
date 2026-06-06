## 附录 A: 依赖关系图 (完整)

```
Phase 0 (骨架: CMake + SDL + bgfx + Lua)
 ├── Phase 1 (脚本引擎: tokenizer/scheduler/flow/CancelToken/GameState)
 │    └── Phase 4 (KAG 标签全集: 20+ tags)
 │         └── Phase 6 (存档/国际化: JSON/i18n/migration)
 ├── Phase 2 (渲染管线: TextureManager/LayerManager/FontRenderer)
 │    └── Phase 5 (高级渲染: 28 blends/LUT/dirty rects/video)
 ├── Phase 3 (音频系统: SoLoud/BGM/SE/Voice/callback)
 ├── Phase 7 (CARC 容器: AES-GCM/Ed25519/Provider Chain/carc_pack)
 │    └── Phase 9 (安全: 沙箱/链式信任/CRL)
 └── Phase 8 (开发工具: 热重载/调试器/ErrorUI) [需 Phase 1-7]
```
