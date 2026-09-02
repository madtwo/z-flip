#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""UE5.8 原生 MCP 的 HTTP(streamable) 极简客户端 —— 纯标准库。

用法:
  python mcp_http.py tools/list
  python mcp_http.py tools/call '{"name":"list_toolsets","arguments":{}}'
  python mcp_http.py tools/call @args.json        # 参数从文件读(避免 shell 转义地狱)

协议要点(踩坑记录):
  * 头必须带 Accept: application/json, text/event-stream
  * 响应头 Mcp-Session-Id 必须逐请求回传
  * 响应体可能是 SSE(event: message / data: {json})
  * UE 侧只暴露 3 个元工具: list_toolsets / describe_toolset / call_tool
    (+VibeUE 完全加载后的 execute_python_code 等顶层工具)
"""
import json
import sys
import urllib.request
import urllib.error
import uuid

URL = "http://127.0.0.1:8000/mcp"
PROTOCOL = "2025-06-18"
SESSION = None


def _post(payload, session_id=None, expect_response=True):
    global SESSION
    data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(URL, data=data, method="POST")
    req.add_header("Content-Type", "application/json")
    req.add_header("Accept", "application/json, text/event-stream")
    if session_id:
        req.add_header("Mcp-Session-Id", session_id)
    try:
        with urllib.request.urlopen(req, timeout=120) as resp:
            body = resp.read().decode("utf-8", "replace")
            sid = resp.headers.get("Mcp-Session-Id")
            ctype = resp.headers.get("Content-Type", "")
            if sid:
                SESSION = sid
            return body, ctype, sid
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", "replace")
        return body, e.headers.get("Content-Type", ""), e.headers.get("Mcp-Session-Id")


def _extract(body, ctype):
    """从 JSON 或 SSE 响应体里抽出所有 jsonrpc 消息。"""
    msgs = []
    if "text/event-stream" in ctype:
        for line in body.splitlines():
            if line.startswith("data:"):
                raw = line[5:].strip()
                if raw:
                    try:
                        msgs.append(json.loads(raw))
                    except Exception:
                        pass
    else:
        s = body.strip()
        if not s:
            return msgs
        try:
            msgs.append(json.loads(s))
        except Exception:
            # 有些实现把多条 JSON 顺序输出
            dec = json.JSONDecoder()
            i = 0
            while i < len(s):
                while i < len(s) and s[i] in " \r\n\t":
                    i += 1
                if i >= len(s):
                    break
                try:
                    obj, j = dec.raw_decode(s, i)
                    msgs.append(obj)
                    i = j
                except Exception:
                    break
    return msgs


def request(method, params=None, notify=False):
    payload = {"jsonrpc": "2.0", "method": method}
    if params is not None:
        payload["params"] = params
    if not notify:
        payload["id"] = str(uuid.uuid4())
    body, ctype, sid = _post(payload, SESSION)
    msgs = _extract(body, ctype)
    if notify:
        return None
    for m in msgs:
        if "result" in m or "error" in m:
            return m
    return {"raw": body, "messages": msgs}


def handshake():
    r = request("initialize", {
        "protocolVersion": PROTOCOL,
        "capabilities": {},
        "clientInfo": {"name": "mcp_http_probe", "version": "1.0"},
    })
    ok = r and "result" in r
    print("[initialize] server=%s session=%s" % (
        (r or {}).get("result", {}).get("serverInfo") if ok else r, SESSION))
    if not ok:
        return False
    request("notifications/initialized", None, notify=True)
    return True


def main():
    if not handshake():
        print("握手失败")
        sys.exit(1)

    if len(sys.argv) < 2:
        print("用法: mcp_http.py <method> [json|@file]")
        return
    method = sys.argv[1]
    params = None
    if len(sys.argv) > 2:
        arg = sys.argv[2]
        if arg.startswith("@"):
            with open(arg[1:], "r", encoding="utf-8") as f:
                params = json.load(f)
        else:
            params = json.loads(arg)
    r = request(method, params)
    out = r.get("result") if r and "result" in r else r
    if isinstance(out, dict) and "tools" in out:
        tools = out["tools"]
        print("[tools] %d 个:" % len(tools))
        for t in tools:
            print("  -", t.get("name"))
        return
    print(json.dumps(out, ensure_ascii=False, indent=2)[:8000])


if __name__ == "__main__":
    main()
