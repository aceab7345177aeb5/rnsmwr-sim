/*
 * rnsmwr_demo.c  --  RNSMWR-SIM INTERFACE DEMO (100% harmless, educational)
 * ---------------------------------------------------------------
 *  A Windows GUI that simulates a historical incident screen for
 *  defensive training only. It does NOT encrypt anything, does NOT
 *  touch your files, does NOT use the network, and does NOT request
 *  payment. Every button simply shows a plain message box. Pure
 *  cosmetic / educational demo.  NOT MALWARE.
 *
 *  EVERYTHING you may want to edit is in the CONFIG section below.
 *
 *  Build (MinGW-w64, e.g. WinLibs):
 *      gcc -municode -mwindows -O2 -o rnsmwr_demo.exe rnsmwr_demo.c
 *
 *  Run:
 *      rnsmwr_demo.exe
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

/* ============================== CONFIG ============================== */

/* Window -------------------------------------------------------------- */
#define WIN_TITLE       L"RNSMWR-SIM Demo"
#define WIN_WIDTH       1000
#define WIN_HEIGHT      620
#define WIN_BG_COLOR    RGB(0, 0, 0)

/* Header (top-left) --------------------------------------------------- */
#define HEADER_TEXT     L"RNSMWR-SIM"
#define HEADER_SIZE     26
#define HEADER_COLOR    RGB(210, 0, 0)

/* Countdown (top-right). Start value = days:hours:minutes:seconds ----- */
#define COUNTDOWN_LABEL L"Simulation timer (training only):"
#define COUNTDOWN_DAYS  2
#define COUNTDOWN_HOURS 0
#define COUNTDOWN_MIN   0
#define COUNTDOWN_SEC   0
#define COUNTDOWN_COLOR RGB(210, 0, 0)

/* Big headline -------------------------------------------------------- */
#define HEADLINE_1      L"Security Drill: This is a simulation."
#define HEADLINE_2      L"This is only a DEMO screen. Nothing was encrypted."
#define HEADLINE_SIZE   34
#define HEADLINE_COLOR  RGB(255, 255, 255)
#define HEADLINE2_SIZE  15
#define HEADLINE2_COLOR RGB(255, 255, 255)

/* Body text ----------------------------------------------------------- */
#define BODY_TEXT       L"You still have time to close this window!\n"
#define BODY_TEXT_2     L"This interface is a replica for education and fun.\n"
#define BODY_TEXT_3     L"No files, folders or drives are modified in any way.\n"
#define BODY_TEXT_4     L"You can close this program at any time."
#define BODY_SIZE       15
#define BODY_COLOR      RGB(200, 200, 200)

/* Fake Bitcoin line (informational only) ------------------------------ */
#define BTC_TEXT        L"BTC Address: 1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa  (fake)"
#define BTC_SIZE        14
#define BTC_COLOR       RGB(160, 160, 160)

/* Buttons -------------------------------------------------------------- */
#define BTN1_TEXT       L"Check Payment"
#define BTN2_TEXT       L"Decrypt"
#define BTN_WIDTH       170
#define BTN_HEIGHT      46
#define BTN_FACE_COLOR  RGB(60, 60, 60)
#define BTN_EDGE_COLOR  RGB(120, 120, 120)
#define BTN_TEXT_COLOR  RGB(255, 255, 255)
#define BTN_HOVER_COLOR RGB(90, 90, 90)
#define BTN_SIZE        18

/* Message box shown when a button is pressed --------------------------- */
#define DEMO_MSG        L"This is a harmless training demo.\n\nNo payment was requested and no files were touched. Really!"
#define DEMO_MSG_TITLE  L"RNSMWR-SIM (Demo)"

/* ======================== end of CONFIG =============================== */

static int  g_secondsLeft = COUNTDOWN_DAYS * 86400 + COUNTDOWN_HOURS * 3600
                          + COUNTDOWN_MIN * 60 + COUNTDOWN_SEC;
static HFONT g_hHeader, g_hHeadline, g_hHeadline2, g_hBody, g_hBtc, g_hBtn;

static HFONT makeFont(int px, BOOL bold)
{
    return CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE,
                       FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static void drawCentered(HDC hdc, HFONT font, COLORREF color, RECT *rc,
                         const wchar_t *text, int lineHeight)
{
    RECT r = *rc;
    int  w = 0, h = 0, x, y;
    HGDIOBJ old = SelectObject(hdc, font);
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), (SIZE *)&w);
    h = lineHeight;
    x = r.left + (r.right - r.left - w) / 2;
    y = r.top + (r.bottom - r.top - h) / 2;
    TextOutW(hdc, x, y, text, (int)wcslen(text));
    SelectObject(hdc, old);
}

static void drawButton(HDC hdc, RECT rc, const wchar_t *text, int hover)
{
    HBRUSH br = CreateSolidBrush(hover ? BTN_HOVER_COLOR : BTN_FACE_COLOR);
    HPEN   pen = CreatePen(PS_SOLID, 1, BTN_EDGE_COLOR);
    HGDIOBJ ob = SelectObject(hdc, br);
    HGDIOBJ op = SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(br);
    DeleteObject(pen);
    drawCentered(hdc, g_hBtn, BTN_TEXT_COLOR, &rc, text, BTN_SIZE + 4);
}

