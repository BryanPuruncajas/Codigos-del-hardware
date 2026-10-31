#pragma once

#include <stdint.h>

// Wire format shared by the blimp firmware and the dedicated Flow Deck node.
namespace FlowProtocol {

constexpr uint16_t MAGIC = 0xB17F;
constexpr uint8_t VERSION = 1;
constexpr uint32_t RX_TIMEOUT_MS = 500;

enum Flag : uint8_t {
    FLOW_VALID = 1u << 0,
    RANGE_VALID = 1u << 1,
};

struct Packet {
    uint16_t magic;
    uint8_t version;
    uint8_t flags;
    uint32_t sequence;
    int16_t dx;
    int16_t dy;
    uint16_t range_mm;
    uint8_t quality;
    uint8_t reserved;
};

static_assert(sizeof(Packet) == 16, "FlowProtocol::Packet wire size changed");

} // namespace FlowProtocol
