#include "app.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <utility>

#include "hea/units.h"
#include "hea/version.h"
#include "imgui.h"
#include "implot.h"
#include "platform.h"

namespace gui {

namespace {

const ImVec4 kErrorColor(1.00f, 0.45f, 0.40f, 1.0f);
const ImVec4 kWarnColor(1.00f, 0.80f, 0.30f, 1.0f);
const ImVec4 kOkColor(0.45f, 0.85f, 0.50f, 1.0f);

// Etiket üstte, kontrol tam genişlikte (dar panelde okunur kalsın)
void labelAbove(const char* text)
{
    ImGui::TextUnformatted(text);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

// Özet tablosunun bir satırı: sol sütun ad, sağ sütun değer
void summaryRow(const char* name, const char* fmt, ...) IM_FMTARGS(2);
void summaryRow(const char* name, const char* fmt, ...)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", name);
    ImGui::TableSetColumnIndex(1);
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

// Çalışma modlarının ekrandaki adı ve rengi (mod şeridi, gösterge, açıklama)
struct ModeStyle {
    hea::Mode mode;
    const char* label;
    ImVec4 color;
};
const ModeStyle kModeStyles[] = {
    {hea::Mode::Electric, "Elektrikle", ImVec4(0.30f, 0.78f, 0.45f, 1.0f)},
    {hea::Mode::Engine, "Motorla", ImVec4(0.95f, 0.50f, 0.25f, 1.0f)},
    {hea::Mode::RegenBraking, "Rejeneratif fren", ImVec4(0.35f, 0.65f, 1.00f, 1.0f)},
    {hea::Mode::Stopped, "Duruş", ImVec4(0.55f, 0.55f, 0.58f, 1.0f)},
};

const ModeStyle& modeStyle(hea::Mode m)
{
    for (const ModeStyle& s : kModeStyles) {
        if (s.mode == m) return s;
    }
    return kModeStyles[3];
}

// Grafiklerin üstündeki büyük rakamlı kutucuklar
struct Tile {
    const char* name = "";
    char value[64] = {};
    // Genişlik hesabında değerin yerine kullanılacak en uzun örnek (canlı değerler
    // her karede değişir; sütun sayısı titremesin). Boşsa değerin kendisi.
    const char* widthSample = "";
    bool colored = false;
    ImVec4 color;
};

void setTile(Tile& t, const char* name, const char* widthSample, const char* fmt, ...) IM_FMTARGS(4);
void setTile(Tile& t, const char* name, const char* widthSample, const char* fmt, ...)
{
    t.name = name;
    t.widthSample = widthSample;
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(t.value, sizeof(t.value), fmt, args);
    va_end(args);
}

void drawTiles(const Tile* tiles, int count)
{
    constexpr int kMaxTiles = 8;
    count = std::min(count, kMaxTiles);
    ImGuiIO& io = ImGui::GetIO();
    ImFont* big = io.Fonts->Fonts.Size > 1 ? io.Fonts->Fonts[1] : nullptr;
    const float pad = ImGui::GetStyle().CellPadding.x * 2.0f + ImGui::GetFontSize();
    float widths[kMaxTiles] = {};
    float widest = 0.0f, sum = 0.0f;
    for (int i = 0; i < count; ++i) {
        float w = ImGui::CalcTextSize(tiles[i].name).x;
        if (big != nullptr) ImGui::PushFont(big);
        const float vw = std::max(ImGui::CalcTextSize(tiles[i].value).x,
                                  ImGui::CalcTextSize(tiles[i].widthSample).x);
        if (big != nullptr) ImGui::PopFont();
        widths[i] = std::max(w, vw) + pad;
        widest = std::max(widest, widths[i]);
        sum += widths[i];
    }
    // Yerleşim (yazılar birbirine taşmasın):
    //   eşit sütunlar tek satıra sığıyorsa öyle,
    //   sığmıyorsa içeriğe orantılı sütunlarla tek satır,
    //   o da sığmıyorsa eşit sütunlar, kutucuklar alt satıra geçer
    const float avail = ImGui::GetContentRegionAvail().x;
    int columns = count;
    bool proportional = false;
    if (widest * static_cast<float>(count) > avail) {
        if (sum <= avail) {
            proportional = true;
        } else {
            columns = std::clamp(static_cast<int>(avail / widest), 1, count);
        }
    }

    const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV
                                  | (proportional ? ImGuiTableFlags_SizingStretchProp
                                                  : ImGuiTableFlags_SizingStretchSame);
    if (ImGui::BeginTable("anahtar", columns, flags)) {
        if (proportional) {
            for (int i = 0; i < count; ++i) {
                ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, widths[i]);
            }
        }
        for (int i = 0; i < count; ++i) {
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", tiles[i].name);
            if (big != nullptr) ImGui::PushFont(big);
            if (tiles[i].colored) {
                ImGui::TextColored(tiles[i].color, "%s", tiles[i].value);
            } else {
                ImGui::TextUnformatted(tiles[i].value);
            }
            if (big != nullptr) ImGui::PopFont();
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
}

} // namespace

App::App(void* nativeWindow, const StartOptions& start)
    : nativeWindow_(nativeWindow), saveDir_(start.saveDir)
{
    settings_.cycleIndex = session_.defaultCycleIndex();

    // Komut satırı seçimleri; hata olursa ekranda gösterilir
    error_ = start.error;
    if (error_.empty() && !start.configFile.empty()) {
        error_ = session_.loadConfigFile(start.configFile);
    }
    if (error_.empty() && !start.cycleFile.empty()) {
        int index = 0;
        error_ = session_.addCycleFile(start.cycleFile, index);
        if (error_.empty()) settings_.cycleIndex = index;
    }
    if (error_.empty() && !start.cycle.empty()) {
        const int index = session_.findCycle(start.cycle);
        if (index < 0) {
            error_ = "'" + start.cycle + "' adında çevrim yok";
        } else {
            settings_.cycleIndex = index;
        }
    }
    if (error_.empty() && !start.strategy.empty()
        && !strategyFromName(start.strategy, settings_.strategy)) {
        error_ = "'" + start.strategy + "' adında strateji yok (ice | ev | kural | model)";
    }
    // Açılışta boş ekran olmasın: seçili ayarlarla bir kez çalıştır
    if (error_.empty()) {
        runNow();
    }
}

// Çalıştır (F5): sonuna kadar hemen. Hata olursa gösterilen sonuç değişmez.
void App::runNow()
{
    LiveRun r;
    std::string err = r.start(session_, settings_);
    if (err.empty()) {
        r.advance(r.totalSteps());
        err = r.error();
    }
    if (!err.empty()) {
        error_ = err;
        return;
    }
    error_.clear();
    run_ = std::move(r);
    liveMode_ = false;
    playing_ = false;
    dirty_ = false;
    saveNotice_.clear();
    resetPlotData();
}

// Canlı oynat: baştan, seçili ayarlarla; sonraki karelerde updateLive ilerletir
void App::startLive()
{
    LiveRun r;
    const std::string err = r.start(session_, settings_);
    if (!err.empty()) {
        error_ = err;
        return;
    }
    error_.clear();
    run_ = std::move(r);
    liveMode_ = true;
    playing_ = true;
    dirty_ = false;
    saveNotice_.clear();
    resetPlotData();
}

void App::finishLive()
{
    if (!liveRunning()) return;
    run_.advance(run_.totalSteps());
    playing_ = false;
    fitPlots_ = true; // yakıt ekseni son değere otursun
    if (!run_.error().empty()) error_ = run_.error();
    syncPlotData();
}

void App::togglePause()
{
    if (liveRunning()) playing_ = !playing_;
}

void App::markDirty(bool affectsComparison)
{
    dirty_ = true;
    if (affectsComparison) compareDirty_ = true;
    saveNotice_.clear(); // "Kaydedildi" yerine artık "ayarlar değişti" görünsün
}

void App::runActive()
{
    if (activeTab_ == Tab::Comparison) {
        runCompare();
    } else {
        runNow();
    }
}

// Karşılaştır: 4 strateji x (1 ham + 9 düzeltme) koşusu, ~0.1 s; hata olursa
// gösterilen karşılaştırma değişmez
void App::runCompare()
{
    compareTried_ = true;
    ComparisonResult r;
    const std::string err = session_.compare(settings_, r);
    if (!err.empty()) {
        error_ = err;
        return;
    }
    error_.clear();
    comparison_ = std::move(r);
    hasComparison_ = true;
    compareDirty_ = false;
    saveNotice_.clear();
}

// Kaydetme yolu: --kayit-klasoru verildiyse oraya önerilen adla, yoksa "Farklı kaydet"
std::string App::pickSavePath(FileKind kind, const std::string& suggestedName)
{
    if (!saveDir_.empty()) {
        const char last = saveDir_.back();
        return saveDir_ + (last == '/' || last == '\\' ? "" : "/") + suggestedName;
    }
    return saveFileDialog(nativeWindow_, kind, suggestedName);
}

void App::reportSaved(const std::string& utf8Path, const std::string& detail)
{
    lastSaved_ = fileNameOf(utf8Path);
    saveNotice_ = "Kaydedildi: " + lastSaved_ + (detail.empty() ? "" : " (" + detail + ")");
}

// Açık sekmenin CSV'si: sürüş -> adımlar (canlı oynatma sürüyorsa o ana kadarki),
// karşılaştırma -> 4 stratejinin satırları. Biçimler hea_cli ile aynı.
void App::saveCsv()
{
    std::string path, err, detail;
    if (activeTab_ == Tab::Comparison) {
        if (!hasComparison_) return;
        path = pickSavePath(FileKind::ComparisonCsv, suggestedComparisonFileName(comparison_));
        if (path.empty()) return; // vazgeçildi
        err = saveComparisonCsv(path, comparison_);
        detail = "4 strateji";
    } else {
        if (!run_.active()) return;
        const RunResult& r = run_.result();
        path = pickSavePath(FileKind::StepsCsv, suggestedStepsFileName(r));
        if (path.empty()) return;
        err = saveStepsCsv(path, r);
        detail = std::to_string(r.steps.size()) + " adım";
    }
    if (!err.empty()) {
        error_ = err;
        return;
    }
    reportSaved(path, detail);
}

void App::requestScreenshot()
{
    std::string name = "hea_ekran";
    if (activeTab_ == Tab::Comparison && hasComparison_) {
        name = comparison_.cycleName + "_karsilastirma";
    } else if (run_.active()) {
        name = run_.result().cycleName + "_" + run_.result().strategyName;
    }
    const std::string path = pickSavePath(FileKind::Png, name + ".png");
    if (!path.empty()) pendingScreenshot_ = path;
}

bool App::takeScreenshotRequest(std::string& utf8Path)
{
    if (pendingScreenshot_.empty()) return false;
    utf8Path = pendingScreenshot_;
    pendingScreenshot_.clear();
    return true;
}

void App::screenshotSaved(const std::string& utf8Path, bool ok)
{
    if (!ok) {
        error_ = fileNameOf(utf8Path) + ": görüntü yazılamadı";
        return;
    }
    reportSaved(utf8Path, "");
}

// Başlık satırının sağında küçük "kaydet" düğmeleri (iki sekmede de)
void App::drawExportButtons(bool csvEnabled)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const char* csvLabel = "CSV olarak kaydet";
    const char* pngLabel = "Görüntü kaydet";
    const float width = ImGui::CalcTextSize(csvLabel).x + ImGui::CalcTextSize(pngLabel).x
                        + style.FramePadding.x * 4.0f + style.ItemSpacing.x;
    // Sağa yaslı: başlık satırında yer varsa orada, yoksa (dar pencere, uzun uyarı)
    // alt satırda; düğmeler hiçbir zaman pencerenin dışına itilmez
    ImGui::SameLine();
    const float availOnLine = ImGui::GetContentRegionAvail().x;
    if (availOnLine >= width) {
        ImGui::SameLine(ImGui::GetCursorPosX() + availOnLine - width);
    } else {
        ImGui::NewLine();
        const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - width;
        if (right > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(right);
    }

    ImGui::BeginDisabled(!csvEnabled);
    if (ImGui::SmallButton(csvLabel)) saveCsv();
    record("csv_kaydet");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip(activeTab_ == Tab::Comparison
                              ? "Karşılaştırma tablosu: strateji başına bir satır\n"
                                "(hea_cli --karsilastir-csv ile aynı biçim)"
                              : "Adım sonuçları: her adım bir satır, SI birimleri\n"
                                "(hea_cli --csv ile aynı biçim). Canlı oynatma sürüyorsa\n"
                                "o ana kadarki adımlar.");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(pngLabel)) requestScreenshot();
    record("goruntu_kaydet");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Pencerenin görüntüsünü PNG olarak kaydeder");
    }
}

void App::updateLive()
{
    if (liveRunning() && playing_) {
        run_.advanceRealTime(ImGui::GetIO().DeltaTime, liveSpeed_);
        if (run_.finished()) {
            playing_ = false;
            fitPlots_ = true;
            if (!run_.error().empty()) error_ = run_.error();
        }
    }
    syncPlotData();
}

void App::resetPlotData()
{
    socLimits_[0] = session_.config().sim.battery.socMin * 100.0;
    socLimits_[1] = session_.config().sim.battery.socMax * 100.0;
    const hea::DriveCycle& c = run_.cycle();
    tc_.resize(c.samples.size());
    vTarget_.resize(c.samples.size());
    for (std::size_t i = 0; i < c.samples.size(); ++i) {
        tc_[i] = c.samples[i].time_s;
        vTarget_[i] = hea::units::mpsToKmh(c.samples[i].targetSpeed_mps);
    }
    t_.clear();
    v_.clear();
    soc_.clear();
    fuel_.clear();
    modeSegments_.clear();
    plotted_ = 0;
    fitPlots_ = true;
    syncPlotData();
}

void App::syncPlotData()
{
    const std::vector<hea::StepResult>& steps = run_.result().steps;
    for (; plotted_ < steps.size(); ++plotted_) {
        const hea::StepResult& s = steps[plotted_];
        t_.push_back(s.time_s);
        v_.push_back(hea::units::mpsToKmh(s.speed_mps));
        soc_.push_back(s.soc * 100.0);
        fuel_.push_back(s.fuelUsed_kg / hea::constants::kGasolineDensity);
        // Adım [t - dt, t] aralığını kapsar; önceki adımla aynı moddaysa birleştir
        const double t0 = s.time_s - s.dt_s;
        if (!modeSegments_.empty() && modeSegments_.back().mode == s.mode
            && std::fabs(modeSegments_.back().t1 - t0) < 1e-6) {
            modeSegments_.back().t1 = s.time_s;
        } else {
            modeSegments_.push_back({t0, s.time_s, s.mode});
        }
    }
}

void App::addCycleFromDialog()
{
    const std::string path = openFileDialog(nativeWindow_, FileKind::CycleCsv);
    if (path.empty()) return; // vazgeçildi
    int index = 0;
    const std::string err = session_.addCycleFile(path, index);
    if (!err.empty()) {
        error_ = err;
        return;
    }
    error_.clear();
    settings_.cycleIndex = index;
    info_ = "Çevrim eklendi: " + session_.cycles()[static_cast<std::size_t>(index)].cycle.name;
    markDirty();
}

void App::loadConfigFromDialog()
{
    const std::string path = openFileDialog(nativeWindow_, FileKind::ConfigIni);
    if (path.empty()) return;
    const std::string err = session_.loadConfigFile(path);
    if (!err.empty()) {
        error_ = err;
        return;
    }
    error_.clear();
    info_ = "Ayar dosyası yüklendi: " + session_.configSource();
    markDirty();
}

void App::record(const std::string& key)
{
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    rects_[key] = Rect{a.x, a.y, b.x, b.y};
}

void App::recordFullWidth(const std::string& key)
{
    const ImVec2 a = ImGui::GetCursorScreenPos();
    rects_[key] = Rect{a.x, a.y, a.x + ImGui::GetContentRegionAvail().x, a.y + ImGui::GetFrameHeight()};
}

bool App::findWidget(const std::string& key, Rect& out) const
{
    const auto it = rects_.find(key);
    if (it == rects_.end()) return false;
    out = it->second;
    return true;
}

std::string App::statusLine() const
{
    char buf[512];
    const hea::FullConfig& cfg = session_.config();
    const std::string cycle =
        session_.cycles()[static_cast<std::size_t>(settings_.cycleIndex)].cycle.name;
    const std::string strategy = makeStrategy(settings_.strategy, cfg)->name();
    std::snprintf(buf, sizeof(buf),
                  "cevrim=%s strateji=%s soc0=%.2f ortam=%s%.0f yuk=%s%.0f klima=%d soguk=%d "
                  "ayar=%s kirli=%d",
                  cycle.c_str(), strategy.c_str(), cfg.sim.initialSoc,
                  settings_.overrideAmbient ? "sabit:" : "cevrim:", settings_.ambient_C,
                  settings_.overridePayload ? "sabit:" : "cevrim:", settings_.payload_kg,
                  cfg.sim.hvac.enabled ? 1 : 0, cfg.sim.engineColdStart ? 1 : 0,
                  session_.configSource().c_str(), dirty_ ? 1 : 0);
    std::string line = buf;
    if (run_.active()) {
        const RunResult& r = run_.result();
        std::snprintf(buf, sizeof(buf), " sonuc=%s/%s sonuc_soc0=%.2f yakit_L=%.4f soc_son=%.4f",
                      r.cycleName.c_str(), r.strategyName.c_str(), r.initialSoc, r.summary.fuel_L,
                      r.summary.socEnd);
        line += buf;
    } else {
        line += " sonuc=yok";
    }
    // Canlı oynatma: yok (sonuç Çalıştır ile) / oynuyor / duraklatildi / bitti
    const char* live = !liveMode_         ? "yok"
                       : run_.finished() ? "bitti"
                       : playing_        ? "oynuyor"
                                         : "duraklatildi";
    std::snprintf(buf, sizeof(buf), " canli=%s adim=%zu/%zu hiz=%.0f", live, run_.stepCount(),
                  run_.totalSteps(), static_cast<double>(liveSpeed_));
    line += buf;
    // Karşılaştırma: açık sekme, çevrim, her strateji için SOC düzeltmeli / eşdeğer
    // L/100 km (%.9g: hea_cli --karsilastir-csv ile aynı biçim, testler karşılaştırır)
    line += activeTab_ == Tab::Comparison ? " sekme=karsilastirma" : " sekme=surus";
    if (hasComparison_) {
        std::snprintf(buf, sizeof(buf), " kars=%s kars_kirli=%d", comparison_.cycleName.c_str(),
                      compareDirty_ ? 1 : 0);
        line += buf;
        for (const ComparisonEntry& e : comparison_.entries) {
            std::snprintf(buf, sizeof(buf), " kars_%s=%.9g/%.9g", e.strategyName.c_str(),
                          e.correctedPer100km_L(), e.equivalentPer100km_L());
            line += buf;
        }
    } else {
        line += " kars=yok";
    }
    line += " kayit=" + (lastSaved_.empty() ? std::string("-") : lastSaved_);
    line += " hata=" + (error_.empty() ? std::string("-") : error_);
    return line;
}

void App::draw()
{
    // Uygulama penceresinin tamamını kaplayan tek bir ImGui penceresi
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("ana", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                     | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);
    rects_.clear();

    // F5: açık sekmenin işini çalıştır, Boşluk: canlı oynatmayı duraklat / devam et
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        runActive();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        togglePause();
    }
    updateLive();

