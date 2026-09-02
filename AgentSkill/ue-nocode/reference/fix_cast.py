import json,urllib.request,time
URL='http://127.0.0.1:8000/mcp'
def post(payload,sid=None):
    req=urllib.request.Request(URL,data=json.dumps(payload).encode(),
        headers={'Content-Type':'application/json','Accept':'application/json, text/event-stream',
                 **({'Mcp-Session-Id':sid} if sid else {})})
    r=urllib.request.urlopen(req,timeout=120)
    return r.read().decode('utf-8',errors='replace'),r.headers.get('Mcp-Session-Id')
class UE:
    def __init__(self):
        b,sid=post({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"py","version":"1"}}})
        post({"jsonrpc":"2.0","method":"notifications/initialized"},sid)
        self.sid=sid; self.i=5300
    def call(self,toolset,tool,args):
        self.i+=1
        b,_=post({"jsonrpc":"2.0","id":self.i,"method":"tools/call","params":{"name":"call_tool",
            "arguments":{"toolset_name":toolset,"tool_name":tool,"arguments":args}}},self.sid)
        d=json.loads(b)
        raw=d['result']['content'][0]['text']
        try: return json.loads(raw)
        except Exception: return raw
ue=UE()
BT="editor_toolset.toolsets.blueprint.BlueprintTools"
AT="editor_toolset.toolsets.asset.AssetTools"
G="/Game/ThirdPerson/Blueprints/BP_Coin.BP_Coin:EventGraph"
BP="/Game/ThirdPerson/Blueprints/BP_Coin.BP_Coin"
pos={'N1':(-1900,-500),'N2':(-2350,-700),'N3':(-2700,-750),'N4':(-2700,-500),'N5':(-2700,-250),'N6':(-1550,-100),'N7':(-1250,-100),'N8':(-950,-100),'N9':(-650,-100),'N10':(-350,-100),'NR':(-700,-600),'T1':(900,-400),'T2':(1150,-400),'T3':(1450,-450),'T4':(1800,-350),'T5':(2100,-350),'T6':(2400,-350),'T7':(2400,-800),'T8':(2700,-800),'T9':(3000,-750),'T10':(3300,-700),'T11':(3600,-300),'T12':(3950,-250)}
key_by_pos={(v[0],v[1]):k for k,v in pos.items()}
E1=G+'.K2Node_CustomEvent_2'; E2=G+'.K2Node_CustomEvent_3'
nodes={'E1':E1,'E2':E2}
cast=None
refs=[x['refPath'] for x in ue.call(BT,"find_nodes",{"graph":{"refPath":G},"title":""}).get('returnValue',[]) if isinstance(x,dict)]
pinmap={}
for r in refs:
    info=ue.call(BT,"get_node_infos",{"nodes":[{"refPath":r}]})
    e=(info.get('returnValue') or [{}])[0]
    p=e.get('position') or {}
    k=key_by_pos.get((p.get('x'),p.get('y')))
    if k: nodes[k]=r
    tid=str(e.get('type_id'))
    if 'CastToStaticMeshActor' in tid: cast=r
    m={}
    for q in e.get('output_pins') or []:
        pid=q.get('pin_id') or {}; m[('EGPD_Output',pid.get('name'))]=pid
    for q in e.get('input_pins') or []:
        pid=q.get('pin_id') or {}; m[('EGPD_Input',pid.get('name'))]=pid
    pinmap[r]=(m,e)
