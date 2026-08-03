#include <Arduino.h>
#include <sfud.h>

namespace {

constexpr size_t kChunkSize = 512;
constexpr uint32_t kProtocolMagic = 0x50554D57UL;  // WMUP, little endian
// Seeed's SFUD port defaults to only 100 ms of busy polling. A W25Q32
// 4 KiB sector erase may legitimately take several hundred milliseconds.
constexpr size_t kFlashBusyRetries = 10000;

#pragma pack(push, 1)
struct UploadRequest {
  uint32_t magic;
  uint32_t size;
  uint32_t crc32;
};
#pragma pack(pop)

uint8_t buffer[kChunkSize];

uint32_t updateCrc(uint32_t value, const uint8_t *data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    value ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      value = (value >> 1) ^ (0xEDB88320UL & (0UL - (value & 1UL)));
    }
  }
  return value;
}

bool readExact(void *destination, size_t size) {
  return Serial.readBytes(static_cast<char *>(destination), size) == size;
}

bool verify(const sfud_flash *flash, uint32_t size, uint32_t expectedCrc) {
  uint32_t crc = 0xFFFFFFFFUL;
  uint32_t offset = 0;
  while (offset < size) {
    const size_t count = min(static_cast<uint32_t>(sizeof(buffer)), size - offset);
    if (sfud_read(flash, offset, count, buffer) != SFUD_SUCCESS) return false;
    crc = updateCrc(crc, buffer, count);
    offset += count;
  }
  return (crc ^ 0xFFFFFFFFUL) == expectedCrc;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(8000);
  while (!Serial) delay(10);
  Serial.println("WIO_MEMO_FONT_INSTALLER 1");

  if (sfud_init() != SFUD_SUCCESS) {
    Serial.println("ERROR SFUD_INIT");
    return;
  }
#ifdef SFUD_USING_QSPI
  sfud_qspi_fast_read_enable(sfud_get_device(SFUD_W25Q32_DEVICE_INDEX), 2);
#endif
  sfud_flash *flash = sfud_get_device(SFUD_W25Q32_DEVICE_INDEX);
  flash->retry.times = kFlashBusyRetries;
  UploadRequest request{};
  if (!readExact(&request, sizeof(request)) || request.magic != kProtocolMagic || request.size < 32 ||
      request.size > flash->chip.capacity) {
    Serial.println("ERROR REQUEST");
    return;
  }
  const uint32_t eraseGranularity = flash->chip.erase_gran;
  const uint32_t eraseSize =
      ((request.size + eraseGranularity - 1) / eraseGranularity) * eraseGranularity;
  const sfud_err eraseResult = sfud_erase(flash, 0, eraseSize);
  if (eraseResult != SFUD_SUCCESS) {
    Serial.print("ERROR ERASE ");
    Serial.println(static_cast<int>(eraseResult));
    return;
  }
  Serial.println("READY");

  uint32_t offset = 0;
  while (offset < request.size) {
    const size_t count = min(static_cast<uint32_t>(sizeof(buffer)), request.size - offset);
    if (!readExact(buffer, count) || sfud_write(flash, offset, count, buffer) != SFUD_SUCCESS) {
      Serial.println("ERROR WRITE");
      return;
    }
    offset += count;
    Serial.println("ACK");
  }
  Serial.println(verify(flash, request.size, request.crc32) ? "DONE" : "ERROR VERIFY");
}

void loop() { delay(1000); }
