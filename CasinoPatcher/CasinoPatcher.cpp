#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commdlg.h>

#include <string>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include "Resource.h"

#pragma comment(lib, "Comdlg32.lib")

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

static HWND g_hEditPath = nullptr;
static HWND g_hBtnLoad = nullptr;
static HWND g_hBtnPatch = nullptr;
static HWND g_hInfo = nullptr;

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

static void AppendInfo(const std::wstring& s)
{
    int len = GetWindowTextLengthW(g_hInfo);
    SendMessageW(g_hInfo, EM_SETSEL, len, len);
    SendMessageW(g_hInfo, EM_REPLACESEL, 0, (LPARAM)s.c_str());
    SendMessageW(g_hInfo, EM_REPLACESEL, 0, (LPARAM)L"\r\n");
}

static std::wstring BrowseForFile(HWND owner)
{
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn))
    {
        return file;
    }
    return L"";
}

void CenterWindowOnMonitor(HWND hwnd)
{
    RECT rcWindow;
    GetWindowRect(hwnd, &rcWindow);
    int windowWidth = rcWindow.right - rcWindow.left;
    int windowHeight = rcWindow.bottom - rcWindow.top;
    HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);
    RECT rcWork = mi.rcWork;
    int x = rcWork.left + ((rcWork.right - rcWork.left) - windowWidth) / 2;
    int y = rcWork.top + ((rcWork.bottom - rcWork.top) - windowHeight) / 2;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CASINOPATCHER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CASINOPATCHER));

    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_CASINOPATCHER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON1));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance;

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
      CW_USEDEFAULT, CW_USEDEFAULT, 600, 400, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   CenterWindowOnMonitor(hWnd);

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case IDC_BTN_LOAD:
            {
                auto path = BrowseForFile(hWnd);
                if (!path.empty())
                {
                    SetWindowTextW(g_hEditPath, path.c_str());
                    EnableWindow(g_hBtnPatch, TRUE);
                    AppendInfo(L"Selected: " + path);
                }
                return 0;
            }
            case IDC_BTN_PATCH:
            {
                wchar_t path[MAX_PATH]{};
                GetWindowTextW(g_hEditPath, path, MAX_PATH);
                AppendInfo(L"Patching: " + std::wstring(path));
                // TODO - do patching here
                AppendInfo(L"Done.");
                return 0;
            }
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_CREATE:
    {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        g_hEditPath = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 12, 12, 420, 24, hWnd, (HMENU)IDC_EDIT_PATH, hInst, nullptr);
        g_hBtnLoad = CreateWindowEx(0, L"BUTTON", L"Load file...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 440, 12, 120, 24, hWnd, (HMENU)IDC_BTN_LOAD, hInst, nullptr);
        g_hBtnPatch = CreateWindowEx(0, L"BUTTON", L"Patch file", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 12, 44, 120, 28, hWnd, (HMENU)IDC_BTN_PATCH, hInst, nullptr);
        g_hInfo = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 12, 84, 548, 260, hWnd, (HMENU)IDC_EDIT_INFO, hInst, nullptr);

        SendMessageW(g_hEditPath, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_hBtnLoad, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_hBtnPatch, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_hInfo, WM_SETFONT, (WPARAM)hFont, TRUE);

        EnableWindow(g_hBtnPatch, FALSE);
        return 0;
    }
    case WM_SIZE:
    {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);
        int margin = 12;
        int top = 12;
        int btnW = 120;
        int editH = 24;
        MoveWindow(g_hEditPath, margin, top, w - (margin * 3) - btnW, editH, TRUE);
        MoveWindow(g_hBtnLoad, w - margin - btnW, top, btnW, editH, TRUE);
        MoveWindow(g_hBtnPatch, margin, top + 32, btnW, 28, TRUE);
        int infoTop = top + 72;
        MoveWindow(g_hInfo, margin, infoTop, w - margin * 2, h - infoTop - margin, TRUE);
        return 0;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}