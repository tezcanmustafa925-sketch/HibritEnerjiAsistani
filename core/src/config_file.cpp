#include "hea/config_file.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace hea {

namespace {

// Bir yapılandırma anahtarının hedefi. Yalnızca biri dolu.
struct Entry {
    const char* section;
    const char* name;
    const char* description;
    double* d = nullptr;
    int* i = nullptr;
    bool* b = nullptr;
    std::vector<double>* v = nullptr;
};

Entry D(const char* s, const char* n, const char* desc, double& x) { Entry e{s, n, desc}; e.d = &x; return e; }
Entry I(const char* s, const char* n, const char* desc, int& x) { Entry e{s, n, desc}; e.i = &x; return e; }
Entry B(const char* s, const char* n, const char* desc, bool& x) { Entry e{s, n, desc}; e.b = &x; return e; }
Entry V(const char* s, const char* n, const char* desc, std::vector<double>& x) { Entry e{s, n, desc}; e.v = &x; return e; }

// Tüm anahtarlar. Sıra, configToText çıktısının sırasıdır.
std::vector<Entry> entries(FullConfig& c)
{
    SimulationConfig& s = c.sim;
    return {
        D("simulasyon", "baslangic_soc", "Baslangic SOC [0..1]", s.initialSoc),
        D("simulasyon", "yardimci_yuk_W", "Surekli yardimci yuk (12 V, kontrol uniteleri) [W]", s.auxiliaryPower_W),
        D("simulasyon", "lastik_surtunme", "Lastik-yol surtunme katsayisi mu (fren pedali olcegi) [-]", s.tireRoadFriction),
        D("simulasyon", "motor_calistirma_yakiti_kg", "Her motor calistirmada ek yakit [kg] (model stratejisi de kullanir)", s.engineStartFuel_kg),
        B("simulasyon", "soguk_baslangic", "Motor ortam sicakliginda baslar (evet/hayir)", s.engineColdStart),

        D("arac", "bos_kutle_kg", "Bos arac kutlesi [kg]", s.vehicle.curbMass_kg),
        D("arac", "cd", "Aerodinamik direnc katsayisi [-]", s.vehicle.dragCoefficient),
        D("arac", "on_alan_m2", "On kesit alani [m^2]", s.vehicle.frontalArea_m2),
        D("arac", "crr", "Yuvarlanma direnci katsayisi [-]", s.vehicle.rollingResistanceCoeff),
        D("arac", "teker_yaricapi_m", "Dinamik tekerlek yaricapi [m]", s.vehicle.wheelRadius_m),
        D("arac", "donen_kutle_katsayisi", "Donen kutle katsayisi delta [-]", s.vehicle.rotationalMassFactor),

        I("batarya", "seri_hucre", "Seri hucre sayisi [-]", s.battery.cellsInSeries),
        D("batarya", "kapasite_Ah", "Nominal kapasite [Ah]", s.battery.capacity_Ah),
        D("batarya", "ic_direnc_ohm", "25 C'de seri ic direnc R0 [Ohm]", s.battery.internalResistance25_Ohm),
        D("batarya", "direnc_sicaklik_katsayisi", "Direncin sicaklik katsayisi [1/K]", s.battery.resistanceTempCoeff_perK),
        D("batarya", "polarizasyon_direnci_ohm", "RC dali direnci R1 [Ohm] (0 = dal yok)", s.battery.polarizationResistance_Ohm),
        D("batarya", "polarizasyon_zaman_sabiti_s", "RC dali zaman sabiti tau [s]", s.battery.polarizationTimeConstant_s),
        D("batarya", "max_desarj_W", "En buyuk desarj gucu [W]", s.battery.maxDischargePower_W),
        D("batarya", "max_sarj_W", "En buyuk sarj gucu [W]", s.battery.maxChargePower_W),
        D("batarya", "max_akim_A", "Akim siniri [A]", s.battery.maxCurrent_A),
        D("batarya", "soc_min", "SOC penceresi alt siniri [0..1]", s.battery.socMin),
        D("batarya", "soc_max", "SOC penceresi ust siniri [0..1]", s.battery.socMax),
        D("batarya", "isil_kapasite_JperK", "Isil kapasite [J/K]", s.battery.thermalCapacity_JperK),
        D("batarya", "isi_aktarimi_WperK", "Ortama isi aktarim katsayisi [W/K]", s.battery.heatTransfer_WperK),

        D("elektrik_motoru", "max_guc_W", "Surekli guc siniri [W]", s.motor.maxPower_W),
        D("elektrik_motoru", "max_tork_Nm", "Tepe tork [Nm]", s.motor.maxTorque_Nm),
        D("elektrik_motoru", "max_devir_radps", "Azami devir [rad/s]", s.motor.maxSpeed_radps),
        D("elektrik_motoru", "bakir_kaybi", "Bakir kaybi katsayisi k_c [W/Nm^2]", s.motor.copperLossCoeff),
        D("elektrik_motoru", "demir_kaybi", "Demir kaybi katsayisi k_i [W/(rad/s)]", s.motor.ironLossCoeff),
        D("elektrik_motoru", "ruzgar_kaybi", "Ruzgar kaybi katsayisi k_w [W/(rad/s)^3]", s.motor.windageLossCoeff),
        D("elektrik_motoru", "sabit_kayip_W", "Sabit kayip C [W]", s.motor.constantLoss_W),

        D("motor", "rolanti_devri_radps", "Rolanti devri [rad/s]", s.engine.idleSpeed_radps),
        D("motor", "max_devir_radps", "Azami devir [rad/s]", s.engine.maxSpeed_radps),
        V("motor", "tam_yuk_devirleri_radps", "Tam yuk egrisi devirleri [rad/s], artan", s.engine.fullLoadSpeeds_radps),
        V("motor", "tam_yuk_torklari_Nm", "Tam yuk egrisi torklari [Nm]", s.engine.fullLoadTorques_Nm),
        D("motor", "indike_verim", "Willans indike verimi e [-]", s.engine.indicatedEfficiency),
        D("motor", "surtunme_torku_Nm", "Surtunme torku sabit kismi a [Nm]", s.engine.frictionTorque0_Nm),
        D("motor", "surtunme_egimi", "Surtunme torkunun devirle artisi b [Nm/(rad/s)]", s.engine.frictionTorqueSlope),
        D("motor", "soguk_surtunme_artisi", "20 C'de surtunme artisi orani [-]", s.engine.coldFrictionIncrease),
        D("motor", "isil_kapasite_JperK", "Motor isil kapasitesi [J/K]", s.engine.thermalCapacity_JperK),
        D("motor", "sogutma_suyu_payi", "Yakit enerjisinin sogutma suyuna giden payi [-]", s.engine.heatToCoolantFraction),
        D("motor", "isi_kaybi_WperK", "Ortama isi kaybi katsayisi [W/K]", s.engine.heatLoss_WperK),

        D("aktarma", "motor_dis_orani", "Elektrik motoru redusiyon orani [-]", s.drivetrain.motorGearRatio),
        D("aktarma", "motor_dis_verimi", "Redusiyon disli verimi [-]", s.drivetrain.motorGearEfficiency),
        V("aktarma", "vitesler", "Vites oranlari [-]", s.drivetrain.gearRatios),
        D("aktarma", "son_oran", "Diferansiyel orani [-]", s.drivetrain.finalDriveRatio),
        D("aktarma", "sanziman_verimi", "Vites kutusu + diferansiyel verimi [-]", s.drivetrain.gearboxEfficiency),
        D("aktarma", "rejen_min_teker_devri_radps", "Bunun altinda rejeneratif fren yok [rad/s]", s.drivetrain.regenMinWheelSpeed_radps),

        B("klima", "acik", "Klima/isitma yuku hesaba katilsin (evet/hayir)", s.hvac.enabled),
        D("klima", "konfor_C", "Kabin konfor sicakligi [C]", s.hvac.comfortTemp_C),
        D("klima", "olu_bant_K", "Konfor bandi yari genisligi [K]", s.hvac.deadband_K),
        D("klima", "sogutma_WperK", "Sogutma gucu katsayisi [W/K]", s.hvac.coolingCoeff_WperK),
        D("klima", "isitma_WperK", "Isitma gucu katsayisi [W/K]", s.hvac.heatingCoeff_WperK),
        D("klima", "max_guc_W", "En buyuk klima gucu [W]", s.hvac.maxPower_W),

        D("kural", "soc_hedef", "Korunan SOC", c.rule.socTarget),
        D("kural", "soc_alt", "Bunun altinda EV yasak", c.rule.socLow),
        D("kural", "soc_ust", "Bunun ustunde EV guc esigi yukselir", c.rule.socHigh),
        D("kural", "ev_max_hiz_mps", "EV azami hizi [m/s]", c.rule.evMaxSpeed_mps),
        D("kural", "hiz_histerezisi_mps", "Motoru kapatmak icin hiz dususu [m/s]", c.rule.speedHysteresis_mps),
        D("kural", "ev_max_guc_W", "EV azami tekerlek gucu [W]", c.rule.evMaxPower_W),
        D("kural", "ev_max_guc_yuksek_soc_W", "SOC yuksekken EV guc esigi [W]", c.rule.evMaxPowerHighSoc_W),
        D("kural", "guc_histerezisi", "Kapatma icin guc esigi orani [-]", c.rule.powerHysteresis),
        D("kural", "min_motor_suresi_s", "Motorun asgari calisma suresi [s]", c.rule.minEngineOnTime_s),
        D("kural", "sarj_kazanci_W", "Sarj kazanci [W / birim SOC]", c.rule.chargeGain_W),
        D("kural", "max_sarj_W", "En buyuk sarj gucu [W]", c.rule.maxChargePower_W),

        D("model", "soc_hedef", "Korunan SOC", c.model.socTarget),
        D("model", "esdegerlik_katsayisi", "Esdegerlik katsayisi s0 [-]", c.model.equivalenceFactor),
        D("model", "soc_geri_besleme", "SOC geri besleme kazanci [-]", c.model.socFeedbackGain),
        D("model", "soc_min_ev", "Bunun altinda EV yasak", c.model.socMinForEv),
        D("model", "sarj_gucu_W", "Sarj karliysa istenen guc [W]", c.model.chargePower_W),
        D("model", "min_motor_suresi_s", "Motorun asgari calisma suresi [s]", c.model.minEngineOnTime_s),
    };
}

std::string trim(const std::string& s)
{
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) {
        return {};
    }
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

double parseDouble(const std::string& key, const std::string& text)
{
    const std::string s = trim(text);
    errno = 0;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (s.empty() || end == s.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite(v)) {
        throw std::invalid_argument("'" + key + "': sayi bekleniyordu, '" + s + "' okunamadi");
    }
    return v;
}

bool parseBool(const std::string& key, const std::string& text)
{
    const std::string s = trim(text);
    if (s == "evet" || s == "1" || s == "true" || s == "acik") return true;
    if (s == "hayir" || s == "0" || s == "false" || s == "kapali") return false;
    throw std::invalid_argument("'" + key + "': evet/hayir bekleniyordu, '" + s + "' okunamadi");
}

std::string formatDouble(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.10g", v);
    return buf;
}

} // namespace

