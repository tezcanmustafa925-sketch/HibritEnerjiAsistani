#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace gui {

// 8 bit RGB görüntüyü sıkıştırmasız PNG olarak yazar (dış kütüphane yok).
// rgb: satırlar yukarıdan aşağıya, piksel başına 3 bayt (width * height * 3).
// Yol UTF-8'dir. Başarılıysa true.
bool writePngRgb(const std::string& utf8Path, int width, int height,
                 const std::vector<unsigned char>& rgb);

// PNG/zlib sağlama toplamları (testler için açık)
unsigned long crc32(const unsigned char* data, std::size_t size,
                    unsigned long crc = 0xFFFFFFFFul);
unsigned long adler32(const unsigned char* data, std::size_t size);

} // namespace gui
