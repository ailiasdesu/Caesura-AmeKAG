import copy
import yaml
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT))

from scripts.generate_platform_status import validate_matrix, DEFAULT_MATRIX_PATH

raw = yaml.safe_load(open(DEFAULT_MATRIX_PATH, encoding='utf-8'))

print("=== Test 1: Clean Matrix ===")
errs = validate_matrix(raw, ROOT)
print("Errors count:", len(errs))
assert len(errs) == 0, f"Expected 0 errors, got: {errs}"

print("=== Test 2: Invalid Enum ('almost-done') ===")
d2 = copy.deepcopy(raw)
d2['platforms']['windows']['capabilities']['build']['status'] = 'almost-done'
errs2 = validate_matrix(d2, ROOT)
print("Errors:", errs2)
assert any("invalid status 'almost-done'" in e for e in errs2)

print("=== Test 3: Missing Evidence commit in Verified Status ===")
d3 = copy.deepcopy(raw)
del d3['platforms']['windows']['capabilities']['build']['evidence']['commit']
errs3 = validate_matrix(d3, ROOT)
print("Errors:", errs3)
assert any("evidence commit" in e for e in errs3)

print("=== Test 4: Non-existent Document Path in Evidence ===")
d4 = copy.deepcopy(raw)
d4['platforms']['windows']['capabilities']['build']['evidence']['document'] = 'docs/fake/nonexistent_doc.md'
errs4 = validate_matrix(d4, ROOT)
print("Errors:", errs4)
assert any("referenced document does not exist" in e for e in errs4)

print("=== Test 5: iOS Real Device upgraded to verified (Violation) ===")
d5 = copy.deepcopy(raw)
d5['platforms']['ios']['capabilities']['real_device']['status'] = 'verified'
errs5 = validate_matrix(d5, ROOT)
print("Errors:", errs5)
assert any("must be 'hardware-gated'" in e for e in errs5)

print("=== Test 6: Missing Required Platform ('android') ===")
d6 = copy.deepcopy(raw)
del d6['platforms']['android']
errs6 = validate_matrix(d6, ROOT)
print("Errors:", errs6)
assert any("Missing required target platform: 'android'" in e for e in errs6)

print("=== Test 7: Invalid SHA commit ('not-a-sha!') ===")
d7 = copy.deepcopy(raw)
d7['platforms']['windows']['capabilities']['build']['evidence']['commit'] = 'not-a-sha!'
errs7 = validate_matrix(d7, ROOT)
print("Errors:", errs7)
assert any("is invalid (must be 7-40 hex chars)" in e for e in errs7)

print("\n>>> ALL 7 ADVERSARIAL VALIDATION TESTS PASSED <<<")
