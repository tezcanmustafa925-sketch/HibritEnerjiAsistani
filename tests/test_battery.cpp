// Batarya modeli testleri. Beklenen değerler varsayılan parametrelerle elle hesaplandı:
//   56 seri hücre, 6.5 Ah -> Q = 6.5 * 3600 = 23400 C, R25 = 0.30 Ohm
//   SOC 0.6'da hücre OCV = 3.78 V -> paket OCV = 211.68 V

#include <cmath>
#include <initializer_list>

#include "hea/battery.h"
#include "test_util.h"

using namespace hea;

static const BatteryParams kParams{};

static void testParams()
{
    HEA_CHECK(kParams.isValid());
    BatteryParams bad = kParams;
    bad.socMin = 0.9; // socMin > socMax
    HEA_CHECK(!bad.isValid());
}

static void testOcvCurve()
{
    HEA_CHECK_NEAR(cellOpenCircuitVoltage(0.0), 3.00, 1e-12);
    HEA_CHECK_NEAR(cellOpenCircuitVoltage(1.0), 4.18, 1e-12);
    HEA_CHECK_NEAR(cellOpenCircuitVoltage(0.5), 3.72, 1e-12);
    // Tablo noktaları arasında doğrusal: (3.72 + 3.78) / 2
    HEA_CHECK_NEAR(cellOpenCircuitVoltage(0.55), 3.75, 1e-12);
    // Aralık dışı değerler kırpılır
    HEA_CHECK_NEAR(cellOpenCircuitVoltage(1.5), 4.18, 1e-12);
    HEA_CHECK_NEAR(cellOpenCircuitVoltage(-0.2), 3.00, 1e-12);
    // OCV, SOC ile monoton artmalı
    for (int i = 0; i < 100; ++i) {
        HEA_CHECK(cellOpenCircuitVoltage((i + 1) / 100.0) > cellOpenCircuitVoltage(i / 100.0));
    }

    const Battery b(kParams, 0.6, 25.0);
    HEA_CHECK_NEAR(b.openCircuitVoltage(), 211.68, 1e-9);
}

static void testResistanceVsTemperature()
{
    // R(T) = 0.30 * exp(0.03 * (25 - T))
    HEA_CHECK_NEAR(Battery(kParams, 0.6, 25.0).internalResistance(), 0.30, 1e-12);
    HEA_CHECK_NEAR(Battery(kParams, 0.6, 0.0).internalResistance(), 0.635100, 1e-6);
    HEA_CHECK_NEAR(Battery(kParams, 0.6, 45.0).internalResistance(), 0.164643, 1e-6);
}

static void testZeroPower()
{
    Battery b(kParams, 0.6, 25.0);
    const BatteryStep s = b.step(0.0, 1.0, 25.0);
    HEA_CHECK_NEAR(s.current_A, 0.0, 1e-12);
    HEA_CHECK_NEAR(s.voltage_V, 211.68, 1e-9); // akım yoksa uç gerilimi = OCV
    HEA_CHECK_NEAR(s.soc, 0.6, 1e-12);
    HEA_CHECK_NEAR(s.temp_C, 25.0, 1e-12);
    HEA_CHECK(!s.limited);
}

static void testDischarge10kW()
{
    // I = (211.68 - sqrt(211.68^2 - 4*0.3*10000)) / 0.6
    //   = (211.68 - sqrt(32808.4224)) / 0.6 = (211.68 - 181.130954) / 0.6 = 50.915077 A
    // V = 211.68 - 50.915077*0.3 = 196.405477 V,  I^2*R = 777.70 W
    // dSOC = 50.915077 / 23400 = 0.00217586,  dT = 777.70 / 36000 = 0.021603 K
    Battery b(kParams, 0.6, 25.0);
    const BatteryStep s = b.step(10000.0, 1.0, 25.0);
    HEA_CHECK_NEAR(s.current_A, 50.915077, 1e-5);
    HEA_CHECK_NEAR(s.voltage_V, 196.405477, 1e-5);
    HEA_CHECK_NEAR(s.voltage_V * s.current_A, 10000.0, 1e-6); // P = V*I
    HEA_CHECK_NEAR(s.heat_W, 777.70, 1e-2);
    HEA_CHECK_NEAR(s.soc, 0.6 - 0.00217586, 1e-7);
    HEA_CHECK_NEAR(s.temp_C, 25.0 + 0.021603, 1e-5);
    HEA_CHECK(!s.limited);
}

static void testChargeRaisesVoltage()
{
    Battery b(kParams, 0.6, 25.0);
    const BatteryStep s = b.step(-10000.0, 1.0, 25.0);
    HEA_CHECK(s.current_A < 0.0);            // şarj akımı negatif
    HEA_CHECK(s.voltage_V > s.ocv_V);        // şarjda uç gerilimi OCV'nin üstünde
    HEA_CHECK(s.soc > 0.6);                  // SOC artar
    HEA_CHECK_NEAR(s.voltage_V * s.current_A, -10000.0, 1e-6);
}

