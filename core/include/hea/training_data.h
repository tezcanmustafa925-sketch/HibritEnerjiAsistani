#pragma once

#include <string>
#include <vector>

#include "hea/drive_cycle.h"
#include "hea/simulation.h"
#include "hea/strategy.h"

namespace hea {

// Başka bir stratejiyi sarar: kararı ona bırakır, her adımda gelen girdiyi kaydeder.
// Simülasyonu değiştirmeden stratejinin gördüğü durumları toplamak için kullanılır.
class RecordingStrategy : public Strategy {
public:
    explicit RecordingStrategy(Strategy& inner) : inner_(inner) {}

    const char* name() const override { return inner_.name(); }
    void reset() override;
    Decision decide(const StrategyInput& in) override;

    const std::vector<StrategyInput>& inputs() const { return inputs_; }

private:
    Strategy& inner_;
    std::vector<StrategyInput> inputs_;
};

// Tüketim modeli için bir eğitim örneği: bir tahrik anında stratejinin gördüğü
// durum ve aynı an için İKİ modun da maliyeti ("ne olurdu" hesabı).
//
// Gerçek simülasyonda her an yalnızca bir mod uygulanır; model ise iki seçeneği
// karşılaştıracağı için ikisinin de sonucunu bilmelidir. Bu yüzden her örnekte
// güç aktarma organı aynı talep için Electric ve Engine modlarıyla ayrıca çözülür.
// Bu karşı-olgusal çözümler simülasyonun gidişatını etkilemez.
struct TrainingSample {
    std::string cycle;
    double dt_s = 1.0;                 // Adım süresi [s] (toplam tüketim = sum(debi * dt))
    StrategyInput in;                  // Özellikler (stratejinin gördüğü her şey)

    // EV seçilseydi:
    bool evFeasible = false;           // Motor ve batarya talebi karşılayabilir mi?
    double evBatteryPower_W = 0.0;     // Motorun bataryadan çektiği güç (yardımcı yük hariç)

    // ICE seçilseydi (şarjsız):
    bool iceFeasible = false;          // Takviyesiz ve eksiksiz karşılanabilir mi?
    double iceFuelRate_kgps = 0.0;     // Yakıt debisi
    int iceGear = 0;                   // Seçilen vites

    // ICE + şarj seçilseydi (sabit şarj isteği, bkz. kTrainingChargeRequest_W):
    double iceChargePower_W = 0.0;     // Gerçekte bataryaya giden elektrik gücü (>= 0)
    double iceChargeFuelRate_kgps = 0.0; // Şarjlı durumdaki yakıt debisi
};

// Karşı-olgusal şarj örneği için istenen şarj gücü [W]
constexpr double kTrainingChargeRequest_W = 4000.0;

// Çevrimi `driver` stratejisiyle çalıştırır ve tekerlek gücünün pozitif olduğu
// (yani EV/ICE kararının anlamlı olduğu) her adım için bir örnek üretir.
// Fren, iniş ve duruş anları dahil edilmez: o anlarda karar güç aktarma organınındır.
// Karşı-olgusal çözümler SICAK motorla yapılır: tüketim modeli sıcak motoru temsil
// eder. Gerçek simülasyonda motor soğuk başlayabilir; o fark ısınma modelinden gelir.
std::vector<TrainingSample> buildTrainingSamples(const SimulationConfig& config,
                                                 const DriveCycle& cycle, Strategy& driver);

} // namespace hea
