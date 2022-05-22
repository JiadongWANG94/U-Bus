#include "data_channel.hpp"

struct KeepAliveMessage {};

using KeepAliveChannel = DataChannel<KeepAliveMessage>;