static void drawCountdown(HDC hdc, RECT rc)
{
    wchar_t buf[128];
    int d = g_secondsLeft / 86400;
    int h = (g_secondsLeft % 86400) / 3600;
    int m = (g_secondsLeft % 3600) / 60;
    int s = g_secondsLeft % 60;
    RECT r1 = rc, r2 = rc;
    HGDIOBJ old = SelectObject(hdc, g_hBody);
    SetTextColor(hdc, COUNTDOWN_COLOR);
    SetBkMode(hdc, TRANSPARENT);
    r2.bottom = r2.top + 40;
    r1.top = r2.bottom;
    TextOutW(hdc, rc.left, r2.top, COUNTDOWN_LABEL, (int)wcslen(COUNTDOWN_LABEL));
    swprintf(buf, 128, L"Time left: %02d:%02d:%02d:%02d  |  Close window to stop it.",
             d, h, m, s);
    TextOutW(hdc, rc.left, r1.top, buf, (int)wcslen(buf));
    SelectObject(hdc, old);
}

static void paintClient(HWND hwnd, HDC hdc)
{
    RECT c;
    RECT r;
    GetClientRect(hwnd, &c);

    FillRect(hdc, &c, (HBRUSH)GetStockObject(BLACK_BRUSH));

    /* header top-left */
    r = c;
    r.right = 380;
    r.bottom = 60;
    drawCentered(hdc, g_hHeader, HEADER_COLOR, &r, HEADER_TEXT, HEADER_SIZE + 6);

    /* countdown top-right */
    r = c;
    r.left = c.right - 520;
    r.bottom = 110;
    drawCountdown(hdc, r);

    /* headline */
    r = c;
    r.top = 180;
    r.bottom = 180 + HEADLINE_SIZE + 10;
    drawCentered(hdc, g_hHeadline, HEADLINE_COLOR, &r, HEADLINE_1, HEADLINE_SIZE);

    r.top = r.bottom;
    r.bottom = r.top + HEADLINE2_SIZE + 10;
    drawCentered(hdc, g_hHeadline2, HEADLINE2_COLOR, &r, HEADLINE_2, HEADLINE2_SIZE);

    /* body */
    r = c;
    r.top = 320;
    r.bottom = 460;
    drawCentered(hdc, g_hBody, BODY_COLOR, &r, BODY_TEXT, BODY_SIZE + 6);
    r.top = 360;
    r.bottom = 460;
    drawCentered(hdc, g_hBody, BODY_COLOR, &r, BODY_TEXT_2, BODY_SIZE + 6);
    r.top = 400;
    r.bottom = 460;
    drawCentered(hdc, g_hBody, BODY_COLOR, &r, BODY_TEXT_3, BODY_SIZE + 6);
    r.top = 440;
    r.bottom = 460;
    drawCentered(hdc, g_hBody, BODY_COLOR, &r, BODY_TEXT_4, BODY_SIZE + 6);

    /* btc address */
    r = c;
    r.top = 510;
    r.bottom = 545;
    drawCentered(hdc, g_hBtc, BTC_COLOR, &r, BTC_TEXT, BTC_SIZE + 4);

    /* buttons */
    {
        RECT b1, b2;
        POINT pt;
        int  cx = (c.right + c.left) / 2;
        int  by = 470;
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);
        b1 = (RECT){ cx - BTN_WIDTH - 12, by, cx - 12, by + BTN_HEIGHT };
        b2 = (RECT){ cx + 12, by, cx + BTN_WIDTH + 12, by + BTN_HEIGHT };
        drawButton(hdc, b1, BTN1_TEXT, PtInRect(&b1, pt));
        drawButton(hdc, b2, BTN2_TEXT, PtInRect(&b2, pt));
    }
}

static void hitTestButtons(HWND hwnd, POINT pt, int *which)
{
    RECT c, b1, b2;
    int  cx, by;
    GetClientRect(hwnd, &c);
    cx = (c.right + c.left) / 2;
    by = 470;
    b1 = (RECT){ cx - BTN_WIDTH - 12, by, cx - 12, by + BTN_HEIGHT };
    b2 = (RECT){ cx + 12, by, cx + BTN_WIDTH + 12, by + BTN_HEIGHT };
    *which = 0;
    if (PtInRect(&b1, pt)) *which = 1;
    else if (PtInRect(&b2, pt)) *which = 2;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
        g_hHeader = makeFont(HEADER_SIZE, TRUE);
        g_hHeadline = makeFont(HEADLINE_SIZE, TRUE);
        g_hHeadline2 = makeFont(HEADLINE2_SIZE, TRUE);
        g_hBody = makeFont(BODY_SIZE, FALSE);
        g_hBtc = makeFont(BTC_SIZE, FALSE);
        g_hBtn = makeFont(BTN_SIZE, FALSE);
        SetTimer(hwnd, 1, 1000, NULL);
        return 0;

    case WM_TIMER:
        if (g_secondsLeft > 0) g_secondsLeft--;
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paintClient(hwnd, hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_MOUSEMOVE:
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    case WM_LBUTTONDOWN:
        {
            int which;
            POINT pt = { LOWORD(lp), HIWORD(lp) };
            hitTestButtons(hwnd, pt, &which);
            if (which)
                MessageBoxW(hwnd, DEMO_MSG, DEMO_MSG_TITLE,
                            MB_OK | MB_ICONINFORMATION);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        DeleteObject(g_hHeader);
        DeleteObject(g_hHeadline);
        DeleteObject(g_hHeadline2);
        DeleteObject(g_hBody);
        DeleteObject(g_hBtc);
        DeleteObject(g_hBtn);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show)
{
    WNDCLASSW wc = {0};
    HWND hwnd;
    MSG msg;

    (void)hPrev; (void)cmd;

    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(WIN_BG_COLOR);
    wc.lpszClassName = L"RnsmwrSimDemoClass";

    RegisterClassW(&wc);

    hwnd = CreateWindowW(wc.lpszClassName, WIN_TITLE,
                         WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         WIN_WIDTH, WIN_HEIGHT,
                         NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}