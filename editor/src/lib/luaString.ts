/**
 * Lua long-bracket string literal escaping (single source of truth for
 * every /api/eval bridge in the editor).
 *
 * A long bracket `[=N[ ... ]=N]` terminates at the FIRST `]=N]` inside
 * the content, so arbitrary text can break out into executable Lua. The
 * safe level is N = (longest run of `=` anywhere in the content) + 1:
 * the terminator would need N consecutive `=` after a `]`, and the
 * content contains at most N-1.
 *
 * Note: it is NOT enough to scan for `]=` runs followed by `[` — a
 * payload like `]=] print(1) --` (a `]` + one `=` + `]`) is the level-1
 * terminator and must be counted. Scanning all `=` runs covers every
 * `]=...` shape regardless of what follows.
 */
export function luaString(s: string): string {
  const body = String(s)
  let maxRun = 0
  const re = /=+/g
  let m: RegExpExecArray | null
  while ((m = re.exec(body))) {
    if (m[0].length > maxRun) maxRun = m[0].length
  }
  const eq = '='.repeat(maxRun + 1)
  return `[${eq}[${body}]${eq}]`
}

/** Lua double-quoted string literal (safe for arbitrary content). */
function luaQuote(s: string): string {
  return (
    '"' +
    String(s)
      .replace(/\\/g, '\\\\')
      .replace(/"/g, '\\"')
      .replace(/\n/g, '\\n')
      .replace(/\r/g, '\\r')
      .replace(/\t/g, '\\t')
      .replace(/[\x00-\x1f]/g, (c) => `\\u{${c.charCodeAt(0).toString(16)}}`) +
    '"'
  )
}

/**
 * JSON value → Lua literal. JSON object syntax (`{"a":1}`) is a Lua
 * syntax error, so object/array args must be converted to Lua table
 * constructors before being embedded in an /api/eval chunk (review
 * finding: generate_dialogue / explain_diagnostic always failed with a
 * compile error because of this).
 */
export function luaValue(v: unknown): string {
  if (v === null || v === undefined) return 'nil'
  if (typeof v === 'number') return Number.isFinite(v) ? String(v) : 'nil'
  if (typeof v === 'boolean') return v ? 'true' : 'false'
  if (typeof v === 'string') return luaQuote(v)
  if (Array.isArray(v)) {
    return '{' + v.map((x) => luaValue(x)).join(',') + '}'
  }
  if (typeof v === 'object') {
    const parts = Object.entries(v as Record<string, unknown>).map(
      ([k, val]) => `[${luaQuote(k)}]=${luaValue(val)}`,
    )
    return '{' + parts.join(',') + '}'
  }
  return 'nil'
}

