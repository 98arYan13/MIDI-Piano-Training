"""CPU inference for the pretrained ArGNN piano-fingering checkpoints.

The layer equations, feature embedding, and autoregressive dimensions follow
PRamoneda/Automatic-Piano-Fingering at the revision recorded in dependencies.json.
"""

from __future__ import annotations

import hashlib
import json
import math
import pathlib
import warnings
from typing import Callable

# PyTorch probes optional NumPy on import and emits this one warning when the
# optional package is absent; inference here uses only Torch tensors.
warnings.filterwarnings("ignore", message="Failed to initialize NumPy: No module named 'numpy'",
                        category=UserWarning)
import torch
from torch import nn
from torch.nn import functional as F


MAX_WINDOW = 256
WINDOW_CONTEXT = 64
BEAM_WIDTH = 10
_GRAPH_TYPES = 4


class GatedGraph(nn.Module):
    """Checkpoint-compatible copy of upstream nns/GGCN.py GatedGraph."""

    def __init__(self, size: int = 64) -> None:
        super().__init__()
        self.size = size
        self.secondary_size = size
        self.num_type = _GRAPH_TYPES
        self.ba = nn.Parameter(torch.empty(_GRAPH_TYPES, size))
        self.bw = nn.Parameter(torch.empty(size * 3))
        self.wz_wr_wh = nn.Parameter(torch.empty(_GRAPH_TYPES, size, size * 3))
        self.uz_ur = nn.Parameter(torch.empty(size, size * 2))
        self.uh = nn.Parameter(torch.empty(size, size))
        nn.init.zeros_(self.ba)
        nn.init.zeros_(self.bw)
        nn.init.normal_(self.wz_wr_wh, std=math.sqrt(2 / (size * _GRAPH_TYPES + size)))
        nn.init.normal_(self.uz_ur, std=math.sqrt(2 / (size + size)))
        nn.init.normal_(self.uh, std=math.sqrt(2 / (size + size)))

    def forward(self, inputs: torch.Tensor, edge_matrix: torch.Tensor) -> torch.Tensor:
        batch, length, _ = inputs.shape
        edge_batch = edge_matrix.reshape(batch * self.num_type, length, length)
        broadcast_inputs = inputs.repeat(1, self.num_type, 1).reshape(
            batch * self.num_type, length, self.size
        )
        activation = torch.matmul(edge_batch.transpose(1, 2), broadcast_inputs)
        activation = activation + self.ba.unsqueeze(1).repeat(batch, 1, 1)
        weights = self.wz_wr_wh.unsqueeze(0).repeat(batch, 1, 1, 1).reshape(
            batch * self.num_type, self.size, self.size * 3
        )
        gates = (torch.bmm(activation, weights) + self.bw).reshape(
            batch, self.num_type, length, self.size * 3
        )
        z, r, candidate = gates.split(self.size, dim=-1)
        z, r, candidate = z.sum(1), r.sum(1), candidate.sum(1)
        input_z, input_r = torch.matmul(inputs, self.uz_ur).split(self.size, -1)
        z = torch.sigmoid(z + input_z)
        r = torch.sigmoid(r + input_r)
        hidden = torch.tanh(candidate + torch.matmul(r * inputs, self.uh))
        return (1 - z) * inputs + z * hidden


class ArGNN(nn.Module):
    """Minimal module tree with upstream checkpoint state-dict names."""

    def __init__(self) -> None:
        super().__init__()
        self.embedding = nn.Module()
        self.embedding.emb = nn.Embedding(127, 64)
        self.encoder = nn.Module()
        self.encoder.gnn1 = GatedGraph()
        self.encoder.gnn2 = GatedGraph()
        self.encoder.gnn3 = GatedGraph()
        self.decoder = nn.Module()
        self.decoder.ar_lstm = nn.LSTM(128, 64, batch_first=True)
        self.decoder.FC = nn.Linear(64, 5)
        self.decoder.class_embedding = nn.Embedding(5, 64)

    def encode(self, notes: torch.Tensor, edges: torch.Tensor) -> torch.Tensor:
        hidden = self.embedding.emb((notes * 127).long().squeeze(-1))
        hidden = self.encoder.gnn1(hidden, edges)
        hidden = self.encoder.gnn2(hidden, edges)
        return self.encoder.gnn3(hidden, edges)


