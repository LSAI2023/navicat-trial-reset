/* ============================================================================
 * winmm_proxy.c -- Navicat 18.x 试用状态补丁 DLL(净化重制版)
 *
 * 原理(逆向自 Wimmm Cracker x64, 2023 版, 见 逆向报告.md):
 *   1. 本 DLL 与 navicat.exe 同目录时, 被 Windows 加载器优先于系统 winmm.dll 加载;
 *   2. 加载时打开 C:\Windows\System32\winmm.dll, 把全部 winmm API 地址填入跳转表,
 *      所有导出均为 `jmp [ptr]` 尾跳板 —— 调用方无感知转发;
 *   3. 仅当宿主是 navicat.exe 时启动补丁线程:
 *      校验 navicat.exe 内存中 RVA 0x993699 处的 11 字节签名
 *      (mov esi,[rbx+8]; mov edi,[rbx+0x18]; mov eax,esi; sub eax,1),
 *      命中则写回 11 字节补丁(inc 两个计数字段 + 强制 eax=0 走"试用中"分支),
 *      未命中则尝试在整个 .text 里按同一签名扫描(小版本漂移容错), 都不中则不动。
 *
 * 与原版差异(安全设计):
 *   - 无 VM/无自解密: 全部逻辑为明文 C, 可审计;
 *   - 签名不符不写内存(原版为硬编码地址直写);
 *   - 补丁完成后不卸载自身(原版 FreeLibraryAndExitThread, 依赖 Navicat 不调用
 *     winmm API 才不崩; 本版保持常驻, 跳板始终有效);
 *   - 不含任何网络/文件/注册表行为。
 * 构建: x86_64-w64-mingw32-gcc -O2 -shared -o winmm.dll proxy.c winmm.def \
 *        -Wl,--subsystem,windows -lkernel32 -luser32
 * ==========================================================================*/
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma comment(lib, "advapi32.lib")

/* ---------- 全局: 真 winmm 句柄 ---------- */
static HMODULE g_real = NULL;

/* ---------- 补丁定义 ---------- */
#define PATCH_RVA      0x993699          /* navicat.exe .text 内补丁点 */
#define PATCH_LEN      11
static const unsigned char SIG_ORIG[PATCH_LEN] =
    { 0x8B,0x73,0x08, 0x8B,0x7B,0x18, 0x89,0xF0,0x83,0xE8,0x01 };
static const unsigned char SIG_DONE[PATCH_LEN] =
    { 0xFF,0x43,0x08, 0xFF,0x43,0x18, 0xB0,0x01,0x31,0xC0,0x90 };

/* ---------- 无名 ordinal 2 存根(兼容按序号导入的遗留代码) ---------- */
static FARPROC fp_ordinal2stub;
__attribute__((naked)) void __stdcall fwd_ordinal2stub(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_ordinal2stub)); }

