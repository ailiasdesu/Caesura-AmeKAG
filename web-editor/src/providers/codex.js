// Codex local agent provider — communicates via IPC
export async function codexChat(messages, settings) {
  // Codex runs locally; use IPC to the Electron main process
  if (!window.caesura?.codexChat) {
    throw new Error("Codex bridge not available. Run editor via Electron.");
  }

  const response = await window.caesura.codexChat({ messages, settings });
  return response?.content || "";
}