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
#include <wininet.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "wininet.lib")

#define CURRENT_BUILD_NUMBER 9

void CheckForUpdates(HWND hwnd) {
    HINTERNET hInternet = InternetOpen(L"zShot", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) return;
    
    HINTERNET hConnect = InternetConnect(hInternet, L"github.com", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (hConnect) {
        const wchar_t* acceptTypes[] = { L"*/*", NULL };
        HINTERNET hRequest = HttpOpenRequest(hConnect, L"HEAD", L"/CoolBeanGames/zShot/releases/latest", NULL, NULL, acceptTypes, INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_AUTO_REDIRECT, 0);
        if (hRequest) {
            if (HttpSendRequest(hRequest, NULL, 0, NULL, 0)) {
                DWORD statusCode = 0;
                DWORD length = sizeof(statusCode);
                HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &length, NULL);
                
                if (statusCode == 302 || statusCode == 301) {
                    wchar_t location[1024] = {0};
                    length = sizeof(location);
                    if (HttpQueryInfo(hRequest, HTTP_QUERY_LOCATION, location, &length, NULL)) {
                        std::wstring loc(location);
                        size_t pos = loc.find(L"zShot_");
                        if (pos != std::wstring::npos) {
                            int latestBuild = _wtoi(loc.c_str() + pos + 6);
                            if (latestBuild > CURRENT_BUILD_NUMBER) {
                                wchar_t exePath[MAX_PATH] = {0};
                                GetModuleFileName(NULL, exePath, MAX_PATH);
                                std::wstring exeDir = exePath;
                                size_t lastSlash = exeDir.find_last_of(L"\\/");
                                if (lastSlash != std::wstring::npos) {
                                    exeDir = exeDir.substr(0, lastSlash);
                                }
                                std::wstring scriptPath = exeDir + L"\\zShot_updater.ps1";
                                std::wstring args = L"-ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + scriptPath + L"\" " + std::to_wstring(latestBuild);
                                ShellExecute(NULL, L"open", L"powershell.exe", args.c_str(), NULL, SW_HIDE);
                                PostMessage(hwnd, WM_CLOSE, 0, 0);
                            }
                        }
                    }
                }
            }
            InternetCloseHandle(hRequest);
        }
        InternetCloseHandle(hConnect);
    }
    InternetCloseHandle(hInternet);
}




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

#include "third_party/zui/bindings/cpp/zui.h"

HWND hAboutWnd = NULL;
std::unique_ptr<zui::Host> aboutUi;

void build_ui(zui::Host& host, const std::unordered_map<std::string, zui::MessageHandler>& handlers);

void OpenAboutWindow(HWND parent) {
    if (hAboutWnd) {
        SetForegroundWindow(hAboutWnd);
        return;
    }
    
    WNDCLASS wc = {};
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
        if (msg == WM_DESTROY) {
            aboutUi.reset();
            hAboutWnd = NULL;
            return 0;
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    };
    wc.hInstance = hInst;
    wc.lpszClassName = L"zShotAboutWnd";
    wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(1));
    RegisterClass(&wc);
    
    hAboutWnd = CreateWindowEx(0, L"zShotAboutWnd", L"About zShot", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 400, 250, parent, NULL, hInst, NULL);
    
    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileName(NULL, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L"\\/"));
    char exeDirA[MAX_PATH] = {0};
    WideCharToMultiByte(CP_UTF8, 0, exeDir.c_str(), -1, exeDirA, MAX_PATH, NULL, NULL);
    std::string coreRoot = std::string(exeDirA) + "\\zui";
    
    aboutUi = std::make_unique<zui::Host>(hAboutWnd);
    aboutUi->set_core_root(coreRoot);
    build_ui(*aboutUi, {});
    aboutUi->set_theme("holo");
    aboutUi->on("close-about", [](const std::string&) {
        PostMessage(hAboutWnd, WM_CLOSE, 0, 0);
    });
}

#define ID_TRAY_EXIT 1001
#define ID_TRAY_CAPTURE 1002
#define ID_TRAY_UPDATE 1003
#define ID_TRAY_ABOUT 1004

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            CheckForUpdates(hwnd);
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.hWnd = hwnd;
            nid.uID = 1;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_TRAYICON;
            nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(1)); // 1 is the icon ID
            wcscpy_s(nid.szTip, L"zShot");
            Shell_NotifyIcon(NIM_ADD, &nid);
            return 0;
        case WM_TRAYICON:
            if (lp == WM_RBUTTONUP || lp == WM_LBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_CAPTURE, L"Take Screenshot");
                InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_UPDATE, L"Check for Update");
                InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_ABOUT, L"About");
                InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");
                SetForegroundWindow(hwnd);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
                if (cmd == ID_TRAY_EXIT) {
                    PostQuitMessage(0);
                } else if (cmd == ID_TRAY_CAPTURE) {
                    if (!isCapturing) StartCapture();
                } else if (cmd == ID_TRAY_UPDATE) {
                    CheckForUpdates(hwnd);
                } else if (cmd == ID_TRAY_ABOUT) {
                    OpenAboutWindow(hwnd);
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
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
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
