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

    ctx.input_focus = "history"
    local selected = #ctx.backlog
    local scroll   = 0
    local ITEMS    = 12

    while true do
        local backend = require("backend")

        -- ── Render: semi-transparent dark overlay ──
        backend.debug_clear()
        backend.debug_rect(0, 0, 1280, 720, 0x80000000)

        -- ── Render: scrollable entry list ──
        local y = 20
        local lastVisible = math.min(scroll + ITEMS, #ctx.backlog)
        for i = scroll + 1, lastVisible do
            local e       = ctx.backlog[i]
            local prefix  = (i == selected) and ">" or " "
            local preview = (e.text or ""):sub(1, 60)
            local color   = (i == selected) and 0x0E or 0x0F
            backend.debug_text(10, y, color, string.format("%s %s", prefix, preview))
            y = y + 18
        end

        -- ── Render: help bar ──
        backend.debug_text(10, 700, 0x0A, "[↑↓]Scroll  [Enter]Jump  [Esc]Close  [V]Replay Voice")

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
                    return { jump = true, scene = e.scene, index = e.token_index }
                end
            elseif key == "V" then
                local e = ctx.backlog[selected]
                if e and e.voice and #e.voice > 0 then
                    backend.audio_play("voice", e.voice)
                end
            elseif key == "ESC" then
                ctx.input_focus = "kag"
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
