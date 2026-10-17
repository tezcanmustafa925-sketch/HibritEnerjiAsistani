#pragma once

#include "hea/mode.h"

namespace hea {

// Stratejilerin "araç hareket ediyor" saydığı en düşük hız [m/s]
constexpr double kMovingSpeed_mps = 0.1;

// Stratejinin her adımda gördüğü bilgiler (SI). Simülasyon bunları araç modeli
// ve bataryadan hesaplayıp stratejiye verir; strateji araç durumunu değiştiremez.
struct StrategyInput {
    double time_s = 0.0;
    double speed_mps = 0.0;           // Adımın ortalama hızı
    double acceleration_mps2 = 0.0;   // İstenen ivme
    double grade_rad = 0.0;
    double payload_kg = 0.0;
    double wheelTorque_Nm = 0.0;      // Tekerlekte istenen tork (negatif = fren)
    double wheelSpeed_radps = 0.0;
    double wheelPower_W = 0.0;        // Tekerlekte istenen güç (negatif = fren)
    double soc = 0.0;
    double batteryTemp_C = 0.0;
    double engineTemp_C = 0.0;          // İçten yanmalı motor sıcaklığı [°C]
    double batteryMaxDischarge_W = 0.0; // Bataryanın bu adımdaki sınırları
    double batteryMaxCharge_W = 0.0;    // (yardımcı yük düşülmüş)
};

// Stratejinin kararı.
struct Decision {
    // Electric veya Engine. Fren / iniş anında Engine, "motor açık kalsın (yakıt
    // kesmede dönsün)" demektir; enerjiyi yine rejeneratif fren toplar.
    Mode mode = Mode::Electric;
    // Engine modunda bataryaya ek şarj için istenen elektrik gücü [W] (>= 0)
    double chargePower_W = 0.0;
};

// Enerji yönetim stratejisi arayüzü. Kural tabanlı ve model tabanlı
// stratejiler bundan türetilir. Fren/durma anlarında da çağrılır ama kararı
// o anlarda güç aktarma organı (rejeneratif fren / durma) belirler.
class Strategy {
public:
    virtual ~Strategy() = default;
    virtual const char* name() const = 0;
    // Yeni çevrim başlarken iç durumu sıfırla
    virtual void reset() {}
    virtual Decision decide(const StrategyInput& in) = 0;
};

// Her zaman aynı modu öneren strateji. Kıyas tabanı olarak kullanılır:
//   - Engine  : "sadece ICE" — rejeneratif fren dışında elektrik kullanmaz
//   - Electric: "EV öncelikli" — batarya/motor yetmeyince güç aktarma
//               organı kendiliğinden ICE'ye geçer, şarj etmez
class FixedModeStrategy : public Strategy {
public:
    explicit FixedModeStrategy(Mode mode) : mode_(mode) {}
    const char* name() const override;
    Decision decide(const StrategyInput& in) override;

private:
    Mode mode_;
};

} // namespace hea
