// AIBinding - expose an LLM query API to Lua scripts (Neo-Genesis).
// Registers a global `AI` table:
//   AI.available() -> bool
//   AI.query(prompt, opts?) -> string | nil, err      (sync, blocking)
//   AI.query_async(prompt, opts?, callback) -> bool    (async via JobSystem;
//       callback(text|nil, err) runs on the main Lua thread)
//   AI.cancel() -> bool                                (cancel pending async)
//
// Endpoints: OpenAI-compatible (/v1/chat/completions) and Ollama
// (/api/generate); format auto-detected from the endpoint URL. Config:
// scripts/config.lua `config.ai = { endpoint, model, api_key,
// timeout_ms, system }`. Unavailable services degrade to
// nil/error (never raise into Lua).
#pragma once

struct lua_State;

namespace Caesura {
void registerAIBinding(lua_State* L);
} // namespace Caesura
