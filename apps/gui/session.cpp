#include "session.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <utility>

#include "hea/cycle_io.h"
#include "hea/model_based_strategy.h"
#include "hea/rule_based_strategy.h"
#include "result_csv.h"

namespace gui {

namespace fs = std::filesystem;

namespace {

fs::path pathFromUtf8(const std::string& utf8)
{
    return fs::u8path(utf8);
}

} // namespace

StrategyKind strategyFromIndex(int i)
{
    switch (i) {
    case 0: return StrategyKind::IceOnly;
    case 1: return StrategyKind::EvFirst;
    case 2: return StrategyKind::RuleBased;
    default: return StrategyKind::ModelBased;
    }
}

int strategyIndex(StrategyKind k)
{
    switch (k) {
    case StrategyKind::IceOnly: return 0;
    case StrategyKind::EvFirst: return 1;
    case StrategyKind::RuleBased: return 2;
    case StrategyKind::ModelBased: return 3;
    }
    return 3;
}

const char* strategyLabel(StrategyKind k)
{
    switch (k) {
    case StrategyKind::IceOnly: return "Sadece benzinli motor (ICE)";
    case StrategyKind::EvFirst: return "Elektrik öncelikli (EV)";
    case StrategyKind::RuleBased: return "Kural tabanlı";
    case StrategyKind::ModelBased: return "Model tabanlı (ECMS)";
    }
    return "";
}

const char* strategyDescription(StrategyKind k)
{
    switch (k) {
    case StrategyKind::IceOnly:
        return "Hep benzinli motorla gider; elektriği yalnızca frende geri kazanır. "
               "Kıyas için en kötü durum.";
    case StrategyKind::EvFirst:
        return "Mümkün olduğunca elektrikle gider; batarya bitince motora geçer. "
               "Bataryayı bilerek doldurmaz.";
    case StrategyKind::RuleBased:
        return "Mühendisin elle yazdığı kurallar: hıza, güce ve batarya doluluğuna göre "
               "elektrik ya da motor.";
    case StrategyKind::ModelBased:
        return "Her saniye elektrik, motor ve motor + şarj seçeneklerinin maliyetini "
               "eğitilmiş modelle tahmin eder, en ucuzunu seçer.";
    }
    return "";
}

bool strategyFromName(const std::string& name, StrategyKind& out)
{
    if (name == "ice") out = StrategyKind::IceOnly;
    else if (name == "ev") out = StrategyKind::EvFirst;
    else if (name == "kural") out = StrategyKind::RuleBased;
    else if (name == "model") out = StrategyKind::ModelBased;
    else return false;
    return true;
}

std::unique_ptr<hea::Strategy> makeStrategy(StrategyKind k, const hea::FullConfig& cfg)
{
    switch (k) {
    case StrategyKind::IceOnly:
        return std::make_unique<hea::FixedModeStrategy>(hea::Mode::Engine);
    case StrategyKind::EvFirst:
        return std::make_unique<hea::FixedModeStrategy>(hea::Mode::Electric);
    case StrategyKind::RuleBased:
        return std::make_unique<hea::RuleBasedStrategy>(cfg.rule);
    case StrategyKind::ModelBased:
        return std::make_unique<hea::ModelBasedStrategy>(cfg.model);
    }
    return nullptr;
}

std::string fileNameOf(const std::string& utf8Path)
{
    const auto slash = utf8Path.find_last_of("/\\");
    return slash == std::string::npos ? utf8Path : utf8Path.substr(slash + 1);
}

Session::Session()
{
    for (hea::DriveCycle& c : hea::testCycles()) cycles_.push_back({"Test", std::move(c)});
    for (hea::DriveCycle& c : hea::trainingCycles()) cycles_.push_back({"Eğitim", std::move(c)});
    for (hea::DriveCycle& c : hea::standardCycles()) cycles_.push_back({"Standart", std::move(c)});
}

int Session::findCycle(const std::string& name) const
{
    for (std::size_t i = 0; i < cycles_.size(); ++i) {
        if (cycles_[i].cycle.name == name) return static_cast<int>(i);
    }
    return -1;
}

int Session::defaultCycleIndex() const
{
    const int i = findCycle("sehir_101");
    return i >= 0 ? i : 0;
}

std::string Session::addCycleFile(const std::string& utf8Path, int& index)
{
    try {
        std::ifstream f(pathFromUtf8(utf8Path));
        if (!f) {
            return fileNameOf(utf8Path) + ": dosya açılamadı";
        }
        // Hata mesajlarında tam yol yerine dosya adı (kısa ve okunur)
        hea::DriveCycle c = hea::loadCycleCsv(f, fileNameOf(utf8Path));
        for (std::size_t i = 0; i < cycles_.size(); ++i) {
            if (cycles_[i].group == "Dosya" && cycles_[i].cycle.name == c.name) {
                cycles_[i].cycle = std::move(c);
                index = static_cast<int>(i);
                return {};
            }
        }
        cycles_.push_back({"Dosya", std::move(c)});
        index = static_cast<int>(cycles_.size() - 1);
        return {};
    } catch (const std::exception& e) {
        return e.what();
    }
}

std::string Session::loadConfigFile(const std::string& utf8Path)
{
    try {
        std::ifstream f(pathFromUtf8(utf8Path));
        if (!f) {
            return fileNameOf(utf8Path) + ": dosya açılamadı";
        }
        // hea_cli ile aynı anlam: varsayılanlar <- dosya
        hea::FullConfig fresh;
        hea::loadConfigFile(f, fileNameOf(utf8Path), fresh);
        const std::string err = fresh.validate();
        if (!err.empty()) {
            return fileNameOf(utf8Path) + ": geçersiz ayar: " + err;
        }
        config_ = fresh;
        configSource_ = fileNameOf(utf8Path);
        return {};
    } catch (const std::exception& e) {
        return e.what();
    }
}

void Session::resetConfig()
{
    config_ = hea::FullConfig{};
    configSource_ = "varsayılan";
}

hea::DriveCycle Session::applyOverrides(const hea::DriveCycle& c, const RunSettings& s)
{
    hea::DriveCycle out = c;
    for (hea::DriveInput& in : out.samples) {
        if (s.overrideAmbient) in.ambientTemp_C = s.ambient_C;
        if (s.overridePayload) in.payload_kg = s.payload_kg;
    }
    return out;
}

std::string Session::prepare(const RunSettings& settings, hea::DriveCycle& cycleOut,
                             std::unique_ptr<hea::Strategy>& strategyOut) const
{
    if (settings.cycleIndex < 0 || settings.cycleIndex >= static_cast<int>(cycles_.size())) {
        return "geçersiz çevrim seçimi";
    }
    if (settings.overridePayload && !(settings.payload_kg >= 0.0)) {
        return "yük negatif olamaz";
    }
    const std::string err = config_.validate();
    if (!err.empty()) {
        return "geçersiz ayar: " + err;
    }
    try {
        hea::DriveCycle cycle =
            applyOverrides(cycles_[static_cast<std::size_t>(settings.cycleIndex)].cycle, settings);
        std::unique_ptr<hea::Strategy> strategy = makeStrategy(settings.strategy, config_);
        cycleOut = std::move(cycle);
        strategyOut = std::move(strategy);
        return {};
    } catch (const std::exception& e) {
        return e.what();
    }
}

std::string suggestedStepsFileName(const RunResult& r)
{
    return r.cycleName + "_" + r.strategyName + "_adimlar.csv";
}

std::string suggestedComparisonFileName(const ComparisonResult& c)
{
    return c.cycleName + "_karsilastirma.csv";
}

namespace {

// Metin kipinde açar (Windows'ta CRLF, hea_cli ile aynı); yazıp kapatır
template <typename WriteFn>
std::string writeTextFile(const std::string& utf8Path, WriteFn&& write)
{
    try {
        std::ofstream f(pathFromUtf8(utf8Path));
        if (!f) {
            return fileNameOf(utf8Path) + ": dosya yazılamadı";
        }
        write(f);
        f.close();
        if (f.fail()) {
            return fileNameOf(utf8Path) + ": dosya yazılamadı";
        }
        return {};
    } catch (const std::exception& e) {
        return e.what();
    }
}

} // namespace

std::string saveStepsCsv(const std::string& utf8Path, const RunResult& r)
{
    return writeTextFile(utf8Path, [&](std::ostream& out) {
        hea_csv::writeStepsHeader(out);
        hea_csv::writeStepRows(out, r.cycleName, r.strategyName, r.steps);
    });
}

std::string saveComparisonCsv(const std::string& utf8Path, const ComparisonResult& c)
{
    return writeTextFile(utf8Path, [&](std::ostream& out) {
        hea_csv::writeComparisonHeader(out);
        for (const ComparisonEntry& e : c.entries) {
            hea_csv::writeComparisonRow(out, c.cycleName, e.strategyName, e.raw,
                                        e.rawStoredEnergy_J, e.corrected);
        }
    });
}

double ComparisonEntry::equivalentPer100km_L() const
{
    return hea::litersPer100km(hea::equivalentFuel_kg(raw.fuel_kg, rawStoredEnergy_J),
                               raw.distance_m);
}

namespace {

double pctDiff(double v, double ref)
{
    return ref > 0.0 ? 100.0 * (v / ref - 1.0) : 0.0;
}

} // namespace

double ComparisonResult::correctedDiffPct(int i) const
{
    return pctDiff(entries[static_cast<std::size_t>(i)].correctedPer100km_L(),
                   entries[static_cast<std::size_t>(referenceIndex)].correctedPer100km_L());
}

double ComparisonResult::equivalentDiffPct(int i) const
{
    return pctDiff(entries[static_cast<std::size_t>(i)].equivalentPer100km_L(),
                   entries[static_cast<std::size_t>(referenceIndex)].equivalentPer100km_L());
}

std::string Session::compare(const RunSettings& settings, ComparisonResult& out) const
{
    ComparisonResult r;
    for (int i = 0; i < kStrategyCount; ++i) {
        RunSettings s = settings;
        s.strategy = strategyFromIndex(i);
        hea::DriveCycle cycle;
        std::unique_ptr<hea::Strategy> strategy;
        const std::string err = prepare(s, cycle, strategy);
        if (!err.empty()) {
            return err;
        }
        try {
            // hea_cli --karsilastir ile aynı çağrılar, aynı sıra (apps/cli/main.cpp)
            ComparisonEntry e;
            e.kind = s.strategy;
            e.strategyName = strategy->name();
            e.raw = hea::summarize(hea::runCycle(config_.sim, cycle, *strategy),
                                   config_.sim.initialSoc);
            e.rawStoredEnergy_J =
                hea::storedEnergyReleased_J(config_.sim.battery, e.raw.socStart, e.raw.socEnd);
            e.corrected = hea::socCorrectedFuel(config_.sim, cycle, *strategy);
            r.cycleName = cycle.name;
            r.entries.push_back(std::move(e));
        } catch (const std::exception& ex) {
            return ex.what();
        }
    }
    r.initialSoc = config_.sim.initialSoc;
    r.referenceIndex = strategyIndex(StrategyKind::RuleBased);
    r.bestIndex = 0;
    for (int i = 1; i < static_cast<int>(r.entries.size()); ++i) {
        if (r.entries[static_cast<std::size_t>(i)].corrected.fuel_kg()
            < r.entries[static_cast<std::size_t>(r.bestIndex)].corrected.fuel_kg()) {
            r.bestIndex = i;
        }
    }
    out = std::move(r);
    return {};
}

std::string Session::run(const RunSettings& settings, RunResult& out) const
{
    hea::DriveCycle cycle;
    std::unique_ptr<hea::Strategy> strategy;
    const std::string err = prepare(settings, cycle, strategy);
    if (!err.empty()) {
        return err;
    }
    try {
        RunResult r;
        r.cycleName = cycle.name;
        r.strategyName = strategy->name();
        r.initialSoc = config_.sim.initialSoc;
        r.steps = hea::runCycle(config_.sim, cycle, *strategy);
        r.summary = hea::summarize(r.steps, config_.sim.initialSoc);
        out = std::move(r);
        return {};
    } catch (const std::exception& e) {
        return e.what();
    }
}

} // namespace gui
