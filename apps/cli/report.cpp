#include "report.h"

#include <cmath>
#include <cstdio>

#include "hea/units.h"

namespace cli {

using hea::units::mpsToKmh;
using hea::units::radpsToRpm;

void printStepHeader()
{
    // Birimler başlığın ikinci satırında
    std::printf("%7s %6s %6s %5s %4s %4s %-6s %2s %5s %5s %5s %5s %6s %6s %5s %5s %6s %7s\n",
                "t", "hedef", "hiz", "egim", "gaz", "fren", "mod", "v", "ICE", "ICE",
                "EM", "EM", "bat", "bat", "bat", "SOC", "yakit", "yakit");
    std::printf("%7s %6s %6s %5s %4s %4s %-6s %2s %5s %5s %5s %5s %6s %6s %5s %5s %6s %7s\n",
                "[s]", "[km/h]", "[km/h]", "[%]", "[%]", "[%]", "", "", "[rpm]", "[Nm]",
                "[rpm]", "[Nm]", "[V]", "[A]", "[C]", "[%]", "[g/s]", "top[g]");
}

void printStep(const hea::StepResult& r)
{
    // Öneri uygulanamadıysa mod adının yanına '*' konur (ör. EV istendi, ICE çalıştı)
    char mode[8];
    std::snprintf(mode, sizeof(mode), "%s%s", hea::modeName(r.mode), r.modeOverridden ? "*" : "");
    std::printf("%7.0f %6.1f %6.1f %5.1f %4.0f %4.0f %-6s %2d %5.0f %5.1f %5.0f %5.1f "
                "%6.1f %6.1f %5.1f %5.1f %6.3f %7.1f\n",
                r.time_s, mpsToKmh(r.targetSpeed_mps), mpsToKmh(r.speed_mps),
                std::tan(r.grade_rad) * 100.0, 100.0 * r.throttle, 100.0 * r.brake, mode,
                r.gear, radpsToRpm(r.engineSpeed_radps), r.engineTorque_Nm,
                radpsToRpm(r.motorSpeed_radps), r.motorTorque_Nm, r.batteryVoltage_V,
                r.batteryCurrent_A, r.batteryTemp_C, 100.0 * r.soc,
                1000.0 * r.fuelRate_kgps, 1000.0 * r.fuelUsed_kg);
}

void printSummaryTable(const std::vector<RunRecord>& runs)
{
    std::printf("\nOZET (ham degerler, SOC farki duzeltilmemis; adil karsilastirma icin: --karsilastir)\n");
    std::printf("%-15s %-13s %7s %8s %7s %6s %6s %7s %5s %5s %5s %5s %6s %6s %6s\n",
                "cevrim", "strateji", "yol", "L/100km", "yakit", "SOC0", "SOC1", "dSOC",
                "EV", "ICE", "REG", "DUR", "calis.", "rejen", "uyg.yok");
    std::printf("%-15s %-13s %7s %8s %7s %6s %6s %7s %5s %5s %5s %5s %6s %6s %6s\n",
                "", "", "[km]", "", "[L]", "[%]", "[%]", "[%]", "[%]", "[%]", "[%]", "[%]",
                "[adet]", "[Wh]", "[%]");
    std::string lastCycle;
    for (const RunRecord& run : runs) {
        if (!lastCycle.empty() && run.cycle != lastCycle) {
            std::printf("\n"); // çevrim grupları arasında boş satır
        }
        lastCycle = run.cycle;
        const hea::RunSummary& s = run.summary;
        std::printf("%-15s %-13s %7.2f %8.2f %7.3f %6.1f %6.1f %+7.1f %5.1f %5.1f %5.1f %5.1f "
                    "%6d %6.0f %6.1f\n",
                    run.cycle.c_str(), run.strategy.c_str(), s.distance_m / 1000.0,
                    s.fuelPer100km_L, s.fuel_L, 100.0 * s.socStart, 100.0 * s.socEnd,
                    100.0 * (s.socEnd - s.socStart), 100.0 * s.timeElectric,
                    100.0 * s.timeEngine, 100.0 * s.timeRegen, 100.0 * s.timeStopped,
                    s.engineStarts, s.regenEnergy_J / 3600.0, 100.0 * s.overriddenFraction);
    }
    // Enerji dengesi uyarısı: batarya alt sınırdayken yardımcı yük karşılanamadıysa
    for (const RunRecord& run : runs) {
        if (run.summary.unmetElectricalEnergy_J > 3600.0) { // 1 Wh
            std::printf("UYARI: %s / %s: %.1f Wh elektrik yuku karsilanamadi "
                        "(batarya SOC alt sinirinda, motor calismiyordu)\n",
                        run.cycle.c_str(), run.strategy.c_str(),
                        run.summary.unmetElectricalEnergy_J / 3600.0);
        }
    }
}

void printCorrectionPoints(const std::vector<ComparisonRow>& rows)
{
    std::printf("\nDUZELTME NOKTALARI (her satir bir kosu; E_bat = depolanan enerji kaybi)\n");
    std::printf("%-15s %-13s %6s %6s %8s %8s\n", "cevrim", "strateji", "SOC0", "SOC1",
                "E_bat", "yakit");
    std::printf("%-15s %-13s %6s %6s %8s %8s\n", "", "", "[%]", "[%]", "[Wh]", "[g]");
    for (const ComparisonRow& r : rows) {
        for (const hea::CorrectionPoint& p : r.corrected.points) {
            std::printf("%-15s %-13s %6.1f %6.1f %+8.1f %8.1f\n", r.cycle.c_str(),
                        r.strategy.c_str(), 100.0 * p.initialSoc, 100.0 * p.finalSoc,
                        p.batteryEnergy_J / 3600.0, 1000.0 * p.fuel_kg);
        }
    }
}

void printComparisonTable(const std::vector<ComparisonRow>& rows,
                          const std::string& referenceStrategy)
{
    std::printf("\nSOC DUZELTMELI KARSILASTIRMA\n");
    std::printf("Ana yontem: dogrusal SOC duzeltmesi (SAE J1711 mantigi). Her strateji 9 farkli\n"
                "baslangic SOC'si (0.40-0.78) ile kosulur; E_bat = 0 (batarya net degismeden)\n"
                "icin yakit, sifirin iki yanindaki en yakin iki noktadan ara degerle bulunur.\n"
                "E_bat = SOC degisiminden depolanan enerji (I^2R kayiplari haric).\n"
                "b = bu iki nokta arasi egim. R2 = tum noktalara oturan dogru (dogrusallik).\n"
                "'*' = sifir kapsanmadi, dis degerleme (guvenilirligi dusuk).\n"
                "Kontrol: esdeger yakit = yakit + k*E_bat, k = 2.8/LHV (1 kWh = 234 g).\n");
    std::printf("\n%-15s %-13s %7s %6s %8s %6s %7s %8s %7s %8s\n", "cevrim", "strateji",
                "ham", "dSOC", "DUZELT.", "R2", "b", "fark", "esdeger", "fark");
    std::printf("%-15s %-13s %7s %6s %8s %6s %7s %8s %7s %8s\n", "", "", "L/100km", "[%]",
                "L/100km", "", "[g/kWh]", "[%]", "L/100km", "[%]");

    // Referans stratejinin çevrim bazında değerleri
    auto findRef = [&](const std::string& cycle) -> const ComparisonRow* {
        for (const ComparisonRow& r : rows) {
            if (r.cycle == cycle && r.strategy == referenceStrategy) return &r;
        }
        return nullptr;
    };
    auto pctDiff = [](double v, double ref) { return ref > 0.0 ? 100.0 * (v / ref - 1.0) : 0.0; };

    std::string lastCycle;
    for (const ComparisonRow& r : rows) {
        if (!lastCycle.empty() && r.cycle != lastCycle) std::printf("\n");
        lastCycle = r.cycle;
        const ComparisonRow* ref = findRef(r.cycle);
        const double corr = r.corrected.fuelPer100km_L();
        const double eq = hea::litersPer100km(
            hea::equivalentFuel_kg(r.raw.fuel_kg, r.rawStoredEnergy_J), r.raw.distance_m);
        double corrRef = 0.0, eqRef = 0.0;
        if (ref != nullptr) {
            corrRef = ref->corrected.fuelPer100km_L();
            eqRef = hea::litersPer100km(
                hea::equivalentFuel_kg(ref->raw.fuel_kg, ref->rawStoredEnergy_J),
                ref->raw.distance_m);
        }
        char corrText[16];
        std::snprintf(corrText, sizeof(corrText), "%.2f%s", corr,
                      r.corrected.extrapolated ? "*" : "");
        std::printf("%-15s %-13s %7.2f %+6.1f %8s %6.3f %7.0f %+8.1f %7.2f %+8.1f\n",
                    r.cycle.c_str(), r.strategy.c_str(), r.raw.fuelPer100km_L,
                    100.0 * (r.raw.socEnd - r.raw.socStart), corrText, r.corrected.fit.r2,
                    -r.corrected.localSlope_kgPerJ * 3.6e9, pctDiff(corr, corrRef), eq,
                    pctDiff(eq, eqRef));
    }

    // Tüm çevrimler üzerinden yol ağırlıklı toplam: sum(yakıt) / sum(yol)
    std::printf("\nTOPLAM (tum cevrimler, yol agirlikli; fark: %s'ye gore)\n",
                referenceStrategy.c_str());
    std::printf("%-13s %8s %8s %8s %8s %6s\n", "strateji", "DUZELT.", "fark", "esdeger", "fark",
                "dis.d.");
    std::printf("%-13s %8s %8s %8s %8s %6s\n", "", "L/100km", "[%]", "L/100km", "[%]", "[adet]");
    std::vector<std::string> strategies;
    for (const ComparisonRow& r : rows) {
        bool seen = false;
        for (const std::string& s : strategies) seen = seen || s == r.strategy;
        if (!seen) strategies.push_back(r.strategy);
    }
    auto totals = [&](const std::string& strategy, double& corr, double& eq, int& extra) {
        double fuelC = 0.0, fuelE = 0.0, dist = 0.0;
        extra = 0;
        for (const ComparisonRow& r : rows) {
            if (r.strategy != strategy) continue;
            fuelC += r.corrected.fuel_kg();
            fuelE += hea::equivalentFuel_kg(r.raw.fuel_kg, r.rawStoredEnergy_J);
            dist += r.raw.distance_m;
            extra += r.corrected.extrapolated ? 1 : 0;
        }
        corr = hea::litersPer100km(fuelC, dist);
        eq = hea::litersPer100km(fuelE, dist);
    };
    double refC = 0.0, refE = 0.0;
    int refX = 0;
    totals(referenceStrategy, refC, refE, refX);
    for (const std::string& s : strategies) {
        double c = 0.0, e = 0.0;
        int x = 0;
        totals(s, c, e, x);
        std::printf("%-13s %8.3f %+8.1f %8.3f %+8.1f %6d\n", s.c_str(), c, pctDiff(c, refC), e,
                    pctDiff(e, refE), x);
    }
}

} // namespace cli
