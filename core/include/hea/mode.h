#pragma once

namespace hea {

// Aracın o adımdaki çalışma modu.
// Stratejiler yalnızca Electric veya Engine önerir; Stopped ve RegenBraking
// durumlarını simülasyon, aracın durumuna (durmuş / frenliyor) göre kendisi belirler.
enum class Mode {
    Stopped,      // Araç duruyor, tahrik gücü yok
    Electric,     // Tahrik yalnızca elektrik motorundan (batarya boşalır)
    Engine,       // Tahrik içten yanmalı motordan (yakıt harcanır)
    RegenBraking  // Frenleme enerjisi elektrik motoruyla bataryaya geri kazanılıyor
};

// Modun kısa, ASCII adı (ekrana/CSV'ye yazmak için; core kendisi yazdırmaz).
const char* modeName(Mode mode);

} // namespace hea
