#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <string>
#include <map>
#include <vector>
#include "system_guard.h"

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "User32.lib")

namespace fs = std::filesystem;

HWND hMainWnd;
HWND hInputBox;
HWND hTitleLabel, hFolderLabel, hBtnMove, hBtnSelectAll;
std::map<std::string, HWND> checkBoxes;
std::map<std::string, int> lastKnownCounts;
HFONT hFont;
const int WIN_WIDTH = 340;
const int WIN_HEIGHT_BASE = 420;

fs::path GetDesktopPath() {
    PWSTR path = NULL;
    if (SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &path) == S_OK) {
        fs::path p(path);
        CoTaskMemFree(path);
        return p;
    }
    return "";
}

void RunPowerShellNotify(const std::string& title, const std::string& msg) {
    std::string cmd = "powershell -WindowStyle Hidden -Command \"& {Add-Type -AssemblyName System.Windows.Forms; [System.Windows.Forms.NotifyIcon]::new().ShowBalloonTip(1000, '" + title + "', '" + msg + "', [System.Windows.Forms.ToolTipIcon]::Info)}\"";
    WinExec(cmd.c_str(), SW_HIDE);
}

void RunJSPopup(int count, const std::string& status) {
    std::string jsCode = "javascript:";
    jsCode += "var sh = new ActiveXObject('WScript.Shell');";
    jsCode += "sh.Popup('Nebula-Tidy Report:\\n\\nFiles Moved: " + std::to_string(count) + "\\nSystem Status: " + status + "', 10, 'Mission Complete (Auto-close 10s)', 64);";
    jsCode += "close();";

    std::string cmd = "mshta \"" + jsCode + "\"";
    WinExec(cmd.c_str(), SW_HIDE);
}

void CreateModernFont() {
    NONCLIENTMETRICS ncm = { sizeof(NONCLIENTMETRICS) };
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);
    hFont = CreateFontIndirect(&ncm.lfMessageFont);
}

bool allSelected = false;
void ToggleSelectAll() {
    allSelected = !allSelected;
    int checkState = allSelected ? BST_CHECKED : BST_UNCHECKED;
    for (auto const& [ext, hwnd] : checkBoxes) SendMessage(hwnd, BM_SETCHECK, checkState, 0);
    SetWindowText(hBtnSelectAll, allSelected ? "Deselect All" : "Select All");
}

