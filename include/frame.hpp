/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

#pragma once

#include "stdint.h"

enum FrameType : uint8_t {
    FRAME_UNKNOWN = 0,
    FRAME_INITIATION,
    FRAME_EVENT_SUBSCRIBE,
    FRAME_EVENT_REGISTER,
    FRAME_EVENT,
    FRAME_KEEP_ALIVE,
    FRAME_METHOD_PROVIDE,
    FRAME_METHOD_QUERY,
    FRAME_METHOD_CALL,
    FRAME_METHOD_RESPONSE
};

struct FrameHeader {
    FrameType message_type = FRAME_UNKNOWN;
    uint32_t data_length = 0;
};

struct Frame {
    FrameHeader header;
    uint8_t *data;
};