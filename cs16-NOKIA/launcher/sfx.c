// CS 1.6 E52 Edition — single-file PC launcher.
// The game jar and its Java runtime are packed (xz) at the end of this exe.
// First start: unpacks them to %LOCALAPPDATA%\CS16E52\runtime_<id>\ (with a progress window),
// later starts: runs the game straight away.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "xz.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
typedef HANDLE FH;
#else
#include <sys/stat.h>
#include <errno.h>
typedef FILE *FH;
#endif

#define MAGIC "CS16PAYL"
#define CHUNK (1 << 20)

typedef struct {
    uint8_t magic[8];
    uint64_t off[2], len[2];
    uint64_t raw;      // total unpacked payload bytes (entry headers included)
    uint32_t id;       // payload id (crc), names the runtime folder
    uint32_t pad;
} Trailer;

// ---------------------------------------------------------------- portable file helpers
#ifdef _WIN32
static wchar_t gDest[MAX_PATH];
static FH self_open(const wchar_t *p) { return CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); }
static int fh_ok(FH h) { return h != INVALID_HANDLE_VALUE; }
static int fh_seek(FH h, uint64_t o) { LARGE_INTEGER li; li.QuadPart = (LONGLONG)o; return SetFilePointerEx(h, li, NULL, FILE_BEGIN) ? 0 : -1; }
static int64_t fh_size(FH h) { LARGE_INTEGER li; return GetFileSizeEx(h, &li) ? li.QuadPart : -1; }
static size_t fh_read(FH h, void *b, size_t n) { DWORD r = 0; if (!ReadFile(h, b, (DWORD)n, &r, NULL)) return 0; return r; }
static int fh_write(FH h, const void *b, size_t n) { DWORD w = 0; return (WriteFile(h, b, (DWORD)n, &w, NULL) && w == n) ? 0 : -1; }
static void fh_close(FH h) { CloseHandle(h); }
static void mkdirs(wchar_t *p) {   // create every directory of path p (p is a file path)
    for (wchar_t *s = p + 3; *s; s++) if (*s == L'\\') { *s = 0; CreateDirectoryW(p, NULL); *s = L'\\'; }
}
static FH out_open(const char *rel) {
    wchar_t w[MAX_PATH], full[MAX_PATH * 2];
    if (!MultiByteToWideChar(CP_UTF8, 0, rel, -1, w, MAX_PATH)) return INVALID_HANDLE_VALUE;
    for (wchar_t *s = w; *s; s++) if (*s == L'/') *s = L'\\';
    swprintf(full, MAX_PATH * 2, L"%ls\\%ls", gDest, w);
    mkdirs(full);
    return CreateFileW(full, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}
#else
static char gDest[4096];
static FH self_open(const char *p) { return fopen(p, "rb"); }
static int fh_ok(FH h) { return h != NULL; }
static int fh_seek(FH h, uint64_t o) { return fseeko(h, (off_t)o, SEEK_SET); }
static int64_t fh_size(FH h) { fseeko(h, 0, SEEK_END); return ftello(h); }
static size_t fh_read(FH h, void *b, size_t n) { return fread(b, 1, n, h); }
static int fh_write(FH h, const void *b, size_t n) { return fwrite(b, 1, n, h) == n ? 0 : -1; }
static void fh_close(FH h) { fclose(h); }
static FH out_open(const char *rel) {
    char full[8192];
    snprintf(full, sizeof full, "%s/%s", gDest, rel);
    for (char *s = full + 1; *s; s++) if (*s == '/') { *s = 0; mkdir(full, 0755); *s = '/'; }
    return fopen(full, "wb");
}
#endif

// ---------------------------------------------------------------- payload unpacker
// Unpacked stream = sequence of entries: u16 nameLen, name (utf-8, '/'), u32 size, data. nameLen 0 = end.
static volatile uint64_t gDone, gTotal;
static char gErr[256];

typedef struct {
    int state;           // 0 = header, 1 = data, 2 = finished
    uint8_t hdr[2 + 1024 + 4];
    size_t hlen, need;
    uint32_t left;
    FH out;
} Parser;

static int parse(Parser *ps, const uint8_t *b, size_t n) {
    while (n > 0 && ps->state != 2) {
        if (ps->state == 0) {
            size_t want = ps->hlen < 2 ? 2 : ps->need;
            size_t k = want - ps->hlen; if (k > n) k = n;
            memcpy(ps->hdr + ps->hlen, b, k); ps->hlen += k; b += k; n -= k;
            if (ps->hlen == 2) {
                size_t nl = ps->hdr[0] | (ps->hdr[1] << 8);
                if (nl == 0) { ps->state = 2; break; }
                if (nl > 1000) { strcpy(gErr, "bad payload"); return -1; }
                ps->need = 2 + nl + 4;
            } else if (ps->hlen == ps->need) {
                size_t nl = ps->need - 6;
                char name[1024]; memcpy(name, ps->hdr + 2, nl); name[nl] = 0;
                const uint8_t *s = ps->hdr + 2 + nl;
                ps->left = s[0] | (s[1] << 8) | (s[2] << 16) | ((uint32_t)s[3] << 24);
                ps->out = out_open(name);
                if (!fh_ok(ps->out)) { snprintf(gErr, sizeof gErr, "cannot write %s", name); return -1; }
                ps->state = 1; ps->hlen = 0;
            }
        }
        if (ps->state == 1) {
            size_t k = ps->left < n ? ps->left : n;
            if (k && fh_write(ps->out, b, k)) { strcpy(gErr, "write failed (disk full?)"); return -1; }
            b += k; n -= k; ps->left -= (uint32_t)k;
            if (ps->left == 0) { fh_close(ps->out); ps->state = 0; ps->hlen = 0; }
        }
    }
    return 0;
}

static int unpack(FH self, const Trailer *t) {
    Parser ps; memset(&ps, 0, sizeof ps);
    uint8_t *in = malloc(CHUNK), *out = malloc(CHUNK);
    if (!in || !out) { strcpy(gErr, "out of memory"); return -1; }
    xz_crc32_init();
    for (int s = 0; s < 2; s++) {
        struct xz_dec *d = xz_dec_init(XZ_DYNALLOC, 1u << 26);
        if (!d || fh_seek(self, t->off[s])) { strcpy(gErr, "init failed"); return -1; }
        uint64_t left = t->len[s];
        struct xz_buf xb = { in, 0, 0, out, 0, CHUNK };
        enum xz_ret r = XZ_OK;
        while (r == XZ_OK) {
            if (xb.in_pos == xb.in_size && left) {
                size_t k = left < CHUNK ? (size_t)left : CHUNK;
                if (fh_read(self, in, k) != k) { strcpy(gErr, "read failed"); return -1; }
                left -= k; xb.in_pos = 0; xb.in_size = k;
            }
            r = xz_dec_run(d, &xb);
            if (xb.out_pos == CHUNK || r == XZ_STREAM_END) {
                if (parse(&ps, out, xb.out_pos)) return -1;
                gDone += xb.out_pos; xb.out_pos = 0;
            }
            if (r == XZ_OK && xb.in_pos == xb.in_size && !left && xb.out_pos == 0) { r = XZ_DATA_ERROR; }
        }
        xz_dec_end(d);
        if (r != XZ_STREAM_END) { snprintf(gErr, sizeof gErr, "archive damaged (xz %d)", (int)r); return -1; }
    }
    free(in); free(out);
    if (ps.state != 2) { strcpy(gErr, "archive truncated"); return -1; }
    return 0;
}

static int read_trailer(FH self, Trailer *t) {
    int64_t sz = fh_size(self);
    if (sz < (int64_t)sizeof *t || fh_seek(self, (uint64_t)sz - sizeof *t) || fh_read(self, t, sizeof *t) != sizeof *t) return -1;
    return memcmp(t->magic, MAGIC, 8) ? -1 : 0;
}

#ifndef _WIN32
// Linux test build: sfx <packed-file> <dest-dir>
int main(int argc, char **argv) {
    if (argc < 3) return 2;
    FH self = self_open(argv[1]); Trailer t;
    if (!fh_ok(self) || read_trailer(self, &t)) { puts("no payload"); return 1; }
    snprintf(gDest, sizeof gDest, "%s", argv[2]); mkdir(gDest, 0755);
    gTotal = t.raw;
    int r = unpack(self, &t);
    printf("result %d %s  %llu/%llu  id %08x\n", r, gErr, (unsigned long long)gDone, (unsigned long long)gTotal, t.id);
    return r ? 1 : 0;
}
#else
// ---------------------------------------------------------------- Windows UI + launch
static const wchar_t *TITLE = L"CS 1.6 E52 (PC)";   // no version: the stub does not change between game versions

// language of the game (settings.bin byte 36 since settings v8; English until the player picks Russian)
static int gRu;
static void read_lang(void) {
    wchar_t p[MAX_PATH];
    if (!GetEnvironmentVariableW(L"APPDATA", p, MAX_PATH)) return;
    wcscat(p, L"\\CS16E52\\settings.bin");
    HANDLE f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    unsigned char b[40]; DWORD got = 0;
    if (ReadFile(f, b, sizeof b, &got, NULL) && got > 36 && b[0] >= 8) gRu = b[36] == 1;
    CloseHandle(f);
}
#define L2(en, ru) (gRu ? (ru) : (en))
static HWND gWnd;
static volatile LONG gState;   // 0 running, 1 ok, 2 error
static FH gSelf; static Trailer gT;

static DWORD WINAPI worker(LPVOID p) {
    (void)p;
    int r = unpack(gSelf, &gT);
    InterlockedExchange(&gState, r ? 2 : 1);
    PostMessageW(gWnd, WM_APP, 0, 0);
    return 0;
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_TIMER: InvalidateRect(h, NULL, FALSE); return 0;
    case WM_APP: DestroyWindow(h); return 0;
    case WM_CLOSE: return 0;   // do not interrupt unpacking
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HBRUSH bg = CreateSolidBrush(RGB(0x4C, 0x58, 0x44)); FillRect(dc, &rc, bg); DeleteObject(bg);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(0xD8, 0xDE, 0xD3));
        HFONT f = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Tahoma");
        HFONT of = SelectObject(dc, f);
        int pct = gTotal ? (int)(gDone * 100 / gTotal) : 0;
        wchar_t s[128]; swprintf(s, 128, L2(L"First start: unpacking the game... %d%%", L"Первый запуск: распаковка игры... %d%%"), pct);
        RECT tr = { 16, 14, rc.right - 16, 36 }; DrawTextW(dc, s, -1, &tr, DT_LEFT | DT_SINGLELINE);
        RECT b = { 16, 46, rc.right - 16, 66 };
        HBRUSH dk = CreateSolidBrush(RGB(0x28, 0x2E, 0x22)); FillRect(dc, &b, dk); DeleteObject(dk);
        RECT fb = b; fb.left += 2; fb.top += 2; fb.bottom -= 2; fb.right = fb.left + (b.right - b.left - 4) * pct / 100;
        HBRUSH or = CreateSolidBrush(RGB(0xC4, 0xB5, 0x50)); FillRect(dc, &fb, or); DeleteObject(or);
        SetTextColor(dc, RGB(0xA0, 0xAA, 0x95));
        RECT nr = { 16, 74, rc.right - 16, 94 }; DrawTextW(dc, L2(L"This happens only once, then the game starts right away.", L"Это только один раз, дальше игра запускается сразу."), -1, &nr, DT_LEFT | DT_SINGLELINE);
        SelectObject(dc, of); DeleteObject(f);
        EndPaint(h, &ps); return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

static void remove_old(const wchar_t *base, const wchar_t *keep) {
    wchar_t pat[MAX_PATH]; swprintf(pat, MAX_PATH, L"%ls\\runtime_*", base);
    WIN32_FIND_DATAW fd; HANDLE fh = FindFirstFileW(pat, &fd);
    if (fh == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !wcscmp(fd.cFileName, keep)) continue;
        wchar_t p[MAX_PATH + 2]; memset(p, 0, sizeof p);
        swprintf(p, MAX_PATH, L"%ls\\%ls", base, fd.cFileName);   // double-0 terminated for SHFileOperation
        SHFILEOPSTRUCTW op; memset(&op, 0, sizeof op);
        op.wFunc = FO_DELETE; op.pFrom = p; op.fFlags = FOF_NO_UI;
        SHFileOperationW(&op);
    } while (FindNextFileW(fh, &fd));
    FindClose(fh);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, PWSTR cmd, int show) {
    (void)hp; (void)cmd; (void)show;
    read_lang();
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL, exe, MAX_PATH);
    gSelf = self_open(exe);
    if (!fh_ok(gSelf) || read_trailer(gSelf, &gT)) {
        MessageBoxW(NULL, L2(L"The game file is damaged (no built-in data).\nDownload CS16_E52_PC.exe again.", L"Файл игры повреждён (нет встроенных данных).\nСкачайте CS16_E52_PC.exe заново."), TITLE, MB_ICONERROR);
        return 1;
    }
    wchar_t base[MAX_PATH], leaf[64], marker[MAX_PATH], javaw[MAX_PATH], jar[MAX_PATH], cl[4096];
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH) && !GetEnvironmentVariableW(L"APPDATA", base, MAX_PATH)) {
        wcscpy(base, exe); wchar_t *s = wcsrchr(base, L'\\'); if (s) *s = 0;
    }
    wcscat(base, L"\\CS16E52");
    CreateDirectoryW(base, NULL);
    swprintf(leaf, 64, L"runtime_%08x", gT.id);
    swprintf(gDest, MAX_PATH, L"%ls\\%ls", base, leaf);
    swprintf(marker, MAX_PATH, L"%ls\\unpacked.ok", gDest);
    swprintf(javaw, MAX_PATH, L"%ls\\jre\\bin\\javaw.exe", gDest);
    swprintf(jar, MAX_PATH, L"%ls\\CS16_PC.jar", gDest);

    if (GetFileAttributesW(marker) == INVALID_FILE_ATTRIBUTES || GetFileAttributesW(javaw) == INVALID_FILE_ATTRIBUTES) {
        CreateDirectoryW(gDest, NULL);
        gTotal = gT.raw;
        WNDCLASSW wc; memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc = wndproc; wc.hInstance = hi; wc.lpszClassName = L"CS16E52Unpack";
        wc.hCursor = LoadCursor(NULL, IDC_WAIT); wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1));
        RegisterClassW(&wc);
        RECT r = { 0, 0, 420, 104 }; AdjustWindowRect(&r, WS_CAPTION | WS_SYSMENU, FALSE);
        int ww = r.right - r.left, wh = r.bottom - r.top;
        gWnd = CreateWindowW(wc.lpszClassName, TITLE, WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
            (GetSystemMetrics(SM_CXSCREEN) - ww) / 2, (GetSystemMetrics(SM_CYSCREEN) - wh) / 2, ww, wh, NULL, NULL, hi, NULL);
        SetTimer(gWnd, 1, 60, NULL);
        HANDLE th = CreateThread(NULL, 0, worker, NULL, 0, NULL);
        MSG msg; while (GetMessageW(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        WaitForSingleObject(th, INFINITE); CloseHandle(th);
        if (gState != 1) {
            wchar_t e[512], we[256]; MultiByteToWideChar(CP_UTF8, 0, gErr, -1, we, 256);
            swprintf(e, 512, L2(L"Could not unpack the game:\n%ls\n\nFolder: %ls", L"Не удалось распаковать игру:\n%ls\n\nПапка: %ls"), we, gDest);
            MessageBoxW(NULL, e, TITLE, MB_ICONERROR);
            return 1;
        }
        HANDLE m = CreateFileW(marker, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (m != INVALID_HANDLE_VALUE) CloseHandle(m);
        remove_old(base, leaf);
    }
    fh_close(gSelf);

    swprintf(cl, 4096, L"\"%ls\" -Xmx512m -Dsun.java2d.uiScale=1 -Dfile.encoding=UTF-8 -jar \"%ls\"", javaw, jar);
    STARTUPINFOW si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si); si.cb = sizeof si; ZeroMemory(&pi, sizeof pi);
    if (!CreateProcessW(javaw, cl, NULL, NULL, FALSE, 0, NULL, gDest, &si, &pi)) {
        DeleteFileW(marker);
        MessageBoxW(NULL, L2(L"Could not start the game. Run CS16_E52_PC.exe again.", L"Не удалось запустить игру. Запустите CS16_E52_PC.exe ещё раз."), TITLE, MB_ICONERROR);
        return 1;
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 0;
}
#endif
