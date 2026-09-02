"""UE5 Python 远程执行客户端(纯标准库)。
协议: 组播 239.0.0.1:6766 发 open_connection -> 编辑器反向连到本机 TCP -> 发 command -> 收 command_result。
用法: python ue_pyexec.py "import unreal; print('hi')" [--port 16901]
"""
import socket, json, sys, argparse, threading, time

MCAST_GRP = '239.0.0.1'
MCAST_PORT = 6766

def msg(t, data=None, dest=''):
    m = {"version": 1, "magic": "ue_py", "type": t, "source": f"zcode-{int(time.time()*1000)%10000000}"}
    if dest: m["dest"] = dest
    if data is not None: m["data"] = data
    return json.dumps(m, separators=(',', ':')).encode()

def run(code, cmd_port, timeout=60):
    out = {'done': False}
    if '\n' in code or len(code) > 300:
        path = rf'C:\Users\20625\AppData\Local\Temp\ue_pyexec_{int(time.time()*1000)%100000000}.py'
        open(path, 'w', encoding='utf-8').write(code)
        payload = {'command': path, 'unattended': True, 'exec_mode': 'ExecuteFile'}
    else:
        payload = {'command': code, 'unattended': True, 'exec_mode': 'ExecuteStatement'}
    server = socket.socket()
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(('0.0.0.0', cmd_port))
    server.listen(1)
    server.settimeout(timeout)
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 1)
    udp.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
    udp.sendto(msg('open_connection', {'command_ip': '127.0.0.1', 'command_port': cmd_port}), ('239.0.0.1', MCAST_PORT))
    try:
        conn, addr = server.accept()
    except socket.timeout:
        server.close(); return None, '编辑器未连接(远程执行可能未启用)'
    conn.settimeout(timeout)
    conn.sendall(msg('command', payload))
    buf = b''
    result = None
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            d = conn.recv(65536)
        except socket.timeout:
            break
        if not d: break
        buf += d
        try:
            obj = json.loads(buf.decode('utf-8', errors='replace'))
        except Exception:
            continue
        if obj.get('type') == 'command_result':
            result = obj.get('data', {}); break
    conn.close(); server.close()
    return result, None

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('code')
    ap.add_argument('--port', type=int, default=16901)
    ap.add_argument('--timeout', type=int, default=60)
    a = ap.parse_args()
    r, err = run(a.code, a.port, a.timeout)
    if err:
        print('ERROR:', err); sys.exit(2)
    print('success:', r.get('success'))
    for o in r.get('output', []):
        print(f"[{o.get('type')}] {o.get('output','').rstrip()}")
    if r.get('result'):
        print('result:', r.get('result'))
