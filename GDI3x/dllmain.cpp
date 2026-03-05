// Take up the names before including windows.h to suppress warnings
#define CreateCompatibleBitmap  CreateCompatibleBitmap__win
#define CreateDIBSection        CreateDIBSection__win
#define DeleteObject            DeleteObject__win
#define GetDIBits               GetDIBits__win
#define GetDeviceCaps           GetDeviceCaps__win
#define GetStockObject          GetStockObject__win
#define PatBlt                  PatBlt__win
#define SelectPalette           SelectPalette__win
#define SetDIBitsToDevice       SetDIBitsToDevice__win

#include <windows.h>

// Undefine the blocked names when done including windows.h
#undef CreateCompatibleBitmap
#undef CreateDIBSection
#undef DeleteObject
#undef GetDIBits
#undef GetDeviceCaps
#undef GetStockObject
#undef PatBlt
#undef SelectPalette
#undef SetDIBitsToDevice

//#include <fstream>
//std::ofstream logger;

// ==================== Real GDI32 pointers ====================

HMODULE hRealGDI = NULL;

typedef HBITMAP(WINAPI* tCreateCompatibleBitmap)(HDC hdc, int cx, int cy);
typedef HBITMAP(WINAPI* tCreateDIBSection)(HDC hdc, const BITMAPINFO* pbmi, UINT usage, void** ppvBits, HANDLE hSection, DWORD offset);
typedef BOOL(WINAPI* tDeleteObject)(HGDIOBJ ho);
typedef int     (WINAPI* tGetDIBits)(HDC hdc, HBITMAP hbm, UINT start, UINT cLines, LPVOID lpvBits, LPBITMAPINFO lpbmi, UINT usage);
typedef int     (WINAPI* tGetDeviceCaps)(HDC hdc, int index);
typedef HGDIOBJ(WINAPI* tGetStockObject)(int i);
typedef BOOL(WINAPI* tPatBlt)(HDC hdc, int x, int y, int w, int h, DWORD rop);
typedef HPALETTE(WINAPI* tSelectPalette)(HDC hdc, HPALETTE hpal, BOOL bForceBkgd);
typedef int     (WINAPI* tSetDIBitsToDevice)(HDC hdc, int XDest, int YDest, DWORD dwWidth, DWORD dwHeight,
    int XSrc, int YSrc, UINT uStartScan, UINT cScanLines,
    const VOID* lpvBits, const BITMAPINFO* lpbmi, UINT fuColorUse);

tCreateCompatibleBitmap pCreateCompatibleBitmap = NULL;
tCreateDIBSection       pCreateDIBSection = NULL;
tDeleteObject           pDeleteObject = NULL;
tGetDIBits              pGetDIBits = NULL;
tGetDeviceCaps          pGetDeviceCaps = NULL;
tGetStockObject         pGetStockObject = NULL;
tPatBlt                 pPatBlt = NULL;
tSelectPalette          pSelectPalette = NULL;
tSetDIBitsToDevice      pSetDIBitsToDevice = NULL;

// ==================== Color stuff helpers ====================

static constexpr bool kUse565 = true;

static inline void Fill16bppBitfieldsMasks(DWORD* masks /*3 DWORDs*/)
{
    if (kUse565)
    {
        masks[0] = 0xF800; // R
        masks[1] = 0x07E0; // G
        masks[2] = 0x001F; // B
    }
    else
    {
        masks[0] = 0x7C00; // R
        masks[1] = 0x03E0; // G
        masks[2] = 0x001F; // B
    }
}

// Writes 16bpp BI_BITFIELDS into a caller-provided BITMAPINFO.
// Assumes lpbmi points to at least (BITMAPINFOHEADER + 3 DWORD masks).
static inline void ForceBITMAPINFO_16bpp_Bitfields(LPBITMAPINFO lpbmi)
{
    if (!lpbmi)
    {
        return;
    }

    // Preserve width/height/sign etc, but force the format fields.
    BITMAPINFOHEADER& h = lpbmi->bmiHeader;
    h.biPlanes = 1;
    h.biBitCount = 16;
    h.biCompression = BI_BITFIELDS;

    // DIB bitfields masks are stored in bmiColors as DWORDs
    DWORD* masks = reinterpret_cast<DWORD*>(lpbmi->bmiColors);
    Fill16bppBitfieldsMasks(masks);
}

// ==================== Wrappers for export ====================

extern "C" __declspec(dllexport) HBITMAP WINAPI CreateCompatibleBitmap(HDC hdc, int cx, int cy)
{
    //logger << "CreateCompatibleBitmap called - " << hdc << " - " << cx << " - " << cy << "\n";
    return pCreateCompatibleBitmap ? pCreateCompatibleBitmap(hdc, cx, cy) : NULL;
}

extern "C" __declspec(dllexport) HBITMAP WINAPI CreateDIBSection(HDC hdc, const BITMAPINFO* pbmi, UINT usage,
    void** ppvBits, HANDLE hSection, DWORD offset)
{
    //logger << "CreateDIBSection called - " << hdc << " - " << pbmi << " - " << usage << " - " << ppvBits << " - " << hSection << " - " << offset << "\n";
    return pCreateDIBSection ? pCreateDIBSection(hdc, pbmi, usage, ppvBits, hSection, offset) : NULL;
}

