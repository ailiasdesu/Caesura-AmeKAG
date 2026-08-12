# Caesura Editor — Web IDE for the Caesura (AmeKAG) engine

VS Code-style web IDE for the engine, built with Vite + React + TypeScript
and **Monaco Editor** (the VS Code editor core). It combines a powerful
code editor with visual editing (live frame preview, Live2D model loading)
against the engine's HTTP RPC server (`localhost:9876`, 18 routes).

> Roadmap: market-analysis P2-7 (visual editor front-end). The RPC backend
> was already complete; this front-end adds the IDE shell around it.

## Features

- **Workbench layout** (VS Code model): activity bar → sidebar
  (Explorer / Run-and-Debug / Visual Preview) → editor area with tabs →
  output panel → status bar
- **Monaco editor** with a custom **KAG Neo-Genesis** language definition
  (`.ks`): tag/param/comment/label highlighting, bracket matching,
  auto-closing pairs
- **Explorer**: live asset tree (scripts / images / audio) from
  `/api/assets`; double-click a `.ks` to open it in the editor
- **Run and Debug**: run Lua snippets, stop, KAG breakpoints
  (`/api/debug/*`), live scene/token/paused state, Continue
- **Visual Preview**: periodic frame capture (`/api/debug/getFrame`) and
  Live2D model listing/loading (`/api/live2d/*`)
- **Output panel**: engine log tail (polled `/api/logs`)
- **Status bar**: engine connection, current scene, token index, pause state

## Usage

### 1. Start the engine in editor mode

```bash
./build/Debug/CaesuraAmeKAG.exe --editor          # HTTP on :9876 (hidden GPU window)
# optional bearer auth:
#   CAESURA_EDITOR_TOKEN=secret ./build/Debug/CaesuraAmeKAG.exe --editor
```

### 2. Run the editor front-end

```bash
cd editor
npm install
npm run dev        # http://localhost:5173  (dev proxy → engine :9876)
```

Production build:

```bash
npm run build      # static bundle in editor/dist/
npm run preview    # serve the bundle (same /api proxy in dev only;
                   # deploy behind the engine origin for same-origin API)
```

### 3. Connect

The header shows the engine state; enter the bearer token if the engine
was started with `CAESURA_EDITOR_TOKEN`. Double-click a `.ks` in the
Explorer to edit; use Run and Debug to drive the engine.

## Project layout

```
editor/
├── index.html
├── vite.config.ts          # /api proxy → localhost:9876
├── package.json
└── src/
    ├── App.tsx             # workbench shell
    ├── store.ts            # zustand IDE state (tabs/views/engine)
    ├── lib/rpc.ts          # typed engine RPC client (18 routes)
    ├── ide/
    │   ├── ActivityBar.tsx # left activity bar
    │   ├── StatusBar.tsx   # bottom status line
    │   ├── ExplorerView.tsx
    │   ├── DebugView.tsx   # run/stop/breakpoints/continue
    │   ├── VisualView.tsx  # frame preview + Live2D
    │   ├── EditorArea.tsx  # Monaco multi-tab editor
    │   ├── OutputPanel.tsx # engine log tail
    │   └── kagLanguage.ts  # KAG .ks Monaco language definition
    └── components/ConnectionPanel.tsx
```

## API contract

The front-end mirrors `docs/api/editor-api-reference.md` — see
`src/lib/rpc.ts` for the typed client and `src/rpc/EditorServer.cpp` for
the engine side.
