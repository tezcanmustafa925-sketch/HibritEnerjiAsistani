#pragma once

#include <string>

#include "hea/model_based_strategy.h"
#include "hea/rule_based_strategy.h"
#include "hea/simulation.h"

namespace hea {

// Uygulamanın ayarlanabilir tüm parametreleri: simülasyon + iki stratejinin eşikleri.
struct FullConfig {
    SimulationConfig sim;
    RuleBasedParams rule;
    ModelBasedParams model;

    // Hepsi geçerliyse boş metin, değilse ilk hatanın açıklaması
    std::string validate() const;
};

// ----------------------------------------------------------------------------
// Yapılandırma dosyası (INI benzeri, UTF-8):
//
//   # yorum
//   [batarya]
//   kapasite_Ah = 6.5
//   [aktarma]
//   vitesler = 3.5, 2.0, 1.4, 1.0, 0.8
//
// Anahtarlar "bolum.ad" biçimindedir; [bolum] başlığı sonraki satırlara önek
// olarak eklenir. Tüm anahtarların listesi ve varsayılan değerleri için
// configToText(FullConfig{}) ya da "hea_cli --ayarlari-yaz <dosya>".
// Dosyada olmayan anahtarlar varsayılan değerinde kalır.
//
// simulasyon.motor_calistirma_yakiti_kg hem simülasyonun cezasını hem de model
// tabanlı stratejinin maliyet hesabını değiştirir (tek kaynak).
// ----------------------------------------------------------------------------

// Tek bir anahtarı ayarlar. Anahtar bilinmiyorsa ya da değer okunamıyorsa
// std::invalid_argument fırlatır.
void applyConfigValue(FullConfig& cfg, const std::string& key, const std::string& value);

// "anahtar=deger" biçimindeki metni ayarlar (komut satırındaki --ayar için).
void applyConfigAssignment(FullConfig& cfg, const std::string& assignment);

// Dosyayı okur ve cfg'ye uygular. Hata olursa dosya:satır bilgisiyle
// std::runtime_error fırlatır. Değerlerin birlikte geçerliliği burada denetlenmez;
// çağıran validate() ile denetlemelidir.
void loadConfigFile(const std::string& path, FullConfig& cfg);

// Tüm anahtarları açıklamaları ve mevcut değerleriyle, yeniden okunabilir
// biçimde metne döker.
std::string configToText(const FullConfig& cfg);

} // namespace hea
