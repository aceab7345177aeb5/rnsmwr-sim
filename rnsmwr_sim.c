/*
 * rnsmwr_sim.c -- RNSMWR-SIM Security Awareness Simulation (harmless, educational).
 *
 *  DEFENSIVE TRAINING TOOL ONLY -- NOT MALWARE. See README.md.
 *
 *  What this program does and does NOT do:
 *   - It ONLY displays a fullscreen cosmetic simulation screen: a Windows-98-style
 *     mock wallpaper behind a centered awareness panel (styled after historical
 *     security incidents for training realism). It does NOT touch the user's real
 *     wallpaper, files, folders, registry or network.
 *   - It blocks common keyboard shortcuts (WIN, ALT+TAB, ALT+F4, CTRL+ESC...)
 *     and hides the taskbar while it runs, so the person at the machine has
 *     to ask IT for the unlock key -- the point of a lab exercise.
 *   - The unlock key is encrypted into the binary at build time with
 *     ML-KEM-1024 (see keygen_tool.c + key_secret.h). Typing the correct
 *     key closes the program. Task Manager (CTRL+ALT+DEL) always works as
 *     an emergency exit, and taskbar_guard.exe restores the taskbar if the
 *     program is killed.
 *   - On startup it writes an informational drill notice to the Desktop.
 *
 *  Build: run build.ps1  (needs MinGW-w64 gcc).
 */
#define INITGUID
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <propsys.h>
#include <propkey.h>
#include <shlobj.h>
#include <shellapi.h>
#include <wincodec.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <mmsystem.h>

#include "api.h"
#include "fips202.h"
#include "keyblob.h"
#include "wallpaper_png.h"
#include "sound_wav.h"
#include "gif_frames.h"

/* ============================== CONFIG ============================== */

/* Window / masquerade ------------------------------------------------ */
#define WIN_APP_TITLE   L"Just another document."
#define WIN_TOOLTIP     L"Just another document."

/* Panel geometry ------------------------------------------------------ */
#define PANEL_WIDTH     1150
#define PANEL_HEIGHT    720

/* Header -------------------------------------------------------------- */
#define HEADER_TEXT     L"RNSMWR-SIM"
#define HEADER_COLOR    RGB(210, 0, 0)

/* Countdown ------------------------------------------------------------ */
#define COUNTDOWN_LABEL L"Exercise timer:"
#define COUNTDOWN_DAYS  2
#define COUNTDOWN_HOURS 0
#define COUNTDOWN_MIN   0
#define COUNTDOWN_SEC   0
#define COUNTDOWN_COLOR RGB(210, 0, 0)

/* Headlines ------------------------------------------------------------ */
#define HEADLINE_1      L"SIMULATION: This is a security awareness drill."
#define HEADLINE_2      L"No files were encrypted. This is a harmless lab exercise."
#define HEADLINE_COLOR  RGB(255, 255, 255)
#define HEADLINE2_COLOR RGB(230, 230, 230)

/* Body ------------------------------------------------------------------ */
#define BODY_1          L"This machine is in a controlled training simulation."
#define BODY_2          L"No files were encrypted, nothing was modified, and no"
#define BODY_3          L"payment is required. The screen is purely cosmetic."
#define BODY_4          L"Type the unlock key below and press Enter (or click Unlock)."
#define BODY_COLOR      RGB(200, 200, 200)

/* Fake BTC line (informational) ---------------------------------------- */
#define BTC_TEXT        L"BTC: N3v3r60nN@G1v3uUpN3v3rGuNnALe7tY08ud0wN  (fake / informational)"
#define BTC_COLOR       RGB(170, 170, 170)

/* Key entry -------------------------------------------------------------- */
#define KEY_LABEL       L"Unlock key:"
#define UNLOCK_TEXT     L"Unlock"
#define STATUS_IDLE     L"Enter the unlock key (ask IT if you do not have it)."
#define STATUS_WRONG    L"Invalid key. Contact IT for assistance."
#define STATUS_OK       L"Correct key! Unlocking..."

/* "How to unlock" animated GIF section ------------------------------ */
#define GIF_LABEL       L"How to unlock:"

/* Button colors ---------------------------------------------------------- */
#define BTN_FACE        RGB(40, 40, 40)
#define BTN_HOVER       RGB(70, 70, 70)
#define BTN_EDGE        RGB(160, 0, 0)
#define BTN_TEXT_COLOR  RGB(255, 255, 255)

/* Drill notice on desktop -------------------------------------------------- */
#define NOTE_FILENAME   L"RNSMWR-SIM_README.txt"

/* ======================== end of CONFIG ================================ */

#define IDC_EDIT 1001
#define IDC_BTN  1002

