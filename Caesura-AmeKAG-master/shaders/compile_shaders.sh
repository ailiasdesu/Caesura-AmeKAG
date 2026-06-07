#!/bin/bash
# ============================================================================
# Caesura (AmeKAG) - Shader Compilation Script (Unix / macOS)
# ============================================================================
# Compiles bgfx shaderc-format (.sc) GLSL shaders into platform-specific
# binary shaders using bgfx`s shaderc tool.
#
# Usage:
#   ./compile_shaders.sh [platform]
#
# Platforms:
#   linux   - OpenGL (GLSL 120)
#   macos   - Metal (MSL)
#   windows - Direct3D 11 (DXBC via HLSL)
#   all     - Compile for all platforms (default)
#
# Prerequisites:
#   - bgfx shaderc built and available in PATH or BGFX_DIR/bin/
#   - Set BGFX_DIR or SHADERC to point to shaderc executable
# ============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GLSL_DIR="$SCRIPT_DIR/glsl"
OUT_DIR="$SCRIPT_DIR/compiled"
SHADERC="${SHADERC:-shaderc}"

# bgfx varying definition file
VARYING_DEF="$SCRIPT_DIR/varying.def"

PLATFORM="${1:-all}"

# -- Create varying.def if not exists ----------------------------------------
if [ ! -f "$VARYING_DEF" ]; then
    cat > "$VARYING_DEF" << 'VDEF'
vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);

vec2 a_position  : POSITION;
vec2 a_texcoord0 : TEXCOORD0;
VDEF
    echo "[shaders] Created varying.def"
fi

# -- shaderc profiles --------------------------------------------------------
compile_for_platform() {
    local platform="$1"
    local out_subdir="$OUT_DIR/$platform"
    mkdir -p "$out_subdir"

    local profile_args=()
    local ext=".bin"

    case "$platform" in
        linux)
            profile_args=(--platform linux --profile 120)
            ;;
        macos)
            profile_args=(--platform osx --profile metal)
            ext=".metal.bin"
            ;;
        windows)
            profile_args=(--platform windows --profile vs_4_0 -O 3)
            ;;
        *)
            echo "[shaders] Unknown platform: $platform"
            return 1
            ;;
    esac

    echo ""
    echo "=== Compiling shaders for $platform ==="

    # -- Vertex shaders -------------------------------------------------------
    local vs_shaders=(
        "vs_sprite"
        "vs_fullscreen"
        "stretch_blt_vs"
        "affine_blt_vs"
    )
    for name in "${vs_shaders[@]}"; do
        local src="$GLSL_DIR/${name}.sc"
        local out="$out_subdir/${name}${ext}"
        if [ -f "$src" ]; then
            echo "  [VS] $name"
            "$SHADERC" -f "$src" -o "$out" \
                --type vertex \
                --varyingdef "$VARYING_DEF" \
                "${profile_args[@]}" \
                --depends
        else
            echo "  [SKIP] $src not found"
        fi
    done

    # -- Fragment shaders -----------------------------------------------------
    local fs_shaders=(
        "fs_texture"
        "fs_blend"
        "fs_transition"
        "fs_vfx"
        "stretch_blt_fs"
        "affine_blt_fs"
    )
    for name in "${fs_shaders[@]}"; do
        local src="$GLSL_DIR/${name}.sc"
        local out="$out_subdir/${name}${ext}"
        if [ -f "$src" ]; then
            echo "  [FS] $name"
            "$SHADERC" -f "$src" -o "$out" \
                --type fragment \
                --varyingdef "$VARYING_DEF" \
                "${profile_args[@]}" \
                --depends
        else
            echo "  [SKIP] $src not found"
        fi
    done

    echo "=== $platform shaders compiled to $out_subdir ==="
}

# -- Main --------------------------------------------------------------------
echo "=============================================="
echo " Caesura (AmeKAG) Shader Compiler"
echo "=============================================="
echo "Source dir : $GLSL_DIR"
echo "Output dir : $OUT_DIR"
echo ""

if [ "$PLATFORM" = "all" ]; then
    compile_for_platform "linux"
    compile_for_platform "macos"
    compile_for_platform "windows"
else
    compile_for_platform "$PLATFORM"
fi

echo ""
echo "All shaders compiled successfully."
echo "Output: $OUT_DIR"