    const float panelWidth = ImGui::GetFontSize() * 23.0f;
    // Dış panel hiç kaymaz: alt bölümün yüksekliği bir kare gecikmeyle ölçüldüğünden
    // mesaj belirdiği karede içerik bir an taşar; kaydırma çubuğu açılırsa kontroller
    // bir kare için kayar (titreme). Kaydırma yalnızca iç bölümdedir.
    ImGui::BeginChild("ayarlar", ImVec2(panelWidth, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    // Ayarlar kendi içinde kayar; alttaki Çalıştır bölümü her zaman görünür.
    // Alt bölümün yüksekliği bir önceki karede ölçülür (mesaj satır sayısı değişebilir).
    if (footerHeight_ <= 0.0f) footerHeight_ = ImGui::GetFrameHeightWithSpacing() * 3.0f;
    ImGui::BeginChild("ayarlar_icerik", ImVec2(0, -footerHeight_));
    drawSettings();
    ImGui::EndChild();
    const float footerTop = ImGui::GetCursorPosY();
    drawRunFooter();
    footerHeight_ = ImGui::GetCursorPosY() - footerTop;
    ImGui::EndChild();

    // Sağ taraf: iki sekme. Sol paneldeki ayarlar ikisinde de geçerli; Çalıştır (F5)
    // açık sekmenin işini yapar.
    ImGui::SameLine();
    ImGui::BeginChild("sag", ImVec2(0, 0));
    if (ImGui::BeginTabBar("sekmeler")) {
        const bool drive = ImGui::BeginTabItem("Sürüş");
        record("sekme_surus");
        if (drive) {
            activeTab_ = Tab::Drive;
            drawPlots();
            ImGui::EndTabItem();
        }
        const bool comparison = ImGui::BeginTabItem("Strateji karşılaştırması");
        record("sekme_karsilastirma");
        if (comparison) {
            activeTab_ = Tab::Comparison;
            // İlk açılışta kendiliğinden hesapla (kısa sürer; boş sekme görünmesin)
            if (!hasComparison_ && !compareTried_) runCompare();
            drawComparison();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();

    ImGui::End();
}

void App::drawSettings()
{
    ImGui::TextUnformatted("Akıllı Hibrit Enerji Asistanı");
    ImGui::TextDisabled("sürüm %s", hea::coreVersion());

    drawCycleSelector();

    // --- Strateji ---
    ImGui::SeparatorText("Strateji");
    int strategy = strategyIndex(settings_.strategy);
    recordFullWidth("strateji");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##strateji", strategyLabel(settings_.strategy))) {
        for (int i = 0; i < kStrategyCount; ++i) {
            const bool selected = i == strategy;
            if (ImGui::Selectable(strategyLabel(strategyFromIndex(i)), selected)) {
                strategy = i;
                settings_.strategy = strategyFromIndex(i);
                markDirty(false); // karşılaştırma 4 stratejiyi birden kapsar
            }
            record("strateji/" + std::to_string(i));
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", strategyDescription(settings_.strategy));
    ImGui::PopStyleColor();

    // --- Koşullar ---
    ImGui::SeparatorText("Koşullar");
    hea::FullConfig& cfg = session_.config();
    const hea::BatteryParams& bat = cfg.sim.battery;
    float socPct = static_cast<float>(cfg.sim.initialSoc * 100.0);
    labelAbove("Başlangıç batarya doluluğu (SOC)");
    if (ImGui::SliderFloat("##soc", &socPct, static_cast<float>(bat.socMin * 100.0),
                           static_cast<float>(bat.socMax * 100.0), "%.0f %%",
                           ImGuiSliderFlags_AlwaysClamp)) {
        cfg.sim.initialSoc = socPct / 100.0;
        markDirty();
    }
    record("soc");

    const hea::DriveCycle& cycle =
        session_.cycles()[static_cast<std::size_t>(settings_.cycleIndex)].cycle;
    const hea::DriveInput first = cycle.samples.empty() ? hea::DriveInput{} : cycle.samples.front();

    if (ImGui::Checkbox("Sabit ortam sıcaklığı", &settings_.overrideAmbient)) {
        if (settings_.overrideAmbient) settings_.ambient_C = first.ambientTemp_C;
        markDirty();
    }
    record("sabit_ortam");
    ImGui::BeginDisabled(!settings_.overrideAmbient);
    float ambient = static_cast<float>(settings_.ambient_C);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderFloat("##ortam", &ambient, -20.0f, 45.0f, "%.0f °C", ImGuiSliderFlags_AlwaysClamp)) {
        settings_.ambient_C = ambient;
        markDirty();
    }
    record("ortam");
    ImGui::EndDisabled();

    if (ImGui::Checkbox("Sabit yük (yolcu + bagaj)", &settings_.overridePayload)) {
        if (settings_.overridePayload) settings_.payload_kg = first.payload_kg;
        markDirty();
    }
    record("sabit_yuk");
    ImGui::BeginDisabled(!settings_.overridePayload);
    float payload = static_cast<float>(settings_.payload_kg);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::SliderFloat("##yuk", &payload, 0.0f, 500.0f, "%.0f kg", ImGuiSliderFlags_AlwaysClamp)) {
        settings_.payload_kg = payload;
        markDirty();
    }
    record("yuk");
    ImGui::EndDisabled();

    if (ImGui::Checkbox("Klima / ısıtma açık", &cfg.sim.hvac.enabled)) markDirty();
    record("klima");
    if (ImGui::Checkbox("Motor soğuk başlar", &cfg.sim.engineColdStart)) markDirty();
    record("soguk");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Açık: motor ortam sıcaklığında başlar, ısınana kadar daha çok yakar.\n"
                          "Kapalı: motor sıcak (90 °C) başlar.");
    }

    // --- Ayar dosyası ---
    ImGui::SeparatorText("Ayar dosyası");
    ImGui::TextDisabled("Kaynak: %s", session_.configSource().c_str());
    ImGui::BeginDisabled(!fileDialogSupported());
    if (ImGui::Button("Yükle...")) loadConfigFromDialog();
    record("ayar_yukle");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Varsayılana dön")) {
        session_.resetConfig();
        info_ = "Ayarlar varsayılana döndü";
        markDirty();
    }
    record("varsayilan");

