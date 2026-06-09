// OpenAI-compatible chat provider
export async function openaiChat(messages, settings) {
  const key = settings.openaiKey || "";
  if (!key) throw new Error("OpenAI API key not set. Configure in Settings (⚙).");

  const model = settings.openaiModel || "gpt-4o";
  const temperature = settings.temperature ?? 0.7;

  const response = await fetch("https://api.openai.com/v1/chat/completions", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      Authorization: `Bearer ${key}`,
    },
    body: JSON.stringify({
      model,
      messages,
      temperature,
      max_tokens: 2048,
    }),
  });

  if (!response.ok) {
    const err = await response.json().catch(() => ({}));
    throw new Error(err.error?.message || `HTTP ${response.status}`);
  }

  const data = await response.json();
  return data.choices?.[0]?.message?.content || "";
}