def _sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_models(root: pathlib.Path) -> dict[str, torch.nn.Module]:
    """Load and verify both pinned checkpoint state dictionaries on CPU."""
    root = pathlib.Path(root)
    manifest = json.loads((pathlib.Path(__file__).resolve().parent / "dependencies.json").read_text())
    expected = manifest["checkpoints"]
    torch.set_num_threads(2)
    try:
        torch.set_num_interop_threads(1)
    except RuntimeError:
        pass
    models = {}
    for hand in ("left", "right"):
        path = root / "models" / f"{hand}_ArGNN.pth"
        if not path.is_file():
            raise RuntimeError(f"ArGNN checkpoint missing: {path}")
        digest = _sha256(path)
        if digest != expected[hand]["sha256"]:
            raise RuntimeError(f"ArGNN checkpoint hash mismatch: {hand}")
        model = ArGNN()
        try:
            # The upstream artifact stores a criterion module beside its
            # tensors. Allowlist that inert nn class, but keep weights_only on.
            torch.serialization.add_safe_globals([nn.NLLLoss])
            checkpoint = torch.load(path, map_location="cpu", weights_only=True)
            state = checkpoint["model_state_dict"]
            model.load_state_dict(state, strict=True)
        except Exception as error:
            raise RuntimeError(f"ArGNN checkpoint incompatible: {hand}: {error}") from error
        model.eval()
        models[hand] = model
    return models


def _first_left_symmetric(note: int) -> int:
    offsets = (16, 14, 12, 10, 8, 6, 4, 2, 0, -2, -4, -6)
    octave = (note - 60) // 12
    return note + offsets[note % 12] - (2 * octave * 12) - 24


def _left_transform(pitches: list[int]) -> list[int]:
    if not pitches:
        return []
    transformed = [_first_left_symmetric(pitches[0])]
    for previous, current in zip(pitches, pitches[1:]):
        transformed.append(transformed[-1] - (current - previous))
    return transformed


def _ordered_indices(notes: list[dict]) -> list[int]:
    return sorted(range(len(notes)), key=lambda index: (
        int(notes[index]["startTick"]), int(notes[index]["midiNote"]),
        str(notes[index]["id"]),
    ))


def make_windows(notes: list[dict], max_notes: int = MAX_WINDOW,
                 context: int = WINDOW_CONTEXT, ppq: int = 480) -> list[list[int]]:
    """Return ordered original indexes in chord-safe bounded windows."""
    if not notes:
        return []
    if max_notes < 1 or not 0 <= context < max_notes:
        raise ValueError("window limits must satisfy 0 <= context < max_notes")
    groups: list[list[int]] = []
    for index in _ordered_indices(notes):
        if groups and notes[groups[-1][0]]["startTick"] == notes[index]["startTick"]:
            groups[-1].append(index)
        else:
            groups.append([index])
    groups = [group for group in groups if len(group) <= max_notes]
    windows: list[list[int]] = []
    cursor = 0
    while cursor < len(groups):
        start, end, size = cursor, cursor, 0
        while start > 0 and size + len(groups[start - 1]) <= context:
            start -= 1
            size += len(groups[start])
        # Trim whole context groups so the next chord always fits intact.
        while start < cursor and size + len(groups[cursor]) > max_notes:
            size -= len(groups[start])
            start += 1
        while end < len(groups) and size + len(groups[end]) <= max_notes:
            size += len(groups[end])
            end += 1
        if end == cursor:
            break
        # Prefer a recent event boundary following a full-beat rest.
        rest_boundaries = []
        for boundary in range(cursor + 1, end):
            prior, current = groups[boundary - 1], groups[boundary]
            prior_end = max(int(notes[i]["endTick"]) for i in prior)
            if int(notes[current[0]]["startTick"]) - prior_end >= ppq:
                rest_boundaries.append(boundary)
        if rest_boundaries:
            end = rest_boundaries[-1]
        windows.append([index for group in groups[start:end] for index in group])
        cursor = end
    return windows


