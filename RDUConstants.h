#ifndef RDUCONSTANTS_H
#define RDUCONSTANTS_H
#include <cstdint>

constexpr uint32_t LINES = 275;
constexpr uint32_t COLUMNS = 480;
constexpr uint32_t BYTES_PER_PIXEL = 2; //RGB565
constexpr uint32_t BYTES_PER_LINE = COLUMNS * BYTES_PER_PIXEL;
constexpr uint32_t BYTES_PER_FRAME = BYTES_PER_LINE * LINES;
constexpr uint32_t BYTES_PACKET_OVERHEAD = 4;

#endif // RDUCONSTANTS_H
