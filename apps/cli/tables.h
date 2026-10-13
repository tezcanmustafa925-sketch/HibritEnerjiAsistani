#pragma once

#include <vector>

#include "hea/drive_cycle.h"

// Bileşen modellerinin kontrol tabloları (--tablolar) ve çevrim özet tablosu.
namespace cli {

void printComponentTables();
void printCycleTable(const char* title, const std::vector<hea::DriveCycle>& cycles);

} // namespace cli
