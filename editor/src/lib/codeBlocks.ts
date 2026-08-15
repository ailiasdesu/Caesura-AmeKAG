// Caesura Editor — split an LLM reply into prose and fenced code blocks
// so the AiPanel "Ask" transcript can render ``` fences as <pre><code>.
export interface CodeBlock {
  kind: 'text' | 'code'
  /** Optional fence language tag ('' when none). */
  lang?: string
  content: string
}

/** Split text on triple-backtick fences. Unclosed fences are treated as prose. */
export function splitCodeBlocks(text: string): CodeBlock[] {
  const blocks: CodeBlock[] = []
  // \n? = an optional line break right after the opening fence.
  const re = new RegExp('```([A-Za-z0-9_+.-]*)\\n?([\\s\\S]*?)```', 'g')
  let last = 0
  let m: RegExpExecArray | null
  while ((m = re.exec(text)) !== null) {
    if (m.index > last) {
      blocks.push({ kind: 'text', content: text.slice(last, m.index) })
    }
    blocks.push({ kind: 'code', lang: m[1] || undefined, content: m[2] })
    last = m.index + m[0].length
  }
  if (last < text.length) blocks.push({ kind: 'text', content: text.slice(last) })
  if (blocks.length === 0) blocks.push({ kind: 'text', content: text })
  return blocks
}