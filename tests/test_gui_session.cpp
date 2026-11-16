// Arayüz mantığı (gui::Session) ve PNG yazıcı testleri. ImGui gerektirmez.

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "hea/model_based_strategy.h"
#include "hea/rule_based_strategy.h"
#include "live_run.h"
#include "png_writer.h"
#include "session.h"
#include "test_util.h"

using namespace gui;
namespace fs = std::filesystem;

namespace {

void writeText(const fs::path& p, const std::string& text)
{
    std::ofstream f(p, std::ios::binary);
    f << text;
}

std::string toUtf8(const fs::path& p)
{
    const auto s = p.u8string(); // C++17: std::string
    return std::string(s.begin(), s.end());
}

// Test klasörü: Türkçe karakterli (Windows'ta sistem kod sayfası sorunlarını yakalar)
fs::path testDir()
{
    const fs::path d = fs::temp_directory_path() / fs::u8path("hea_gui_testi_çğış");
    fs::create_directories(d);
    return d;
}

void testStrategyNames()
{
    // Dizin <-> tür dönüşümü tutarlı, her türün adı ve açıklaması var
    for (int i = 0; i < kStrategyCount; ++i) {
        const StrategyKind k = strategyFromIndex(i);
        HEA_CHECK(strategyIndex(k) == i);
        HEA_CHECK(std::string(strategyLabel(k)).size() > 3);
        HEA_CHECK(std::string(strategyDescription(k)).size() > 10);
    }
    StrategyKind k = StrategyKind::IceOnly;
    HEA_CHECK(strategyFromName("model", k) && k == StrategyKind::ModelBased);
    HEA_CHECK(strategyFromName("kural", k) && k == StrategyKind::RuleBased);
    HEA_CHECK(strategyFromName("ev", k) && k == StrategyKind::EvFirst);
    HEA_CHECK(strategyFromName("ice", k) && k == StrategyKind::IceOnly);
    HEA_CHECK(!strategyFromName("hepsi", k));

    // Arayüzdeki seçim core'da doğru stratejiyi oluşturur (hea_cli ile aynı adlar)
    const hea::FullConfig cfg;
    HEA_CHECK(std::string(makeStrategy(StrategyKind::IceOnly, cfg)->name()) == "sadece-ICE");
    HEA_CHECK(std::string(makeStrategy(StrategyKind::EvFirst, cfg)->name()) == "EV-oncelikli");
    HEA_CHECK(std::string(makeStrategy(StrategyKind::RuleBased, cfg)->name()) == "kural-tabanli");
    HEA_CHECK(std::string(makeStrategy(StrategyKind::ModelBased, cfg)->name()) == "model-tabanli");
}

void testCycleList()
{
    const Session s;
    // 4 test + 7 eğitim + 1 standart çevrim (hea_cli --cevrimler ile aynı)
    HEA_CHECK(s.cycles().size() == 12);
    HEA_CHECK(s.findCycle("sehir_101") >= 0);
    HEA_CHECK(s.findCycle("nedc") >= 0);
    HEA_CHECK(s.findCycle("otoyol_11") >= 0);
    HEA_CHECK(s.findCycle("yok_boyle") == -1);
    HEA_CHECK(s.cycles()[static_cast<std::size_t>(s.defaultCycleIndex())].cycle.name == "sehir_101");
    HEA_CHECK(s.cycles()[static_cast<std::size_t>(s.findCycle("nedc"))].group == "Standart");
    HEA_CHECK(s.cycles()[static_cast<std::size_t>(s.findCycle("sehir_101"))].group == "Test");
    HEA_CHECK(s.cycles()[static_cast<std::size_t>(s.findCycle("sehir_1"))].group == "Eğitim");
    HEA_CHECK(s.configSource() == "varsayılan");
}

// Arayüzden çalıştırma, core'u doğrudan çalıştırmakla (hea_cli'nin yaptığı) aynı sonucu verir
void testRunMatchesCore()
{
    const Session s;
    RunSettings rs;
    rs.cycleIndex = s.defaultCycleIndex();
    rs.strategy = StrategyKind::ModelBased;
    RunResult r;
    HEA_CHECK(s.run(rs, r).empty());

    const hea::DriveCycle& c = s.cycles()[static_cast<std::size_t>(rs.cycleIndex)].cycle;
    const hea::FullConfig cfg;
    hea::ModelBasedStrategy direct(cfg.model);
    const std::vector<hea::StepResult> steps = hea::runCycle(cfg.sim, c, direct);
    const hea::RunSummary sum = hea::summarize(steps, cfg.sim.initialSoc);

    HEA_CHECK(r.cycleName == "sehir_101");
    HEA_CHECK(r.strategyName == "model-tabanli");
    HEA_CHECK(r.steps.size() == c.samples.size() - 1);
    HEA_CHECK(r.steps.size() == steps.size());
    HEA_CHECK_NEAR(r.summary.fuel_kg, sum.fuel_kg, 0.0);
    HEA_CHECK_NEAR(r.summary.socEnd, sum.socEnd, 0.0);
    HEA_CHECK_NEAR(r.summary.distance_m, sum.distance_m, 0.0);
    HEA_CHECK(r.summary.fuel_kg > 0.0);

    // Aynı ayarlarla ikinci çalıştırma aynı sonucu verir (strateji her seferinde yeni)
    RunResult r2;
    HEA_CHECK(s.run(rs, r2).empty());
    HEA_CHECK_NEAR(r2.summary.fuel_kg, r.summary.fuel_kg, 0.0);
}

void testAllStrategiesRun()
{
    const Session s;
    RunSettings rs;
    rs.cycleIndex = s.findCycle("nedc");
    double evTime = 0.0, iceTime = 0.0;
    for (int i = 0; i < kStrategyCount; ++i) {
        rs.strategy = strategyFromIndex(i);
        RunResult r;
        HEA_CHECK(s.run(rs, r).empty());
        HEA_CHECK(r.steps.size() == 1180);
        HEA_CHECK(std::isfinite(r.summary.fuel_kg));
        if (rs.strategy == StrategyKind::EvFirst) evTime = r.summary.timeElectric;
        if (rs.strategy == StrategyKind::IceOnly) iceTime = r.summary.timeElectric;
    }
    // EV öncelikli strateji, sadece-ICE'den daha uzun süre elektrikle gider
    HEA_CHECK(evTime > iceTime);
}

void testOverrides()
{
    Session s;
    RunSettings rs;
    rs.cycleIndex = s.defaultCycleIndex();

    // Sabit sıcaklık ve yük her adıma uygulanır
    rs.overrideAmbient = true;
    rs.ambient_C = 35.0;
    rs.overridePayload = true;
    rs.payload_kg = 320.0;
    RunResult r;
    HEA_CHECK(s.run(rs, r).empty());
    const double hvac35 = s.config().sim.hvac.power_W(35.0);
    HEA_CHECK(hvac35 > 0.0);
    bool payloadOk = true, auxOk = true;
    for (const hea::StepResult& st : r.steps) {
        payloadOk = payloadOk && st.payload_kg == 320.0;
        auxOk = auxOk
                && std::fabs(st.auxiliaryPower_W - (s.config().sim.auxiliaryPower_W + hvac35)) < 1e-9;
    }
    HEA_CHECK(payloadOk);
    HEA_CHECK(auxOk);

    // Klima kapatılınca yalnızca sabit yardımcı yük kalır
    s.config().sim.hvac.enabled = false;
    HEA_CHECK(s.run(rs, r).empty());
    HEA_CHECK_NEAR(r.steps[10].auxiliaryPower_W, s.config().sim.auxiliaryPower_W, 1e-9);

    // Ağır yükte yakıt artar
    RunResult light, heavy;
    rs.payload_kg = 0.0;
    HEA_CHECK(s.run(rs, light).empty());
    rs.payload_kg = 500.0;
    HEA_CHECK(s.run(rs, heavy).empty());
    HEA_CHECK(heavy.summary.fuel_kg > light.summary.fuel_kg);

    // Başlangıç SOC'si sonuca yansır
    s.config().sim.initialSoc = 0.45;
    HEA_CHECK(s.run(rs, r).empty());
    HEA_CHECK_NEAR(r.summary.socStart, 0.45, 0.0);
    HEA_CHECK_NEAR(r.initialSoc, 0.45, 0.0);

    // Özgün çevrim değişmez (ayarlar kopyaya uygulanır)
    const hea::DriveCycle& c = s.cycles()[static_cast<std::size_t>(rs.cycleIndex)].cycle;
    HEA_CHECK(c.samples[5].payload_kg != 500.0);
}

void testRunErrors()
{
    Session s;
    RunSettings rs;
    rs.cycleIndex = s.defaultCycleIndex();
    RunResult r;
    HEA_CHECK(s.run(rs, r).empty());
    const double fuelBefore = r.summary.fuel_kg;

    // SOC penceresi dışı: hata, sonuç değişmez
    s.config().sim.initialSoc = 0.95;
    const std::string err = s.run(rs, r);
    HEA_CHECK(!err.empty());
    HEA_CHECK(err.find("SOC") != std::string::npos);
    HEA_CHECK_NEAR(r.summary.fuel_kg, fuelBefore, 0.0);
    s.config().sim.initialSoc = 0.6;

    rs.cycleIndex = 999;
    HEA_CHECK(!s.run(rs, r).empty());
    rs.cycleIndex = -1;
    HEA_CHECK(!s.run(rs, r).empty());
    rs.cycleIndex = s.defaultCycleIndex();
    rs.overridePayload = true;
    rs.payload_kg = -10.0;
    HEA_CHECK(!s.run(rs, r).empty());
}

void testCycleFile()
{
    const fs::path dir = testDir();
    const fs::path csv = dir / fs::u8path("tepe_yolu_ığş.csv");
    writeText(csv, "time_s,speed_kmh,grade_percent\n0,0,0\n10,36,2\n20,36,2\n30,0,0\n");

    Session s;
    const std::size_t before = s.cycles().size();
    int index = -1;
    const std::string err = s.addCycleFile(toUtf8(csv), index);
    HEA_CHECK(err.empty());
    if (!err.empty()) std::fprintf(stderr, "addCycleFile: %s\n", err.c_str());
    HEA_CHECK(s.cycles().size() == before + 1);
    HEA_CHECK(index == static_cast<int>(before));
    if (index >= 0 && index < static_cast<int>(s.cycles().size())) {
        const CycleEntry& e = s.cycles()[static_cast<std::size_t>(index)];
        HEA_CHECK(e.group == "Dosya");
        HEA_CHECK(e.cycle.name == "tepe_yolu_ığş");
        HEA_CHECK(e.cycle.samples.size() == 31);

        // Eklenen çevrim çalıştırılabilir
        RunSettings rs;
        rs.cycleIndex = index;
        RunResult r;
        HEA_CHECK(s.run(rs, r).empty());
        HEA_CHECK(r.steps.size() == 30);
    }

    // Aynı dosya yeniden eklenince çoğalmaz, güncellenir
    writeText(csv, "time_s,speed_kmh\n0,0\n40,50\n");
    int index2 = -1;
    HEA_CHECK(s.addCycleFile(toUtf8(csv), index2).empty());
    HEA_CHECK(index2 == index);
    HEA_CHECK(s.cycles().size() == before + 1);
    HEA_CHECK(s.cycles()[static_cast<std::size_t>(index2)].cycle.samples.size() == 41);

    // Hatalı dosya: satır numaralı hata, liste değişmez
    const fs::path bad = dir / "hatali.csv";
    writeText(bad, "time_s,speed_kmh\n0,0\n1,abc\n");
    int index3 = -1;
    const std::string badErr = s.addCycleFile(toUtf8(bad), index3);
    HEA_CHECK(badErr.find("hatali.csv:3:") != std::string::npos);
    HEA_CHECK(s.cycles().size() == before + 1);

    // Olmayan dosya
    const std::string missing = s.addCycleFile(toUtf8(dir / "yok.csv"), index3);
    HEA_CHECK(missing.find("açılamadı") != std::string::npos);

    // Depodaki örnek çevrim
    int index4 = -1;
    HEA_CHECK(s.addCycleFile(HEA_SOURCE_DIR "/data/cevrimler/ornek_tepe_yolu.csv", index4).empty());
    HEA_CHECK(s.cycles()[static_cast<std::size_t>(index4)].cycle.name == "ornek_tepe_yolu");

    fs::remove_all(dir);
}

void testConfigFile()
{
    Session s;
    // Depodaki örnek ayar dosyası
    HEA_CHECK(s.loadConfigFile(HEA_SOURCE_DIR "/data/ornek_ayar.ini").empty());
    HEA_CHECK(s.configSource() == "ornek_ayar.ini");

    const fs::path dir = testDir();
    const fs::path ini = dir / fs::u8path("ayarlarım.ini");
    writeText(ini, "[simulasyon]\nbaslangic_soc = 0.5\n[klima]\nacik = hayir\n");
    HEA_CHECK(s.loadConfigFile(toUtf8(ini)).empty());
    HEA_CHECK(s.configSource() == "ayarlarım.ini");
    HEA_CHECK_NEAR(s.config().sim.initialSoc, 0.5, 0.0);
    HEA_CHECK(!s.config().sim.hvac.enabled);

    // Dosya varsayılanların üzerine okunur: önceki dosyanın değerleri kalmaz
    writeText(ini, "[simulasyon]\nyardimci_yuk_W = 500\n");
    HEA_CHECK(s.loadConfigFile(toUtf8(ini)).empty());
    HEA_CHECK_NEAR(s.config().sim.initialSoc, hea::FullConfig{}.sim.initialSoc, 0.0);
    HEA_CHECK(s.config().sim.hvac.enabled);
    HEA_CHECK_NEAR(s.config().sim.auxiliaryPower_W, 500.0, 0.0);

    // Geçersiz değer birleşimi: hata, mevcut ayarlar korunur
    writeText(ini, "[batarya]\nsoc_min = 0.9\nsoc_max = 0.5\n");
    const std::string err = s.loadConfigFile(toUtf8(ini));
    HEA_CHECK(!err.empty());
    HEA_CHECK(s.configSource() == "ayarlarım.ini");
    HEA_CHECK_NEAR(s.config().sim.auxiliaryPower_W, 500.0, 0.0);

    // Bilinmeyen anahtar: dosya:satır bilgisi
    writeText(ini, "[simulasyon]\n\nboyle_bir_sey = 1\n");
    HEA_CHECK(s.loadConfigFile(toUtf8(ini)).find("ayarlarım.ini:3") != std::string::npos);

    HEA_CHECK(!s.loadConfigFile(toUtf8(dir / "yok.ini")).empty());

    s.resetConfig();
    HEA_CHECK(s.configSource() == "varsayılan");
    HEA_CHECK_NEAR(s.config().sim.auxiliaryPower_W, hea::FullConfig{}.sim.auxiliaryPower_W, 0.0);

    fs::remove_all(dir);
}

// İki adım dizisi birebir aynı mı (her adımın ana çıktıları, kayan noktada tam eşitlik)
bool sameSteps(const std::vector<hea::StepResult>& a, const std::vector<hea::StepResult>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const hea::StepResult& x = a[i];
        const hea::StepResult& y = b[i];
        if (x.time_s != y.time_s || x.speed_mps != y.speed_mps || x.soc != y.soc
            || x.fuelUsed_kg != y.fuelUsed_kg || x.mode != y.mode
            || x.requestedMode != y.requestedMode || x.engineSpeed_radps != y.engineSpeed_radps
            || x.engineTemp_C != y.engineTemp_C || x.batteryEnergyUsed_J != y.batteryEnergyUsed_J
            || x.distance_m != y.distance_m || x.engineStarted != y.engineStarted) {
            std::fprintf(stderr, "adim %zu farkli\n", i);
            return false;
        }
    }
    return true;
}

