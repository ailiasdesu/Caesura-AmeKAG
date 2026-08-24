# Context — Caesura (AmeKAG) Platform & Runtime Sprint

## Environment
- OS: Windows
- Workspace Root: `d:\文件存放处\code\Caesura(AmeKAG)`
- Parent Conversation ID: `17331f2e-d6ff-4bc6-b4ad-44a0743e2567`
- Orchestrator Working Directory: `d:\文件存放处\code\Caesura(AmeKAG)\.agents\orchestrator_1`

## Key Documents
- `AGENTS.md`: Module boundaries, BackendRegistry access only, coupling limits, zero build errors, all tests green.
- `ORIGINAL_REQUEST.md`: Requirements for R1 (IME), R2 (Android AAB/Release), R3 (iOS/Metal), R4 (Stress & Baseline QA).
- `docs/plans/`: Historical execution records and roadmaps.

## Architecture Constraints
- 16 core modules: archive, audio, debug, di, entry, input, job, live2d, minigame, platform, render, resource, rpc, script, steam, storage.
- Each module only exposes symbols via `src/<module>/api/I<ModuleName>.h`.
- No cross-module implementation includes.
- BackendRegistry is the sole point of backend access.
- Composition Root: `src/main.cpp` + `src/entry/`.