    drawSummary();
}

// Panelin altına sabit: Çalıştır düğmesi ve durum mesajı (küçük pencerede de görünür)
void App::drawRunFooter()
{
    ImGui::Separator();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(ImGui::GetStyle().FramePadding.x, ImGui::GetFontSize() * 0.6f));
    // Açık sekmenin işi: sürüş -> tek çalıştırma, karşılaştırma -> 4 strateji.
    // ### ile etiket değişse de kimlik aynı kalır.
    const bool comparisonTab = activeTab_ == Tab::Comparison;
    if (ImGui::Button(comparisonTab ? "Karşılaştır  (F5)###calistir" : "Çalıştır  (F5)###calistir",
                      ImVec2(-FLT_MIN, 0))) {
        runActive();
    }
    record("calistir");
    ImGui::PopStyleVar();

    const bool stale = comparisonTab ? (hasComparison_ && compareDirty_) : dirty_;
    if (!error_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kErrorColor);
        ImGui::TextWrapped("Hata: %s", error_.c_str());
        ImGui::PopStyleColor();
    } else if (!saveNotice_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kOkColor);
        ImGui::TextWrapped("%s", saveNotice_.c_str());
        ImGui::PopStyleColor();
    } else if (stale) {
        ImGui::PushStyleColor(ImGuiCol_Text, kWarnColor);
        ImGui::TextWrapped(comparisonTab
                               ? "Ayarlar değişti; karşılaştırmayı güncellemek için Karşılaştır'a basın."
                               : "Ayarlar değişti; sonucu güncellemek için Çalıştır'a basın.");
        ImGui::PopStyleColor();
    } else if (!info_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, kOkColor);
        ImGui::TextWrapped("%s", info_.c_str());
        ImGui::PopStyleColor();
    }
}