// Canlı çalıştırma, parça parça ilerletilse de sonunda Session::run ile birebir aynıdır.
// Model tabanlı strateji iç durum tuttuğu için özellikle önemli.
void testLiveRunMatchesRun()
{
    const Session s;
    RunSettings rs;
    rs.cycleIndex = s.findCycle("nedc");
    const std::size_t chunks[] = {1, 7, 100, 3, 250};
    for (int i = 0; i < kStrategyCount; ++i) {
        rs.strategy = strategyFromIndex(i);
        RunResult whole;
        HEA_CHECK(s.run(rs, whole).empty());

        LiveRun live;
        HEA_CHECK(!live.active());
        HEA_CHECK(live.start(s, rs).empty());
        HEA_CHECK(live.active());
        HEA_CHECK(!live.finished());
        HEA_CHECK(live.stepCount() == 0);
        HEA_CHECK(live.totalSteps() == 1180);
        HEA_CHECK(live.cycle().name == "nedc");
        std::size_t c = 0, guard = 0;
        while (!live.finished() && guard++ < 10000) {
            live.advance(chunks[c++ % 5]);
        }
        HEA_CHECK(live.finished());
        HEA_CHECK(live.error().empty());
        HEA_CHECK(live.stepCount() == 1180);
        const RunResult& r = live.result();
        HEA_CHECK(r.cycleName == whole.cycleName);
        HEA_CHECK(r.strategyName == whole.strategyName);
        HEA_CHECK(sameSteps(r.steps, whole.steps));
        HEA_CHECK_NEAR(r.summary.fuel_kg, whole.summary.fuel_kg, 0.0);
        HEA_CHECK_NEAR(r.summary.socEnd, whole.summary.socEnd, 0.0);
        HEA_CHECK(r.summary.engineStarts == whole.summary.engineStarts);
        HEA_CHECK_NEAR(r.summary.timeElectric, whole.summary.timeElectric, 0.0);
        // Bitince daha fazla ilerlemez
        HEA_CHECK(live.advance(10) == 0);
        HEA_CHECK(live.advanceRealTime(1.0, 100.0) == 0);
        HEA_CHECK(live.stepCount() == 1180);
    }
}