def _edge_matrix(notes: list[dict], indices: list[int]) -> torch.Tensor:
    length = len(indices)
    edges = torch.zeros((1, _GRAPH_TYPES, length, length), dtype=torch.float32)
    onsets = [int(notes[index]["startTick"]) for index in indices]
    for i, onset in enumerate(onsets):
        for j, other in enumerate(onsets):
            if i != j and onset == other:
                edges[0, 0, i, j] = 1
        later = [other for other in onsets if other > onset]
        if later:
            next_onset = min(later)
            for j, other in enumerate(onsets):
                if other == next_onset:
                    edges[0, 1, i, j] = 1
                    edges[0, 3, j, i] = 1
    edges[0, 2] = torch.eye(length)
    return edges


def _active(notes: list[dict], assignments: dict[int, int], before_tick: int,
            allowed: set[int] | None = None) -> dict[int, set[int]]:
    occupancy: dict[int, set[int]] = {}
    for index, finger in assignments.items():
        if allowed is not None and index not in allowed:
            continue
        note = notes[index]
        if int(note["startTick"]) < before_tick < int(note["endTick"]):
            occupancy.setdefault(int(note["midiNote"]), set()).add(finger)
    return occupancy


def _valid_fingers(notes: list[dict], path: list[int | None], index: int,
                   incoming: dict[int, set[int]], forced: int | None) -> list[int]:
    pitch = int(notes[index]["midiNote"])
    occupied: dict[int, int] = {}
    for active_pitch, fingers in incoming.items():
        if active_pitch == pitch:
            continue
        for finger in fingers:
            occupied[finger] = active_pitch
    for previous_index, finger in enumerate(path):
        if finger is None:
            continue
        previous = notes[previous_index]
        if int(previous["endTick"]) > int(notes[index]["startTick"]):
            previous_pitch = int(previous["midiNote"])
            if previous_pitch != pitch:
                occupied[finger] = previous_pitch
            else:
                occupied[finger] = pitch
    same_pitch = set(incoming.get(pitch, set()))
    same_pitch.update(
        finger for previous_index, finger in enumerate(path)
        if finger is not None and int(notes[previous_index]["midiNote"]) == pitch
        and int(notes[previous_index]["endTick"]) > int(notes[index]["startTick"])
    )
    if len(same_pitch) > 1:
        return []
    if same_pitch:
        required = next(iter(same_pitch))
        return [required] if forced in (None, required) else []
    candidates = [forced] if forced is not None else list(range(5))
    return [finger for finger in candidates if finger not in occupied]