static HWND g_edit, g_btn;
static HHOOK g_hook;
static HWND g_tray;
static HBRUSH g_editBg;
static WNDPROC g_btnOldProc;
static int g_secondsLeft = COUNTDOWN_DAYS * 86400 + COUNTDOWN_HOURS * 3600
                         + COUNTDOWN_MIN * 60 + COUNTDOWN_SEC;

static HFONT g_hTitle, g_hCount, g_hHeadline, g_hHeadline2, g_hBody,
             g_hBtc, g_hLabel, g_hStatus;

static HBITMAP g_wallBmp;
static HDC g_wallDC;
static int g_wallW, g_wallH;

/* Wallpaper loaded from the PNG embedded at compile time (wallpaper_png.h) */
static HBITMAP g_pngBmp;
static int g_pngW, g_pngH;

/* Animated GIF (decrypt instructions) embedded at compile time (gif_frames.h) */
static HBITMAP *g_gifBmps;
static int g_gifCount, g_gifW, g_gifH, g_gifCur;
static uint32_t g_gifDelayMs[64];

static wchar_t g_status[128];
static int g_statusState; /* 0=idle, 1=wrong, 2=ok */

static HFONT make_font(int px, BOOL bold) {
    return CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE,
                       FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

/* ------------------------------------------------------------------ */
/* Layout                                                             */
/* ------------------------------------------------------------------ */
static void get_layout(RECT *panel, RECT *edit, RECT *btn, RECT *status) {
    int W = GetSystemMetrics(SM_CXSCREEN);
    int H = GetSystemMetrics(SM_CYSCREEN);
    int PW = PANEL_WIDTH, PH = PANEL_HEIGHT;
    int PX, PY;

    if (PW > W - 40) PW = W - 40;
    if (PH > H - 40) PH = H - 40;
    PX = (W - PW) / 2;
    PY = (H - PH) / 2;

    if (panel) *panel = (RECT){PX, PY, PX + PW, PY + PH};

    if (edit || btn) {
        int tot = 460 + 16 + 130;
        int x0 = PX + (PW - tot) / 2;
        int y = PY + PH - 175;
        if (edit) *edit = (RECT){x0, y, x0 + 460, y + 36};
        if (btn)  *btn = (RECT){x0 + 476, y, x0 + 476 + 130, y + 36};
    }
    if (status) *status = (RECT){PX, PY + PH - 120, PX + PW, PY + PH - 92};
}

/* ------------------------------------------------------------------ */
/* Text helpers                                                       */
/* ------------------------------------------------------------------ */
static void textL(HDC hdc, int x, int y, const wchar_t *s, HFONT f, COLORREF c) {
    HGDIOBJ old = SelectObject(hdc, f);
    SetTextColor(hdc, c);
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x, y, s, (int)wcslen(s));
    SelectObject(hdc, old);
}

static void textC(HDC hdc, int cx, int y, const wchar_t *s, HFONT f, COLORREF c) {
    SIZE sz;
    HGDIOBJ old = SelectObject(hdc, f);
    SetTextColor(hdc, c);
    SetBkMode(hdc, TRANSPARENT);
    GetTextExtentPoint32W(hdc, s, (int)wcslen(s), &sz);
    TextOutW(hdc, cx - sz.cx / 2, y, s, (int)wcslen(s));
    SelectObject(hdc, old);
}

static void textR(HDC hdc, int right, int y, const wchar_t *s, HFONT f, COLORREF c) {
    SIZE sz;
    HGDIOBJ old = SelectObject(hdc, f);
    SetTextColor(hdc, c);
    SetBkMode(hdc, TRANSPARENT);
    GetTextExtentPoint32W(hdc, s, (int)wcslen(s), &sz);
    TextOutW(hdc, right - sz.cx, y, s, (int)wcslen(s));
    SelectObject(hdc, old);
}

/* ------------------------------------------------------------------ */
/* Windows 98 style mock wallpaper (drawn only, real wallpaper intact) */
/* ------------------------------------------------------------------ */
static void wall_cloud(int cx, int cy, int sc) {
    HBRUSH w = CreateSolidBrush(RGB(255, 255, 255));
    HGDIOBJ ob = SelectObject(g_wallDC, w);
    Ellipse(g_wallDC, cx - 70 * sc, cy - 30 * sc, cx + 70 * sc, cy + 30 * sc);
    Ellipse(g_wallDC, cx - 35 * sc, cy - 48 * sc, cx + 45 * sc, cy + 20 * sc);
    Ellipse(g_wallDC, cx + 30 * sc, cy - 35 * sc, cx + 105 * sc, cy + 25 * sc);
    SelectObject(g_wallDC, ob);
    DeleteObject(w);
}

