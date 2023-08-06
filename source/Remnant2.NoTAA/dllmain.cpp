#include "stdafx.h"
#include "helper.hpp"
#include "memory.hpp"
#include "git_ver.h"

HMODULE baseModule = GetModuleHandle(NULL);

#define wstr(s) L#s
#define wxstr(s) wstr(s)
#define _PROJECT_NAME L"Remnant2.NoTAA"
#define _PROJECT_LOG_PATH _PROJECT_NAME L".log"

wchar_t exePath[_MAX_PATH] = { 0 };

// INI Variables
bool bDisableTAA;
float fSharpness;

void ReadConfig(void)
{
    inipp::Ini<wchar_t> ini;
    // Get game name and exe path
    LOG(_PROJECT_NAME " Built: " __TIME__ " @ " __DATE__ "\n");
    LOG(L"" GIT_COMMIT "\n");
    LOG(L"" GIT_VER "\n");
    LOG(L"" GIT_NUM "\n");
    LOG(L"Game Name: %s\n", Memory::GetVersionProductName().c_str());
    LOG(L"Game Path: %s\n", exePath);

    // Initialize config
    // UE4 games use launchers so config path is relative to launcher
    std::wstring config_path = _PROJECT_NAME L".ini";
    std::wifstream iniFile(config_path);
    if (!iniFile)
    {
        // no ini, lets generate one.
        LOG(L"Failed to load config file.\n");
        std::wstring ini_defaults = L"[Settings]\n"
                                    wstr(bDisableTAA)" = true\n"
                                    wstr(fSharpness)" = 0.0\n";
        std::wofstream iniFile(config_path);
        iniFile << ini_defaults;
        bDisableTAA = true;
        fSharpness = 0;
        LOG(L"Created default config file.\n");
    }
    else
    {
        ini.parse(iniFile);
        inipp::get_value(ini.sections[L"Settings"], wstr(bDisableTAA), bDisableTAA);
        inipp::get_value(ini.sections[L"Settings"], wstr(fSharpness), fSharpness);
    }

    // Log config parse
    LOG(L"%s: %s (%i)\n", wstr(bDisableTAA), GetBoolStr(bDisableTAA) , bDisableTAA);
    LOG(L"%s: %.1f\n", wstr(fSharpness), fSharpness);
}

void DisableTAA(void)
{
    const unsigned char patch[] = { 0xEB, 0x02, 0x33, 0xC0, 0x90, 0x90, 0x90 }; 
    // EB02   | jmp remnant2-win64-shipping.7FF63AD8C7B0 
    // 33C0   | xor eax, eax
    // 8B0438 | mov eax, dword ptr ds : [rax + rdi]             --> 90     | nop
    //                                                              90     | nop
    //                                                              90     | nop
    // 85C0   | test eax, eax
    // 7904   | jns remnant2 - win64 - shipping.7FF63AD8C7BB
    WritePatchPattern(L"EB 02 33 C0 8B 04 38 85 C0 79 04", patch, sizeof(patch), L"Disable TAA", 0);    //sets r.AntiAliasingMethod=0
    WritePatchPattern(L"EB 02 33 C0 8B 04 38 85 C0 79 04", patch, sizeof(patch), L"Disable TAA", 0);    //sets r.Mobile.AntiAliasing=0 (not required, but the sub body is the same)
}

DWORD64 SetCustomSharpnessReturnAddress;
void __attribute__((naked)) SetCustomSharpnessAsm()
{
    __asm
    {
        modified_code:
            movss xmm13, [fSharpness]     // modified from: movss xmm13, dword ptr ds : [rax + 0x4]
        original_code:
            comiss xmm13, xmm7
            mov qword ptr ss : [rbp+0x60], rbx

        jmp [SetCustomSharpnessReturnAddress]
    }
}

void SetCustomSharpness(void)
{
    WritePatchPattern_Hook(L"F3 44 0F 10 68 04 44 0F 2F EF 48 89 5d 60", 14, L"Set Custom Sharpness", 0, SetCustomSharpnessAsm, &SetCustomSharpnessReturnAddress);
}

DWORD __stdcall Main(void*)
{
    bLoggingEnabled = false;

    wchar_t LogPath[_MAX_PATH] = { 0 };
    wcscpy_s(exePath, _countof(exePath), GetRunningPath(exePath));
    _snwprintf_s(LogPath, _countof(LogPath), _TRUNCATE, L"%s\\%s", exePath, L"" _PROJECT_LOG_PATH);
    LoggingInit(L"" _PROJECT_NAME, LogPath);
    ReadConfig();

    if (bDisableTAA)
        DisableTAA();
    SetCustomSharpness();

    LOG(L"Shutting down " wstr(fp_log) " file handle.\n");
    fclose(fp_log);
    return true;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        CreateThread(NULL, 0, Main, 0, NULL, 0);
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
