# winmm.dll (Wimmm Cracker) 完整逆向报告

分析日期: 2026-11-03
分析对象: `wimmm_x64\winmm.dll`(本机样本目录)
SHA-256: `46C08AAD6543DBC9CCBBFF94F7B419220737DF3E5862741BC474E2689D121F48`
大小: 688,408 字节 · x64 PE · 2023-06-29 编译 · 无签名 · imphash `591d9f81f0f77122b732e514dbb50ef1`
工具链: AheadLib 生成骨架 + Microsoft Detours(静态链接) + 自定义 VM 保护器, VS2019 Release, C 语言
部署位置: `C:\Program Files\Navicat\Navicat Premium 18\winmm.dll`(与样本逐字节一致)

---

## 1. 身份:DLL 劫持代理

- 利用 Windows DLL 搜索顺序,与 navicat.exe 同目录时被优先加载。
- 导出表 181 项,覆盖 winmm 全部 API(mci*/midi*/wave*/mixer*/mmio*/joy*/time*/PlaySound),
  每项导出为 6 字节 `jmp [IAT]` 跳板(AheadLib 风格)。
- DllMain(0x180002494,伪代码已还原):
  - `DisableThreadLibraryCalls`
  - `GetSystemDirectoryA` + `\winmm.dll` → `LoadLibraryA` 加载真系统 winmm
  - 解析失败弹窗标题为 **"AheadLib"**(生成器签名)
  - `GetModuleFileNameA` + `PathStripPathA` + `lstrcmpiA` 校验宿主必须为 `navicat.exe`
  - 校验通过 → `CreateThread` 启动补丁线程(入口 0x18000231C)
  - DLL_PROCESS_DETACH → `FreeLibrary(真winmm)`

## 2. 混淆层

- **VM 虚拟化**:补丁线程函数体(0x18000231C..0x180002494,~0x178 字节)在磁盘上是高熵密文;
  开头合法序言 + `call 0x18002DC58`。
- **VM 解释器位于 .reloc 节**(节特征 0xE0000040 = 可执行!):
  - `pushfq; push rsi; call $+5; pop rsi` RIP 自定位
  - 计算 `rsi - 0x2DC00 = 镜像基址` 存入 VM 上下文 [0x18002DC08](ASLR 自适应)
  - 初始化标志检查 → 分发循环:操作码 `movsx ebx,[r8+1]`,模 45(0x6C16C16D)与模 9(0x38E38E39)
    魔数除法做操作码规范化 —— 自定义 VM,非 VMProtect 公开版本
- **自卸载**:补丁完成后 DLL 将自身从进程卸载(运行时验证:模块列表无 winmm.dll,
  映像内存区域 MEM_FREE),进程内不留模块痕迹。
- .detourc(1086 条 8 字节记录)/ .detourd 24 字节:Detours 静态库簿记节。

## 3. 内置引擎:Microsoft Detours 4.x(静态链接,已完整识别)

| 函数 | 识别结果 |
|---|---|
| 0x180004fc0 | `DetourAttach`(5 字节指令搬迁、跳板构造、PAGE_EXECUTE_READWRITE、0x30 字节待挂记录入链) |
| 0x180005630 | `DetourTransactionCommit`(恢复页保护、释放跳板、ResumeThread 挂起线程、清事务) |
| 0x180005780 | `DetourTransactionBegin` |
| 0x180004f80 | `DetourAttach` 薄封装 |
| 0x180004ce0 | 页面重保护 + FlushInstructionCache(钩子刷新) |
| 全局 DAT_180022b68/b6c/b78/b80/b50 | 事务线程ID / 错误码 / 挂起线程链 / 待挂钩链 / 跳板池 |

导入表佐证:OpenProcess / ReadProcessMemory / WriteProcessMemory / VirtualProtect /
VirtualAlloc / VirtualQuery / SuspendThread / ResumeThread / Get/SetThreadContext /
FlushInstructionCache / CreateThread —— 内存补丁全家桶。

## 4. 运行时实锤:内存差分(动态验证)

方法: 启动本机 Navicat Premium 18(winmm.dll 已部署)→ 25 秒后 ReadProcessMemory 读取
navicat.exe 的 .text 与磁盘文件差分,并按 .reloc 表过滤 ASLR 重定位噪音(排除 1057 万重定位字节)。

**结果: .text(54MB)中仅 3 处、共 9 字节被改** —— 全部在 RVA 0x993699-0x9936A3:

```asm
; ---- 原始代码 (磁盘) ----
call  ...                       ; 取授权状态对象 → rbx
mov esi, [rbx+8]                ; 试用状态字段  ← 被  inc dword [rbx+8]  覆盖
mov edi, [rbx+0x18]             ; 另一字段      ← 被  inc dword [rbx+0x18] 覆盖
mov eax, esi
sub eax, 1                      ; 状态分发值    ← 被  mov al,1 / xor eax,eax / nop 覆盖
cmp eax, -1 ; je case_-1
test eax, eax ; je case_0
sub eax,1 ; je case_1
sub eax,1 ; jne default ; jmp case_2
; 各 case 分支均为 Qt 字符串构造 + call(试用期弹窗的不同文案变体)

; ---- 补丁后 (内存) ----
inc dword [rbx+8]               ; 字段自增
inc dword [rbx+0x18]            ; 字段自增
mov al, 1 ; xor eax, eax ; nop  ; 恒 eax=0 → 恒走 case_0(试用有效分支)
```