static void wall_hill(int cx, int cy, int rx, int ry, COLORREF col) {
    HBRUSH b = CreateSolidBrush(col);
    HGDIOBJ ob = SelectObject(g_wallDC, b);
    Ellipse(g_wallDC, cx - rx, cy - ry, cx + rx, cy + ry);
    SelectObject(g_wallDC, ob);
    DeleteObject(b);
}

/* ------------------------------------------------------------------ */
/* Embedded PNG wallpaper (read during compilation into wallpaper_png.h) */
/* ------------------------------------------------------------------ */
static int load_wallpaper_png(void) {
    IWICImagingFactory *fac = NULL;
    IStream *stream = NULL;
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *conv = NULL;
    HGLOBAL hg = NULL;
    void *p = NULL;
    BYTE *buf = NULL;
    BITMAPINFO bmi;
    void *bits = NULL;
    UINT w = 0, h = 0, stride, bufsize;
    HBITMAP hbm = NULL;
    int ok = 0;

    if (WALLPAPER_PNG_SIZE == 0) return 0;

    hg = GlobalAlloc(GMEM_MOVEABLE, WALLPAPER_PNG_SIZE);
    if (!hg) return 0;
    p = GlobalLock(hg);
    if (!p) { GlobalFree(hg); return 0; }
    memcpy(p, WALLPAPER_PNG, WALLPAPER_PNG_SIZE);
    GlobalUnlock(hg);

    if (FAILED(CreateStreamOnHGlobal(hg, TRUE, &stream))) {
        GlobalFree(hg);
        return 0;
    }
    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IWICImagingFactory, (void **)&fac))) {
        goto done;
    }
    if (FAILED(fac->lpVtbl->CreateDecoderFromStream(fac, stream, NULL,
                        WICDecodeMetadataCacheOnLoad, &dec))) {
        goto done;
    }
    if (FAILED(dec->lpVtbl->GetFrame(dec, 0, &frame))) goto done;
    if (FAILED(frame->lpVtbl->GetSize(frame, &w, &h)) || w == 0 || h == 0) {
        goto done;
    }
    if (FAILED(fac->lpVtbl->CreateFormatConverter(fac, &conv))) goto done;
    if (FAILED(conv->lpVtbl->Initialize(conv, (IWICBitmapSource *)frame,
                        &GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                        NULL, 0.0, WICBitmapPaletteTypeMedianCut))) {
        goto done;
    }

    stride = w * 4;
    bufsize = stride * h;
    buf = (BYTE *)malloc(bufsize);
    if (!buf) goto done;
    if (FAILED(conv->lpVtbl->CopyPixels(conv, NULL, stride, bufsize, buf))) {
        goto done;
    }

    memset(&bmi, 0, sizeof bmi);
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (LONG)w;
    bmi.bmiHeader.biHeight = -((LONG)h); /* top-down DIB */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    hbm = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (hbm && bits) {
        memcpy(bits, buf, bufsize);
        g_pngBmp = hbm;
        g_pngW = (int)w;
        g_pngH = (int)h;
        ok = 1;
    } else if (hbm) {
        DeleteObject(hbm);
    }

done:
    if (buf) free(buf);
    if (conv) conv->lpVtbl->Release(conv);
    if (frame) frame->lpVtbl->Release(frame);
    if (dec) dec->lpVtbl->Release(dec);
    if (fac) fac->lpVtbl->Release(fac);
    if (stream) stream->lpVtbl->Release(stream);
    return ok;
}

