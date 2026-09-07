#include <windows.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_CAPTURE 1002

using namespace Gdiplus;

HINSTANCE hInst;
NOTIFYICONDATA nid = {};
HBITMAP hScreenBmp = NULL;
int screenX, screenY, screenW, screenH;

bool isCapturing = false;
bool isDragging = false;
POINT dragStart;
POINT dragEnd;

HWND hOverlay = NULL;

std::wstring GetSavePath() {
    PWSTR path = NULL;
    SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &path);
    std::wstring pics(path);
    CoTaskMemFree(path);
    
    std::wstring dir = pics + L"\\zShotCaptures";
    CreateDirectoryW(dir.c_str(), NULL);
    
    std::time_t t = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &t);
    std::wstringstream wss;
    wss << dir << L"\\" << std::put_time(&tm, L"%Y-%m-%d_%H-%M-%S") << L".png";
    return wss.str();
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

void SaveBitmapToPng(HBITMAP hBmp, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    Bitmap bmp(hBmp, NULL);
    Bitmap cropped(w, h, bmp.GetPixelFormat());
    Graphics g(&cropped);
    g.DrawImage(&bmp, 0, 0, x, y, w, h, UnitPixel);

    CLSID pngClsid;
    GetEncoderClsid(L"image/png", &pngClsid);
    std::wstring path = GetSavePath();
    cropped.Save(path.c_str(), &pngClsid, NULL);
}

void DrawOverlay(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hScreenBmp);
    
    HDC hdcBuffer = CreateCompatibleDC(hdc);
    HBITMAP hBuffer = CreateCompatibleBitmap(hdc, screenW, screenH);
    HBITMAP hOldBuffer = (HBITMAP)SelectObject(hdcBuffer, hBuffer);
    
    BitBlt(hdcBuffer, 0, 0, screenW, screenH, hdcMem, 0, 0, SRCCOPY);
    
    Graphics g(hdcBuffer);
    SolidBrush darkBrush(Color(128, 0, 0, 0));
    
    if (isDragging) {
        int rx = min(dragStart.x, dragEnd.x);
        int ry = min(dragStart.y, dragEnd.y);
        int rw = abs(dragStart.x - dragEnd.x);
        int rh = abs(dragStart.y - dragEnd.y);
        
        g.FillRectangle(&darkBrush, 0, 0, screenW, ry);
        g.FillRectangle(&darkBrush, 0, ry + rh, screenW, screenH - (ry + rh));
        g.FillRectangle(&darkBrush, 0, ry, rx, rh);
        g.FillRectangle(&darkBrush, rx + rw, ry, screenW - (rx + rw), rh);
        
        Pen borderPen(Color(255, 255, 255, 255), 1.0f);
        g.DrawRectangle(&borderPen, rx, ry, rw, rh);
    } else {
        g.FillRectangle(&darkBrush, 0, 0, screenW, screenH);
    }
    
    BitBlt(hdc, 0, 0, screenW, screenH, hdcBuffer, 0, 0, SRCCOPY);
    
    SelectObject(hdcBuffer, hOldBuffer);
    DeleteObject(hBuffer);
    DeleteDC(hdcBuffer);
    SelectObject(hdcMem, hOldBmp);
    DeleteDC(hdcMem);
    
    EndPaint(hwnd, &ps);
}

void CopyToClipboard(HBITMAP hBmp, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hCopy = CreateCompatibleBitmap(hdcScreen, w, h);
        HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hCopy);
        
        HDC hdcSrc = CreateCompatibleDC(hdcScreen);
        HBITMAP hOldSrc = (HBITMAP)SelectObject(hdcSrc, hBmp);
        
        BitBlt(hdcMem, 0, 0, w, h, hdcSrc, x, y, SRCCOPY);
        
        SelectObject(hdcSrc, hOldSrc);
        DeleteDC(hdcSrc);
        
        SelectObject(hdcMem, hOld);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        
        SetClipboardData(CF_BITMAP, hCopy);
        CloseClipboard();
    }
}

void StartCapture() {
    screenX = GetSystemMetrics(SM_XVIRTUALSCREEN);
    screenY = GetSystemMetrics(SM_YVIRTUALSCREEN);
    screenW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    screenH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    hScreenBmp = CreateCompatibleBitmap(hdcScreen, screenW, screenH);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hScreenBmp);
    BitBlt(hdcMem, 0, 0, screenW, screenH, hdcScreen, screenX, screenY, SRCCOPY);
    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    
    WNDCLASS wc = {};
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
        switch (msg) {
            case WM_PAINT:
                DrawOverlay(hwnd);
                return 0;
            case WM_LBUTTONDOWN:
                isDragging = true;
                dragStart.x = LOWORD(lp);
                dragStart.y = HIWORD(lp);
                dragEnd = dragStart;
                SetCapture(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            case WM_MOUSEMOVE:
                if (isDragging) {
                    dragEnd.x = LOWORD(lp);
                    dragEnd.y = HIWORD(lp);
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            case WM_LBUTTONUP:
                if (isDragging) {
                    isDragging = false;
                    dragEnd.x = LOWORD(lp);
                    dragEnd.y = HIWORD(lp);
                    ReleaseCapture();
                    
                    int rx = min(dragStart.x, dragEnd.x);
                    int ry = min(dragStart.y, dragEnd.y);
                    int rw = abs(dragStart.x - dragEnd.x);
                    int rh = abs(dragStart.y - dragEnd.y);
                    
                    if (rw > 0 && rh > 0) {
                        SaveBitmapToPng(hScreenBmp, rx, ry, rw, rh);
                        CopyToClipboard(hScreenBmp, rx, ry, rw, rh);
                    }
                    DestroyWindow(hwnd);
                }
                return 0;
            case WM_KEYDOWN:
                if (wp == VK_ESCAPE) {
                    DestroyWindow(hwnd);
                }
                return 0;
            case WM_DESTROY:
                isCapturing = false;
                if (hScreenBmp) {
                    DeleteObject(hScreenBmp);
                    hScreenBmp = NULL;
                }
                hOverlay = NULL;
                return 0;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    };
    wc.hInstance = hInst;
    wc.lpszClassName = L"zShotOverlay";
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    RegisterClass(&wc);
    
    isCapturing = true;
    hOverlay = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"zShotOverlay", L"",
        WS_POPUP | WS_VISIBLE,
        screenX, screenY, screenW, screenH,
        NULL, NULL, hInst, NULL);
    
    SetForegroundWindow(hOverlay);
    SetFocus(hOverlay);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hwnd;
            nid.uID = 1;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_TRAYICON;
            nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(101)); // 101 is IDI_APP_ICON
            wcscpy_s(nid.szTip, L"zShot");
            Shell_NotifyIcon(NIM_ADD, &nid);
            return 0;
        case WM_TRAYICON:
            if (lp == WM_RBUTTONUP || lp == WM_LBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_CAPTURE, L"Take Screenshot");
                InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");
                SetForegroundWindow(hwnd);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
                if (cmd == ID_TRAY_EXIT) {
                    PostQuitMessage(0);
                } else if (cmd == ID_TRAY_CAPTURE) {
                    if (!isCapturing) StartCapture();
                }
            }
            return 0;
        case WM_DESTROY:
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HANDLE hMutex = CreateMutex(NULL, TRUE, L"zShot_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }
    
    hInst = hInstance;
    
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"zShotHiddenWnd";
    RegisterClass(&wc);
    
    HWND hwnd = CreateWindowEx(0, L"zShotHiddenWnd", L"zShot", 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    GdiplusShutdown(gdiplusToken);
    return 0;
}
