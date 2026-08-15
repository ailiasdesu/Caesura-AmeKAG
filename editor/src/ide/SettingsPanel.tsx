// Ide Settings panel — editor preference settings (theme / font / line numbers /
// engine connection). The draft lives in component state and is committed
// (persisted + applied) via the store's `setSettings` action, which mirrors to
// localStorage. Invalid numeric input falls back to the default on apply rather
// than being persisted raw. Engine connection params are displayed read-only
// because the EngineClient transport is not currently port/token-parameterized
// (the dev proxy fixes :9876; the token is typed in the connection panel).

import { useEffect, useState } from 'react'
import { useEditor } from '../store'
import {
  DEFAULT_SETTINGS,
  clearSettings,
  type EditorTheme,
} from '../lib/settings'

/** Shared validation bounds (kept in step with lib/settings.ts sanitize). */
export const MIN_FONT = 8
export const MAX_FONT = 32
export const MIN_PORT = 1
export const MAX_PORT = 65535

function toInt(x: string, dflt: number): number {
  const n = Number(x)
  return Number.isFinite(n) ? Math.round(n) : dflt
}

export function SettingsPanel() {
  const settings = useEditor((s) => s.settings)
  const setSettings = useEditor((s) => s.setSettings)

  // Local draft, seeded from the store whenever the store settings change
  // externally (e.g. reset-to-defaults or a reload).
  const [theme, setTheme] = useState<EditorTheme>(settings.theme)
  const [fontSize, setFontSize] = useState(String(settings.fontSize))
  const [lineNumbers, setLineNumbers] = useState(settings.showLineNumbers)

  useEffect(() => {
    setTheme(settings.theme)
    setFontSize(String(settings.fontSize))
    setLineNumbers(settings.showLineNumbers)
  }, [settings])

  const apply = () => {
    const parsed = toInt(fontSize, DEFAULT_SETTINGS.fontSize)
    // Validation fallback: clamp out-of-range / non-numeric to the default.
    const size =
      parsed >= MIN_FONT && parsed <= MAX_FONT ? parsed : DEFAULT_SETTINGS.fontSize
    setSettings({ theme, fontSize: size, showLineNumbers: lineNumbers })
  }

  const reset = () => {
    clearSettings()
    // Restore everything to the built-in defaults in one store + draft update.
    setSettings({ ...DEFAULT_SETTINGS })
  }

  return (
    <div className="settings-panel sidebar-pane" data-testid="settings-panel">
      <div className="panel-title">Settings</div>

      <section className="settings-group" aria-labelledby="set-theme">
        <div className="panel-subtitle" id="set-theme">Theme</div>
        <select
          className="settings-select"
          value={theme}
          onChange={(e) => setTheme(e.target.value as EditorTheme)}
          aria-label="Color theme"
        >
          <option value="dark">Dark</option>
          <option value="light">Light</option>
        </select>
      </section>

      <section className="settings-group" aria-labelledby="set-editor">
        <div className="panel-subtitle" id="set-editor">Editor</div>
        <label className="settings-row">
          <span className="settings-label">Font size</span>
          <input
            className="settings-input"
            type="number"
            min={MIN_FONT}
            max={MAX_FONT}
            value={fontSize}
            onChange={(e) => setFontSize(e.target.value)}
            aria-label="Editor font size"
          />
        </label>
        <label className="settings-row settings-check">
          <input
            type="checkbox"
            checked={lineNumbers}
            onChange={(e) => setLineNumbers(e.target.checked)}
            aria-label="Show line numbers"
          />
          <span className="settings-label">Show line numbers</span>
        </label>
        <span className="settings-hint">
          Font {MIN_FONT}–{MAX_FONT}px · out-of-range values reset on apply
        </span>
      </section>

      <section className="settings-group" aria-labelledby="set-engine">
        <div className="panel-subtitle" id="set-engine">Engine connection</div>
        <div className="settings-row settings-readonly">
          <span className="settings-label">HTTP port</span>
          <code className="settings-value">{settings.enginePort}</code>
        </div>
        <div className="settings-row settings-readonly">
          <span className="settings-label">Bearer token</span>
          <code className="settings-value">
            {settings.engineToken ? '••••••••' : '(none)'}
          </code>
        </div>
        <span className="settings-hint">
          Read-only: the dev proxy fixes :9876 and the token is typed in the
          connection panel.
        </span>
      </section>

      <div className="settings-actions">
        <button className="settings-primary" onClick={() => apply()}>
          Apply
        </button>
        <button className="settings-secondary" onClick={() => reset()}>
          Reset to defaults
        </button>
      </div>
    </div>
  )
}
