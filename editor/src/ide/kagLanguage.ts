// KAG Neo-Genesis language definition for Monaco.
// Registered once at startup; gives .ks files tag/param/comment/label
// syntax highlighting in the editor area (IDE P2-7).
//
// The recognized command-name set lives in lib/commandLint.ts (a single source
// of truth shared with the Inspector's param lint). It is re-exported here as
// KAG_COMMANDS so existing consumers (kagLanguage.test.ts, Monaco) keep the
// same name without pulling the lint module's rules into the Monaco graph.
import * as monaco from 'monaco-editor'
import { KNOWN_COMMANDS } from '../lib/commandLint'

export { KNOWN_COMMANDS as KAG_COMMANDS }


export function registerKagLanguage() {
  const id = 'kag'
  if (monaco.languages.getLanguages().some((l) => l.id === id)) return

  monaco.languages.register({ id, extensions: ['.ks'] })

  monaco.languages.setMonarchTokensProvider(id, {
    defaultToken: '',
    tokenPostfix: '.kag',
    keywords: KNOWN_COMMANDS,
    tokenizer: {
      root: [
        // comments: ; ... line
        [/;.*$/, 'comment'],
        // labels: *name at line start
        [/^\*[a-zA-Z_][\w]*/, 'type.identifier'],
        // block text """ ... """
        [/"""/, { token: 'string', next: '@blocktext' }],
        // tags: [command ...]
        [
          /\[[a-zA-Z_][\w]*/,
          {
            cases: {
              '@keywords': 'tag',
              '@default': 'tag.invalid',
            },
          },
        ],
        [/[\[\]]/, 'delimiter.bracket'],
        // named params: key=value
        [/([\w_]+)(\s*)(=)/, ['attribute.name', '', 'operator']],
        // quoted strings
        [/"(?:[^"\\]|\\.)*"/, 'string'],
        [/'[^']*'/, 'string'],
        // numbers
        [/\b\d+(?:\.\d+)?\b/, 'number'],
        // variables: $f.x / %f.x% / ${expr}
        [/\$\{/, { token: 'variable', next: '@expr' }],
        [/\$[a-zA-Z_][\w]*\.[\w.]+/, 'variable'],
        [/%[a-zA-Z_][\w]*\.[\w.]+%/, 'variable'],
        [/\$\w+/, 'variable'],
        // text content
        [/[^\s\[\]%$"';]+/, 'text'],
        [/\s+/, 'white'],
      ],
      blocktext: [
        [/"""/, { token: 'string', next: '@pop' }],
        [/./, 'string'],
      ],
      expr: [
        [/\}/, { token: 'variable', next: '@pop' }],
        [/[^}]+/, 'variable'],
      ],
    },
  })

  // Syntax error highlight for unmatched brackets is handled by the
  // engine's ks_check; Monaco provides bracket matching out of the box.
  monaco.languages.setLanguageConfiguration(id, {
    comments: { lineComment: ';' },
    brackets: [
      ['[', ']'],
      ['{', '}'],
    ],
    autoClosingPairs: [
      { open: '[', close: ']' },
      { open: '"', close: '"' },
      { open: '{', close: '}' },
    ],
    surroundingPairs: [
      { open: '[', close: ']' },
      { open: '"', close: '"' },
    ],
  })
}