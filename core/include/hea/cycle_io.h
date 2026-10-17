#pragma once

#include <string>
#include <vector>

#include "hea/drive_cycle.h"

namespace hea {

// ----------------------------------------------------------------------------
// Standart (yönetmelik) çevrimleri
//
// NEDC (New European Driving Cycle, UN ECE R83/R101): 4 x ECE-15 şehir içi
// (her biri 195 s) + 1 x EUDC şehir dışı (400 s) = 1180 s, ~11.0 km.
// Hız profili yönetmelikteki parça parça doğrusal tanımdan üretilir; vites
// değiştirme sırasındaki kısa sabit hız bölümleri doğrusal ivmelenmeye katılmıştır.
// Düz yol, 75 kg yük (sürücü), 25 °C (laboratuvar koşulu).
// ----------------------------------------------------------------------------
DriveCycle makeNedcCycle();

// Hazır gelen standart çevrimler (şimdilik yalnızca NEDC)
std::vector<DriveCycle> standardCycles();

// ----------------------------------------------------------------------------
// CSV dosyasından çevrim okuma
//
// Biçim (virgülle ayrılmış, ilk satır başlık, '#' ile başlayan satırlar yorum):
//   time_s,speed_kmh[,grade_percent][,payload_kg][,ambient_C]
// speed_kmh yerine speed_mps de verilebilir. Zamanlar kesin artan olmalıdır;
// örnekler düzgün aralıklı olmak zorunda değildir: dt_s aralığına doğrusal ara
// değerle yeniden örneklenir. İlk zaman 0 değilse 0'a kaydırılır.
// Eksik isteğe bağlı sütunlar: eğim 0, yük 75 kg, ortam 25 °C.
// Çevrim adı dosya adıdır (uzantısız).
//
// Hata olursa (dosya açılamadı, sütun eksik, sayı okunamadı, zaman artmıyor,
// negatif hız) satır numarasıyla birlikte std::runtime_error fırlatır.
// ----------------------------------------------------------------------------
DriveCycle loadCycleCsv(const std::string& path, double dt_s = 1.0);

} // namespace hea