std::string FullConfig::validate() const
{
    const std::string err = sim.validate();
    if (!err.empty()) return err;
    if (!rule.isValid()) return "kural tabanli strateji parametreleri gecersiz (soc_alt < soc_hedef < soc_ust ...)";
    if (!model.isValid()) return "model tabanli strateji parametreleri gecersiz";
    return {};
}

void applyConfigValue(FullConfig& cfg, const std::string& key, const std::string& value)
{
    const std::string k = trim(key);
    for (Entry& e : entries(cfg)) {
        if (k != std::string(e.section) + "." + e.name) {
            continue;
        }
        if (e.d) {
            *e.d = parseDouble(k, value);
        } else if (e.i) {
            const double v = parseDouble(k, value);
            if (v != std::floor(v) || std::fabs(v) > 1e6) {
                throw std::invalid_argument("'" + k + "': tam sayi bekleniyordu");
            }
            *e.i = static_cast<int>(v);
        } else if (e.b) {
            *e.b = parseBool(k, value);
        } else if (e.v) {
            std::vector<double> out;
            std::stringstream ss(value);
            std::string item;
            while (std::getline(ss, item, ',')) {
                out.push_back(parseDouble(k, item));
            }
            if (out.empty()) {
                throw std::invalid_argument("'" + k + "': en az bir deger gerekli");
            }
            *e.v = std::move(out);
        }
        // Motor çalıştırma yakıtı tek kaynaktan: model stratejisi de aynı değeri kullanır
        cfg.model.engineStartFuel_kg = cfg.sim.engineStartFuel_kg;
        return;
    }
    throw std::invalid_argument("bilinmeyen ayar anahtari '" + k
                                + "' (liste icin: hea_cli --ayarlari-yaz <dosya>)");
}

