#include "png_writer.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>

namespace gui {

// PNG yapısı: imza, IHDR (boyut, renk türü), IDAT (zlib akışı), IEND.
// zlib akışı "stored" (sıkıştırmasız) bloklardan oluşur: her blok en çok 65535 bayt.
// Ayrıntı: RFC 2083 (PNG), RFC 1950 (zlib), RFC 1951 (deflate, BTYPE=00).

unsigned long crc32(const unsigned char* data, std::size_t size, unsigned long crc)
{
    static unsigned long table[256];
    static bool init = false;
    if (!init) {
        for (unsigned long n = 0; n < 256; ++n) {
            unsigned long c = n;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1ul) ? 0xEDB88320ul ^ (c >> 1) : c >> 1;
            }
            table[n] = c;
        }
        init = true;
    }
    for (std::size_t i = 0; i < size; ++i) {
        crc = table[(crc ^ data[i]) & 0xFFul] ^ (crc >> 8);
    }
    return crc;
}

unsigned long adler32(const unsigned char* data, std::size_t size)
{
    unsigned long a = 1, b = 0;
    for (std::size_t i = 0; i < size; ++i) {
        a = (a + data[i]) % 65521ul;
        b = (b + a) % 65521ul;
    }
    return (b << 16) | a;
}

namespace {

void put32(std::vector<unsigned char>& v, unsigned long x)
{
    v.push_back(static_cast<unsigned char>((x >> 24) & 0xFF));
    v.push_back(static_cast<unsigned char>((x >> 16) & 0xFF));
    v.push_back(static_cast<unsigned char>((x >> 8) & 0xFF));
    v.push_back(static_cast<unsigned char>(x & 0xFF));
}

void writeChunk(std::ofstream& out, const char* type, const std::vector<unsigned char>& data)
{
    std::vector<unsigned char> buf;
    buf.reserve(data.size() + 12);
    put32(buf, static_cast<unsigned long>(data.size()));
    buf.insert(buf.end(), type, type + 4);
    buf.insert(buf.end(), data.begin(), data.end());
    // CRC: tür + veri
    const unsigned long crc = crc32(buf.data() + 4, data.size() + 4) ^ 0xFFFFFFFFul;
    put32(buf, crc);
    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
}

} // namespace

bool writePngRgb(const std::string& utf8Path, int width, int height,
                 const std::vector<unsigned char>& rgb)
{
    if (width <= 0 || height <= 0) return false;
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 3;
    if (rgb.size() != rowBytes * static_cast<std::size_t>(height)) return false;

    // Ham görüntü: her satırın başında filtre baytı (0 = filtre yok)
    std::vector<unsigned char> raw;
    raw.reserve((rowBytes + 1) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        const auto row = rgb.begin() + static_cast<std::ptrdiff_t>(rowBytes * static_cast<std::size_t>(y));
        raw.insert(raw.end(), row, row + static_cast<std::ptrdiff_t>(rowBytes));
    }

    // zlib: başlık (0x78 0x01), stored bloklar, Adler-32
    std::vector<unsigned char> z;
    z.reserve(raw.size() + raw.size() / 65535 * 5 + 16);
    z.push_back(0x78);
    z.push_back(0x01);
    std::size_t pos = 0;
    do {
        const std::size_t len = std::min<std::size_t>(65535, raw.size() - pos);
        const bool last = pos + len == raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<unsigned char>(len & 0xFF));
        z.push_back(static_cast<unsigned char>((len >> 8) & 0xFF));
        z.push_back(static_cast<unsigned char>(~len & 0xFF));
        z.push_back(static_cast<unsigned char>((~len >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                 raw.begin() + static_cast<std::ptrdiff_t>(pos + len));
        pos += len;
    } while (pos < raw.size());
    put32(z, adler32(raw.data(), raw.size()));

    std::vector<unsigned char> ihdr;
    put32(ihdr, static_cast<unsigned long>(width));
    put32(ihdr, static_cast<unsigned long>(height));
    ihdr.push_back(8); // bit derinliği
    ihdr.push_back(2); // renk türü: RGB
    ihdr.push_back(0); // sıkıştırma: deflate
    ihdr.push_back(0); // filtre yöntemi
    ihdr.push_back(0); // taramalı değil

    std::ofstream out(std::filesystem::u8path(utf8Path), std::ios::binary);
    if (!out) return false;
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    out.write(reinterpret_cast<const char*>(signature), 8);
    writeChunk(out, "IHDR", ihdr);
    writeChunk(out, "IDAT", z);
    writeChunk(out, "IEND", {});
    return static_cast<bool>(out);
}

} // namespace gui