void App::drawCycleSelector()
{
    ImGui::SeparatorText("Sürüş çevrimi");
    const std::vector<CycleEntry>& cycles = session_.cycles();
    const CycleEntry& current = cycles[static_cast<std::size_t>(settings_.cycleIndex)];
    recordFullWidth("cevrim");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##cevrim", current.cycle.name.c_str(), ImGuiComboFlags_HeightLarge)) {
        std::string group;
        for (int i = 0; i < static_cast<int>(cycles.size()); ++i) {
            const CycleEntry& e = cycles[static_cast<std::size_t>(i)];
            if (e.group != group) {
                group = e.group;
                ImGui::SeparatorText(group.c_str());
            }
            ImGui::PushID(i);
            const bool selected = i == settings_.cycleIndex;
            if (ImGui::Selectable(e.cycle.name.c_str(), selected)) {
                settings_.cycleIndex = i;
                markDirty();
            }
            record("cevrim/" + e.cycle.name);
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    const hea::DriveCycle& c = current.cycle;
    const hea::DriveInput first = c.samples.empty() ? hea::DriveInput{} : c.samples.front();
    ImGui::TextDisabled("%s | %.0f s | %.2f km", current.group.c_str(), c.duration_s(),
                        c.distance_m() / 1000.0);
    ImGui::TextDisabled("ort. %.0f km/h | en çok %.0f km/h",
                        hea::units::mpsToKmh(c.averageSpeed_mps()),
                        hea::units::mpsToKmh(c.maxSpeed_mps()));
    ImGui::TextDisabled("başta %.0f °C, %.0f kg yük", first.ambientTemp_C, first.payload_kg);

    ImGui::BeginDisabled(!fileDialogSupported());
    if (ImGui::Button("CSV dosyasından çevrim ekle...")) addCycleFromDialog();
    record("csv_ekle");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Biçim: time_s,speed_kmh[,grade_percent][,payload_kg][,ambient_C]\n"
                          "Örnek: data/cevrimler/ornek_tepe_yolu.csv");
    }
}

