// Akıllı Hibrit Enerji Asistanı — terminal uygulaması.
// Core'dan gelen StepResult verilerini ekrana / CSV'ye yazar. Hesap yapmaz.

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
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
#include "hea/config_file.h"
#include "hea/cycle_io.h"
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
constexpr int kExitRuntime = 3;

struct Options {
    // Varsayılan: tek çevrim + tek strateji, canlı akış
    std::string cycle = "sehir_101"; // çevrim adı ya da egitim / test / standart / hepsi
    bool cycleGiven = false;         // --cevrim açıkça verildi mi?
    std::vector<std::string> cycleFiles; // --cevrim-dosyasi ile okunacak CSV'ler
    std::string strategy = "model";  // ice / ev / kural / model / hepsi
    bool steps = false;              // adım çıktısını zorla (çoklu koşuda da)
    bool summaryOnly = false;        // adım çıktısı yok, yalnızca özet
    double speed = 10.0;             // canlı akış hızı: simülasyon saniyesi / gerçek saniye
    bool fast = false;               // canlı akışta bekleme yapma
    int interval_s = 1;              // adım çıktısında kaç saniyede bir satır
    std::string csvPath;             // boş değilse adım CSV'si yaz
    std::string summaryCsvPath;      // boş değilse özet CSV'si yaz
    std::string trainingPath;        // boş değilse eğitim verisi CSV'si yaz ve çık
    std::string trainingDriver = "kural"; // eğitim verisini üreten sürüş: kural / hepsi
    bool componentTables = false;
    bool listCycles = false;
    bool help = false;
    bool compare = false;            // SOC düzeltmeli karşılaştırma
    bool comparePoints = false;      // karşılaştırmada düzeltme noktalarını da yaz
    std::string compareCsvPath;      // boş değilse karşılaştırma CSV'si yaz
    std::string configPath;          // --ayar-dosyasi
    std::vector<std::string> assignments; // --ayar anahtar=deger (dosyadan sonra uygulanır)
    std::string writeConfigPath;     // --ayarlari-yaz: geçerli ayarları dosyaya yaz ve çık
    bool runRequested = false;       // simülasyonla ilgili bir seçenek verildi mi?
};

void printUsage()
{
    std::printf(
        "Kullanim: hea_cli [secenekler]\n"
        "Varsayilan: sehir_101 cevrimi model tabanli stratejiyle canli akar\n"
        "(10 kat hizli), sonunda ozet tablo gosterilir.\n\n"
        "Cevrim ve strateji:\n"
        "  --cevrim <ad>      Bir cevrim adi (varsayilan sehir_101, 'nedc' dahil) ya da grup:\n"
        "                     'egitim', 'test', 'standart' veya 'hepsi'\n"
        "  --cevrim-dosyasi <dosya.csv>\n"
        "                     CSV'den cevrim oku (time_s,speed_kmh[,grade_percent]\n"
        "                     [,payload_kg][,ambient_C]); birden cok kez verilebilir\n"
        "  --strateji <ad>    ice | ev | kural | model (varsayilan) | hepsi\n\n"
        "Parametreler:\n"
        "  --ayar-dosyasi <dosya>  Yapilandirma dosyasini oku (INI; ornek: data/ornek_ayar.ini)\n"
        "  --ayar <anahtar=deger>  Tek parametre degistir, ornek: --ayar batarya.kapasite_Ah=8\n"
        "  --soc0 <deger>          Baslangic SOC'si [0..1] (= --ayar simulasyon.baslangic_soc=...)\n"
        "  --ayarlari-yaz <dosya>  Gecerli tum ayarlari aciklamalariyla dosyaya yaz ve cik\n\n"
        "Cikti:\n"
        "  --hiz <kat>        Canli akis hizi, simulasyon sn / gercek sn (varsayilan 10)\n"
        "  --hizli            Beklemeden yaz (canli akis yok)\n"
        "  --ozet             Adimlari yazma, yalnizca ozet tablo\n"
        "  --adimlar          Birden cok kosuda da adimlari yaz (normalde yalnizca ozet)\n"
        "  --aralik <s>       Her <s> saniyede bir adim satiri (varsayilan 1)\n"
        "  --csv <dosya>      Tum adim sonuclarini CSV dosyasina yaz (SI birimleri)\n"
        "  --ozet-csv <dosya> Kosu ozetlerini CSV dosyasina yaz\n"
        "  --karsilastir      SOC duzeltmeli karsilastirma tablosu (kurala gore fark)\n"
        "  --noktalar         --karsilastir ile duzeltme noktalarini da goster\n"
        "  --karsilastir-csv <dosya>\n"
        "                     Karsilastirma sonuclarini CSV'ye yaz (--karsilastir ile)\n\n"
        "Model egitimi:\n"
        "  --egitim-verisi <dosya>\n"
        "                     Secilen cevrimler icin tuketim modeli egitim verisini\n"
        "                     yaz (iki modun maliyeti) ve cik\n"
        "  --egitim-surucu <ad>\n"
        "                     Egitim verisindeki durumlari ureten surus: kural (varsayilan)\n"
        "                     ya da hepsi (kural + model + EV-oncelikli; daha cesitli veri)\n\n"
        "Bilgi:\n"
        "  --cevrimler        Egitim, test ve standart cevrimlerin listesini goster\n"
        "  --tablolar         Bilesen kontrol tablolarini goster\n"
        "  --yardim           Bu metni goster\n\n"
        "Cikis kodlari: 0 basarili, 1 dosya hatasi, 2 hatali kullanim/ayar, 3 calisma hatasi\n");
}

