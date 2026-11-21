// CS 1.6 E52 Edition — PC test launcher: runs the bundled Java runtime with the game jar.
#include <windows.h>
#include <wchar.h>

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, PWSTR cmd, int show) {
    wchar_t dir[MAX_PATH], javaw[MAX_PATH], jar[MAX_PATH], cmdline[4096];
    GetModuleFileNameW(NULL, dir, MAX_PATH);
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash) *slash = 0;
    swprintf(javaw, MAX_PATH, L"%ls\\jre\\bin\\javaw.exe", dir);
    swprintf(jar, MAX_PATH, L"%ls\\CS16_PC.jar", dir);
    if (GetFileAttributesW(javaw) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL, L"Не найдена папка jre рядом с CS16_E52_PC.exe.\nРаспакуйте архив целиком.", L"CS 1.6 E52 (PC)", MB_ICONERROR);
        return 1;
    }
    if (GetFileAttributesW(jar) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL, L"Не найден CS16_PC.jar рядом с CS16_E52_PC.exe.", L"CS 1.6 E52 (PC)", MB_ICONERROR);
        return 1;
    }
    swprintf(cmdline, 4096, L"\"%ls\" -Xmx512m -Dsun.java2d.uiScale=1 -Dfile.encoding=UTF-8 -jar \"%ls\"", javaw, jar);
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(javaw, cmdline, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
        MessageBoxW(NULL, L"Не удалось запустить Java.", L"CS 1.6 E52 (PC)", MB_ICONERROR);
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
