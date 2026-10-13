import copy
import io
import json
import pathlib
import subprocess
import sys
import tempfile
import time
import types
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from fingering.model import generate, load_models, make_windows  # noqa: E402
from fingering.worker import main, read_frame, write_frame  # noqa: E402


def note(identity, pitch, start, end, hand="right", manual=None):
    return {"id": identity, "midiNote": pitch, "startTick": start,
            "endTick": end, "hand": hand, "manualFinger": manual}


class FingeringModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.models = load_models(ROOT / ".tools" / "fingering")

    def request(self, notes):
        return {"version": 1, "jobId": "test-job", "songId": "song",
                "fingerprint": "test", "ppq": 480, "notes": notes}

    def test_digits_and_identity(self):
        notes = [note(1, 60, 0, 120), note(2, 62, 120, 240, "left")]
        before = copy.deepcopy(notes)
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        self.assertEqual(notes, before)
        self.assertEqual([item["id"] for item in result["assignments"]], [1, 2])
        self.assertTrue(all(a["finger"] is None or 1 <= a["finger"] <= 5
                            for a in result["assignments"]))
        self.assertEqual(result["model"]["name"], "ArGNN")
        manifest = json.loads((ROOT / "fingering" / "dependencies.json").read_text())
        self.assertEqual(result["model"]["checkpointHashes"],
                         {hand: manifest["checkpoints"][hand]["sha256"]
                          for hand in ("left", "right")})

    def test_worker_preserves_numeric_native_ids(self):
        ids = [0, 2**53 + 1, 2**64 - 1]
        request = self.request([note(identity, 60 + i, i * 120, (i + 1) * 120,
                                     "left" if i == 1 else "right")
                                for i, identity in enumerate(ids)])
        incoming = io.BytesIO()
        write_frame(incoming, request)
        process = subprocess.run([sys.executable, "-I", str(ROOT / "fingering/worker.py"),
                                  "--runtime-dir", str(ROOT / ".tools/fingering")],
                                 input=incoming.getvalue(), capture_output=True, timeout=30)
        self.assertEqual(process.returncode, 0, process.stderr.decode())
        self.assertEqual(process.stderr, b"")
        outgoing = io.BytesIO(process.stdout)
        frames = []
        while (frame := read_frame(outgoing)) is not None:
            frames.append(frame)
        self.assertEqual(frames[-1]["type"], "result")
        self.assertEqual([a["id"] for a in frames[-1]["assignments"]], ids)
        self.assertTrue(all(type(a["id"]) is int for a in frames[-1]["assignments"]))

    def test_c8_and_manual_anchors_with_actual_models(self):
        for hand in ("right", "left"):
            with self.subTest(hand=hand):
                result = generate(self.request([note(1, 108, 0, 120, hand),
                                                note(2, 108, 240, 360, hand, 3)]),
                                  lambda *_: None, models=self.models)
                if hand == "right":
                    self.assertIn(result["assignments"][0]["finger"], range(1, 6))
                else:
                    self.assertEqual(result["assignments"][0]["reason"],
                                     "transformedPitchOutOfRange")
                self.assertEqual(result["assignments"][1]["finger"], 3)
        # Deliberately unsupported reflection must retain authoritative anchors.
        result = generate(self.request([note(1, 21, 0, 120, "left", 2),
                                        note(2, 108, 240, 360, "left", 3)]),
                          lambda *_: None, models=self.models)
        self.assertEqual([a["finger"] for a in result["assignments"]], [2, 3])
        chord = [note(i, 60 + i, 0, 120, manual=4 if i == 0 else None)
                 for i in range(6)]
        result = generate(self.request(chord), lambda *_: None, models=self.models)
        self.assertEqual(result["assignments"][0]["finger"], 4)
        self.assertTrue(all(a["reason"] == "oversizedChord"
                            for a in result["assignments"][1:]))

    def test_invalid_native_ids_rejected(self):
        for identity in (True, False, -1, 2**64, 1.0, "1", "", None):
            with self.subTest(identity=identity):
                with self.assertRaisesRegex(ValueError, "uint64"):
                    generate(self.request([note(identity, 60, 0, 120)]),
                             lambda *_: None, models=self.models)

    def test_deterministic_both_hands(self):
        notes = [note(i, 55 + i * 2, i * 120, (i + 1) * 120,
                      "left" if i % 2 else "right") for i in range(6)]
        first = generate(self.request(notes), lambda *_: None, models=self.models)
        second = generate(self.request(notes), lambda *_: None, models=self.models)
        self.assertEqual(first["assignments"], second["assignments"])

    def test_chord_and_held_occupancy(self):
        notes = [note(1, 60, 0, 300), note(2, 64, 100, 200),
                 note(3, 67, 100, 200)]
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        assigned = {a["id"]: a["finger"] for a in result["assignments"]}
        self.assertEqual(len({assigned[k] for k in (1, 2, 3)
                              if assigned[k] is not None}),
                         sum(assigned[k] is not None for k in (1, 2, 3)))

    def test_anchor_and_window_boundary(self):
        notes = [note(i, 45 + i % 24, i * 10,
                      4000 if i == 0 else i * 10 + 15,
                      manual=3 if i == 0 else (2 if i == 260 else None))
                 for i in range(700)]
        notes[259]["endTick"] = 2600
        windows = make_windows(notes)
        self.assertTrue(all(len(window) <= 256 for window in windows))
        self.assertTrue(all(len(set(windows[i - 1]) & set(windows[i])) <= 64
                            for i in range(1, len(windows))))
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        self.assertEqual(next(a["finger"] for a in result["assignments"]
                              if a["id"] == 0), 3)
        self.assertEqual(next(a["finger"] for a in result["assignments"]
                              if a["id"] == 260), 2)
        self.assertNotEqual(next(a["finger"] for a in result["assignments"]
                                 if a["id"] == 0),
                            next(a["finger"] for a in result["assignments"]
                                 if a["id"] == 260))

    def test_oversized_chord_unresolved(self):
        notes = [note(i, 40 + i, 0, 100) for i in range(6)]
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        self.assertTrue(all(a["finger"] is None and a.get("reason") == "oversizedChord"
                            for a in result["assignments"]))

    def test_large_unison_chord_does_not_drop_later_windows(self):
        notes = [note(i, 60, i, i + 1) for i in range(256)]
        notes += [note(256 + i, 62, 300, 400) for i in range(220)]
        windows = make_windows(notes)
        self.assertEqual(set(i for window in windows for i in window), set(range(len(notes))))
        self.assertTrue(all(len(window) <= 256 for window in windows))

    def test_future_manual_anchor_reserves_held_finger_across_windows(self):
        notes = [note(i, pitch, 0, 10000)
                 for i, pitch in enumerate((60, 62, 64, 66))]
        notes += [note(10 + i, 70, 10 + i * 10, 20 + i * 10)
                  for i in range(300)]
        notes.append(note(1000, 72, 4000, 4100, manual=1))
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        assigned = {a["id"]: a["finger"] for a in result["assignments"]}
        self.assertEqual(assigned[1000], 1)
        self.assertTrue(all(assigned[i] not in (None, 1) for i in range(4)))

    def test_future_same_pitch_manual_anchor_forces_held_digit(self):
        notes = [note(1, 60, 0, 10000)]
        notes += [note(10 + i, 70, 10 + i * 10, 20 + i * 10)
                  for i in range(300)]
        notes.append(note(1000, 60, 4000, 4100, manual=5))
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        assigned = {a["id"]: a["finger"] for a in result["assignments"]}
        self.assertEqual(assigned[1], 5)
        self.assertEqual(assigned[1000], 5)

    def test_conflicting_future_same_pitch_anchors_leave_held_unresolved(self):
        notes = [note(1, 60, 0, 10000),
                 note(2, 60, 100, 200, manual=1),
                 note(3, 60, 300, 400, manual=5)]
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        assigned = {a["id"]: a for a in result["assignments"]}
        self.assertIsNone(assigned[1]["finger"])
        self.assertTrue(assigned[1].get("reason"))
        self.assertEqual(assigned[2]["finger"], 1)
        self.assertEqual(assigned[3]["finger"], 5)

    def test_missing_or_corrupt_checkpoint(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(RuntimeError, "checkpoint"):
                load_models(pathlib.Path(directory))
            for hand in ("left", "right"):
                path = pathlib.Path(directory) / "models" / f"{hand}_ArGNN.pth"
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"not a checkpoint")
            with self.assertRaisesRegex(RuntimeError, "checkpoint"):
                load_models(pathlib.Path(directory))

    def test_protocol_rejects_truncated_frame(self):
        with self.assertRaisesRegex(ValueError, "truncated"):
            read_frame(io.BytesIO(b"\x05\x00\x00\x00{}"))

    def test_heartbeat_preserves_latest_progress(self):
        incoming, outgoing = io.BytesIO(), io.BytesIO()
        write_frame(incoming, self.request([note(1, 60, 0, 120)]))
        incoming.seek(0)

        def slow_generate(request, progress, models):
            progress("decoding", 7, 10)
            time.sleep(5.2)
            return {"version": 1, "jobId": request["jobId"], "type": "result"}

        with patch("sys.stdin", types.SimpleNamespace(buffer=incoming)), \
                patch("sys.stdout", types.SimpleNamespace(buffer=outgoing)), \
                patch("fingering.model.load_models", return_value=self.models), \
                patch("fingering.model.generate", side_effect=slow_generate):
            self.assertEqual(main(["--runtime-dir", str(ROOT / ".tools/fingering")]), 0)
        outgoing.seek(0)
        frames = []
        while (frame := read_frame(outgoing)) is not None:
            frames.append(frame)
        self.assertEqual([(f["stage"], f["completed"], f["total"])
                          for f in frames if f["type"] == "progress"][-1],
                         ("decoding", 7, 10))

    def test_missing_torch_dependency_is_actionable_setup_error(self):
        incoming, outgoing = io.BytesIO(), io.BytesIO()
        write_frame(incoming, self.request([note(1, 60, 0, 480)]))
        incoming.seek(0)
        real_import = __import__
        def unavailable(name, *args, **kwargs):
            if name == "fingering.model":
                raise OSError("WinError 126: DLL cannot load")
            return real_import(name, *args, **kwargs)
        with patch("sys.stdin", types.SimpleNamespace(buffer=incoming)), \
                patch("sys.stdout", types.SimpleNamespace(buffer=outgoing)), \
                patch("sys.stderr", io.StringIO()), patch("builtins.__import__", side_effect=unavailable):
            self.assertEqual(main(["--runtime-dir", str(ROOT / ".tools/fingering")]), 1)
        outgoing.seek(0)
        result = read_frame(outgoing)
        self.assertEqual(result["type"], "error")
        self.assertEqual(result["code"], "setup_missing")
        self.assertIn("Microsoft Visual C++ v14 x64", result["message"])
        self.assertIn("normal library and playback remain usable", result["message"])

    def test_constant_onsets_and_pitches_are_finite(self):
        notes = [note(i, 60, 100, 200) for i in range(8)]
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        self.assertEqual(len(result["assignments"]), len(notes))
        self.assertEqual(len({a["finger"] for a in result["assignments"]}), 1)
        self.assertIsNotNone(result["assignments"][0]["finger"])


if __name__ == "__main__":
    unittest.main()
