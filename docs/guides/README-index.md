# docs/guides — 指南索引

> 本页索引 `docs/guides/` 下全部指南文件（22 个）。
> 按读者分组：**内容作者**（写剧本/资产/发布）与**引擎开发者**（构建/源码/平台）。
> 数字基线（ROADMAP-200 权威）：C++ 用例 **976** · Lua 主套件 **132** + 孤儿 **24** ·
> 命令契约 **123** · 教程 **16** · C++ 接口 **31**。

---

## 内容作者（做游戏）

| 文件 | 一句话 | 读者 |
|------|--------|------|
| [getting-started.md](getting-started.md) | 从克隆到跑通示例游戏：三平台环境、构建、运行、测试、FAQ | 所有人（第一站） |
| [kag-language-tour.md](kag-language-tour.md) | KAG Neo-Genesis 完整语法：123 命令分类表、表达式管道、五段常用模板 | 剧本作者 |
| [creator-toolchain-and-i18n.md](creator-toolchain-and-i18n.md) | 创作者工具链与统一语义层：kag_semantic.lua、Story Flow 大纲流向图与多语言流水线 | 创作者/工具开发者 |
| [template-quickstart.md](template-quickstart.md) | 用 demo/template 骨架五分钟起一个新的视觉小说项目 | 新作者 |
| [sample-library.md](sample-library.md) | 16 教程 + 示例游戏 + showcase 的清单与覆盖矩阵 | 学习者 |
| [community.md](community.md) | 社区入口、16 步学习路径、参与贡献方式、发布入口 | 新来者 |

## 资产与内容制作

| 文件 | 一句话 | 读者 |
|------|--------|------|
| [asset-pipeline.md](asset-pipeline.md) | 图片/音频/视频/字体格式支持矩阵（对照 src/ 解码器）+ 目录规范 + 资源加载流程 | 美术/音频/内容作者 |
| [i18n.md](i18n.md) | i18n 本地化管线：提取→翻译→回填→验证全流程 + 复数形态 | 本地化译者/作者 |
| [sample-game-assets.md](sample-game-assets.md) | 示例游戏《单程回信》资产审计、复用清单、6 项降级策略、owr_ 键预留 | 内容作者参考 |
| [live2d-setup.md](live2d-setup.md) | Live2D Cubism SDK 集成：下载、CMake 配置、模型放置、渲染路径状态 | Live2D 用户 |

## 打包与发布

| 文件 | 一句话 | 读者 |
|------|--------|------|
| [packaging-ux.md](packaging-ux.md) | 一键打包为静态 Web 站：package_game.sh 全流程 + 产物说明 + itch/Pages/Netlify 分发 | 发布者 |
| [release-process.md](release-process.md) | 桌面 Release 全流程：门禁→changelog→CPack→ZIP 验证→GitHub Release（含流程总览图） | 发布者/维护者 |
| [sample-game-release.md](sample-game-release.md) | 示例游戏双路径发布就绪检查：GitHub Releases vs itch.io + v1.0.0 发布命令序列 | 维护者 |
| [carc-packaging.md](carc-packaging.md) | CARC 加密归档格式：命令行工具、二进制布局、KAG 用法、安全守则 | 分发/加密需求者 |

## 引擎开发者（源码/构建/平台）

| 文件 | 一句话 | 读者 |
|------|--------|------|
| [kag3-import.md](kag3-import.md) | KAG3 脚本导入器：&var/TJS 转换规则、不支持命令报告、退出码 | 移植/工具开发者 |
| [kag3-migration.md](kag3-migration.md) | 完整 KAG3 作品迁移路径：xp3→tlg→音频→.ks→路径重写→验证（6 步流水线 + 样例） | 迁移方/维护者 |
| [xp3-compat.md](xp3-compat.md) | XP3 归档读取器原型：格式研究（魔数勘误/索引/段）、CLI 用法、v2 路线图 | 兼容层开发者 |
| [tlg-compat.md](tlg-compat.md) | TLG5/TLG6 解码器：格式规范、LZSS/Golomb/滤波、工具用法、迁移衔接 | 兼容层开发者 |
| [android-build.md](android-build.md) | Android 交叉编译链：NDK、SDL3 Android 包、构建命令、风险清单 R1–R11、真机清单 | 移动端开发者 |
| [mobile-pipeline.md](mobile-pipeline.md) | 移动端管线：IMobileAdapter 对接、触摸/IME/生命周期、APK 组装、待真机项 | 移动端开发者 |
| [metal-readiness.md](metal-readiness.md) | Metal 后端就绪度审计：初始化路径、postfx 三重降级、D3D 假设、CI 覆盖缺口 | macOS 后端开发者 |
| [cross-platform-verification.md](cross-platform-verification.md) | 三平台×能力域验证矩阵：CI 已验证 vs 待真机 + round 102 风险核对 | 跨平台 QA |
| [sample-game-verification.md](sample-game-verification.md) | 示例游戏端到端验证：verify_sample_game.sh 全流程、headless mock 契约、三结局可达性 | QA/编辑器开发者 |

---

> 文档分类总纲见 AGENTS.md §12；API 参考在 [docs/api/](../../docs/api/)（command-contracts.md 为 123 命令权威自动生成）。