void RefreshGUI() {
    for (auto const& [ext, hwnd] : checkBoxes) DestroyWindow(hwnd);
    checkBoxes.clear();
    lastKnownCounts.clear();

    std::map<std::string, int> counts;
    fs::path desk = GetDesktopPath();
    for (const auto& e : fs::directory_iterator(desk)) {
        if (e.is_regular_file()) {
            std::string x = e.path().extension().string();
            std::string name = e.path().filename().string();
            if (x != ".lnk" && x != ".ini" && x != ".exe" && name != "Nebula-Tidy.exe") counts[x]++;
        }
    }

    lastKnownCounts = counts;

    int y = 50;
    for (auto const& [ext, cnt] : counts) {
        std::string lbl = (ext.empty() ? "No Ext" : ext) + " (" + std::to_string(cnt) + ")";
        checkBoxes[ext] = CreateWindowA("BUTTON", lbl.c_str(), WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 20, y, 250, 20, hMainWnd, NULL, NULL, NULL);
        SendMessage(checkBoxes[ext], WM_SETFONT, (WPARAM)hFont, TRUE);
        y += 25;
    }

    y += 10;
    SetWindowPos(hFolderLabel, NULL, 20, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    y += 20;
    SetWindowPos(hInputBox, NULL, 20, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    y += 40;
    SetWindowPos(hBtnMove, NULL, 20, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    SetWindowPos(hMainWnd, NULL, 0, 0, WIN_WIDTH, y + 100, SWP_NOMOVE | SWP_NOZORDER);
    InvalidateRect(hMainWnd, NULL, TRUE);
    UpdateWindow(hMainWnd);
}

void CheckForUpdates() {
    std::map<std::string, int> currentCounts;
    fs::path desk = GetDesktopPath();

    for (const auto& e : fs::directory_iterator(desk)) {
        if (e.is_regular_file()) {
            std::string x = e.path().extension().string();
            std::string name = e.path().filename().string();
            if (x != ".lnk" && x != ".ini" && x != ".exe" && name != "Nebula-Tidy.exe") currentCounts[x]++;
        }
    }

    if (currentCounts != lastKnownCounts) {
        RefreshGUI();
    }
}

void RunTidy() {
    char folderName[256];
    GetWindowTextA(hInputBox, folderName, 256);
    if (strlen(folderName) == 0) { MessageBoxA(hMainWnd, "Enter folder name!", "Error", MB_ICONERROR); return; }

    fs::path desktop = GetDesktopPath();
    fs::path destFolder = desktop / folderName;

    char systemStatus[256];
    check_system_status(desktop.string().c_str(), systemStatus, 256);

    RunPowerShellNotify("Nebula-Tidy", "Operation Started...");

    std::vector<std::string> targets;
    for (auto const& [ext, hCb] : checkBoxes) {
        if (SendMessage(hCb, BM_GETCHECK, 0, 0) == BST_CHECKED) targets.push_back(ext);
    }
    if (targets.empty()) { MessageBoxA(hMainWnd, "Select files!", "Warning", MB_ICONWARNING); return; }

    if (!fs::exists(destFolder)) fs::create_directory(destFolder);

    int count = 0;
    for (const auto& entry : fs::directory_iterator(desktop)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::string name = entry.path().filename().string();
        if (ext == ".lnk" || ext == ".ini" || ext == ".exe" || name == "Nebula-Tidy.exe") continue;

        for (const auto& t : targets) {
            if (ext == t) {
                try { fs::rename(entry.path(), destFolder / name); count++; }
                catch (...) {}
            }
        }
    }

    RefreshGUI();
    RunJSPopup(count, systemStatus);
    ShellExecuteA(NULL, "open", destFolder.string().c_str(), NULL, NULL, SW_SHOWDEFAULT);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        hMainWnd = hwnd;
        CreateModernFont();
        hTitleLabel = CreateWindowA("STATIC", "Select Types:", WS_VISIBLE | WS_CHILD, 20, 15, 100, 20, hwnd, NULL, NULL, NULL);
        SendMessage(hTitleLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        hBtnSelectAll = CreateWindowA("BUTTON", "Select All", WS_VISIBLE | WS_CHILD, 210, 10, 90, 25, hwnd, (HMENU)2, NULL, NULL);
        SendMessage(hBtnSelectAll, WM_SETFONT, (WPARAM)hFont, TRUE);
        hFolderLabel = CreateWindowA("STATIC", "Destination Folder:", WS_VISIBLE | WS_CHILD, 20, 0, 200, 20, hwnd, NULL, NULL, NULL);
        SendMessage(hFolderLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        hInputBox = CreateWindowA("EDIT", "Nebula_Files", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 20, 0, 280, 25, hwnd, NULL, NULL, NULL);
        SendMessage(hInputBox, WM_SETFONT, (WPARAM)hFont, TRUE);
        hBtnMove = CreateWindowA("BUTTON", "MOVE FILES", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 20, 0, 280, 40, hwnd, (HMENU)1, NULL, NULL);
        SendMessage(hBtnMove, WM_SETFONT, (WPARAM)hFont, TRUE);

        RefreshGUI();
        SetTimer(hwnd, 1, 2000, NULL);
        break;
    }
    case WM_TIMER:
        CheckForUpdates();
        break;
    case WM_COMMAND:
        if (LOWORD(wp) == 1) RunTidy();
        if (LOWORD(wp) == 2) ToggleSelectAll();
        break;
    case WM_DESTROY:
        DeleteObject(hFont);
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        break;
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    const char* CLASS_NAME = "NebulaClass";
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindow(CLASS_NAME, "Nebula-Tidy", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, (screenW - WIN_WIDTH) / 2, (screenH - WIN_HEIGHT_BASE) / 2, WIN_WIDTH, WIN_HEIGHT_BASE, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nCmdShow);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}