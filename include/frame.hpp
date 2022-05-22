#pragma once

#include "stdint.h"

enum FrameType : uint8_t {
    FRAME_UNKNOWN = 0,
    FRAME_INITIATION,
    FRAME_SUBSCRIBE,
    FRAME_PUBLISH,
    FRAME_KEEP_ALIVE,
    FRAME_METHOD,
    FRAME_EVENT
};

struct FrameHeader {
    uint32_t data_length = 0;
    FrameType message_type = FRAME_UNKNOWN;
};

struct Frame {
    FrameHeader header;
    uint8_t *data;
};