-- ===========================================================================
--  Caesura (AmeKAG) — history_ui.lua
--  Backlog display interface. Renders scrollable semi-transparent overlay
--  with text history entries, supports jump-to-scene and voice replay.
--  Spec: U3 backlog display — [history] tag triggers HistoryUI.show(ctx).
--
--  Controller: [↑↓] navigate, [Enter] jump, [V] replay voice, [Esc] close.
-- ===========================================================================

local HistoryUI = {}

function HistoryUI.show(ctx)
    -- Guard: nothing to show
    if not ctx.backlog or #ctx.backlog == 0 then
        return
    end

    -- Set input focus explicitly on entry
    pcall(function()
        local backend = require("backend")
        backend.set_input_focus("GAME")
    end)

    ctx.input_focus = "history"
    local selected = #ctx.backlog
    local scroll   = 0
    local ITEMS    = 12

    while true do
        local backend = require("backend")

        -- ── Clear and render dark overlay background ──
        backend.debug_clear()
        backend.debug_rect(0, 0, 1280, 720, 0xD0000000)  -- darker overlay (alpha ~208)

        -- ── Title bar: "Backlog / 消息记录" ──
        backend.debug_rect(0, 0, 1280, 40, 0xE0101030)    -- dark blue title bar
        local titleStr = "Backlog / 消息记录"
        backend.debug_text(20, 8, 0x0E, titleStr)           -- bright yellow for title

        -- ── Entry count indicator: "Entry 5 / 45" ──
        local posStr = "Entry " .. selected .. " / " .. #ctx.backlog
        backend.debug_text(640, 8, 0x0A, posStr)            -- green indicator at center

        -- ── Entry separator line under title bar ──
        backend.debug_rect(0, 40, 1280, 1, 0x604060A0)      -- subtle separator

        -- ── Render: scrollable entry list ──
        local y = 52
        local entryH = 22     -- line height per entry
        local lastVisible = math.min(scroll + ITEMS, #ctx.backlog)
        for i = scroll + 1, lastVisible do
            local e = ctx.backlog[i]
            local isSelected = (i == selected)

            -- Highlight background for selected entry
            if isSelected then
                backend.debug_rect(10, y - 1, 1260, entryH + 2, 0x602040A0)  -- blue highlight
            end

            -- Entry prefix: selection indicator
            local prefix = isSelected and ">" or " "

            -- Truncated preview text
            local preview = (e.text or ""):sub(1, 60)
            if #(e.text or "") > 60 then preview = preview .. "..." end

            -- Speaker / scene name
            local speaker = e.name or ""
            if speaker == "" then speaker = "(Narration)" end
            local speakerPreview = speaker:sub(1, 16)

            -- Voice indicator
            local voiceIndicator = ""
            if e.voice and #e.voice > 0 then
                voiceIndicator = " [V]"  -- voice available
            end

            -- Render entry line: " > [Speaker] preview text... [V]"
            local color = isSelected and 0x0E or 0x0F
            local line = string.format("%s %-17s %s%s", prefix, "[" .. speakerPreview .. "]", preview, voiceIndicator)
            backend.debug_text(20, y + 1, color, line)

            -- Timestamp on the right
            if e.time then
                backend.debug_text(1150, y + 1, 0x08, e.time)  -- dim gray timestamp
            elseif e.timestamp then
                local ts = os.date("%H:%M", e.timestamp)
                backend.debug_text(1150, y + 1, 0x08, ts)
            end

            -- Scene name indicator (if available)
            if e.scene and #e.scene > 0 then
                local sceneShort = e.scene:match("[^/\\]+$") or e.scene
                sceneShort = sceneShort:sub(1, 20)
                backend.debug_text(750, y + 1, 0x09, "[" .. sceneShort .. "]")  -- blue scene tag
            end

            -- Thin separator line between entries
            backend.debug_rect(30, y + entryH, 1220, 1, 0x30102030)

            y = y + entryH + 2   -- 2px gap for separator
        end

        -- ── Render: help bar at bottom ──
        local footerY = 690
        backend.debug_rect(0, footerY - 4, 1280, 34, 0xE0101030)   -- footer background
        local helpStr = "[Up/Down] Navigate  [Enter] Jump to Scene  [V] Replay Voice  [Esc] Close"
        backend.debug_text(20, footerY + 3, 0x0A, helpStr)

        -- Footer: position info on right
        backend.debug_text(1100, footerY + 3, 0x0A, posStr)

        -- ── Yield to engine frame ──
        coroutine.yield()

        -- ── Input handling ──
        if ctx.key_pressed then
            local key = ctx.key_pressed

            if key == "UP" then
                selected = math.max(1, selected - 1)
            elseif key == "DOWN" then
                selected = math.min(#ctx.backlog, selected + 1)
            elseif key == "ENTER" then
                local e = ctx.backlog[selected]
                if e and e.scene and e.token_index then
                    ctx.input_focus = "kag"
                    -- Restore input focus before returning
                    pcall(function()
                        require("backend").set_input_focus("KAG")
                    end)
                    return { jump = true, scene = e.scene, index = e.token_index }
                end
            elseif key == "V" then
                local e = ctx.backlog[selected]
                if e and e.voice and #e.voice > 0 then
                    backend.audio_play("voice", e.voice)
                end
            elseif key == "ESC" then
                ctx.input_focus = "kag"
                -- Restore input focus on exit
                pcall(function()
                    require("backend").set_input_focus("KAG")
                end)
                return
            end

            ctx.key_pressed = nil
        end

        -- ── Scroll clamping ──
        if selected <= scroll then
            scroll = selected - 1
        elseif selected >= scroll + ITEMS then
            scroll = selected - ITEMS + 1
        end
        scroll = math.max(0, math.min(#ctx.backlog - ITEMS, scroll))
    end
end

return HistoryUI
