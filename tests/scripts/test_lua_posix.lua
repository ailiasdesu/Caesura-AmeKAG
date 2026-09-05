-- Run this with the CMake lua_cli product before applying the game sandbox.
-- Desktop POSIX builds must supply native process/loader capabilities; using
-- a system Lua here would conceal missing platform definitions in our build.
local pipe = assert(io.popen("printf 'caesura-native-posix'", "r"))
local output = pipe:read("*a")
local closed, reason, status = pipe:close()
assert(output == "caesura-native-posix", "io.popen must read the actual child output")
assert(closed == true and reason == "exit" and status == 0,
       "io.popen must report a successful POSIX child exit")

local failed = assert(io.popen("exit 7", "r"))
local ok, failure_reason, failure_status = failed:close()
assert(ok == nil and failure_reason == "exit" and failure_status == 7,
       "io.popen must preserve the failing child exit code")

local executed, execute_reason, execute_status = os.execute("exit 9")
assert(executed == nil and execute_reason == "exit" and execute_status == 9,
       "os.execute must decode the POSIX wait status")

local loader, message, stage = package.loadlib(
    arg[0] .. ".missing-native-module", "luaopen_caesura_missing")
assert(loader == nil and type(message) == "string" and stage == "open",
       "package.loadlib must attempt the native loader instead of reporting absent")
print("LUA POSIX BUILD TESTS: 5 passed")
