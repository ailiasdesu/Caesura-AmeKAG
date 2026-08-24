local test_dir = "tests/scripts/"
package.path = test_dir .. "../../scripts/?.lua;" .. test_dir .. "?.lua;" .. package.path
local _real_dofile = dofile
pcall(require, "replay")
pcall(require, "palette")
pcall(require, "viewport")

local f = io.open("tests/scripts/run_lua_tests.lua", "r")
local content = f:read("*a")
f:close()

local list = {}
for w in content:match("local tests = {([^}]+)}"):gmatch('"([^"]+)"') do
    list[#list+1] = w
end
print("Total tests in runner: " .. #list)

for i = 1, #list do
    local name = list[i]
    if name == "test_input_cmd" then break end
    if name ~= "test_ks_i18n_flow" then
        pcall(function() _real_dofile(test_dir .. name .. ".lua") end)
    end
    local ok_inp, err_inp = pcall(function() _real_dofile(test_dir .. "test_input_cmd.lua") end)
    if not ok_inp then
        print(string.format("FAILURE INTRODUCED BY [%d] %s: %s", i, name, tostring(err_inp)))
        break
    end
end
print("Search completed.")
