-- =============================================================================
--  run_lua_tests.lua — Standalone Lua test runner
--  Usage: lua tests/scripts/run_lua_tests.lua
-- =============================================================================

-- Add scripts/ to package.path
local test_dir = arg and arg[0] and arg[0]:match("(.*[/\\])") or ""
package.path = test_dir .. "../../scripts/?.lua;" .. test_dir .. "?.lua;" .. package.path

-- Save globals that sandbox may replace
local _real_os_exit = os.exit
local _real_dofile  = dofile

-- NOTE: test_kag_commands must run BEFORE test_scheduler because
-- scheduler internally loads kag module which caches a partial table.
local tests = {
    "test_tokenizer",
    "test_kag_commands",
    "test_scheduler",
    "test_rollback",
    "test_backlog",
    "test_kag3_compat",
    "test_choice",
    "test_title_menu",
    "test_title_entry",
    "test_benchmark",
    "test_schema",
    "test_layers",
    "test_sandbox",
    "test_label_index",
    "test_expr_cache",
    "test_bg_dedup",
    "test_macro_nested",
    "test_scene_restore",
    "test_elseif",
    "test_if_nested",
    "test_switch",
    "test_switch_exotic",
    "test_switch_scan",
    "test_switch_taken_nested",
    "test_while",
    "test_for",
    "test_loop_control",
    "test_effect_aliases",
    "test_waitclick",
    "test_iscript_sandbox",
    "test_eval_env",
    "test_sandbox_create",
    "test_select",
    "test_label_jump",
    "test_end_title",
    "test_history",
    "test_unlock",
    "test_settings",
    "test_saveplace",
    "test_story_meta",
    "test_saveload",
    "test_audio_cmds",
    "test_textflow",
    "test_vib_camera",
    "test_transitions",
    "test_layer_cmds",
    "test_scroll",
    "test_layopt",
    "test_text_deco",
    "test_reset_pt",
    "test_emb_sync",
    "test_operation",
    "test_fade_audio",
    "test_volume",
    "test_ch_state",
    "test_playbgmstop",
    "test_layout",
    "test_resolve",
    "test_sprite_family",
    "test_toast",
    "test_textbox",
    "test_vfx_clamp",
    "test_delay",
    "test_wait_audio",
    "test_video",
    "test_font",
    "test_jump_path",
    "test_bareval",
    "test_alias_bare",
    "test_unlock_bare",
    "test_macro_bare",
    "test_save_slot",
    "test_gallery_bare",
    "test_offsets",
    "test_ks_check",
    "test_four_remaining",
    "test_scroll",
}

local passed, failed = 0, 0
print("\n=== Caesura Lua Test Suite ===\n")

for _, name in ipairs(tests) do
    print(string.format("Running %s.lua...", name))
    local ok, err = pcall(function() _real_dofile(test_dir .. name .. ".lua") end)
    if ok then
        passed = passed + 1
        print(string.format("  [OK] %s\n", name))
    else
        failed = failed + 1
        print(string.format("  [FAIL] %s: %s\n", name, tostring(err)))
    end
end

print(string.format("Results: %d passed, %d failed, %d total",
    passed, failed, passed + failed))
if failed > 0 then _real_os_exit(1) end
