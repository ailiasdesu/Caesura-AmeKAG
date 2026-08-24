# Android Release Packaging & Signing Guide (Track A5)

本文档规范 Caesura (AmeKAG) 引擎在 Android 平台上的 Release 构建、密钥生成、环境变量注入、签名配置、APK/AAB 打包、对齐验证及 CI/CD 自动化流水线。

---

## 1. 密钥库（Keystore）生成

生产环境的签名密钥由发布者独立持有并妥善保管，**严禁将 keystore 文件及密钥密码提交至 Git 仓库**。

### 1.1 手动执行 `keytool` 生成 PKCS12 密钥库

使用 JDK 17+ 自带的 `keytool` 命令生成标准的 PKCS12 格式密钥库：

```bash
keytool -genkeypair -v \
    -keystore caesura-release.keystore \
    -alias caesura \
    -keyalg RSA \
    -keysize 2048 \
    -validity 10000 \
    -storetype PKCS12 \
    -dname "CN=Caesura Game, OU=Release, O=CaesuraEngine, C=JP"
```

参数说明：
- `-storetype PKCS12`：工业标准密钥库格式（替代废弃的 JKS 格式）。
- `-keyalg RSA -keysize 2048`：2048 位或 4096 位 RSA 算法。
- `-validity 10000`：有效期约 27 年（Google Play 强制要求应用签名密钥有效期至少 25 年）。
- `-alias caesura`：密钥条目别名。

### 1.2 使用引擎自动化脚本生成

引擎提供了跨平台的密钥生成辅助脚本：

- **Linux / macOS / Git Bash**:
  ```bash
  # 交互式生成正式密钥
  bash scripts/generate_android_keystore.sh --keystore ~/caesura-release.keystore --alias caesura

  # CI 或自动化测试生成临时测试密钥（非交互式）
  bash scripts/generate_android_keystore.sh --test --keystore /tmp/ci-test.keystore
  ```

- **Windows (CMD / PowerShell)**:
  ```cmd
  scripts\generate_android_keystore.bat --keystore caesura-release.keystore --alias caesura
  scripts\generate_android_keystore.bat --test
  ```

---

## 2. 环境变量注入与 Gradle 签名配置

签名凭据通过环境变量注入，保证代码仓库零密码泄露风险。

### 2.1 环境变量规范

引擎 Gradle 构建脚本同时兼容支持两套环境变量命名约定：

| 命名集 A（推荐） | 命名集 B（兼容） | 说明 | 示例 |
|---|---|---|---|
| `CAESURA_ANDROID_KEYSTORE` | `CAESURA_KEYSTORE_PATH` | 密钥库文件的绝对或相对路径 | `/path/to/caesura-release.keystore` |
| `CAESURA_ANDROID_KEYSTORE_PASS` | `CAESURA_KEYSTORE_PASSWORD` | 密钥库访问密码 | `your_keystore_pass` |
| `CAESURA_ANDROID_KEY_ALIAS` | `CAESURA_KEY_ALIAS` | 密钥条目别名 | `caesura` |
| `CAESURA_ANDROID_KEY_PASS` | `CAESURA_KEY_PASSWORD` | 密钥独立密码（缺省时同 storepass） | `your_key_pass` |

在终端中设置：

```bash
export CAESURA_ANDROID_KEYSTORE="/path/to/caesura-release.keystore"
export CAESURA_ANDROID_KEYSTORE_PASS="your_secret_keystore_password"
export CAESURA_ANDROID_KEY_ALIAS="caesura"
export CAESURA_ANDROID_KEY_PASS="your_secret_key_password"
```

### 2.2 `android/app/build.gradle` 签名与打包配置