void applyConfigAssignment(FullConfig& cfg, const std::string& assignment)
{
    const auto eq = assignment.find('=');
    if (eq == std::string::npos) {
        throw std::invalid_argument("'" + assignment + "': anahtar=deger bekleniyordu");
    }
    applyConfigValue(cfg, assignment.substr(0, eq), assignment.substr(eq + 1));
}

void loadConfigFile(const std::string& path, FullConfig& cfg)
{
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error(path + ": dosya acilamadi");
    }
    std::string section;
    std::string line;
    int lineNo = 0;
    while (std::getline(f, line)) {
        ++lineNo;
        // Satır içi yorumlar: '#' sonrası yok sayılır
        const auto hash = line.find('#');
        const std::string t = trim(hash == std::string::npos ? line : line.substr(0, hash));
        if (t.empty()) {
            continue;
        }
        std::string body = t;
        // Windows Not Defteri'nin eklediği UTF-8 BOM'u atla
        if (lineNo == 1 && body.compare(0, 3, "\xEF\xBB\xBF") == 0) {
            body = trim(body.substr(3));
            if (body.empty()) continue;
        }
        if (body.front() == '[') {
            if (body.back() != ']') {
                throw std::runtime_error(path + ":" + std::to_string(lineNo) + ": bolum basligi ']' ile bitmeli");
            }
            section = trim(body.substr(1, body.size() - 2));
            continue;
        }
        const auto eq = body.find('=');
        if (eq == std::string::npos) {
            throw std::runtime_error(path + ":" + std::to_string(lineNo) + ": anahtar = deger bekleniyordu");
        }
        std::string key = trim(body.substr(0, eq));
        if (!section.empty() && key.find('.') == std::string::npos) {
            key = section + "." + key;
        }
        try {
            applyConfigValue(cfg, key, body.substr(eq + 1));
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error(path + ":" + std::to_string(lineNo) + ": " + e.what());
        }
    }
}