void App::drawSummary()
{
    if (!run_.active()) return;
    ImGui::SeparatorText(liveRunning() ? "Sonuç (şu ana kadar)" : "Sonuç");
    const RunResult& r = run_.result();
    const hea::RunSummary& s = r.summary;
    ImGui::TextWrapped("%s  /  %s", r.cycleName.c_str(), r.strategyName.c_str());
    if (ImGui::BeginTable("ozet", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        summaryRow("Süre / yol", "%.0f s  /  %.2f km", s.duration_s, s.distance_m / 1000.0);
        summaryRow("Yakıt", "%.3f L", s.fuel_L);
        summaryRow("Tüketim", "%.2f L/100 km", s.fuelPer100km_L);
        summaryRow("SOC baş / son", "%.1f %%  /  %.1f %%", s.socStart * 100.0, s.socEnd * 100.0);
        summaryRow("Batarya net", "%.0f Wh", s.batteryEnergyUsed_J / 3600.0);
        summaryRow("Frenden geri kazanılan", "%.0f Wh", s.regenEnergy_J / 3600.0);
        summaryRow("Motor çalıştırma", "%d kez", s.engineStarts);
        summaryRow("Elektrikle", "%.0f %% süre", s.timeElectric * 100.0);
        summaryRow("Motorla", "%.0f %% süre", s.timeEngine * 100.0);
        summaryRow("Rejeneratif fren", "%.0f %% süre", s.timeRegen * 100.0);
        summaryRow("Duruş", "%.0f %% süre", s.timeStopped * 100.0);
        summaryRow("En büyük hız hatası", "%.1f km/h", hea::units::mpsToKmh(s.maxSpeedError_mps));
        ImGui::EndTable();
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Tüketim hamdır (SOC düzeltmesi yok): bataryayı boşaltan strateji "
                       "daha az yakmış görünür.");
    ImGui::PopStyleColor();
    if (s.unmetElectricalEnergy_J > 0.0) {
        ImGui::PushStyleColor(ImGuiCol_Text, kWarnColor);
        ImGui::TextWrapped("Uyarı: %.0f Wh elektrik ihtiyacı karşılanamadı (batarya alt sınırda).",
                           s.unmetElectricalEnergy_J / 3600.0);
        ImGui::PopStyleColor();
    }
}

// Grafiklerin üstünde, kaydırmadan görünen ana rakamlar. Canlı oynatma sürerken
// anlık değerler (hız, motor devri, SOC, mod, yakıt), bitince çalıştırmanın özeti.
void App::drawKeyFigures()
{
    const RunResult& r = run_.result();
    ImGui::TextDisabled("%s  /  %s", r.cycleName.c_str(), r.strategyName.c_str());
    if (dirty_) {
        ImGui::SameLine();
        ImGui::TextColored(kWarnColor, "(ayarlar değişti, sonuç eski)");
    }
    drawExportButtons(run_.active());

    Tile tiles[5];
    if (liveRunning()) {
        // İlk adımdan önce: çevrimin başındaki durum
        const hea::StepResult* last = r.steps.empty() ? nullptr : &r.steps.back();
        const hea::DriveCycle& c = run_.cycle();
        const double v0 = c.samples.empty() ? 0.0 : c.samples.front().targetSpeed_mps;
        setTile(tiles[0], "Hız", "888 km/h", "%.0f km/h",
                hea::units::mpsToKmh(last != nullptr ? last->speed_mps : v0));
        if (last != nullptr && last->engineRunning) {
            setTile(tiles[1], "Motor devri", "8888 rpm", "%.0f rpm",
                    hea::units::radpsToRpm(last->engineSpeed_radps));
        } else {
            setTile(tiles[1], "Motor devri", "8888 rpm", "kapalı");
        }
        setTile(tiles[2], "Batarya (SOC)", "88.8 %", "%.1f %%",
                (last != nullptr ? last->soc : r.initialSoc) * 100.0);
        if (last != nullptr) {
            const ModeStyle& m = modeStyle(last->mode);
            setTile(tiles[3], "Mod", "Rejeneratif fren", "%s", m.label);
            tiles[3].colored = true;
            tiles[3].color = m.color;
        } else {
            setTile(tiles[3], "Mod", "Rejeneratif fren", "-");
        }
        setTile(tiles[4], "Toplam yakıt", "8.888 L", "%.3f L", r.summary.fuel_L);
    } else {
        const hea::RunSummary& s = r.summary;
        setTile(tiles[0], "Tüketim (ham)", "", "%.2f L/100 km", s.fuelPer100km_L);
        setTile(tiles[1], "Toplam yakıt", "", "%.3f L", s.fuel_L);
        setTile(tiles[2], "SOC baş / son", "", "%.0f %% / %.0f %%", s.socStart * 100.0,
                s.socEnd * 100.0);
        setTile(tiles[3], "Elektrikle geçen süre", "", "%.0f %%", s.timeElectric * 100.0);
        setTile(tiles[4], "Motor çalıştırma", "", "%d", s.engineStarts);
    }
    drawTiles(tiles, 5);
}

// Grafiklerin üstünde: canlı oynatma düğmeleri, hız çarpanı, ilerleme
void App::drawLiveBar()
{
    const bool running = liveRunning();
    const ImGuiStyle& style = ImGui::GetStyle();
    // Değişen etiketli düğmeler sabit genişlikte (yanındakiler kaymasın); ### ile
    // etiket değişse de kimlik aynı kalır
    auto buttonWidth = [&](const char* a, const char* b) {
        return std::max(ImGui::CalcTextSize(a).x, ImGui::CalcTextSize(b).x) + style.FramePadding.x * 2.0f;
    };

    if (ImGui::Button(running ? "Baştan oynat###canli" : "Canlı oynat###canli",
                      ImVec2(buttonWidth("Baştan oynat", "Canlı oynat"), 0.0f))) {
        startLive();
    }
    record("canli");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Seçili ayarlarla çevrimi baştan, gerçek zamana bağlı oynatır.\n"
                          "Grafikler simülasyon ilerledikçe dolar. Boşluk: duraklat / devam et.");
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!running);
    if (ImGui::Button(running && !playing_ ? "Devam et###duraklat" : "Duraklat###duraklat",
                      ImVec2(buttonWidth("Devam et", "Duraklat"), 0.0f))) {
        togglePause();
    }
    record("duraklat");
    ImGui::SameLine();
    if (ImGui::Button("Sona git")) finishLive();
    record("sona_git");
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, style.ItemSpacing.x * 3.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Hız");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
    ImGui::SliderFloat("##hiz", &liveSpeed_, 1.0f, 100.0f, "%.0f kat",
                       ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
    record("hiz");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Canlı oynatma hızı: 1 gerçek saniyede geçen simülasyon saniyesi.\n"
                          "Sonucu değiştirmez.");
    }

    // İlerleme: çevrimin ne kadarı simüle edildi. Dar pencerede / büyük ölçekte
    // yer kalmadıysa kendi satırında, tam genişlikte.
    ImGui::SameLine(0.0f, style.ItemSpacing.x * 3.0f);
    if (ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 9.0f) {
        ImGui::NewLine();
    }
    const std::size_t total = run_.totalSteps();
    const float fraction =
        total > 0 ? static_cast<float>(run_.stepCount()) / static_cast<float>(total) : 0.0f;
    char overlay[64] = "";
    if (run_.active()) {
        std::snprintf(overlay, sizeof(overlay), "%.0f / %.0f s", run_.time_s() - run_.startTime_s(),
                      run_.endTime_s() - run_.startTime_s());
    }
    // Varsayılan sarı uyarı gibi duruyor: düğmelerle aynı mavi tonu
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.26f, 0.48f, 0.78f, 1.0f));
    ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0.0f), overlay);
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void App::drawPlots()
{
    drawLiveBar();
    if (!run_.active()) {
        ImGui::TextDisabled("Sonuç yok. Ayarları seçip Çalıştır'a basın.");
        return;
    }
    drawKeyFigures();

    const int n = static_cast<int>(t_.size());
    const int nc = static_cast<int>(tc_.size());
    const bool live = liveRunning();
    const double now = run_.time_s();
    // Canlı oynatmada simülasyonun bulunduğu an: her grafikte dikey çizgi
    auto nowMarker = [&]() {
        if (!live) return;
        ImPlot::SetNextLineStyle(ImVec4(1.0f, 1.0f, 1.0f, 0.55f), 1.0f);
        ImPlot::PlotInfLines("##simdi", &now, 1);
    };

    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float stripHeight = ImGui::GetFontSize() * 4.8f;
    const float h = (ImGui::GetContentRegionAvail().y - stripHeight - 3.0f * spacing) / 3.0f;
    // En küçük yükseklik yazı boyutuna bağlı; daha da küçük pencerede bölüm kayar
    const float minH = ImGui::GetFontSize() * 6.0f;
    const ImVec2 size(-1.0f, std::max(h, minH));
    const bool fit = fitPlots_;
    fitPlots_ = false;
    if (fit) {
        // Zaman ekseni doğrudan çevrimin başı-sonu (bağlı eksen, tüm grafikler izler).
        // Grafiklerde yalnızca dikey eksen veriye oturtulur: canlı oynatmanın başında
        // yakıt grafiğinde veri yokken x'i oturtmak ekseni t=0 çevresine daraltırdı.
        xMin_ = run_.startTime_s();
        xMax_ = std::max(run_.endTime_s(), xMin_ + 1.0);
    }

    // Hizalı grafikler: y ekseni yazılarının genişliği farklı olsa da çizim alanları
    // alt alta aynı yerden başlar (zaman eksenleri bağlı)
    if (!ImPlot::BeginAlignedPlots("grafikler")) return;

    if (fit) ImPlot::SetNextAxisToFit(ImAxis_Y1);
    if (ImPlot::BeginPlot("Araç hızı", size)) {
        ImPlot::SetupAxes(nullptr, "km/h");
        ImPlot::SetupAxisLinks(ImAxis_X1, &xMin_, &xMax_);
        // Hedef: çevrimin tamamı baştan bellidir. Kalın ve yarı saydam: gerçekleşen hız
        // çoğu zaman tam üstündedir, ayrıldığı yerde (güç yetmediğinde) fark görünür
        ImPlot::SetNextLineStyle(ImVec4(0.35f, 0.60f, 1.00f, 0.45f), 5.0f);
        ImPlot::PlotLine("hedef (çevrim)", tc_.data(), vTarget_.data(), nc);
        ImPlot::SetNextLineStyle(ImVec4(1.00f, 0.60f, 0.25f, 1.00f), 1.5f);
        ImPlot::PlotLine("gerçekleşen", t_.data(), v_.data(), n);
        nowMarker();
        ImPlot::EndPlot();
    }

    drawModeStrip(stripHeight, live, now);

    if (fit) ImPlot::SetNextAxisToFit(ImAxis_Y1);
    if (ImPlot::BeginPlot("Batarya doluluğu (SOC)", size)) {
        ImPlot::SetupAxes(nullptr, "%");
        ImPlot::SetupAxisLinks(ImAxis_X1, &xMin_, &xMax_);
        // Batarya penceresinin alt ve üst sınırı: çevrim boyunca yatay doğrular
        if (nc > 0) {
            const double xs[2] = {tc_.front(), tc_.back()};
            for (double limit : socLimits_) {
                const double ys[2] = {limit, limit};
                ImPlot::SetNextLineStyle(ImVec4(0.75f, 0.75f, 0.75f, 0.55f), 1.0f);
                ImPlot::PlotLine("SOC penceresi", xs, ys, 2);
            }
        }
        ImPlot::SetNextLineStyle(ImVec4(0.35f, 0.60f, 1.00f, 1.00f), 1.5f);
        ImPlot::PlotLine("SOC", t_.data(), soc_.data(), n);
        nowMarker();
        ImPlot::EndPlot();
    }

    if (fit) ImPlot::SetNextAxisToFit(ImAxis_Y1);
    if (ImPlot::BeginPlot("Toplam yakıt", size)) {
        // Canlı oynatmada yakıt sürekli artar: dikey eksen her karede veriye oturur
        ImPlot::SetupAxes("zaman [s]", "L", ImPlotAxisFlags_None,
                          live ? ImPlotAxisFlags_AutoFit : ImPlotAxisFlags_None);
        ImPlot::SetupAxisLinks(ImAxis_X1, &xMin_, &xMax_);
        ImPlot::SetNextLineStyle(ImVec4(0.90f, 0.35f, 0.35f, 1.00f), 1.5f);
        ImPlot::PlotLine("yakıt", t_.data(), fuel_.data(), n);
        nowMarker();
        ImPlot::EndPlot();
    }

    ImPlot::EndAlignedPlots();
}

