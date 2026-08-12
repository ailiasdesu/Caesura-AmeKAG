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