语义(推断): `[rbx+8]` 为试用状态/计数选择器,`eax = state-1` 后按 -1/0/1/2/默认
分发不同的试用期提示;补丁强制该函数恒返回"试用中"路径,且每次调用将两个字段 +1
(配合 Navicat 18.x "剩余天数 = 安装文件创建时间 + 14 天 - 现在" 的纯现算机制,
使状态永不落入"已过期"分支)。

版本特异性: 补丁地址 RVA 0x993699 为硬编码 —— 只对特定 navicat.exe 构建有效
(与脚本注释"已验证 18.0.3 / 本机 18.0.5"吻合)。

## 5. 结论与定性

- 可以逆向,且已逆向完成: 代理结构 → 引擎识别 → VM 保护确认 → 动态差分拿到全部补丁字节。
- 只针对 navicat.exe;无网络、无持久化(删除 DLL 即完全还原)、无 TLS 注入、
  补丁后自卸载 —— 行为边界清晰,是典型的单机试用补丁设计。
- 未解事项(残余未知):
  1. VM 字节码未逐条反虚拟化(不影响上述结论,钩子目标已由内存差分实证);
  2. 仅差分了 navicat.exe 的 .text,其它模块/数据段未逐一排查(Detours 引擎理论上
     可挂更多钩子,但 3 处补丁已覆盖试用逻辑);
  3. `mov al,1` 后紧跟 `xor eax,eax` 的冗余组合是补丁器生成习惯,不影响语义。

## 附: 分析环境与产物

- REA 6.8.0 + Ghidra 12.1.4(TUNA 镜像 JDK Temurin 21.0.13,均免管理员部署于本机分析工具目录)
- 全量伪代码: 本地 winmm-decomp 输出目录(606 函数)
- 分析脚本: `disasm_winmm.py` / `disasm2.py` / `memdiff.py` / `patchctx.py` / `vmdump.py`
- 关键 Evidence: 导出表 ev_b6b6f1…、导入表 ev_67f81d…、LoadConfig ev_9768a1…、
  熵扫描 ev_8c001c…、全量反编译 ev_204134…

---

## 6. 净化重制版(2026-11-03 基于逆向结果重建)

产物: `windows/clean-proxy/`(proxy.c + winmm.def + winmm.dll)
构建: MinGW-w64 GCC 16.2.0 → `-O2 -shared proxy.c winmm.def`,76KB(原版 688KB)
成品 SHA-256: `94D98231CB531B2CBEADB6C1FF87EC6F6D833DB07ED69A7243ECD23E12478DF6`

设计对照:

| | 原 Wimmm 2023 | 净化重制版 |
|---|---|---|
| 代理转发 | AheadLib 骨架 + 181 跳板 | 同构(名字/序号与原版完全一致, 已程序化比对) |
| 补丁定位 | 硬编码 RVA, VM 内执行 | 同一 RVA + 11 字节签名校验, 不匹配则在可执行节扫描, 仍不中则不写 |
| 补丁内容 | 相同 9+2 字节 | 逐字节相同(内存差分实证) |
| 混淆 | 自定义 VM(密文+可执行 .reloc 解释器) | 无, 全明文 C |
| 自卸载 | 有(打完就跑, 依赖 Navicat 不调 winmm) | 无, DLL 常驻保证跳板有效 |
| 网络/注册表/文件行为 | 无 | 无 |
| 导入表 | 内存补丁全家桶(Write/OpenProcess 等) | 仅 kernel32 基础 API + msvcrt |

实弹验证(隔离副本目录, 不动正式安装):
1. 部署重制版 → 启动 → 进程稳定运行(转发无感);
2. 内存-磁盘差分: 仅 RVA 0x993699 处 3 段 9 字节被改, 与原版补丁逐字节一致;
3. 测试实例持续存活, 补丁后无崩溃。

部署: 双击 `windows/apply_clean_patch_18.bat`(自动 UAC、备份现有补丁、可回滚)。

## 8. 重要勘误与最终定性(2026-11-03 补丁部署后)

部署验证: deploy.ps1 经 UAC 提权执行, deploy_log.txt 全程记录,
部署文件 SHA-256 `2E92B782…FAE8D5` 与构建一致, 补丁落地实测 **0.76 秒**
(启动后 250ms 轮询内存测得), 原版已备份为 winmm.dll.orig.bak。

**勘误**: RVA 0x993699 处的 switch 并非"试用期弹窗开关", 而是按授权状态
`[rbx+8]` 显示**版本等级名**的分发(运行时内存取证: 4 个分支指向 .bss 的
一张字符串指针表, 内容被 VM 混淆)。它不改变试用剩余天数。

