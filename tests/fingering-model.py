import copy
import io
import json
import pathlib
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
        notes = [note("a", 60, 0, 120), note("b", 62, 120, 240, "left")]
        before = copy.deepcopy(notes)
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        self.assertEqual(notes, before)
        self.assertEqual([item["id"] for item in result["assignments"]], ["a", "b"])
        self.assertTrue(all(a["finger"] is None or 1 <= a["finger"] <= 5
                            for a in result["assignments"]))
        self.assertEqual(result["model"]["name"], "ArGNN")
        manifest = json.loads((ROOT / "fingering" / "dependencies.json").read_text())
        self.assertEqual(result["model"]["checkpointHashes"],
                         {hand: manifest["checkpoints"][hand]["sha256"]
                          for hand in ("left", "right")})

    def test_deterministic_both_hands(self):
        notes = [note(str(i), 55 + i * 2, i * 120, (i + 1) * 120,
                      "left" if i % 2 else "right") for i in range(6)]
        first = generate(self.request(notes), lambda *_: None, models=self.models)
        second = generate(self.request(notes), lambda *_: None, models=self.models)
        self.assertEqual(first["assignments"], second["assignments"])

    def test_chord_and_held_occupancy(self):
        notes = [note("held", 60, 0, 300), note("chord", 64, 100, 200),
                 note("third", 67, 100, 200)]
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        assigned = {a["id"]: a["finger"] for a in result["assignments"]}
        self.assertEqual(len({assigned[k] for k in ("held", "chord", "third")
                              if assigned[k] is not None}),
                         sum(assigned[k] is not None for k in ("held", "chord", "third")))

    def test_anchor_and_window_boundary(self):
        notes = [note(str(i), 45 + i % 24, i * 10,
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
                              if a["id"] == "0"), 3)
        self.assertEqual(next(a["finger"] for a in result["assignments"]
                              if a["id"] == "260"), 2)
        self.assertNotEqual(next(a["finger"] for a in result["assignments"]
                                 if a["id"] == "0"),
                            next(a["finger"] for a in result["assignments"]
                                 if a["id"] == "260"))

    def test_oversized_chord_unresolved(self):
        notes = [note(str(i), 40 + i, 0, 100) for i in range(6)]
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        self.assertTrue(all(a["finger"] is None and a.get("reason") == "oversizedChord"
                            for a in result["assignments"]))

    def test_large_unison_chord_does_not_drop_later_windows(self):
        notes = [note(str(i), 60, i, i + 1) for i in range(256)]
        notes += [note(f"unison-{i}", 62, 300, 400) for i in range(220)]
        windows = make_windows(notes)
        self.assertEqual(set(i for window in windows for i in window), set(range(len(notes))))
        self.assertTrue(all(len(window) <= 256 for window in windows))

    def test_future_manual_anchor_reserves_held_finger_across_windows(self):
        notes = [note(f"held-{i}", pitch, 0, 10000)
                 for i, pitch in enumerate((60, 62, 64, 66))]
        notes += [note(f"short-{i}", 70, 10 + i * 10, 20 + i * 10)
                  for i in range(300)]
        notes.append(note("future-anchor", 72, 4000, 4100, manual=1))
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        assigned = {a["id"]: a["finger"] for a in result["assignments"]}
        self.assertEqual(assigned["future-anchor"], 1)
        self.assertTrue(all(assigned[f"held-{i}"] not in (None, 1) for i in range(4)))

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
        write_frame(incoming, self.request([note("a", 60, 0, 120)]))
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

    def test_constant_onsets_and_pitches_are_finite(self):
        notes = [note(str(i), 60, 100, 200) for i in range(8)]
        result = generate(self.request(notes), lambda *_: None, models=self.models)
        self.assertEqual(len(result["assignments"]), len(notes))
        self.assertEqual(len({a["finger"] for a in result["assignments"]}), 1)
        self.assertIsNotNone(result["assignments"][0]["finger"])


if __name__ == "__main__":
    unittest.main()