```groovy
plugins {
     id 'com.android.application'
}

def ksPath = System.getenv('CAESURA_ANDROID_KEYSTORE') ?: System.getenv('CAESURA_KEYSTORE_PATH')
def ksPass = System.getenv('CAESURA_ANDROID_KEYSTORE_PASS') ?: System.getenv('CAESURA_KEYSTORE_PASSWORD') ?: ''
def kAlias = System.getenv('CAESURA_ANDROID_KEY_ALIAS') ?: System.getenv('CAESURA_KEY_ALIAS') ?: ''
def kPass  = System.getenv('CAESURA_ANDROID_KEY_PASS') ?: System.getenv('CAESURA_KEY_PASSWORD') ?: ''

android {
    namespace "com.caesura.app"
    compileSdkVersion 35
    defaultConfig {
        applicationId "com.caesura.app"
        minSdkVersion 24
        targetSdkVersion 35
        versionCode 1
        versionName "1.0"
        ndk {
            abiFilters 'arm64-v8a'
        }
    }
    signingConfigs {
        caesura {
            if (ksPath != null && file(ksPath).exists()) {
                storeFile file(ksPath)
                storePassword ksPass
                keyAlias kAlias
                keyPassword kPass
                v1SigningEnabled true
                v2SigningEnabled true
            }
        }
    }
    buildTypes {
        release {
            minifyEnabled false
            proguardFiles getDefaultProguardFile('proguard-android.txt'), 'proguard-rules.pro'
            // 签名凭据仅在环境变量中存在时生效；未配置时构建为未签名 APK/AAB 供 CI 测试
            if (ksPath != null && file(ksPath).exists()) {
                signingConfig signingConfigs.caesura
            }
        }
    }
    bundle {
        // Visual Novel 特性：禁用语言、密度、ABI 动态分片，确保基础包完整包含多语言剧本与资源
        language {
            enableSplit = false
        }
        density {
            enableSplit = false
        }
        abi {
            enableSplit = false
        }
    }
    lint {
        abortOnError false
    }
}
```

#### Bundle Split 禁用说明：
视觉小说游戏（Visual Novel）拥有大量的本地化剧本文本（`assets/lang/*.lua`、`scripts/kag/*.lua`）、CG 图片资源及音频。开启 Google Play 语言或密度分片可能导致多语言切换功能因资源缺失而崩溃。通过配置 `bundle { language { enableSplit = false } }` 确保全语言剧本与基础资源打包于 Base Module 中。

---

## 3. 构建与打包流程

### 3.1 预编译 C++ 原生动态库

```bash
# 使用 NDK 交叉编译 arm64-v8a 目标
scripts/build_android.sh --release --abi arm64-v8a

# 将编译产物归档至 jniLibs
mkdir -p android/app/src/main/jniLibs/arm64-v8a
cp build-android-arm64-v8a/libCaesuraAmeKAG.so android/app/src/main/jniLibs/arm64-v8a/
cp "$SDL3_DIR/../libSDL3.so" android/app/src/main/jniLibs/arm64-v8a/
```

### 3.2 准备游戏资产与剧本

```bash
rm -rf android/app/src/main/assets/game
mkdir -p android/app/src/main/assets/game/demo
cp -r scripts  android/app/src/main/assets/game/scripts
cp -r assets   android/app/src/main/assets/game/assets
cp -r tests/projects/first_vn android/app/src/main/assets/game/demo/first_vn
```

### 3.3 构建产物

- **生成 Release APK（用于独立分发、官网下载或侧载安装）**：
  ```bash
  cd android
  gradle assembleRelease --no-daemon --console=plain
  ```
  产物路径：`android/app/build/outputs/apk/release/app-release.apk`

- **生成 Release Android App Bundle（AAB，用于 Google Play 上架发布）**：
  ```bash
  cd android
  gradle bundleRelease --no-daemon --console=plain
  ```
  产物路径：`android/app/build/outputs/bundle/release/app-release.aab`

---

## 4. 自动化构建流水线 (`build_android_release.sh`)

引擎提供了一键式 Release 构建与验证脚本：

```bash
# 本地发布构建（使用指定密钥）
bash scripts/build_android_release.sh \
    --keystore /path/to/caesura.keystore \
    --storepass "your_password" \
    --alias "caesura"

# 本地/CI 自动化验证（自动生成临时密钥签名并进行全套合规检查）
bash scripts/build_android_release.sh --ephemeral-key
```

