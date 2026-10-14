#pragma once

#include <string>
#include <vector>

#include "hea/comparison.h"
#include "hea/simulation.h"
#include "hea/step_result.h"

// Terminal çıktısı: adım satırları ve özet tablo.
// Kural: hızlar km/h, devirler dev/dak olarak gösterilir; hesaplar SI kalır.
namespace cli {

// Bir çevrim + strateji çalıştırmasının sonucu
struct RunRecord {
    std::string cycle;
    std::string strategy;
    std::vector<hea::StepResult> steps;
    hea::RunSummary summary;
};

void printStepHeader();
void printStep(const hea::StepResult& r);

// Tüm çalıştırmaların karşılaştırmalı özet tablosu (çevrim bazında gruplu)
void printSummaryTable(const std::vector<RunRecord>& runs);

// SOC düzeltmeli karşılaştırmanın bir satırı (bir çevrim + bir strateji)
struct ComparisonRow {
    std::string cycle;
    std::string strategy;
    hea::RunSummary raw;               // başlangıç SOC 0.60 ile tek koşu
    double rawStoredEnergy_J = 0.0;    // ham koşuda depolanan enerji kaybı (+ = boşaldı)
    hea::SocCorrectedFuel corrected;   // doğrusal SOC düzeltmesi (ana yöntem)
};

// Düzeltilmiş tüketim tablosu ve tüm çevrimler üzerinden toplam.
// referenceStrategy: farkların hesaplandığı strateji (ör. "kural-tabanli")
void printComparisonTable(const std::vector<ComparisonRow>& rows,
                          const std::string& referenceStrategy);

// Her satırın düzeltme noktaları (başlangıç/bitiş SOC, E_bat, yakıt)
void printCorrectionPoints(const std::vector<ComparisonRow>& rows);

} // namespace cli
