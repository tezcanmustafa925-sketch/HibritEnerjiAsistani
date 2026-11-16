#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "hea/mode.h"
#include "live_run.h"
#include "platform.h"
#include "session.h"

namespace gui {

// Komut satırından gelen başlangıç seçimleri (hea_gui --cevrim nedc ...)
struct StartOptions {
    std::string cycle;      // hazır çevrim adı
    std::string strategy;   // ice / ev / kural / model
    std::string cycleFile;  // CSV çevrim dosyası (seçili hale gelir)
    std::string configFile; // ayar dosyası
    // Doluysa "kaydet" düğmeleri dosya penceresi açmadan bu klasöre önerilen adla yazar
    // (otomatik testler ve toplu kullanım için)
    std::string saveDir;
    std::string error;      // argümanlar okunurken oluşan hata (ekranda gösterilir)
};

// Arayüzün durumu ve çizimi. Pencere/OpenGL ayrıntılarını bilmez (main.cpp'de);
// her karede draw() çağrılır. Hesap yapmaz: LiveRun üzerinden core'u çalıştırır,
// gelen StepResult'ları gösterir.
//
// İki çalıştırma yolu aynı LiveRun'ı kullanır:
//   Çalıştır (F5)  çevrimi hemen sonuna kadar çalıştırır
//   Canlı oynat    çevrimi gerçek zamana bağlı, hız çarpanıyla adım adım oynatır;
//                  geçen süre ImGui'nin kare süresinden (io.DeltaTime) alınır
class App {
public:
    // nativeWindow: dosya penceresinin sahibi (Windows'ta HWND), yoksa nullptr
    App(void* nativeWindow, const StartOptions& start);
    void draw();

    // Son hata (boşsa hata yok). Ekran görüntüsü modunda çıkış kodunu belirler.
    const std::string& lastError() const { return error_; }
    bool hasResult() const { return run_.active(); }

    // --- Otomatik test sürücüsü için (hea_gui --eylem ...) ---
    // Son karede çizilen kontrollerin ekran dikdörtgenleri, anahtarla:
    // "cevrim", "cevrim/<ad>" (liste açıkken), "strateji", "strateji/<0..3>",
    // "soc", "sabit_ortam", "ortam", "sabit_yuk", "yuk", "klima", "soguk",
    // "csv_ekle", "ayar_yukle", "varsayilan", "calistir",
    // "canli", "duraklat", "sona_git", "hiz", "sekme_surus", "sekme_karsilastirma"
    struct Rect {
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    };
    bool findWidget(const std::string& key, Rect& out) const;
    // Ayarların ve son sonucun tek satırlık özeti (testler bunu denetler)
    std::string statusLine() const;

    // "Görüntü kaydet": arayüz bir yol ister, pencereyi çizen taraf (main.cpp) o karenin
    // görüntüsünü alıp PNG'ye yazar ve sonucu bildirir
    bool takeScreenshotRequest(std::string& utf8Path);
    void screenshotSaved(const std::string& utf8Path, bool ok);

private:
    void record(const std::string& key);        // son çizilen kontrol
    void recordFullWidth(const std::string& key); // sıradaki tam genişlikli kontrol
    void drawSettings();
    void drawCycleSelector();
    void drawSummary();
    void drawKeyFigures();
    void drawRunFooter();
    void drawLiveBar();
    void drawPlots();
    void drawModeStrip(float height, bool live, double now);
    void drawComparison();
    void drawComparisonChart(float height);
    void drawComparisonTable();
    void runActive();  // Çalıştır (F5): açık sekmenin işi (sürüş ya da karşılaştırma)
    void runCompare();
    // Ayar değişti: gösterilen sonuçlar eskidi. Strateji seçimi karşılaştırmayı
    // etkilemez (4 strateji birden karşılaştırılır).
    void markDirty(bool affectsComparison = true);
    void runNow();
    void startLive();
    void finishLive();
    void togglePause();
    void updateLive();      // her karenin başında: canlı çalıştırmayı ilerlet
    void resetPlotData();   // yeni çalıştırma: grafik dizilerini baştan kur
    void syncPlotData();    // yeni atılan adımları grafik dizilerine ekle
    bool liveRunning() const { return liveMode_ && run_.active() && !run_.finished(); }
    void addCycleFromDialog();
    void loadConfigFromDialog();
    // Dışa aktarma: açık sekmenin CSV'si (adımlar ya da karşılaştırma), ekran görüntüsü
    void drawExportButtons(bool csvEnabled);
    void saveCsv();
    void requestScreenshot();
    std::string pickSavePath(FileKind kind, const std::string& suggestedName);
    void reportSaved(const std::string& utf8Path, const std::string& detail);

    Session session_;
    RunSettings settings_;
    void* nativeWindow_ = nullptr;

    LiveRun run_;             // gösterilen çalıştırma (canlıysa yarıda olabilir)
    bool liveMode_ = false;   // run_ "Canlı oynat" ile mi başlatıldı?
    bool playing_ = false;    // canlı çalıştırma oynuyor mu (false: duraklatıldı)
    float liveSpeed_ = 10.0f; // simülasyon saniyesi / gerçek saniye (hea_cli --hiz ile aynı)
    bool dirty_ = false;      // ayarlar son çalıştırmadan beri değişti mi?

    // Strateji karşılaştırması (ikinci sekme). Sekme ilk açıldığında bir kez kendiliğinden
    // hesaplanır (~0.1 s); sonra Karşılaştır (F5) ile
    enum class Tab { Drive, Comparison };
    Tab activeTab_ = Tab::Drive;
    ComparisonResult comparison_;
    bool hasComparison_ = false;
    bool compareDirty_ = false; // ayarlar karşılaştırmadan beri değişti mi?
    bool compareTried_ = false; // kendiliğinden hesaplama denendi mi (hatada tekrar denemesin)

    bool fitPlots_ = true;    // sonraki karede eksenleri veriye oturt
    std::string error_;
    std::string info_;
    std::string saveDir_;           // --kayit-klasoru (boşsa dosya penceresi)
    std::string saveNotice_;        // "Kaydedildi: ..." (ayar değişince / yeni çalıştırmada silinir)
    std::string lastSaved_;         // son kaydedilen dosyanın adı (durum satırı için)
    std::string pendingScreenshot_; // bu karenin görüntüsü bu yola yazılacak

    // Grafik dizileri: hedef hız çevrimin tamamı (baştan bilinir), diğerleri
    // atılan adımlar geldikçe büyür
    std::vector<double> tc_, vTarget_;
    std::vector<double> t_, v_, soc_, fuel_;
    struct ModeSegment {
        double t0, t1;
        hea::Mode mode;
    };
    std::vector<ModeSegment> modeSegments_; // aynı moddaki ardışık adımlar birleşik
    std::size_t plotted_ = 0;               // grafik dizilerine eklenmiş adım sayısı
    // Grafiklerin ortak zaman ekseni (birinde yakınlaştırınca hepsi izler)
    double xMin_ = 0.0, xMax_ = 1.0;
    double socLimits_[2] = {0.0, 100.0}; // çalıştırma anındaki SOC penceresi [%]

    std::map<std::string, Rect> rects_; // her karenin başında temizlenir
    float footerHeight_ = 0.0f;         // Çalıştır bölümünün ölçülen yüksekliği
};

} // namespace gui
