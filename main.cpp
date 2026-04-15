#define _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include "overlay.h"
#include <Windows.h>
#include <iostream>
#include <csignal>

// Crash handler - keeps console open on any crash
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* pExceptionInfo)
{
    std::cout << std::endl;
    std::cout << "[ ! ] CRASH at address: 0x" << std::hex << pExceptionInfo->ExceptionRecord->ExceptionAddress << std::endl;
    std::cout << "[ ! ] Exception code: 0x" << std::hex << pExceptionInfo->ExceptionRecord->ExceptionCode << std::dec << std::endl;
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    return EXCEPTION_EXECUTE_HANDLER;
}

// Check if target process is still running by testing if world pointer is valid (DMA-only, no Windows API)
void CheckProcess()
{
    static DWORD lastCheckTick = 0;
    static int failCount = 0;
    DWORD now = GetTickCount();
    if (now - lastCheckTick < 5000)
        return;
    lastCheckTick = now;

    uint64_t test = 0;
    globals.vmmManager->readMemory(globals.process_id, globals.World, &test, sizeof(test), VMMDLL_FLAG_NOCACHE);
    if (!test) {
        failCount++;
        if (failCount >= 3) // 3 consecutive failures = game is gone
            exit(0);
    } else {
        failCount = 0;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

    switch (uMsg)
    {
    case WM_DESTROY:
    {
        ov::clean_directx();
        exit(0);
        return 0;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Log file that flushes every line so we capture output even on crash
static FILE* g_logFile = nullptr;

void Log(const char* msg)
{
    printf("%s\n", msg);
    if (g_logFile) {
        fprintf(g_logFile, "%s\n", msg);
        fflush(g_logFile);
    }
}

void Log(const std::string& msg)
{
    Log(msg.c_str());
}

int main()
{
    // Open log file FIRST - this survives any crash
    g_logFile = fopen("dayz_external_log.txt", "w");
    Log("========================================");
    Log("  DayZ External - DMA Edition");
    Log("========================================");

    // Keep console visible and install crash handler
    AllocConsole();
    ShowWindow(GetConsoleWindow(), SW_SHOW);
    SetUnhandledExceptionFilter(CrashHandler);

    Log("[ * ] About to call init()...");

    try {
        init();
    }
    catch (const std::exception& e) {
        Log(std::string("[ ! ] Exception during init: ") + e.what());
        if (g_logFile) fclose(g_logFile);
        std::cin.get();
        return 1;
    }
    catch (...) {
        Log("[ ! ] Unknown exception during init");
        if (g_logFile) fclose(g_logFile);
        std::cin.get();
        return 1;
    }

    Log("[ + ] Starting overlay...");
    ov::create_window();

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            CheckProcess();
            ov::loop();
        }
    }

    return 0;
}
