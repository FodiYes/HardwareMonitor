#pragma once
#include <windows.h>
#include <shellapi.h>
#include "resource.h"

#ifndef IDI_ICON1
#define IDI_ICON1 101
#endif

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_ICON 1001

#define ID_TRAY_EXIT  2001
#define ID_TRAY_UNPIN 2002

class Tray {
public:
    static void AddIcon(HWND hwnd, HINSTANCE hInstance) {
        NOTIFYICONDATA nid = { 0 };
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hwnd;
        nid.uID = ID_TRAY_ICON;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));

        if (!nid.hIcon) {
            nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        }

        wcscpy_s(nid.szTip, L"System Widget (Pinned)");

        Shell_NotifyIcon(NIM_ADD, &nid);
    }

    static void RemoveIcon(HWND hwnd) {
        NOTIFYICONDATA nid = { 0 };
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hwnd;
        nid.uID = ID_TRAY_ICON;
        Shell_NotifyIcon(NIM_DELETE, &nid);
    }

    static void ShowContextMenu(HWND hwnd, POINT pt) {
        HMENU hMenu = CreatePopupMenu();
        if (hMenu) {
            AppendMenu(hMenu, MF_STRING, ID_TRAY_UNPIN, L"Unpin from Desktop");
            AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Close Widget");

            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
    }
};