def _decode_window(model: ArGNN, notes: list[dict], indices: list[int], hand: str,
                   anchors: dict[int, int], incoming: list[tuple[int, int, int, int]],
                   progress: Callable[[str, int, int], None], done: int,
                   total: int) -> dict[int, tuple[int | None, str | None]]:
    if not indices:
        return {}
    pitches = [int(notes[index]["midiNote"]) for index in indices]
    if hand == "left":
        pitches = _left_transform(pitches)
    # The left-hand augmentation in upstream drops sequences outside the
    # playable keyboard after reflection; keep the same 21..107 support.
    if any(not 21 <= pitch < 108 for pitch in pitches):
        return {index: (None, "transformedPitchOutOfRange") for index in indices}
    # Upstream normalizes onset/duration for other embeddings; this ArGNN uses
    # only emb_pitch, while graph edges encode onset relationships directly.
    note_tensor = torch.tensor(pitches, dtype=torch.float32).reshape(1, -1, 1) / 127.0
    edges = _edge_matrix(notes, indices)
    with torch.inference_mode():
        encoded = model.encode(note_tensor, edges)
        progress("encoding", done, total)
        beams = [{"score": 0.0, "h": torch.zeros((1, 1, 64)),
                  "c": torch.zeros((1, 1, 64)), "previous": torch.zeros((1, 1, 64)),
                  "path": [], "reasons": []}]
        local_notes = [notes[index] for index in indices]
        for position, original in enumerate(indices):
            tick = int(notes[original]["startTick"])
            active: dict[int, set[int]] = {}
            for pitch, start, end, finger in incoming:
                if start < tick < end:
                    active.setdefault(pitch, set()).add(finger)
            forced = anchors.get(original)
            next_beams = []
            current = encoded[:, position:position + 1]
            for beam in beams:
                logits, (hidden, cell) = model.decoder.ar_lstm(
                    torch.cat((current, beam["previous"]), dim=-1), (beam["h"], beam["c"])
                )
                log_probs = F.log_softmax(model.decoder.FC(logits[0, 0]), dim=-1)
                assignments = {indices[i]: finger for i, finger in enumerate(beam["path"])
                               if finger is not None}
                in_use = _active(notes, assignments, tick)
                for pitch, fingers in active.items():
                    in_use.setdefault(pitch, set()).update(fingers)
                for finger in _valid_fingers(local_notes, beam["path"], position, in_use,
                                             forced):
                    next_beams.append({
                        "score": beam["score"] + float(log_probs[finger]),
                        "h": hidden.clone(), "c": cell.clone(),
                        "previous": model.decoder.class_embedding(
                            torch.tensor([[finger]], dtype=torch.long)),
                        "path": beam["path"] + [finger],
                        "reasons": beam["reasons"] + [None],
                    })
            if not next_beams:
                # Keep the sequence clock moving without inventing a digit.
                beam = max(beams, key=lambda candidate: candidate["score"])
                logits, (hidden, cell) = model.decoder.ar_lstm(
                    torch.cat((current, beam["previous"]), dim=-1), (beam["h"], beam["c"])
                )
                reason = "manualAnchorConflict" if forced is not None else "fingerOccupancyConflict"
                next_beams = [{"score": beam["score"], "h": hidden, "c": cell,
                               "previous": torch.zeros((1, 1, 64)),
                               "path": beam["path"] + [None],
                               "reasons": beam["reasons"] + [reason]}]
            beams = sorted(next_beams, key=lambda candidate: candidate["score"], reverse=True)[:BEAM_WIDTH]
            progress("decoding", done + position + 1, total)
        best = max(beams, key=lambda candidate: candidate["score"])
    return {original: (None if digit is None else digit + 1, reason)
            for original, digit, reason in zip(indices, best["path"], best["reasons"])}


