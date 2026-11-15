#include "stdafx.h"
#include "helper.hpp"
#include "memory.hpp"

HMODULE baseModule = GetModuleHandle(L"GameAssembly.dll");

#define wstr(s) L#s
#define wxstr(s) wstr(s)
#define _PROJECT_LOG_PATH _PROJECT_NAME L".log"

wchar_t exePath[_MAX_PATH] = { 0 };

// INI Variables
bool bDisableTAA;

void ReadConfig(void)
{
    inipp::Ini<wchar_t> ini;
    // Get game name and exe path
    LOG(_PROJECT_NAME " Built: " __TIME__ " @ " __DATE__ "\n");
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
                                    wstr(bDisableTAA)" = true\n";
        std::wofstream iniFile(config_path);
        iniFile << ini_defaults;
        bDisableTAA = true;
        LOG(L"Created default config file.\n");
    }
    else
    {
        ini.parse(iniFile);
        inipp::get_value(ini.sections[L"Settings"], wstr(bDisableTAA), bDisableTAA);
    }

    // Log config parse
    LOG(L"%s: %s (%i)\n", wstr(bDisableTAA), GetBoolStr(bDisableTAA) , bDisableTAA);
}

void DisableTAA(void)
{
    // 00007FFA051C5E2F | 84C0            | test al, al
    // 00007FFA051C5E31 | 75 17           | jne gameassembly.7FFA051C5E4A
    // 00007FFA051C5E33 | 0FB683 88010000 | movzx eax, byte ptr ds : [rbx + 188] --> 00007FFA051C5E33       | 30C0                     | xor al,al
    // 
    // 00007FFA051C5E3A | 48 : 8B7C24 38  | mov rdi, qword ptr ss : [rsp + 38]
    // 00007FFA051C5E3F | 48 : 8B5C24 40  | mov rbx, qword ptr ss : [rsp + 40]
    const unsigned char xor_al_al_nop[] = { 0x30, 0xC0, 0x90, 0x90, 0x90, 0x90, 0x90 };
    WritePatchPattern(L"75 ?? 0F B6 83 88 01 00 00", xor_al_al_nop, sizeof(xor_al_al_nop), L"Disable TAA", 2);
}

DWORD __stdcall Main(void*)
{
    bLoggingEnabled = false;
    bDisableTAA = false;
    wchar_t LogPath[_MAX_PATH] = { 0 };
    wcscpy_s(exePath, _countof(exePath), GetRunningPath(exePath));
    _snwprintf_s(LogPath, _countof(LogPath), _TRUNCATE, L"%s\\%s", exePath, L"" _PROJECT_LOG_PATH);
    LoggingInit(L"" _PROJECT_NAME, LogPath);
    ReadConfig();
    if (bDisableTAA)
        DisableTAA();
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
