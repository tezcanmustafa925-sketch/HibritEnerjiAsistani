#pragma once

#include "hea/consumption_model.h"
#include "hea/defaults.h"
#include "hea/strategy.h"

namespace hea {

// Model tabanlı stratejinin parametreleri (benim seçimlerim).
// Hiçbiri test çevrimlerine bakılarak ayarlanmadı.
struct ModelBasedParams {
    double socTarget = defaults::kSocTarget;
    // Eşdeğerlik katsayısı s0 [-]: bataryadaki 1 J elektriğin kaç J yakıt
    // değerinde olduğu. Tüketim modelinin ortalama şarj maliyetinden türetildi:
    // ~6.5e-8 kg/J * 43e6 J/kg = ~2.8 (8c: 1 kWh elektrik ~234 g yakıt).
    // Böylece SOC hedefteyken ICE ile şarj etmek ne kârlı ne zararlı olur.
    double equivalenceFactor = defaults::kEquivalenceFactor;
    // SOC geri beslemesi: s = s0 * (1 + kazanc * (SOC_hedef - SOC)).
    // SOC 0.50'de s = 1.5*s0 (elektrik pahalı, ICE ve şarj tercih edilir),
    // 0.70'te s = 0.5*s0 (elektrik ucuz, EV tercih edilir).
    double socFeedbackGain = 5.0;
    double socMinForEv = 0.35;      // Bunun altında EV yasak (batarya koruması)
    double chargePower_W = 8000.0;  // Şarj kârlıysa istenen güç (kural tabanlıyla aynı üst sınır)
    // Motor çalıştırma maliyetinin dağıtılması: motor kapalıyken ICE maliyetine
    // (çalıştırma yakıtı / asgari çalışma süresi) eklenir. Simülasyondaki ceza ile aynı
    // kaynaktan gelir (defaults.h); yapılandırma dosyası ikisini birlikte değiştirir.
    double engineStartFuel_kg = defaults::kEngineStartFuel_kg;
    double minEngineOnTime_s = defaults::kMinEngineOnTime_s; // Kural tabanlıyla aynı

    bool isValid() const;
};

// Bir karardaki eşdeğer maliyetler [kg/s] (test ve çıktı için)
struct EcmsCosts {
    double s = 0.0;               // Kullanılan eşdeğerlik katsayısı
    double ev = 0.0;              // EV: s * P_batarya / LHV (uygun değilse çok büyük)
    double ice = 0.0;             // ICE (şarjsız) + gerekiyorsa çalıştırma payı
    double iceCharge = 0.0;       // ICE + şarj
    bool evAllowed = false;
};

// Model tabanlı strateji: ECMS (Equivalent Consumption Minimization Strategy).
//
// Her tahrik anında üç seçeneğin eşdeğer yakıt maliyeti [kg/s] hesaplanır ve en
// küçüğü seçilir. Tüm tüketim değerleri Python'da eğitilen modelden gelir:
//
//   J_EV       = s(SOC) * P_bat,EV / LHV
//   J_ICE      = m_dot_ICE                          (+ çalıştırma payı, motor kapalıysa)
//   J_ICE+şarj = m_dot_ICE + c * P_ş - s(SOC) * P_ş / LHV
//
//   P_bat,EV : modelin tahmini EV batarya gücü          [W]
//   m_dot_ICE: modelin tahmini ICE yakıt debisi         [kg/s]
//   c        : modelin tahmini şarj marjinal maliyeti   [kg/J]
//   P_ş      : şarj gücü                                [W]
//   s(SOC)   = s0 * (1 + k * (SOC_hedef - SOC))
//
// Şarj terimi: ICE çalışırken P_ş kadar şarj c*P_ş ek yakıta mal olur, ama
// bataryaya s*P_ş/LHV değerinde enerji koyar. Değer maliyetten büyükse
// (s/LHV > c, yani SOC hedefin altındaysa) şarj kârlıdır.
class ModelBasedStrategy : public Strategy {
public:
    // Parametreler veya tüketim modeli geçersizse std::invalid_argument fırlatır.
    explicit ModelBasedStrategy(const ModelBasedParams& params = ModelBasedParams{},
                                const ConsumptionModel& model =
                                    ConsumptionModel::fromGeneratedCoefficients());

    const char* name() const override { return "model-tabanli"; }
    void reset() override;
    Decision decide(const StrategyInput& in) override;

    bool engineOn() const { return engineOn_; }
    const EcmsCosts& lastCosts() const { return last_; }

    // s(SOC) = s0 * (1 + k * (SOC_hedef - SOC)), 0'dan küçük olamaz
    double equivalenceFactor(double soc) const;

private:
    ModelBasedParams p_;
    ConsumptionModel model_;
    EcmsCosts last_;
    bool engineOn_ = false;
    double engineOnTime_s_ = 0.0;
    double lastTime_s_ = 0.0;
    bool hasLastTime_ = false;
};

} // namespace hea
