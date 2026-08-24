# Module Coupling & Architecture Boundary Report

- **Target Commit**: `62132e783dd238752659d4227ff26b0235258ea9`
- **Audit Tool**: `scripts/count_coupling.py`
- **Result**: **16 / 16 modules fully compliant with AGENTS.md budgets**

| Module | Cross-Module #include Count | Architectural Budget | Status |
|--------|----------------------------|----------------------|--------|
| `archive` | 2 | ≤ 4 | PASS |
| `audio` | 2 | ≤ 4 | PASS |
| `debug` | 0 | ≤ 4 | PASS |
| `di` | 13 | ≤ 14 (Composition/DI) | PASS |
| `entry` | 14 | ≤ 14 (Composition Root) | PASS |
| `input` | 0 | ≤ 4 | PASS |
| `job` | 1 | ≤ 4 | PASS |
| `live2d` | 3 | ≤ 4 | PASS |
| `minigame` | 4 | ≤ 4 | PASS |
| `platform` | 0 | ≤ 4 | PASS |
| `render` | 4 | ≤ 4 | PASS |
| `resource` | 3 | ≤ 4 | PASS |
| `rpc` | 2 | ≤ 4 | PASS |
| `script` | 11 | ≤ 14 (Binding Layer) | PASS |
| `steam` | 0 | ≤ 4 | PASS |
| `storage` | 4 | ≤ 4 | PASS |