static void build_wallpaper(void) {
    HDC screen;
    int W, H, y;
    HFONT f;
    HGDIOBJ of;

    if (g_wallDC) { DeleteDC(g_wallDC); g_wallDC = NULL; }
    if (g_wallBmp) { DeleteObject(g_wallBmp); g_wallBmp = NULL; }

    screen = GetDC(NULL);
    W = GetSystemMetrics(SM_CXSCREEN);
    H = GetSystemMetrics(SM_CYSCREEN);
    g_wallDC = CreateCompatibleDC(screen);
    g_wallBmp = CreateCompatibleBitmap(screen, W, H);
    SelectObject(g_wallDC, g_wallBmp);
    g_wallW = W;
    g_wallH = H;

    /* embedded PNG (scaled to screen) takes priority */
    if (g_pngBmp) {
        HDC mem = CreateCompatibleDC(g_wallDC);
        HGDIOBJ ob = SelectObject(mem, g_pngBmp);
        SetStretchBltMode(g_wallDC, HALFTONE);
        StretchBlt(g_wallDC, 0, 0, W, H, mem, 0, 0, g_pngW, g_pngH, SRCCOPY);
        SelectObject(mem, ob);
        DeleteDC(mem);
        ReleaseDC(NULL, screen);
        return;
    }

    /* sky gradient */
    for (y = 0; y < H; y += 2) {
        double t = (double)y / (H - 1);
        int r = (int)(44 + (112 - 44) * t);
        int g = (int)(84 + (160 - 84) * t);
        int b = (int)(150 + (216 - 150) * t);
        HBRUSH br = CreateSolidBrush(RGB(r, g, b));
        RECT rr = {0, y, W, y + 2};
        FillRect(g_wallDC, &rr, br);
        DeleteObject(br);
    }

    /* sun glow (soft circles, top-left) */
    {
        HBRUSH s1 = CreateSolidBrush(RGB(255, 245, 200));
        HGDIOBJ ob = SelectObject(g_wallDC, s1);
        Ellipse(g_wallDC, 60, 40, 280, 260);
        SelectObject(g_wallDC, ob);
        DeleteObject(s1);
    }

    /* clouds */
    wall_cloud((int)(W * 0.62), (int)(H * 0.18), 3);
    wall_cloud((int)(W * 0.30), (int)(H * 0.32), 2);
    wall_cloud((int)(W * 0.80), (int)(H * 0.42), 2);
    wall_cloud((int)(W * 0.12), (int)(H * 0.55), 1);

    /* hills */
    wall_hill((int)(W * 0.15), H + 40, (int)(W * 0.55), (int)(H * 0.55), RGB(40, 96, 52));
    wall_hill((int)(W * 0.85), H + 60, (int)(W * 0.60), (int)(H * 0.55), RGB(34, 82, 46));
    wall_hill((int)(W * 0.45), H + 20, (int)(W * 0.75), (int)(H * 0.60), RGB(52, 118, 64));

    /* "Microsoft Windows 98" */
    f = CreateFontW(-46, 0, 0, 0, FW_BOLD, TRUE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    0, L"Times New Roman");
    of = SelectObject(g_wallDC, f);
    SetBkMode(g_wallDC, TRANSPARENT);
    SetTextColor(g_wallDC, RGB(20, 40, 80));
    TextOutW(g_wallDC, 40, 32, L"Microsoft Windows 98", 20);
    SetTextColor(g_wallDC, RGB(255, 255, 255));
    TextOutW(g_wallDC, 38, 30, L"Microsoft Windows 98", 20);
    SelectObject(g_wallDC, of);
    DeleteObject(f);

    ReleaseDC(NULL, screen);
}

/* ------------------------------------------------------------------ */
/* Sound + animated GIF (embedded at compile time)                     */
/* ------------------------------------------------------------------ */
static void play_alert(void) {
    if (WAV_ALERT_SIZE > 0) {
        PlaySoundW((LPCWSTR)WAV_ALERT, NULL,
                   SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }
}

static void load_gif(void) {
    int i;
    if (GIF_FRAME_COUNT == 0 || GIF_FRAME_COUNT > 64) return;
    if (GIF_FRAME_W == 0 || GIF_FRAME_H == 0) return;
    g_gifBmps = (HBITMAP *)calloc((size_t)GIF_FRAME_COUNT, sizeof(HBITMAP));
    if (!g_gifBmps) return;
    g_gifCount = GIF_FRAME_COUNT;
    g_gifW = GIF_FRAME_W;
    g_gifH = GIF_FRAME_H;
    for (i = 0; i < GIF_FRAME_COUNT; i++) {
        BITMAPINFO bmi;
        void *bits = NULL;
        HBITMAP hbm;
        memset(&bmi, 0, sizeof bmi);
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = GIF_FRAME_W;
        bmi.bmiHeader.biHeight = -GIF_FRAME_H; /* top-down DIB */
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        hbm = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!hbm || !bits) {
            if (hbm) DeleteObject(hbm);
            continue;
        }
        memcpy(bits, GIF_FRAMES[i], (size_t)GIF_FRAME_W * GIF_FRAME_H * 4);
        g_gifBmps[i] = hbm;
        g_gifDelayMs[i] = GIF_FRAME_DELAY_MS[i];
        if (g_gifDelayMs[i] < 50) g_gifDelayMs[i] = 100;
    }
    g_gifCur = 0;
}

