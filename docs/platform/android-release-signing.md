# Android Release Packaging & Signing Guide (Track M A5)

本文档规范 Caesura (AmeKAG) 引擎在 Android 平台上的 Release 构建、密钥生成、签名配置、APK/AAB 打包与验证流程。

---

## 1. 密钥库（Keystore）生成

生产环境的签名密钥由发布者独立持有并妥善保管，**严禁将 keystore 文件提交至 Git 仓库**。

使用 JDK 自带的 `keytool` 命令生成标准的 PKCS12 格式密钥库：

```bash
keytool -genkeypair \
    -v \
    -keystore caesura-release.keystore \
    -alias CaesuraReleaseKey \
    -keyalg RSA \
    -keysize 4096 \
    -validity 9125 \
    -storetype PKCS12 \
    -dname "CN=Caesura Game, OU=Production, O=CaesuraEngine, L=Tokyo, ST=Tokyo, C=JP"
```

参数说明：
- `-keysize 4096`：推荐使用 4096 位 RSA 算法。
- `-validity 9125`：有效期 25 年（Google Play 强制要求至少 25 年有效期）。
- `-alias CaesuraReleaseKey`：密钥别名。

---

## 2. Gradle 签名配置

在 `android/app/build.gradle` 中通过环境变量注入密钥，避免硬编码密码。

### 2.1 环境变量配置

在 CI/CD 或本地环境设置以下环境变量：

```bash
export CAESURA_KEYSTORE_PATH="/path/to/caesura-release.keystore"
export CAESURA_KEYSTORE_PASSWORD="your-keystore-password"
export CAESURA_KEY_ALIAS="CaesuraReleaseKey"
export CAESURA_KEY_PASSWORD="your-key-password"
```

### 2.2 Gradle `signingConfigs` 结构

```groovy
android {
    signingConfigs {
        release {
            if (System.getenv("CAESURA_KEYSTORE_PATH") != null) {
                storeFile file(System.getenv("CAESURA_KEYSTORE_PATH"))
                storePassword System.getenv("CAESURA_KEYSTORE_PASSWORD")
                keyAlias System.getenv("CAESURA_KEY_ALIAS")
                keyPassword System.getenv("CAESURA_KEY_PASSWORD")
            }
        }
    }

    buildTypes {
        release {
            minifyEnabled false
            shrinkResources false
            if (System.getenv("CAESURA_KEYSTORE_PATH") != null) {
                signingConfig signingConfigs.release
            }
            proguardFiles getDefaultProguardFile('proguard-android-optimize.txt'), 'proguard-rules.pro'
        }
    }
}
```

---

## 3. 构建与打包

### 3.1 预构建原生 .so 动态库

在执行 Gradle 打包前，必须先编译目标架构的 C++ 共享库：

```bash
# ARM64 (arm64-v8a)
cmake -B build-android-arm64-v8a \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-33 \
    -DCMAKE_BUILD_TYPE=Release
ninja -C build-android-arm64-v8a

mkdir -p android/app/src/main/jniLibs/arm64-v8a/
cp build-android-arm64-v8a/libCaesuraAmeKAG.so android/app/src/main/jniLibs/arm64-v8a/
```

### 3.2 生成 Release APK（用于独立分发/侧载）

```bash
cd android/app
gradle assembleRelease --rerun-tasks -Pandroid.overridePathCheck=true
```

产物路径：`android/app/build/outputs/apk/release/app-release.apk`

### 3.3 生成 Android App Bundle (AAB，用于 Google Play)

```bash
cd android/app
gradle bundleRelease --rerun-tasks -Pandroid.overridePathCheck=true
```

产物路径：`android/app/build/outputs/bundle/release/app-release.aab`

---

## 4. 产物验证与合规性检查

### 4.1 对齐验证 (`zipalign`)

```bash
zipalign -c -v 4 android/app/build/outputs/apk/release/app-release.apk
```

### 4.2 签名体系验证 (`apksigner`)

```bash
apksigner verify --verbose --print-certs android/app/build/outputs/apk/release/app-release.apk
```

预期输出应包含：
- `Verifies: true`
- `Verified using v1 scheme (JAR signing): true`
- `Verified using v2 scheme (APK Signature Scheme v2): true`
- `Verified using v3 scheme (APK Signature Scheme v3): true`

---

## 5. ABI 过滤与架构规划

Caesura 引擎默认主目标为 `arm64-v8a`，可选扩展支持 `armeabi-v7a` 和 `x86_64`：

```groovy
android {
    defaultConfig {
        ndk {
            abiFilters 'arm64-v8a'
        }
    }
}
```
