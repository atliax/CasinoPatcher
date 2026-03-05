#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commdlg.h>
#include <Shlwapi.h>
#include <wincrypt.h>

#include <string>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <vector>
#include <unordered_set>

#include "Resource.h"

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shlwapi.lib")

#define MAX_LOADSTRING 100

// MD5 sums of the versions tested so far
std::unordered_set<std::string> knownVersions = {
    "05d25f2c0d01d09c378a875e4db542b8", // 1.0.0.3
    "64445f05ea841899118bb8e5a6345606"  // 1.0.0.5
};

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

static HWND g_hEditPath = nullptr;
static HWND g_hBtnLoad = nullptr;
static HWND g_hBtnPatch = nullptr;
static HWND g_hInfo = nullptr;
static HWND g_hLblPath = nullptr;

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

static std::string BytesToLowerCaseHex(const BYTE* data, DWORD length)
{
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.resize(length * 2);
    for (DWORD i = 0; i < length; ++i)
    {
        out[i * 2 + 0] = kHex[(data[i] >> 4) & 0xF];
        out[i * 2 + 1] = kHex[(data[i]) & 0xF];
    }
    return out;
}

static std::string MD5OfFile(const std::wstring& path)
{
    HANDLE hFile = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return {};
    }

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    if (!CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        CloseHandle(hFile);
        return {};
    }

    if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
    {
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return {};
    }

    constexpr DWORD kBufSize = 1 << 16; //65536 bytes
    std::vector<BYTE> buf(kBufSize);

    while (true)
    {
        DWORD bytesRead = 0;
        if (!ReadFile(hFile, buf.data(), kBufSize, &bytesRead, nullptr))
        {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            CloseHandle(hFile);
            return {};
        }

        if (bytesRead == 0)
        {
            break; // reached EOF
        }

        if (!CryptHashData(hHash, buf.data(), bytesRead, 0))
        {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            CloseHandle(hFile);
            return {};
        }
    }

    BYTE hash[16];
    DWORD hashLen = sizeof(hash);
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0) || hashLen != 16)
    {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return {};
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    CloseHandle(hFile);

    return BytesToLowerCaseHex(hash, hashLen);
}

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

static BOOL isRecognized(std::wstring& path)
{
    std::string md5 = MD5OfFile(path);
    if (md5.empty())
    {
        return false;
    }
    return knownVersions.find(md5) != knownVersions.end();
}

static std::wstring MakeBakPath(const std::wstring& path)
{
    size_t slash = path.find_last_of(L"\\/");
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash))
    {
        return path + L".bak";
    }

    return path.substr(0, dot) + L".bak";
}

static BOOL DLLNameReplace(const std::wstring& path, const char find[10], const char replace[10])
{
    HANDLE hFile = CreateFileW(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(hFile, &size) || size.QuadPart <= 0 || size.QuadPart > 0x7fffffff)
    {
        CloseHandle(hFile);
        return FALSE;
    }

    HANDLE hMap = CreateFileMappingW(hFile, nullptr, PAGE_READWRITE, 0, 0, nullptr);
    if (!hMap)
    {
        CloseHandle(hFile);
        return FALSE;
    }

    BYTE* data = (BYTE*)MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    if (!data)
    {
        CloseHandle(hMap);
        CloseHandle(hFile);
        return FALSE;
    }

    const DWORD fileSize = (DWORD)size.QuadPart;
    const DWORD patternLen = 10;

    BOOL ok = FALSE;

    for (DWORD i = 0; i + patternLen < fileSize; ++i)
    {
        if (memcmp(data + i, find, patternLen) == 0)
        {
            memcpy(data + i, replace, patternLen);

            if (FlushViewOfFile(data, 0) && FlushFileBuffers(hFile))
            {
                ok = TRUE;
            }

            break;
        }
    }

    UnmapViewOfFile(data);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return ok;
}

