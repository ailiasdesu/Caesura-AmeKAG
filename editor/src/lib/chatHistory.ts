// Caesura Editor — AI chat history persistence (localStorage-backed).
// The AiPanel "Ask" section keeps a rolling transcript in component state and
// mirrors it to localStorage so the panel survives a re-open / page reload.

export interface ChatMessage {
  id: string
  role: 'user' | 'assistant' | 'error'
  text: string
  time: number
}

/** localStorage key for the AiPanel chat transcript. */
export const AI_CHAT_KEY = 'caesura.aipanel.chat'

/** Hard cap on the persisted transcript (newest N messages survive). */
export const MAX_CHAT_MESSAGES = 100

export function isChatMessage(x: unknown): x is ChatMessage {
  if (typeof x !== 'object' || x === null) return false
  const m = x as Record<string, unknown>
  if (typeof m.role !== 'string' || !['user', 'assistant', 'error'].includes(m.role))
    return false
  if (typeof m.text !== 'string') return false
  if (typeof m.id !== 'string') return false
  if (typeof m.time !== 'number') return false
  return true
}

/** Load and validate the persisted transcript (corrupt/missing -> []). */
export function loadChat(key: string = AI_CHAT_KEY): ChatMessage[] {
  try {
    const raw = localStorage.getItem(key)
    if (!raw) return []
    const arr = JSON.parse(raw) as unknown
    if (!Array.isArray(arr)) return []
    return arr.filter(isChatMessage).slice(-MAX_CHAT_MESSAGES)
  } catch {
    return []
  }
}

/** Persist the transcript (newest N messages only). Non-fatal on failure. */
export function saveChat(msgs: ChatMessage[], key: string = AI_CHAT_KEY): void {
  try {
    localStorage.setItem(key, JSON.stringify(msgs.slice(-MAX_CHAT_MESSAGES)))
  } catch {
    /* storage full / unavailable — non-fatal */
  }
}

/** Remove the persisted transcript. */
export function clearChat(key: string = AI_CHAT_KEY): void {
  try {
    localStorage.removeItem(key)
  } catch {
    /* non-fatal */
  }
}

let _seq = 0
/** Deterministic-ish unique id for a chat message. */
export function nextMessageId(): string {
  _seq += 1
  return 'm' + Date.now().toString(36) + '-' + _seq.toString(36)
}