std::string configToText(const FullConfig& cfgIn)
{
    FullConfig cfg = cfgIn; // entries() değiştirilebilir referans ister
    std::string out =
        "# Akilli Hibrit Enerji Asistani - yapilandirma dosyasi\n"
        "# Kullanim: hea_cli --ayar-dosyasi <bu dosya>\n"
        "# Dosyada olmayan anahtarlar varsayilan degerinde kalir; '#' sonrasi yorumdur.\n"
        "# Tum birimler SI (m, s, kg, W, Nm, rad/s); sicakliklar C.\n";
    std::string section;
    for (const Entry& e : entries(cfg)) {
        if (section != e.section) {
            section = e.section;
            out += "\n[" + section + "]\n";
        }
        std::string value;
        if (e.d) {
            value = formatDouble(*e.d);
        } else if (e.i) {
            value = std::to_string(*e.i);
        } else if (e.b) {
            value = *e.b ? "evet" : "hayir";
        } else if (e.v) {
            for (std::size_t i = 0; i < e.v->size(); ++i) {
                value += (i ? ", " : "") + formatDouble((*e.v)[i]);
            }
        }
        out += "# " + std::string(e.description) + "\n";
        out += std::string(e.name) + " = " + value + "\n";
    }
    return out;
}

} // namespace hea
