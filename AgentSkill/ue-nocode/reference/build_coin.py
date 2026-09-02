import json,urllib.request
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
        self.sid=sid; self.i=4900
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
E1=G+'.K2Node_CustomEvent_2'; E2=G+'.K2Node_CustomEvent_3'
pos={'N1':(-1900,-500),'N2':(-2350,-700),'N3':(-2700,-750),'N4':(-2700,-500),'N5':(-2700,-250),'N6':(-1550,-100),'N7':(-1250,-100),'N8':(-950,-100),'N9':(-650,-100),'N10':(-350,-100),'NR':(-700,-600),'T1':(900,-400),'T2':(1150,-400),'T3':(1450,-450),'T4':(1800,-350),'T5':(2100,-350),'T6':(2400,-350),'T7':(2400,-800),'T8':(2700,-800),'T9':(3000,-750),'T11':(3600,-300),'T12':(3950,-250)}
key_by_pos={(v[0],v[1]):k for k,v in pos.items()}
nodes={'E1':E1,'E2':E2}
refs=[x['refPath'] for x in ue.call(BT,"find_nodes",{"graph":{"refPath":G},"title":""}).get('returnValue',[]) if isinstance(x,dict)]
for r in refs:
    if r in (E1,E2): continue
    info=ue.call(BT,"get_node_infos",{"nodes":[{"refPath":r}]})
    e=(info.get('returnValue') or [{}])[0]
    p=e.get('position') or {}
    k=key_by_pos.get((p.get('x'),p.get('y')))
    if k: nodes[k]=r
    else: ue.call(BT,"delete_node",{"node":{"refPath":r}})
missing=[k for k in pos if k not in nodes]
print('认领:',len([k for k in nodes if k not in('E1','E2')]),'缺失:',missing or '无')
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
 'T11':{'execute':('EGPD_Input',0),'Condition':('EGPD_Input',1),'then':('EGPD_Output',0),'else':('EGPD_Output',1)},
 'T12':{'execute':('EGPD_Input',0),'self':('EGPD_Input',1),'then':('EGPD_Output',0)},
}
def OP(k,name):
    d,i=PIN[k][name]; return {"direction":d,"index_id":i,"node":{"refPath":nodes[k]}}
def IP(k,name):
    d,i=PIN[k][name]; return {"direction":d,"index_id":i,"node":{"refPath":nodes[k]}}
fails=[]
def wire(o,i):
    r=ue.call(BT,"connect_pins",{"output_pin":o,"input_pin":i})
    if isinstance(r,str): fails.append('WIRE '+str(r)[:110])
def val(k,name,v):
    d,i=PIN[k][name]
    r=ue.call(BT,"set_pin_value",{"pin":{"direction":d,"index_id":i,"node":{"refPath":nodes[k]}},"value":v})
    if isinstance(r,str): fails.append(f'VAL {k}.{name} '+str(r)[:90])
T9out={"direction":"EGPD_Output","index_id":0,"node":{"refPath":nodes['T9']}}
r=ue.call(BT,"find_node_types",{"graph":{"refPath":G},"type_id_filter":"小于","context_pins":[T9out]})
cands=[x if isinstance(x,str) else x.get('type_id','?') for x in (r.get('returnValue') or [])]
print('候选:',cands)
t10=None
if cands:
    cand=cands[-1]
    r=ue.call(BT,"create_node",{"graph":{"refPath":G},"type_id":cand,"pos":{"x":3300,"y":-700}})
    ref=(r.get('refPath') or (r.get('returnValue') or {}).get('refPath')) if isinstance(r,dict) else None
    if ref:
        nodes['T10']=ref
        PIN['T10']={'A':('EGPD_Input',0),'B':('EGPD_Input',1),'ReturnValue':('EGPD_Output',0)}
        r=ue.call(BT,"connect_pins",{"output_pin":T9out,"input_pin":IP('T10','A')})
        print('连接A:',str(r)[:100])
        e=(ue.call(BT,"get_node_infos",{"nodes":[{"refPath":ref}]}).get('returnValue') or [{}])[0]
        tid=str(e.get('type_id'))
        print('连线后类型:',tid)
        if 'Timespan' in tid or '时间' in tid:
            ue.call(BT,"delete_node",{"node":{"refPath":ref}}); nodes.pop('T10'); PIN.pop('T10')
        else:
            t10=ref
print('T10 状态:','OK' if t10 else '失败')
if not t10: raise SystemExit('比较节点失败')
wire(OP('E1','then'),IP('N1','execute'))
wire(OP('N1','then'),IP('N7','execute')); wire(OP('N7','then'),IP('N8','execute')); wire(OP('N8','then'),IP('N9','execute')); wire(OP('N9','then'),IP('N10','execute'))
wire(OP('N2','ReturnValue'),IP('N1','SpawnTransform'))
wire(OP('N3','ReturnValue'),IP('N2','Location')); wire(OP('N4','ReturnValue'),IP('N2','Rotation')); wire(OP('N5','ReturnValue'),IP('N2','Scale'))
wire(OP('N1','ReturnValue'),IP('N6','self')); wire(OP('N1','ReturnValue'),IP('N10','self'))
wire(OP('N6','StaticMeshComponent'),IP('N7','self')); wire(OP('N6','StaticMeshComponent'),IP('N8','self')); wire(OP('N6','StaticMeshComponent'),IP('N9','self'))
wire(OP('NR','self'),IP('N10','ParentActor'))
val('N1','Class','/Script/Engine.StaticMeshActor'); val('N1','CollisionHandlingOverride','AlwaysSpawn')
val('N4','Roll','90.0'); val('N5','X','1.0'); val('N5','Y','1.0'); val('N5','Z','0.15')
val('N7','NewMesh','/Engine/BasicShapes/Cylinder.Cylinder')
val('N8','ElementIndex','0'); val('N8','Material','/Game/ThirdPerson/Materials/M_CoinGold.M_CoinGold')
val('N9','InCollisionProfileName','NoCollision')
val('N10','LocationRule','SnapToTarget'); val('N10','RotationRule','SnapToTarget'); val('N10','ScaleRule','KeepWorld')
wire(OP('E2','then'),IP('T4','execute')); wire(OP('T4','then'),IP('T6','exec'))
wire(OP('T6','Is Valid'),IP('T11','execute')); wire(OP('T11','then'),IP('T12','execute'))
wire(OP('T1','ReturnValue'),IP('T2','A')); val('T2','B','140.0')
wire(OP('T2','ReturnValue'),IP('T3','Yaw')); wire(OP('T3','ReturnValue'),IP('T4','DeltaRotation'))
wire(OP('T5','ReturnValue'),IP('T6','InputObject')); val('T5','PlayerIndex','0')
wire(OP('T5','ReturnValue'),IP('T8','self'))
wire(OP('T7','ReturnValue'),IP('T9','V1')); wire(OP('T8','ReturnValue'),IP('T9','V2'))
wire(OP('T9','ReturnValue'),IP('T10','A')); val('T10','B','130.0')
wire(OP('T10','ReturnValue'),IP('T11','Condition'))
print('失败数:',len(fails))
for f in fails[:10]: print('  ',f)
r=ue.call(BT,"compile_blueprint",{"blueprint":{"refPath":BP}})
print('编译:',json.dumps(r,ensure_ascii=False)[:600] if isinstance(r,dict) else str(r)[:600])
print('保存:',str(ue.call(AT,"save_assets",{"asset_paths":[BP]}))[:150])
