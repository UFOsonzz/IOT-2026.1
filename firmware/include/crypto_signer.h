#pragma once

#include "app_types.h"

namespace crypto {

void computeSha256Hex(const char* input, char outputHex[65]);
void signTelemetryPacket(TelemetryPacket& pkt);

} // namespace crypto