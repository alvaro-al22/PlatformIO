#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_wifi_types.h"

#define LAB_WIFI_TRUST_LIMIT 16
#define LAB_WIFI_SSID_DISPLAY_SIZE 129

typedef enum {
    LAB_WIFI_PMF_UNKNOWN,
    LAB_WIFI_PMF_UNSUPPORTED,
    LAB_WIFI_PMF_OPTIONAL,
    LAB_WIFI_PMF_REQUIRED,
} lab_wifi_pmf_t;

bool lab_wifi_pmf_frame_matches(const uint8_t *frame_without_fcs, size_t length,
                                const uint8_t bssid[6]);
lab_wifi_pmf_t lab_wifi_pmf_from_frame(const uint8_t *frame_without_fcs, size_t length);
const char *lab_wifi_pmf_name(lab_wifi_pmf_t pmf);

typedef struct {
    wifi_ap_record_t aps[LAB_WIFI_TRUST_LIMIT];
    size_t count;
} lab_wifi_trust_store_t;

typedef enum {
    LAB_WIFI_TRUST_ADDED,
    LAB_WIFI_TRUST_EXISTS,
    LAB_WIFI_TRUST_CHANGED,
    LAB_WIFI_TRUST_FULL,
    LAB_WIFI_TRUST_HIDDEN,
} lab_wifi_trust_result_t;

enum {
    LAB_WIFI_UNKNOWN_BSSID = 1U << 0,
    LAB_WIFI_SSID_CHANGED = 1U << 1,
    LAB_WIFI_SECURITY_CHANGED = 1U << 2,
};

bool lab_wifi_parse_bssid(const char *text, uint8_t address[6]);
void lab_wifi_ssid_display(const uint8_t ssid[33], char text[LAB_WIFI_SSID_DISPLAY_SIZE]);
const char *lab_wifi_auth_name(wifi_auth_mode_t mode);
const char *lab_wifi_cipher_name(wifi_cipher_type_t cipher);
const char *lab_wifi_auth_warning(wifi_auth_mode_t mode);
bool lab_wifi_legacy_cipher(wifi_cipher_type_t cipher);
lab_wifi_trust_result_t lab_wifi_trust_add(lab_wifi_trust_store_t *store, const wifi_ap_record_t *ap);
unsigned lab_wifi_suspect_flags(const lab_wifi_trust_store_t *store, const wifi_ap_record_t *ap);