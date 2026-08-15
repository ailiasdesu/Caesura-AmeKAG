// G4 editor — scene outline shared types (first increment).
// Kept in a separate module so both the parser (lib/) and the component
// (ide/) can import them without a DOM dependency.

/** A row in the scene outline: a *label heading, a [command ...] row, or a
 *  bare text line (dialogue / narration) rendered as content. */
export type OutlineItem =
  | { kind: 'label'; line: number; name: string }
  | { kind: 'command'; line: number; cmd: string; params: Record<string, string> }
  | { kind: 'text'; line: number; content: string }

export interface OutlineSection {
  /** Label name (no leading '*'), or null for the prologue before the first label. */
  label: string | null
  /** Source line of the label (or first item for the prologue). */
  line: number
  items: OutlineItem[]
}
