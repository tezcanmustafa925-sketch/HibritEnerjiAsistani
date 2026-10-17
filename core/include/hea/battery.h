#pragma once

namespace hea {

// Yüksek gerilim batarya paketinin parametreleri. Varsayılanlar, kompakt bir tam
// hibritteki küçük Li-iyon pakete yakın seçildi: 56 seri hücre (~207 V nominal),
// 6.5 Ah kapasite (~1.35 kWh).
struct BatteryParams {
    // Seri bağlı hücre sayısı [-]; paket OCV'si = hücre OCV'si * bu sayı
    int cellsInSeries = 56;
    // Nominal kapasite [Ah]
    double capacity_Ah = 6.5;
    // 25 °C'de paket iç direnci [Ohm]
    double internalResistance25_Ohm = 0.30;
    // İç direncin sıcaklık katsayısı [1/K]: R(T) = R25 * exp(k * (25 - T)).
    // Soğukta iyon hareketi yavaşladığı için direnç üstel olarak artar.
    double resistanceTempCoeff_perK = 0.03;
    // Polarizasyon (RC) dalı: R0'a seri bir R1 || C1. Akım değişince gerilim
    // anında değil, tau = R1*C1 zaman sabitiyle oturur (difüzyon / çift katman etkisi).
    // R1 de R0 ile aynı sıcaklık katsayısıyla değişir. R1 = 0 ise dal yoktur.
    double polarizationResistance_Ohm = 0.05;
    double polarizationTimeConstant_s = 20.0;
    // Bataryadan çekilebilecek / bataryaya verilebilecek en büyük güç [W] (pozitif büyüklük)
    double maxDischargePower_W = 27000.0;
    double maxChargePower_W = 25000.0;
    // Akım sınırı [A] (her iki yönde)
    double maxCurrent_A = 150.0;
    // İzin verilen SOC penceresi [0..1]. Hibritlerde ömür için dar tutulur.
    double socMin = 0.30;
    double socMax = 0.80;
    // Toplu (tek düğümlü) ısıl model: ısıl kapasite C [J/K] ve
    // ortama ısı aktarım katsayısı hA [W/K] (fanlı hava soğutma)
    double thermalCapacity_JperK = 36000.0;
    double heatTransfer_WperK = 25.0;

    bool isValid() const;
};

// Bir batarya adımının sonucu. Akım işareti: pozitif = deşarj, negatif = şarj.
struct BatteryStep {
    double requestedPower_W = 0.0; // İstenen uç gücü [W]
    double power_W = 0.0;          // Sınırlar uygulandıktan sonra gerçekleşen uç gücü [W]
    double current_A = 0.0;        // Akım [A]
    double voltage_V = 0.0;        // Uç gerilimi [V]
    double ocv_V = 0.0;            // Açık devre gerilimi [V]
    double heat_W = 0.0;           // İç kayıp I*(OCV - V) = R0*I^2 + V1*I [W]
    double polarization_V = 0.0;   // Adım sonundaki RC dalı gerilimi V1 [V]
    double soc = 0.0;              // Adım sonundaki SOC [0..1]
    double temp_C = 0.0;           // Adım sonundaki sıcaklık [°C]
    bool limited = false;          // İstenen güç sınırlar yüzünden kısıldı mı?
};

// Eşdeğer devre batarya modeli (Thevenin, 1 RC): ideal gerilim kaynağı (OCV),
// seri iç direnç R0 ve bir polarizasyon dalı (R1 || C1).
//
//   Uç gerilimi:  V = OCV(SOC) - I * R0(T) - V1
//   RC dalı:      dV1/dt = I / C1 - V1 / (R1 * C1)
//   Uç gücü:      P = V * I = (OCV - V1) * I - R0 * I^2
//
// V1 adım başındaki değerinde sabit tutulursa P verildiğinde I, ikinci derece
// denklemin fiziksel (küçük) kökünden bulunur (E = OCV - V1):
//   R0*I^2 - E*I + P = 0  ->  I = (E - sqrt(E^2 - 4*R0*P)) / (2*R0)
// Aynı formül şarjda (P < 0) da geçerlidir ve negatif akım verir. Sonra V1 tam
// (üstel) çözümle güncellenir: V1' = V1*exp(-dt/tau) + R1*I*(1 - exp(-dt/tau)).
//
// Varsayımlar:
//   - Tek RC dalı; daha yavaş difüzyon etkileri yok.
//   - Coulomb verimi %100: kayıpların tamamı iç dirençlerdeki ısıdır.
//   - OCV yalnızca SOC'ye bağlı; sıcaklık ve yaşlanma OCV'yi etkilemez.
//   - Hücreler dengeli; paket tek büyük hücre gibi davranır.
//   - Hücre gerilim sınırları yerine güç, akım ve SOC sınırları kullanılır.
class Battery {
public:
    Battery(const BatteryParams& params, double initialSoc, double initialTemp_C);

    const BatteryParams& params() const { return params_; }
    double soc() const { return soc_; }
    double temperature_C() const { return temp_C_; }

    // Mevcut SOC'deki açık devre gerilimi [V]
    double openCircuitVoltage() const;
    // Mevcut sıcaklıktaki seri (ohmik) iç direnç R0 [Ohm]
    double internalResistance() const;
    // Polarizasyon dalının gerilimi V1 [V] (pozitif = deşarj yönünde)
    double polarizationVoltage() const { return v1_; }

    // Şu anki durumda verilebilecek en büyük deşarj / şarj gücü [W] (pozitif büyüklük).
    // Güç, akım ve SOC sınırlarının en kısıtlayıcısıdır.
    double availableDischargePower() const;
    double availableChargePower() const;

    // dt süresince uçlardan requestedPower_W çek (pozitif) veya ver (negatif).
    // Güç sınırlara göre kısılır, sonra SOC ve sıcaklık güncellenir.
    BatteryStep step(double requestedPower_W, double dt_s, double ambientTemp_C);

private:
    BatteryParams params_;
    double soc_;
    double temp_C_;
    double v1_ = 0.0;
};

// Tek hücre açık devre gerilimi [V], SOC'nin fonksiyonu (tablodan doğrusal ara değer).
double cellOpenCircuitVoltage(double soc);

// SOC socFrom'dan socTo'ya inerken paketten çıkan depolanan (kimyasal) enerji [J]:
//   E = Q * N_seri * integral_{socTo}^{socFrom} OCV_hucre(s) ds,   Q = kapasite [C]
// Pozitif = boşaldı (socTo < socFrom). Uç enerjisinden farklı olarak iç dirençteki
// I^2*R kayıplarını içermez; yalnızca SOC değişimine bağlıdır ve SOC aynı kaldığında
// tam olarak sıfırdır.
double storedEnergyReleased_J(const BatteryParams& params, double socFrom, double socTo);

} // namespace hea
