"""跑一个本地 .py 文件到编辑器(组播通道)。用法: python run_file.py <脚本路径> [超时秒]"""
import sys, ue_pyexec

path = sys.argv[1]
timeout = int(sys.argv[2]) if len(sys.argv) > 2 else 90
code = open(path, encoding='utf-8').read()
r, err = ue_pyexec.run(code, 16901, timeout)
if err:
    print('ERROR:', err)
    sys.exit(2)
print('success:', r.get('success'))
for o in r.get('output', []):
    print(o.get('output', '').rstrip())
if r.get('result'):
    print('result:', r.get('result'))