extern "C" __declspec(dllexport) BOOL WINAPI DeleteObject(HGDIOBJ ho)
{
    //logger << "DeleteObject called - " << ho << "\n";
    return pDeleteObject ? pDeleteObject(ho) : FALSE;
}

extern "C" __declspec(dllexport) int WINAPI GetDIBits(HDC hdc, HBITMAP hbm, UINT start, UINT cLines,
    LPVOID lpvBits, LPBITMAPINFO lpbmi, UINT usage)
{
    //logger << "GetDIBits called - " << hdc << " - " << hbm << " - " << start << " - " << cLines << " - " << lpvBits << " - " << lpbmi << " - " << usage << "\n";

    if (!pGetDIBits)
    {
        return 0;
    }

    // Call the real one first so size/width/height get filled in.
    int ret = pGetDIBits(hdc, hbm, start, cLines, lpvBits, lpbmi, usage);

    // Probe pattern: lpvBits == NULL means "just fill BITMAPINFO"
    // Your logs show exactly that.
    if (ret != 0 && lpvBits == nullptr && lpbmi != nullptr)
    {
        // Only stomp the fields the game checks.
        // This makes the game's "is it 16-bit?" test pass.
        ForceBITMAPINFO_16bpp_Bitfields(lpbmi);
    }

    return ret;
}

extern "C" __declspec(dllexport) int WINAPI GetDeviceCaps(HDC hdc, int index)
{
    //logger << "GetDeviceCaps called - " << hdc << " - " << index << "\n";
    return pGetDeviceCaps ? pGetDeviceCaps(hdc, index) : 0;
}

extern "C" __declspec(dllexport) HGDIOBJ WINAPI GetStockObject(int i)
{
    //logger << "GetStockObject called - " << i << "\n";
    return pGetStockObject ? pGetStockObject(i) : NULL;
}

extern "C" __declspec(dllexport) BOOL WINAPI PatBlt(HDC hdc, int x, int y, int w, int h, DWORD rop)
{
    //logger << "PatBlt called - " << hdc << " - " << x << " - " << y << " - " << w << " - " << h << " - " << rop << "\n";
    return pPatBlt ? pPatBlt(hdc, x, y, w, h, rop) : FALSE;
}

extern "C" __declspec(dllexport) HPALETTE WINAPI SelectPalette(HDC hdc, HPALETTE hpal, BOOL bForceBkgd)
{
    //logger << "SelectPalette called - " << hdc << " - " << hpal << " - " << bForceBkgd << "\n";
    return pSelectPalette ? pSelectPalette(hdc, hpal, bForceBkgd) : NULL;
}

extern "C" __declspec(dllexport) int WINAPI SetDIBitsToDevice(HDC hdc, int XDest, int YDest, DWORD dwWidth, DWORD dwHeight,
    int XSrc, int YSrc, UINT uStartScan, UINT cScanLines,
    const VOID* lpvBits, const BITMAPINFO* lpbmi, UINT fuColorUse)
{
    //logger << "SetDIBitsToDevice called - " << hdc << " - " << XDest << " - " << YDest << " - " << dwWidth << " - " << dwHeight << " - " << XSrc << " - " << YSrc << " - " << uStartScan << " - " << cScanLines << " - " << lpvBits << " - " << lpbmi << " - " << fuColorUse << "\n";
    return pSetDIBitsToDevice ? pSetDIBitsToDevice(hdc, XDest, YDest, dwWidth, dwHeight, XSrc, YSrc, uStartScan, cScanLines, lpvBits, lpbmi, fuColorUse) : 0;
}

// ==================== Load real functions ====================

static void LoadRealFunctions()
{
    pCreateCompatibleBitmap = (tCreateCompatibleBitmap)GetProcAddress(hRealGDI, "CreateCompatibleBitmap");
    pCreateDIBSection = (tCreateDIBSection)GetProcAddress(hRealGDI, "CreateDIBSection");
    pDeleteObject = (tDeleteObject)GetProcAddress(hRealGDI, "DeleteObject");
    pGetDIBits = (tGetDIBits)GetProcAddress(hRealGDI, "GetDIBits");
    pGetDeviceCaps = (tGetDeviceCaps)GetProcAddress(hRealGDI, "GetDeviceCaps");
    pGetStockObject = (tGetStockObject)GetProcAddress(hRealGDI, "GetStockObject");
    pPatBlt = (tPatBlt)GetProcAddress(hRealGDI, "PatBlt");
    pSelectPalette = (tSelectPalette)GetProcAddress(hRealGDI, "SelectPalette");
    pSetDIBitsToDevice = (tSetDIBitsToDevice)GetProcAddress(hRealGDI, "SetDIBitsToDevice");
}

// ==================== DLL entry point ====================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        //logger.open("GDI_CALLS.log", std::ios::app);
        //logger << "Started...\n";

        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);
        strcat_s(path, "\\gdi32.dll");
        hRealGDI = LoadLibraryA(path);

        if (hRealGDI)
        {
            //logger << "Loaded real gdi32.dll\n";
            LoadRealFunctions();
        }
        else
        {
            //logger << "FAILED to load real gdi32.dll!\n";
        }
    }
    break;

    case DLL_PROCESS_DETACH:
        if (hRealGDI)
        {
            //logger << "detached DLL\n";
            FreeLibrary(hRealGDI);
        }
        break;
    }
    return TRUE;
}