// Gerçek zamana bağlı ilerleme: hız çarpanı, kare birikimi, uzun kare sınırı
void testLiveRunRealTime()
{
    const Session s;
    RunSettings rs;
    rs.cycleIndex = s.defaultCycleIndex(); // sehir_101, adım 1 s
    LiveRun live;
    HEA_CHECK(live.start(s, rs).empty());
    HEA_CHECK_NEAR(live.time_s(), 0.0, 0.0);
    HEA_CHECK_NEAR(live.startTime_s(), 0.0, 0.0);
    HEA_CHECK_NEAR(live.endTime_s(), 1241.0, 0.0);

    // 10 kat hızda 60 kare x 1/60 s = 1 gerçek saniye = 10 adım (kayan nokta birikimi
    // bir adımı kaçırmamalı)
    std::size_t n = 0;
    for (int i = 0; i < 60; ++i) n += live.advanceRealTime(1.0 / 60.0, 10.0);
    HEA_CHECK(n == 10);
    HEA_CHECK(live.stepCount() == 10);
    HEA_CHECK_NEAR(live.time_s(), 10.0, 0.0);
    // Kısmi özet: 10 s'lik sürüş, başlangıç SOC'si korunur
    HEA_CHECK_NEAR(live.result().summary.duration_s, 10.0, 1e-12);
    HEA_CHECK_NEAR(live.result().summary.socStart, s.config().sim.initialSoc, 0.0);

    // Bir adımdan kısa süre birikir: 0.04 s x 10 kat = 0.4 s -> adım yok, üç kare sonra 1 adım
    HEA_CHECK(live.advanceRealTime(0.04, 10.0) == 0);
    HEA_CHECK(live.advanceRealTime(0.04, 10.0) == 0);
    HEA_CHECK(live.advanceRealTime(0.04, 10.0) == 1); // 1.2 s birikti -> 1 adım, 0.2 s kaldı
    HEA_CHECK(live.stepCount() == 11);

    // Uzun kare (ör. pencere sürüklendi): en çok kMaxFrameSeconds sayılır.
    // 5 s x 10 kat = 50 adım değil; 0.25 s x 10 = 2.5 s (+0.2 kalan) -> 2 adım
    HEA_CHECK(live.advanceRealTime(5.0, 10.0) == 2);
    HEA_CHECK(live.stepCount() == 13);

    // Geçersiz süre / hız ilerletmez
    HEA_CHECK(live.advanceRealTime(0.0, 10.0) == 0);
    HEA_CHECK(live.advanceRealTime(-1.0, 10.0) == 0);
    HEA_CHECK(live.advanceRealTime(0.1, 0.0) == 0);
    HEA_CHECK(live.advanceRealTime(0.1, std::nan("")) == 0);
    HEA_CHECK(live.advanceRealTime(std::nan(""), 10.0) == 0);
    HEA_CHECK(live.stepCount() == 13);

    // 100 kat hızda sonuna kadar gerçek zamanlı oynatma da Session::run ile aynı
    int frames = 0;
    while (!live.finished() && frames < 100000) {
        live.advanceRealTime(1.0 / 60.0, 100.0);
        ++frames;
    }
    HEA_CHECK(live.finished());
    // 1228 kalan adım, karede 100/60 adım -> ~737 kare
    HEA_CHECK(frames > 700 && frames < 760);
    RunResult whole;
    HEA_CHECK(s.run(rs, whole).empty());
    HEA_CHECK(sameSteps(live.result().steps, whole.steps));
    HEA_CHECK_NEAR(live.time_s(), 1241.0, 0.0);
}

