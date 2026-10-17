#pragma once

#include "hea/defaults.h"
#include "hea/strategy.h"

namespace hea {

// Kural tabanlı stratejinin eşikleri. Değerler benim seçimimdir; tam hibritlerin
// tipik davranışını (düşük hız ve düşük güçte EV, aksi halde ICE, SOC'yi bir
// hedef etrafında tutma) taklit edecek şekilde ayarlandı.
struct RuleBasedParams {
    double socTarget = defaults::kSocTarget; // Korunmaya çalışılan SOC
    double socLow = 0.45;          // Bunun altında EV yasak, motor çalışır ve şarj eder
    double socHigh = 0.70;         // Bunun üstünde EV güç eşiği yükseltilir (bataryayı kullan)
    double evMaxSpeed_mps = 13.89; // EV için azami hız (50 km/h)
    double speedHysteresis_mps = 1.39; // Motoru kapatmak için hız bu kadar düşmeli (5 km/h)
    double evMaxPower_W = 12000.0;     // EV için azami tekerlek gücü (normal SOC)
    double evMaxPowerHighSoc_W = 20000.0; // SOC yüksekken EV güç eşiği
    double powerHysteresis = 0.8;      // Motoru kapatmak için güç eşiğin bu oranının altına inmeli
    double minEngineOnTime_s = defaults::kMinEngineOnTime_s; // Motor açıldıktan sonra en az bu kadar çalışır
    // ICE modunda şarj: P_sarj = kazanc * (SOC_hedef - SOC), [0, azami] aralığında
    double chargeGain_W = 40000.0;     // [W / birim SOC]; SOC 0.50'de 4 kW
    double maxChargePower_W = 8000.0;

    bool isValid() const;
};

// Kural tabanlı enerji yönetimi.
//
// Motor ne zaman AÇILIR (herhangi biri doğruysa):
//   1. SOC < socLow                          (batarya korunmalı)
//   2. hız > evMaxSpeed                      (EV verimsiz / batarya gücü yetmez)
//   3. tekerlek gücü > EV güç eşiği          (eşik SOC > socHigh iken daha yüksek)
// Motor ne zaman KAPANIR (hepsi doğruysa):
//   - en az minEngineOnTime süredir çalışıyor (sık aç-kapa önlenir)
//   - SOC > socLow + 0.05, hız < evMaxSpeed - histerezis, güç < eşik * powerHysteresis
// Fren / serbest yuvarlanma (tekerlek gücü <= 0): enerjiyi güç aktarma organı
// rejeneratif frenle geri kazanır. Motor kapanır; ancak asgari çalışma süresi
// dolmadıysa ve araç hareket ediyorsa yakıt kesmede açık kalır.
// Motor çalışırken SOC hedefin altındaysa ek yükle şarj edilir (yük noktası
// kaydırma): düşük yükte çalışan motor daha verimli bölgeye taşınır ve
// bataryaya sonra EV'de kullanılacak enerji depolanır.
class RuleBasedStrategy : public Strategy {
public:
    // Parametreler geçersizse std::invalid_argument fırlatır.
    explicit RuleBasedStrategy(const RuleBasedParams& params = RuleBasedParams{});

    const char* name() const override { return "kural-tabanli"; }
    void reset() override;
    Decision decide(const StrategyInput& in) override;

    bool engineOn() const { return engineOn_; }

private:
    double evPowerLimit(double soc) const;

    RuleBasedParams p_;
    bool engineOn_ = false;
    double engineOnTime_s_ = 0.0;
    double lastTime_s_ = 0.0;
    bool hasLastTime_ = false;
};

} // namespace hea
