# PR #27：Windows Release 性能门禁接续

用户已授权全部推送 GitHub，并在最终提交 CI 全绿后合并 master。本记录提交时，新的托管验收待完成；合并结果以 [PR #27](https://github.com/caesura-team/Caesura-AmeKAG/pull/27) 的最新提交和检查为准。

上一轮 [CI 37122411104](https://github.com/caesura-team/Caesura-AmeKAG/actions/runs/37122411104) 在 `ec9e8ceb` 上通过 Windows/Linux/macOS Debug、Linux/macOS Release、移动端门禁和 Web 最终包。唯一生产者失败是 Windows Release 的合成故事吞吐：三个样本2083.7/2076.1/2025.2ms，中位2076.1ms，低于原2 ticks/ms门槛；最终输入汇总因此失败。浏览器启动修复和真实故事吞吐已通过，历史失败原件保留。

只读实测定位了图层快照转换和结尾 backlog 发布开销。最终补丁包含三项生产优化：图层每次完整扫描后，仅复用内容严格一致的不可变 Lua 编码，构造器仍返回独立值表；24个数值字段的固定边界在模块初始化计算，每个输入仍经过原数值谓词；固定ASCII `{t,x,y}` backlog通过有界编码一次传输，保留原发布时点。完整 `_ctx` 仍按原方式复制。Unicode、NUL、BOM、异常结构、非有限或不安全数值以及超限数据继续走原 reader 和 JSON copy，编码或解析异常明确传播。

快速 backlog 路径保持1MiB输出、256KiB字符串和65536值预算，并保留 Wasmoon 空表投影、旧JSON负零归一和存档恢复顺序。真实Wasmoon反证发现 `math.abs(math.mininteger)` 溢出后，资格检查改为直接比较安全整数上下界。原29项测试在修复前1失败、28通过，修复后全部通过。

最终本地验收：Lua主套件153/153、隔离套件60/60；图层恢复324项检查；真实Wasmoon图层18项；backlog29项与浏览器报告60项；原性能套件12/12；完整Web58文件、770/770。所有必需进程实际退出0，源码输入稳定，拥有进程树清理完成。独立审查接受最终冻结版本。

原一次预热、三个样本、2 ticks/ms与1.5 tokens/ms门槛保持。合成故事仍完成3000 token、4000 ticks、64个回滚点和2000个已读标记；单文件复验中位1291.5ms，完整Web复验中位1657.2ms。不同运行存在波动，这些本地结果不替代托管验收。独立转换对照中，原backlog字段读取约55–68ms，候选编码和解析约17–18ms，数据等价检查通过。

原件位于恢复根的 `ci-pr27-windows-release-sixth-job.log`、`synthetic-original-before-01`、`synthetic-hotpath-observed-01..05b`、`layer-encoding-validation-green-02`、`runtime-hotpath-full-gates-01`、`backlog-transfer-red-01`、`backlog-transfer-green-01`、`runtime-hotpath-final-web-01`。未证明收益的私有字符缓存已撤回，其 `runtime-hotpath-validation-green-03` 原始失败保留。诊断耗时不作为吞吐门禁结果，完整U1–U29目标未因此宣布完成。
