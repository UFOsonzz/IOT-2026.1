#include "crypto_signer.h"
#include <Arduino.h>
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <string.h>

namespace crypto {

void computeSha256Hex(const char* input, char outputHex[65]) {
    uint8_t hashBytes[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, reinterpret_cast<const unsigned char*>(input), strlen(input));
    mbedtls_sha256_finish(&ctx, hashBytes);
    mbedtls_sha256_free(&ctx);

    for (int i = 0; i < 32; ++i) {
        snprintf(&outputHex[i * 2], 3, "%02x", hashBytes[i]);
    }
    outputHex[64] = '\0';
}

void signTelemetryPacket(TelemetryPacket& pkt) {
    char canonicalStr[160];
    snprintf(canonicalStr, sizeof(canonicalStr), "%s|%lu|%lu|%.2f|%.2f|%u",
             pkt.device_id,
             static_cast<unsigned long>(pkt.seq_no),
             static_cast<unsigned long>(pkt.ts_rtc),
             pkt.temp_c,
             pkt.hum_pct,
             pkt.alert_code);

    computeSha256Hex(canonicalStr, pkt.sha256_hash);
    strncpy(pkt.ecdsa_sig, "DUMMY_SIG_OK_ESP32_SECP256R1", sizeof(pkt.ecdsa_sig) - 1);
    pkt.ecdsa_sig[sizeof(pkt.ecdsa_sig) - 1] = '\0';

    Serial.printf("[Core 0] Canonical: %s\n", canonicalStr);
}

} // namespace crypto