local test_dir = "tests/scripts/"
package.path = test_dir .. "../../scripts/?.lua;" .. test_dir .. "?.lua;" .. package.path
local _real_dofile = dofile
pcall(require, "replay")
pcall(require, "palette")
pcall(require, "viewport")

-- Run tests up to test_sandbox
_real_dofile(test_dir .. "test_color_filter.lua")
_real_dofile(test_dir .. "test_sandbox.lua")

-- Check if rawset on _G or adding to _G_whitelist allows test_input_cmd to pass
local sandbox = require("sandbox")
local sb_env = debug.getregistry()

print("Testing sandboxed execution of test_input_cmd...")
local ok, err = pcall(function() _real_dofile(test_dir .. "test_input_cmd.lua") end)
print("Result before whitelist:", ok, err)
