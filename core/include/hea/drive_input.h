#pragma once

namespace hea {

// Sürüş çevriminin bir zaman örneği: simülasyona dışarıdan verilen BAĞIMSIZ girdiler.
//
// Neden yalnızca bunlar? Kullanıcının istediği diğer büyüklükler bu girdilerden
// fiziksel olarak türetilir, bağımsız üretilirlerse model tutarsız olur:
//   - gaz pedalı / fren   -> sürücü modeli, istenen hızı takip etmek için hesaplar
//   - motor devri         -> araç hızı, vites/aktarma oranı ve tekerlek yarıçapından
//   - batarya V / I / T   -> batarya modeli, çekilen güçten hesaplar
// Bu türetilmiş değerler StepResult içinde raporlanır.
struct DriveInput {
    // Çevrim başlangıcından itibaren zaman [s]
    double time_s = 0.0;
    // Hedef araç hızı [m/s]
    double targetSpeed_mps = 0.0;
    // Yol eğim açısı theta [rad]; pozitif = yokuş yukarı
    double grade_rad = 0.0;
    // Araca binen ek yük: sürücü + yolcu + bagaj [kg]
    double payload_kg = 75.0;
};

} // namespace hea
