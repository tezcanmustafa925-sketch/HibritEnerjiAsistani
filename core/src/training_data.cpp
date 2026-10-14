#include "hea/training_data.h"

#include "hea/powertrain.h"

namespace hea {

void RecordingStrategy::reset()
{
    inputs_.clear();
    inner_.reset();
}

Decision RecordingStrategy::decide(const StrategyInput& in)
{
    inputs_.push_back(in);
    return inner_.decide(in);
}

std::vector<TrainingSample> buildTrainingSamples(const SimulationConfig& config,
                                                 const DriveCycle& cycle, Strategy& driver)
{
    // 1) Gerçek simülasyon: durumlar (SOC gidişatı dahil) sürücü stratejiye göre oluşur
    RecordingStrategy recorder(driver);
    runCycle(config, cycle, recorder);

    // 2) Her tahrik anında iki modu ayrı ayrı çöz
    const Powertrain pt(config.motor, config.engine, config.drivetrain);
    std::vector<TrainingSample> samples;
    for (const StrategyInput& in : recorder.inputs()) {
        if (in.wheelPower_W <= 0.0) {
            continue; // fren / iniş / duruş: strateji kararı etkisiz
        }
        TrainingSample s;
        s.cycle = cycle.name;
        s.in = in;

        PowertrainRequest req;
        req.wheelTorque_Nm = in.wheelTorque_Nm;
        req.wheelSpeed_radps = in.wheelSpeed_radps;
        req.batteryMaxDischarge_W = in.batteryMaxDischarge_W;
        req.batteryMaxCharge_W = in.batteryMaxCharge_W;

        // EV seçilseydi
        req.requestedMode = Mode::Electric;
        const PowertrainResult ev = pt.resolve(req);
        s.evFeasible = ev.mode == Mode::Electric;
        s.evBatteryPower_W = s.evFeasible ? ev.motorElectricalPower_W : 0.0;

        // ICE seçilseydi (şarjsız)
        req.requestedMode = Mode::Engine;
        const PowertrainResult ice = pt.resolve(req);
        s.iceFeasible = ice.unmetWheelTorque_Nm <= 0.0 && ice.motorTorque_Nm <= 0.0;
        s.iceFuelRate_kgps = ice.fuelRate_kgps;
        s.iceGear = ice.gear;

        // ICE + sabit şarj isteği
        req.chargePower_W = kTrainingChargeRequest_W;
        const PowertrainResult iceCharge = pt.resolve(req);
        s.iceChargePower_W = iceCharge.motorElectricalPower_W < 0.0
                           ? -iceCharge.motorElectricalPower_W : 0.0;
        s.iceChargeFuelRate_kgps = iceCharge.fuelRate_kgps;

        samples.push_back(s);
    }
    return samples;
}

} // namespace hea
