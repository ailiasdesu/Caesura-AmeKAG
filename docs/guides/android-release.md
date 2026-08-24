# Android Release Artifact 工作流（Track M A5）

> 状态：2026-08-24。Debug APK 链路已在 CI 验证（android-compile 探针 assembleDebug）；
> 本文定义 release 产物与签名流程。**密钥永远不进入仓库**（A5 硬性要求：
> signing credentials 不提交）。

## 1. 产物矩阵

| 产物 | 命令（android/ 下） | 签名 | CI 验证 |
|---|---|---|---|
| debug APK | `gradle assembleDebug` | debug key（AGP 自动） | ✅ 探针全绿 |
| release APK | `gradle assembleRelease` | 无 env → **未签名**；置 env → 签名 | ✅ 签名与未签名构建 |
| AAB | `gradle bundleRelease` | 同 release | ✅ 自动化脚本已接入（`scripts/build_android_release.sh`） |

## 2. 签名（环境变量驱动，零仓库密钥）

```bash
# 首次生成 keystore（本机执行，产物切勿提交）
keytool -genkeypair -v -keystore ~/.caesura/android-release.keystore \
  -alias caesura -keyalg RSA -keysize 2048 -validity 10000 \
  -storepass <秘密> -keypass <秘密> -dname "CN=Caesura Team"

# 构建时注入（gradle 读取；不设置则输出未签名 APK）
export CAESURA_ANDROID_KEYSTORE=~/.caesura/android-release.keystore
export CAESURA_ANDROID_KEYSTORE_PASS=<storepass>
export CAESURA_ANDROID_KEY_ALIAS=caesura
export CAESURA_ANDROID_KEY_PASS=<keypass>
gradle -p android assembleRelease
```

`android/app/build.gradle` 的 `signingConfigs.caesura` 仅在
`CAESURA_ANDROID_KEYSTORE` 存在时被 release variant 引用；未设置时构建为
未签名 release（可安装需要 `apksigner sign` 手动步，见 §4）。

## 3. 可复现构建（容器化/CI 配方）

探针即权威配方（.github/workflows/ci.yml android-compile job）：

```text
1. SDL3 3.2.4   : cmake -DCMAKE_TOOLCHAIN_FILE=$NDK/android.toolchain.cmake \
                  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 → install
2. OpenSSL 3.3.2: PATH=$NDK/.../linux-x86_64/bin + ./Configure android-arm64 \
                  --prefix=<dir> -D__ANDROID_API__=24 && make && make install_sw
3. 引擎          : cmake -DCMAKE_TOOLCHAIN_FILE=... -DSDL3_DIR=... \
                  -DOPENSSL_ROOT_DIR=... → build-android/libCaesuraAmeKAG.so
4. 装配          : .so ×2 → app/src/main/jniLibs/arm64-v8a/ + assets/game/**
5. gradle        : assembleDebug / assembleRelease（凭据仅构建机环境变量）
```

复现校验：`unzip -l app-release.apk | grep libCaesuraAmeKAG.so`；
hash 记录进发布记录（A4 表格的 APK hash 列同理）。

## 4. 发布检查单（编写中，真机验证后生效）

- [ ] 签名 release APK（§2）→ `apksigner verify app-release.apk`
- [ ] ABI 单一 arm64-v8a（当前基线；多 ABI 后续再扩）
- [ ] bundle versionCode/versionName 与引擎版本一致（app/build.gradle）
- [ ] AAB（Play 阶段）：`bundleRelease` + Play 上传，Test track 后续
- [ ] 真机 A4 清单通过后才可打“release-verified”分层（device-unverified 铁律）

## 5. 已知边界

- minSdk 24（android-24 原生基线）、compileSdk/target 35；
- 未签名 release APK **不可直接安装**（需 apksigner 或走 debug 安装）；
- Play Console / 上架过程不在本轮范围（计划明确不阻塞核心 Runtime）。