#include "hea/powertrain.h"

#include <algorithm>
#include <cstddef>

namespace hea {

namespace {
// Bu tekerlek devrinin altında araç "duruyor" kabul edilir [rad/s]
constexpr double kStandstillWheelSpeed = 1e-3;
constexpr double kTorqueEps = 1e-9;
} // namespace

bool DrivetrainParams::isValid() const
{
    if (gearRatios.empty()) {
        return false;
    }
    for (double g : gearRatios) {
        if (g <= 0.0) {
            return false;
        }
    }
    return motorGearRatio > 0.0 && finalDriveRatio > 0.0
        && motorGearEfficiency > 0.0 && motorGearEfficiency <= 1.0
        && gearboxEfficiency > 0.0 && gearboxEfficiency <= 1.0
        && regenMinWheelSpeed_radps >= 0.0;
}

Powertrain::Powertrain(const MotorParams& motor, const EngineParams& engine,
                       const DrivetrainParams& drivetrain)
    : motor_(motor)
    , engine_(engine)
    , drivetrain_(drivetrain)
{
}

PowertrainResult Powertrain::resolve(const PowertrainRequest& req) const
{
    // 1) Duran araç
    if (req.wheelSpeed_radps < kStandstillWheelSpeed && req.wheelTorque_Nm <= 0.0) {
        PowertrainResult out;
        out.mode = Mode::Stopped;
        // Eğimde duran aracı (negatif tork talebi) sürtünmeli fren tutar
        out.frictionBrakeTorque_Nm = req.wheelTorque_Nm;
        out.deliveredWheelTorque_Nm = req.wheelTorque_Nm;
        return out;
    }

    // 2) Frenleme / iniş
    if (req.wheelTorque_Nm < 0.0) {
        return solveRegen(req);
    }

    // 3) Elektrikli tahrik isteği
    if (req.requestedMode == Mode::Electric) {
        PowertrainResult out;
        if (solveElectric(req, out)) {
            return out;
        }
        PowertrainResult fallback = solveEngine(req);
        fallback.overridden = true;
        return fallback;
    }

    // 4) İçten yanmalı motorla tahrik
    return solveEngine(req);
}

double Powertrain::maxWheelTorque(double wheelSpeed_radps) const
{
    const DrivetrainParams& d = drivetrain_;
    const EngineParams& e = engine_.params();
    double engineAxle = 0.0;
    for (std::size_t i = 0; i < d.gearRatios.size(); ++i) {
        const double g = d.gearRatios[i] * d.finalDriveRatio;
        const double wGear = wheelSpeed_radps * g;
        if (wGear > e.maxSpeed_radps || (wGear < e.idleSpeed_radps && i != 0)) {
            continue;
        }
        const double wEngine = std::max(wGear, e.idleSpeed_radps);
        engineAxle = std::max(engineAxle, engine_.maxTorque(wEngine) * g * d.gearboxEfficiency);
    }
    const double motorAxle = motor_.maxTorque(wheelSpeed_radps * d.motorGearRatio)
                           * d.motorGearRatio * d.motorGearEfficiency;
    return engineAxle + motorAxle;
}

PowertrainResult Powertrain::solveRegen(const PowertrainRequest& req) const
{
    const DrivetrainParams& d = drivetrain_;
    PowertrainResult out;
    out.mode = Mode::RegenBraking;
    out.motorSpeed_radps = req.wheelSpeed_radps * d.motorGearRatio;
    out.deliveredWheelTorque_Nm = req.wheelTorque_Nm;

    double tMotor = 0.0;
    double pElec = 0.0;
    if (req.wheelSpeed_radps >= d.regenMinWheelSpeed_radps && req.batteryMaxCharge_W > 0.0) {
        // Geri kazanımda tork: T_motor = T_tekerlek * eta / G  (negatif)
        tMotor = req.wheelTorque_Nm * d.motorGearEfficiency / d.motorGearRatio;
        // Motorun tork sınırı
        tMotor = std::max(tMotor, -motor_.maxTorque(out.motorSpeed_radps));
        pElec = motor_.electricalPower(tMotor, out.motorSpeed_radps);
        // Bataryanın şarj gücü sınırı
        if (pElec < -req.batteryMaxCharge_W) {
            tMotor = motor_.torqueForElectricalPower(-req.batteryMaxCharge_W, out.motorSpeed_radps);
            pElec = motor_.electricalPower(tMotor, out.motorSpeed_radps);
        }
        // Kayıplar geri kazanımdan büyükse (çok hafif fren) motoru hiç kullanma
        if (pElec >= 0.0) {
            tMotor = 0.0;
            pElec = 0.0;
        }
    }
    out.motorTorque_Nm = tMotor;
    out.motorElectricalPower_W = pElec;

    // Motorun tekerlekte karşıladığı fren torku: T_tekerlek = T_motor * G / eta
    const double regenWheelTorque = tMotor * d.motorGearRatio / d.motorGearEfficiency;
    // Kalanını sürtünmeli fren karşılar (ikisi de negatif)
    out.frictionBrakeTorque_Nm = std::min(0.0, req.wheelTorque_Nm - regenWheelTorque);
    return out;
}

bool Powertrain::solveElectric(const PowertrainRequest& req, PowertrainResult& out) const
{
    const DrivetrainParams& d = drivetrain_;
    const double wMotor = req.wheelSpeed_radps * d.motorGearRatio;
    // Tahrikte tork: T_motor = T_tekerlek / (G * eta)
    const double tMotor = req.wheelTorque_Nm / (d.motorGearRatio * d.motorGearEfficiency);
    if (!motor_.canDeliver(tMotor, wMotor)) {
        return false;
    }
    // Sıfır torkta invertör kapalı kabul edilir: kayıp yok
    const double pElec = tMotor > kTorqueEps ? motor_.electricalPower(tMotor, wMotor) : 0.0;
    if (pElec > req.batteryMaxDischarge_W) {
        return false;
    }
    out = PowertrainResult{};
    out.mode = Mode::Electric;
    out.motorSpeed_radps = wMotor;
    out.motorTorque_Nm = tMotor;
    out.motorElectricalPower_W = pElec;
    out.deliveredWheelTorque_Nm = req.wheelTorque_Nm;
    return true;
}

bool Powertrain::bestGear(double axleTorque_Nm, double wheelSpeed_radps,
                          PowertrainResult& out) const
{
    const DrivetrainParams& d = drivetrain_;
    const EngineParams& e = engine_.params();
    bool found = false;
    double bestFuel = 0.0;

    for (std::size_t i = 0; i < d.gearRatios.size(); ++i) {
        const double g = d.gearRatios[i] * d.finalDriveRatio;
        const double wGear = wheelSpeed_radps * g; // vites üzerinden motor devri
        if (wGear > e.maxSpeed_radps) {
            continue; // aşırı devir
        }
        // Rölantinin altında yalnızca 1. viteste kavrama kaydırılarak kalkılabilir.
        // Kayma sırasında motor rölantide döner, tork kavramadan aynen geçer;
        // aradaki güç farkı kavramada ısıya döner.
        const bool slip = wGear < e.idleSpeed_radps;
        if (slip && i != 0) {
            continue;
        }
        const double wEngine = slip ? e.idleSpeed_radps : wGear;
        // Tahrikte tork: T_motor = T_eksen / (G * eta)
        const double tEngine = axleTorque_Nm / (g * d.gearboxEfficiency);
        if (!engine_.canDeliver(tEngine, wEngine)) {
            continue;
        }
        const double fuel = engine_.fuelRate(tEngine, wEngine);
        if (!found || fuel < bestFuel) {
            found = true;
            bestFuel = fuel;
            out.gear = static_cast<int>(i) + 1;
            out.clutchSlip = slip;
            out.engineSpeed_radps = wEngine;
            out.engineTorque_Nm = tEngine;
            out.fuelRate_kgps = fuel;
        }
    }
    return found;
}

PowertrainResult Powertrain::solveEngine(const PowertrainRequest& req) const
{
    const DrivetrainParams& d = drivetrain_;
    const EngineParams& e = engine_.params();
    PowertrainResult out;
    out.mode = Mode::Engine;
    out.motorSpeed_radps = req.wheelSpeed_radps * d.motorGearRatio;
    const double wMotor = out.motorSpeed_radps;

    // --- Şarj isteği: elektrik motoru jeneratör olarak çalışır ---
    double tGen = 0.0;
    double pGen = 0.0;
    const double pCharge = std::min(req.chargePower_W, req.batteryMaxCharge_W);
    if (pCharge > 0.0 && wMotor > 0.0) {
        tGen = motor_.torqueForElectricalPower(-pCharge, wMotor);
        tGen = std::max(tGen, -motor_.maxTorque(wMotor));
        pGen = motor_.electricalPower(tGen, wMotor);
        if (pGen >= 0.0) { // bu devirde net şarj üretilemiyor
            tGen = 0.0;
            pGen = 0.0;
        }
    }
    // Yakıt kesme (DFCO): tekerleğe tahrik gerekmiyor ve şarj da yapılmıyorsa
    // enjektörler kapanır, yakıt sıfırdır. Motorun sürüklenme direnci ihmal edilir.
    if (req.wheelTorque_Nm <= kTorqueEps && tGen == 0.0) {
        return out;
    }

    // Jeneratörü döndürmek için eksende gereken ek tork: T_eksen = -T_gen * G / eta
    const double axleForCharge = -tGen * d.motorGearRatio / d.motorGearEfficiency;

    // --- En iyi vites: önce şarjlı, olmazsa şarjsız dene ---
    if (bestGear(req.wheelTorque_Nm + axleForCharge, req.wheelSpeed_radps, out)) {
        out.motorTorque_Nm = tGen;
        out.motorElectricalPower_W = pGen;
        out.deliveredWheelTorque_Nm = req.wheelTorque_Nm;
        return out;
    }
    if (tGen != 0.0 && bestGear(req.wheelTorque_Nm, req.wheelSpeed_radps, out)) {
        out.deliveredWheelTorque_Nm = req.wheelTorque_Nm;
        return out;
    }

    // --- Motor tek başına yetmiyor: tam yükte çalış, elektrik motoru takviye etsin ---
    double engineAxleTorque = 0.0;
    for (std::size_t i = 0; i < d.gearRatios.size(); ++i) {
        const double g = d.gearRatios[i] * d.finalDriveRatio;
        const double wGear = req.wheelSpeed_radps * g;
        if (wGear > e.maxSpeed_radps) {
            continue;
        }
        const bool slip = wGear < e.idleSpeed_radps;
        if (slip && i != 0) {
            continue;
        }
        const double wEngine = slip ? e.idleSpeed_radps : wGear;
        const double tMax = engine_.maxTorque(wEngine);
        const double axle = tMax * g * d.gearboxEfficiency;
        if (axle > engineAxleTorque) {
            engineAxleTorque = axle;
            out.gear = static_cast<int>(i) + 1;
            out.clutchSlip = slip;
            out.engineSpeed_radps = wEngine;
            out.engineTorque_Nm = tMax;
            out.fuelRate_kgps = engine_.fuelRate(tMax, wEngine);
        }
    }

    const double remaining = req.wheelTorque_Nm - engineAxleTorque;
    double tAssist = std::min(remaining / (d.motorGearRatio * d.motorGearEfficiency),
                              motor_.maxTorque(wMotor));
    double pAssist = tAssist > kTorqueEps ? motor_.electricalPower(tAssist, wMotor) : 0.0;
    if (pAssist > req.batteryMaxDischarge_W) {
        tAssist = req.batteryMaxDischarge_W > 0.0
                ? motor_.torqueForElectricalPower(req.batteryMaxDischarge_W, wMotor)
                : 0.0;
        tAssist = std::max(tAssist, 0.0);
        pAssist = tAssist > kTorqueEps ? motor_.electricalPower(tAssist, wMotor) : 0.0;
    }
    out.motorTorque_Nm = tAssist;
    out.motorElectricalPower_W = pAssist;

    const double assistAxle = tAssist * d.motorGearRatio * d.motorGearEfficiency;
    out.deliveredWheelTorque_Nm = engineAxleTorque + assistAxle;
    out.unmetWheelTorque_Nm = std::max(0.0, req.wheelTorque_Nm - out.deliveredWheelTorque_Nm);
    return out;
}

} // namespace hea
