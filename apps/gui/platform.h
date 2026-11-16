#pragma once

// İşletim sistemine özgü küçük yardımcılar (şimdilik yalnızca Windows uygulaması var).

#include <string>
#include <vector>

namespace gui {

enum class FileKind {
    CycleCsv,      // açma: sürüş çevrimi
    ConfigIni,     // açma: ayar dosyası
    StepsCsv,      // kaydetme: adım sonuçları
    ComparisonCsv, // kaydetme: strateji karşılaştırması
    Png            // kaydetme: ekran görüntüsü
};

// Sistemin "Dosya aç" penceresini gösterir. Seçilen yolu UTF-8 olarak döndürür;
// vazgeçilirse ya da desteklenmiyorsa boş metin. owner: sahip pencere (HWND) ya da nullptr.
std::string openFileDialog(void* owner, FileKind kind);
// Sistemin "Farklı kaydet" penceresini önerilen dosya adıyla (UTF-8) gösterir. Uzantı
// yazılmazsa eklenir, var olan dosyanın üzerine yazmadan önce sorulur. Seçilen yol
// UTF-8; vazgeçilirse boş metin.
std::string saveFileDialog(void* owner, FileKind kind, const std::string& suggestedName);
bool fileDialogSupported();

// Komut satırı argümanları UTF-8 olarak (program adı hariç). Windows'ta argv sistem
// kod sayfasındadır; Türkçe karakterli yollar için geniş karakterli komut satırı okunur.
std::vector<std::string> commandLineArgsUtf8(int argc, char** argv);

// Hata kutusu gösterir (başlık ve metin UTF-8), kullanıcı kapatana kadar bekler.
// Program konsolsuz olduğu için stderr görünmez; açılış hataları böyle bildirilir.
// Desteklenmeyen sistemlerde hiçbir şey yapmaz.
void showErrorBox(const std::string& title, const std::string& message);

} // namespace gui