// Hız grafiğinin altında ince şerit: her an hangi modda gidildiği (renkli)
void App::drawModeStrip(float height, bool live, double now)
{
    if (!ImPlot::BeginPlot("##mod", ImVec2(-1.0f, height),
                           ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText)) {
        return;
    }
    // Zaman ekseni diğer grafiklerle bağlı; rakamları alttaki grafiklerde
    ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);
    // Dikey eksen kilitli ve rakamsız: fare üstüne gelince vurgulanmasın (mavi blok
    // gibi görünüp kafa karıştırıyordu)
    ImPlot::SetupAxis(ImAxis_Y1, "mod",
                      ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoTickMarks
                          | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_Lock
                          | ImPlotAxisFlags_NoHighlight);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 1.0, ImPlotCond_Always);
    ImPlot::SetupAxisLinks(ImAxis_X1, &xMin_, &xMax_);
    ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Outside | ImPlotLegendFlags_Horizontal);

    // Açıklama (renk kutuları) için veri içermeyen öğeler
    for (const ModeStyle& m : kModeStyles) {
        ImPlot::SetNextLineStyle(m.color);
        ImPlot::PlotDummy(m.label);
    }
    // Mod parçaları: her biri [t0, t1] aralığında tam yükseklikte dikdörtgen.
    // En az 1 piksel: uzaklaştırınca 1 saniyelik parçalar da görünsün.
    ImDrawList* dl = ImPlot::GetPlotDrawList();
    ImPlot::PushPlotClipRect();
    for (const ModeSegment& s : modeSegments_) {
        if (s.t1 < xMin_ || s.t0 > xMax_) continue;
        const ImVec2 a = ImPlot::PlotToPixels(s.t0, 1.0);
        ImVec2 b = ImPlot::PlotToPixels(s.t1, 0.0);
        if (b.x - a.x < 1.0f) b.x = a.x + 1.0f;
        dl->AddRectFilled(a, b, ImGui::GetColorU32(modeStyle(s.mode).color));
    }
    ImPlot::PopPlotClipRect();
    if (live) {
        ImPlot::SetNextLineStyle(ImVec4(1.0f, 1.0f, 1.0f, 0.55f), 1.0f);
        ImPlot::PlotInfLines("##simdi", &now, 1);
    }
    ImPlot::EndPlot();
}

