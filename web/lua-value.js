// JSON-safe value -> Lua literal source (bracketed keys + escaped strings).
// Round 46: the web save bridge parses stored state back into real Lua
// tables with load('return ' .. literal) — wasmoon's userdata proxies are
// rejected by the engine's load handler (type(state) ~= 'table').
export function luaLiteralValue(v) {
  if (v === null || v === undefined) return 'nil'
  if (typeof v === 'number') {
    if (!Number.isFinite(v) || (Number.isInteger(v) && !Number.isSafeInteger(v))) throw new Error('Number exceeds Web JSON safe range')
    return String(v)
  }
  if (typeof v === 'boolean') return v ? 'true' : 'false'
  if (typeof v === 'string') {
    let out = '"'
    for (const ch of v) {
      const code = ch.codePointAt(0)
      if (ch === '\\') out += '\\\\'
      else if (ch === '"') out += '\\"'
      else if (ch === '\n') out += '\\n'
      else if (ch === '\r') out += '\\r'
      else if (ch === '\t') out += '\\t'
      else if (code < 32) out += '\\' + String(code).padStart(3, '0')
      else out += ch
    }
    return out + '"'
  }
  if (Array.isArray(v)) return '{' + v.map(luaLiteralValue).join(',') + '}'
  if (typeof v === 'object') {
    const parts = []
    // Bracket quoted keys whenever they are NOT a valid plain Lua identifier:
    // reserved words (end/for/do/while/return/... — e.g. a [for end="3"]
    // param) or otherwise non-alnum keys must be '["end"]=..' not 'end=..'.
    const RESERVED = new Set(['and','break','do','else','elseif','end','false','for','function','goto','if','in','local','nil','not','or','repeat','return','then','true','until','while'])
    for (const k of Object.keys(v)) {
      const bare = /^[A-Za-z_][A-Za-z0-9_]*$/.test(k) && !RESERVED.has(k)
      const key = bare ? k : '[' + luaLiteralValue(k) + ']'
      parts.push(key + '=' + luaLiteralValue(v[k]))
    }
    return '{' + parts.join(',') + '}'
  }
  return 'nil'
}