流水线自动执行：
1. 密钥检测或临时密钥生成；
2. 动态库与资产同步；
3. `assembleRelease` APK 构建；
4. `bundleRelease` AAB 构建；
5. `zipalign -c 4` 对齐合规检查；
6. `apksigner verify` V1/V2/V3 签名验证；
7. AAB 归档结构完整性验证。

---

## 5. 产物合规性验证方法

### 5.1 4-Byte 内存对齐验证 (`zipalign`)

Android 系统要求 APK 中的未压缩共享库与资源按 4 字节边界对齐，以便通过 `mmap` 直接映射内存：

```bash
zipalign -c -v 4 android/app/build/outputs/apk/release/app-release.apk
```

预期输出：
```text
Verification successful
```

### 5.2 签名方案验证 (`apksigner`)

验证 APK 是否正确应用了 V1 (JAR Signing)、V2 (APK Signature Scheme v2) 及 V3 签名方案：

```bash
apksigner verify --verbose --print-certs android/app/build/outputs/apk/release/app-release.apk
```

预期输出：
```text
Verifies: true
Verified using v1 scheme (JAR signing): true
Verified using v2 scheme (APK Signature Scheme v2): true
Verified using v3 scheme (APK Signature Scheme v3): true
Signer #1 certificate DN: CN=Caesura Game, OU=Release, O=CaesuraEngine, C=JP
Signer #1 certificate SHA-256 digest: ...
```

### 5.3 AAB 归档结构检查

检查生成的 AAB 文件是否包含原生库、剧本与 AndroidManifest：

```bash
unzip -l android/app/build/outputs/bundle/release/app-release.aab | grep -E "base/lib/arm64-v8a/libCaesuraAmeKAG.so|base/lib/arm64-v8a/libSDL3.so|base/assets/game/scripts/kag/init.lua|base/manifest/AndroidManifest.xml"
```

---

## 6. GitHub Actions CI 持续集成

在 `.github/workflows/ci.yml` 的 `android-compile` 作业中已接入全套 Release 构建与签名验证：

```yaml
- name: "Generate ephemeral test keystore (Track A5)"
  shell: bash
  run: |
    set -e
    bash scripts/generate_android_keystore.sh --test --keystore "$RUNNER_TEMP/ci-release.keystore"
    echo "CAESURA_ANDROID_KEYSTORE=$RUNNER_TEMP/ci-release.keystore" >> "$GITHUB_ENV"
    echo "CAESURA_ANDROID_KEYSTORE_PASS=caesura_test_pass" >> "$GITHUB_ENV"
    echo "CAESURA_ANDROID_KEY_ALIAS=caesura-test" >> "$GITHUB_ENV"
    echo "CAESURA_ANDROID_KEY_PASS=caesura_test_pass" >> "$GITHUB_ENV"

- name: "Gradle assembleDebug, assembleRelease & bundleRelease (Track A5)"
  shell: bash
  run: |
    cd android
    "$GRADLE" --no-daemon --console=plain assembleDebug
    "$GRADLE" --no-daemon --console=plain assembleRelease
    "$GRADLE" --no-daemon --console=plain bundleRelease
    cd ..

- name: "Verify Android release artifacts (zipalign & apksigner) (Track A5)"
  shell: bash
  run: |
    BT_DIR=$(ls -d "$ANDROID_SDK_ROOT/build-tools/"* 2>/dev/null | sort -V | tail -n 1)
    "$BT_DIR/zipalign" -c -v 4 android/app/build/outputs/apk/release/app-release.apk
    "$BT_DIR/apksigner" verify --verbose --print-certs android/app/build/outputs/apk/release/app-release.apk
    unzip -l android/app/build/outputs/bundle/release/app-release.aab | grep -E "base/lib/arm64-v8a/libCaesuraAmeKAG.so|base/manifest/AndroidManifest.xml"
```
