#include "hea/drive_cycle.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

#include "hea/units.h"

namespace hea {

namespace {

// Bu hızın altı "duruyor" sayılır [m/s]
constexpr double kStopSpeed = 0.01;

// Platformdan bağımsız rastgele sayı üreteci. std::mt19937'nin çıktısı standartta
// tanımlıdır, ama std::uniform_real_distribution derleyiciye göre farklı sonuç
// verebilir; bu yüzden [a, b) dönüşümünü kendimiz yapıyoruz.
class Rng {
public:
    explicit Rng(std::uint32_t seed) : gen_(seed) {}
    double uniform(double a, double b)
    {
        return a + (b - a) * (static_cast<double>(gen_()) / 4294967296.0);
    }

private:
    std::mt19937 gen_;
};

double kmh(double v) { return units::kmhToMps(v); }

// Rastgele eğim profili: her segmentLen mesafede yeni bir eğim noktası.
std::vector<GradePoint> randomGradeProfile(Rng& rng, double maxPercent,
                                           double segMin_m, double segMax_m)
{
    // Çevrimlerin hiçbiri 80 km'yi geçmez; fazlası kullanılmaz.
    constexpr double kProfileLength_m = 80000.0;
    std::vector<GradePoint> profile;
    profile.push_back({0.0, 0.0}); // düz yolda başla
    double d = rng.uniform(segMin_m, segMax_m);
    while (d < kProfileLength_m) {
        profile.push_back({d, rng.uniform(-maxPercent, maxPercent)});
        d += rng.uniform(segMin_m, segMax_m);
    }
    return profile;
}

// Şehir içi dur-kalk döngülerini, zaman untilTime_s'ye ulaşana kadar ekler.
// Her döngü durarak biter.
void appendUrban(CycleBuilder& b, Rng& rng, double untilTime_s)
{
    while (b.currentTime() < untilTime_s) {
        b.rampTo(kmh(rng.uniform(20.0, 55.0)), rng.uniform(0.8, 1.5));
        b.hold(rng.uniform(10.0, 50.0));
        if (rng.uniform(0.0, 1.0) < 0.4) {
            // Trafik akışında hız değişimi (yavaşla ya da hızlan)
            b.rampTo(kmh(rng.uniform(15.0, 50.0)), rng.uniform(0.5, 1.0));
            b.hold(rng.uniform(5.0, 30.0));
        }
        b.rampTo(0.0, rng.uniform(0.8, 1.6));
        b.hold(rng.uniform(5.0, 35.0)); // ışıkta bekleme
    }
}

// Yüksek hızlı seyir bölümü: [vMin, vMax] aralığında hız değişimleriyle.
// Durmadan biter; çağıran taraf yavaşlatır.
void appendCruise(CycleBuilder& b, Rng& rng, double untilTime_s,
                  double vMin_kmh, double vMax_kmh)
{
    while (b.currentTime() < untilTime_s) {
        b.rampTo(kmh(rng.uniform(vMin_kmh, vMax_kmh)), rng.uniform(0.2, 0.6));
        b.hold(rng.uniform(30.0, 150.0));
    }
}

DriveCycle mixedCycle(std::uint32_t seed, double maxGradePercent, const std::string& name)
{
    Rng rng(seed);
    CycleBuilder b(name);
    b.hold(rng.uniform(5.0, 15.0));
    appendUrban(b, rng, 600.0);                    // şehir içi
    b.rampTo(kmh(rng.uniform(60.0, 80.0)), 1.2);   // çevre yoluna çık
    appendCruise(b, rng, 1200.0, 60.0, 90.0);      // çevre yolu
    b.rampTo(0.0, 1.0);
    b.hold(rng.uniform(5.0, 20.0));
    appendUrban(b, rng, 1800.0);                   // tekrar şehir içi
    b.grade(randomGradeProfile(rng, maxGradePercent, 400.0, 1200.0));
    b.payload(rng.uniform(75.0, 300.0));
    b.ambient(rng.uniform(5.0, 30.0));
    return b.build();
}

} // namespace

// ---------------------------------------------------------------- DriveCycle

double DriveCycle::duration_s() const
{
    return samples.empty() ? 0.0 : samples.back().time_s;
}

double DriveCycle::distance_m() const
{
    double d = 0.0;
    for (std::size_t i = 1; i < samples.size(); ++i) {
        // Yamuk kuralı: (v_{i-1} + v_i) / 2 * dt
        d += 0.5 * (samples[i - 1].targetSpeed_mps + samples[i].targetSpeed_mps) * dt_s;
    }
    return d;
}

double DriveCycle::maxSpeed_mps() const
{
    double m = 0.0;
    for (const DriveInput& s : samples) {
        m = std::max(m, s.targetSpeed_mps);
    }
    return m;
}

double DriveCycle::averageSpeed_mps() const
{
    const double t = duration_s();
    return t > 0.0 ? distance_m() / t : 0.0;
}

double DriveCycle::stopFraction() const
{
    if (samples.empty()) {
        return 0.0;
    }
    std::size_t stopped = 0;
    for (const DriveInput& s : samples) {
        if (s.targetSpeed_mps < kStopSpeed) {
            ++stopped;
        }
    }
    return static_cast<double>(stopped) / static_cast<double>(samples.size());
}

double DriveCycle::maxAcceleration_mps2() const
{
    double m = 0.0;
    for (std::size_t i = 1; i < samples.size(); ++i) {
        m = std::max(m, (samples[i].targetSpeed_mps - samples[i - 1].targetSpeed_mps) / dt_s);
    }
    return m;
}

double DriveCycle::maxDeceleration_mps2() const
{
    double m = 0.0;
    for (std::size_t i = 1; i < samples.size(); ++i) {
        m = std::max(m, (samples[i - 1].targetSpeed_mps - samples[i].targetSpeed_mps) / dt_s);
    }
    return m;
}

// ---------------------------------------------------------------- Eğim

double gradeAtDistance(const std::vector<GradePoint>& profile, double distance_m)
{
    if (profile.empty()) {
        return 0.0;
    }
    if (distance_m <= profile.front().distance_m) {
        return units::gradePercentToRad(profile.front().gradePercent);
    }
    if (distance_m >= profile.back().distance_m) {
        return units::gradePercentToRad(profile.back().gradePercent);
    }
    const auto it = std::upper_bound(
        profile.begin(), profile.end(), distance_m,
        [](double d, const GradePoint& p) { return d < p.distance_m; });
    const GradePoint& hi = *it;
    const GradePoint& lo = *(it - 1);
    // Yüzde eğim noktalar arasında doğrusal değişir
    const double frac = (distance_m - lo.distance_m) / (hi.distance_m - lo.distance_m);
    return units::gradePercentToRad(lo.gradePercent + (hi.gradePercent - lo.gradePercent) * frac);
}

// ---------------------------------------------------------------- CycleBuilder

CycleBuilder::CycleBuilder(std::string name, double dt_s)
    : name_(std::move(name))
    , dt_(dt_s)
    , speeds_{0.0}
{
}

CycleBuilder& CycleBuilder::hold(double duration_s)
{
    const long n = std::lround(duration_s / dt_);
    const double v = speeds_.back();
    for (long i = 0; i < n; ++i) {
        speeds_.push_back(v);
    }
    return *this;
}

CycleBuilder& CycleBuilder::rampTo(double targetSpeed_mps, double rate_mps2)
{
    const double v0 = speeds_.back();
    const double target = std::max(targetSpeed_mps, 0.0);
    const double delta = target - v0;
    if (std::fabs(delta) < 1e-12 || rate_mps2 <= 0.0) {
        return *this;
    }
    // Adım sayısı yukarı yuvarlanır: gerçek ivme = |delta| / (n*dt) <= rate
    const long n = static_cast<long>(std::ceil(std::fabs(delta) / (rate_mps2 * dt_) - 1e-9));
    for (long k = 1; k < n; ++k) {
        speeds_.push_back(v0 + delta * static_cast<double>(k) / static_cast<double>(n));
    }
    // Son örnek tam hedef: yuvarlama hatası 0 yerine -1e-16 gibi negatif hız üretmesin
    speeds_.push_back(target);
    return *this;
}

CycleBuilder& CycleBuilder::grade(std::vector<GradePoint> profile)
{
    grade_ = std::move(profile);
    return *this;
}

CycleBuilder& CycleBuilder::payload(double kg)
{
    payload_kg_ = kg;
    return *this;
}

CycleBuilder& CycleBuilder::ambient(double temp_C)
{
    ambient_C_ = temp_C;
    return *this;
}

DriveCycle CycleBuilder::build() const
{
    DriveCycle c;
    c.name = name_;
    c.dt_s = dt_;
    c.samples.reserve(speeds_.size());
    double distance = 0.0;
    for (std::size_t i = 0; i < speeds_.size(); ++i) {
        if (i > 0) {
            distance += 0.5 * (speeds_[i - 1] + speeds_[i]) * dt_;
        }
        DriveInput in;
        in.time_s = static_cast<double>(i) * dt_;
        in.targetSpeed_mps = speeds_[i];
        in.grade_rad = gradeAtDistance(grade_, distance);
        in.payload_kg = payload_kg_;
        in.ambientTemp_C = ambient_C_;
        c.samples.push_back(in);
    }
    return c;
}

// ---------------------------------------------------------------- Üreteçler

DriveCycle makeUrbanCycle(std::uint32_t seed)
{
    Rng rng(seed);
    CycleBuilder b("sehir_" + std::to_string(seed));
    b.hold(rng.uniform(5.0, 15.0));
    appendUrban(b, rng, 1200.0);
    b.grade(randomGradeProfile(rng, 2.0, 300.0, 800.0));
    b.payload(rng.uniform(75.0, 250.0));
    b.ambient(rng.uniform(10.0, 30.0));
    return b.build();
}

DriveCycle makeHighwayCycle(std::uint32_t seed)
{
    Rng rng(seed);
    CycleBuilder b("otoyol_" + std::to_string(seed));
    b.hold(10.0);
    b.rampTo(kmh(rng.uniform(80.0, 100.0)), 1.2); // otoyola katılım
    appendCruise(b, rng, 1150.0, 90.0, 130.0);
    b.rampTo(0.0, 1.0);
    b.hold(10.0);
    b.grade(randomGradeProfile(rng, 3.0, 1000.0, 3000.0));
    b.payload(rng.uniform(75.0, 250.0));
    b.ambient(rng.uniform(10.0, 30.0));
    return b.build();
}

DriveCycle makeMixedCycle(std::uint32_t seed)
{
    return mixedCycle(seed, 5.0, "karma_" + std::to_string(seed));
}

std::vector<DriveCycle> trainingCycles()
{
    return {makeUrbanCycle(1), makeUrbanCycle(2), makeUrbanCycle(3),
            makeHighwayCycle(11), makeHighwayCycle(12),
            makeMixedCycle(21), makeMixedCycle(22)};
}

std::vector<DriveCycle> testCycles()
{
    std::vector<DriveCycle> cycles = {makeUrbanCycle(101), makeHighwayCycle(111),
                                      makeMixedCycle(121)};
    // Eğitimde olmayan zorlu koşul: %7'ye varan tepeler, 400 kg yük, 0 °C
    DriveCycle hard = mixedCycle(131, 7.0, "zorlu_131");
    for (DriveInput& s : hard.samples) {
        s.payload_kg = 400.0;
        s.ambientTemp_C = 0.0;
    }
    cycles.push_back(std::move(hard));
    return cycles;
}

} // namespace hea
