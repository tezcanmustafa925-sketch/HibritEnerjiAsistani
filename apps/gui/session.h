#pragma once

// Arayüzün ImGui'den bağımsız mantığı: hangi çevrimler var, hangi ayarlarla
// çalıştırılacak, sonuç ne. Pencere ya da çizim bilmez; birim testlerle sınanır
// (tests/test_gui_session.cpp). Hata durumunda istisna fırlatmaz, açıklama döndürür.
//
// Yollar UTF-8'dir (ImGui ve Windows dosya penceresi UTF-8 verir); dosyalar
// std::filesystem ile açılır, böylece Türkçe karakterli Windows yolları da çalışır.

#include <memory>
#include <string>
#include <vector>

#include "hea/comparison.h"
#include "hea/config_file.h"
#include "hea/drive_cycle.h"
#include "hea/simulation.h"
#include "hea/step_result.h"
#include "hea/strategy.h"

namespace gui {

enum class StrategyKind { IceOnly, EvFirst, RuleBased, ModelBased };

constexpr int kStrategyCount = 4;
StrategyKind strategyFromIndex(int i);
int strategyIndex(StrategyKind k);
// Ekranda görünen ad ("Model tabanlı (ECMS)") ve kısa açıklama
const char* strategyLabel(StrategyKind k);
const char* strategyDescription(StrategyKind k);
// Komut satırı adı: ice / ev / kural / model (hea_cli ile aynı). Bilinmiyorsa false.
bool strategyFromName(const std::string& name, StrategyKind& out);
std::unique_ptr<hea::Strategy> makeStrategy(StrategyKind k, const hea::FullConfig& cfg);

// Seçilebilir bir çevrim ve listede altında göründüğü grup
struct CycleEntry {
    std::string group; // "Test", "Eğitim", "Standart", "Dosya"
    hea::DriveCycle cycle;
};

// Çalıştırma seçimleri. Başlangıç SOC'si, klima ve soğuk başlangıç
// Session::config() içindedir (ayar dosyasından da gelebildikleri için).
struct RunSettings {
    int cycleIndex = 0;
    StrategyKind strategy = StrategyKind::ModelBased;
    // true ise çevrimdeki ortam sıcaklığı / yük yerine sabit değer kullanılır
    bool overrideAmbient = false;
    double ambient_C = 25.0;
    bool overridePayload = false;
    double payload_kg = 75.0;
};

struct RunResult {
    std::string cycleName;
    std::string strategyName; // core'daki ad, ör. "model-tabanli"
    double initialSoc = 0.0;
    std::vector<hea::StepResult> steps;
    hea::RunSummary summary;
};

// Bir stratejinin SOC düzeltmeli karşılaştırma satırı. Hesap hea_cli --karsilastir
// ile aynıdır (aynı core çağrıları, aynı sıra; bkz. hea/comparison.h).
struct ComparisonEntry {
    StrategyKind kind = StrategyKind::ModelBased;
    std::string strategyName;        // core'daki ad, ör. "kural-tabanli"
    hea::RunSummary raw;             // seçili başlangıç SOC'siyle tek koşu (ham)
    double rawStoredEnergy_J = 0.0;  // ham koşuda depolanan enerji kaybı (+ = boşaldı)
    hea::SocCorrectedFuel corrected; // doğrusal SOC düzeltmesi (ana yöntem)

    double correctedPer100km_L() const { return corrected.fuelPer100km_L(); }
    // Kontrol yöntemi: eşdeğer yakıt = yakıt + k * E_bat
    double equivalentPer100km_L() const;
};

struct ComparisonResult {
    std::string cycleName;
    double initialSoc = 0.0;
    std::vector<ComparisonEntry> entries; // 4 strateji, strategyFromIndex sırasıyla
    int referenceIndex = 0;               // farkların referansı: kural tabanlı (CLI ile aynı)
    int bestIndex = 0;                    // SOC düzeltmeli yakıtı en az olan

    // Referansa göre yüzde fark: 100 * (değer / referans - 1)
    double correctedDiffPct(int i) const;
    double equivalentDiffPct(int i) const;
};

class Session {
public:
    // Hazır çevrimleri (test, eğitim, standart) ve varsayılan ayarları yükler
    Session();

    const std::vector<CycleEntry>& cycles() const { return cycles_; }
    // Adı verilen çevrimin sırası; yoksa -1
    int findCycle(const std::string& name) const;
    // Varsayılan seçim: sehir_101 (hea_cli'nin varsayılanı)
    int defaultCycleIndex() const;

    // Simülasyon ve strateji parametreleri (arayüz doğrudan düzenler)
    hea::FullConfig& config() { return config_; }
    const hea::FullConfig& config() const { return config_; }
    // Ayarların kaynağı: "varsayılan" ya da yüklenen dosyanın adı
    const std::string& configSource() const { return configSource_; }

    // CSV çevrimini okuyup "Dosya" grubuna ekler (aynı adlı dosya çevrimi
    // varsa yerine koyar). Başarılıysa boş metin döner ve index çevrimin sırası olur.
    std::string addCycleFile(const std::string& utf8Path, int& index);
    // Ayar dosyasını varsayılanların üzerine okur. Hatalı ya da geçersizse
    // mevcut ayarlar değişmez ve hata açıklaması döner.
    std::string loadConfigFile(const std::string& utf8Path);
    void resetConfig();

    // Seçilen çevrimi seçilen stratejiyle baştan sona çalıştırır.
    // Başarılıysa boş metin döner ve out doldurulur; hata varsa out değişmez.
    std::string run(const RunSettings& settings, RunResult& out) const;

    // Seçili çevrimde 4 stratejinin SOC düzeltmeli karşılaştırması (settings.strategy
    // kullanılmaz; hepsi karşılaştırılır). Strateji başına 1 ham + 9 düzeltme koşusu.
    // Başarılıysa boş metin döner ve out doldurulur; hata varsa out değişmez.
    std::string compare(const RunSettings& settings, ComparisonResult& out) const;

    // Çalıştırmaya hazırlık (run ve LiveRun ortak kullanır): ayarları denetler,
    // ayarlar uygulanmış çevrim kopyasını ve yeni bir strateji nesnesini verir.
    // Hata varsa açıklama döner, çıktılar değişmez.
    std::string prepare(const RunSettings& settings, hea::DriveCycle& cycleOut,
                        std::unique_ptr<hea::Strategy>& strategyOut) const;

    // Ayarlar uygulanmış çevrim kopyası (sabit sıcaklık / yük)
    static hea::DriveCycle applyOverrides(const hea::DriveCycle& c, const RunSettings& s);

private:
    std::vector<CycleEntry> cycles_;
    hea::FullConfig config_;
    std::string configSource_ = "varsayılan";
};

// UTF-8 yoldaki dosya adı (klasörsüz)
std::string fileNameOf(const std::string& utf8Path);

// --- Dışa aktarma: hea_cli ile aynı CSV biçimleri (apps/common/result_csv.h) ---
// Önerilen dosya adları: "<çevrim>_<strateji>_adimlar.csv", "<çevrim>_karsilastirma.csv"
std::string suggestedStepsFileName(const RunResult& r);
std::string suggestedComparisonFileName(const ComparisonResult& c);
// UTF-8 yola yazar (Türkçe karakterli Windows yolları da). Adımlar hea_cli --csv,
// karşılaştırma hea_cli --karsilastir-csv ile bayt bayt aynıdır. Hata varsa açıklama döner.
std::string saveStepsCsv(const std::string& utf8Path, const RunResult& r);
std::string saveComparisonCsv(const std::string& utf8Path, const ComparisonResult& c);

} // namespace gui
