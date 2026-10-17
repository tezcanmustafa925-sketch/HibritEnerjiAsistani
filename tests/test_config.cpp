// Yapılandırma dosyası, CSV çevrim okuma ve standart çevrim testleri.

#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>

#include "hea/config_file.h"
#include "hea/cycle_io.h"
#include "hea/units.h"
#include "test_util.h"

using namespace hea;

static void writeFile(const char* path, const std::string& text)
{
    std::ofstream f(path, std::ios::binary);
    f << text;
}

// İfade std::exception türünden bir hata fırlatmalı
template <typename E, typename F>
static bool throws(F&& f)
{
    try {
        f();
    } catch (const E&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

static void testAssignments()
{
    FullConfig c;
    applyConfigValue(c, "batarya.kapasite_Ah", "8.5");
    HEA_CHECK_NEAR(c.sim.battery.capacity_Ah, 8.5, 0.0);
    applyConfigAssignment(c, "batarya.seri_hucre = 60");
    HEA_CHECK(c.sim.battery.cellsInSeries == 60);
    applyConfigAssignment(c, "klima.acik=hayir");
    HEA_CHECK(!c.sim.hvac.enabled);
    applyConfigAssignment(c, "aktarma.vitesler = 3.0, 1.8, 1.0");
    HEA_CHECK(c.sim.drivetrain.gearRatios.size() == 3);
    HEA_CHECK_NEAR(c.sim.drivetrain.gearRatios[1], 1.8, 0.0);
    // Motor çalıştırma yakıtı tek kaynak: model stratejisi de aynı değeri alır
    applyConfigAssignment(c, "simulasyon.motor_calistirma_yakiti_kg=0.0005");
    HEA_CHECK_NEAR(c.sim.engineStartFuel_kg, 0.0005, 0.0);
    HEA_CHECK_NEAR(c.model.engineStartFuel_kg, 0.0005, 0.0);
    HEA_CHECK(c.validate().empty());

    // Hatalar
    HEA_CHECK(throws<std::invalid_argument>([&] { applyConfigValue(c, "batarya.yok", "1"); }));
    HEA_CHECK(throws<std::invalid_argument>([&] { applyConfigValue(c, "batarya.kapasite_Ah", "5abc"); }));
    HEA_CHECK(throws<std::invalid_argument>([&] { applyConfigValue(c, "batarya.seri_hucre", "5.5"); }));
    HEA_CHECK(throws<std::invalid_argument>([&] { applyConfigValue(c, "klima.acik", "belki"); }));
    HEA_CHECK(throws<std::invalid_argument>([&] { applyConfigAssignment(c, "esittirsiz"); }));

    // Birlikte geçersiz değerler validate() ile yakalanır
    FullConfig bad;
    applyConfigValue(bad, "simulasyon.baslangic_soc", "0.95"); // pencere 0.30-0.80
    HEA_CHECK(!bad.validate().empty());
    FullConfig badRule;
    applyConfigValue(badRule, "kural.soc_alt", "0.7"); // hedefin üstünde
    HEA_CHECK(!badRule.validate().empty());
}

static void testFileRoundTrip()
{
    // Varsayılanların metni yeniden okunduğunda aynı ayarlar elde edilmeli
    FullConfig a;
    a.sim.battery.capacity_Ah = 7.25;
    a.sim.hvac.enabled = false;
    a.sim.engine.fullLoadTorques_Nm[2] = 150.0;
    writeFile("test_ayar_tur.ini", configToText(a));
    FullConfig b;
    loadConfigFile("test_ayar_tur.ini", b);
    HEA_CHECK(configToText(a) == configToText(b));
    HEA_CHECK_NEAR(b.sim.battery.capacity_Ah, 7.25, 0.0);
    HEA_CHECK(!b.sim.hvac.enabled);
    HEA_CHECK_NEAR(b.sim.engine.fullLoadTorques_Nm[2], 150.0, 0.0);
    std::remove("test_ayar_tur.ini");
}

static void testFileSyntax()
{
    // Bölüm başlığı, satır içi yorum, BOM, tam anahtar (bölümden bağımsız)
    writeFile("test_ayar_sozdizimi.ini",
              "\xEF\xBB\xBF# yorum\n"
              "[batarya]\n"
              "kapasite_Ah = 9   # satir ici yorum\n"
              "\n"
              "[arac]\n"
              "bos_kutle_kg=1500\n"
              "klima.konfor_C = 20\n");
    FullConfig c;
    loadConfigFile("test_ayar_sozdizimi.ini", c);
    HEA_CHECK_NEAR(c.sim.battery.capacity_Ah, 9.0, 0.0);
    HEA_CHECK_NEAR(c.sim.vehicle.curbMass_kg, 1500.0, 0.0);
    HEA_CHECK_NEAR(c.sim.hvac.comfortTemp_C, 20.0, 0.0);

    // Hata mesajı satır numarasını içermeli
    writeFile("test_ayar_hatali.ini", "[batarya]\nkapasite_Ah = 9\nbilinmeyen = 1\n");
    bool gotLine = false;
    try {
        FullConfig d;
        loadConfigFile("test_ayar_hatali.ini", d);
    } catch (const std::runtime_error& e) {
        gotLine = std::string(e.what()).find(":3:") != std::string::npos;
    }
    HEA_CHECK(gotLine);
    HEA_CHECK(throws<std::runtime_error>([] {
        FullConfig d;
        loadConfigFile("boyle_bir_dosya_yok.ini", d);
    }));
    std::remove("test_ayar_sozdizimi.ini");
    std::remove("test_ayar_hatali.ini");
}

static void testCycleCsv()
{
    // Düzensiz aralıklı örnekler 1 s'ye yeniden örneklenir, zaman 0'a kaydırılır
    writeFile("test_cevrim.csv",
              "# yorum\n"
              "time_s,speed_kmh,grade_percent\n"
              "10,0,0\n"
              "14,36,2\n"
              "20,36,2\n");
    const DriveCycle c = loadCycleCsv("test_cevrim.csv");
    HEA_CHECK(c.name == "test_cevrim");
    HEA_CHECK(c.samples.size() == 11);
    HEA_CHECK_NEAR(c.samples[0].time_s, 0.0, 0.0);
    HEA_CHECK_NEAR(c.samples[2].targetSpeed_mps, 5.0, 1e-12); // 36 km/h'nin yarısı
    HEA_CHECK_NEAR(c.samples[10].targetSpeed_mps, 10.0, 1e-12);
    HEA_CHECK_NEAR(c.samples[4].grade_rad, units::gradePercentToRad(2.0), 1e-12);
    HEA_CHECK_NEAR(c.samples[0].payload_kg, 75.0, 0.0);     // varsayılan
    HEA_CHECK_NEAR(c.samples[0].ambientTemp_C, 25.0, 0.0);  // varsayılan

    // speed_mps sütunu da kabul edilir
    writeFile("test_cevrim_mps.csv", "time_s,speed_mps,payload_kg,ambient_C\n0,0,200,-5\n2,4,200,-5\n");
    const DriveCycle m = loadCycleCsv("test_cevrim_mps.csv");
    HEA_CHECK_NEAR(m.samples[1].targetSpeed_mps, 2.0, 1e-12);
    HEA_CHECK_NEAR(m.samples[1].payload_kg, 200.0, 0.0);
    HEA_CHECK_NEAR(m.samples[1].ambientTemp_C, -5.0, 0.0);

    // Hatalı dosyalar
    auto bad = [](const char* text) {
        writeFile("test_cevrim_hatali.csv", text);
        return throws<std::runtime_error>([] { loadCycleCsv("test_cevrim_hatali.csv"); });
    };
    HEA_CHECK(bad("speed_kmh\n0\n1\n"));                   // time_s yok
    HEA_CHECK(bad("time_s,hiz\n0,0\n1,1\n"));              // hız sütunu yok
    HEA_CHECK(bad("time_s,speed_kmh\n0,0\n0,1\n"));        // zaman artmıyor
    HEA_CHECK(bad("time_s,speed_kmh\n0,0\n1,-3\n"));       // negatif hız
    HEA_CHECK(bad("time_s,speed_kmh\n0,0\n1,abc\n"));      // sayı değil
    HEA_CHECK(bad("time_s,speed_kmh\n0,0\n1\n"));          // eksik alan
    HEA_CHECK(bad("time_s,speed_kmh\n0,0\n"));             // tek satır
    HEA_CHECK(throws<std::runtime_error>([] { loadCycleCsv("boyle_bir_dosya_yok.csv"); }));
    std::remove("test_cevrim.csv");
    std::remove("test_cevrim_mps.csv");
    std::remove("test_cevrim_hatali.csv");
}

static void testNedc()
{
    const DriveCycle c = makeNedcCycle();
    // 4 x 195 s + 400 s = 1180 s
    HEA_CHECK_NEAR(c.duration_s(), 1180.0, 0.0);
    // Yönetmelik: ~10.93 km (doğrusal yaklaşım: 10.89 km)
    HEA_CHECK(std::fabs(c.distance_m() - 10930.0) / 10930.0 < 0.01);
    HEA_CHECK_NEAR(c.maxSpeed_mps(), units::kmhToMps(120.0), 1e-9);
    // ECE-15 tepe hızı 50 km/h: ilk 780 s'de bu aşılmaz
    for (const DriveInput& s : c.samples) {
        if (s.time_s <= 780.0) {
            HEA_CHECK(s.targetSpeed_mps <= units::kmhToMps(50.0) + 1e-9);
        }
    }
    HEA_CHECK(standardCycles().size() == 1);
}

int main()
{
    testAssignments();
    testFileRoundTrip();
    testFileSyntax();
    testCycleCsv();
    testNedc();
    return HEA_TEST_RESULT();
}
