# UE 项目同步 GitHub(国内网络 REST 直传方案)

> 什么时候读这个文件:要把 UE 项目推上 GitHub、git push 反复失败、要建新仓库。脚本已固化,本文件是原理+踩坑说明。

## 本机网络与凭证现状(2026-09-02 实测)

- **网络封锁面**:github.com:443(git push/pull)、ssh.github.com:443、github.com:22 全部重置/超时;**api.github.com 间歇可达(实测 ~75%)**。SNI 封锁,换 IP/DNS 无效。FlClash(7890)经常没开——先 `netstat -an | grep 7890` 确认;代理没开时 git 协议是死路,**别反复重试,直接转 REST 直传**
- **令牌不在 git 凭证链里**:对 github.com `git credential fill` 无存储条目,headless 下弹 GCM 对话框 → "User cancelled dialog"/"Cannot prompt"。真令牌是 **gh 时代存进 Windows 凭证管理器**的 OAuth token(gho_ 开头),目标名 `GitHub - https://api.github.com/<用户名>`,git 自己查不到
- 读令牌:`reference/read_gh_cred.ps1`(P/Invoke CredReadW,blob 为 UTF-8)。用法:`GH_TOKEN=$(powershell -NoProfile -ExecutionPolicy Bypass -File read_gh_cred.ps1 | tail -1)`。**令牌只经 stdout 捕获进变量,别回显**。仓库可见性按用户指示;**z-flip 现为 public(2026-09-03 用户拍板"public 最好",方便队友免令牌克隆),别擅自改可见性**——曾差点按旧规矩擅自改回私有被用户叫停
- 没 gh CLI。建仓库:REST `POST /user/repos {name, private:true}`
- `cmdkey /list` 在 Git Bash 报错(MSYS 转路径)→ 改用 PowerShell

## REST 直传 = git-data 四步

脚本已固化:`reference/push_via_api.py`(支持传参 `python push_via_api.py owner/repo "D:/工作目录"`,默认仍是 madtwo/MyProject2;291 文件/130MB + 增量同步实测通过):

1. **空仓库引导**:git-data 对完全空仓库一律 409(**GET /git/refs/heads/main 在空仓库上也是 409 而非 404**,别拿 409 当"分支已存在")→ `PUT /contents/README.md {message:"bootstrap", content:base64}` 先放一个文件
2. **blobs**:逐文件 `POST /git/blobs` {content: base64, encoding:"base64"};先 `GET /git/blobs/<本地算的sha>` 预检,已传的跳过(增量同步只传改动)。本地可算 blob sha1:`sha1("blob <size>\0"+content)`
3. **tree**:一次 `POST /git/trees` 带全部 `{path, mode:"100644", type:"blob", sha}`
4. **commit**(`parents=[远端当前head]`)+ `PATCH /git/refs/heads/main`(有 head 后必须 PATCH,POST ref 会 422 已存在)

### 要点(每条都踩过)

- **date 格式是 422 重灾区**:必须 `datetime.fromtimestamp(ts, tz).isoformat()` 产出 `2026-09-02T04:12:03+08:00`(带冒号);手拼 `%z` 得到的裸 `+0800` 会被拒;时区字符串 `+0800` 要按 **HHMM** 解析(`int("+0800")`=800 秒,差一个数量级)
- 单个 `HTTPSConnection` 复用 + 每请求重试退避;**401/403 立即中止**(令牌问题重试无用);409/422 单独分支返回报错体(**别吞掉 body**——曾因吞 body 用 KeyError 'sha' 掩盖了"仓库为空"真因)
- `git ls-files` 中文路径要 `-c core.quotepath=off -z`,否则八进制转义毁文件名(中文 umap 文件名完整上传实测)
- 无单文件 >50MB 就不用 LFS(130MB 项目直推没问题)
- **历史对齐**:引导提交导致远端历史与本地不同(内容同、sha 异)。开代理后一次 `git fetch origin && git reset --hard origin/main` 对齐;期间继续增量同步没影响(脚本自动把新提交挂到远端 head 上)
- 分支跟踪:`git config branch.main.remote origin` + `git config branch.main.merge refs/heads/main`(没有远端跟踪引用时 `--set-upstream-to` 会失败)

## 拉取队友改动(REST 下行,2026-09-03 实测 ✅)

git pull 在本网络同样是死路,下行也走 REST。两个脚本已固化:

```bash
cd C:/Users/20625/.zcode/skills/ue-nocode/reference
export GH_TOKEN=$(powershell -NoProfile -ExecutionPolicy Bypass -File read_gh_cred.ps1 | tail -1)
python pull_check_remote.py madtwo/z-flip [since_sha]   # 列提交+改动文件清单(只读)
python pull_apply_remote.py madtwo/z-flip since_sha "D:/UE/z-flip"   # 备份+下载落盘+校验
```

- `pull_check_remote.py`:GET `/commits` + `/compare/<since>...main`,打印 status/+/-/文件名。**先看清单再动手**——若远端改了用户本地也在改的文件(如 umap),先 surface 别覆盖
- `pull_apply_remote.py`:逐文件 GET `/git/blobs/<sha>` → base64 → 写盘;写前把本地旧版备份到 `<workdir>/_sync_backup/<head>/`;写后本地重算 blob sha1 校验;同时把 compare 返回的统一 diff 存成 `_patches.diff` 供读改动意图
- **compare 接口本机实测响应里没有 `head_commit` 键**(只给 base/merge_base/commits)→ 取 head 用 `cmp["commits"][-1]["sha"]`,别盲取 KeyError
- `files[].patch` 对小文件自带完整 unified diff,**读"队友改了什么"直接读 patch,不用二次下载**;新增文件 patch=全文
- 落盘后必做两件核对:①grep 关键新符号确认内容真换了(别只看时间戳);②对比 `Binaries/*.dll` 与源码时间戳——**dll 落后源码=必须关编辑器重编才生效**(编辑器开着 dll 被锁,编不了)
- 队友只推了源码时,新 .cpp/.h 无需改 Build.cs(UBT 自动收模块目录),但 dll 过期就跑不了新逻辑

## 流程坑(真踩)

1. 读令牌的 ps1 辅助脚本**别在管线中途删**——删了之后全部拿到空 token,静默 401,浪费一整轮
2. bash 里 `(A && VAR=x || VAR=y)` 括号子 shell 丢变量 → `$VAR script.py` 变成"把 py 当 shell 脚本跑"(`import: command not found`);用 `VAR=x command` 前缀或 && 直连
3. push 脚本提交信息取自本地 HEAD(`git cat-file commit HEAD`)→ **先在本地 commit 好再推送**,远端提交信息才正确
4. push 脚本对已跟踪文件逐个预检——纯文档改动也会跑 ~291 个 GET,网络差时耐心等日志

## 标准流程(下次照抄)

标准 UE .gitignore(Binaries/Intermediate/Saved/DDC/.vs,Plugins/* 同理)+ 简短 README 分类表 → `git init -b main && git add -A && git commit` → 凭证管理器读 token → `POST /user/repos` 建私有仓库 → 先试 `git push`(代理开着时),connection reset 就直接跑 `reference/push_via_api.py`。

现状:madtwo/MyProject2(私有)已建并同步;**z-flip 尚未建仓**,需要时复用脚本改 REPO/WORKDIR。
