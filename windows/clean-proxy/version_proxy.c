/* ============================================================================
 * version_proxy.c -- Navicat 18.x 试用状态补丁 DLL(新名字版: version.dll)
 *
 * 与 clean-proxy/winmm.dll 完全同源的逻辑, 仅更换劫持位:
 *   - navicat.exe 静态导入 version.dll(GetFileVersionInfoSizeW 等三个函数),
 *     且 version.dll 不在 KnownDLLs 中 => 同目录 version.dll 会被优先加载;
 *   - 加载时打开 C:\Windows\System32\version.dll, 17 个导出全部
 *     `jmp [ptr]` 尾跳转发;
 *   - 仅当宿主是 navicat.exe 时启动补丁线程: 校验 RVA 0x993699 的
 *     11 字节签名, 命中则写入与原 Wimmm 补丁逐字节相同的 11 字节补丁
 *     (inc 两个计数字段 + 强制 eax=0 走"试用中"分支);
 *     未命中则在整个可执行节扫描, 仍不中则不写内存。
 *
 * 安全设计同 winmm 净化版: 无 VM/无自卸载/无网络/无注册表; 签名不符不写。
 * 注意: 使用本 version.dll 时应移除同目录的 winmm.dll(旧补丁), 避免双补丁源。
 * 构建: x86_64-w64-mingw32-gcc -O2 -shared -o version.dll version_proxy.c version.def
 * ==========================================================================*/
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HMODULE g_real = NULL;

#define PATCH_RVA      0x993699
#define PATCH_LEN      11
static const unsigned char SIG_ORIG[PATCH_LEN] =
    { 0x8B,0x73,0x08, 0x8B,0x7B,0x18, 0x89,0xF0,0x83,0xE8,0x01 };
static const unsigned char SIG_DONE[PATCH_LEN] =
    { 0xFF,0x43,0x08, 0xFF,0x43,0x18, 0xB0,0x01,0x31,0xC0,0x90 };

static FARPROC fp_GetFileVersionInfoA;
__attribute__((naked)) void __stdcall fwd_GetFileVersionInfoA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_GetFileVersionInfoA)); }
static FARPROC fp_GetFileVersionInfoByHandle;
__attribute__((naked)) void __stdcall fwd_GetFileVersionInfoByHandle(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_GetFileVersionInfoByHandle)); }
static FARPROC fp_GetFileVersionInfoExA;
__attribute__((naked)) void __stdcall fwd_GetFileVersionInfoExA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_GetFileVersionInfoExA)); }
static FARPROC fp_GetFileVersionInfoExW;
__attribute__((naked)) void __stdcall fwd_GetFileVersionInfoExW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_GetFileVersionInfoExW)); }
static FARPROC fp_GetFileVersionInfoSizeA;
__attribute__((naked)) void __stdcall fwd_GetFileVersionInfoSizeA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_GetFileVersionInfoSizeA)); }
static FARPROC fp_GetFileVersionInfoSizeExA;
__attribute__((naked)) void __stdcall fwd_GetFileVersionInfoSizeExA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_GetFileVersionInfoSizeExA)); }
static FARPROC fp_GetFileVersionInfoSizeExW;
__attribute__((naked)) void __stdcall fwd_GetFileVersionInfoSizeExW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_GetFileVersionInfoSizeExW)); }
static FARPROC fp_GetFileVersionInfoSizeW;
__attribute__((naked)) void __stdcall fwd_GetFileVersionInfoSizeW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_GetFileVersionInfoSizeW)); }
static FARPROC fp_GetFileVersionInfoW;
__attribute__((naked)) void __stdcall fwd_GetFileVersionInfoW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_GetFileVersionInfoW)); }
static FARPROC fp_VerFindFileA;
__attribute__((naked)) void __stdcall fwd_VerFindFileA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_VerFindFileA)); }
static FARPROC fp_VerFindFileW;
__attribute__((naked)) void __stdcall fwd_VerFindFileW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_VerFindFileW)); }
static FARPROC fp_VerInstallFileA;
__attribute__((naked)) void __stdcall fwd_VerInstallFileA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_VerInstallFileA)); }
static FARPROC fp_VerInstallFileW;
__attribute__((naked)) void __stdcall fwd_VerInstallFileW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_VerInstallFileW)); }
static FARPROC fp_VerLanguageNameA;
__attribute__((naked)) void __stdcall fwd_VerLanguageNameA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_VerLanguageNameA)); }
static FARPROC fp_VerLanguageNameW;
__attribute__((naked)) void __stdcall fwd_VerLanguageNameW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_VerLanguageNameW)); }
static FARPROC fp_VerQueryValueA;
__attribute__((naked)) void __stdcall fwd_VerQueryValueA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_VerQueryValueA)); }
static FARPROC fp_VerQueryValueW;
__attribute__((naked)) void __stdcall fwd_VerQueryValueW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_VerQueryValueW)); }

