/*
 * taskbar_guard.c -- tiny helper that restores the hidden taskbar when
 * the main lab program exits for any reason (including being killed).
 * Launched hidden by rnsmwr_sim.exe with the lab PID as argument.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show) {
    DWORD pid;
    HANDLE h;
    HWND tray;

    (void)hInst;
    (void)hPrev;
    (void)show;

    pid = (DWORD)wcstoul(cmd, NULL, 10);
    if (pid == 0) return 1;

    h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!h) return 1;

    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);

    tray = FindWindowW(L"Shell_TrayWnd", NULL);
    if (tray) ShowWindow(tray, SW_SHOW);
    return 0;
}