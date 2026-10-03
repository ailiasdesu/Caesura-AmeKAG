# GitHub CI 接续：真实浏览器基准与验证前提

用户已授权将全部改动推送 GitHub，并在 CI 全绿后合并 master。PR 为 [#27](https://github.com/caesura-team/Caesura-AmeKAG/pull/27)。本记录提交时托管复验尚待完成；最终结果以该 PR 最新 head 的检查和合并记录为准。

首轮 [CI 37109375408](https://github.com/caesura-team/Caesura-AmeKAG/actions/runs/37109375408) 在 `4e095830` 上保留了三个失败：

1. macOS 完整 C++ 1601/1601 通过，CTest 的 cleanup-failure 反证未先建立可信 terminal 前提，合法的 0.4 秒进度超时可能抢先成为首错。测试现复用真实 child/launcher/cleanup 与已有 publication barrier，再让观察器跨原期限并注入清理返回失败；原期限及失败断言不变。本地完整24项通过，独立审查通过。
2. Linux 原生及完整 Web 通过，平台矩阵生成器拒绝旧代码锚点。只同步 review anchor 和生成文档，保留每个平台原有执行提交、日期和 NOT_REVERIFIED 范围。
3. Windows 严格原生验证通过，Web 主故事吞吐约0.0615 tokens/ms，低于原0.08。首轮其他结果不被后续修复覆盖。

## 故事吞吐的测量宿主修正

独立Windows对照中，Node timeout16、interval首触发、原始jsdom rAF的中位等待分别约30.802、30.826、30.871 ms；150次等待本身约4.29秒，已经超过339 token在原门槛下的总预算。真实Chrome的150次未替换rAF对照约1.08秒，中位回调间隔5.6ms；不将原本假设的16.7ms当作实测值，也不据此认定唯一系统设置原因。

保留同名必跑故事用例、同一源码/资源、一次预热和三次完整采样、原0.8 ticks/ms与0.08 tokens/ms门槛，将测量移入真实Chromium的生产Wasmoon/bridge/DomRenderer。所有平台使用同一路径；缺少已有浏览器或Python即失败，无jsdom回退。未替换rAF、未改浏览器时钟、未安装新依赖或更改系统计时器。

原jsdom基线的音频能力不可用。真实浏览器默认有音频，直接运行会正常返回WAIT_AUDIO，且后续audio-tick会改变原单次pump计数。因此基准通过公开audioContext参数注入创建后已关闭的真实AudioContext，并逐轮检查同一实例、closed状态和真实公开availability=false，保持原基线前提。没有修改场景/音频命令、合成恢复结果或伪造计数；这不是音频正向或完整有声故事吞吐证明。

每个实测样本保持DONE:339:193、5826个调度tick、150次真实呈现。本地定向中位2640ms，约0.128 tokens/ms。新的39项报告/路径反证与完整Web复验均通过；完整结果为57文件、711测试通过、0失败。旧Windows Node/jsdom失败仍保留，本次是测量宿主修正，不是宣称产品代码获得同等倍数提速。

浏览器使用独有profile/CDP和Vite缓存；HTTP路径受限，允许Vite等待尚未发布的自有cache依赖但拒绝越界及链接逃逸。源码manifest包含实际Wasmoon JavaScript/WASM。Python薄入口复用维护中的Windows Job/POSIX进程组所有权；报告要求真实浏览器/launcher退出、endpoint退休、源码稳定、无fallback及完整owner清理，不只接受摘要PASS。

原件留在恢复根的 `ci-pr27-macos-small-01`、`ci-pr27-windows-first-failure.log`、`ci-pr27-targeted-preflight-02`、`ci-web-throughput-readonly-01`、`ci-pr27-real-browser-story-01..04`、`ci-pr27-web-full-green-01`。初次浏览器bootstrap、WAIT_AUDIO等失败也保留；没有重写成通过。

## Linux Vite 路径复验

第二轮 CI `37114535328` 的 macOS Debug 已通过；Linux 的新浏览器故事测试在 bootstrap 阶段失败。Vite 在 POSIX 上通过 `posix.join('/@fs/', absoluteId)` 生成 `/@fs/tmp/...`，其 `fsPathFromId` 会恢复前导斜杠。协调器此前直接对截取后路径调用 `resolve`，误将 Linux 绝对路径解释为仓库相对路径。现按 Vite 规则解码，保留 Windows 绝对盘符和独占缓存的词法、真实路径边界；未扩大任意文件访问范围。

路径夹具改用 Vite 实际的 URL 生成方式，报告新增源码清单摘要校验，失败 stderr 保留有界原始诊断，成功日志记录三个真实样本。最新本地检查为46项通过；真实浏览器故事三个样本均为 DONE:339:193、5826 ticks、150次呈现，中位2875.8ms，原阈值通过，源码稳定且进程树完整清理。原件为 `ci-pr27-real-browser-story-05`。这些是定向验证，最终托管全量结果仍须以最新提交 CI 为准。