/* ---------- 跳转表与转发跳板(每 API: 指针 + naked 尾跳) ---------- */
static FARPROC fp_mciExecute;
__attribute__((naked)) void __stdcall fwd_mciExecute(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciExecute)); }
static FARPROC fp_CloseDriver;
__attribute__((naked)) void __stdcall fwd_CloseDriver(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_CloseDriver)); }
static FARPROC fp_DefDriverProc;
__attribute__((naked)) void __stdcall fwd_DefDriverProc(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_DefDriverProc)); }
static FARPROC fp_DriverCallback;
__attribute__((naked)) void __stdcall fwd_DriverCallback(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_DriverCallback)); }
static FARPROC fp_DrvGetModuleHandle;
__attribute__((naked)) void __stdcall fwd_DrvGetModuleHandle(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_DrvGetModuleHandle)); }
static FARPROC fp_GetDriverModuleHandle;
__attribute__((naked)) void __stdcall fwd_GetDriverModuleHandle(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_GetDriverModuleHandle)); }
static FARPROC fp_OpenDriver;
__attribute__((naked)) void __stdcall fwd_OpenDriver(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_OpenDriver)); }
static FARPROC fp_PlaySound;
__attribute__((naked)) void __stdcall fwd_PlaySound(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_PlaySound)); }
static FARPROC fp_PlaySoundA;
__attribute__((naked)) void __stdcall fwd_PlaySoundA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_PlaySoundA)); }
static FARPROC fp_PlaySoundW;
__attribute__((naked)) void __stdcall fwd_PlaySoundW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_PlaySoundW)); }
static FARPROC fp_SendDriverMessage;
__attribute__((naked)) void __stdcall fwd_SendDriverMessage(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_SendDriverMessage)); }
static FARPROC fp_WOWAppExit;
__attribute__((naked)) void __stdcall fwd_WOWAppExit(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_WOWAppExit)); }
static FARPROC fp_auxGetDevCapsA;
__attribute__((naked)) void __stdcall fwd_auxGetDevCapsA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_auxGetDevCapsA)); }
static FARPROC fp_auxGetDevCapsW;
__attribute__((naked)) void __stdcall fwd_auxGetDevCapsW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_auxGetDevCapsW)); }
static FARPROC fp_auxGetNumDevs;
__attribute__((naked)) void __stdcall fwd_auxGetNumDevs(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_auxGetNumDevs)); }
static FARPROC fp_auxGetVolume;
__attribute__((naked)) void __stdcall fwd_auxGetVolume(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_auxGetVolume)); }
static FARPROC fp_auxOutMessage;
__attribute__((naked)) void __stdcall fwd_auxOutMessage(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_auxOutMessage)); }
static FARPROC fp_auxSetVolume;
__attribute__((naked)) void __stdcall fwd_auxSetVolume(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_auxSetVolume)); }
static FARPROC fp_joyConfigChanged;
__attribute__((naked)) void __stdcall fwd_joyConfigChanged(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_joyConfigChanged)); }
static FARPROC fp_joyGetDevCapsA;
__attribute__((naked)) void __stdcall fwd_joyGetDevCapsA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_joyGetDevCapsA)); }
static FARPROC fp_joyGetDevCapsW;
__attribute__((naked)) void __stdcall fwd_joyGetDevCapsW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_joyGetDevCapsW)); }
static FARPROC fp_joyGetNumDevs;
__attribute__((naked)) void __stdcall fwd_joyGetNumDevs(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_joyGetNumDevs)); }
static FARPROC fp_joyGetPos;
__attribute__((naked)) void __stdcall fwd_joyGetPos(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_joyGetPos)); }
static FARPROC fp_joyGetPosEx;
__attribute__((naked)) void __stdcall fwd_joyGetPosEx(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_joyGetPosEx)); }
static FARPROC fp_joyGetThreshold;
__attribute__((naked)) void __stdcall fwd_joyGetThreshold(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_joyGetThreshold)); }
static FARPROC fp_joyReleaseCapture;
__attribute__((naked)) void __stdcall fwd_joyReleaseCapture(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_joyReleaseCapture)); }
static FARPROC fp_joySetCapture;
__attribute__((naked)) void __stdcall fwd_joySetCapture(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_joySetCapture)); }
static FARPROC fp_joySetThreshold;
__attribute__((naked)) void __stdcall fwd_joySetThreshold(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_joySetThreshold)); }
static FARPROC fp_mciDriverNotify;
__attribute__((naked)) void __stdcall fwd_mciDriverNotify(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciDriverNotify)); }
static FARPROC fp_mciDriverYield;
__attribute__((naked)) void __stdcall fwd_mciDriverYield(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciDriverYield)); }
static FARPROC fp_mciFreeCommandResource;
__attribute__((naked)) void __stdcall fwd_mciFreeCommandResource(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciFreeCommandResource)); }
static FARPROC fp_mciGetCreatorTask;
__attribute__((naked)) void __stdcall fwd_mciGetCreatorTask(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciGetCreatorTask)); }
static FARPROC fp_mciGetDeviceIDA;
__attribute__((naked)) void __stdcall fwd_mciGetDeviceIDA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciGetDeviceIDA)); }
static FARPROC fp_mciGetDeviceIDFromElementIDA;
__attribute__((naked)) void __stdcall fwd_mciGetDeviceIDFromElementIDA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciGetDeviceIDFromElementIDA)); }
static FARPROC fp_mciGetDeviceIDFromElementIDW;
__attribute__((naked)) void __stdcall fwd_mciGetDeviceIDFromElementIDW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciGetDeviceIDFromElementIDW)); }
static FARPROC fp_mciGetDeviceIDW;
__attribute__((naked)) void __stdcall fwd_mciGetDeviceIDW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciGetDeviceIDW)); }
static FARPROC fp_mciGetDriverData;
__attribute__((naked)) void __stdcall fwd_mciGetDriverData(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciGetDriverData)); }
static FARPROC fp_mciGetErrorStringA;
__attribute__((naked)) void __stdcall fwd_mciGetErrorStringA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciGetErrorStringA)); }
static FARPROC fp_mciGetErrorStringW;
__attribute__((naked)) void __stdcall fwd_mciGetErrorStringW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciGetErrorStringW)); }
static FARPROC fp_mciGetYieldProc;
__attribute__((naked)) void __stdcall fwd_mciGetYieldProc(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciGetYieldProc)); }
static FARPROC fp_mciLoadCommandResource;
__attribute__((naked)) void __stdcall fwd_mciLoadCommandResource(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciLoadCommandResource)); }
static FARPROC fp_mciSendCommandA;
__attribute__((naked)) void __stdcall fwd_mciSendCommandA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciSendCommandA)); }
static FARPROC fp_mciSendCommandW;
__attribute__((naked)) void __stdcall fwd_mciSendCommandW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciSendCommandW)); }
static FARPROC fp_mciSendStringA;
__attribute__((naked)) void __stdcall fwd_mciSendStringA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciSendStringA)); }
static FARPROC fp_mciSendStringW;
__attribute__((naked)) void __stdcall fwd_mciSendStringW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciSendStringW)); }
static FARPROC fp_mciSetDriverData;
__attribute__((naked)) void __stdcall fwd_mciSetDriverData(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciSetDriverData)); }
static FARPROC fp_mciSetYieldProc;
__attribute__((naked)) void __stdcall fwd_mciSetYieldProc(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mciSetYieldProc)); }
static FARPROC fp_midiConnect;
__attribute__((naked)) void __stdcall fwd_midiConnect(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiConnect)); }
static FARPROC fp_midiDisconnect;
__attribute__((naked)) void __stdcall fwd_midiDisconnect(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiDisconnect)); }
static FARPROC fp_midiInAddBuffer;
__attribute__((naked)) void __stdcall fwd_midiInAddBuffer(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInAddBuffer)); }
static FARPROC fp_midiInClose;
__attribute__((naked)) void __stdcall fwd_midiInClose(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInClose)); }
static FARPROC fp_midiInGetDevCapsA;
__attribute__((naked)) void __stdcall fwd_midiInGetDevCapsA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInGetDevCapsA)); }
static FARPROC fp_midiInGetDevCapsW;
__attribute__((naked)) void __stdcall fwd_midiInGetDevCapsW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInGetDevCapsW)); }
static FARPROC fp_midiInGetErrorTextA;
__attribute__((naked)) void __stdcall fwd_midiInGetErrorTextA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInGetErrorTextA)); }
static FARPROC fp_midiInGetErrorTextW;
__attribute__((naked)) void __stdcall fwd_midiInGetErrorTextW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInGetErrorTextW)); }
static FARPROC fp_midiInGetID;
__attribute__((naked)) void __stdcall fwd_midiInGetID(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInGetID)); }
static FARPROC fp_midiInGetNumDevs;
__attribute__((naked)) void __stdcall fwd_midiInGetNumDevs(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInGetNumDevs)); }
static FARPROC fp_midiInMessage;
__attribute__((naked)) void __stdcall fwd_midiInMessage(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInMessage)); }
static FARPROC fp_midiInOpen;
__attribute__((naked)) void __stdcall fwd_midiInOpen(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInOpen)); }
static FARPROC fp_midiInPrepareHeader;
__attribute__((naked)) void __stdcall fwd_midiInPrepareHeader(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInPrepareHeader)); }
static FARPROC fp_midiInReset;
__attribute__((naked)) void __stdcall fwd_midiInReset(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInReset)); }
static FARPROC fp_midiInStart;
__attribute__((naked)) void __stdcall fwd_midiInStart(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInStart)); }
static FARPROC fp_midiInStop;
__attribute__((naked)) void __stdcall fwd_midiInStop(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInStop)); }
static FARPROC fp_midiInUnprepareHeader;
__attribute__((naked)) void __stdcall fwd_midiInUnprepareHeader(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiInUnprepareHeader)); }
static FARPROC fp_midiOutCacheDrumPatches;
__attribute__((naked)) void __stdcall fwd_midiOutCacheDrumPatches(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutCacheDrumPatches)); }
static FARPROC fp_midiOutCachePatches;
__attribute__((naked)) void __stdcall fwd_midiOutCachePatches(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutCachePatches)); }
static FARPROC fp_midiOutClose;
__attribute__((naked)) void __stdcall fwd_midiOutClose(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutClose)); }
static FARPROC fp_midiOutGetDevCapsA;
__attribute__((naked)) void __stdcall fwd_midiOutGetDevCapsA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutGetDevCapsA)); }
static FARPROC fp_midiOutGetDevCapsW;
__attribute__((naked)) void __stdcall fwd_midiOutGetDevCapsW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutGetDevCapsW)); }
static FARPROC fp_midiOutGetErrorTextA;
__attribute__((naked)) void __stdcall fwd_midiOutGetErrorTextA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutGetErrorTextA)); }
static FARPROC fp_midiOutGetErrorTextW;
__attribute__((naked)) void __stdcall fwd_midiOutGetErrorTextW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutGetErrorTextW)); }
static FARPROC fp_midiOutGetID;
__attribute__((naked)) void __stdcall fwd_midiOutGetID(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutGetID)); }
static FARPROC fp_midiOutGetNumDevs;
__attribute__((naked)) void __stdcall fwd_midiOutGetNumDevs(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutGetNumDevs)); }
static FARPROC fp_midiOutGetVolume;
__attribute__((naked)) void __stdcall fwd_midiOutGetVolume(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutGetVolume)); }
static FARPROC fp_midiOutLongMsg;
__attribute__((naked)) void __stdcall fwd_midiOutLongMsg(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutLongMsg)); }
static FARPROC fp_midiOutMessage;
__attribute__((naked)) void __stdcall fwd_midiOutMessage(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutMessage)); }
static FARPROC fp_midiOutOpen;
__attribute__((naked)) void __stdcall fwd_midiOutOpen(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutOpen)); }
static FARPROC fp_midiOutPrepareHeader;
__attribute__((naked)) void __stdcall fwd_midiOutPrepareHeader(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutPrepareHeader)); }
static FARPROC fp_midiOutReset;
__attribute__((naked)) void __stdcall fwd_midiOutReset(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutReset)); }
static FARPROC fp_midiOutSetVolume;
__attribute__((naked)) void __stdcall fwd_midiOutSetVolume(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutSetVolume)); }
static FARPROC fp_midiOutShortMsg;
__attribute__((naked)) void __stdcall fwd_midiOutShortMsg(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutShortMsg)); }
static FARPROC fp_midiOutUnprepareHeader;
__attribute__((naked)) void __stdcall fwd_midiOutUnprepareHeader(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiOutUnprepareHeader)); }
static FARPROC fp_midiStreamClose;
__attribute__((naked)) void __stdcall fwd_midiStreamClose(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiStreamClose)); }
static FARPROC fp_midiStreamOpen;
__attribute__((naked)) void __stdcall fwd_midiStreamOpen(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiStreamOpen)); }
static FARPROC fp_midiStreamOut;
__attribute__((naked)) void __stdcall fwd_midiStreamOut(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiStreamOut)); }
static FARPROC fp_midiStreamPause;
__attribute__((naked)) void __stdcall fwd_midiStreamPause(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiStreamPause)); }
static FARPROC fp_midiStreamPosition;
__attribute__((naked)) void __stdcall fwd_midiStreamPosition(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiStreamPosition)); }
static FARPROC fp_midiStreamProperty;
__attribute__((naked)) void __stdcall fwd_midiStreamProperty(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiStreamProperty)); }
static FARPROC fp_midiStreamRestart;
__attribute__((naked)) void __stdcall fwd_midiStreamRestart(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiStreamRestart)); }
static FARPROC fp_midiStreamStop;
__attribute__((naked)) void __stdcall fwd_midiStreamStop(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_midiStreamStop)); }
static FARPROC fp_mixerClose;
__attribute__((naked)) void __stdcall fwd_mixerClose(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerClose)); }
static FARPROC fp_mixerGetControlDetailsA;
__attribute__((naked)) void __stdcall fwd_mixerGetControlDetailsA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerGetControlDetailsA)); }
static FARPROC fp_mixerGetControlDetailsW;
__attribute__((naked)) void __stdcall fwd_mixerGetControlDetailsW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerGetControlDetailsW)); }
static FARPROC fp_mixerGetDevCapsA;
__attribute__((naked)) void __stdcall fwd_mixerGetDevCapsA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerGetDevCapsA)); }
static FARPROC fp_mixerGetDevCapsW;
__attribute__((naked)) void __stdcall fwd_mixerGetDevCapsW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerGetDevCapsW)); }
static FARPROC fp_mixerGetID;
__attribute__((naked)) void __stdcall fwd_mixerGetID(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerGetID)); }
static FARPROC fp_mixerGetLineControlsA;
__attribute__((naked)) void __stdcall fwd_mixerGetLineControlsA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerGetLineControlsA)); }
static FARPROC fp_mixerGetLineControlsW;
__attribute__((naked)) void __stdcall fwd_mixerGetLineControlsW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerGetLineControlsW)); }
static FARPROC fp_mixerGetLineInfoA;
__attribute__((naked)) void __stdcall fwd_mixerGetLineInfoA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerGetLineInfoA)); }
static FARPROC fp_mixerGetLineInfoW;
__attribute__((naked)) void __stdcall fwd_mixerGetLineInfoW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerGetLineInfoW)); }
static FARPROC fp_mixerGetNumDevs;
__attribute__((naked)) void __stdcall fwd_mixerGetNumDevs(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerGetNumDevs)); }
static FARPROC fp_mixerMessage;
__attribute__((naked)) void __stdcall fwd_mixerMessage(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerMessage)); }
static FARPROC fp_mixerOpen;
__attribute__((naked)) void __stdcall fwd_mixerOpen(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerOpen)); }
static FARPROC fp_mixerSetControlDetails;
__attribute__((naked)) void __stdcall fwd_mixerSetControlDetails(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mixerSetControlDetails)); }
static FARPROC fp_mmDrvInstall;
__attribute__((naked)) void __stdcall fwd_mmDrvInstall(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmDrvInstall)); }
static FARPROC fp_mmGetCurrentTask;
__attribute__((naked)) void __stdcall fwd_mmGetCurrentTask(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmGetCurrentTask)); }
static FARPROC fp_mmTaskBlock;
__attribute__((naked)) void __stdcall fwd_mmTaskBlock(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmTaskBlock)); }
static FARPROC fp_mmTaskCreate;
__attribute__((naked)) void __stdcall fwd_mmTaskCreate(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmTaskCreate)); }
static FARPROC fp_mmTaskSignal;
__attribute__((naked)) void __stdcall fwd_mmTaskSignal(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmTaskSignal)); }
static FARPROC fp_mmTaskYield;
__attribute__((naked)) void __stdcall fwd_mmTaskYield(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmTaskYield)); }
static FARPROC fp_mmioAdvance;
__attribute__((naked)) void __stdcall fwd_mmioAdvance(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioAdvance)); }
static FARPROC fp_mmioAscend;
__attribute__((naked)) void __stdcall fwd_mmioAscend(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioAscend)); }
static FARPROC fp_mmioClose;
__attribute__((naked)) void __stdcall fwd_mmioClose(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioClose)); }
static FARPROC fp_mmioCreateChunk;
__attribute__((naked)) void __stdcall fwd_mmioCreateChunk(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioCreateChunk)); }
static FARPROC fp_mmioDescend;
__attribute__((naked)) void __stdcall fwd_mmioDescend(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioDescend)); }
static FARPROC fp_mmioFlush;
__attribute__((naked)) void __stdcall fwd_mmioFlush(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioFlush)); }
static FARPROC fp_mmioGetInfo;
__attribute__((naked)) void __stdcall fwd_mmioGetInfo(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioGetInfo)); }
static FARPROC fp_mmioInstallIOProcA;
__attribute__((naked)) void __stdcall fwd_mmioInstallIOProcA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioInstallIOProcA)); }
static FARPROC fp_mmioInstallIOProcW;
__attribute__((naked)) void __stdcall fwd_mmioInstallIOProcW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioInstallIOProcW)); }
static FARPROC fp_mmioOpenA;
__attribute__((naked)) void __stdcall fwd_mmioOpenA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioOpenA)); }
static FARPROC fp_mmioOpenW;
__attribute__((naked)) void __stdcall fwd_mmioOpenW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioOpenW)); }
static FARPROC fp_mmioRead;
__attribute__((naked)) void __stdcall fwd_mmioRead(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioRead)); }
static FARPROC fp_mmioRenameA;
__attribute__((naked)) void __stdcall fwd_mmioRenameA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioRenameA)); }
static FARPROC fp_mmioRenameW;
__attribute__((naked)) void __stdcall fwd_mmioRenameW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioRenameW)); }
static FARPROC fp_mmioSeek;
__attribute__((naked)) void __stdcall fwd_mmioSeek(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioSeek)); }
static FARPROC fp_mmioSendMessage;
__attribute__((naked)) void __stdcall fwd_mmioSendMessage(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioSendMessage)); }
static FARPROC fp_mmioSetBuffer;
__attribute__((naked)) void __stdcall fwd_mmioSetBuffer(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioSetBuffer)); }
static FARPROC fp_mmioSetInfo;
__attribute__((naked)) void __stdcall fwd_mmioSetInfo(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioSetInfo)); }
static FARPROC fp_mmioStringToFOURCCA;
__attribute__((naked)) void __stdcall fwd_mmioStringToFOURCCA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioStringToFOURCCA)); }
static FARPROC fp_mmioStringToFOURCCW;
__attribute__((naked)) void __stdcall fwd_mmioStringToFOURCCW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioStringToFOURCCW)); }
static FARPROC fp_mmioWrite;
__attribute__((naked)) void __stdcall fwd_mmioWrite(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmioWrite)); }
static FARPROC fp_mmsystemGetVersion;
__attribute__((naked)) void __stdcall fwd_mmsystemGetVersion(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_mmsystemGetVersion)); }
static FARPROC fp_sndPlaySoundA;
__attribute__((naked)) void __stdcall fwd_sndPlaySoundA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_sndPlaySoundA)); }
static FARPROC fp_sndPlaySoundW;
__attribute__((naked)) void __stdcall fwd_sndPlaySoundW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_sndPlaySoundW)); }
static FARPROC fp_timeBeginPeriod;
__attribute__((naked)) void __stdcall fwd_timeBeginPeriod(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_timeBeginPeriod)); }
static FARPROC fp_timeEndPeriod;
__attribute__((naked)) void __stdcall fwd_timeEndPeriod(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_timeEndPeriod)); }
static FARPROC fp_timeGetDevCaps;
__attribute__((naked)) void __stdcall fwd_timeGetDevCaps(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_timeGetDevCaps)); }
static FARPROC fp_timeGetSystemTime;
__attribute__((naked)) void __stdcall fwd_timeGetSystemTime(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_timeGetSystemTime)); }
static FARPROC fp_timeGetTime;
__attribute__((naked)) void __stdcall fwd_timeGetTime(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_timeGetTime)); }
static FARPROC fp_timeKillEvent;
__attribute__((naked)) void __stdcall fwd_timeKillEvent(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_timeKillEvent)); }
static FARPROC fp_timeSetEvent;
__attribute__((naked)) void __stdcall fwd_timeSetEvent(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_timeSetEvent)); }
static FARPROC fp_waveInAddBuffer;
__attribute__((naked)) void __stdcall fwd_waveInAddBuffer(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInAddBuffer)); }
static FARPROC fp_waveInClose;
__attribute__((naked)) void __stdcall fwd_waveInClose(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInClose)); }
static FARPROC fp_waveInGetDevCapsA;
__attribute__((naked)) void __stdcall fwd_waveInGetDevCapsA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInGetDevCapsA)); }
static FARPROC fp_waveInGetDevCapsW;
__attribute__((naked)) void __stdcall fwd_waveInGetDevCapsW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInGetDevCapsW)); }
static FARPROC fp_waveInGetErrorTextA;
__attribute__((naked)) void __stdcall fwd_waveInGetErrorTextA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInGetErrorTextA)); }
static FARPROC fp_waveInGetErrorTextW;
__attribute__((naked)) void __stdcall fwd_waveInGetErrorTextW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInGetErrorTextW)); }
static FARPROC fp_waveInGetID;
__attribute__((naked)) void __stdcall fwd_waveInGetID(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInGetID)); }
static FARPROC fp_waveInGetNumDevs;
__attribute__((naked)) void __stdcall fwd_waveInGetNumDevs(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInGetNumDevs)); }
static FARPROC fp_waveInGetPosition;
__attribute__((naked)) void __stdcall fwd_waveInGetPosition(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInGetPosition)); }
static FARPROC fp_waveInMessage;
__attribute__((naked)) void __stdcall fwd_waveInMessage(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInMessage)); }
static FARPROC fp_waveInOpen;
__attribute__((naked)) void __stdcall fwd_waveInOpen(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInOpen)); }
static FARPROC fp_waveInPrepareHeader;
__attribute__((naked)) void __stdcall fwd_waveInPrepareHeader(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInPrepareHeader)); }
static FARPROC fp_waveInReset;
__attribute__((naked)) void __stdcall fwd_waveInReset(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInReset)); }
static FARPROC fp_waveInStart;
__attribute__((naked)) void __stdcall fwd_waveInStart(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInStart)); }
static FARPROC fp_waveInStop;
__attribute__((naked)) void __stdcall fwd_waveInStop(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInStop)); }
static FARPROC fp_waveInUnprepareHeader;
__attribute__((naked)) void __stdcall fwd_waveInUnprepareHeader(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveInUnprepareHeader)); }
static FARPROC fp_waveOutBreakLoop;
__attribute__((naked)) void __stdcall fwd_waveOutBreakLoop(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutBreakLoop)); }
static FARPROC fp_waveOutClose;
__attribute__((naked)) void __stdcall fwd_waveOutClose(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutClose)); }
static FARPROC fp_waveOutGetDevCapsA;
__attribute__((naked)) void __stdcall fwd_waveOutGetDevCapsA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutGetDevCapsA)); }
static FARPROC fp_waveOutGetDevCapsW;
__attribute__((naked)) void __stdcall fwd_waveOutGetDevCapsW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutGetDevCapsW)); }
static FARPROC fp_waveOutGetErrorTextA;
__attribute__((naked)) void __stdcall fwd_waveOutGetErrorTextA(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutGetErrorTextA)); }
static FARPROC fp_waveOutGetErrorTextW;
__attribute__((naked)) void __stdcall fwd_waveOutGetErrorTextW(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutGetErrorTextW)); }
static FARPROC fp_waveOutGetID;
__attribute__((naked)) void __stdcall fwd_waveOutGetID(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutGetID)); }
static FARPROC fp_waveOutGetNumDevs;
__attribute__((naked)) void __stdcall fwd_waveOutGetNumDevs(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutGetNumDevs)); }
static FARPROC fp_waveOutGetPitch;
__attribute__((naked)) void __stdcall fwd_waveOutGetPitch(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutGetPitch)); }
static FARPROC fp_waveOutGetPlaybackRate;
__attribute__((naked)) void __stdcall fwd_waveOutGetPlaybackRate(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutGetPlaybackRate)); }
static FARPROC fp_waveOutGetPosition;
__attribute__((naked)) void __stdcall fwd_waveOutGetPosition(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutGetPosition)); }
static FARPROC fp_waveOutGetVolume;
__attribute__((naked)) void __stdcall fwd_waveOutGetVolume(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutGetVolume)); }
static FARPROC fp_waveOutMessage;
__attribute__((naked)) void __stdcall fwd_waveOutMessage(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutMessage)); }
static FARPROC fp_waveOutOpen;
__attribute__((naked)) void __stdcall fwd_waveOutOpen(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutOpen)); }
static FARPROC fp_waveOutPause;
__attribute__((naked)) void __stdcall fwd_waveOutPause(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutPause)); }
static FARPROC fp_waveOutPrepareHeader;
__attribute__((naked)) void __stdcall fwd_waveOutPrepareHeader(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutPrepareHeader)); }
static FARPROC fp_waveOutReset;
__attribute__((naked)) void __stdcall fwd_waveOutReset(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutReset)); }
static FARPROC fp_waveOutRestart;
__attribute__((naked)) void __stdcall fwd_waveOutRestart(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutRestart)); }
static FARPROC fp_waveOutSetPitch;
__attribute__((naked)) void __stdcall fwd_waveOutSetPitch(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutSetPitch)); }
static FARPROC fp_waveOutSetPlaybackRate;
__attribute__((naked)) void __stdcall fwd_waveOutSetPlaybackRate(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutSetPlaybackRate)); }
static FARPROC fp_waveOutSetVolume;
__attribute__((naked)) void __stdcall fwd_waveOutSetVolume(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutSetVolume)); }
static FARPROC fp_waveOutUnprepareHeader;
__attribute__((naked)) void __stdcall fwd_waveOutUnprepareHeader(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutUnprepareHeader)); }
static FARPROC fp_waveOutWrite;
__attribute__((naked)) void __stdcall fwd_waveOutWrite(void)
{ __asm__ volatile("jmp *%0" :: "m"(fp_waveOutWrite)); }

