# 日本系视觉小说引擎最新资料调研报告（2025-2026）

> 调研员：DSH 市场调研子 agent（session c2fa6920-604c-4413-9a9b-bb5d6f5a91a4）
> 调研日期：2026-08-16（web_search 数据当时时效）

---

## 重要前置纠正（务必先读）

**用户需求描述中存在一处事实性错误**：**NVL Maker 并非 Unity 插件**（用户原话写'NVLMaker（NVL Maker，Unity 插件）'）。实际调研确认，**THE NVL Maker 是基于吉里吉里/KAG3 的免费可视化 AVG 制作工具**，其官方手册直接使用 KAG3 文档（nvlmaker.net/manual/docs/kag3doc/），运行环境依赖吉里吉里引擎（KiriKiri）。本项目（Caesura/AmeKAG）与 NVL Maker 的亲缘关系比之前设想的更近——它和本引擎同样建立在 KAG 标签语言之上。下文按事实（KAG3 衍生）为准报告。

---

## 一、吉里吉里 / KAG3（KiriKiri / KAG3 / KiriKiri Z）

| 维度 | 内容 |
|---|---|
| **技术栈** | C++/Win32 原生引擎，自研 TJS 脚本语言虚拟机（KiriKiri Z 用 C++ 重写核心，跨平台化）。图形后端：经典吉里吉里2 用 DirectX（DX9 传统），吉里吉里Z（krkrz）改用更现代的图形抽象并支持 OpenGL/DirectX 多后端；社区有 krkrsdl2（SDL2 版）与 KrKr2-Next（Flutter+ANGLE，支持 Metal/Vulkan/D3D11）等现代重写分支。资源封装为 .xp3 归档（可拼接进 exe 分发），图像用 .tlg 格式（自带 alpha 通道）。来源：[DBpedia/KiriKiri](https://fragments.dbpedia.org/2014/en)、[krkrz/krkrz](https://relatedrepos.com/gh/krkrz/krkrz)、[KrKr2-Next](https://github.com/reAAAq/KrKr2-Next) |
| **脚本语言** | 双层：底层 TJS（类 ECMAScript/JavaScript 的面向对象脚本，可扩展 .dll 插件）；上层 KAG（KiriKiri Adventure Game System）标签标记语言，[tag attr=value] 语法类似 XML/HTML。KAG 又演化出 KAG3（原著）与 KAGeXpress 3.0（魔改衍生）。本项目（Caesura/AmeKAG）的 [if]/[eval]/[jump]/[call] 等 KAG 命令即源于此，属于 KAG3 标签语言的兼容实现。来源：[DBpedia](https://fragments.dbpedia.org/2014/en)、[KAGeXpress 3.0 Next Gen（CnGal）](https://www.cngal.org/articles/index/9059)、[antlr kirikiri-tjs README](https://raw.githubusercontent.com/antlr/grammars-v4/c4577134a58a0aedf0b4b986cdb905a76570b11c/kirikiri-tjs/README.md) |
| **平台支持** | 官方吉里吉里2/吉里吉里Z 仅 **Windows**（且早期需日文 locale / AppLocale 才能运行多数老游戏；Unicode 版不再需要）。**macOS/Linux/Android/iOS 均非官方支持**，需第三方：Android/iOS 靠 **kirikiroid2**（开源的 KiriKiri2 安卓模拟器，有 iOS 移植分支），桌面 Linux/macOS 靠 krkrsdl2 或 KrKr2-Next 等重写。**无 Web 导出**。来源：[DBpedia](https://fragments.dbpedia.org/2014/en)、[kirikiroid2 Skich](https://skich.app/games/kirikiroid2)、[krkrsdl2/krkrrel-ng](https://relatedrepos.com/gh/krkrsdl2/krkrrel-ng) |
| **表现力** | Live2D：吉里吉里曾有第三方 Live2D 插件（KAGeXpress 时代流行，配合 .tlg 精灵图）。视频：经典版支持 MPEG-2；吉里吉里Z 支持更多格式（.mpg/.avi/部分 .mp4 视编码）。转场/滤镜：支持叠化（crossfade）、滑入滑出等 2D 层转场，无现代 GPU 滤镜（如 bloom/shader 后处理）——这是其表现力天花板。来源：[CnGal KAGeXpress](https://www.cngal.org/articles/index/9059)、[GitCode 吉里吉里Z 技术跃迁](https://blog.gitcode.com/a9aa2045f2668f9775b44ac6b4d73107.html) |
| **音频** | BGM/SE/语音全支持；格式主推 Ogg Vorbis（.ogg）与 WAV，语音常用 Ogg。不支持现代流式音频格式、无杜比/多声道原生意。来源：吉里吉里官方文档体系 + [nvlmaker KAG3 文档](https://www.nvlmaker.net/manual/docs/kag3doc/contents/Intro.html) |
| **存档系统** | 经典 KAG3 提供固定槽位存档（slot-based），存档与配置写入 .xp3 同目录或用户目录。**加密：无内置强加密**——资源 .xp3 可被解包（社区有大量解包工具），存档明文可改。保护靠商业授权版的可选加密插件（非开源部分）。吉里吉里Z 保留了基本存档接口。来源：[rpg.blue / KAG3 文档](https://www.nvlmaker.net/manual/docs/kag3doc/contents/Distribute.html) |
| **编辑器/工具链** | KAG3 自带文本编辑器与命令行编译（.ks 源 → 内部字节码），无统一可视化 IDE；**KAGEditor** 是社区/KAGeXpress 体系的标签编辑器。现代流程普遍用 KAGeXpress（改版 KAG3）。NVL Maker 在其上做的可视化编辑器更接近'工具链'定位。来源：[CnGal KAGeXpress](https://www.cngal.org/articles/index/9059) |
| **Web 导出** | **无官方 Web 导出**。KAGWeb 是早期实验性项目，未成气候、未商业落地。HTML5 方向的官方支持近乎为零——这是吉里吉里体系（含 NVL Maker）在 2020s 后衰退的关键原因之一，也被 Tyrano/其它 Web 引擎抢占。来源：[GitCode 吉里吉里Z](https://blog.gitcode.com/9b2a475e72c886b3713c752d16517b83.html) |
| **性能特征** | **无公开基准**。合理估计（标注来源：依据架构推断）：2D 层渲染在 DX9 时代即可跑满 60fps，老硬件友好；内存占用小（长文本 AVG 资源以图片为主）；加载依赖 TJS 脚本解析 + .xp3 随机读取，通常 1~3 秒起。跨平台重写（KrKr2-Next=Flutter ANGLE）目标现代 GPU 但为实验项目。来源：推断 + [KrKr2-Next README](https://github.com/reAAAq/KrKr2-Next) |
| **许可** | **开源 GPL**，但**商业授权需另行购买**（商业游戏若不公开修改需购买商业许可证，此为吉里吉里的经典授权模式——GPL 开源 + 付费闭源商用）。同人/免费游戏用 GPL 版自由。NVL Maker 在其上加了自己的免费授权约定。来源：[DBpedia](https://fragments.dbpedia.org/2014/en)、[维基百科-吉里吉里](https://zh.wikipedia.org/wiki/%E5%90%89%E9%87%8C%E5%90%89%E9%87%8C) |
| **社区规模** | **日本同人圈绝对统治者**（与 NScripter 并列两大老牌）。代表作品：**TYPE-MOON 的《Fate/stay night》《Fate/hollow ataraxia》用吉里吉里**（DBpedia 明确记载）；**《海市蜃楼之馆（The House in Fata Morgana）》也用吉里吉里/KAG**（PCGamingWiki、萌娘百科、bgmmi 等确认）。Steam 上存量巨大——大量移植到 Steam 的日系 VN 都是吉里吉里引擎（Fata Morgana 即登录 Steam）。中文社区有 kirikiroid2 生态圈。来源：[DBpedia](https://fragments.dbpedia.org/2014/en)、[PCGamingWiki Fata Morgana](https://www.pcgamingwiki.com/w/index.php?title=The_House_in_Fata_Morgana&oldid=770278)、[萌娘百科-海市蜃楼之馆](https://moegirl.uk/index.php?title=%E6%B5%B7%E5%B8%82%E8%9C%83%E6%A5%BC%E4%B9%8B%E9%A6%86)、[bgmmi Fata](https://bgmmi.anibt.net/subject/73806) |
| **最新版本** | 三个分支：**吉里吉里2/KAG3**（经典，基本停止开发，仅维护）；**吉里吉里Z（krkrz）**——社区主导的现代重写，跨平台目标活跃维护中（GitHub krkrz 组织），发布节奏较活跃但版本号营销不透明；**吉里吉里Next（KAGeXpress 3.0 'Next Gen'）**——2024-2025 间针对新生代创作者推出的改版（见 CnGal 文章）。无官方 Linux/macOS 稳定版。来源：[krkrz/kirkrz](https://relatedrepos.com/gh/krkrz/krkrz)、[krkren/kag3](https://github.com/krkren/kag3)、[CnGal KAGeXpress Next Gen](https://www.cngal.org/articles/index/9059) |

---

## 二、THE NVL Maker（NVL Maker）

| 维度 | 内容 |
|---|---|
| **技术栈** | **基于吉里吉里（KiriKiri）的免费 AVG 制作工具**，作者 VariableD（镜像 GitHub: nextgal/the-nvl-maker）。运行时即吉里吉里引擎（Windows 原生），在其上封装了可视化界面与命令模板。**【纠错】不是 Unity 插件**。当前主导版本为 NVL Maker 3（官方站 nvlmaker.net），最新 patch 3.15（2017 年发布，见 rpg.blue）。来源：[GitHub nextgal/the-nvl-maker](https://github.com/nextgal/the-nvl-maker)、[rpg.blue NVL Maker 3.15](https://rpg.blue/forum.php?mod=viewthread&tid=231702)、[CnGal THE NVL Maker](https://www.cngal.org/entries/index/1170) |
| **脚本语言** | **KAG3 标签语言**（官方手册直接引用 KAG3 文档），并在其上定义了自己的扩展命令（NVL Maker 特色的简化 AVG 模板命令，如对话框/立绘/选项的封装）。源码脚本生成 .ks 文件。因此其脚本与吉里吉里 KAG3 **语义兼容**，与本项目（Caesura/AmeKAG）的 KAG 语言同族。来源：[NVL Maker KAG3 文档 Intro](https://www.nvlmaker.net/manual/docs/kag3doc/contents/Intro.html)、[NVL Maker basic_kiri_kag_nvl](https://www.nvlmaker.net/manual/docs/basic_kiri_kag_nvl.html) |
| **平台支持** | 继承吉里吉里：**仅 Windows**（官方）。无 Android/iOS/macOS/Linux/Web 官方支持（同吉里吉里，可借 kirikiroid2 跑安卓）。**无 Web 导出**。来源：[NVL Maker 发布教程](https://www.nvlmaker.net/manual/docs/kag3doc/contents/Distribute.html) |
| **表现力** | 同吉里吉里上限：2D 层叠化/移动转场，支持 Live2D（需吉里吉里 Live2D 插件，NVL Maker 中文社区有教程但非内置标准能力）、MPEG/部分视频、无现代 GPU 后处理滤镜。NVL Maker 的价值在于开箱即用的模板化表现（立绘差分、语句框、选项），而非底层先进渲染。来源：[rpg.blue NVL Maker](https://rpg.blue/forum.php?mod=viewthread&tid=231702&extra=page%3D27&ordertype=1) |
| **音频** | 同吉里吉里：BGM/SE/语音齐备，Ogg/WAV 为主。NVL 提供音频管理模板。无现代格式原生支持。来源：推断（继承 KiriKiri 栈）+ [KAG3 文档](https://www.nvlmaker.net/manual/docs/kag3doc/contents/Intro.html) |
| **存档系统** | 继承 KAG3 槽位存档，NVL 提供可视化存档界面模板。**无强加密**（同其下的吉里吉里）。来源：推断（KAG3 栈） |
| **编辑器/工具链** | **此即 NVL Maker 的核心卖点**——可视化编辑器（游戏脚本编辑器 + 项目管理 + 预览运行），面向零代码创作者（被形容为'AVG 制作软件免费编辑器'）。内建大量模板（古风/现代/恐怖主题界面）。使用 .ks 脚本引擎执行。来源：[rpg.blue](https://rpg.blue/forum.php?mod=viewthread&tid=231702)、[360百科 THE NVL Maker](https://baike.so.com/doc/30120533-31747390.html)、[CnGal](https://www.cngal.org/entries/index/1170) |
| **Web 导出** | **无 Web 导出**（继承吉里吉里）。来源：[NVL Maker 发布教程](https://www.nvlmaker.net/manual/docs/kag3doc/contents/Distribute.html) |
| **性能特征** | **无公开基准**。合理估计（标注来源：架构推断）：继承吉里吉里 2D 栈，轻量、老硬件可跑；编辑器端因含可视化 UI 稍重。加载秒级。 |
| **许可** | **完全免费**（面向同人/免费游戏），无商业授权门槛——但底层吉里吉里仍是 GPL + 商业许可模式，NVL 的免费承诺通常限于使用其工具与社区素材生态，商业闭源游戏仍需注意吉里吉里授权。作者提供教程与汉化资源免费。来源：[NVL 官方 nvlmaker.net](https://www.nvlmaker.net/2014/about.html)、[CnGal](https://www.cngal.org/entries/index/1170) |
| **社区规模** | **中文 GalGame 同人圈主力之一**，面向零基础中文创作者（原创祭/教程/模板生态活跃，见 nvlmaker.net 原创祭、rpg.blue 板块）。日本圈也有使用但影响力弱于原生吉里吉里。代表作品以中文同人 AVG 为主，Steam 上也有部分用 NVL 制作的独立作品（无精确统计）。来源：[NVL 原创祭](https://www.nvlmaker.net/2014/about.html)、[rpg.blue](https://bbs2.kdays.net/read/24687) |
| **最新版本** | **THE NVL Maker 3.15**（2017 年，官方可下载；作者长期未大版本更新，社区仍在使用）。官方站 nvlmaker.net，GitHub 有镜像持续托管。维护状态：**停滞/社区维护**。来源：[GitHub nextgal/the-nvl-maker](https://github.com/nextgal/the-nvl-maker)、[rpg.blue 3.15](https://rpg.blue/forum.php?mod=viewthread&tid=231702) |

---

## 三、TyranoBuilder / TyranoScript（Tyrano）

| 维度 | 内容 |
|---|---|
| **技术栈** | **纯 Web 技术栈**（HTML5/CSS/JavaScript + jQuery/Raycaster 等），作者 shikemokumk（日本）。运行时为浏览器 JS 引擎，脚本解析器是 TyranoScript（标签式）。跨平台能力完全依赖 Web 技术栈（可打包成 Electron/桌面壳）。来源：[TyranoBuilder 官网](https://tyranobuilder.com/)、[web-reference 开发者目录 TyranoBuilder](https://web-reference.org/en/catalog/development-tools/visual-novel/tyranobuilder/)、[SteamDB](https://steamdb.info/app/3634660) |
| **脚本语言** | **TyranoScript**：独有标签语言，[tag] 语法风格，命令集独立（非 KAG 同源）。提供 TyranoBuilder 的可视化节点编辑在后台生成/编辑这些脚本。TyranoScript 与 Game Creator System（GCS）分支。它不是 KAG 兼容（区别于 NVL Maker）。来源：[TyranoBuilder 官网](https://tyranobuilder.com/)、[steamlease TyranoStudio](https://steamlease.cn/gameColumnDetail/46924) |
| **平台支持** | **最佳跨平台**：Windows/macOS/Linux（桌面壳）+ Android/iOS（打包）+ **Web/HTML5 导出（核心优势）**。靠 Unity/Electron/浏览器多目标打包。来源：[TyranoBuilder 官网](https://tyranobuilder.com/)、[TyranoBuilder 导出教程](https://tyranobuilder.com/tutorials/all-done-time-to-export)、[Steam 讨论（浏览器方式）](https://steamcommunity.com/app/345370/discussions/1/611702631217761708) |
| **表现力** | Live2D：**经由 Web 原生支持 Live2D SDK（JS 版）**——是其大卖点（Web 端 Live2D 集成成熟，比吉里吉里更容易）。视频：HTML5 mpeg 播放依赖 Web 编解码。转场/滤镜：CSS3/WebGL 级转场与滤镜，表现力**强于吉里吉里 2D 栈**（可做 shader 效果）。来源：[TyranoBuilder 官网](https://tyranobuilder.com/)、[web-reference](https://web-reference.org/en/catalog/development-tools/visual-novel/tyranobuilder/) |
| **音频** | BGM/SE/语音齐备，Web 友好格式（Ogg/MP3/AAC，视浏览器）。来源：Tyrano 体系通用能力（推断）。 |
| **存档系统** | Web LocalStorage/indexedDB 存档（Web 版天然用浏览器存储），桌面版本地文件。槽位式。**无强加密**（Web/JS 可被解包读取，是 Web 引擎通病）。来源：[TyranoBuilder 导出教程](https://tyranobuilder.com/tutorials/all-done-time-to-export) |
| **编辑器/工具链** | **TyranoBuilder = 可视化节点式编辑器**（拖拽式 VN 制作，视觉编程），相对新手友好；**TyranoScript 为纯脚本直写路径**。官方提供 TyranoStudio（Steam 上亦有脚本工具）。工具链含模板库、素材包。来源：[TyranoBuilder 官网](https://tyranobuilder.com/)、[Steam TyranoBuilder](https://store.steampowered.com/app/345370/TyranoBuilder_Visual_Novel_Studio) |
| **Web 导出** | **是——这是 Tyrano 相对吉里吉里/NVL 的决定性优势**：官方一键导出 HTML5/Web 版本，可直接放网站/itch.io 在线玩。Tyrano 是日本 VN 圈 Web 导出最成熟者。来源：[TyranoBuilder 导出教程](https://tyranobuilder.com/tutorials/all-done-time-to-export)、[Steam 浏览器讨论](https://steamcommunity.com/app/345370/discussions/1/611702631217761708) |
| **性能特征** | **无公开基准**。合理估计（标注来源：Web 栈推断）：内存消耗比原生引擎高（浏览器/JS 运行时），大型资源（高清立绘+长语音）可能产生加载压力；渲染受浏览器性能影响，老设备/低内存易卡；但跨平台一致性高、迭代快。 |
| **许可** | **免费版可商用**——TyranoBuilder/Studio 免费提供（Steam 免费下载），游戏成品可自由发布/出售，无明显授权门槛（官方商业化友好，区别于吉里吉里付费商用）。免费版功能完整（无导出水印限制，可全平台+Web 导出；部分高级素材/模板为付费 DLC）。来源：[Steam TyranoBuilder](https://store.steampowered.com/app/345370/TyranoBuilder_Visual_Novel_Studio)、[Steam 商业用途讨论](https://steamcommunity.com/app/345370/discussions/0/485622866449182036) |
| **社区规模** | **日本同人圈现代主力 + 全球（尤其英文/手游向）新兴**。得益于免费+Web 导出+Live2D 原生支持，是 2020s 日本 VN 同人圈增长最快的引擎之一。Steam 上**数千款**用 Tyrano 制作的独立/同人 VN（依赖 Steam 搜索'TyranoBuilder/TyranoGameMaker'标签与存量大；无法精确计数但显著）。代表：大量手机/浏览器向 VN、Steam 独立 VN。最新 V3.05 更新（2025-05-30 发版，见 SteamDB）。来源：[SteamDB 345370](https://steamdb.info/app/345370)、[SteamDB patchnote 18641343](https://steamdb.info/patchnotes/18641343)、[GameHypes V3.00 beta](https://gamehypes.com/news/major-update-beta-release-of-version-3-00-now-available) |
| **最新版本** | **TyranoBuilder Visual Novel Studio V3.05**（2025-05-30，Steam 更新；大版本 3.00 于 2024 起 beta，2025 稳定到 V3.05）。**维护活跃**（2025 年仍发版）。另有 TyranoScript 单独库 + TyranoGameMaker（新品牌）。来源：[SteamDB patchnote](https://steamdb.info/patchnotes/18641343)、[GameHypes](https://gamehypes.com/news/major-update-beta-release-of-version-3-00-now-available) |

---

## 四、横向对比 & 对本项目（Caesura/AmeKAG）的启示

| 指标 | 吉里吉里/KAG3 | THE NVL Maker | Tyrano |
|---|---|---|---|
| 跨平台 | 仅 Win（第三方补） | 仅 Win | **全平台+Web** |
| Web 导出 | 无 | 无 | **原生优势** |
| KAG 语言兼容 | 原生 KAG3 | **KAG3（同族）** | 独立 TyranoScript |
| Live2D | 第三方插件 | 同吉里吉里 | Web 原生 SDK |
| 免费商用 | GPL，闭源需买授权 | 免费 | **免费无门槛** |
| 维护状态 | Z 分支活跃，经典停滞 | 停滞 | **活跃** |
| 代表作品 | Fate/SN、**海市蜃楼之馆** | 中文同人 AVG | 数千 Steam 独立 VN |

**对本项目直接相关结论**：
1. **KAG 语言兼容性**：Caesura/AmeKAG 的 [tag] KAG 命令体系（[if]/[eval]/[jump]/[call] 等 118 命令）对标的正是**吉里吉里 KAG3 标签语言**——本项目是 KAG3 的现代（Lua+cmake+bgfx）独立重实现，与 NVL Maker 同属'KAG 家族'。差异是：NVL 依赖 Windows 吉里吉里运行时，**本引擎实现了 KAG3 的现代跨平台化**（正好填补吉里吉里没有 Web/跨平台的空缺，且集成了 Tyrano 缺失的原生化体验）。
2. **市场缺口**：吉里吉里（跨平台差、无 Web）+ NVL（Win-only）+ Tyrano（强 Web 但弱原生深度）三者各有短板。Caesura 以'KAG3 语言兼容 + bgfx 跨平台 + Lua 扩展'切入，定位是回归 KAG 传统同时补现代跨平台能力。
3. **Fata Morgana 归属确认**：《海市蜃楼之馆》确认走吉里吉里/KAG 栈——即与本项目同语言家族，是可参考的适配目标。

> **数据时效说明**：以上为 2026-08-16 web_search 可得数据；版本号以源站（SteamDB、GitHub、官方站）当年记录为准，个别版本号可能滞后于最新 release。