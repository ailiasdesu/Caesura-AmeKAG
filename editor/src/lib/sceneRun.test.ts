import { describe, it, expect } from 'vitest'
import {
  scenePathForDoc,
  buildRunSceneSnippet,
  escapeScenePath,
} from './sceneRun'

describe('scenePathForDoc', () => {
  it('returns the path for a .ks scene', () => {
    expect(scenePathForDoc('assets/script/main.ks')).toBe('assets/script/main.ks')
    expect(scenePathForDoc('projects/demo/assets/script/title.ks')).toBe(
      'projects/demo/assets/script/title.ks',
    )
  })

  it('is case-insensitive about the .ks extension', () => {
    expect(scenePathForDoc('assets/script/SCENE.KS')).toBe('assets/script/SCENE.KS')
  })

  it('returns null for a .lua script', () => {
    expect(scenePathForDoc('assets/script/main.lua')).toBeNull()
  })

  it('returns null for null / undefined / empty input', () => {
    expect(scenePathForDoc(null)).toBeNull()
    expect(scenePathForDoc(undefined)).toBeNull()
    expect(scenePathForDoc('')).toBeNull()
  })

  it('returns null for a non-.ks extension', () => {
    expect(scenePathForDoc('assets/bg/back.png')).toBeNull()
    expect(scenePathForDoc('assets/ui/ui.txt')).toBeNull()
  })
})

describe('buildRunSceneSnippet', () => {
  it('builds a well-formed eval that stops then starts the scene', () => {
    const snip = buildRunSceneSnippet('assets/script/main.ks')
    expect(snip).toContain('require("kag_runner")')
    expect(snip).toContain('if kr.stop then kr.stop() end')
    expect(snip).toContain('return tostring(kr.start("assets/script/main.ks"))')
  })

  it('escapes quotes/backslashes in a projects/ path so the literal stays closed', () => {
    const path = 'projects/foo\\bar"x.ks'
    const snip = buildRunSceneSnippet(path)
    const safe = escapeScenePath(path)
    expect(snip).toContain('kr.start("' + safe + '")')
    // backslash doubled and double-quote escaped
    expect(safe).toContain('\\\\')
    expect(safe).toContain('\\"')
  })

  it('never returns an empty snippet', () => {
    expect(buildRunSceneSnippet('a.ks').length).toBeGreaterThan(0)
  })
})
