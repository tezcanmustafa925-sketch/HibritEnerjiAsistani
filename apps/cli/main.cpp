// Akıllı Hibrit Enerji Asistanı — terminal uygulaması.
// Core'dan gelen StepResult verilerini ekrana / CSV'ye yazar. Hesap yapmaz.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <io.h>
#define HEA_ISATTY _isatty
#define HEA_FILENO _fileno
#else
#include <unistd.h>
#define HEA_ISATTY isatty
#define HEA_FILENO fileno
#endif

#include "csv_export.h"
#include "hea/drive_cycle.h"
#include "hea/model_based_strategy.h"
#include "hea/rule_based_strategy.h"
#include "hea/simulation.h"
#include "hea/strategy.h"
#include "hea/training_data.h"
#include "hea/version.h"
#include "report.h"
#include "tables.h"

namespace {

// Çıkış kodları
constexpr int kExitOk = 0;
constexpr int kExitFileError = 1;
constexpr int kExitUsage = 2;

struct Options {
    // Varsayılan: tek çevrim + tek strateji, canlı akış
    std::string cycle = "sehir_101"; // çevrim adı ya da egitim / test / hepsi
    std::string strategy = "model";  // ice / ev / kural / model / hepsi
    bool steps = false;              // adım çıktısını zorla (çoklu koşuda da)
    bool summaryOnly = false;        // adım çıktısı yok, yalnızca özet
    double speed = 10.0;             // canlı akış hızı: simülasyon saniyesi / gerçek saniye
    bool fast = false;               // canlı akışta bekleme yapma
    int interval_s = 1;              // adım çıktısında kaç saniyede bir satır
    std::string csvPath;             // boş değilse CSV yaz
    std::string trainingPath;        // boş değilse eğitim verisi CSV'si yaz ve çık
    bool componentTables = false;
    bool listCycles = false;
    bool help = false;
    bool compare = false;            // SOC düzeltmeli karşılaştırma (10. adım)
    bool comparePoints = false;      // karşılaştırmada düzeltme noktalarını da yaz
};

void printUsage()
{
    std::printf(
        "Kullanim: hea_cli [secenekler]\n"
        "Varsayilan: sehir_101 cevrimi model tabanli stratejiyle canli akar\n"
        "(10 kat hizli), sonunda ozet tablo gosterilir.\n\n"
        "  --cevrim <ad>      Calistirilacak cevrim: bir cevrim adi (varsayilan sehir_101),\n"
        "                     'egitim', 'test' veya 'hepsi'\n"
        "  --strateji <ad>    ice | ev | kural | model (varsayilan) | hepsi\n"
        "  --hiz <kat>        Canli akis hizi, simulasyon sn / gercek sn (varsayilan 10)\n"
        "  --hizli            Beklemeden yaz (canli akis yok)\n"
        "  --ozet             Adimlari yazma, yalnizca ozet tablo\n"
        "  --adimlar          Birden cok kosuda da adimlari yaz (normalde yalnizca ozet)\n"
        "  --aralik <s>       Her <s> saniyede bir adim satiri (varsayilan 1)\n"
        "  --csv <dosya>      Tum adim sonuclarini CSV dosyasina yaz (SI birimleri)\n"
        "  --egitim-verisi <dosya>\n"
        "                     Secilen cevrimler icin tuketim modeli egitim verisini\n"
        "                     yaz (kural tabanli surusle, iki modun maliyeti) ve cik\n"
        "  --karsilastir      SOC duzeltmeli karsilastirma tablosu (kurala gore fark)\n"
        "  --noktalar         --karsilastir ile duzeltme noktalarini da goster\n"
        "  --cevrimler        Egitim ve test cevrimlerinin listesini goster\n"
        "  --tablolar         Bilesen kontrol tablolarini goster\n"
        "  --yardim           Bu metni goster\n");
}

// Argümanları okur. Hata varsa mesaj yazıp false döner.
bool parseArgs(int argc, char** argv, Options& opt)
{
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto needValue = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Hata: %s bir deger bekliyor.\n", name);
                return nullptr;
            }
            return argv[++i];
        };
        if (a == "--cevrim") {
            const char* v = needValue("--cevrim");
            if (v == nullptr) return false;
            opt.cycle = v;
        } else if (a == "--strateji") {
            const char* v = needValue("--strateji");
            if (v == nullptr) return false;
            opt.strategy = v;
        } else if (a == "--adimlar") {
            opt.steps = true;
        } else if (a == "--ozet") {
            opt.summaryOnly = true;
        } else if (a == "--hizli") {
            opt.fast = true;
        } else if (a == "--hiz") {
            const char* v = needValue("--hiz");
            if (v == nullptr) return false;
            opt.speed = std::atof(v);
            if (!(opt.speed > 0.0)) {
                std::fprintf(stderr, "Hata: --hiz pozitif olmali.\n");
                return false;
            }
        } else if (a == "--aralik") {
            const char* v = needValue("--aralik");
            if (v == nullptr) return false;
            opt.interval_s = std::atoi(v);
            if (opt.interval_s < 1) {
                std::fprintf(stderr, "Hata: --aralik en az 1 olmali.\n");
                return false;
            }
        } else if (a == "--csv") {
            const char* v = needValue("--csv");
            if (v == nullptr) return false;
            opt.csvPath = v;
        } else if (a == "--egitim-verisi") {
            const char* v = needValue("--egitim-verisi");
            if (v == nullptr) return false;
            opt.trainingPath = v;
        } else if (a == "--karsilastir") {
            opt.compare = true;
        } else if (a == "--noktalar") {
            opt.comparePoints = true;
        } else if (a == "--cevrimler") {
            opt.listCycles = true;
        } else if (a == "--tablolar") {
            opt.componentTables = true;
        } else if (a == "--yardim" || a == "-h" || a == "--help") {
            opt.help = true;
        } else {
            std::fprintf(stderr, "Hata: bilinmeyen secenek '%s'.\n", a.c_str());
            return false;
        }
    }
    return true;
}

