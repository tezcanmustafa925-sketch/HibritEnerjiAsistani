#pragma once

// Birim dönüşümleri ve fiziksel sabitler.
// Kural: core içindeki tüm hesaplar SI birimleriyle yapılır (m, s, kg, N, W, Nm, rad/s).
// Buradaki dönüşümler yalnızca girişte (ör. çevrim tanımı km/h ise) ve
// çıktıda (ekranda km/h, rpm göstermek için) kullanılmalıdır.

namespace hea {

namespace constants {
constexpr double kPi = 3.14159265358979323846;
// Yerçekimi ivmesi [m/s^2]
constexpr double kGravity = 9.81;
// Deniz seviyesi, 15 °C'de hava yoğunluğu [kg/m^3]
constexpr double kAirDensity = 1.225;
// Benzin yoğunluğu [kg/L] — yakıt kütlesini litreye çevirmek için
constexpr double kGasolineDensity = 0.745;
// Benzinin alt ısıl değeri [J/kg] — yakıt kütlesinin enerji karşılığı
constexpr double kGasolineLhv = 43.0e6;
} // namespace constants

namespace units {

// Hız: 1 km/h = 1000 m / 3600 s
constexpr double kmhToMps(double kmh) { return kmh / 3.6; }
constexpr double mpsToKmh(double mps) { return mps * 3.6; }

// Açısal hız: 1 dev/dak = 2*pi rad / 60 s
constexpr double rpmToRadps(double rpm) { return rpm * 2.0 * constants::kPi / 60.0; }
constexpr double radpsToRpm(double radps) { return radps * 60.0 / (2.0 * constants::kPi); }

// Enerji: 1 kWh = 3.6e6 J
constexpr double jouleToKwh(double j) { return j / 3.6e6; }
constexpr double kwhToJoule(double kwh) { return kwh * 3.6e6; }

// Eğim: yüzde eğim (ör. %5) -> açı [rad]. tan(theta) = yükselme / yatay mesafe
// std::atan C++17'de constexpr olmadığı için gövdesi units.cpp içinde.
double gradePercentToRad(double percent);

} // namespace units
} // namespace hea
