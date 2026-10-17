#pragma once

// Birden çok yapıda kullanılan varsayılan değerlerin TEK kaynağı.
// Aynı sayı iki yerde ayrı ayrı yazılırsa biri değiştiğinde diğeri eskide kalır;
// bu yüzden simülasyon, stratejiler ve karşılaştırma bu sabitleri kullanır.

namespace hea {
namespace defaults {

// Motorun her çalıştırılmasında harcanan ek yakıt [kg] (bkz. SimulationConfig).
// Model tabanlı strateji aynı değeri maliyet hesabında kullanır.
constexpr double kEngineStartFuel_kg = 0.2e-3;

// Motor açıldıktan sonra en az çalışma süresi [s] (iki strateji için aynı: adil karşılaştırma)
constexpr double kMinEngineOnTime_s = 10.0;

// Eşdeğerlik katsayısı s0 [-]: 1 J batarya enerjisi ~ 2.8 J yakıt
// (tüketim modelinin ortalama şarj maliyeti, 1 kWh ~ 234 g benzin).
// ECMS'in s0'ı ve karşılaştırmadaki eşdeğer yakıt kontrolü bunu kullanır.
constexpr double kEquivalenceFactor = 2.8;

// Korunmaya çalışılan SOC hedefi (iki strateji için aynı)
constexpr double kSocTarget = 0.60;

} // namespace defaults
} // namespace hea
