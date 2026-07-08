# Design Decisions

## No ImGui / Immediate-Mode GUI Framework
[R10-FIX] The engine intentionally does NOT use ImGui or any immediate-mode GUI framework.
Rationale:
- ErrorUI uses direct bgfx rendering for crash resilience.
- All game UI is Lua-driven via backend.render_text()/create_solid_texture()/draw_viewport().
- Minimal C++ surface area, maximal content-creator control.
