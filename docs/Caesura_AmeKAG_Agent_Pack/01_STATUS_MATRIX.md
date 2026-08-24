# Task 01 — Unified Platform Status Matrix

## 目标

建立单一、机器可读、可生成 README/roadmap/status 文档的跨平台状态源。

当前存在一个重要问题：不同文档曾经对同一个平台给出不同完成等级；必须消灭状态漂移。

## 设计要求

新增：

`docs/status/platform-matrix.yaml`

建议字段：

```yaml
version: 1

platforms:
  windows:
    build: verified
    runtime: verified
    first_vn: verified
    packaging: verified
    release: pending

  linux:
    build: verified
    runtime: verified
    first_vn: verified
    packaging: verified
    release: pending

  web:
    build: verified
    runtime: verified
    first_vn: verified
    browser: verified
    release_candidate: verified

  android:
    build: verified
    runtime: verified
    first_vn: verified
    real_device: verified
    signing: verified
    aab: verified
    release: pending

  macos:
    build: probe
    runtime: pending
    first_vn: pending
    real_device: hardware-gated
    release: pending

  ios:
    build: probe
    metal: probe
    runtime: pending
    real_device: hardware-gated
    signing: pending
    testflight: pending
```

状态枚举必须严格定义：
- `verified`
- `probe`
- `pending`
- `hardware-gated`
- `credential-gated`
- `blocked`
- `not-applicable`

禁止使用含糊词：
- almost done
- basically done
- ready-ish
- complete（没有 evidence）

## Evidence

每个状态允许：

```yaml
evidence:
  commit: "<sha>"
  document: "<path>"
  test: "<command>"
  verified_at: "<timestamp>"
```

## 自动生成

新增：
`scripts/generate_platform_status.py`

生成：
`docs/status/platform-status.md`

并可被 README 引用。

## 验收

1. 所有 README/platform/roadmap 中的状态来源统一。
2. 不能出现文档 A = DONE、文档 B = pending 的矛盾。
3. CI 增加状态 schema validation。
4. 新增平台状态必须附 evidence。
5. 任何 `verified` 必须能定位到真实 test/document evidence。

## 禁止

不要把 CI compile probe 升级成 real-device verified。
不要为了“全绿”修改事实状态。