// Hatalı ayarla başlatma önceki çalıştırmayı bozmaz; çalışırken Session'daki
// değişiklikler sürmekte olan çalıştırmayı etkilemez
void testLiveRunIsolation()
{
    Session s;
    RunSettings rs;
    rs.cycleIndex = s.defaultCycleIndex();
    s.config().sim.initialSoc = 0.45;
    LiveRun live;
    HEA_CHECK(live.start(s, rs).empty());
    HEA_CHECK(live.advance(5) == 5);

    // Hatalı ayar: hata döner, sürmekte olan çalıştırma olduğu gibi kalır
    RunSettings bad = rs;
    bad.cycleIndex = 999;
    HEA_CHECK(!live.start(s, bad).empty());
    s.config().sim.initialSoc = 0.95; // SOC penceresi dışı
    const std::string err = live.start(s, rs);
    HEA_CHECK(err.find("SOC") != std::string::npos);
    HEA_CHECK(live.active());
    HEA_CHECK(live.stepCount() == 5);
    HEA_CHECK_NEAR(live.result().initialSoc, 0.45, 0.0);

    // Ayarlar ve çevrim listesi değişse de çalıştırma başladığı ayarlarla sürer
    s.config().sim.initialSoc = 0.70;
    s.config().sim.hvac.enabled = !s.config().sim.hvac.enabled;
    s.resetConfig();
    live.advance(100000);
    HEA_CHECK(live.finished());
    s.config().sim.initialSoc = 0.45;
    RunResult whole;
    HEA_CHECK(s.run(rs, whole).empty());
    HEA_CHECK(sameSteps(live.result().steps, whole.steps));

    // Taşıma (move) sırasında yarıda kalan çalıştırma bozulmaz (Simulation'ın
    // tuttuğu referanslar geçerli kalır)
    LiveRun a;
    HEA_CHECK(a.start(s, rs).empty());
    a.advance(300);
    LiveRun b = std::move(a);
    HEA_CHECK(!a.active()); // NOLINT: taşınan nesnenin durumu bilerek denetleniyor
    HEA_CHECK(b.stepCount() == 300);
    b.advance(100000);
    HEA_CHECK(sameSteps(b.result().steps, whole.steps));

    // Boş (başlatılmamış) çalıştırma güvenli
    const LiveRun empty;
    HEA_CHECK(!empty.finished());
    HEA_CHECK(empty.stepCount() == 0 && empty.totalSteps() == 0);
    HEA_CHECK(empty.result().steps.empty());
    HEA_CHECK(empty.cycle().samples.empty());
}