// Sıkı sayı okuma: tüm metin sayı olmalı ("5abc" reddedilir).
bool parseDouble(const char* text, double& out)
{
    errno = 0;
    char* end = nullptr;
    const double v = std::strtod(text, &end);
    if (end == text || *end != '\0' || errno == ERANGE || !std::isfinite(v)) {
        return false;
    }
    out = v;
    return true;
}

bool parseInt(const char* text, int& out)
{
    double v = 0.0;
    if (!parseDouble(text, v) || v != std::floor(v) || std::fabs(v) > 1e9) {
        return false;
    }
    out = static_cast<int>(v);
    return true;
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
        // Yalnızca değer alan seçenekler için kısa yol
        auto takeString = [&](const char* name, std::string& dst, bool run) {
            const char* v = needValue(name);
            if (v == nullptr) return false;
            dst = v;
            opt.runRequested = opt.runRequested || run;
            return true;
        };
        if (a == "--cevrim") {
            if (!takeString("--cevrim", opt.cycle, true)) return false;
            opt.cycleGiven = true;
        } else if (a == "--cevrim-dosyasi") {
            std::string path;
            if (!takeString("--cevrim-dosyasi", path, true)) return false;
            opt.cycleFiles.push_back(path);
        } else if (a == "--strateji") {
            if (!takeString("--strateji", opt.strategy, true)) return false;
        } else if (a == "--adimlar") {
            opt.steps = true;
            opt.runRequested = true;
        } else if (a == "--ozet") {
            opt.summaryOnly = true;
            opt.runRequested = true;
        } else if (a == "--hizli") {
            opt.fast = true;
            opt.runRequested = true;
        } else if (a == "--hiz") {
            const char* v = needValue("--hiz");
            if (v == nullptr) return false;
            if (!parseDouble(v, opt.speed) || !(opt.speed > 0.0)) {
                std::fprintf(stderr, "Hata: --hiz pozitif bir sayi olmali ('%s').\n", v);
                return false;
            }
            opt.runRequested = true;
        } else if (a == "--aralik") {
            const char* v = needValue("--aralik");
            if (v == nullptr) return false;
            if (!parseInt(v, opt.interval_s) || opt.interval_s < 1) {
                std::fprintf(stderr, "Hata: --aralik en az 1 olan bir tam sayi olmali ('%s').\n", v);
                return false;
            }
            opt.runRequested = true;
        } else if (a == "--csv") {
            if (!takeString("--csv", opt.csvPath, true)) return false;
        } else if (a == "--ozet-csv") {
            if (!takeString("--ozet-csv", opt.summaryCsvPath, true)) return false;
        } else if (a == "--egitim-verisi") {
            if (!takeString("--egitim-verisi", opt.trainingPath, true)) return false;
        } else if (a == "--egitim-surucu") {
            if (!takeString("--egitim-surucu", opt.trainingDriver, true)) return false;
            if (opt.trainingDriver != "kural" && opt.trainingDriver != "hepsi") {
                std::fprintf(stderr, "Hata: --egitim-surucu kural ya da hepsi olmali.\n");
                return false;
            }
        } else if (a == "--karsilastir") {
            opt.compare = true;
            opt.runRequested = true;
        } else if (a == "--noktalar") {
            opt.comparePoints = true;
            opt.runRequested = true;
        } else if (a == "--karsilastir-csv") {
            if (!takeString("--karsilastir-csv", opt.compareCsvPath, true)) return false;
        } else if (a == "--ayar-dosyasi") {
            if (!takeString("--ayar-dosyasi", opt.configPath, false)) return false;
        } else if (a == "--ayar") {
            std::string kv;
            if (!takeString("--ayar", kv, false)) return false;
            opt.assignments.push_back(kv);
        } else if (a == "--soc0") {
            const char* v = needValue("--soc0");
            if (v == nullptr) return false;
            double soc = 0.0;
            if (!parseDouble(v, soc)) {
                std::fprintf(stderr, "Hata: --soc0 bir sayi olmali ('%s').\n", v);
                return false;
            }
            opt.assignments.push_back(std::string("simulasyon.baslangic_soc=") + v);
        } else if (a == "--ayarlari-yaz") {
            if (!takeString("--ayarlari-yaz", opt.writeConfigPath, false)) return false;
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
    if (!opt.compareCsvPath.empty() && !opt.compare) {
        std::fprintf(stderr, "Hata: --karsilastir-csv yalnizca --karsilastir ile kullanilir.\n");
        return false;
    }
    return true;
}

// Seçilen çevrimleri döndürür; ad bulunamazsa boş liste.
std::vector<hea::DriveCycle> selectCycles(const std::string& name)
{
    const std::vector<hea::DriveCycle> train = hea::trainingCycles();
    const std::vector<hea::DriveCycle> test = hea::testCycles();
    const std::vector<hea::DriveCycle> standard = hea::standardCycles();
    if (name == "egitim") return train;
    if (name == "test") return test;
    if (name == "standart") return standard;
    std::vector<hea::DriveCycle> all = train;
    all.insert(all.end(), test.begin(), test.end());
    all.insert(all.end(), standard.begin(), standard.end());
    if (name == "hepsi") return all;
    for (const hea::DriveCycle& c : all) {
        if (c.name == name) return {c};
    }
    return {};
}

// Seçilen stratejileri oluşturur; ad bilinmiyorsa boş liste.
std::vector<std::unique_ptr<hea::Strategy>> selectStrategies(const std::string& name,
                                                             const hea::FullConfig& cfg)
{
    std::vector<std::unique_ptr<hea::Strategy>> out;
    const bool all = name == "hepsi";
    if (all || name == "ice") out.push_back(std::make_unique<hea::FixedModeStrategy>(hea::Mode::Engine));
    if (all || name == "ev") out.push_back(std::make_unique<hea::FixedModeStrategy>(hea::Mode::Electric));
    if (all || name == "kural") out.push_back(std::make_unique<hea::RuleBasedStrategy>(cfg.rule));
    if (all || name == "model") out.push_back(std::make_unique<hea::ModelBasedStrategy>(cfg.model));
    return out;
}

int run(const Options& opt)
{
    std::printf("Akilli Hibrit Enerji Asistani (core %s)\n", hea::coreVersion());

    // --- Yapılandırma: varsayılanlar <- dosya <- --ayar / --soc0 ---
    hea::FullConfig full;
    if (!opt.configPath.empty()) {
        hea::loadConfigFile(opt.configPath, full); // hata: runtime_error (dosya:satir)
        std::printf("Ayar dosyasi okundu: %s\n", opt.configPath.c_str());
    }
    for (const std::string& kv : opt.assignments) {
        try {
            hea::applyConfigAssignment(full, kv);
        } catch (const std::invalid_argument& e) {
            std::fprintf(stderr, "Hata: --ayar %s\n", e.what());
            return kExitUsage;
        }
    }
    const std::string configError = full.validate();
    if (!configError.empty()) {
        std::fprintf(stderr, "Hata: gecersiz ayar: %s\n", configError.c_str());
        return kExitUsage;
    }
    if (!opt.writeConfigPath.empty()) {
        std::ofstream f(opt.writeConfigPath, std::ios::binary);
        f << hea::configToText(full);
        if (!f) {
            std::fprintf(stderr, "Hata: '%s' dosyasina yazilamadi.\n", opt.writeConfigPath.c_str());
            return kExitFileError;
        }
        std::printf("Ayarlar yazildi: %s\n", opt.writeConfigPath.c_str());
    }

    if (opt.componentTables) {
        cli::printComponentTables();
    }
    if (opt.listCycles) {
        cli::printCycleTable("EGITIM CEVRIMLERI", hea::trainingCycles());
        cli::printCycleTable("TEST CEVRIMLERI", hea::testCycles());
        cli::printCycleTable("STANDART CEVRIMLER", hea::standardCycles());
    }
    // Yalnızca bilgi istendiyse (tablo, liste, ayar yazma) simülasyon çalıştırma
    const bool infoOnly = opt.componentTables || opt.listCycles || !opt.writeConfigPath.empty();
    if (infoOnly && !opt.runRequested) {
        return kExitOk;
    }

    // --- Çevrimler: dosyadan okunanlar + (verildiyse ya da dosya yoksa) adla seçilenler ---
    std::vector<hea::DriveCycle> cycles;
    for (const std::string& path : opt.cycleFiles) {
        cycles.push_back(hea::loadCycleCsv(path)); // hata: runtime_error (dosya:satir)
        std::printf("Cevrim okundu: %s (%s, %.0f s)\n", path.c_str(), cycles.back().name.c_str(),
                    cycles.back().duration_s());
    }
    if (opt.cycleGiven || opt.cycleFiles.empty()) {
        const std::vector<hea::DriveCycle> named = selectCycles(opt.cycle);
        if (named.empty()) {
            std::fprintf(stderr, "Hata: '%s' adinda cevrim yok. Liste icin --cevrimler.\n",
                         opt.cycle.c_str());
            return kExitUsage;
        }
        cycles.insert(cycles.end(), named.begin(), named.end());
    }

    const hea::SimulationConfig& cfg = full.sim;

    // Eğitim verisi: durumlar seçilen sürüşle oluşur, her tahrik anında iki modun
    // maliyeti ayrıca hesaplanır (bkz. hea::buildTrainingSamples).
    if (!opt.trainingPath.empty()) {
        std::vector<hea::TrainingSample> samples;
        for (const hea::DriveCycle& c : cycles) {
            std::vector<std::unique_ptr<hea::Strategy>> drivers;
            drivers.push_back(std::make_unique<hea::RuleBasedStrategy>(full.rule));
            if (opt.trainingDriver == "hepsi") {
                drivers.push_back(std::make_unique<hea::ModelBasedStrategy>(full.model));
                drivers.push_back(std::make_unique<hea::FixedModeStrategy>(hea::Mode::Electric));
            }
            for (const auto& driver : drivers) {
                const auto part = hea::buildTrainingSamples(cfg, c, *driver);
                samples.insert(samples.end(), part.begin(), part.end());
                std::printf("%-15s %-13s %5zu ornek\n", c.name.c_str(), driver->name(),
                            part.size());
            }
        }
        if (!cli::writeTrainingCsv(opt.trainingPath, samples)) {
            std::fprintf(stderr, "Hata: '%s' dosyasina yazilamadi.\n", opt.trainingPath.c_str());
            return kExitFileError;
        }
        std::printf("Egitim verisi yazildi: %s (%zu ornek)\n", opt.trainingPath.c_str(),
                    samples.size());
        return kExitOk;
    }

    std::vector<std::unique_ptr<hea::Strategy>> strategies = selectStrategies(opt.strategy, full);
    if (strategies.empty()) {
        std::fprintf(stderr, "Hata: '%s' adinda strateji yok (ice | ev | kural | model | hepsi).\n",
                     opt.strategy.c_str());
        return kExitUsage;
    }

    // SOC düzeltmeli karşılaştırma: her çevrim ve strateji için ham koşu +
    // defaultCorrectionSocs() (9 başlangıç SOC'si) ile doğrusal düzeltme (bkz. hea/comparison.h)
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
        if (!opt.compareCsvPath.empty()) {
            if (!cli::writeComparisonCsv(opt.compareCsvPath, rows)) {
                std::fprintf(stderr, "Hata: '%s' dosyasina yazilamadi.\n",
                             opt.compareCsvPath.c_str());
                return kExitFileError;
            }
            std::printf("\nKarsilastirma CSV'si yazildi: %s (%zu satir)\n",
                        opt.compareCsvPath.c_str(), rows.size());
        }
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
    if (!opt.summaryCsvPath.empty()) {
        if (!cli::writeSummaryCsv(opt.summaryCsvPath, runs)) {
            std::fprintf(stderr, "Hata: '%s' dosyasina yazilamadi.\n", opt.summaryCsvPath.c_str());
            return kExitFileError;
        }
        std::printf("\nOzet CSV'si yazildi: %s (%zu satir)\n", opt.summaryCsvPath.c_str(),
                    runs.size());
    }
    return kExitOk;
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
    // Core hatalarını (geçersiz parametre, okunamayan dosya) ekrana yazmaz,
    // istisna olarak bildirir; burada kullanıcıya anlaşılır mesaja çevrilir.
    try {
        return run(opt);
    } catch (const std::invalid_argument& e) {
        std::fprintf(stderr, "Hata: %s\n", e.what());
        return kExitUsage;
    } catch (const std::runtime_error& e) {
        std::fprintf(stderr, "Hata: %s\n", e.what());
        return kExitFileError;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Beklenmeyen hata: %s\n", e.what());
        return kExitRuntime;
    }
}