def generate(request: dict, progress: Callable[[str, int, int], None],
             models: dict[str, torch.nn.Module] | None = None) -> dict:
    """Generate identity-preserving, constrained predictions for both hands."""
    if request.get("version") != 1 or not isinstance(request.get("notes"), list):
        raise ValueError("invalid fingering request")
    if models is None:
        runtime = pathlib.Path(request.get("runtimeDir", ".tools/fingering"))
        models = load_models(runtime)
    source_notes = request["notes"]
    if any(not isinstance(note, dict) or not isinstance(note.get("id"), str)
           or not note["id"] for note in source_notes):
        raise ValueError("every note needs a non-empty string id")
    ids = [note["id"] for note in source_notes]
    if len(set(ids)) != len(ids):
        raise ValueError("note IDs must be unique")
    notes = [dict(note) for note in source_notes]
    assignments: dict[int, tuple[int | None, str | None]] = {}
    unsupported = 0
    for index, note in enumerate(notes):
        pitch, start, end = int(note["midiNote"]), int(note["startTick"]), int(note["endTick"])
        manual = note.get("manualFinger")
        if manual is not None and (isinstance(manual, bool) or not isinstance(manual, int)
                                   or not 1 <= manual <= 5):
            raise ValueError(f"invalid manual finger for note {note['id']}")
        hand = note.get("hand")
        if hand not in ("left", "right"):
            assignments[index] = (None, "unsupportedHand")
            unsupported += 1
        elif not 21 <= pitch <= 107:
            assignments[index] = (None, "unsupportedPitch")
            unsupported += 1
        elif start < 0 or end < start:
            raise ValueError(f"invalid timing for note {note['id']}")
    for hand in ("left", "right"):
        hand_indexes = [i for i, note in enumerate(notes)
                        if note.get("hand") == hand and i not in assignments]
        hand_notes = [notes[i] for i in hand_indexes]
        windows = make_windows(hand_notes, ppq=int(request.get("ppq", 480)))
        mapped_windows = [[hand_indexes[position] for position in window] for window in windows]
        chord_groups: dict[int, list[int]] = {}
        for index in hand_indexes:
            chord_groups.setdefault(int(notes[index]["startTick"]), []).append(index)
        unsupported_groups = {tick for tick, group in chord_groups.items()
                              if len(group) > MAX_WINDOW
                              or len({int(notes[i]["midiNote"]) for i in group}) > 5}
        for tick in unsupported_groups:
            for index in chord_groups[tick]:
                assignments[index] = (None, "oversizedChord")
                unsupported += 1
        previous: dict[int, int] = {}
        total = sum(len(window) for window in mapped_windows)
        done = 0
        for window in mapped_windows:
            filtered = [index for index in window
                        if int(notes[index]["startTick"]) not in unsupported_groups]
            if not filtered:
                continue
            if len(filtered) > MAX_WINDOW:
                for index in filtered:
                    assignments[index] = (None, "oversizedChord")
                    unsupported += 1
                continue
            first_tick = min(int(notes[i]["startTick"]) for i in filtered)
            anchors = {index: int(notes[index]["manualFinger"]) - 1
                       for index in filtered
                       if notes[index].get("manualFinger") is not None
                       and 1 <= int(notes[index]["manualFinger"]) <= 5}
            anchors.update({index: finger - 1 for index, finger in previous.items()
                            if index in filtered})
            external_active: list[tuple[int, int, int, int]] = []
            for index, finger in previous.items():
                if int(notes[index]["endTick"]) > first_tick:
                    external_active.append((int(notes[index]["midiNote"]),
                                            int(notes[index]["startTick"]),
                                            int(notes[index]["endTick"]), finger - 1))
            result = _decode_window(models[hand], notes, filtered, hand, anchors,
                                    external_active, progress, done, total)
            for index, (finger, reason) in result.items():
                if index in anchors:
                    if finger is None:
                        assignments[index] = (None, reason)
                    elif notes[index].get("manualFinger") is not None:
                        assignments[index] = (int(notes[index]["manualFinger"]), "manualAnchor")
                elif index not in previous:
                    assignments[index] = (finger, reason)
                if finger is not None:
                    previous[index] = finger
            done += len(window)
    output = []
    for index, note in enumerate(notes):
        finger, reason = assignments.get(index, (None, "noValidCandidate"))
        item = {"id": note["id"], "hand": note.get("hand"), "finger": finger}
        if finger is None:
            item["reason"] = reason or "noValidCandidate"
        elif note.get("manualFinger") is not None:
            item["reason"] = "manualAnchor"
        output.append(item)
    manifest = json.loads((pathlib.Path(__file__).resolve().parent / "dependencies.json").read_text())
    return {"version": 1, "jobId": request.get("jobId"), "type": "result",
            "assignments": output,
            "model": {"name": "ArGNN", "revision": manifest["upstream"]["revision"],
                      "checkpointHashes": {hand: manifest["checkpoints"][hand]["sha256"]
                                           for hand in ("left", "right")}}}
