// Akıllı Hibrit Enerji Asistanı — terminal uygulaması.
// Core'dan gelen StepResult verilerini ekrana / CSV'ye yazar. Hesap yapmaz.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

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
    std::string cycle = "test";      // çevrim adı ya da egitim / test / hepsi
    std::string strategy = "hepsi";  // ice / ev / kural / hepsi
    bool steps = false;              // adım adım çıktı
    int interval_s = 1;              // adım çıktısında kaç saniyede bir satır
    std::string csvPath;             // boş değilse CSV yaz
    std::string trainingPath;        // boş değilse eğitim verisi CSV'si yaz ve çık
    bool componentTables = false;
    bool listCycles = false;
    bool help = false;
};

void printUsage()
{
    std::printf(
        "Kullanim: hea_cli [secenekler]\n"
        "  --cevrim <ad>      Calistirilacak cevrim: bir cevrim adi (or. sehir_101),\n"
        "                     'egitim', 'test' (varsayilan) veya 'hepsi'\n"
        "  --strateji <ad>    ice | ev | kural | model | hepsi (varsayilan)\n"
        "  --adimlar          Her adimi ekrana yaz\n"
        "  --aralik <s>       --adimlar ile her <s> saniyede bir satir (varsayilan 1)\n"
        "  --csv <dosya>      Tum adim sonuclarini CSV dosyasina yaz (SI birimleri)\n"
        "  --egitim-verisi <dosya>\n"
        "                     Secilen cevrimler icin tuketim modeli egitim verisini\n"
        "                     yaz (kural tabanli surusle, iki modun maliyeti) ve cik\n"
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
    std::vector<cli::RunRecord> runs;
    for (const hea::DriveCycle& c : cycles) {
        for (const auto& strategy : strategies) {
            cli::RunRecord run;
            run.cycle = c.name;
            run.strategy = strategy->name();
            run.steps = hea::runCycle(cfg, c, *strategy);
            run.summary = hea::summarize(run.steps, cfg.initialSoc);

            if (opt.steps) {
                std::printf("\n=== %s / %s ===\n", run.cycle.c_str(), run.strategy.c_str());
                cli::printStepHeader();
                for (const hea::StepResult& r : run.steps) {
                    const long t = std::lround(r.time_s);
                    if (t % opt.interval_s == 0) {
                        cli::printStep(r);
                    }
                }
            }
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