// Karşılaştırma, hea_cli --karsilastir ile aynı core çağrılarını yapar: sonuçlar
// core'u doğrudan çağırmakla birebir aynı
void testCompareMatchesCore()
{
    const Session s;
    RunSettings rs;
    rs.cycleIndex = s.defaultCycleIndex();
    rs.strategy = StrategyKind::IceOnly; // karşılaştırmada kullanılmamalı
    ComparisonResult c;
    HEA_CHECK(s.compare(rs, c).empty());
    HEA_CHECK(c.cycleName == "sehir_101");
    HEA_CHECK(c.entries.size() == 4);
    HEA_CHECK(c.referenceIndex == strategyIndex(StrategyKind::RuleBased));
    HEA_CHECK_NEAR(c.initialSoc, 0.6, 0.0);
    if (c.entries.size() != 4) return;

    const hea::DriveCycle& cyc = s.cycles()[static_cast<std::size_t>(rs.cycleIndex)].cycle;
    const hea::FullConfig cfg;
    for (int i = 0; i < kStrategyCount; ++i) {
        std::unique_ptr<hea::Strategy> st = makeStrategy(strategyFromIndex(i), cfg);
        const hea::RunSummary raw =
            hea::summarize(hea::runCycle(cfg.sim, cyc, *st), cfg.sim.initialSoc);
        const double stored = hea::storedEnergyReleased_J(cfg.sim.battery, raw.socStart, raw.socEnd);
        const hea::SocCorrectedFuel corr = hea::socCorrectedFuel(cfg.sim, cyc, *st);

        const ComparisonEntry& e = c.entries[static_cast<std::size_t>(i)];
        HEA_CHECK(e.kind == strategyFromIndex(i));
        HEA_CHECK(e.strategyName == st->name());
        HEA_CHECK_NEAR(e.raw.fuel_kg, raw.fuel_kg, 0.0);
        HEA_CHECK_NEAR(e.raw.socEnd, raw.socEnd, 0.0);
        HEA_CHECK_NEAR(e.rawStoredEnergy_J, stored, 0.0);
        HEA_CHECK(e.corrected.points.size() == corr.points.size());
        HEA_CHECK(e.corrected.points.size() == 9);
        HEA_CHECK_NEAR(e.corrected.fuel_kg(), corr.fuel_kg(), 0.0);
        HEA_CHECK_NEAR(e.corrected.localSlope_kgPerJ, corr.localSlope_kgPerJ, 0.0);
        HEA_CHECK_NEAR(e.corrected.fit.r2, corr.fit.r2, 0.0);
        HEA_CHECK(e.corrected.extrapolated == corr.extrapolated);
        HEA_CHECK_NEAR(e.equivalentPer100km_L(),
                       hea::litersPer100km(hea::equivalentFuel_kg(raw.fuel_kg, stored),
                                           raw.distance_m),
                       0.0);
    }
    // Referansın farkı 0; en az yakan seçili. sehir_101'de (hea_cli tablosu):
    // model < kural < EV öncelikli < sadece ICE
    HEA_CHECK_NEAR(c.correctedDiffPct(c.referenceIndex), 0.0, 0.0);
    HEA_CHECK_NEAR(c.equivalentDiffPct(c.referenceIndex), 0.0, 0.0);
    HEA_CHECK(c.bestIndex == strategyIndex(StrategyKind::ModelBased));
    HEA_CHECK(c.correctedDiffPct(strategyIndex(StrategyKind::ModelBased)) < 0.0);
    HEA_CHECK(c.correctedDiffPct(strategyIndex(StrategyKind::IceOnly)) > 0.0);
    for (int i = 0; i < kStrategyCount; ++i) {
        HEA_CHECK(c.entries[static_cast<std::size_t>(c.bestIndex)].correctedPer100km_L()
                  <= c.entries[static_cast<std::size_t>(i)].correctedPer100km_L());
    }
}