static void gif_rect(const RECT *panel, RECT *out) {
    int dw, dh, cx, top;
    if (g_gifW <= 0 || g_gifH <= 0) {
        *out = (RECT){0, 0, 0, 0};
        return;
    }
    dw = 560;
    if (dw > (panel->right - panel->left) - 40) {
        dw = (panel->right - panel->left) - 40;
    }
    dh = (int)((long long)dw * g_gifH / g_gifW);
    if (dh > 100) {
        dh = 100;
        dw = (int)((long long)dh * g_gifW / g_gifH);
    }
    cx = (panel->left + panel->right) / 2;
    top = panel->top + 395;
    *out = (RECT){cx - dw / 2, top, cx + dw / 2, top + dh};
}

static void draw_gif(HDC hdc, const RECT *panel) {
    RECT r;
    HDC mem;
    HGDIOBJ ob, oldPen, oldBrush;
    HPEN pen;
    if (g_gifCount <= 0 || !g_gifBmps[g_gifCur]) return;
    gif_rect(panel, &r);
    if (r.right <= r.left || r.bottom <= r.top) return;

    pen = CreatePen(PS_SOLID, 1, RGB(120, 0, 0));
    oldPen = SelectObject(hdc, pen);
    oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, r.left, r.top, r.right, r.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    mem = CreateCompatibleDC(hdc);
    ob = SelectObject(mem, g_gifBmps[g_gifCur]);
    SetStretchBltMode(hdc, HALFTONE);
    StretchBlt(hdc, r.left, r.top, r.right - r.left, r.bottom - r.top,
               mem, 0, 0, g_gifW, g_gifH, SRCCOPY);
    SelectObject(mem, ob);
    DeleteDC(mem);
}

/* ------------------------------------------------------------------ */
/* Simulation panel (visual style inspired by historical incidents)   */
/* ------------------------------------------------------------------ */
static void draw_panel(HDC hdc, const RECT *p, const RECT *statusR) {
    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    HPEN frame = CreatePen(PS_SOLID, 1, RGB(150, 0, 0));
    HPEN sep = CreatePen(PS_SOLID, 2, RGB(120, 0, 0));
    HGDIOBJ oldPen, oldBrush;
    int left = p->left, top = p->top, right = p->right;
    int cx = (left + right) / 2;
    wchar_t t[96];
    int d, h, m, s;
    RECT edit, btn, status;

    get_layout(NULL, &edit, &btn, &status);
    (void)statusR;

    FillRect(hdc, p, black);
    DeleteObject(black);

    oldPen = SelectObject(hdc, frame);
    oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, left, top, right, p->bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(frame);

    textL(hdc, left + 24, top + 12, HEADER_TEXT, g_hTitle, HEADER_COLOR);

    d = g_secondsLeft / 86400;
    h = (g_secondsLeft % 86400) / 3600;
    m = (g_secondsLeft % 3600) / 60;
    s = g_secondsLeft % 60;
    textR(hdc, right - 24, top + 14, COUNTDOWN_LABEL, g_hCount, COUNTDOWN_COLOR);
    swprintf(t, 96, L"Time left: %02d:%02d:%02d:%02d", d, h, m, s);
    textR(hdc, right - 24, top + 38, t, g_hCount, COUNTDOWN_COLOR);

    oldPen = SelectObject(hdc, sep);
    MoveToEx(hdc, left + 24, top + 64, NULL);
    LineTo(hdc, right - 24, top + 64);
    SelectObject(hdc, oldPen);
    DeleteObject(sep);

    textC(hdc, cx, top + 95, HEADLINE_1, g_hHeadline, HEADLINE_COLOR);
    textC(hdc, cx, top + 150, HEADLINE_2, g_hHeadline2, HEADLINE2_COLOR);

    textC(hdc, cx, top + 205, BODY_1, g_hBody, BODY_COLOR);
    textC(hdc, cx, top + 235, BODY_2, g_hBody, BODY_COLOR);
    textC(hdc, cx, top + 265, BODY_3, g_hBody, BODY_COLOR);
    textC(hdc, cx, top + 295, BODY_4, g_hBody, BODY_COLOR);

    textC(hdc, cx, top + 335, BTC_TEXT, g_hBtc, BTC_COLOR);

    if (p->bottom - p->top >= 700) {
        textC(hdc, cx, top + 365, GIF_LABEL, g_hBtc, RGB(220, 220, 220));
        draw_gif(hdc, p);
    }

    textC(hdc, cx, edit.top - 34, KEY_LABEL, g_hLabel, RGB(255, 255, 255));

    if (g_statusState == 2) {
        textC(hdc, cx, status.top, g_status, g_hStatus, RGB(80, 220, 120));
    } else if (g_statusState == 1) {
        textC(hdc, cx, status.top, g_status, g_hStatus, RGB(235, 90, 90));
    } else {
        textC(hdc, cx, status.top, g_status, g_hStatus, RGB(190, 190, 190));
    }
}

