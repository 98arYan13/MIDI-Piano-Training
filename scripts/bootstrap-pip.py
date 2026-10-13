"""Bootstrap pinned pip into the embedded runtime from its verified wheel."""

import pathlib
import stat
import sys
import zipfile

wheel, target = map(pathlib.Path, sys.argv[1:3])
version = sys.argv[3]
target.mkdir(parents=True, exist_ok=True)
allowed = {"pip", f"pip-{version}.dist-info"}
with zipfile.ZipFile(wheel) as archive:
    names = archive.namelist()
    roots = {name.split("/", 1)[0] for name in names if "/" in name}
    if roots != allowed:
        raise SystemExit("unexpected paths in pinned pip wheel")
    for info in archive.infolist():
        parts = pathlib.PurePosixPath(info.filename).parts
        if any(part in ("", ".", "..") for part in parts) or parts[0] not in allowed:
            raise SystemExit("unsafe path in pinned pip wheel")
        if stat.S_ISLNK(info.external_attr >> 16):
            raise SystemExit("symbolic link in pinned pip wheel")
        destination = target.joinpath(*parts)
        if info.is_dir():
            destination.mkdir(parents=True, exist_ok=True)
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(archive.read(info))
