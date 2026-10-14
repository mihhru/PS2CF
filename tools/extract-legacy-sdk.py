"""Extract only /usr/local/ps2dev from the pinned upstream OCI layers."""
from pathlib import Path
import hashlib
import json
import os
import sys
import tarfile

image, dest = map(Path, sys.argv[1:])
dest = dest.resolve()
dest.mkdir(parents=True, exist_ok=True)
manifest = json.loads((image / 'manifest.json').read_text())
links = []
prefix = 'usr/local/ps2dev/'
for layer in manifest['layers']:
    digest = layer['digest'].split(':')[1]
    blob = image / digest
    assert hashlib.sha256(blob.read_bytes()).hexdigest() == digest
    with tarfile.open(blob) as archive:
        for member in archive:
            name = member.name.lstrip('./')
            if not name.startswith(prefix):
                continue
            relative = name[len(prefix):]
            if not relative:
                continue
            assert (dest / relative).resolve().is_relative_to(dest)
            member.name = relative
            if member.islnk():
                target = member.linkname.lstrip('./').removeprefix(prefix)
                assert (dest / target).resolve().is_relative_to(dest)
                links.append((relative, target))
                continue
            if member.issym() and member.linkname.startswith('/' + prefix):
                member.linkname = os.path.relpath(dest / member.linkname[len(prefix) + 1:], (dest / relative).parent)
            archive.extract(member, dest, filter='data')
for name, target in links:
    p = dest / name
    if not p.exists():
        os.link(dest / target, p)
print('Extracted legacy SDK:', dest)
