# First-VN Cross-Platform Behavioral Parity Report

- **Target Commit**: `62132e783dd238752659d4227ff26b0235258ea9`
- **Comparator Tool**: `scripts/compare_platform_parity.py`
- **Parity Status**: **PASS (Verified=4, Gated=1, Failed=0)**
- **Unit Test Suite**: `tests/scripts/test_platform_parity.py` (10/10 passed)
- **E2E Acceptance Suite**: `scripts/verify_first_vn.sh` (13/13 passed)

| Platform | Tier | Status | Route A (Sun) | Route B (Rain) | Languages | Result |
|----------|------|--------|---------------|----------------|-----------|--------|
| Windows | 1 | `verified` | `sun/flag=1/sunset` | `rain/flag=0/rain_shelter` | zh, en, ja | PASS |
| Linux | 1 | `verified` | `sun/flag=1/sunset` | `rain/flag=0/rain_shelter` | zh, en, ja | PASS |
| Web | 1 | `verified` | `sun/flag=1/sunset` | `rain/flag=0/rain_shelter` | zh, en, ja | PASS |
| Android | 1 | `verified` | `sun/flag=1/sunset` | `rain/flag=0/rain_shelter` | zh, en, ja | PASS |
| iOS | 2 | `hardware-gated` | `sun/flag=1/sunset` | `rain/flag=0/rain_shelter` | zh, en, ja | GATED (Honest) |
