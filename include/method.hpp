/**
 * Wang Jiadong <jiadong.wang.94@outlook.com>
 */

#pragma once

#include <utility>
#include <string>
#include <stdint.h>

#include "log.hpp"

#include "event.hpp"

template <typename RequestT, typename ResponseT>
class Method {
 public:
    RequestT request;
    ResponseT response;

    static std::pair<uint32_t, uint32_t> get_id() { return std::make_pair(RequestT::id, ResponseT::id); }
};
