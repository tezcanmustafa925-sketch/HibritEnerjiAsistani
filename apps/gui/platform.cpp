#include "platform.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#endif

namespace gui {

#ifdef _WIN32

namespace {

std::string toUtf8(const wchar_t* w)
{
    if (w == nullptr || *w == L'\0') return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(static_cast<std::size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring toWide(const std::string& s)
{
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 1) return {};
    std::wstring w(static_cast<std::size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

} // namespace

bool fileDialogSupported()
{
    return true;
}

void showErrorBox(const std::string& title, const std::string& message)
{
    MessageBoxW(nullptr, toWide(message).c_str(), toWide(title).c_str(),
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

std::string openFileDialog(void* owner, FileKind kind)
{
    wchar_t path[4096] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(owner);
    ofn.lpstrFile = path;
    ofn.nMaxFile = static_cast<DWORD>(sizeof(path) / sizeof(path[0]));
    if (kind == FileKind::CycleCsv) {
        ofn.lpstrFilter = L"Çevrim dosyası (*.csv)\0*.csv\0Tüm dosyalar (*.*)\0*.*\0";
        ofn.lpstrTitle = L"Sürüş çevrimi CSV dosyasını seçin";
    } else {
        ofn.lpstrFilter = L"Ayar dosyası (*.ini)\0*.ini\0Tüm dosyalar (*.*)\0*.*\0";
        ofn.lpstrTitle = L"Ayar dosyasını seçin";
    }
    // OFN_NOCHANGEDIR: programın çalışma klasörü değişmesin
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&ofn)) {
        return {};
    }
    return toUtf8(path);
}

std::string saveFileDialog(void* owner, FileKind kind, const std::string& suggestedName)
{
    wchar_t path[4096] = L"";
    // Önerilen ad (UTF-8 -> UTF-16); sığmazsa boş bırakılır
    if (!suggestedName.empty()) {
        const int n = MultiByteToWideChar(CP_UTF8, 0, suggestedName.c_str(), -1, nullptr, 0);
        if (n > 0 && n < static_cast<int>(sizeof(path) / sizeof(path[0]))) {
            MultiByteToWideChar(CP_UTF8, 0, suggestedName.c_str(), -1, path, n);
        }
    }
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(owner);
    ofn.lpstrFile = path;
    ofn.nMaxFile = static_cast<DWORD>(sizeof(path) / sizeof(path[0]));
    switch (kind) {
    case FileKind::Png:
        ofn.lpstrFilter = L"PNG resmi (*.png)\0*.png\0";
        ofn.lpstrDefExt = L"png";
        ofn.lpstrTitle = L"Ekran görüntüsünü kaydet";
        break;
    case FileKind::ComparisonCsv:
        ofn.lpstrFilter = L"CSV dosyası (*.csv)\0*.csv\0";
        ofn.lpstrDefExt = L"csv";
        ofn.lpstrTitle = L"Strateji karşılaştırmasını kaydet";
        break;
    default:
        ofn.lpstrFilter = L"CSV dosyası (*.csv)\0*.csv\0";
        ofn.lpstrDefExt = L"csv";
        ofn.lpstrTitle = L"Sürüş sonuçlarını (adımlar) kaydet";
        break;
    }
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (!GetSaveFileNameW(&ofn)) {
        return {};
    }
    return toUtf8(path);
}

std::vector<std::string> commandLineArgsUtf8(int argc, char** argv)
{
    std::vector<std::string> out;
    int n = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &n);
    if (wargv == nullptr) {
        for (int i = 1; i < argc; ++i) out.emplace_back(argv[i]);
        return out;
    }
    for (int i = 1; i < n; ++i) out.push_back(toUtf8(wargv[i]));
    LocalFree(wargv);
    return out;
}

#else

bool fileDialogSupported()
{
    return false;
}

std::string openFileDialog(void*, FileKind)
{
    return {};
}

std::string saveFileDialog(void*, FileKind, const std::string&)
{
    return {};
}

std::vector<std::string> commandLineArgsUtf8(int argc, char** argv)
{
    std::vector<std::string> out;
    for (int i = 1; i < argc; ++i) out.emplace_back(argv[i]);
    return out;
}

void showErrorBox(const std::string&, const std::string&) {}

#endif

} // namespace gui
