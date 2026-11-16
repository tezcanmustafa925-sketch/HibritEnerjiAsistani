#pragma once

// Canlı (adım adım) çalıştırma. ImGui bilmez; birim testlerle sınanır
// (tests/test_gui_session.cpp).
//
// Arayüz her karede advanceRealTime() ile geçen gerçek süreyi verir; simülasyon
// "hız çarpanı" kadar hızlı ilerler (10 kat: 1 gerçek saniyede 10 simülasyon saniyesi,
// hea_cli --hiz ile aynı anlam). Sonuna kadar ilerletildiğinde sonuç
// Session::run ile birebir aynıdır (aynı core çağrıları, aynı sıra).
//
// Çevrimin (ayarlar uygulanmış) ve stratejinin kendi kopyalarını tutar: çalışırken
// Session'daki çevrim listesi ya da ayarlar değişse de bu çalıştırma etkilenmez.

#include <cstddef>
#include <memory>
#include <string>

#include "session.h"

namespace gui {

class LiveRun {
public:
    // Bir karede hesaba katılan en uzun gerçek süre [s]. Pencere sürüklenirken ya da
    // dosya penceresi açıkken kareler durur; ardından gelen uzun kare simülasyonu
    // bir anda ileri atmasın.
    static constexpr double kMaxFrameSeconds = 0.25;

    LiveRun();
    ~LiveRun();
    LiveRun(LiveRun&&) noexcept;
    LiveRun& operator=(LiveRun&&) noexcept;

    // Seçilen ayarlarla yeni bir çalıştırma hazırlar (henüz adım atılmaz).
    // Session::run ile aynı denetimler; hata varsa açıklama döner ve önceki
    // çalıştırma olduğu gibi kalır.
    std::string start(const Session& session, const RunSettings& settings);

    bool active() const { return state_ != nullptr; } // bir çalıştırma var mı (bitmiş olabilir)
    bool finished() const;                             // tüm adımlar atıldı mı
    // Adım sırasında core hata verdiyse açıklaması (çalıştırma orada durur)
    const std::string& error() const;

    // En çok maxSteps adım ilerler; atılan adım sayısını döner.
    std::size_t advance(std::size_t maxSteps);
    // realSeconds gerçek saniye geçti; speed: simülasyon saniyesi / gerçek saniye.
    // Biriken simülasyon süresi bir adımı karşılayınca adım atılır (artan kısım
    // sonraki çağrıya kalır). Atılan adım sayısını döner.
    std::size_t advanceRealTime(double realSeconds, double speed);

    std::size_t stepCount() const;  // atılan adım
    std::size_t totalSteps() const; // çevrimdeki toplam adım (örnek sayısı - 1)
    double time_s() const;          // simülasyon zamanı: son adımın sonu (başta ilk örnek)
    double startTime_s() const;     // çevrimin ilk örneğinin zamanı
    double endTime_s() const;       // çevrimin son örneğinin zamanı

    // Ayarlar uygulanmış çevrim (hedef hız grafiği için tamamı baştan bilinir)
    const hea::DriveCycle& cycle() const;
    // O ana kadar atılan adımlar ve özetleri (çalıştırma bitince Session::run ile aynı)
    const RunResult& result() const;

private:
    struct State;
    std::unique_ptr<State> state_; // Simulation çevrime ve stratejiye referans tutar:
                                   // hepsi yığında, yer değiştirmez
};

} // namespace gui
