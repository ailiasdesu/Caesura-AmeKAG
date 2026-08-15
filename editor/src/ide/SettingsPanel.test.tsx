// @vitest-environment jsdom
import { describe, it, expect, beforeEach, afterEach } from 'vitest'
import { render, screen, fireEvent, cleanup } from '@testing-library/react'
import { SettingsPanel } from './SettingsPanel'
import { useEditor } from '../store'
import {
  loadSettings,
  saveSettings,
  clearSettings,
  sanitizeSettings,
  SETTINGS_KEY,
  DEFAULT_SETTINGS,
} from '../lib/settings'

beforeEach(() => {
  localStorage.clear()
  // Restore a clean store + fresh (default) persisted settings for every test.
  useEditor.setState({
    docs: [],
    activePath: null,
    sideView: 'explorer',
    engineConnected: false,
    engineScene: '',
    engineToken: 0,
    enginePaused: false,
    revealRequest: null,
    settings: { ...DEFAULT_SETTINGS },
  })
})

afterEach(() => {
  cleanup()
  localStorage.clear()
})

function applyPanel() {
  fireEvent.click(screen.getByRole('button', { name: 'Apply' }))
}

describe('SettingsPanel (component)', () => {
  it('renders the theme, editor and engine connection sections', () => {
    render(<SettingsPanel />)
    expect(screen.getByText('Settings')).toBeTruthy()
    expect(screen.getByText('Theme')).toBeTruthy()
    expect(screen.getByText('Editor')).toBeTruthy()
    expect(screen.getByText('Engine connection')).toBeTruthy()
    expect(screen.getByLabelText('Color theme')).toBeTruthy()
    expect(screen.getByLabelText('Editor font size')).toBeTruthy()
    expect(screen.getByLabelText('Show line numbers')).toBeTruthy()
    // Engine params displayed read-only.
    expect(screen.getByText('HTTP port')).toBeTruthy()
    expect(screen.getByText('9876')).toBeTruthy()
    expect(screen.getByText(/read-only/i)).toBeTruthy()
  })

  it('seeds the draft from current store settings', () => {
    useEditor.getState().setSettings({ theme: 'light', fontSize: 16, showLineNumbers: false })
    render(<SettingsPanel />)
    const select = screen.getByLabelText('Color theme') as HTMLSelectElement
    const size = screen.getByLabelText('Editor font size') as HTMLInputElement
    const lineNums = screen.getByLabelText('Show line numbers') as HTMLInputElement
    expect(select.value).toBe('light')
    expect(size.value).toBe('16')
    expect(lineNums.checked).toBe(false)
    localStorage.clear() // this helper modified store + localStorage
  })

  it('applies changes to the store and persists them to localStorage', () => {
    render(<SettingsPanel />)
    fireEvent.change(screen.getByLabelText('Color theme'), { target: { value: 'light' } })
    fireEvent.change(screen.getByLabelText('Editor font size'), { target: { value: '17' } })
    // Toggle the line-number checkbox (default true) off via a click.
    fireEvent.click(screen.getByLabelText('Show line numbers'))
    applyPanel()

    const s = useEditor.getState().settings
    expect(s.theme).toBe('light')
    expect(s.fontSize).toBe(17)
    expect(s.showLineNumbers).toBe(false)

    const persisted = loadSettings()
    expect(persisted.theme).toBe('light')
    expect(persisted.fontSize).toBe(17)
    expect(persisted.showLineNumbers).toBe(false)
  })

  it('notifies store subscribers (change propagation) after Apply', () => {
    const seen: (string | undefined)[] = []
    const unsub = useEditor.subscribe((state, prev) => {
      if (state.settings.theme !== prev.settings.theme) seen.push(state.settings.theme)
    })
    render(<SettingsPanel />)
    fireEvent.change(screen.getByLabelText('Color theme'), { target: { value: 'light' } })
    applyPanel()
    expect(seen).toContain('light')
    unsub()
  })

  it('sanitizes and persists out-of-range font size as the default (validation fallback)', () => {
    render(<SettingsPanel />)
    fireEvent.change(screen.getByLabelText('Editor font size'), { target: { value: '999' } })
    applyPanel()

    const s = useEditor.getState().settings
    expect(s.fontSize).toBe(DEFAULT_SETTINGS.fontSize) // 13, not 999
    const persisted = loadSettings()
    expect(persisted.fontSize).toBe(DEFAULT_SETTINGS.fontSize)
  })

  it('fallback also applies to a non-numeric font input', () => {
    render(<SettingsPanel />)
    fireEvent.change(screen.getByLabelText('Editor font size'), { target: { value: 'abc' } })
    applyPanel()
    expect(useEditor.getState().settings.fontSize).toBe(DEFAULT_SETTINGS.fontSize)
  })

  it('reset restores defaults in the store and clears persisted settings', () => {
    useEditor.getState().setSettings({ theme: 'light', fontSize: 20, showLineNumbers: false })
    expect(loadSettings().theme).toBe('light')

    render(<SettingsPanel />)
    fireEvent.click(screen.getByRole('button', { name: /reset to defaults/i }))

    const s = useEditor.getState().settings
    expect(s.theme).toBe('dark')
    expect(s.fontSize).toBe(DEFAULT_SETTINGS.fontSize)
    expect(s.showLineNumbers).toBe(true)
    // Reset re-persists the default blob (a reload then yields defaults).
    expect(loadSettings().theme).toBe('dark')
    expect(loadSettings().fontSize).toBe(DEFAULT_SETTINGS.fontSize)
  })

  it('keeps engine connection params read-only (not editable inputs)', () => {
    render(<SettingsPanel />)
    // No text/number inputs exist for port/token — only the font input echoes.
    const numberInputs = screen.queryAllByRole('spinbutton')
    expect(numberInputs).toHaveLength(1) // font size only
    expect(screen.getByText('(none)')).toBeTruthy() // no token set
  })
})

