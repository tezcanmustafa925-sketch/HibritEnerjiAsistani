#pragma once

#include <string>
#include <vector>

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

} // namespace cli
