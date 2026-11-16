#pragma once

// Sonuç CSV biçimleri: terminal uygulaması (hea_cli --csv, --karsilastir-csv) ve grafik
// arayüz ("CSV olarak kaydet") aynı satırları buradan yazar, iki programın dosyaları
// bayt bayt aynıdır.
//
// Dosya açmaz: verilen akışa yazar. Dosyayı her uygulama kendisi açar (arayüz Türkçe
// karakterli Windows yolları için std::filesystem kullanır). Akış metin kipinde açılmalı
// (Windows'ta satır sonu CRLF olur, hea_cli'nin eski çıktısıyla aynı).
//
// CSV makine için olduğundan tüm sütunlar SI biriminde, başlıkta birim soneki var
// (ör. speed_mps, fuel_rate_kgps). Ondalık ayırıcı nokta, alan ayırıcı virgül, sayılar %.9g.

#include <ostream>
#include <string>
#include <vector>

#include "hea/comparison.h"
#include "hea/simulation.h"
#include "hea/step_result.h"

namespace hea_csv {

// Adım sonuçları: bir satır = bir adım
void writeStepsHeader(std::ostream& out);
void writeStepRows(std::ostream& out, const std::string& cycle, const std::string& strategy,
                   const std::vector<hea::StepResult>& steps);

// SOC düzeltmeli karşılaştırma: bir satır = bir çevrim + strateji
void writeComparisonHeader(std::ostream& out);
void writeComparisonRow(std::ostream& out, const std::string& cycle, const std::string& strategy,
                        const hea::RunSummary& raw, double rawStoredEnergy_J,
                        const hea::SocCorrectedFuel& corrected);

} // namespace hea_csv