describe('settings persistence (lib/settings)', () => {
  it('loads defaults when storage is empty', () => {
    expect(loadSettings()).toEqual(DEFAULT_SETTINGS)
  })

  it('round-trips a settings blob through localStorage', () => {
    saveSettings({ theme: 'light', fontSize: 14, showLineNumbers: false, enginePort: 9000, engineToken: 'tok' })
    const loaded = loadSettings()
    expect(loaded.theme).toBe('light')
    expect(loaded.fontSize).toBe(14)
    expect(loaded.showLineNumbers).toBe(false)
    expect(loaded.enginePort).toBe(9000)
    expect(loaded.engineToken).toBe('tok')
  })

  it('falls back field-by-field when the stored value is corrupt', () => {
    localStorage.setItem(SETTINGS_KEY, JSON.stringify({ theme: 'pink', fontSize: -5, showLineNumbers: 'yes', enginePort: 123456, engineToken: 42 }))
    const loaded = loadSettings()
    expect(loaded.theme).toBe('dark')
    expect(loaded.fontSize).toBe(DEFAULT_SETTINGS.fontSize)
    expect(loaded.showLineNumbers).toBe(true)
    expect(loaded.enginePort).toBe(DEFAULT_SETTINGS.enginePort)
    expect(loaded.engineToken).toBe('')
  })

  it('returns defaults when the stored JSON is unparseable', () => {
    localStorage.setItem(SETTINGS_KEY, '{corrupt')
    expect(loadSettings()).toEqual(DEFAULT_SETTINGS)
  })

  it('sanitizeSettings fills missing fields with defaults', () => {
    expect(sanitizeSettings({ theme: 'light' })).toEqual({
      theme: 'light',
      fontSize: DEFAULT_SETTINGS.fontSize,
      showLineNumbers: true,
      enginePort: DEFAULT_SETTINGS.enginePort,
      engineToken: '',
    })
  })

  it('clearSettings removes the persisted blob', () => {
    saveSettings({ ...DEFAULT_SETTINGS, theme: 'light' })
    expect(localStorage.getItem(SETTINGS_KEY)).not.toBeNull()
    clearSettings()
    expect(localStorage.getItem(SETTINGS_KEY)).toBeNull()
  })
})