namespace {

// Grafiğin yatay eksenindeki kısa adlar
const char* strategyShortLabel(StrategyKind k)
{
    switch (k) {
    case StrategyKind::IceOnly: return "Sadece ICE";
    case StrategyKind::EvFirst: return "EV öncelikli";
    case StrategyKind::RuleBased: return "Kural tabanlı";
    case StrategyKind::ModelBased: return "Model tabanlı";
    }
    return "";
}

// Fark hücresi: eksi (daha az yakıt) yeşil, artı turuncu, referans soluk
void diffCell(double pct, bool isReference)
{
    if (isReference) {
        ImGui::TextDisabled("referans");
    } else {
        ImGui::TextColored(pct < 0.0 ? kOkColor : kWarnColor, "%+.1f %%", pct);
    }
}

} // namespace

// Strateji karşılaştırması sekmesi: SOC düzeltmeli tüketim (hea_cli --karsilastir ile aynı)
void App::drawComparison()
{
    if (!hasComparison_) {
        ImGui::TextDisabled("Karşılaştırma yok. Ayarları seçip Karşılaştır'a (F5) basın.");
        return;
    }
    const ComparisonResult& c = comparison_;
    ImGui::TextDisabled("%s  /  4 strateji  /  ham koşunun başlangıç SOC'si %.0f %%",
                        c.cycleName.c_str(), c.initialSoc * 100.0);
    if (compareDirty_) {
        ImGui::SameLine();
        ImGui::TextColored(kWarnColor, "(ayarlar değişti, sonuç eski)");
    }
    drawExportButtons(true);

    // En az yakan strateji (SOC düzeltmeli)
    const ComparisonEntry& best = c.entries[static_cast<std::size_t>(c.bestIndex)];
    ImGuiIO& io = ImGui::GetIO();
    ImFont* big = io.Fonts->Fonts.Size > 1 ? io.Fonts->Fonts[1] : nullptr;
    if (big != nullptr) ImGui::PushFont(big);
    ImGui::TextColored(kOkColor, "En az yakan: %s", strategyLabel(best.kind));
    if (big != nullptr) ImGui::PopFont();
    if (c.bestIndex == c.referenceIndex) {
        ImGui::Text("SOC düzeltmeli %.2f L/100 km (referans strateji)", best.correctedPer100km_L());
    } else {
        ImGui::Text("SOC düzeltmeli %.2f L/100 km, kural tabanlıya göre %+.1f %%",
                    best.correctedPer100km_L(), c.correctedDiffPct(c.bestIndex));
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Stratejiler çevrimi farklı batarya doluluğuyla bitirir; bataryayı boşaltan "
                       "daha az yakmış görünür. Adil kıyas için her strateji 9 farklı başlangıç "
                       "SOC'siyle (%%40-78) çalıştırılır ve batarya net değişmeden bitseydi "
                       "harcanacak yakıt bulunur (SAE J1711 mantığı, hea_cli --karsilastir ile "
                       "aynı hesap).");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Grafik kalan yüksekliği tablo ve notlarla paylaşır
    const ImGuiStyle& style = ImGui::GetStyle();
    const float rowH = ImGui::GetTextLineHeight() + style.CellPadding.y * 2.0f + 1.0f;
    const float tableH = rowH * static_cast<float>(c.entries.size() + 1);
    const float notesH = ImGui::GetTextLineHeightWithSpacing() * 4.0f;
    const float chartH = std::max(ImGui::GetContentRegionAvail().y - tableH - notesH
                                      - style.ItemSpacing.y * 3.0f,
                                  ImGui::GetFontSize() * 9.0f);
    drawComparisonChart(chartH);
    drawComparisonTable();

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("L/100 km. Fark: kural tabanlı stratejiye göre, eksi = daha az yakıt. "
                       "Ham: seçili başlangıç SOC'siyle tek koşu (SOC düzeltmesi yok). "
                       "Eşdeğer yakıt: kontrol yöntemi, yakıt + bataryadan çekilen enerjinin "
                       "yakıt karşılığı (1 kWh = %.0f g).",
                       hea::kDefaultFuelPerBatteryJoule * 3.6e6 * 1000.0);
    bool anyExtrapolated = false;
    for (const ComparisonEntry& e : c.entries) anyExtrapolated = anyExtrapolated || e.corrected.extrapolated;
    if (anyExtrapolated) {
        ImGui::TextWrapped("* 9 koşunun hepsi bataryayı aynı yönde değiştirdi (hep doldurdu ya da "
                           "hep boşalttı); düzeltilmiş değer dış değerlemeyle bulundu, daha az "
                           "güvenilir.");
    }
    ImGui::PopStyleColor();
}

