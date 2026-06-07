local ok, err = pcall(dofile, "scripts/game_logic.lua")
if not ok then print("SYNTAX ERROR: " .. tostring(err)) else print("SYNTAX OK") end
