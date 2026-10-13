#pragma once

namespace hea {

// Elektrik motoru (motor-jeneratör + invertör) parametreleri.
// Varsayılanlar kompakt bir tam hibritin çekiş motoruna yakın (~53 kW, 163 Nm).
struct MotorParams {
    double maxPower_W = 53000.0;    // Sürekli üst güç sınırı [W]
    double maxTorque_Nm = 163.0;    // Tepe tork [Nm]
    double maxSpeed_radps = 1466.0; // Azami devir [rad/s] (~14000 dev/dak)

    // Kayıp modeli katsayıları (bkz. ElectricMotor::loss):
    double copperLossCoeff = 0.15;   // k_c [W/Nm^2]: sargı (bakır) kaybı, I^2*R ~ T^2
    double ironLossCoeff = 1.0;      // k_i [W/(rad/s)]: demir (histerezis) kaybı ~ omega
    double windageLossCoeff = 1e-7;  // k_w [W/(rad/s)^3]: rüzgâr/sürtünme kaybı ~ omega^3
    double constantLoss_W = 200.0;   // C [W]: invertör ve yardımcı sabit kayıplar

    bool isValid() const;
};

// Elektrik motoru modeli. Verim haritası tablo yerine fiziksel kayıp
// bileşenlerinden analitik olarak üretilir:
//
//   P_kayip(T, omega) = k_c*T^2 + k_i*omega + k_w*omega^3 + C
//   P_elektrik        = T*omega + P_kayip
//
// Motor modunda (T*omega > 0) bataryadan mekanik güçten fazlası çekilir;
// jeneratör modunda (T*omega < 0) bataryaya mekanik güçten azı gider.
// Böylece tek formül iki yönü de kapsar. Verim düşük torkta (sabit kayıplar
// baskın) ve tepe torkta (bakır kaybı baskın) düşer; orta yükte en yüksektir.
//
// Varsayımlar: kayıplar sıcaklıktan ve batarya geriliminden bağımsız;
// motor dört bölgede de (ileri/geri, motor/jeneratör) simetrik.
class ElectricMotor {
public:
    explicit ElectricMotor(const MotorParams& params);

    const MotorParams& params() const { return params_; }

    // Verilen devirde kullanılabilir en büyük tork [Nm]:
    //   T_max(omega) = min(T_tepe, P_max / omega)
    // Düşük devirde tork sınırlı (akım sınırı), yüksek devirde güç sınırlı.
    double maxTorque(double speed_radps) const;

    // Toplam kayıp gücü [W]
    double loss(double torque_Nm, double speed_radps) const;

    // Bataryadan çekilen (pozitif) veya bataryaya verilen (negatif) elektrik gücü [W]
    double electricalPower(double torque_Nm, double speed_radps) const;

    // electricalPower'ın tersi: istenen elektrik gücünü veren torku bulur.
    //   k_c*T^2 + omega*T + (k_i*omega + k_w*omega^3 + C - P_e) = 0
    // Küçük |T| kökü seçilir. Kök yoksa (istenen jeneratör gücü bu devirde
    // üretilemiyorsa) en çok güç üretilen tork T = -omega / (2*k_c) döndürülür.
    double torqueForElectricalPower(double power_W, double speed_radps) const;

    // Bu tork/devir çifti motor sınırları içinde mi?
    bool canDeliver(double torque_Nm, double speed_radps) const;

private:
    MotorParams params_;
};

} // namespace hea
