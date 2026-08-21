// @vitest-environment jsdom
// Recent-projects persistence & pure helpers (Project Manager, Sprint 2).
// Covers sanitize/reload (defensive localStorage) and pushRecentProject's
// de-duplicate + cap + most-recent-first ordering.
import { describe, it, expect, beforeEach, vi } from 'vitest'
import {
  RECENT_KEY,
  RECENT_LIMIT,
  sanitizeRecentProjects,
  loadRecentProjects,
  saveRecentProjects,
  pushRecentProject,
  type RecentProject,
} from './recentProjects'

const p = (path: string, name: string, openedAt: number): RecentProject => ({
  path,
  name,
  openedAt,
})

beforeEach(() => {
  localStorage.clear()
})

describe('sanitizeRecentProjects', () => {
  it('returns [] for non-array input', () => {
    expect(sanitizeRecentProjects(null)).toEqual([])
    expect(sanitizeRecentProjects('x')).toEqual([])
    expect(sanitizeRecentProjects({})).toEqual([])
  })

  it('drops invalid entries and keeps valid ones', () => {
    const input = [
      p('projects/a', 'a', 1),
      { path: 5, name: 'bad', openedAt: 1 },
      null,
      'garbage',
    ]
    expect(sanitizeRecentProjects(input)).toEqual([p('projects/a', 'a', 1)])
  })

  it('caps the result at RECENT_LIMIT', () => {
    const many = Array.from({ length: RECENT_LIMIT + 5 }, (_, i) =>
      p('projects/p' + i, 'p' + i, i),
    )
    expect(sanitizeRecentProjects(many)).toHaveLength(RECENT_LIMIT)
  })
})

describe('loadRecentProjects / saveRecentProjects (localStorage round-trip)', () => {
  it('returns [] when nothing is stored', () => {
    expect(loadRecentProjects()).toEqual([])
  })

  it('restores what saveRecentProjects persisted', () => {
    const recent = [p('projects/a', 'a', 3), p('projects/b', 'b', 2)]
    saveRecentProjects(recent)
    expect(loadRecentProjects()).toEqual(recent)
  })

  it('falls back to [] on corrupt JSON', () => {
    localStorage.setItem(RECENT_KEY, '{not valid json')
    expect(loadRecentProjects()).toEqual([])
  })

  it('non-fatal save does not throw on storage failure', () => {
    const spy = vi.spyOn(Storage.prototype, 'setItem').mockImplementation(() => {
      throw new Error('quota')
    })
    expect(() => saveRecentProjects([p('projects/a', 'a', 1)])).not.toThrow()
    spy.mockRestore()
  })
})

describe('pushRecentProject (pure)', () => {
  it('inserts a new entry at the front', () => {
    const recent: RecentProject[] = [p('projects/b', 'b', 2)]
    const next = pushRecentProject(recent, 'projects/a', 'a', 10)
    expect(next.map((r) => r.path)).toEqual(['projects/a', 'projects/b'])
    expect(next[0].openedAt).toBe(10)
  })

  it('de-duplicates by path, moving the existing entry to the front', () => {
    const recent: RecentProject[] = [p('projects/a', 'a', 1), p('projects/b', 'b', 2)]
    const next = pushRecentProject(recent, 'projects/a', 'a', 99)
    expect(next).toHaveLength(2)
    expect(next[0]).toEqual({ path: 'projects/a', name: 'a', openedAt: 99 })
    expect(next[1].path).toBe('projects/b')
  })

  it('never mutates the input array (immutable)', () => {
    const recent: RecentProject[] = [p('projects/a', 'a', 1), p('projects/b', 'b', 2)]
    pushRecentProject(recent, 'projects/c', 'c', 3)
    expect(recent).toHaveLength(2)
    expect(recent[0].path).toBe('projects/a')
    expect(recent).toEqual([p('projects/a', 'a', 1), p('projects/b', 'b', 2)])
  })

  it('caps the history at RECENT_LIMIT', () => {
    let recent: RecentProject[] = []
    for (let i = 0; i < RECENT_LIMIT; i++) {
      recent = pushRecentProject(recent, 'projects/p' + i, 'p' + i, i)
    }
    const next = pushRecentProject(recent, 'projects/new', 'new', 999)
    expect(next).toHaveLength(RECENT_LIMIT)
    expect(next[0].path).toBe('projects/new')
    // The oldest/first-inserted entry (p0) is the one evicted at the cap.
    expect(next.some((r) => r.path === 'projects/p0')).toBe(false)
  })
})
