#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""UE5.8 MCP 命令行助手(纯标准库)。mcp_http.py 的增强版。

子命令:
  tools                       列出顶层工具名 + 输入 schema
  toolsets                    列出工具集名(计数)
  schema <toolset>            describe_toolset
  call <toolset> <tool> '{json}'   等价于 call_tool
  call <toolset> <tool> @f.json
  py '<python code>'          走顶层 execute_python_code(VibeUE)
  py @script.py               多行代码走文件
  raw <method> [json|@file]   裸 jsonrpc 调用

所有输出写 stdout(强制 utf-8),重定向到文件不会丢字符。
"""
import io
import json
import os
import re
import sys
import uuid
import urllib.request
import urllib.error

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")

URL = "http://127.0.0.1:8000/mcp"
PROTOCOL = "2025-06-18"
SESSION = None
LIMIT = int(os.environ.get("UE_OUT_LIMIT", "12000"))


def _post(payload):
    global SESSION
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(URL, data=data, method="POST")
    req.add_header("Content-Type", "application/json")
    req.add_header("Accept", "application/json, text/event-stream")
    if SESSION:
        req.add_header("Mcp-Session-Id", SESSION)
    try:
        with urllib.request.urlopen(req, timeout=300) as resp:
            body = resp.read().decode("utf-8", "replace")
            sid = resp.headers.get("Mcp-Session-Id")
            ctype = resp.headers.get("Content-Type", "")
            if sid:
                SESSION = sid
            return body, ctype
    except urllib.error.HTTPError as e:
        return e.read().decode("utf-8", "replace"), e.headers.get("Content-Type", "")


def _msgs(body, ctype):
    out = []
    if "text/event-stream" in ctype:
        for line in body.splitlines():
            if line.startswith("data:"):
                raw = line[5:].strip()
                if raw:
                    try:
                        out.append(json.loads(raw))
                    except Exception:
                        pass
        return out
    s = body.strip()
    if not s:
        return out
    try:
        out.append(json.loads(s))
        return out
    except Exception:
        pass
    dec = json.JSONDecoder()
    i = 0
    while i < len(s):
        while i < len(s) and s[i] in " \r\n\t":
            i += 1
        if i >= len(s):
            break
        try:
            obj, j = dec.raw_decode(s, i)
            out.append(obj)
            i = j
        except Exception:
            break
    return out


def request(method, params=None, notify=False):
    p = {"jsonrpc": "2.0", "method": method}
    if params is not None:
        p["params"] = params
    if not notify:
        p["id"] = str(uuid.uuid4())
    body, ctype = _post(p)
    for m in _msgs(body, ctype):
        if "result" in m or "error" in m:
            return m
    return {"error": {"code": -1, "message": "no jsonrpc message", "raw": body[:2000]}}


def init():
    r = request("initialize", {"protocolVersion": PROTOCOL, "capabilities": {},
                               "clientInfo": {"name": "ue", "version": "1.0"}})
    if "error" in r:
        print("握手失败:", r["error"])
        sys.exit(1)
    request("notifications/initialized", None, notify=True)


def unwrap(r):
    """MCP tools/call 的返回:content[0].text,尽量解析成 json。"""
    if "error" in r:
        return {"__error__": r["error"]}
    res = r.get("result", {})
    if isinstance(res, dict) and "content" in res:
        texts = [c.get("text", "") for c in res["content"] if c.get("type") == "text"]
        joined = "\n".join(texts)
        try:
            return json.loads(joined)
        except Exception:
            return {"__text__": joined}
    return res


def show(obj):
    s = json.dumps(obj, ensure_ascii=False, indent=2)
    print(s[:LIMIT])
    if len(s) > LIMIT:
        print("...[截断 %d 字符, 设 UE_OUT_LIMIT 调整]" % (len(s) - LIMIT))


def arg_of(s):
    if s is None:
        return None
    if s.startswith("@"):
        with open(s[1:], "r", encoding="utf-8") as f:
            return json.load(f)
    return json.loads(s)


def main():
    init()
    cmd = sys.argv[1] if len(sys.argv) > 1 else "tools"

    if cmd == "tools":
        r = request("tools/list")
        tl = r.get("result", {}).get("tools", [])
        print("[tools] %d 个" % len(tl))
        for t in tl:
            print("  - %s" % t.get("name"))
            sc = t.get("inputSchema", {})
            props = sc.get("properties", {})
            if props:
                print("      params: %s  required=%s" % (
                    ", ".join(props.keys()), sc.get("required", [])))
        return

    if cmd == "toolsets":
        r = unwrap(request("tools/call", {"name": "list_toolsets", "arguments": {}}))
        txt = r.get("__text__", "")
        # 注意:描述正文里也有以 "- " 开头的行(如 "- **Plugin**"),必须严格过滤
        pat = re.compile(r"^- ([A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)+):")
        names = []
        for ln in txt.splitlines():
            m = pat.match(ln)
            if m:
                names.append(m.group(1))
        print("[toolsets] %d 个" % len(names))
        for n in names:
            print("  " + n)
        return

    if cmd == "schema":
        ts = sys.argv[2]
        show(unwrap(request("tools/call", {
            "name": "describe_toolset", "arguments": {"toolset_name": ts}})))
        return

    if cmd == "describe":
        # 打印某个工具集里单个工具的参数 schema
        ts, tool = sys.argv[2], sys.argv[3]
        r = unwrap(request("tools/call", {
            "name": "describe_toolset", "arguments": {"toolset_name": ts}}))
        tools = r.get("tools") or []
        for t in tools:
            nm = t.get("name", "")
            if nm == tool or nm.split(".")[-1] == tool:
                print("TOOL:", nm)
                print("DESC:", t.get("description", "")[:2000])
                sc = t.get("inputSchema", {})
                props = sc.get("properties", {})
                print("REQUIRED:", sc.get("required", []))
                for k, v in props.items():
                    print("  - %s: %s %s" % (k, v.get("type", "?"), v.get("description", "")[:200]))
                return
        print("未找到工具:", tool)
        print("可用:", ", ".join(t.get("name", "").split(".")[-1] for t in tools))
        return

    if cmd == "call":
        ts, tool = sys.argv[2], sys.argv[3]
        args = arg_of(sys.argv[4] if len(sys.argv) > 4 else None) or {}
        show(unwrap(request("tools/call", {
            "name": "call_tool",
            "arguments": {"toolset_name": ts, "tool_name": tool, "arguments": args}})))
        return

    if cmd == "py":
        code = sys.argv[2]
        if code.startswith("@"):
            with open(code[1:], "r", encoding="utf-8") as f:
                code = f.read()
        show(unwrap(request("tools/call", {
            "name": "execute_python_code", "arguments": {"code": code}})))
        return

    if cmd == "raw":
        method = sys.argv[2]
        show(request(method, arg_of(sys.argv[3] if len(sys.argv) > 3 else None)))
        return

    print(__doc__)


if __name__ == "__main__":
    main()
