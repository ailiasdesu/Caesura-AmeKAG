// Custom OpenAI-compatible endpoint provider
export async function customChat(messages, settings) {
  const url = settings.customUrl || "";
  if (!url) throw new Error("Custom endpoint URL not set. Configure in Settings (⚙).");

  const key = settings.customKey || "";
  const model = settings.customModel || "default";
  const temperature = settings.temperature ?? 0.7;

  const headers = { "Content-Type": "application/json" };
  if (key) headers["Authorization"] = `Bearer ${key}`;

  const response = await fetch(url, {
    method: "POST",
    headers,
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