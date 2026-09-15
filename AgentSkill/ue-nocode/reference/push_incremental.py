# 增量推送(2026-09-15 新增):GitHub 的 POST /git/trees 对"整棵树"会超时(422
# "request timed out ... consider building the tree incrementally, or building the commits
# you need in a local clone")。本脚本按官方建议用 base_tree 只提交**变更条目**:
#   GET /git/trees/<head>?recursive=1 拿远端 path->sha
#   POST /git/trees {base_tree: <远端根树>, tree: [仅本地 sha 不同的条目]}
#   POST /git/commits(parents=[head]) + PATCH /git/refs/heads/main
# 语义与整树重建一致(base_tree 未提及的远端条目保留;本地已跟踪清单之外的远端文件不会被删),
# 推完务必用 compare/<head>...main 核对 removed=0。
# 用法: GH_TOKEN=... python push_incremental.py owner/repo workdir
import base64, datetime, hashlib, http.client, json, os, subprocess, sys, time

REPO = os.environ.get("GS_REPO") or (sys.argv[1] if len(sys.argv) > 1 else "madtwo/z-flip")
WORKDIR = os.environ.get("GS_WORKDIR") or (sys.argv[2] if len(sys.argv) > 2 else r"D:\UE\z-flip")
TOKEN = os.environ.get("GH_TOKEN", "")


def log(m): print(m, flush=True)


def connect(): return http.client.HTTPSConnection("api.github.com", timeout=60)


def api(conn, method, path, payload=None, retries=6):
    body = json.dumps(payload).encode() if payload is not None else None
    headers = {"Authorization": "token " + TOKEN,
               "Accept": "application/vnd.github+json", "User-Agent": "rest-push",
               "Content-Type": "application/json"}
    last = None
    for i in range(retries):
        try:
            conn.request(method, path, body=body, headers=headers)
            r = conn.getresponse(); data = r.read()
            if r.status in (200, 201): return json.loads(data) if data else {}
            if r.status == 404: return {"__notfound": True}
            if r.status in (409, 422):
                raise RuntimeError("%d %s" % (r.status, data[:400].decode(errors="replace")))
            if r.status in (401, 403):
                raise RuntimeError("%d %s (令牌问题,勿重试)" % (r.status, data[:300].decode(errors="replace")))
            last = RuntimeError("HTTP %d %s" % (r.status, data[:200].decode(errors="replace")))
        except (http.client.HTTPException, OSError) as e:
            last = e
            try: conn.close()
            except Exception: pass
            conn = connect()
        time.sleep(min(2 * (i + 1), 10))
    raise RuntimeError("API failed: %s %s (%s)" % (method, path, last))


def person(line):
    lt, gt = line.index("<"), line.index(">")
    name = line[:lt].split(" ", 1)[1].strip()
    mail = line[lt + 1:gt]
    ts, tz = line[gt + 1:].split()
    dt = datetime.datetime.fromtimestamp(int(ts), datetime.timezone(datetime.timedelta(
        hours=int(tz[:3]), minutes=int(tz[0] + tz[3:5]))))
    return {"name": name, "email": mail, "date": dt.isoformat()}


def blob_sha(data): return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


out = subprocess.run(["git", "-c", "core.quotepath=off", "ls-files", "-z"],
                     cwd=WORKDIR, capture_output=True, check=True).stdout
files = [f.decode("utf-8") for f in out.split(b"\0") if f]
log("local tracked files: %d" % len(files))

# 默认不删远端"本地磁盘上还存在"的文件(那多半是想留着的);GS_ALLOW_DELETE=1 才删
# —— 用于清理"本地已取消跟踪、但仍留在磁盘上"的误提交文件(2026-09-15 用它清 5 个调试产物)。
ALLOW_DELETE = os.environ.get("GS_ALLOW_DELETE") == "1"

conn = connect()
head = api(conn, "GET", "/repos/%s/git/refs/heads/main" % REPO)["object"]["sha"]
log("remote head: %s" % head[:7])
rtree = api(conn, "GET", "/repos/%s/git/trees/%s?recursive=1" % (REPO, head))
remote = {e["path"]: e["sha"] for e in rtree["tree"] if e.get("type") == "blob"}
log("remote blobs: %d%s" % (len(remote), " (truncated!)" if rtree.get("truncated") else ""))

changed, uploaded = [], 0
for p in files:
    with open(os.path.join(WORKDIR, p.replace("/", os.sep)), "rb") as fh:
        data = fh.read()
    sha = blob_sha(data)
    if remote.get(p) == sha:
        continue
    r = api(conn, "GET", "/repos/%s/git/blobs/%s" % (REPO, sha))
    if r.get("__notfound"):
        api(conn, "POST", "/repos/%s/git/blobs" % REPO,
            {"content": base64.b64encode(data).decode(), "encoding": "base64"})
        uploaded += 1
    changed.append({"path": p, "mode": "100644", "type": "blob", "sha": sha})
log("changed entries: %d (newly uploaded blobs: %d)" % (len(changed), uploaded))
for e in changed[:40]:
    log("  M %s" % e["path"])
if len(changed) > 40:
    log("  ... (+%d)" % (len(changed) - 40))

# 远端有、本地已跟踪清单却没有的 blob → 用 sha=null 删除(与"整树重建"语义一致:
# 本地 git ls-files 即权威清单)。删前先确认这些文件确实不在本地工作区。
locset = set(files)
for p, sha in remote.items():
    if p not in locset:
        if (not ALLOW_DELETE) and os.path.exists(os.path.join(WORKDIR, p.replace("/", os.sep))):
            log("  !! 远端独有但本地磁盘存在,跳过删除(要删设 GS_ALLOW_DELETE=1): %s" % p); continue
        changed.append({"path": p, "mode": "100644", "type": "blob", "sha": None})
        log("  D %s" % p)

if not changed:
    log("nothing to push"); raise SystemExit(0)

tree = api(conn, "POST", "/repos/%s/git/trees" % REPO,
           {"base_tree": rtree["sha"], "tree": changed})
raw = subprocess.run(["git", "cat-file", "commit", "HEAD"], cwd=WORKDIR,
                     capture_output=True, check=True).stdout
hdr, _, msg = raw.partition(b"\n\n")
lines = hdr.decode("utf-8").splitlines()
commit = api(conn, "POST", "/repos/%s/git/commits" % REPO,
             {"message": msg.decode("utf-8"), "tree": tree["sha"], "parents": [head],
              "author": person(next(l for l in lines if l.startswith("author "))),
              "committer": person(next(l for l in lines if l.startswith("committer ")))})
log("commit: %s" % commit["sha"][:7])
ref = api(conn, "PATCH", "/repos/%s/git/refs/heads/main" % REPO, {"sha": commit["sha"]})
log("main -> %s" % ref["object"]["sha"][:7])
log("完成。核对: compare/%s...main 的 removed 应为 0" % head[:7])
