#pragma once

#include <string>
#include <vector>

#include "hea/training_data.h"
#include "report.h"

// Adım sonuçlarını CSV dosyasına yazar (Python'da analiz ve model eğitimi için).
// CSV makine için olduğundan tüm sütunlar SI biriminde, başlıkta birim soneki var
// (ör. speed_mps, fuel_rate_kgps). Ondalık ayırıcı nokta, alan ayırıcı virgül.
namespace cli {

// Başarılıysa true; dosya açılamazsa false.
bool writeStepsCsv(const std::string& path, const std::vector<RunRecord>& runs);

// Tüketim modeli eğitim örneklerini yazar (özellikler + iki modun maliyeti).
bool writeTrainingCsv(const std::string& path, const std::vector<hea::TrainingSample>& samples);

// Her koşunun özeti (bir satır = bir çevrim + strateji).
bool writeSummaryCsv(const std::string& path, const std::vector<RunRecord>& runs);

// SOC düzeltmeli karşılaştırma (bir satır = bir çevrim + strateji).
bool writeComparisonCsv(const std::string& path, const std::vector<ComparisonRow>& rows);

} // namespace cli