// Ayarlar karşılaştırmaya yansır; hatalı ayarda sonuç değişmez
void testCompareSettings()
{
    Session s;
    RunSettings rs;
    rs.cycleIndex = s.defaultCycleIndex();
    ComparisonResult base;
    HEA_CHECK(s.compare(rs, base).empty());

    // Ağır sabit yük: her stratejide düzeltilmiş yakıt artar
    rs.overridePayload = true;
    rs.payload_kg = 500.0;
    ComparisonResult heavy;
    HEA_CHECK(s.compare(rs, heavy).empty());
    for (std::size_t i = 0; i < heavy.entries.size() && i < base.entries.size(); ++i) {
        HEA_CHECK(heavy.entries[i].corrected.fuel_kg() > base.entries[i].corrected.fuel_kg());
    }

    // Başlangıç SOC'si yalnızca ham koşuyu değiştirir; düzeltme sabit 9 SOC ile yapılır
    s.config().sim.initialSoc = 0.45;
    ComparisonResult soc45;
    HEA_CHECK(s.compare(rs, soc45).empty());
    HEA_CHECK_NEAR(soc45.initialSoc, 0.45, 0.0);
    for (std::size_t i = 0; i < soc45.entries.size() && i < heavy.entries.size(); ++i) {
        HEA_CHECK_NEAR(soc45.entries[i].raw.socStart, 0.45, 0.0);
        HEA_CHECK_NEAR(soc45.entries[i].corrected.fuel_kg(), heavy.entries[i].corrected.fuel_kg(), 0.0);
    }

    // Hatalar: açıklama döner, önceki sonuç olduğu gibi kalır
    ComparisonResult kept = soc45;
    rs.cycleIndex = 999;
    HEA_CHECK(!s.compare(rs, kept).empty());
    rs.cycleIndex = s.defaultCycleIndex();
    s.config().sim.initialSoc = 0.95;
    const std::string err = s.compare(rs, kept);
    HEA_CHECK(err.find("SOC") != std::string::npos);
    HEA_CHECK_NEAR(kept.initialSoc, 0.45, 0.0);
    HEA_CHECK(kept.entries.size() == 4);
}

