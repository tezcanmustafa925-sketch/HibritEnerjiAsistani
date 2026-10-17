#include "hea/cycle_io.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

#include "hea/units.h"

namespace hea {

namespace {

// ---------------------------------------------------------------- NEDC

// Parça parça doğrusal hız profili: (süre [s], bitiş hızı [km/h]).
// Bitiş hızı başlangıçla aynıysa sabit hız (ya da bekleme) bölümüdür.
struct Segment {
    double duration_s;
    double endSpeed_kmh;
};

// ECE-15 temel şehir çevrimi, 195 s (UN ECE R83 Ek 4a, Tablo 1)
const Segment kEce15[] = {
    {11, 0},                      // rölanti
    {4, 15}, {8, 15}, {2, 10}, {3, 0}, {21, 0},
    {12, 32}, {24, 32}, {11, 0}, {21, 0},
    {26, 50}, {12, 50}, {8, 35}, {13, 35}, {12, 0}, {7, 0},
};

// EUDC şehir dışı çevrimi, 400 s (UN ECE R83 Ek 4a, Tablo 2)
const Segment kEudc[] = {
    {20, 0},
    {41, 70}, {50, 70}, {8, 50}, {69, 50}, {13, 70}, {50, 70},
    {35, 100}, {30, 100}, {20, 120}, {10, 120},
    {16, 80}, {8, 50}, {10, 0}, {20, 0}, // 120 -> 0 yavaşlaması üç kademede
};

void appendSegments(CycleBuilder& b, const Segment* segs, std::size_t n)
{
    for (std::size_t i = 0; i < n; ++i) {
        const double target = units::kmhToMps(segs[i].endSpeed_kmh);
        const double delta = std::fabs(target - b.currentSpeed());
        if (delta < 1e-12) {
            b.hold(segs[i].duration_s);
        } else {
            // Sabit ivme = |delta| / süre; dt = 1 s'de adım sayısı tam süreye eşit olur
            b.rampTo(target, delta / segs[i].duration_s);
        }
    }
}

// ---------------------------------------------------------------- CSV

std::string trim(const std::string& s)
{
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) {
        return {};
    }
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> splitComma(const std::string& line)
{
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) {
        out.push_back(trim(field));
    }
    // "a,b," gibi sondaki boş alan
    if (!line.empty() && line.back() == ',') {
        out.emplace_back();
    }
    return out;
}

[[noreturn]] void fail(const std::string& path, int line, const std::string& msg)
{
    throw std::runtime_error(path + ":" + std::to_string(line) + ": " + msg);
}

double parseNumber(const std::string& s, const std::string& path, int line, const std::string& col)
{
    if (s.empty()) {
        fail(path, line, "'" + col + "' sutunu bos");
    }
    errno = 0;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite(v)) {
        fail(path, line, "'" + col + "' sayi degil: '" + s + "'");
    }
    return v;
}

std::string stemOf(const std::string& path)
{
    const auto slash = path.find_last_of("/\\");
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const auto dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) {
        name = name.substr(0, dot);
    }
    return name.empty() ? std::string("cevrim") : name;
}

// Ham (düzensiz aralıklı) bir örnek
struct RawSample {
    double t;
    double v_mps;
    double gradePercent;
    double payload_kg;
    double ambient_C;
};

} // namespace

DriveCycle makeNedcCycle()
{
    CycleBuilder b("nedc");
    for (int i = 0; i < 4; ++i) {
        appendSegments(b, kEce15, sizeof(kEce15) / sizeof(kEce15[0]));
    }
    appendSegments(b, kEudc, sizeof(kEudc) / sizeof(kEudc[0]));
    b.payload(75.0);
    b.ambient(25.0);
    return b.build();
}

std::vector<DriveCycle> standardCycles()
{
    return {makeNedcCycle()};
}

DriveCycle loadCycleCsv(const std::string& path, double dt_s)
{
    if (!(dt_s > 0.0)) {
        throw std::invalid_argument("loadCycleCsv: dt_s pozitif olmali");
    }
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error(path + ": dosya acilamadi");
    }

    std::map<std::string, std::size_t> col;
    std::vector<RawSample> raw;
    std::string line;
    int lineNo = 0;
    bool haveHeader = false;
    while (std::getline(f, line)) {
        ++lineNo;
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') {
            continue;
        }
        const std::vector<std::string> fields = splitComma(t);
        if (!haveHeader) {
            for (std::size_t i = 0; i < fields.size(); ++i) {
                col[fields[i]] = i;
            }
            if (!col.count("time_s")) {
                fail(path, lineNo, "baslikta 'time_s' sutunu yok");
            }
            if (!col.count("speed_kmh") && !col.count("speed_mps")) {
                fail(path, lineNo, "baslikta 'speed_kmh' veya 'speed_mps' sutunu yok");
            }
            haveHeader = true;
            continue;
        }
        if (fields.size() != col.size()) {
            fail(path, lineNo, "alan sayisi (" + std::to_string(fields.size())
                                   + ") baslikla (" + std::to_string(col.size()) + ") uyusmuyor");
        }
        auto get = [&](const char* name, double fallback) {
            const auto it = col.find(name);
            return it == col.end() ? fallback : parseNumber(fields[it->second], path, lineNo, name);
        };
        RawSample s;
        s.t = get("time_s", 0.0);
        s.v_mps = col.count("speed_mps") ? get("speed_mps", 0.0)
                                         : units::kmhToMps(get("speed_kmh", 0.0));
        s.gradePercent = get("grade_percent", 0.0);
        s.payload_kg = get("payload_kg", 75.0);
        s.ambient_C = get("ambient_C", 25.0);
        if (s.v_mps < 0.0) {
            fail(path, lineNo, "hiz negatif olamaz");
        }
        if (s.payload_kg < 0.0) {
            fail(path, lineNo, "yuk negatif olamaz");
        }
        if (!raw.empty() && !(s.t > raw.back().t)) {
            fail(path, lineNo, "zaman kesin artan olmali");
        }
        raw.push_back(s);
    }
    if (!haveHeader) {
        throw std::runtime_error(path + ": dosya bos (baslik yok)");
    }
    if (raw.size() < 2) {
        throw std::runtime_error(path + ": en az iki veri satiri gerekli");
    }

    // dt aralığına doğrusal ara değerle yeniden örnekle (ilk zaman 0'a kaydırılır)
    const double t0 = raw.front().t;
    const double duration = raw.back().t - t0;
    const auto n = static_cast<std::size_t>(std::floor(duration / dt_s + 1e-9)) + 1;
    DriveCycle c;
    c.name = stemOf(path);
    c.dt_s = dt_s;
    c.samples.reserve(n);
    std::size_t j = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const double t = t0 + static_cast<double>(i) * dt_s;
        while (j + 2 < raw.size() && raw[j + 1].t < t) {
            ++j;
        }
        const RawSample& a = raw[j];
        const RawSample& b = raw[j + 1];
        const double frac = std::clamp((t - a.t) / (b.t - a.t), 0.0, 1.0);
        auto lerp = [frac](double x, double y) { return x + (y - x) * frac; };
        DriveInput in;
        in.time_s = static_cast<double>(i) * dt_s;
        in.targetSpeed_mps = std::max(0.0, lerp(a.v_mps, b.v_mps));
        in.grade_rad = units::gradePercentToRad(lerp(a.gradePercent, b.gradePercent));
        in.payload_kg = lerp(a.payload_kg, b.payload_kg);
        in.ambientTemp_C = lerp(a.ambient_C, b.ambient_C);
        c.samples.push_back(in);
    }
    return c;
}

} // namespace hea
