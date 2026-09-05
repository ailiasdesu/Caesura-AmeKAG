# 可复现验证与原始证据

验证分成三个步骤：受控执行器运行固定profile，采集器校验并归一化原始结果，验证器与执行边界保存的receipt和profile复核。任何一步都不会自行授予RC-GO或发布许可。

## 原生验证入口

现有原生profile为windows/linux/macos的Debug与Release，定义于scripts/validation_profiles.json。它们覆盖构建、完整C++、Lua主/孤儿套件、验证工具回归、耦合、测试注册和CTest。真实GPU、Web完整包、设备与商店验证仍属于各自专项，原生profile通过不能替代它们。

Windows在Git Bash中从仓库根执行：

~~~bash
cmake --preset windows-foundation
cmake --build --preset windows-debug
RAW="artifacts/validation/raw/windows-debug-$(date -u +%Y%m%dT%H%M%SZ)"
python scripts/run_validation.py \
  --profile-name windows-debug \
  --build-dir build/presets/windows-foundation \
  --configuration Debug \
  --run-dir "$RAW"
~~~

执行器核对宿主平台、profile配置、CMake源码目录和单/多配置类型；不允许把其他checkout的产物或Debug单配置目录标成当前源码的Release。新输出目录必须不存在。运行期间不要修改源码、profile或声明夹具，否则证据会标记变化。

若本机Visual Studio未登记到安装发现接口，需在首次configure时用本机已核实的CMAKE_GENERATOR_INSTANCE指定实例。不要把其他机器的绝对路径写进共享preset。本项目当前开发机曾出现vswhere返回空但MSBuild可用的情况，诊断记录保存在执行日志中。

Linux/macOS可使用linux-foundation、macos-foundation preset；单配置Release需在独立目录以CMAKE_BUILD_TYPE=Release配置，再选择对应Release profile。linux-sanitized使用Clang的ASan/UBSan，要求事先提供编译器与平台依赖；不支持的组合直接配置失败。

## 采集与验证

run.json内的source_sha/run_id用于定位结果目录。此文件必须保留在受控执行边界；验证时不能从待验包中挑一份receipt作为信任来源。

~~~bash
OUT=$(python -c 'import json,sys; r=json.load(open(sys.argv[1],encoding="utf-8")); print("artifacts/validation/"+r["source_sha"]+"/"+r["run_id"]+"/windows-debug")' "$RAW/run.json")
python scripts/collect_validation_evidence.py \
  --profile scripts/validation_profiles.json --profile-name windows-debug \
  --run "$RAW/run.json" --output "$OUT"
python scripts/verify_release_candidate.py \
  --profile scripts/validation_profiles.json --profile-name windows-debug \
  --expected-run "$RAW/run.json" --artifacts-dir "$OUT" --check
~~~

工作树包含尚未提交的改动时，可以给最后一步加--diagnostic。这仅检查该诊断记录的实际profile结果，不把dirty源码或test-fixture授予发布资格。源码/输入在执行中变化、必需检查失败或缺失，即使diagnostic也不能被解释为通过。

只想重组已有真实记录时，--generate-bundle需要真实--run和相应profile参数。旧的硬编码测试报告不能升级为新证据；--skip-if-missing在缺证据时返回77，绝不会返回0冒充已验证。

## 原始记录与计数

记录包含完整SHA、工作树指纹、夹具前后指纹、实际时间、配置/工具链、展开后的argv和cwd、执行程序与被测二进制摘要、stdout/stderr及结构化报告。执行前后复核二进制，防止把旧进程结果绑定到替换后的新文件。

执行器通过argv调用命令，不经shell解释。Windows使用受控启动握手和JobObject，POSIX使用独立进程组；完成、超时或中断时只回收本轮拥有的进程。它不约束主动逃离隔离范围的恶意程序。

PASS、FAIL、SKIP、NOT_RUN、NOT_APPLICABLE分别记录。发现数、执行数、通过数不能互相替代。原生C++完整运行不允许跳过；CTest的真实服务AI smoke属于明确的可选项，其他必需项缺依赖不能静默跳过。测试注册检查不代表代码覆盖率。

profile中的min_discovered来自已知基线，只用于发现明显倒退，不填写passed。增加/删除测试或改变配置时，先检查实际发现数与原因，再更新对应基线。性能采样还需单独的measurement_status与预先确定的比较阈值。

## CI与信任范围

Linux CI采集真实原生profile结果并上传原始receipt与归一化产物。已有文档freshness步骤可能改写生成文件，因此该阶段采用diagnostic验证，明确不授予发布许可。最终发布仍需后续的固定workflow/run/attempt/artifact与最终包digest验证。

本通路信任受控执行器和指定CI，不声称能识别已攻陷runner或恶意改写测试逻辑。摘要用于绑定实际字节，不能单凭自报SHA、日志或profile授予通过。测试夹具必须保持test-fixture标记，不得进入发布证据。