// Dosyanın satırları (CRLF/LF fark etmez)
std::vector<std::string> readLines(const fs::path& p)
{
    std::ifstream f(p);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

std::size_t countFields(const std::string& line)
{
    std::size_t n = 1;
    for (char ch : line) n += ch == ',' ? 1 : 0;
    return n;
}

// CSV olarak kaydetme: Türkçe karakterli yollar, hea_cli ile aynı başlıklar, hata durumu.
// (Dosyaların hea_cli çıktısıyla bayt bayt aynılığı gui_disa_aktarma testinde.)
void testExportCsv()
{
    const Session s;
    RunSettings rs;
    rs.cycleIndex = s.defaultCycleIndex();
    RunResult r;
    HEA_CHECK(s.run(rs, r).empty());
    HEA_CHECK(suggestedStepsFileName(r) == "sehir_101_model-tabanli_adimlar.csv");

    const fs::path dir = testDir();
    const fs::path steps = dir / fs::u8path("adımlar_şğü.csv");
    HEA_CHECK(saveStepsCsv(toUtf8(steps), r).empty());
    const std::vector<std::string> lines = readLines(steps);
    HEA_CHECK(lines.size() == r.steps.size() + 1);
    if (lines.size() > 1) {
        HEA_CHECK(lines[0].rfind("cycle,strategy,time_s,dt_s,", 0) == 0);
        HEA_CHECK(lines[1].rfind("sehir_101,model-tabanli,1,1,", 0) == 0);
        HEA_CHECK(countFields(lines[0]) == countFields(lines[1]));
        HEA_CHECK(countFields(lines[0]) == 43);
        HEA_CHECK(lines.back().rfind("sehir_101,model-tabanli,1241,1,", 0) == 0);
    }

    ComparisonResult c;
    HEA_CHECK(s.compare(rs, c).empty());
    HEA_CHECK(suggestedComparisonFileName(c) == "sehir_101_karsilastirma.csv");
    const fs::path cmp = dir / fs::u8path("karşılaştırma.csv");
    HEA_CHECK(saveComparisonCsv(toUtf8(cmp), c).empty());
    const std::vector<std::string> clines = readLines(cmp);
    HEA_CHECK(clines.size() == 5);
    if (clines.size() == 5) {
        HEA_CHECK(clines[0].rfind("cycle,strategy,distance_m,", 0) == 0);
        HEA_CHECK(clines[1].rfind("sehir_101,sadece-ICE,", 0) == 0);
        HEA_CHECK(clines[3].rfind("sehir_101,kural-tabanli,", 0) == 0);
        HEA_CHECK(countFields(clines[0]) == 15 && countFields(clines[4]) == 15);
    }

    // Yarım kalmış canlı çalıştırma da kaydedilebilir (o ana kadarki adımlar)
    LiveRun live;
    HEA_CHECK(live.start(s, rs).empty());
    live.advance(25);
    const fs::path part = dir / "yarim.csv";
    HEA_CHECK(saveStepsCsv(toUtf8(part), live.result()).empty());
    HEA_CHECK(readLines(part).size() == 26);

    // Olmayan klasör: hata, dosya adıyla
    const std::string err = saveStepsCsv(toUtf8(dir / "yok_klasor" / "x.csv"), r);
    HEA_CHECK(err.find("x.csv") != std::string::npos);
    HEA_CHECK(err.find("yazılamadı") != std::string::npos);

    fs::remove_all(dir);
}

void testChecksums()
{
    // Bilinen değerler: "123456789" -> CRC-32 cbf43926, Adler-32 091e01de
    const unsigned char d[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    HEA_CHECK((crc32(d, 9) ^ 0xFFFFFFFFul) == 0xCBF43926ul);
    HEA_CHECK(adler32(d, 9) == 0x091E01DEul);
}

unsigned long be32(const std::vector<unsigned char>& b, std::size_t at)
{
    return (static_cast<unsigned long>(b[at]) << 24) | (static_cast<unsigned long>(b[at + 1]) << 16)
           | (static_cast<unsigned long>(b[at + 2]) << 8) | static_cast<unsigned long>(b[at + 3]);
}

void testPng()
{
    const fs::path dir = testDir();
    const fs::path png = dir / fs::u8path("görüntü.png");
    // 300x250: ham veri (250 * 901 bayt) birden çok 65535'lik bloğa bölünür
    const int w = 300, h = 250;
    std::vector<unsigned char> rgb(static_cast<std::size_t>(w * h * 3));
    for (std::size_t i = 0; i < rgb.size(); ++i) rgb[i] = static_cast<unsigned char>(i * 7);
    HEA_CHECK(writePngRgb(toUtf8(png), w, h, rgb));

    std::ifstream f(png, std::ios::binary);
    const std::vector<unsigned char> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    HEA_CHECK(b.size() > 33);
    if (b.size() > 33) {
        HEA_CHECK(b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G');
        HEA_CHECK(be32(b, 16) == static_cast<unsigned long>(w));
        HEA_CHECK(be32(b, 20) == static_cast<unsigned long>(h));
        // Dosya IEND ile biter; IEND'in CRC'si sabittir (ae426082)
        HEA_CHECK(be32(b, b.size() - 4) == 0xAE426082ul);
        // Boyut: imza 8 + IHDR 25 + IDAT (12 + zlib) + IEND 12
        const std::size_t raw = static_cast<std::size_t>(h) * (static_cast<std::size_t>(w) * 3 + 1);
        const std::size_t blocks = (raw + 65534) / 65535;
        const std::size_t zlib = 2 + raw + blocks * 5 + 4;
        HEA_CHECK(b.size() == 8 + 25 + 12 + zlib + 12);
        // IDAT içindeki ham veriden geri okuma: ilk satırın ilk pikseli
        // (imza 8 + IHDR 25 + IDAT uzunluk/tür 8 + zlib başlık 2 + blok başlığı 5 + filtre 1)
        const std::size_t p = 8 + 25 + 8 + 2 + 5 + 1;
        HEA_CHECK(b[p] == rgb[0] && b[p + 1] == rgb[1] && b[p + 2] == rgb[2]);
    }

    // Hatalı girdiler
    HEA_CHECK(!writePngRgb(toUtf8(png), 0, 10, {}));
    HEA_CHECK(!writePngRgb(toUtf8(png), 2, 2, std::vector<unsigned char>(5)));
    fs::remove_all(dir);
}

} // namespace

int main()
{
    testStrategyNames();
    testCycleList();
    testRunMatchesCore();
    testAllStrategiesRun();
    testOverrides();
    testRunErrors();
    testCycleFile();
    testConfigFile();
    testLiveRunMatchesRun();
    testLiveRunRealTime();
    testLiveRunIsolation();
    testCompareMatchesCore();
    testCompareSettings();
    testExportCsv();
    testChecksums();
    testPng();
    return HEA_TEST_RESULT();
}
