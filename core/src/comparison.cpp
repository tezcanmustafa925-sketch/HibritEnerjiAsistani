#include "hea/comparison.h"

#include <algorithm>
#include <cstddef>

namespace hea {

LineFit fitLine(const std::vector<double>& x, const std::vector<double>& y)
{
    LineFit f;
    const std::size_t n = std::min(x.size(), y.size());
    if (n == 0) {
        return f;
    }
    double mx = 0.0, my = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        mx += x[i];
        my += y[i];
    }
    mx /= static_cast<double>(n);
    my /= static_cast<double>(n);

    // b = sum((x - mx)(y - my)) / sum((x - mx)^2),  a = my - b * mx
    double sxy = 0.0, sxx = 0.0, syy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sxy += (x[i] - mx) * (y[i] - my);
        sxx += (x[i] - mx) * (x[i] - mx);
        syy += (y[i] - my) * (y[i] - my);
    }
    if (sxx <= 0.0) {
        // Tüm x aynı: eğim belirsiz, ortalama döndürülür
        f.intercept = my;
        f.r2 = 0.0;
        return f;
    }
    f.slope = sxy / sxx;
    f.intercept = my - f.slope * mx;
    // R^2 = 1 - SS_artik / SS_toplam = sxy^2 / (sxx * syy)
    f.r2 = syy > 0.0 ? (sxy * sxy) / (sxx * syy) : 1.0;
    return f;
}

double litersPer100km(double fuel_kg, double distance_m)
{
    return distance_m > 0.0 ? fuel_kg / constants::kGasolineDensity * 1e5 / distance_m : 0.0;
}

double SocCorrectedFuel::fuelPer100km_L() const
{
    return litersPer100km(fuel_kg(), distance_m);
}

std::vector<double> defaultCorrectionSocs()
{
    return {0.40, 0.45, 0.50, 0.55, 0.60, 0.65, 0.70, 0.75, 0.78};
}

SocCorrectedFuel socCorrectedFuel(const SimulationConfig& config, const DriveCycle& cycle,
                                  Strategy& strategy, const std::vector<double>& initialSocs)
{
    SocCorrectedFuel out;
    std::vector<double> energy, fuel;
    for (double soc0 : initialSocs) {
        SimulationConfig cfg = config;
        cfg.initialSoc = soc0;
        const auto steps = runCycle(cfg, cycle, strategy); // strateji her koşuda sıfırlanır
        const RunSummary s = summarize(steps, soc0);
        CorrectionPoint p;
        p.initialSoc = soc0;
        p.finalSoc = s.socEnd;
        // Depolanan enerji kaybı: SOC başlangıçtan bitişe inerken çıkan kimyasal enerji
        p.batteryEnergy_J = storedEnergyReleased_J(cfg.battery, soc0, s.socEnd);
        p.fuel_kg = s.fuel_kg;
        out.points.push_back(p);
        out.distance_m = s.distance_m;
        energy.push_back(p.batteryEnergy_J);
        fuel.push_back(p.fuel_kg);
    }
    out.fit = fitLine(energy, fuel);
    if (out.points.size() < 2) {
        out.correctedFuel_kg = out.fit.intercept;
        out.extrapolated = true;
        return out;
    }

    // Noktaları E_bat'a göre sırala, E_bat = 0'ı kapsayan komşu çifti bul
    std::vector<CorrectionPoint> sorted = out.points;
    std::sort(sorted.begin(), sorted.end(),
              [](const CorrectionPoint& a, const CorrectionPoint& b) {
                  return a.batteryEnergy_J < b.batteryEnergy_J;
              });
    std::size_t i1 = 0, i2 = 1;
    out.extrapolated = true;
    for (std::size_t i = 0; i + 1 < sorted.size(); ++i) {
        if (sorted[i].batteryEnergy_J <= 0.0 && 0.0 <= sorted[i + 1].batteryEnergy_J) {
            i1 = i;
            i2 = i + 1;
            out.extrapolated = false;
            break;
        }
    }
    if (out.extrapolated && sorted.front().batteryEnergy_J > 0.0) {
        i1 = 0; i2 = 1;                                   // hepsi pozitif: en küçük ikisi
    } else if (out.extrapolated) {
        i1 = sorted.size() - 2; i2 = sorted.size() - 1;  // hepsi negatif: en büyük ikisi
    }
    const CorrectionPoint& p1 = sorted[i1];
    const CorrectionPoint& p2 = sorted[i2];
    const double dE = p2.batteryEnergy_J - p1.batteryEnergy_J;
    if (dE <= 0.0) {
        out.correctedFuel_kg = 0.5 * (p1.fuel_kg + p2.fuel_kg); // aynı E: ortalama
        out.localSlope_kgPerJ = 0.0;
        return out;
    }
    // b = (m2 - m1) / (E2 - E1),  m_duz = m1 + b * (0 - E1)
    out.localSlope_kgPerJ = (p2.fuel_kg - p1.fuel_kg) / dE;
    out.correctedFuel_kg = p1.fuel_kg + out.localSlope_kgPerJ * (0.0 - p1.batteryEnergy_J);
    return out;
}

double equivalentFuel_kg(double fuel_kg, double batteryEnergy_J, double kgPerJoule)
{
    // m_esd = m_yakit + k * E_bat
    return fuel_kg + kgPerJoule * batteryEnergy_J;
}

} // namespace hea