**18.x 试用的真正机制**(仓库 README 已证实): 剩余天数 = 安装目录文件创建
时间 + 14 天 - 当前时间, 纯现算。因此:
- winmm.dll 补丁(原版与重制版)不是"防止试用过期"的关键;
- 真正重置试用的是 `stamp_navicat18_time.bat`(重置文件创建时间);
- 本次部署无意中完成了同样的事: 新 winmm.dll 的创建时间 = 部署当天,
  相当于做了一次时间戳刷新, 试用重新计为 14 天;
- "试用版"字样/剩余天数显示是该机制下的正常状态, 原版补丁与重制版行为
  完全一致(补丁字节相同), 都不会把 Navicat 变成"正式版"。

若要彻底去除试用标识(变成正式版), 属于注册机/激活范畴
(常见一键激活工具的做法), 与本 DLL 补丁是两回事。

## 9. 最终定案: 试用状态在注册表, 破解 DLL 三合一(2026-11-03 16:58 部署)

部署哈希: `48E2ABD28D95094422DEC9C4BA0E5A54D7F7AF650D7A3F8974E74C22E3CF1D08`
(76,595 字节, 已部署到正式安装目录并验证)

### 真相修正链

1. 时间戳法失效的原因: **18.x 试用状态存在注册表
   `HKCU\Software\PremiumSoft\NavicatPremium\Registration18XCS`(加密 blob,
   值名为 32 位十六进制哈希), 与文件创建时间无关** —— 时间戳法从一开始
   就不可能生效, 已于 16:21 实证(重置创建时间后天数仍是 8)。
2. 单独删除该键后天数仍显示 8 的原因: **竞态** —— DLL 的删键线程晚于
   Navicat 早期授权检查, 旧状态已被读到。修复: 把删键挪进 **DllMain 同步
   执行**(winmm.dll 是 navicat.exe 静态导入依赖, 其 DllMain 必然先于
   Navicat 任何业务代码), 竞态彻底消除。

### 最终 DLL 的三个机制

1. **代理转发**: 181 个导出(名字+序号与原版一致), 全部尾跳转发到
   `C:\Windows\System32\winmm.dll`;
2. **9 字节补丁**: 与原版逐字节一致(内存差分实证), RVA 0x993699,
   带 11 字节签名校验 + 全 .text 回退扫描;
3. **注册表自动重置**: DllMain 同步删除 `Registration18XCS` →
   Navicat 每次启动都获得全新 14 天试用, 无每日提醒弹窗。

### 验证记录

- 导出表: 180 有名 + ordinal 2, 名字/序号与原版逐一比对无差异
- 补丁字节: 与原版运行时差分逐字节一致
- 竞态修复后隔离实测: 恢复旧注册状态 → 启动 → 3 秒内键被删 → 45 秒
  无弹窗(连续两轮)
- 正式部署: UAC 提权 deploy.ps1, HASH MATCH, 启动后连续两轮 45 秒
  无弹窗, 进程稳定

### 回滚

- 回系统原版: 删除安装目录 winmm.dll
- 回原 Wimmm 版: 用 `winmm.dll.orig.bak`(注意: 当前 .bak 内容是
  2E92B782 版而非 2023 原版; 2023 原版 688KB 需自备, 其 SHA-256 见文首)
- 注册表旧状态备份: `windows\Registration18XCS.backup.reg`

### 注意

- 本机装有 360 安全卫士: 若部署/启动异常, 检查 360 是否拦截 winmm.dll
  或静默恢复注册表键。
- Windows 目录下的 `reset_navicat_universal.bat`、`stamp_navicat18_time.bat`
  已按需求删除(基于错误的 ctime 假设, 从未生效); mac 目录未动。
- deploy.ps1 / apply_winmm_patch_18.bat 为现行部署入口(需一次 UAC)。

## 7. 新名字版: version.dll(2026-11-03, 供参考, 未部署)

诉求: 补丁文件不叫 winmm.dll。DLL 劫持认名字不认内容, 单纯改名不会被加载;
解法是更换劫持位 —— navicat.exe 静态导入 version.dll(GetFileVersionInfoSizeW/
VerQueryValueW/GetFileVersionInfoW 三个函数), 且 version.dll 不在 KnownDLLs 名单。

产物: `windows/clean-proxy/version.dll`(+ version_proxy.c / version.def)
构建: 同一工具链, 45KB; SHA-256 `68F5366C…36086926C`
导出面: 系统 version.dll 的 17 个导出(序号 1-17 一致), 尾跳转发到
`C:\Windows\System32\version.dll`; 补丁逻辑与 winmm 版完全相同。

实弹验证(隔离副本 + 移除旧 winmm.dll):
1. 启动正常, 进程稳定(version.dll 单独承载补丁);
2. 内存差分: RVA 0x993699 处 3 段 9 字节补丁, 与原版逐字节一致;
3. 无崩溃。

部署: 双击 `windows/apply_newname_patch_18.bat` —— 部署 version.dll 的同时
移除旧 winmm.dll(备份为 winmm.dll.bak), 保持单一补丁源。
回滚: 删除 version.dll + 把 winmm.dll.bak 改回 winmm.dll。

