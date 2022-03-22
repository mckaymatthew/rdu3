#ifndef RDUCONSTANTS_H
#define RDUCONSTANTS_H
#include <cstdint>
#include <QAtomicInt>

constexpr uint32_t LINES = 272;
constexpr uint32_t COLUMNS = 480;
constexpr uint32_t BYTES_PER_PIXEL = 2; //RGB565
constexpr uint32_t BYTES_PER_LINE = COLUMNS * BYTES_PER_PIXEL;
constexpr uint32_t BYTES_PER_FRAME = BYTES_PER_LINE * LINES;
constexpr uint32_t BYTES_PACKET_OVERHEAD = 6;

constexpr uint32_t CLK_GATE = 0xf0002800;
constexpr uint32_t CPU_RESET = 0xf0000000;
constexpr uint32_t FPS_DIVISOR = 0xf0002804;
constexpr uint32_t MAIN_DAIL_OFFSET = 0xf0003000;
constexpr uint32_t MULTI_DIAL_OFFSET = 0xf0003004;
constexpr uint32_t BPF_IN_OFFSET = 0xf0003008;
constexpr uint32_t BPF_OUT_OFFSET = 0xf000300C;

constexpr int g_scaleFactor = 100;
extern QAtomicInt g_NetworkBytesPerSecond;
extern QAtomicInt g_NetworkLinesPerSecond;
extern QAtomicInt g_NetworkFramesPerSecond;
extern QAtomicInt g_NetworkFramesTotal;
extern QAtomicInt g_FramesLostNoBuffer;
extern QAtomicInt g_packetsTotal;
extern QAtomicInt g_packetsRejected;



#endif // RDUCONSTANTS_H