/* ------------------------------------------------------------------ */
/* Key check (ML-KEM-1024 decapsulation)                              */
/* ------------------------------------------------------------------ */
static int key_matches(const wchar_t *in) {
    char utf8[96];
    int n = WideCharToMultiByte(CP_UTF8, 0, in, -1, utf8, sizeof utf8, NULL, NULL);
    uint8_t m[32], ss[32], c[32];
    size_t len;
    int i;

    if (n <= 1) return 0;
    len = (size_t)(n - 1);

    shake256(m, 32, (const uint8_t *)utf8, len);
    PQCLEAN_MLKEM1024_CLEAN_crypto_kem_dec(ss, KEYBLOB_CT, KEYBLOB_SK);
    for (i = 0; i < 32; i++) c[i] = (uint8_t)(m[i] ^ ss[i]);
    for (i = 0; i < 32; i++) {
        if (c[i] != KEYBLOB_CHECK[i]) return 0;
    }
    return 1;
}

static void do_unlock(HWND hwnd) {
    wchar_t buf[96];

    GetWindowTextW(g_edit, buf, 96);
    if (key_matches(buf)) {
        wcscpy(g_status, STATUS_OK);
        g_statusState = 2;
        InvalidateRect(hwnd, NULL, TRUE);
        if (g_tray) ShowWindow(g_tray, SW_SHOW);
        SetTimer(hwnd, 2, 800, NULL);
    } else {
        wcscpy(g_status, STATUS_WRONG);
        g_statusState = 1;
        InvalidateRect(hwnd, NULL, TRUE);
        SendMessageW(g_edit, EM_SETSEL, 0, -1);
    }
}

/* ------------------------------------------------------------------ */
/* Keyboard block                                                      */
/* ------------------------------------------------------------------ */
static LRESULT CALLBACK lowlevel(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION) {
        KBDLLHOOKSTRUCT *k = (KBDLLHOOKSTRUCT *)lp;
        DWORD vk = k->vkCode;
        int block = 0;

        if (vk == VK_LWIN || vk == VK_RWIN || vk == VK_LMENU || vk == VK_RMENU) {
            block = 1;                       /* WIN+*, and ALT never registers */
        } else if ((vk == VK_TAB || vk == VK_ESCAPE) &&
                   (GetAsyncKeyState(VK_MENU) & 0x8000)) {
            block = 1;                       /* ALT+TAB, ALT+ESC */
        } else if (vk == VK_F4 && (GetAsyncKeyState(VK_MENU) & 0x8000)) {
            block = 1;                       /* ALT+F4 */
        } else if (vk == VK_ESCAPE && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
            block = 1;                       /* CTRL+ESC */
        }

        if (block) return 1;
    }
    return CallNextHookEx(NULL, code, wp, lp);
}

/* ------------------------------------------------------------------ */
/* Taskbar hide + watchdog                                             */
/* ------------------------------------------------------------------ */
static void hide_taskbar(void) {
    g_tray = FindWindowW(L"Shell_TrayWnd", NULL);
    if (g_tray) ShowWindow(g_tray, SW_HIDE);
}

static void spawn_watchdog(void) {
    wchar_t path[MAX_PATH], dir[MAX_PATH], cmd[MAX_PATH];
    wchar_t *slash;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    GetModuleFileNameW(NULL, path, MAX_PATH);
    wcscpy(dir, path);
    slash = wcsrchr(dir, L'\\');
    if (slash) *slash = 0;
    swprintf(cmd, MAX_PATH, L"\"%s\\taskbar_guard.exe\" %lu", dir,
             (unsigned long)GetCurrentProcessId());

    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

/* ------------------------------------------------------------------ */
/* Drill notice on desktop (harmless informational file)               */
/* ------------------------------------------------------------------ */
static void write_ransom_note(void) {
    wchar_t desk[MAX_PATH], file[MAX_PATH];
    HANDLE h;
    DWORD written;
    static const char note[] =
        "=== RNSMWR-SIM SECURITY AWARENESS DRILL (NOT REAL) ===\r\n"
        "\r\n"
        "This is a controlled, authorized lab exercise.\r\n"
        "No files were encrypted, no data was modified, and no\r\n"
        "payment is required. The fullscreen panel is purely cosmetic\r\n"
        "for security training.\r\n"
        "\r\n"
        "Simulation reference (historical context only, fake value):\r\n"
        "    BTC: N3v3r60nN@G1v3uUpN3v3rGuNnALe7tY08ud0wN (demonstration only)\r\n"
        "\r\n"
        "Never gonna give you up,\r\n"
        "Never gonna let you down,\r\n"
        "Never gonna run around and desert you,\r\n"
        "Never gonna make you cry,\r\n"
        "Never gonna say goodbye,\r\n"
        "Never gonna tell a lie and hurt you.\r\n"
        "\r\n"
        "To unlock the machine, ask your IT department for the\r\n"
        "unlock key and type it into the window. No files were touched.\r\n";

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL,
                                SHGFP_TYPE_CURRENT, desk))) {
        return;
    }
    swprintf(file, MAX_PATH, L"%s\\%s", desk, NOTE_FILENAME);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, note, (DWORD)strlen(note), &written, NULL);
    CloseHandle(h);
}

