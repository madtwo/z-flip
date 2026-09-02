# GitHub REST 直传(代理没开、github.com:443 被封时的 git push 替代)
# 用法: GH_TOKEN=$(powershell -NoProfile -File read_gh_cred.ps1 | tail -1) python push_via_api.py [owner/repo] [工作目录]
# 2026-09-02 在 madtwo/MyProject2(291 文件/130MB)验证通过。
# 令牌读法见 read_gh_cred.ps1(Windows 凭证管理器 CredRead P/Invoke)。
import base64, datetime, hashlib, http.client, json, os, subprocess, sys, time

# 不传参用默认值;也可用环境变量 GS_REPO / GS_WORKDIR
REPO = os.environ.get("GS_REPO") or (sys.argv[1] if len(sys.argv) > 1 else "madtwo/MyProject2")
WORKDIR = os.environ.get("GS_WORKDIR") or (sys.argv[2] if len(sys.argv) > 2 else r"D:\UE\MyProject2")

def log(m): print(m, flush=True)

def connect(): return http.client.HTTPSConnection("api.github.com", timeout=60)

def api(conn, method, path, payload=None, retries=6):
    body = json.dumps(payload).encode() if payload is not None else None
    headers = {"Authorization": "token " + os.environ["GH_TOKEN"],
               "Accept": "application/vnd.github+json", "User-Agent": "rest-push",
               "Content-Type": "application/json"}
    last = None
    for i in range(retries):
        try:
            conn.request(method, path, body=body, headers=headers)
            r = conn.getresponse(); data = r.read()
            if r.status in (200, 201): return json.loads(data) if data else {}
            if r.status == 404: return {"__notfound": True}
            if r.status in (409, 422): return {"__conflict": True, "body": data[:300].decode(errors="replace")}
            if r.status in (401, 403): raise RuntimeError(f"{r.status} {data[:300].decode(errors='replace')} (令牌问题,勿重试)")
            last = RuntimeError(f"HTTP {r.status}")
        except (http.client.HTTPException, OSError) as e:
            last = e
            try: conn.close()
            except Exception: pass
            conn = connect()
        time.sleep(min(2 * (i + 1), 10))
    raise RuntimeError(f"API failed: {method} {path} ({last})")

def person(line):
    # "author Name <mail> 1756785523 +0800" -> GitHub API 要的 {name,email,date}
    lt, gt = line.index("<"), line.index(">")
    ts, tz = line[gt+1:].split()          # tz 形如 +0800,必须按 HHMM 解析
    sign = -1 if tz[0] == "-" else 1
    off = datetime.timezone(sign * datetime.timedelta(hours=int(tz[1:3]), minutes=int(tz[3:5])))
    # date 必须 ISO 8601 带冒号偏移(+08:00);裸 +0800 会被 422 拒
    return {"name": line[line.index(" ")+1:lt].strip(), "email": line[lt+1:gt],
            "date": datetime.datetime.fromtimestamp(int(ts), off).isoformat()}

def main():
    os.chdir(WORKDIR)
    conn = connect()
    files = [f for f in subprocess.run(["git", "-c", "core.quotepath=off", "ls-files", "-z"],
            capture_output=True, check=True).stdout.decode("utf-8").split("\0") if f]
    log(f"files: {len(files)}")

    # 空仓库引导:git-data 的 tree/refs API 对完全空仓库一律 409(GET ref 也是 409 不是 404!)
    ref0 = api(conn, "GET", f"/repos/{REPO}/git/refs/heads/main")
    if ref0.get("__notfound") or ref0.get("__conflict"):
        with open("README.md", "rb") as fh: rb = fh.read()
        base = api(conn, "PUT", f"/repos/{REPO}/contents/README.md",
                   {"message": "bootstrap", "content": base64.b64encode(rb).decode()})["commit"]["sha"]
        log(f"bootstrap commit: {base}")
    else:
        base = ref0["object"]["sha"]; log(f"head: {base}")

    entries = []
    for i, path in enumerate(files, 1):
        with open(path, "rb") as fh: data = fh.read()
        bsha = hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()
        r = api(conn, "GET", f"/repos/{REPO}/git/blobs/{bsha}")     # 预检:已传过的跳过
        if r.get("__notfound") or r.get("__conflict"):
            r = api(conn, "POST", f"/repos/{REPO}/git/blobs",
                    {"content": base64.b64encode(data).decode(), "encoding": "base64"})
            if r.get("__conflict"): r = {"sha": bsha}
        entries.append({"path": path, "mode": "100644", "type": "blob", "sha": r["sha"]})
        if i % 25 == 0 or i == len(files): log(f"blobs {i}/{len(files)}")

    tree = api(conn, "POST", f"/repos/{REPO}/git/trees", {"tree": entries})
    raw = subprocess.run(["git", "cat-file", "commit", "HEAD"], capture_output=True, check=True).stdout
    head, _, msg = raw.partition(b"\n\n"); lines = head.decode("utf-8").splitlines()
    commit = api(conn, "POST", f"/repos/{REPO}/git/commits",
                 {"message": msg.decode("utf-8"), "tree": tree["sha"], "parents": [base],
                  "author": person(next(l for l in lines if l.startswith("author "))),
                  "committer": person(next(l for l in lines if l.startswith("committer ")))})
    log(f"commit: {commit['sha']}")
    ref = api(conn, "PATCH", f"/repos/{REPO}/git/refs/heads/main", {"sha": commit["sha"]})
    log(f"main -> {ref['object']['sha']}")
    log("完成。历史对齐(开代理后一次): git fetch origin && git reset --hard origin/main")

if __name__ == "__main__":
    main()