static BOOL DoPatching(std::wstring path)
{
    constexpr char kFind[10]    = "GDI32.dll";
    constexpr char kReplace[10] = "GDI3x.dll";

    const std::wstring bakPath = MakeBakPath(path);

    if (!MoveFileExW(path.c_str(), bakPath.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        return FALSE;
    }

    if (!CopyFileW(bakPath.c_str(), path.c_str(), FALSE))
    {
        MoveFileExW(bakPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
        return FALSE;
    }

    if (!DLLNameReplace(path, kFind, kReplace))
    {
        DeleteFileW(path.c_str());
        MoveFileExW(bakPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
        return FALSE;
    }

    return TRUE;
}

static BOOL CopyShimDLL(const std::wstring& path)
{
    wchar_t srcDir[MAX_PATH]{};
    wchar_t dstDir[MAX_PATH]{};
    wchar_t srcDll[MAX_PATH]{};
    wchar_t dstDll[MAX_PATH]{};

    if (!GetModuleFileNameW(nullptr, srcDir, MAX_PATH))
    {
        return FALSE;
    }

    PathRemoveFileSpecW(srcDir);
    
    lstrcpynW(srcDll, srcDir, MAX_PATH);
    PathAppendW(srcDll, L"GDI3x.dll");

    if (GetFileAttributesW(srcDll) == INVALID_FILE_ATTRIBUTES)
    {
        return FALSE;
    }

    lstrcpynW(dstDir, path.c_str(), MAX_PATH);
    PathRemoveFileSpecW(dstDir);

    lstrcpynW(dstDll, dstDir, MAX_PATH);
    PathAppendW(dstDll, L"GDI3x.dll");

    if (!CopyFileW(srcDll, dstDll, FALSE))
    {
        return FALSE;
    }

    return TRUE;
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
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE+1);
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

                if (path.empty())
                {
                    SetWindowTextW(g_hEditPath, L"");
                    EnableWindow(g_hBtnPatch, FALSE);
                    AppendInfo(L"File selection aborted.");
                    return 0;
                }

                SetWindowTextW(g_hEditPath, path.c_str());
                EnableWindow(g_hBtnPatch, TRUE);
                AppendInfo(L"Selected: " + path);

                if (isRecognized(path))
                {
                    AppendInfo(L"Selected .exe file is a recognized version.");
                }
                else
                {
                    AppendInfo(L"Unrecognized .exe file hash.");
                    MessageBoxW(hWnd, L"That .exe is not a version I recognize but if it is indeed LCasino.exe, patching it could still work.", L"CasinoPatcher", MB_OK | MB_ICONINFORMATION);
                }

                AppendInfo(L"Click 'Patch file' to patch LCasino.exe and copy the GDI shim dll to the game directory.");

                return 0;
            }
            case IDC_BTN_PATCH:
            {
                wchar_t path[MAX_PATH]{};
                GetWindowTextW(g_hEditPath, path, MAX_PATH);

                if (path[0] == L'\0')
                {
                    MessageBoxW(hWnd, L"No file selected. Please select a file before patching.", L"CasinoPatcher", MB_OK | MB_ICONWARNING);
                    return 0;
                }

                int r = MessageBoxW(hWnd, L"Ready to patch. Proceed?", L"Confirm patch", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

                if(r != IDYES)
                {
                    AppendInfo(L"Patch aborted.");
                    return 0;
                }

                AppendInfo(L"Patching: " + std::wstring(path));

                if (DoPatching(std::wstring(path)))
                {
                    AppendInfo(L"Done patching.");
                }
                else
                {
                    AppendInfo(L"Something went wrong during patching!");
                }

                AppendInfo(L"Copying shim DLL...");
                
                if (CopyShimDLL(std::wstring(path)))
                {
                    AppendInfo(L"Copied shim DLL successfully.");
                }
                else
                {
                    AppendInfo(L"Something went wrong during copying of shim DLL.");
                }

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

        g_hLblPath = CreateWindowEx(0, L"STATIC", L"Target file:", WS_CHILD | WS_VISIBLE, 12, 16, 90, 20, hWnd, nullptr, hInst, nullptr);
        g_hEditPath = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY, 110, 12, 322, 24, hWnd, (HMENU)IDC_EDIT_PATH, hInst, nullptr);
        g_hBtnLoad = CreateWindowEx(0, L"BUTTON", L"Select file", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 440, 12, 120, 24, hWnd, (HMENU)IDC_BTN_LOAD, hInst, nullptr);
        g_hBtnPatch = CreateWindowEx(0, L"BUTTON", L"Patch file", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 12, 44, 120, 28, hWnd, (HMENU)IDC_BTN_PATCH, hInst, nullptr);
        g_hInfo = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 12, 84, 548, 260, hWnd, (HMENU)IDC_EDIT_INFO, hInst, nullptr);

        SendMessageW(g_hLblPath, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_hEditPath, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_hBtnLoad, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_hBtnPatch, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_hInfo, WM_SETFONT, (WPARAM)hFont, TRUE);

        EnableWindow(g_hBtnPatch, FALSE);

        MessageBoxW(hWnd, L"Note: This patcher has been tested with versions 1.0.0.3 and 1.0.0.5 only. Theoretically it should work on the other versions as well, though no guarantees.", L"CasinoPatcher", MB_ICONINFORMATION | MB_OK);

        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        PathRemoveFileSpecW(exePath);
        std::wstring dllPath = std::wstring(exePath) + L"\\GDI3x.dll";
        if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            MessageBoxW(hWnd, L"GDI3x.dll not found. It should be placed in the same directory as CasinoPatcher.exe", L"CasinoPatcher", MB_ICONERROR | MB_OK);
            PostQuitMessage(1);
        }

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
        int labelW = 50;
        MoveWindow(g_hEditPath, margin + labelW + 8, top, w - (margin * 3) - btnW - labelW - 8, editH, TRUE);
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