/* ------------------------------------------------------------------ */
/* Icon + taskbar tooltip                                              */
/* ------------------------------------------------------------------ */
static void set_doc_icon(HWND hwnd) {
    SHFILEINFOW sfi;
    HICON big = NULL, small = NULL;
    HICON old1, old2;

    memset(&sfi, 0, sizeof sfi);
    if (SHGetFileInfoW(L"readme.txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof sfi,
                       SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES)) {
        big = sfi.hIcon;
    }
    memset(&sfi, 0, sizeof sfi);
    if (SHGetFileInfoW(L"readme.txt", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof sfi,
                       SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
        small = sfi.hIcon;
    }
    old1 = (HICON)SetClassLongPtrW(hwnd, GCLP_HICON, (LONG_PTR)big);
    old2 = (HICON)SetClassLongPtrW(hwnd, GCLP_HICONSM, (LONG_PTR)small);
    if (old1) DestroyIcon(old1);
    if (old2) DestroyIcon(old2);
}

static void set_hover_tooltip(HWND hwnd) {
    IPropertyStore *ps = NULL;
    PROPVARIANT pv;
    HRESULT hr = SHGetPropertyStoreForWindow(hwnd, &IID_IPropertyStore,
                                             (void **)&ps);
    if (FAILED(hr) || !ps) return;
    PropVariantInit(&pv);
    pv.vt = VT_LPWSTR;
    pv.pwszVal = WIN_TOOLTIP;
    if (SUCCEEDED(ps->lpVtbl->SetValue(ps, &PKEY_Title, &pv))) {
        ps->lpVtbl->Commit(ps);
    }
    ps->lpVtbl->Release(ps);
    PropVariantClear(&pv);
}

/* ------------------------------------------------------------------ */
/* Unlock button (owner-drawn)                                         */
/* ------------------------------------------------------------------ */
static LRESULT CALLBACK btn_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        POINT pt;
        HBRUSH b;
        HPEN pen;
        HGDIOBJ ob, op;
        wchar_t t[64];
        SIZE sz;
        int hover;

        GetClientRect(h, &rc);
        GetCursorPos(&pt);
        ScreenToClient(h, &pt);
        hover = PtInRect(&rc, pt);

        b = CreateSolidBrush(hover ? BTN_HOVER : BTN_FACE);
        pen = CreatePen(PS_SOLID, 1, BTN_EDGE);
        ob = SelectObject(dc, b);
        op = SelectObject(dc, pen);
        RoundRect(dc, 1, 1, rc.right - 1, rc.bottom - 1, 8, 8);
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(b);
        DeleteObject(pen);

        GetWindowTextW(h, t, 64);
        SetTextColor(dc, BTN_TEXT_COLOR);
        SetBkMode(dc, TRANSPARENT);
        GetTextExtentPoint32W(dc, t, (int)wcslen(t), &sz);
        TextOutW(dc, (rc.right - sz.cx) / 2, (rc.bottom - sz.cy) / 2, t,
                 (int)wcslen(t));
        EndPaint(h, &ps);
        return 0;
    }
    if (msg == WM_MOUSEMOVE) {
        InvalidateRect(h, NULL, FALSE);
    }
    return CallWindowProcW(g_btnOldProc, h, msg, wp, lp);
}