void App::drawComparisonChart(float height)
{
    const ComparisonResult& c = comparison_;
    const int n = static_cast<int>(c.entries.size());
    if (n <= 0 || n > kStrategyCount) return;
    constexpr double kBarWidth = 0.36; // bir grubun iki çubuğu: ham | düzeltilmiş
    double pos[kStrategyCount], xRaw[kStrategyCount], xCor[kStrategyCount];
    double raw[kStrategyCount], cor[kStrategyCount];
    const char* ticks[kStrategyCount];
    double top = 0.0;
    for (int i = 0; i < n; ++i) {
        const ComparisonEntry& e = c.entries[static_cast<std::size_t>(i)];
        pos[i] = i;
        xRaw[i] = i - kBarWidth / 2.0;
        xCor[i] = i + kBarWidth / 2.0;
        raw[i] = e.raw.fuelPer100km_L;
        cor[i] = e.correctedPer100km_L();
        ticks[i] = strategyShortLabel(e.kind);
        top = std::max(top, std::max(raw[i], cor[i]));
    }
    // Başlıksız: sekme adı ve "L/100 km" ekseni yeterli; açıklama satırı en üstte kalır
    if (!ImPlot::BeginPlot("##yakit_tuketimi", ImVec2(-1.0f, height),
                           ImPlotFlags_NoInputs | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {
        return;
    }
    ImPlot::SetupAxes(nullptr, "L/100 km", ImPlotAxisFlags_NoGridLines, ImPlotAxisFlags_None);
    ImPlot::SetupAxisTicks(ImAxis_X1, pos, n, ticks);
    ImPlot::SetupAxisLimits(ImAxis_X1, -0.6, n - 0.4, ImPlotCond_Always);
    // Üstte değer yazılarına ve açıklamaya yer kalsın
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, top > 0.0 ? top * 1.25 : 1.0, ImPlotCond_Always);
    // Açıklama çizim alanının dışında, üstte: dar pencerede de çubukları kapatmaz
    ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Outside | ImPlotLegendFlags_Horizontal);

    const ImVec4 rawColor(0.55f, 0.57f, 0.62f, 1.0f);
    const ImVec4 corColor(0.35f, 0.60f, 1.00f, 1.0f);
    ImPlot::SetNextFillStyle(rawColor);
    ImPlot::SetNextLineStyle(rawColor);
    ImPlot::PlotBars("ham (SOC düzeltmesiz)", xRaw, raw, n, kBarWidth * 0.92);
    ImPlot::SetNextFillStyle(corColor);
    ImPlot::SetNextLineStyle(corColor);
    ImPlot::PlotBars("SOC düzeltmeli", xCor, cor, n, kBarWidth * 0.92);
    // Referans (kural tabanlı, SOC düzeltmeli) yatay çizgi: farklar buna göre
    const double ref = cor[c.referenceIndex];
    ImPlot::SetNextLineStyle(ImVec4(1.0f, 1.0f, 1.0f, 0.45f), 1.0f);
    ImPlot::PlotInfLines("kural tabanlı (referans)", &ref, 1, ImPlotInfLinesFlags_Horizontal);

    // Çubukların üstünde değerler
    const ImVec2 above(0.0f, -ImGui::GetFontSize() * 0.7f);
    for (int i = 0; i < n; ++i) {
        char text[24];
        std::snprintf(text, sizeof(text), "%.2f", raw[i]);
        ImPlot::PlotText(text, xRaw[i], raw[i], above);
        std::snprintf(text, sizeof(text), "%.2f%s", cor[i],
                      c.entries[static_cast<std::size_t>(i)].corrected.extrapolated ? "*" : "");
        ImPlot::PlotText(text, xCor[i], cor[i], above);
    }
    ImPlot::EndPlot();
}

void App::drawComparisonTable()
{
    const ComparisonResult& c = comparison_;
    // Dar pencerede kısa adlar (uzunları kesilirdi); tam açıklamalar ipuçlarında
    const bool compact = ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 44.0f;
    static const char* const kHeadersFull[] = {"Strateji",       "Ham",       "SOC değişimi",
                                               "SOC düzeltmeli", "Fark##duz", "Eşdeğer yakıt",
                                               "Fark##esd"};
    static const char* const kHeadersCompact[] = {"Strateji",    "Ham",       "SOC farkı",
                                                  "Düzeltilmiş", "Fark##duz", "Eşdeğer",
                                                  "Fark##esd"};
    const char* const* kHeaders = compact ? kHeadersCompact : kHeadersFull;
    static const char* const kHelp[] = {
        "Enerji yönetim stratejisi",
        "Seçili başlangıç SOC'siyle tek koşunun tüketimi [L/100 km], SOC düzeltmesi yok",
        "Ham koşuda bitiş SOC'si - başlangıç SOC'si [yüzde puan]. Eksi: bataryayı boşalttı",
        "Ana sonuç [L/100 km]: batarya net değişmeden bitseydi harcanacak yakıt",
        "SOC düzeltmeli tüketimin kural tabanlıya göre farkı",
        "Kontrol yöntemi [L/100 km]: yakıt + bataryadan çekilen enerjinin yakıt karşılığı",
        "Eşdeğer yakıtın kural tabanlıya göre farkı"};
    constexpr int kColumns = 7;
    const float weights[kColumns] = {2.3f, 0.9f, 1.2f, 1.3f, 1.0f, 1.3f, 1.0f};
    if (!ImGui::BeginTable("karsilastirma", kColumns,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
                               | ImGuiTableFlags_SizingStretchProp)) {
        return;
    }
    for (int i = 0; i < kColumns; ++i) {
        ImGui::TableSetupColumn(kHeaders[i], ImGuiTableColumnFlags_WidthStretch, weights[i]);
    }
    // Başlıklar elle: her birinin üstüne gelince açıklama
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    for (int i = 0; i < kColumns; ++i) {
        ImGui::TableSetColumnIndex(i);
        ImGui::TableHeader(ImGui::TableGetColumnName(i));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kHelp[i]);
    }

    for (int i = 0; i < static_cast<int>(c.entries.size()); ++i) {
        const ComparisonEntry& e = c.entries[static_cast<std::size_t>(i)];
        const bool isRef = i == c.referenceIndex;
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const char* name = compact ? strategyShortLabel(e.kind) : strategyLabel(e.kind);
        if (i == c.bestIndex) {
            ImGui::TextColored(kOkColor, "%s", name);
        } else {
            ImGui::TextUnformatted(name);
        }
        if (compact && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", strategyLabel(e.kind));
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.2f", e.raw.fuelPer100km_L);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%+.1f", (e.raw.socEnd - e.raw.socStart) * 100.0);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.2f%s", e.correctedPer100km_L(), e.corrected.extrapolated ? "*" : "");
        if (ImGui::IsItemHovered()) {
            // Düzeltmenin ayrıntısı (hea_cli tablosundaki R2 ve b sütunları)
            ImGui::SetTooltip("%zu başlangıç SOC'siyle koşuldu\n"
                              "R² = %.3f (1: noktalar tam doğru üzerinde)\n"
                              "b = %.0f g/kWh (batarya enerjisinin yakıtla yerine konma maliyeti)%s",
                              e.corrected.points.size(), e.corrected.fit.r2,
                              -e.corrected.localSlope_kgPerJ * 3.6e9,
                              e.corrected.extrapolated ? "\n* dış değerleme" : "");
        }
        ImGui::TableSetColumnIndex(4);
        diffCell(c.correctedDiffPct(i), isRef);
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%.2f", e.equivalentPer100km_L());
        ImGui::TableSetColumnIndex(6);
        diffCell(c.equivalentDiffPct(i), isRef);
    }
    ImGui::EndTable();
}

} // namespace gui
