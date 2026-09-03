"""List recent commits + changed files on remote repo (for pulling teammate changes).
Usage: GH_TOKEN=... python pull_check_remote.py owner/repo [since_sha]
"""
import base64
import json
import os
import sys
import time
import urllib.request

REPO = sys.argv[1] if len(sys.argv) > 1 else "madtwo/z-flip"
SINCE = sys.argv[2] if len(sys.argv) > 2 else "c12121e"
TOKEN = os.environ.get("GH_TOKEN", "")

API = "https://api.github.com"


def req(path, raw=False):
    url = API + path
    last = None
    for attempt in range(5):
        try:
            r = urllib.request.Request(url, headers={
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


commits = req("/repos/%s/commits?per_page=15" % REPO)
print("=== COMMITS (newest first) ===")
for c in commits:
    print(c["sha"][:7], c["commit"]["author"]["date"], "-",
          c["commit"]["message"].splitlines()[0][:80])

if SINCE:
    try:
        cmp = req("/repos/%s/compare/%s...main" % (REPO, SINCE))
        print("\n=== FILES CHANGED %s...main (%d commits, %d files) ==="
              % (SINCE[:7], cmp["total_commits"], len(cmp["files"])))
        for f in cmp["files"]:
            print("%-8s +%-5d -%-5d %s" % (f["status"], f["additions"], f["deletions"], f["filename"]))
    except SystemExit as exc:
        print("\nCOMPARE FAILED:", exc)