// Seçilen çevrimleri döndürür; ad bulunamazsa boş liste.
std::vector<hea::DriveCycle> selectCycles(const std::string& name)
{
    const std::vector<hea::DriveCycle> train = hea::trainingCycles();
    const std::vector<hea::DriveCycle> test = hea::testCycles();
    if (name == "egitim") return train;
    if (name == "test") return test;
    std::vector<hea::DriveCycle> all = train;
    all.insert(all.end(), test.begin(), test.end());
    if (name == "hepsi") return all;
    for (const hea::DriveCycle& c : all) {
        if (c.name == name) return {c};
    }
    return {};
}

// Seçilen stratejileri oluşturur; ad bilinmiyorsa boş liste.
std::vector<std::unique_ptr<hea::Strategy>> selectStrategies(const std::string& name)
{
    std::vector<std::unique_ptr<hea::Strategy>> out;
    const bool all = name == "hepsi";
    if (all || name == "ice") out.push_back(std::make_unique<hea::FixedModeStrategy>(hea::Mode::Engine));
    if (all || name == "ev") out.push_back(std::make_unique<hea::FixedModeStrategy>(hea::Mode::Electric));
    if (all || name == "kural") out.push_back(std::make_unique<hea::RuleBasedStrategy>());
    if (all || name == "model") out.push_back(std::make_unique<hea::ModelBasedStrategy>());
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    Options opt;
    if (!parseArgs(argc, argv, opt)) {
        printUsage();
        return kExitUsage;
    }
    if (opt.help) {
        printUsage();
        return kExitOk;
    }

    std::printf("Akilli Hibrit Enerji Asistani (core %s)\n", hea::coreVersion());

    if (opt.componentTables) {
        cli::printComponentTables();
    }
    if (opt.listCycles) {
        cli::printCycleTable("EGITIM CEVRIMLERI", hea::trainingCycles());
        cli::printCycleTable("TEST CEVRIMLERI", hea::testCycles());
    }
    if (opt.componentTables || opt.listCycles) {
        // Yalnızca tablo istendiyse simülasyon çalıştırma (seçenek de verilmediyse)
        bool onlyTables = true;
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            if (a != "--tablolar" && a != "--cevrimler") onlyTables = false;
        }
        if (onlyTables) return kExitOk;
    }

    const std::vector<hea::DriveCycle> cycles = selectCycles(opt.cycle);
    if (cycles.empty()) {
        std::fprintf(stderr, "Hata: '%s' adinda cevrim yok. Liste icin --cevrimler.\n",
                     opt.cycle.c_str());
        return kExitUsage;
    }
    // Eğitim verisi: durumlar kural tabanlı sürüşle oluşur, her tahrik anında
    // iki modun maliyeti ayrıca hesaplanır (bkz. hea::buildTrainingSamples).
    if (!opt.trainingPath.empty()) {
        std::vector<hea::TrainingSample> samples;
        for (const hea::DriveCycle& c : cycles) {
            hea::RuleBasedStrategy driver;
            const auto part = hea::buildTrainingSamples(hea::SimulationConfig{}, c, driver);
            samples.insert(samples.end(), part.begin(), part.end());
            std::printf("%-11s %5zu ornek\n", c.name.c_str(), part.size());
        }
        if (!cli::writeTrainingCsv(opt.trainingPath, samples)) {
            std::fprintf(stderr, "Hata: '%s' dosyasina yazilamadi.\n", opt.trainingPath.c_str());
            return kExitFileError;
        }
        std::printf("Egitim verisi yazildi: %s (%zu ornek)\n", opt.trainingPath.c_str(),
                    samples.size());
        return kExitOk;
    }

    std::vector<std::unique_ptr<hea::Strategy>> strategies = selectStrategies(opt.strategy);
    if (strategies.empty()) {
        std::fprintf(stderr, "Hata: '%s' adinda strateji yok (ice | ev | kural | model | hepsi).\n",
                     opt.strategy.c_str());
        return kExitUsage;
    }

    const hea::SimulationConfig cfg;

    // SOC düzeltmeli karşılaştırma: her çevrim ve strateji için ham koşu +
    // 6 başlangıç SOC'si ile doğrusal düzeltme (bkz. hea/comparison.h)
    if (opt.compare) {
        std::vector<cli::ComparisonRow> rows;
        for (const hea::DriveCycle& c : cycles) {
            for (const auto& strategy : strategies) {
                cli::ComparisonRow row;
                row.cycle = c.name;
                row.strategy = strategy->name();
                row.raw = hea::summarize(hea::runCycle(cfg, c, *strategy), cfg.initialSoc);
                row.rawStoredEnergy_J = hea::storedEnergyReleased_J(
                    cfg.battery, row.raw.socStart, row.raw.socEnd);
                row.corrected = hea::socCorrectedFuel(cfg, c, *strategy);
                rows.push_back(std::move(row));
            }
        }
        if (opt.comparePoints) {
            cli::printCorrectionPoints(rows);
        }
        cli::printComparisonTable(rows, "kural-tabanli");
        return kExitOk;
    }

    // Adımlar yazılsın mı? Tek koşuda varsayılan olarak evet (canlı akış);
    // birden çok koşuda yalnızca --adimlar ile (yoksa çıktı on binlerce satır olur).
    const bool singleRun = cycles.size() * strategies.size() == 1;
    const bool printSteps = !opt.summaryOnly && (singleRun || opt.steps);
    // Çıktı bir dosyaya/boruya gidiyorsa beklemenin anlamı yok (ör. CTest, > dosya)
    const bool toTerminal = HEA_ISATTY(HEA_FILENO(stdout)) != 0;
    const bool live = printSteps && !opt.fast && toTerminal;
    if (live) {
        std::printf("Canli akis: %.0f kat hizli (beklemesiz: --hizli, yalnizca ozet: --ozet)\n",
                    opt.speed);
    }
    constexpr int kHeaderEvery = 30; // okunabilirlik için başlık tekrarı (satır)

    std::vector<cli::RunRecord> runs;
    for (const hea::DriveCycle& c : cycles) {
        for (const auto& strategy : strategies) {
            cli::RunRecord run;
            run.cycle = c.name;
            run.strategy = strategy->name();
            if (printSteps) {
                std::printf("\n=== %s / %s ===\n", run.cycle.c_str(), run.strategy.c_str());
            }
            // Simülasyon adım adım ilerletilir; her StepResult üretildiği anda yazılır
            hea::Simulation sim(cfg, c, *strategy);
            int printed = 0;
            // Canlı akış saati: her satır, çevrim başından beri geçmesi gereken mutlak
            // zamana kadar bekler (sleep_until). Tek tek bekleme (sleep_for) kullanılsaydı
            // Windows zamanlayıcısının ~15.6 ms çözünürlüğü yüzünden gecikmeler birikirdi.
            const auto liveStart = std::chrono::steady_clock::now();
            const double cycleStart_s = c.samples.empty() ? 0.0 : c.samples.front().time_s;
            while (!sim.finished()) {
                const hea::StepResult r = sim.step();
                run.steps.push_back(r);
                if (!printSteps || std::lround(r.time_s) % opt.interval_s != 0) {
                    continue;
                }
                if (printed % kHeaderEvery == 0) {
                    cli::printStepHeader();
                }
                cli::printStep(r);
                ++printed;
                if (live) {
                    std::fflush(stdout);
                    // Hedef: (simülasyon zamanı / hız) kadar gerçek zaman geçmiş olsun
                    const double target_s = (r.time_s - cycleStart_s) / opt.speed;
                    std::this_thread::sleep_until(
                        liveStart + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                        std::chrono::duration<double>(target_s)));
                }
            }
            run.summary = hea::summarize(run.steps, cfg.initialSoc);
            runs.push_back(std::move(run));
        }
    }

    cli::printSummaryTable(runs);

    if (!opt.csvPath.empty()) {
        if (!cli::writeStepsCsv(opt.csvPath, runs)) {
            std::fprintf(stderr, "Hata: '%s' dosyasina yazilamadi.\n", opt.csvPath.c_str());
            return kExitFileError;
        }
        std::size_t rows = 0;
        for (const cli::RunRecord& r : runs) rows += r.steps.size();
        std::printf("\nCSV yazildi: %s (%zu satir)\n", opt.csvPath.c_str(), rows);
    }
    return kExitOk;
}