static FARPROC *const fp_TABLE[] = {
    (FARPROC *)&fp_GetFileVersionInfoA,
    (FARPROC *)&fp_GetFileVersionInfoByHandle,
    (FARPROC *)&fp_GetFileVersionInfoExA,
    (FARPROC *)&fp_GetFileVersionInfoExW,
    (FARPROC *)&fp_GetFileVersionInfoSizeA,
    (FARPROC *)&fp_GetFileVersionInfoSizeExA,
    (FARPROC *)&fp_GetFileVersionInfoSizeExW,
    (FARPROC *)&fp_GetFileVersionInfoSizeW,
    (FARPROC *)&fp_GetFileVersionInfoW,
    (FARPROC *)&fp_VerFindFileA,
    (FARPROC *)&fp_VerFindFileW,
    (FARPROC *)&fp_VerInstallFileA,
    (FARPROC *)&fp_VerInstallFileW,
    (FARPROC *)&fp_VerLanguageNameA,
    (FARPROC *)&fp_VerLanguageNameW,
    (FARPROC *)&fp_VerQueryValueA,
    (FARPROC *)&fp_VerQueryValueW,
};

static const char *kExportNames[] = {

    "GetFileVersionInfoA",
    "GetFileVersionInfoByHandle",
    "GetFileVersionInfoExA",
    "GetFileVersionInfoExW",
    "GetFileVersionInfoSizeA",
    "GetFileVersionInfoSizeExA",
    "GetFileVersionInfoSizeExW",
    "GetFileVersionInfoSizeW",
    "GetFileVersionInfoW",
    "VerFindFileA",
    "VerFindFileW",
    "VerInstallFileA",
    "VerInstallFileW",
    "VerLanguageNameA",
    "VerLanguageNameW",
    "VerQueryValueA",
    "VerQueryValueW",
};

static bool mem_eq(const unsigned char *a, const unsigned char *b, size_t n) {
    for (size_t i = 0; i < n; i++) if (a[i] != b[i]) return false;
    return true;
}

static bool apply_patch_at(unsigned char *p) {
    DWORD old;
    if (!VirtualProtect(p, PATCH_LEN, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(p, SIG_DONE, PATCH_LEN);
    FlushInstructionCache(GetCurrentProcess(), p, PATCH_LEN);
    VirtualProtect(p, PATCH_LEN, old, &old);
    return true;
}

static DWORD WINAPI patch_thread(LPVOID arg) {
    (void)arg;
    unsigned char *base = (unsigned char *)GetModuleHandleA(NULL);
    if (!base) return 1;
    for (int attempt = 0; attempt < 24; attempt++) {
        Sleep(5000);
        unsigned char *p = base + PATCH_RVA;
        if (mem_eq(p, SIG_DONE, PATCH_LEN)) return 0;
        if (mem_eq(p, SIG_ORIG, PATCH_LEN)) { apply_patch_at(p); return 0; }
        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) continue;
        PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) continue;
        PIMAGE_SECTION_HEADER sec = IMAGE_FIRST_SECTION(nt);
        for (int i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
            if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            unsigned char *s = base + sec->VirtualAddress;
            size_t len = sec->Misc.VirtualSize;
            if (len < PATCH_LEN) continue;
            for (size_t o = 0; o + PATCH_LEN <= len; o++) {
                if (s[o] != SIG_ORIG[0]) continue;
                if (mem_eq(s + o, SIG_ORIG, PATCH_LEN)) { apply_patch_at(s + o); return 0; }
            }
        }
    }
    return 2;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        char sys[MAX_PATH];
        UINT n = GetSystemDirectoryA(sys, MAX_PATH);
        if (n == 0 || n + 12 > MAX_PATH) return FALSE;
        lstrcatA(sys, "\\version.dll");
        g_real = LoadLibraryA(sys);
        if (!g_real) return FALSE;
        for (int i = 0; i < 17; i++)
            *fp_TABLE[i] = GetProcAddress(g_real, kExportNames[i]);
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        const char *fn = path;
        for (const char *q = path; *q; q++) if (*q == '\\' || *q == '/') fn = q + 1;
        if (!lstrcmpiA(fn, "navicat.exe")) {
            HANDLE t = CreateThread(NULL, 0, patch_thread, NULL, 0, NULL);
            if (t) CloseHandle(t);
        }
    } else if (reason == DLL_PROCESS_DETACH && g_real) {
        FreeLibrary(g_real);
    }
    return TRUE;
}
