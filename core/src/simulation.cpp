#include "hea/simulation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "hea/units.h"

namespace hea {

namespace {
// Bu hızın altı "duruyor" sayılır [m/s]
constexpr double kStopSpeed = 1e-3;
} // namespace

// ---------------------------------------------------------------- HVAC

bool HvacParams::isValid() const
{
    return deadband_K >= 0.0 && coolingCoeff_WperK >= 0.0 && heatingCoeff_WperK >= 0.0
        && maxPower_W >= 0.0;
}

double HvacParams::power_W(double ambientTemp_C) const
{
    if (!enabled) {
        return 0.0;
    }
    double p = 0.0;
    if (ambientTemp_C > comfortTemp_C + deadband_K) {
        p = coolingCoeff_WperK * (ambientTemp_C - comfortTemp_C);
    } else if (ambientTemp_C < comfortTemp_C - deadband_K) {
        p = heatingCoeff_WperK * (comfortTemp_C - ambientTemp_C);
    }
    return std::min(p, maxPower_W);
}

// ---------------------------------------------------------------- Doğrulama

std::string SimulationConfig::validate() const
{
    if (!vehicle.isValid()) return "arac parametreleri gecersiz (kutle, Cd, alan, Crr, r > 0, delta >= 1)";
    if (!battery.isValid()) return "batarya parametreleri gecersiz";
    if (!motor.isValid()) return "elektrik motoru parametreleri gecersiz";
    if (!engine.isValid()) return "icten yanmali motor parametreleri gecersiz";
    if (!drivetrain.isValid()) return "aktarma organi parametreleri gecersiz";
    if (!hvac.isValid()) return "klima parametreleri gecersiz";
    if (!(initialSoc >= battery.socMin && initialSoc <= battery.socMax)) {
        return "baslangic SOC'si batarya penceresinin (socMin..socMax) disinda";
    }
    if (!(auxiliaryPower_W >= 0.0)) return "yardimci yuk negatif olamaz";
    if (!(tireRoadFriction > 0.0)) return "lastik-yol surtunme katsayisi pozitif olmali";
    if (!(engineStartFuel_kg >= 0.0)) return "motor calistirma yakiti negatif olamaz";
    return {};
}

// ---------------------------------------------------------------- Simulation

Simulation::Simulation(const SimulationConfig& config, const DriveCycle& cycle,
                       Strategy& strategy)
    : config_(config)
    , cycle_(cycle)
    , strategy_(strategy)
    , vehicle_(config.vehicle)
    , powertrain_(config.motor, config.engine, config.drivetrain)
    , battery_(config.battery, config.initialSoc,
               cycle.samples.empty() ? 25.0 : cycle.samples.front().ambientTemp_C)
    , speed_(cycle.samples.empty() ? 0.0 : cycle.samples.front().targetSpeed_mps)
{
    const std::string err = config.validate();
    if (!err.empty()) {
        throw std::invalid_argument("Gecersiz yapilandirma: " + err);
    }
    if (!(cycle.dt_s > 0.0)) {
        throw std::invalid_argument("Cevrim '" + cycle.name + "': adim suresi pozitif olmali");
    }
    const double ambient0 = cycle.samples.empty() ? 25.0 : cycle.samples.front().ambientTemp_C;
    engineTemp_C_ = config.engineColdStart ? ambient0 : std::max(kEngineWarmTemp_C, ambient0);
    strategy_.reset();
}

StepResult Simulation::step()
{
    if (finished()) {
        throw std::logic_error("Simulation::step: cevrim bitti, adim kalmadi");
    }
    const DriveInput& in = cycle_.samples[next_];
    const double dt = cycle_.dt_s;
    const double r = config_.vehicle.wheelRadius_m;
    const double m = vehicle_.totalMass(in.payload_kg);
    const double v0 = speed_;
    const double vTarget = in.targetSpeed_mps;

    StepResult out;
    out.time_s = in.time_s;
    out.dt_s = dt;
    out.targetSpeed_mps = vTarget;
    out.grade_rad = in.grade_rad;
    out.payload_kg = in.payload_kg;

    // --- 1) Talep: hedef hıza ulaşmak için gereken ivme ve tekerlek torku ---
    // Araç duruyor ve durmaya devam edecekse fren aracı tutar, tahrik istenmez.
    const bool holding = v0 < kStopSpeed && vTarget < kStopSpeed;
    const double aRequested = holding ? 0.0 : (vTarget - v0) / dt;
    const double vAvg = 0.5 * (v0 + vTarget);
    const RoadLoad load = vehicle_.roadLoad(vAvg, aRequested, in.grade_rad, in.payload_kg);
    const double wheelTorque = holding ? 0.0 : vehicle_.wheelTorque(load.total_N);
    const double wheelSpeed = vehicle_.wheelSpeed(vAvg);

    // --- 2) Batarya sınırları (yardımcı yük + klima düşülerek) ---
    const double aux = config_.auxiliaryPower_W + config_.hvac.power_W(in.ambientTemp_C);
    const double availDischarge = battery_.availableDischargePower();
    const double maxDischarge = std::max(0.0, availDischarge - aux);
    const double maxCharge = battery_.availableChargePower() + aux;
    // Batarya yardımcı yükü bile karşılayamıyorsa açık, motor çalışırsa jeneratörle üretilir
    const double auxDeficit = std::max(0.0, aux - availDischarge);

    // --- 3) Strateji kararı ---
    StrategyInput si;
    si.time_s = in.time_s;
    si.speed_mps = vAvg;
    si.acceleration_mps2 = aRequested;
    si.grade_rad = in.grade_rad;
    si.payload_kg = in.payload_kg;
    si.wheelTorque_Nm = wheelTorque;
    si.wheelSpeed_radps = wheelSpeed;
    si.wheelPower_W = wheelTorque * wheelSpeed;
    si.soc = battery_.soc();
    si.batteryTemp_C = battery_.temperature_C();
    si.engineTemp_C = engineTemp_C_;
    si.batteryMaxDischarge_W = maxDischarge;
    si.batteryMaxCharge_W = maxCharge;
    const Decision decision = strategy_.decide(si);

    // --- 4) Güç aktarma organı ---
    PowertrainRequest req;
    req.wheelTorque_Nm = wheelTorque;
    req.wheelSpeed_radps = wheelSpeed;
    req.requestedMode = decision.mode == Mode::Engine ? Mode::Engine : Mode::Electric;
    req.chargePower_W = std::max(0.0, decision.chargePower_W) + auxDeficit;
    req.batteryMaxDischarge_W = maxDischarge;
    req.batteryMaxCharge_W = maxCharge;
    req.engineTemp_C = engineTemp_C_;
    const PowertrainResult pt = powertrain_.resolve(req);

    // --- 5) Gerçekleşen hareket ---
    double aActual = aRequested;
    if (pt.unmetWheelTorque_Nm > 0.0) {
        // Verilen tork yetmedi: a = (F_verilen - F_aero - F_yuv - F_egim) / (delta * m)
        const double fDelivered = pt.deliveredWheelTorque_Nm / r;
        aActual = (fDelivered - load.aero_N - load.rolling_N - load.grade_N)
                / (config_.vehicle.rotationalMassFactor * m);
    }
    const double v1 = std::max(0.0, v0 + aActual * dt);
    distance_ += 0.5 * (v0 + v1) * dt; // yamuk kuralı

    // --- 6) Batarya ve yakıt ---
    const double batteryRequest = pt.motorElectricalPower_W + aux;
    const BatteryStep bs = battery_.step(batteryRequest, dt, in.ambientTemp_C);
    // Deşarj isteği kısıldıysa eksik kalan elektrik gücü (enerji dengesi için raporlanır)
    out.unmetElectricalPower_W = batteryRequest > 0.0 ? std::max(0.0, batteryRequest - bs.power_W)
                                                      : 0.0;
    fuelUsed_ += pt.fuelRate_kgps * dt;
    // Motor dönüyor mu? Yakıt yakıyorsa ya da strateji motoru açık tutuyorken araç
    // hareket ediyorsa (fren / inişte yakıt kesme). Dönmüyorken dönmeye başlarsa çalıştırma.
    const bool engineRunning = pt.fuelRate_kgps > 0.0
        || (req.requestedMode == Mode::Engine && pt.mode != Mode::Stopped && v1 > kStopSpeed);
    out.engineRunning = engineRunning;
    out.engineStarted = engineRunning && !engineRunning_;
    if (out.engineStarted) {
        fuelUsed_ += config_.engineStartFuel_kg;
    }
    engineRunning_ = engineRunning;
    batteryEnergy_ += bs.power_W * dt;
    // Motor ısıl modeli: yakıt gücünün bir kısmı motoru ısıtır, ortama ısı kaybı soğutur
    engineTemp_C_ = powertrain_.engine().nextTemperature(
        engineTemp_C_, pt.fuelRate_kgps * constants::kGasolineLhv, in.ambientTemp_C, dt);

    // --- 7) Sürücü pedalları (talepten türetilen göstergeler) ---
    if (wheelTorque > 0.0) {
        const double tMax = powertrain_.maxWheelTorque(wheelSpeed);
        out.throttle = tMax > 0.0 ? std::clamp(wheelTorque / tMax, 0.0, 1.0) : 1.0;
    }
    if (!holding && load.total_N < 0.0) {
        // fren = -F_cekis / (mu * m * g)
        const double fMax = config_.tireRoadFriction * m * constants::kGravity;
        out.brake = std::clamp(-load.total_N / fMax, 0.0, 1.0);
    }

    // --- 8) Sonuç ---
    out.speed_mps = v1;
    out.acceleration_mps2 = aActual;
    out.distance_m = distance_;
    out.aeroForce_N = load.aero_N;
    out.rollingForce_N = load.rolling_N;
    out.gradeForce_N = load.grade_N;
    out.inertiaForce_N = load.inertia_N;
    out.tractionForce_N = holding ? 0.0 : load.total_N;
    out.wheelTorque_Nm = wheelTorque;
    out.wheelPower_W = wheelTorque * wheelSpeed;
    out.unmetWheelTorque_Nm = pt.unmetWheelTorque_Nm;

    out.requestedMode = req.requestedMode;
    out.mode = pt.mode;
    out.modeOverridden = pt.overridden;
    out.gear = pt.gear;
    out.engineSpeed_radps = pt.engineSpeed_radps;
    out.engineTorque_Nm = pt.engineTorque_Nm;
    out.engineTemp_C = engineTemp_C_;
    out.motorSpeed_radps = pt.motorSpeed_radps;
    out.motorTorque_Nm = pt.motorTorque_Nm;
    out.motorElectricalPower_W = pt.motorElectricalPower_W;
    // Sürtünmeli fren gücü = -T_fren * omega_tekerlek (>= 0)
    out.frictionBrakePower_W = std::max(0.0, -pt.frictionBrakeTorque_Nm * wheelSpeed);

    out.batteryPower_W = bs.power_W;
    out.batteryVoltage_V = bs.voltage_V;
    out.batteryCurrent_A = bs.current_A;
    out.batteryTemp_C = bs.temp_C;
    out.soc = bs.soc;
    out.auxiliaryPower_W = aux;

    out.fuelRate_kgps = pt.fuelRate_kgps;
    out.fuelUsed_kg = fuelUsed_;
    out.batteryEnergyUsed_J = batteryEnergy_;

    speed_ = v1;
    ++next_;
    return out;
}

std::vector<StepResult> runCycle(const SimulationConfig& config, const DriveCycle& cycle,
                                 Strategy& strategy)
{
    Simulation sim(config, cycle, strategy);
    std::vector<StepResult> steps;
    steps.reserve(cycle.samples.size());
    while (!sim.finished()) {
        steps.push_back(sim.step());
    }
    return steps;
}

RunSummary summarize(const std::vector<StepResult>& steps, double initialSoc)
{
    RunSummary s;
    s.socStart = initialSoc;
    s.socEnd = initialSoc;
    if (steps.empty()) {
        return s;
    }
    std::size_t overridden = 0;
    std::size_t driving = 0;
    for (const StepResult& r : steps) {
        s.duration_s += r.dt_s;
        switch (r.mode) {
        case Mode::Electric:     s.timeElectric += r.dt_s; break;
        case Mode::Engine:       s.timeEngine += r.dt_s; break;
        case Mode::RegenBraking: s.timeRegen += r.dt_s; break;
        case Mode::Stopped:      s.timeStopped += r.dt_s; break;
        }
        if (r.mode == Mode::Electric || r.mode == Mode::Engine) {
            ++driving;
            if (r.modeOverridden) {
                ++overridden;
            }
        }
        if (r.mode == Mode::RegenBraking && r.motorElectricalPower_W < 0.0) {
            s.regenEnergy_J += -r.motorElectricalPower_W * r.dt_s;
        }
        s.frictionBrakeEnergy_J += r.frictionBrakePower_W * r.dt_s;
        s.auxiliaryEnergy_J += r.auxiliaryPower_W * r.dt_s;
        s.unmetElectricalEnergy_J += r.unmetElectricalPower_W * r.dt_s;
        if (r.engineStarted) {
            ++s.engineStarts;
        }
        s.maxSpeedError_mps = std::max(s.maxSpeedError_mps,
                                       std::fabs(r.targetSpeed_mps - r.speed_mps));
    }
    const StepResult& last = steps.back();
    s.distance_m = last.distance_m;
    s.fuel_kg = last.fuelUsed_kg;
    s.fuel_L = s.fuel_kg / constants::kGasolineDensity;
    // L/100 km = L / (mesafe [km] / 100) = L * 1e5 / mesafe [m]
    s.fuelPer100km_L = s.distance_m > 0.0 ? s.fuel_L * 1e5 / s.distance_m : 0.0;
    s.socEnd = last.soc;
    s.batteryEnergyUsed_J = last.batteryEnergyUsed_J;
    if (s.duration_s > 0.0) {
        s.timeElectric /= s.duration_s;
        s.timeEngine /= s.duration_s;
        s.timeRegen /= s.duration_s;
        s.timeStopped /= s.duration_s;
    }
    s.overriddenFraction = driving > 0
        ? static_cast<double>(overridden) / static_cast<double>(driving) : 0.0;
    return s;
}

} // namespace hea
