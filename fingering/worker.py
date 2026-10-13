"""Length-prefixed JSON worker for the native service."""

from __future__ import annotations

import argparse
import json
import pathlib
import struct
import sys
import threading
import time
import traceback

MAX_FRAME = 256 * 1024 * 1024


def read_frame(stream):
    def exact(size):
        parts = []
        remaining = size
        while remaining:
            block = stream.read(remaining)
            if not block:
                break
            parts.append(block)
            remaining -= len(block)
        return b"".join(parts)

    header = exact(4)
    if not header:
        return None
    if len(header) != 4:
        raise ValueError("truncated frame header")
    length = struct.unpack("<I", header)[0]
    if length > MAX_FRAME:
        raise ValueError("frame exceeds 256 MiB limit")
    body = exact(length)
    if len(body) != length:
        raise ValueError("truncated frame payload")
    try:
        return json.loads(body.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"invalid JSON frame: {error}") from error


def write_frame(stream, message):
    body = json.dumps(message, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(body) > MAX_FRAME:
        raise ValueError("outgoing frame exceeds 256 MiB limit")
    stream.write(struct.pack("<I", len(body)))
    stream.write(body)
    stream.flush()


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtime-dir", required=True, type=pathlib.Path)
    args = parser.parse_args(argv)
    # -I deliberately excludes the script directory and environment paths.
    trusted_root = pathlib.Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(trusted_root))
    lock = threading.Lock()
    request = None
    latest_progress = None

    def emit(message):
        nonlocal latest_progress
        with lock:
            if message.get("type") == "progress":
                latest_progress = dict(message)
            elif message.get("type") in ("result", "error"):
                latest_progress = None
            write_frame(sys.stdout.buffer, message)

    try:
        request = read_frame(sys.stdin.buffer)
        if request is None:
            return 0
        if not isinstance(request, dict):
            raise ValueError("request frame must be a JSON object")
        if request.get("version") != 1:
            raise ValueError("unsupported protocol version")
        started = time.monotonic()
        stopping = threading.Event()

        def heartbeat():
            while not stopping.wait(5):
                with lock:
                    if latest_progress is not None:
                        write_frame(sys.stdout.buffer, latest_progress)

        pulse = threading.Thread(target=heartbeat, daemon=True)
        pulse.start()
        try:
            try:
                from fingering.model import generate, load_models
            except (ImportError, OSError) as error:
                raise RuntimeError(
                    "Missing or unavailable local inference dependency. This package requires "
                    "an existing Microsoft Visual C++ v14 x64 runtime on Windows "
                    "(MSVCP140, VCRUNTIME140 and VCRUNTIME140_1). Check that prerequisite "
                    "and the project-local Python/Torch setup; normal library and playback remain usable."
                ) from error

            emit({"version": 1, "jobId": request.get("jobId"), "type": "progress",
                  "stage": "loading", "completed": 0, "total": 1})
            models = load_models(args.runtime_dir.resolve())
            emit({"version": 1, "jobId": request.get("jobId"), "type": "progress",
                  "stage": "inference", "completed": 0, "total": len(request.get("notes", []))})
            result = generate(request, lambda stage, done, total: emit({
                "version": 1, "jobId": request.get("jobId"), "type": "progress",
                "stage": stage, "completed": done, "total": total,
            }), models=models)
            if time.monotonic() - started > 15 * 60:
                raise TimeoutError("ArGNN job exceeded 15-minute time limit")
            emit(result)
        finally:
            stopping.set()
            pulse.join(timeout=1)
        return 0
    except Exception as error:
        details = str(error)[:1024]
        text = traceback.format_exc(limit=3).encode("utf-8", "replace")[-2048:].decode("utf-8", "replace")
        sys.stderr.write(text + "\n")
        code = "setup_missing" if "missing" in details.lower() else (
            "checkpoint_incompatible" if "checkpoint" in details.lower() else "inference_failed"
        )
        emit({"version": 1, "jobId": request.get("jobId") if isinstance(request, dict) else None,
              "type": "error", "code": code, "message": details or "ArGNN inference failed"})
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