static void testEnergyBalance()
{
    // Enerji korunumu: kimyasal güç (OCV*I) = uç gücü (V*I) + ısı kaybı (I^2*R)
    // Deşarjda kayıp bataryadan ekstra çekilir, şarjda giren enerjinin bir kısmı ısıya gider.
    for (double p : {15000.0, 3000.0, -3000.0, -15000.0}) {
        Battery b(kParams, 0.55, 20.0);
        const BatteryStep s = b.step(p, 1.0, 20.0);
        HEA_CHECK_NEAR(s.ocv_V * s.current_A, s.power_W + s.heat_W, 1e-6);
    }
}

static void testPowerLimits()
{
    // SOC 0.6, 25 °C: akım sınırında güç = 211.68*150 - 0.3*150^2 = 25002 W (< 27 kW)
    Battery b(kParams, 0.6, 25.0);
    HEA_CHECK_NEAR(b.availableDischargePower(), 25002.0, 1e-6);
    // Şarj: 150*(211.68 + 0.3*150) = 38502 W > 25 kW sınırı -> 25000 W
    HEA_CHECK_NEAR(b.availableChargePower(), 25000.0, 1e-9);

    const BatteryStep s = b.step(50000.0, 1.0, 25.0);
    HEA_CHECK(s.limited);
    HEA_CHECK_NEAR(s.power_W, 25002.0, 1e-6);
    HEA_CHECK_NEAR(s.current_A, 150.0, 1e-6);
}

static void testSocWindow()
{
    // Alt sınırda deşarj yok, şarj serbest
    Battery low(kParams, 0.30, 25.0);
    HEA_CHECK_NEAR(low.availableDischargePower(), 0.0, 0.0);
    const BatteryStep s1 = low.step(5000.0, 1.0, 25.0);
    HEA_CHECK(s1.limited);
    HEA_CHECK_NEAR(s1.power_W, 0.0, 0.0);
    HEA_CHECK(low.availableChargePower() > 0.0);

    // Üst sınırda şarj yok
    Battery high(kParams, 0.80, 25.0);
    HEA_CHECK_NEAR(high.availableChargePower(), 0.0, 0.0);
    HEA_CHECK(high.step(-5000.0, 1.0, 25.0).limited);

    // Sürekli 20 kW çekince SOC alt sınırda durmalı; en fazla bir adımlık taşma
    Battery drain(kParams, 0.6, 25.0);
    for (int i = 0; i < 3000; ++i) {
        drain.step(20000.0, 1.0, 25.0);
    }
    HEA_CHECK(drain.soc() <= 0.30);
    HEA_CHECK(drain.soc() > 0.30 - 0.01);
}

static void testThermal()
{
    // Akım yokken ortama soğuma: dT = -25 * (40 - 25) / 36000 * 10 s = -0.104167 K
    Battery b(kParams, 0.6, 40.0);
    HEA_CHECK_NEAR(b.step(0.0, 10.0, 25.0).temp_C, 40.0 - 0.104167, 1e-6);

    // Soğuk batarya: aynı güçte daha yüksek direnç -> daha düşük gerilim, daha çok ısı
    Battery cold(kParams, 0.6, 0.0);
    Battery warm(kParams, 0.6, 25.0);
    const BatteryStep c = cold.step(10000.0, 1.0, 0.0);
    const BatteryStep w = warm.step(10000.0, 1.0, 25.0);
    HEA_CHECK(c.voltage_V < w.voltage_V);
    HEA_CHECK(c.heat_W > w.heat_W);
}

static void testStoredEnergy()
{
    // SOC 0.6 -> 0.5: hücre OCV 3.78 -> 3.72 doğrusal, ortalama 3.75 V
    //   E = 23400 C * 56 * 3.75 V * 0.1 = 491400 J (136.5 Wh)
    HEA_CHECK_NEAR(storedEnergyReleased_J(kParams, 0.6, 0.5), 491400.0, 1e-3);
    // Ters yön: aynı büyüklük, ters işaret (şarj)
    HEA_CHECK_NEAR(storedEnergyReleased_J(kParams, 0.5, 0.6), -491400.0, 1e-3);
    // SOC değişmezse tam sıfır
    HEA_CHECK_NEAR(storedEnergyReleased_J(kParams, 0.63, 0.63), 0.0, 0.0);

    // Depolanan enerji kaybı, uçlardan çekilen enerjiden I^2*R kadar azdır (deşarjda)
    Battery b(kParams, 0.6, 25.0);
    double terminal = 0.0;
    double heat = 0.0;
    for (int i = 0; i < 100; ++i) {
        const BatteryStep s = b.step(15000.0, 1.0, 25.0);
        terminal += s.power_W;
        heat += s.heat_W;
    }
    // OCV*I*dt = V*I*dt + I^2*R*dt  ->  depolanan = uç + ısı (Euler'e göre küçük fark)
    const double stored = storedEnergyReleased_J(kParams, 0.6, b.soc());
    HEA_CHECK(std::fabs(stored - (terminal + heat)) / stored < 0.002);
}

int main()
{
    testParams();
    testStoredEnergy();
    testOcvCurve();
    testResistanceVsTemperature();
    testZeroPower();
    testDischarge10kW();
    testChargeRaisesVoltage();
    testEnergyBalance();
    testPowerLimits();
    testSocWindow();
    testThermal();
    return HEA_TEST_RESULT();
}