/* ------------------------------------------------------------------ */
/* Window proc                                                         */
/* ------------------------------------------------------------------ */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        {
            RECT edit, btn;
            g_editBg = CreateSolidBrush(RGB(12, 12, 12));

            g_hTitle = make_font(28, TRUE);
            g_hCount = make_font(15, TRUE);
            g_hHeadline = make_font(32, TRUE);
            g_hHeadline2 = make_font(16, FALSE);
            g_hBody = make_font(15, FALSE);
            g_hBtc = make_font(14, FALSE);
            g_hLabel = make_font(16, TRUE);
            g_hStatus = make_font(15, FALSE);

            get_layout(NULL, &edit, &btn, NULL);

            g_edit = CreateWindowExW(0, L"EDIT", L"",
                                     WS_CHILD | WS_VISIBLE | WS_BORDER |
                                     ES_AUTOHSCROLL,
                                     edit.left, edit.top,
                                     edit.right - edit.left, edit.bottom - edit.top,
                                     hwnd, (HMENU)IDC_EDIT, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_edit, EM_SETLIMITTEXT, 95, 0);
            SendMessageW(g_edit, WM_SETFONT, (WPARAM)g_hLabel, TRUE);

            g_btn = CreateWindowExW(0, L"BUTTON", UNLOCK_TEXT,
                                    WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                    btn.left, btn.top,
                                    btn.right - btn.left, btn.bottom - btn.top,
                                    hwnd, (HMENU)IDC_BTN, GetModuleHandleW(NULL), NULL);
            g_btnOldProc = (WNDPROC)SetWindowLongPtrW(g_btn, GWLP_WNDPROC,
                                                      (LONG_PTR)btn_proc);
            SendMessageW(g_btn, WM_SETFONT, (WPARAM)g_hLabel, TRUE);

            wcscpy(g_status, STATUS_IDLE);
            g_statusState = 0;

            load_wallpaper_png();
            build_wallpaper();
            load_gif();
            play_alert();
            if (g_gifCount > 0) SetTimer(hwnd, 3, g_gifDelayMs[0], NULL);
            hide_taskbar();
            spawn_watchdog();
            write_ransom_note();
            set_doc_icon(hwnd);
            set_hover_tooltip(hwnd);
            g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, lowlevel,
                                       GetModuleHandleW(NULL), 0);
            SetTimer(hwnd, 1, 1000, NULL);
        }
        return 0;

    case WM_SIZE:
        build_wallpaper();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT panel, status;
            if (g_wallDC) {
                BitBlt(hdc, 0, 0, g_wallW, g_wallH, g_wallDC, 0, 0, SRCCOPY);
            }
            get_layout(&panel, NULL, NULL, &status);
            draw_panel(hdc, &panel, &status);
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_MOUSEMOVE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)wp, RGB(255, 255, 255));
        SetBkColor((HDC)wp, RGB(12, 12, 12));
        return (LRESULT)g_editBg;

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_BTN && HIWORD(wp) == BN_CLICKED) {
            do_unlock(hwnd);
        }
        return 0;

    case WM_SYSKEYDOWN:
        if (wp == VK_F4) return 0;
        break;

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
            return 0;
        }
        break;

    case WM_TIMER:
        if (wp == 1) {
            if (g_secondsLeft > 0) g_secondsLeft--;
            InvalidateRect(hwnd, NULL, TRUE);
        } else if (wp == 2) {
            KillTimer(hwnd, 2);
            PostQuitMessage(0);
        } else if (wp == 3) {
            if (g_gifCount > 0) {
                g_gifCur = (g_gifCur + 1) % g_gifCount;
                KillTimer(hwnd, 3);
                SetTimer(hwnd, 3, g_gifDelayMs[g_gifCur], NULL);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;

    case WM_DESTROY:
        if (g_hook) UnhookWindowsHookEx(g_hook);
        if (g_tray) ShowWindow(g_tray, SW_SHOW);
        KillTimer(hwnd, 1);
        KillTimer(hwnd, 2);
        KillTimer(hwnd, 3);
        PlaySoundW(NULL, NULL, 0);
        if (g_gifBmps) {
            int i;
            for (i = 0; i < g_gifCount; i++) {
                if (g_gifBmps[i]) DeleteObject(g_gifBmps[i]);
            }
            free(g_gifBmps);
            g_gifBmps = NULL;
        }
        DeleteObject(g_hTitle);
        DeleteObject(g_hCount);
        DeleteObject(g_hHeadline);
        DeleteObject(g_hHeadline2);
        DeleteObject(g_hBody);
        DeleteObject(g_hBtc);
        DeleteObject(g_hLabel);
        DeleteObject(g_hStatus);
        DeleteObject(g_editBg);
        if (g_wallDC) DeleteDC(g_wallDC);
        if (g_wallBmp) DeleteObject(g_wallBmp);
        if (g_pngBmp) DeleteObject(g_pngBmp);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show) {
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;
    int W, H;

    (void)hPrev;
    (void)cmd;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    SetProcessDPIAware();

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"RnsmwrSimLabClass";
    RegisterClassW(&wc);

    W = GetSystemMetrics(SM_CXSCREEN);
    H = GetSystemMetrics(SM_CYSCREEN);

    hwnd = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, WIN_APP_TITLE,
                           WS_POPUP, 0, 0, W, H, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd);
    if (g_edit) SetFocus(g_edit);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    CoUninitialize();
    return (int)msg.wParam;
}