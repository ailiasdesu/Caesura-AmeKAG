// Caesura Editor — small promise helpers used by the AiPanel query flow.
/** Reject a promise if it does not settle within ms. The original promise
 *  is NOT cancelled (the engine call may still land), but the caller treats
 *  this as a timeout and degrades the UI. */
export function withTimeout<T>(
  p: Promise<T>,
  ms: number,
  message: string,
): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(message)), ms)
    p.then(
      (v) => {
        clearTimeout(timer)
        resolve(v)
      },
      (e) => {
        clearTimeout(timer)
        reject(e)
      },
    )
  })
}
