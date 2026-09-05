"""Lifecycle checks for processes owned by one validation invocation."""
from __future__ import annotations

from pathlib import Path
import os
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from validation_process import run_owned_command
import validation_process


class ValidationProcessTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="caesura-owned-process-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.stdout_path = self.root / "stdout.log"
        self.stderr_path = self.root / "stderr.log"
        self.ready = self.root / "child-ready"
        self.release = self.root / "release-child"
        self.escaped = self.root / "escaped-child"

    def invoke(self, argv, timeout=5):
        with self.stdout_path.open("wb") as out, self.stderr_path.open("wb") as err:
            return run_owned_command(argv, self.root, out, err, timeout)

    def parent_with_child(self, *, stay_alive=False, exit_code=0):
        # The child waits for a signal created only AFTER the API returns, so
        # the lifecycle assertion does not depend on a guessed startup delay.
        child = (
            "from pathlib import Path; import time; "
            f"ready=Path({str(self.ready)!r}); release=Path({str(self.release)!r}); "
            f"escaped=Path({str(self.escaped)!r}); "
            "ready.write_text('ready'); deadline=time.monotonic()+8\n"
            "while not release.exists() and time.monotonic()<deadline: time.sleep(0.01)\n"
            "if release.exists():\n"
            "    escaped.write_text('still running')\n"
            "    print('late child stdout', flush=True)\n"
            "    import sys; print('late child stderr', file=sys.stderr, flush=True)\n"
        )
        parent = (
            "from pathlib import Path; import subprocess,sys,time; "
            f"subprocess.Popen([sys.executable, '-c', {child!r}]); "
            f"ready=Path({str(self.ready)!r}); deadline=time.monotonic()+5\n"
            "while not ready.exists() and time.monotonic()<deadline: time.sleep(0.01)\n"
            "assert ready.exists(), 'child never started'\n"
            "print('parent ready', flush=True)\n"
            + ("time.sleep(20)\n" if stay_alive else "")
            + f"sys.exit({exit_code})\n"
        )
        return [sys.executable, "-c", parent]

    def assert_child_gone_and_logs_stable(self):
        self.assertTrue(self.ready.exists(), "the actual descendant must have run")
        stdout_before = self.stdout_path.read_bytes()
        stderr_before = self.stderr_path.read_bytes()
        self.release.write_text("go", encoding="utf-8")
        deadline = time.monotonic() + 0.6
        while time.monotonic() < deadline and not self.escaped.exists():
            time.sleep(0.01)
        self.assertFalse(self.escaped.exists(), "owned descendant survived API return")
        self.assertEqual(self.stdout_path.read_bytes(), stdout_before)
        self.assertEqual(self.stderr_path.read_bytes(), stderr_before)

    def test_preserves_real_exit_status_and_output(self):
        code = "import sys; print('stdout'); print('stderr', file=sys.stderr); sys.exit(23)"
        result = self.invoke([sys.executable, "-c", code])
        self.assertEqual(result, 23)
        self.assertIn(b"stdout", self.stdout_path.read_bytes())
        self.assertIn(b"stderr", self.stderr_path.read_bytes())

    def test_parent_exit_reclaims_descendants_before_returning(self):
        result = self.invoke(self.parent_with_child(exit_code=17))
        self.assertEqual(result, 17)
        self.assert_child_gone_and_logs_stable()

    def test_timeout_reclaims_descendants_and_preserves_timeout(self):
        argv = self.parent_with_child(stay_alive=True)
        with self.assertRaises(subprocess.TimeoutExpired) as caught:
            self.invoke(argv, timeout=1)
        self.assertEqual(caught.exception.cmd, argv)
        self.assertEqual(caught.exception.timeout, 1)
        self.assert_child_gone_and_logs_stable()

    def test_keyboard_interrupt_reclaims_descendants_then_propagates(self):
        original_wait = subprocess.Popen.wait
        interrupted = False

        def interrupt_wait(process, *args, **kwargs):
            nonlocal interrupted
            if not interrupted:
                interrupted = True
                deadline = time.monotonic() + 5
                while not self.ready.exists() and time.monotonic() < deadline:
                    time.sleep(0.01)
                self.assertTrue(self.ready.exists(), "interrupt only after a descendant starts")
                raise KeyboardInterrupt
            return original_wait(process, *args, **kwargs)

        with mock.patch.object(subprocess.Popen, "wait", interrupt_wait):
            with self.assertRaises(KeyboardInterrupt):
                self.invoke(self.parent_with_child(stay_alive=True))
        self.assert_child_gone_and_logs_stable()

    def test_arguments_remain_literal_without_a_shell(self):
        value = "literal & echo unsafe > injected.txt $(echo unexpected)"
        result = self.invoke([sys.executable, "-c", "import sys; print(sys.argv[1])", value])
        self.assertEqual(result, 0)
        self.assertIn(value.encode(), self.stdout_path.read_bytes())
        self.assertFalse((self.root / "injected.txt").exists())

    if os.name == "nt":
        def test_command_waits_for_job_assignment_before_starting(self):
            original_assign = validation_process._WindowsJob.assign

            def delayed_assign(job, process):
                time.sleep(0.2) # make the assign-before-launch race observable
                self.assertIsNone(process.poll())
                self.assertFalse(self.ready.exists(), "actual command escaped the handshake")
                original_assign(job, process)

            with mock.patch.object(validation_process._WindowsJob, "assign", delayed_assign):
                self.assertEqual(self.invoke(self.parent_with_child()), 0)
            self.assert_child_gone_and_logs_stable()

        def test_failed_job_assignment_reaps_launcher_without_starting_command(self):
            launchers = []

            def reject_assignment(job, process):
                launchers.append(process)
                raise OSError("injected job assignment failure")

            with mock.patch.object(validation_process._WindowsJob, "assign", reject_assignment):
                with self.assertRaisesRegex(OSError, "injected job assignment failure"):
                    self.invoke(self.parent_with_child())
            self.assertEqual(len(launchers), 1)
            self.assertIsNotNone(launchers[0].poll())
            self.assertFalse(self.ready.exists())
    else:
        def test_command_has_a_separate_owned_process_group(self):
            code = "import os; print(os.getpid(), os.getpgrp())"
            self.assertEqual(self.invoke([sys.executable, "-c", code]), 0)
            pid, group = map(int, self.stdout_path.read_text().split())
            self.assertEqual(pid, group)
            self.assertNotEqual(group, os.getpgrp())


if __name__ == "__main__":
    unittest.main()