missing=[k for k in pos if k not in nodes]
print('认领:',len([k for k in nodes if k not in('E1','E2')]),'缺失:',missing or '无','Cast:',(cast or '不存在').split('.')[-1])
if missing: raise SystemExit('节点不全')
PIN={
 'E1':{'then':('EGPD_Output',1)},'E2':{'then':('EGPD_Output',1)},
 'N1':{'execute':('EGPD_Input',0),'Class':('EGPD_Input',1),'SpawnTransform':('EGPD_Input',2),'CollisionHandlingOverride':('EGPD_Input',3),'then':('EGPD_Output',0),'ReturnValue':('EGPD_Output',1)},
 'N2':{'Location':('EGPD_Input',0),'Rotation':('EGPD_Input',1),'Scale':('EGPD_Input',2),'ReturnValue':('EGPD_Output',0)},
 'N3':{'self':('EGPD_Input',0),'ReturnValue':('EGPD_Output',0)},
 'N4':{'Roll':('EGPD_Input',0),'Pitch':('EGPD_Input',1),'Yaw':('EGPD_Input',2),'ReturnValue':('EGPD_Output',0)},
 'N5':{'X':('EGPD_Input',0),'Y':('EGPD_Input',1),'Z':('EGPD_Input',2),'ReturnValue':('EGPD_Output',0)},
 'N6':{'self':('EGPD_Input',0),'StaticMeshComponent':('EGPD_Output',0)},
 'N7':{'execute':('EGPD_Input',0),'self':('EGPD_Input',1),'NewMesh':('EGPD_Input',2),'then':('EGPD_Output',0)},
 'N8':{'execute':('EGPD_Input',0),'self':('EGPD_Input',1),'ElementIndex':('EGPD_Input',2),'Material':('EGPD_Input',3),'then':('EGPD_Output',0)},
 'N9':{'execute':('EGPD_Input',0),'self':('EGPD_Input',1),'InCollisionProfileName':('EGPD_Input',2),'then':('EGPD_Output',0)},
 'N10':{'execute':('EGPD_Input',0),'self':('EGPD_Input',1),'ParentActor':('EGPD_Input',2),'LocationRule':('EGPD_Input',4),'RotationRule':('EGPD_Input',5),'ScaleRule':('EGPD_Input',6),'then':('EGPD_Output',0)},
 'NR':{'self':('EGPD_Output',0)},
 'T1':{'ReturnValue':('EGPD_Output',0)},
 'T2':{'A':('EGPD_Input',0),'B':('EGPD_Input',1),'ReturnValue':('EGPD_Output',0)},
 'T3':{'Roll':('EGPD_Input',0),'Pitch':('EGPD_Input',1),'Yaw':('EGPD_Input',2),'ReturnValue':('EGPD_Output',0)},
 'T4':{'execute':('EGPD_Input',0),'self':('EGPD_Input',1),'DeltaRotation':('EGPD_Input',2),'then':('EGPD_Output',0)},
 'T5':{'PlayerIndex':('EGPD_Input',0),'ReturnValue':('EGPD_Output',0)},
 'T6':{'exec':('EGPD_Input',0),'InputObject':('EGPD_Input',1),'Is Valid':('EGPD_Output',0),'Is Not Valid':('EGPD_Output',1)},
 'T7':{'self':('EGPD_Input',0),'ReturnValue':('EGPD_Output',0)},
 'T8':{'self':('EGPD_Input',0),'ReturnValue':('EGPD_Output',0)},
 'T9':{'V1':('EGPD_Input',0),'V2':('EGPD_Input',1),'ReturnValue':('EGPD_Output',0)},
 'T10':{'A':('EGPD_Input',0),'B':('EGPD_Input',1),'ReturnValue':('EGPD_Output',0)},
 'T11':{'execute':('EGPD_Input',0),'Condition':('EGPD_Input',1),'then':('EGPD_Output',0),'else':('EGPD_Output',1)},
 'T12':{'execute':('EGPD_Input',0),'self':('EGPD_Input',1),'then':('EGPD_Output',0)},
}
def OP(k,name):
    d,i=PIN[k][name]; return {"direction":d,"index_id":i,"node":{"refPath":nodes[k]}}
def IP(k,name):
    d,i=PIN[k][name]; return {"direction":d,"index_id":i,"node":{"refPath":nodes[k]}}
# 断开 N10.self 的错误连接
try:
    ue.call(BT,"break_pins",{"input_pin":IP('N10','self')})
except Exception: pass
# 创建 Cast(若无)
if not cast:
    r=ue.call(BT,"create_node",{"graph":{"refPath":G},"type_id":"工具|Casting|CastToStaticMeshActor","pos":{"x":-1550,"y":-450}})
    cast=(r.get('refPath') or (r.get('returnValue') or {}).get('refPath')) if isinstance(r,dict) else None
    print('Cast 新建:',(cast or 'FAIL').split('.')[-1])
    time.sleep(1)
if not cast: raise SystemExit('Cast 失败')
def manual(node,d,i): return {"direction":d,"index_id":i,"node":{"refPath":node}}
def castpin(d,name,idx):
    if cast in pinmap:
        m,e=pinmap[cast]
        if (d,name) in m: return m[(d,name)]
    return manual(cast,d,idx)
fails=[]
def wire(o,i):
    r=ue.call(BT,"connect_pins",{"output_pin":o,"input_pin":i})
    if isinstance(r,str): fails.append('WIRE '+str(r)[:110])
wire(OP('N1','ReturnValue'),castpin('EGPD_Input','Object',1))
wire(OP('E1','then'),castpin('EGPD_Input','execute',0))
wire(castpin('EGPD_Output','then',0),IP('N7','execute'))
cr=castpin('EGPD_Output','ReturnValue',2)
wire(cr,IP('N6','self'))
wire(cr,IP('N10','self'))
print('失败数:',len(fails))
for f in fails[:10]: print('  ',f)
r=ue.call(BT,"compile_blueprint",{"blueprint":{"refPath":BP}})
print('编译:',json.dumps(r,ensure_ascii=False)[:600] if isinstance(r,dict) else str(r)[:600])
print('保存:',str(ue.call(AT,"save_assets",{"asset_paths":[BP]}))[:150])