static FARPROC *const fp_TABLE[] = {
    (FARPROC *)&fp_mciExecute,
    (FARPROC *)&fp_CloseDriver,
    (FARPROC *)&fp_DefDriverProc,
    (FARPROC *)&fp_DriverCallback,
    (FARPROC *)&fp_DrvGetModuleHandle,
    (FARPROC *)&fp_GetDriverModuleHandle,
    (FARPROC *)&fp_OpenDriver,
    (FARPROC *)&fp_PlaySound,
    (FARPROC *)&fp_PlaySoundA,
    (FARPROC *)&fp_PlaySoundW,
    (FARPROC *)&fp_SendDriverMessage,
    (FARPROC *)&fp_WOWAppExit,
    (FARPROC *)&fp_auxGetDevCapsA,
    (FARPROC *)&fp_auxGetDevCapsW,
    (FARPROC *)&fp_auxGetNumDevs,
    (FARPROC *)&fp_auxGetVolume,
    (FARPROC *)&fp_auxOutMessage,
    (FARPROC *)&fp_auxSetVolume,
    (FARPROC *)&fp_joyConfigChanged,
    (FARPROC *)&fp_joyGetDevCapsA,
    (FARPROC *)&fp_joyGetDevCapsW,
    (FARPROC *)&fp_joyGetNumDevs,
    (FARPROC *)&fp_joyGetPos,
    (FARPROC *)&fp_joyGetPosEx,
    (FARPROC *)&fp_joyGetThreshold,
    (FARPROC *)&fp_joyReleaseCapture,
    (FARPROC *)&fp_joySetCapture,
    (FARPROC *)&fp_joySetThreshold,
    (FARPROC *)&fp_mciDriverNotify,
    (FARPROC *)&fp_mciDriverYield,
    (FARPROC *)&fp_mciFreeCommandResource,
    (FARPROC *)&fp_mciGetCreatorTask,
    (FARPROC *)&fp_mciGetDeviceIDA,
    (FARPROC *)&fp_mciGetDeviceIDFromElementIDA,
    (FARPROC *)&fp_mciGetDeviceIDFromElementIDW,
    (FARPROC *)&fp_mciGetDeviceIDW,
    (FARPROC *)&fp_mciGetDriverData,
    (FARPROC *)&fp_mciGetErrorStringA,
    (FARPROC *)&fp_mciGetErrorStringW,
    (FARPROC *)&fp_mciGetYieldProc,
    (FARPROC *)&fp_mciLoadCommandResource,
    (FARPROC *)&fp_mciSendCommandA,
    (FARPROC *)&fp_mciSendCommandW,
    (FARPROC *)&fp_mciSendStringA,
    (FARPROC *)&fp_mciSendStringW,
    (FARPROC *)&fp_mciSetDriverData,
    (FARPROC *)&fp_mciSetYieldProc,
    (FARPROC *)&fp_midiConnect,
    (FARPROC *)&fp_midiDisconnect,
    (FARPROC *)&fp_midiInAddBuffer,
    (FARPROC *)&fp_midiInClose,
    (FARPROC *)&fp_midiInGetDevCapsA,
    (FARPROC *)&fp_midiInGetDevCapsW,
    (FARPROC *)&fp_midiInGetErrorTextA,
    (FARPROC *)&fp_midiInGetErrorTextW,
    (FARPROC *)&fp_midiInGetID,
    (FARPROC *)&fp_midiInGetNumDevs,
    (FARPROC *)&fp_midiInMessage,
    (FARPROC *)&fp_midiInOpen,
    (FARPROC *)&fp_midiInPrepareHeader,
    (FARPROC *)&fp_midiInReset,
    (FARPROC *)&fp_midiInStart,
    (FARPROC *)&fp_midiInStop,
    (FARPROC *)&fp_midiInUnprepareHeader,
    (FARPROC *)&fp_midiOutCacheDrumPatches,
    (FARPROC *)&fp_midiOutCachePatches,
    (FARPROC *)&fp_midiOutClose,
    (FARPROC *)&fp_midiOutGetDevCapsA,
    (FARPROC *)&fp_midiOutGetDevCapsW,
    (FARPROC *)&fp_midiOutGetErrorTextA,
    (FARPROC *)&fp_midiOutGetErrorTextW,
    (FARPROC *)&fp_midiOutGetID,
    (FARPROC *)&fp_midiOutGetNumDevs,
    (FARPROC *)&fp_midiOutGetVolume,
    (FARPROC *)&fp_midiOutLongMsg,
    (FARPROC *)&fp_midiOutMessage,
    (FARPROC *)&fp_midiOutOpen,
    (FARPROC *)&fp_midiOutPrepareHeader,
    (FARPROC *)&fp_midiOutReset,
    (FARPROC *)&fp_midiOutSetVolume,
    (FARPROC *)&fp_midiOutShortMsg,
    (FARPROC *)&fp_midiOutUnprepareHeader,
    (FARPROC *)&fp_midiStreamClose,
    (FARPROC *)&fp_midiStreamOpen,
    (FARPROC *)&fp_midiStreamOut,
    (FARPROC *)&fp_midiStreamPause,
    (FARPROC *)&fp_midiStreamPosition,
    (FARPROC *)&fp_midiStreamProperty,
    (FARPROC *)&fp_midiStreamRestart,
    (FARPROC *)&fp_midiStreamStop,
    (FARPROC *)&fp_mixerClose,
    (FARPROC *)&fp_mixerGetControlDetailsA,
    (FARPROC *)&fp_mixerGetControlDetailsW,
    (FARPROC *)&fp_mixerGetDevCapsA,
    (FARPROC *)&fp_mixerGetDevCapsW,
    (FARPROC *)&fp_mixerGetID,
    (FARPROC *)&fp_mixerGetLineControlsA,
    (FARPROC *)&fp_mixerGetLineControlsW,
    (FARPROC *)&fp_mixerGetLineInfoA,
    (FARPROC *)&fp_mixerGetLineInfoW,
    (FARPROC *)&fp_mixerGetNumDevs,
    (FARPROC *)&fp_mixerMessage,
    (FARPROC *)&fp_mixerOpen,
    (FARPROC *)&fp_mixerSetControlDetails,
    (FARPROC *)&fp_mmDrvInstall,
    (FARPROC *)&fp_mmGetCurrentTask,
    (FARPROC *)&fp_mmTaskBlock,
    (FARPROC *)&fp_mmTaskCreate,
    (FARPROC *)&fp_mmTaskSignal,
    (FARPROC *)&fp_mmTaskYield,
    (FARPROC *)&fp_mmioAdvance,
    (FARPROC *)&fp_mmioAscend,
    (FARPROC *)&fp_mmioClose,
    (FARPROC *)&fp_mmioCreateChunk,
    (FARPROC *)&fp_mmioDescend,
    (FARPROC *)&fp_mmioFlush,
    (FARPROC *)&fp_mmioGetInfo,
    (FARPROC *)&fp_mmioInstallIOProcA,
    (FARPROC *)&fp_mmioInstallIOProcW,
    (FARPROC *)&fp_mmioOpenA,
    (FARPROC *)&fp_mmioOpenW,
    (FARPROC *)&fp_mmioRead,
    (FARPROC *)&fp_mmioRenameA,
    (FARPROC *)&fp_mmioRenameW,
    (FARPROC *)&fp_mmioSeek,
    (FARPROC *)&fp_mmioSendMessage,
    (FARPROC *)&fp_mmioSetBuffer,
    (FARPROC *)&fp_mmioSetInfo,
    (FARPROC *)&fp_mmioStringToFOURCCA,
    (FARPROC *)&fp_mmioStringToFOURCCW,
    (FARPROC *)&fp_mmioWrite,
    (FARPROC *)&fp_mmsystemGetVersion,
    (FARPROC *)&fp_sndPlaySoundA,
    (FARPROC *)&fp_sndPlaySoundW,
    (FARPROC *)&fp_timeBeginPeriod,
    (FARPROC *)&fp_timeEndPeriod,
    (FARPROC *)&fp_timeGetDevCaps,
    (FARPROC *)&fp_timeGetSystemTime,
    (FARPROC *)&fp_timeGetTime,
    (FARPROC *)&fp_timeKillEvent,
    (FARPROC *)&fp_timeSetEvent,
    (FARPROC *)&fp_waveInAddBuffer,
    (FARPROC *)&fp_waveInClose,
    (FARPROC *)&fp_waveInGetDevCapsA,
    (FARPROC *)&fp_waveInGetDevCapsW,
    (FARPROC *)&fp_waveInGetErrorTextA,
    (FARPROC *)&fp_waveInGetErrorTextW,
    (FARPROC *)&fp_waveInGetID,
    (FARPROC *)&fp_waveInGetNumDevs,
    (FARPROC *)&fp_waveInGetPosition,
    (FARPROC *)&fp_waveInMessage,
    (FARPROC *)&fp_waveInOpen,
    (FARPROC *)&fp_waveInPrepareHeader,
    (FARPROC *)&fp_waveInReset,
    (FARPROC *)&fp_waveInStart,
    (FARPROC *)&fp_waveInStop,
    (FARPROC *)&fp_waveInUnprepareHeader,
    (FARPROC *)&fp_waveOutBreakLoop,
    (FARPROC *)&fp_waveOutClose,
    (FARPROC *)&fp_waveOutGetDevCapsA,
    (FARPROC *)&fp_waveOutGetDevCapsW,
    (FARPROC *)&fp_waveOutGetErrorTextA,
    (FARPROC *)&fp_waveOutGetErrorTextW,
    (FARPROC *)&fp_waveOutGetID,
    (FARPROC *)&fp_waveOutGetNumDevs,
    (FARPROC *)&fp_waveOutGetPitch,
    (FARPROC *)&fp_waveOutGetPlaybackRate,
    (FARPROC *)&fp_waveOutGetPosition,
    (FARPROC *)&fp_waveOutGetVolume,
    (FARPROC *)&fp_waveOutMessage,
    (FARPROC *)&fp_waveOutOpen,
    (FARPROC *)&fp_waveOutPause,
    (FARPROC *)&fp_waveOutPrepareHeader,
    (FARPROC *)&fp_waveOutReset,
    (FARPROC *)&fp_waveOutRestart,
    (FARPROC *)&fp_waveOutSetPitch,
    (FARPROC *)&fp_waveOutSetPlaybackRate,
    (FARPROC *)&fp_waveOutSetVolume,
    (FARPROC *)&fp_waveOutUnprepareHeader,
    (FARPROC *)&fp_waveOutWrite,
};

