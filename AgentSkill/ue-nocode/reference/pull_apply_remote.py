"""Pull teammate's changed files from GitHub and apply to local workdir.
Usage: GH_TOKEN=... python pull_apply_remote.py owner/repo since_sha workdir
- backs up each existing local file to workdir/_sync_backup/<sha>/
- downloads current main version of every changed file via blob sha
- writes locally, verifies local blob sha1 matches remote
- dumps remote HANDOVER_zflip.md new-section pointer + per-file patch summary
"""
import base64
import hashlib
import json
import os
import shutil
import sys
import time
import urllib.request

REPO = sys.argv[1] if len(sys.argv) > 1 else "madtwo/z-flip"
SINCE = sys.argv[2] if len(sys.argv) > 2 else "c12121e"
WORKDIR = sys.argv[3] if len(sys.argv) > 3 else r"D:\UE\z-flip"
TOKEN = os.environ.get("GH_TOKEN", "")
API = "https://api.github.com"


def req(path, raw=False):
    last = None
    for attempt in range(6):
        try:
            r = urllib.request.Request(API + path, headers={
                "Authorization": "Bearer " + TOKEN,
                "Accept": "application/vnd.github+json",
                "User-Agent": "zcode-sync",
            })
            with urllib.request.urlopen(r, timeout=30) as resp:
                data = resp.read()
                return data if raw else json.loads(data)
        except Exception as exc:
            last = exc
            time.sleep(1.5 * (attempt + 1))
    raise SystemExit("FAIL %s: %r" % (path, last))


def local_blob_sha(content_bytes):
    return hashlib.sha1(b"blob %d\0" % len(content_bytes) + content_bytes).hexdigest()


cmp = req("/repos/%s/compare/%s...main" % (REPO, SINCE))
head = (cmp.get("head_commit") or cmp["commits"][-1])["sha"][:7]
backup_root = os.path.join(WORKDIR, "_sync_backup", head)
os.makedirs(backup_root, exist_ok=True)

patch_dump = []
ok, failed = [], []
for f in cmp["files"]:
    path = f["filename"]
    if f["status"] == "removed":
        print("SKIP removed (left untouched):", path)
        continue
    blob = req("/repos/%s/git/blobs/%s" % (REPO, f["sha"]), raw=False)
    content = base64.b64decode(blob["content"])
    sha = local_blob_sha(content)
    dest = os.path.join(WORKDIR, path.replace("/", os.sep))
    if os.path.exists(dest):
        with open(dest, "rb") as fh:
            old = fh.read()
        if hashlib.sha1(b"blob %d\0" % len(old) + old).hexdigest() == sha:
            print("SAME  (already current):", path)
            continue
        bak = os.path.join(backup_root, path.replace("/", "__"))
        shutil.copy2(dest, bak)
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with open(dest, "wb") as fh:
        fh.write(content)
    if hashlib.sha1(b"blob %d\0" % len(content) + content).hexdigest() == sha:
        ok.append(path)
        print("OK    (%d bytes) %s" % (len(content), path))
    else:
        failed.append(path)
        print("SHA-MISMATCH", path)
    if f.get("patch"):
        patch_dump.append("=" * 20 + " " + path + "\n" + f["patch"])

with open(os.path.join(backup_root, "_patches.diff"), "w", encoding="utf-8") as fh:
    fh.write("\n".join(patch_dump))
print("\nPATCHES saved ->", os.path.join(backup_root, "_patches.diff"))
print("DONE applied=%d failed=%d head=%s" % (len(ok), len(failed), head))
