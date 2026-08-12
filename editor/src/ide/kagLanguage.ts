// KAG Neo-Genesis language definition for Monaco.
// Registered once at startup; gives .ks files tag/param/comment/label
// syntax highlighting in the editor area (IDE P2-7).
import * as monaco from 'monaco-editor'

const KAG_COMMANDS = [
  // flow
  'if', 'elseif', 'elsif', 'else', 'endif', 'while', 'endwhile', 'for',
  'endfor', 'break', 'continue', 'switch', 'case', 'default', 'endswitch',
  'jump', 'call', 'return', 'link', 'label', 'macro', 'endmacro',
  'erasemacro', 'end', 'stop', 'eval', 'emb', 'iscript', 'endscript',
  'until',
  // text
  'ch', 'text', 'l', 'r', 'er', 'p', 'ruby', 'font', 'pt', 'button',
  'endbutton', 'sel', 'select', 'endselect', 'nameplate', 'textbox',
  'sprite_fade', 'sprite_move', 'sprite_scale', 'sprite_swap', 'history',
  'voice_wait', 'waitforclick', 'waitclick', 'reset', 'skip', 'auto', 'nvl',
  // layer
  'bg', 'fg', 'cl', 'image', 'position', 'layopt', 'ld', 'fadeout',
  'scroll', 'trans', 'move', 'moveto', 'quake', 'shake', 'vfx', 'flash',
  'vib', 'camera', 'particles',
  // audio
  'playbgm', 'playbgmstop', 'playse', 'playvoice', 'stopbgm', 'stopse',
  'fadebgm', 'fadevol', 'xfadebgm', 'play', 'bgm', 'se', 'voice',
  'voice_off', 'playstop', 'setbgmvolume', 'setsevolume', 'setvoicevolume',
  'waitsound', 'waitbgm',
  // system / resource / save
  'wait', 'delay', 's', 'chapter', 'ending', 'gallery', 'music', 'unlock',
  'rollback', 'toast', 'replay', 'save', 'load', 'listsaves', 'saveplace',
  'loadplace', 'preload', 'get_texture', 'is_loaded', 'is_pending',
  'flush_cache', 'video', 'stopvideo', 'ai_dialog', 'set', 'inc', 'random',
  'assert', 'sma_play', 'sma_stop',
  'clear', 'ct', 'endtag', 'endform', 'g', 'br', 'hr', 'cancel', 'close',
]

export function registerKagLanguage() {
  const id = 'kag'
  if (monaco.languages.getLanguages().some((l) => l.id === id)) return

  monaco.languages.register({ id, extensions: ['.ks'] })

  monaco.languages.setMonarchTokensProvider(id, {
    defaultToken: '',
    tokenPostfix: '.kag',
    keywords: KAG_COMMANDS,
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