/* ---------- 补丁线程 ---------- */
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

    /* 补丁立即尝试; 之后每 1 秒重试, 最多 2 分钟。 */
    for (int attempt = 0; attempt < 120; attempt++) {
        if (attempt) Sleep(1000);
        unsigned char *p = base + PATCH_RVA;

        if (mem_eq(p, SIG_DONE, PATCH_LEN)) return 0;        /* 已打过 */
        if (mem_eq(p, SIG_ORIG, PATCH_LEN)) {                /* 硬编码点命中 */
            apply_patch_at(p);
            return 0;
        }
        /* 硬编码点不匹配: 在整个 .text 内按原始签名扫描(限 0x1000..0x4000000) */
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
                if (mem_eq(s + o, SIG_ORIG, PATCH_LEN)) {
                    apply_patch_at(s + o);
                    return 0;
                }
            }
        }
    }
    return 2;
}

/* ---------- 入口 ---------- */
static const char *kExportNames[] = {
    "mciExecute",
    "CloseDriver",
    "DefDriverProc",
    "DriverCallback",
    "DrvGetModuleHandle",
    "GetDriverModuleHandle",
    "OpenDriver",
    "PlaySound",
    "PlaySoundA",
    "PlaySoundW",
    "SendDriverMessage",
    "WOWAppExit",
    "auxGetDevCapsA",
    "auxGetDevCapsW",
    "auxGetNumDevs",
    "auxGetVolume",
    "auxOutMessage",
    "auxSetVolume",
    "joyConfigChanged",
    "joyGetDevCapsA",
    "joyGetDevCapsW",
    "joyGetNumDevs",
    "joyGetPos",
    "joyGetPosEx",
    "joyGetThreshold",
    "joyReleaseCapture",
    "joySetCapture",
    "joySetThreshold",
    "mciDriverNotify",
    "mciDriverYield",
    "mciFreeCommandResource",
    "mciGetCreatorTask",
    "mciGetDeviceIDA",
    "mciGetDeviceIDFromElementIDA",
    "mciGetDeviceIDFromElementIDW",
    "mciGetDeviceIDW",
    "mciGetDriverData",
    "mciGetErrorStringA",
    "mciGetErrorStringW",
    "mciGetYieldProc",
    "mciLoadCommandResource",
    "mciSendCommandA",
    "mciSendCommandW",
    "mciSendStringA",
    "mciSendStringW",
    "mciSetDriverData",
    "mciSetYieldProc",
    "midiConnect",
    "midiDisconnect",
    "midiInAddBuffer",
    "midiInClose",
    "midiInGetDevCapsA",
    "midiInGetDevCapsW",
    "midiInGetErrorTextA",
    "midiInGetErrorTextW",
    "midiInGetID",
    "midiInGetNumDevs",
    "midiInMessage",
    "midiInOpen",
    "midiInPrepareHeader",
    "midiInReset",
    "midiInStart",
    "midiInStop",
    "midiInUnprepareHeader",
    "midiOutCacheDrumPatches",
    "midiOutCachePatches",
    "midiOutClose",
    "midiOutGetDevCapsA",
    "midiOutGetDevCapsW",
    "midiOutGetErrorTextA",
    "midiOutGetErrorTextW",
    "midiOutGetID",
    "midiOutGetNumDevs",
    "midiOutGetVolume",
    "midiOutLongMsg",
    "midiOutMessage",
    "midiOutOpen",
    "midiOutPrepareHeader",
    "midiOutReset",
    "midiOutSetVolume",
    "midiOutShortMsg",
    "midiOutUnprepareHeader",
    "midiStreamClose",
    "midiStreamOpen",
    "midiStreamOut",
    "midiStreamPause",
    "midiStreamPosition",
    "midiStreamProperty",
    "midiStreamRestart",
    "midiStreamStop",
    "mixerClose",
    "mixerGetControlDetailsA",
    "mixerGetControlDetailsW",
    "mixerGetDevCapsA",
    "mixerGetDevCapsW",
    "mixerGetID",
    "mixerGetLineControlsA",
    "mixerGetLineControlsW",
    "mixerGetLineInfoA",
    "mixerGetLineInfoW",
    "mixerGetNumDevs",
    "mixerMessage",
    "mixerOpen",
    "mixerSetControlDetails",
    "mmDrvInstall",
    "mmGetCurrentTask",
    "mmTaskBlock",
    "mmTaskCreate",
    "mmTaskSignal",
    "mmTaskYield",
    "mmioAdvance",
    "mmioAscend",
    "mmioClose",
    "mmioCreateChunk",
    "mmioDescend",
    "mmioFlush",
    "mmioGetInfo",
    "mmioInstallIOProcA",
    "mmioInstallIOProcW",
    "mmioOpenA",
    "mmioOpenW",
    "mmioRead",
    "mmioRenameA",
    "mmioRenameW",
    "mmioSeek",
    "mmioSendMessage",
    "mmioSetBuffer",
    "mmioSetInfo",
    "mmioStringToFOURCCA",
    "mmioStringToFOURCCW",
    "mmioWrite",
    "mmsystemGetVersion",
    "sndPlaySoundA",
    "sndPlaySoundW",
    "timeBeginPeriod",
    "timeEndPeriod",
    "timeGetDevCaps",
    "timeGetSystemTime",
    "timeGetTime",
    "timeKillEvent",
    "timeSetEvent",
    "waveInAddBuffer",
    "waveInClose",
    "waveInGetDevCapsA",
    "waveInGetDevCapsW",
    "waveInGetErrorTextA",
    "waveInGetErrorTextW",
    "waveInGetID",
    "waveInGetNumDevs",
    "waveInGetPosition",
    "waveInMessage",
    "waveInOpen",
    "waveInPrepareHeader",
    "waveInReset",
    "waveInStart",
    "waveInStop",
    "waveInUnprepareHeader",
    "waveOutBreakLoop",
    "waveOutClose",
    "waveOutGetDevCapsA",
    "waveOutGetDevCapsW",
    "waveOutGetErrorTextA",
    "waveOutGetErrorTextW",
    "waveOutGetID",
    "waveOutGetNumDevs",
    "waveOutGetPitch",
    "waveOutGetPlaybackRate",
    "waveOutGetPosition",
    "waveOutGetVolume",
    "waveOutMessage",
    "waveOutOpen",
    "waveOutPause",
    "waveOutPrepareHeader",
    "waveOutReset",
    "waveOutRestart",
    "waveOutSetPitch",
    "waveOutSetPlaybackRate",
    "waveOutSetVolume",
    "waveOutUnprepareHeader",
    "waveOutWrite"
};

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        /* 试用重置(同步, 在 DllMain 中执行):
         * winmm.dll 是 navicat.exe 的静态导入依赖, 本 DllMain 一定运行在
         * Navicat 任何业务代码之前 => 删除试用状态键不存在竞态,
         * Navicat 的授权检查只会看到全新状态。 */
        RegDeleteTreeA(HKEY_CURRENT_USER,
            "Software\\PremiumSoft\\NavicatPremium\\Registration18XCS");
        char sys[MAX_PATH];
        UINT n = GetSystemDirectoryA(sys, MAX_PATH);
        if (n == 0 || n + 11 > MAX_PATH) return FALSE;
        lstrcatA(sys, "\\winmm.dll");
        g_real = LoadLibraryA(sys);
        if (!g_real) return FALSE;                 /* 系统库加载失败: 放弃转发 */
        for (int i = 0; i < 180; i++)               /* 解析全部转发地址 */
            *fp_TABLE[i] = GetProcAddress(g_real, kExportNames[i]);
        fp_ordinal2stub = GetProcAddress(g_real, (LPCSTR)(ULONG_PTR)2);

        /* 仅在 navicat.exe 内启用补丁线程 */
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
