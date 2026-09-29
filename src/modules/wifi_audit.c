#include "wifi_audit.h"
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#else
#define IRAM_ATTR
#endif

bool IRAM_ATTR lab_wifi_pmf_frame_matches(const uint8_t *frame_without_fcs, size_t length,
                                         const uint8_t bssid[6])
{
    if (frame_without_fcs == NULL || bssid == NULL || length < 36) return false;
    return (frame_without_fcs[0] == 0x80 || frame_without_fcs[0] == 0x50) &&
           (frame_without_fcs[1] & 0xc7) == 0 && (frame_without_fcs[22] & 0x0f) == 0 &&
           (bssid[0] & 1U) == 0 && memcmp(frame_without_fcs + 16, bssid, 6) == 0 &&
           memcmp(frame_without_fcs + 10, bssid, 6) == 0;
}

static uint16_t IRAM_ATTR read_le16(const uint8_t *bytes)
{
    return (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
}

static lab_wifi_pmf_t IRAM_ATTR pmf_from_rsn(const uint8_t *rsn, size_t length)
{
    if (length < 8 || read_le16(rsn) != 1) return LAB_WIFI_PMF_UNKNOWN;
    size_t offset = 6;
    for (unsigned list = 0; list < 2; ++list) {
        if (length - offset < 2) return LAB_WIFI_PMF_UNKNOWN;
        uint16_t count = read_le16(rsn + offset);
        offset += 2;
        if (count == 0 || count > (length - offset) / 4) return LAB_WIFI_PMF_UNKNOWN;
        offset += (size_t)count * 4;
    }
    uint16_t capabilities = 0;
    if (offset < length) {
        if (length - offset < 2) return LAB_WIFI_PMF_UNKNOWN;
        capabilities = read_le16(rsn + offset);
        offset += 2;
    }
    if (offset < length) {
        if (length - offset < 2) return LAB_WIFI_PMF_UNKNOWN;
        uint16_t count = read_le16(rsn + offset);
        offset += 2;
        if (count > (length - offset) / 16) return LAB_WIFI_PMF_UNKNOWN;
        offset += (size_t)count * 16;
        if (length - offset != 0 && length - offset != 4) return LAB_WIFI_PMF_UNKNOWN;
    }
    bool capable = (capabilities & (1U << 7)) != 0;
    bool required = (capabilities & (1U << 6)) != 0;
    if (required && !capable) return LAB_WIFI_PMF_UNKNOWN;
    if (required) return LAB_WIFI_PMF_REQUIRED;
    return capable ? LAB_WIFI_PMF_OPTIONAL : LAB_WIFI_PMF_UNSUPPORTED;
}

lab_wifi_pmf_t IRAM_ATTR lab_wifi_pmf_from_frame(const uint8_t *frame_without_fcs, size_t length)
{
    if (frame_without_fcs == NULL || length < 36) return LAB_WIFI_PMF_UNKNOWN;
    if ((frame_without_fcs[0] != 0x80 && frame_without_fcs[0] != 0x50) ||
        (frame_without_fcs[1] & 0xc7) != 0 ||
        (frame_without_fcs[22] & 0x0f) != 0) return LAB_WIFI_PMF_UNKNOWN;
    bool found_rsn = false;
    bool found_ssid = false;
    lab_wifi_pmf_t pmf = LAB_WIFI_PMF_UNSUPPORTED;
    size_t offset = 36;
    while (offset < length) {
        if (length - offset < 2) return LAB_WIFI_PMF_UNKNOWN;
        uint8_t id = frame_without_fcs[offset];
        size_t size = frame_without_fcs[offset + 1];
        offset += 2;
        if (size > length - offset) return LAB_WIFI_PMF_UNKNOWN;
        if (id == 0) {
            if (found_ssid || size > 32) return LAB_WIFI_PMF_UNKNOWN;
            found_ssid = true;
        } else if (id == 48) {
            if (found_rsn) return LAB_WIFI_PMF_UNKNOWN;
            found_rsn = true;
            pmf = pmf_from_rsn(frame_without_fcs + offset, size);
        }
        offset += size;
    }
    return found_ssid ? pmf : LAB_WIFI_PMF_UNKNOWN;
}

const char *lab_wifi_pmf_name(lab_wifi_pmf_t pmf)
{
    switch (pmf) {
    case LAB_WIFI_PMF_UNSUPPORTED: return "unsupported";
    case LAB_WIFI_PMF_OPTIONAL: return "optional";
    case LAB_WIFI_PMF_REQUIRED: return "required";
    default: return "unknown";
    }
}

static int hex_value(char character)
{
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

bool lab_wifi_parse_bssid(const char *text, uint8_t address[6])
{
    if (text == NULL || address == NULL || strlen(text) != 17) return false;
    uint8_t parsed[6];
    unsigned nonzero = 0;
    for (size_t index = 0; index < 6; ++index) {
        int high = hex_value(text[index * 3]);
        int low = hex_value(text[index * 3 + 1]);
        if (high < 0 || low < 0 || (index < 5 && text[index * 3 + 2] != ':')) return false;
        parsed[index] = (uint8_t)((high << 4) | low);
        nonzero |= parsed[index];
    }
    if (nonzero == 0 || (parsed[0] & 1U) != 0) return false;
    memcpy(address, parsed, sizeof(parsed));
    return true;
}

void lab_wifi_ssid_display(const uint8_t ssid[33], char text[LAB_WIFI_SSID_DISPLAY_SIZE])
{
    static const char hex[] = "0123456789abcdef";
    size_t out = 0;
    for (size_t index = 0; index < 32 && ssid[index] != 0; ++index) {
        uint8_t ch = ssid[index];
        if (ch >= 0x20 && ch < 0x7f && ch != '\\' && ch != '"') {
            text[out++] = (char)ch;
        } else {
            text[out++] = '\\';
            text[out++] = 'x';
            text[out++] = hex[ch >> 4];
            text[out++] = hex[ch & 15];
        }
    }
    text[out] = '\0';
}

const char *lab_wifi_auth_name(wifi_auth_mode_t mode)
{
    switch (mode) {
    case WIFI_AUTH_OPEN: return "open";
    case WIFI_AUTH_WEP: return "wep";
    case WIFI_AUTH_WPA_PSK: return "wpa";
    case WIFI_AUTH_WPA2_PSK: return "wpa2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "wpa/wpa2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "wpa2-ent";
    case WIFI_AUTH_WPA3_PSK: return "wpa3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "wpa2/wpa3";
    case WIFI_AUTH_WAPI_PSK: return "wapi";
    case WIFI_AUTH_OWE: return "owe";
    case WIFI_AUTH_WPA3_ENT_192: return "wpa3-ent-192";
    case WIFI_AUTH_WPA3_EXT_PSK: return "wpa3-ext";
    case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE: return "wpa3-ext-mixed";
    case WIFI_AUTH_DPP: return "dpp";
    case WIFI_AUTH_WPA3_ENTERPRISE: return "wpa3-ent";
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE: return "wpa2/wpa3-ent";
    default: return "unknown";
    }
}

const char *lab_wifi_cipher_name(wifi_cipher_type_t cipher)
{
    switch (cipher) {
    case WIFI_CIPHER_TYPE_NONE: return "none";
    case WIFI_CIPHER_TYPE_WEP40: return "wep40";
    case WIFI_CIPHER_TYPE_WEP104: return "wep104";
    case WIFI_CIPHER_TYPE_TKIP: return "tkip";
    case WIFI_CIPHER_TYPE_CCMP: return "ccmp";
    case WIFI_CIPHER_TYPE_TKIP_CCMP: return "tkip/ccmp";
    case WIFI_CIPHER_TYPE_AES_CMAC128: return "aes-cmac128";
    case WIFI_CIPHER_TYPE_SMS4: return "sms4";
    case WIFI_CIPHER_TYPE_GCMP: return "gcmp";
    case WIFI_CIPHER_TYPE_GCMP256: return "gcmp256";
    default: return "unknown";
    }
}

const char *lab_wifi_auth_warning(wifi_auth_mode_t mode)
{
    switch (mode) {
    case WIFI_AUTH_OPEN: return "Open network: no Wi-Fi encryption or authentication";
    case WIFI_AUTH_WEP: return "Obsolete WEP: use WPA2-AES or WPA3";
    case WIFI_AUTH_WPA_PSK: return "Obsolete WPA: use WPA2-AES or WPA3";
    case WIFI_AUTH_WPA_WPA2_PSK: return "Mixed WPA/WPA2: disable legacy WPA compatibility";
    default: return NULL;
    }
}

bool lab_wifi_legacy_cipher(wifi_cipher_type_t cipher)
{
    return cipher == WIFI_CIPHER_TYPE_WEP40 || cipher == WIFI_CIPHER_TYPE_WEP104 ||
           cipher == WIFI_CIPHER_TYPE_TKIP || cipher == WIFI_CIPHER_TYPE_TKIP_CCMP;
}

static bool same_ssid(const wifi_ap_record_t *left, const wifi_ap_record_t *right)
{
    return strncmp((const char *)left->ssid, (const char *)right->ssid, 32) == 0;
}

static bool same_security(const wifi_ap_record_t *left, const wifi_ap_record_t *right)
{
    return left->authmode == right->authmode && left->pairwise_cipher == right->pairwise_cipher &&
           left->group_cipher == right->group_cipher;
}

lab_wifi_trust_result_t lab_wifi_trust_add(lab_wifi_trust_store_t *store, const wifi_ap_record_t *ap)
{
    if (ap->ssid[0] == 0) return LAB_WIFI_TRUST_HIDDEN;
    for (size_t index = 0; index < store->count; ++index) {
        const wifi_ap_record_t *known = &store->aps[index];
        if (memcmp(known->bssid, ap->bssid, 6) == 0) {
            return same_ssid(known, ap) && same_security(known, ap) ?
                   LAB_WIFI_TRUST_EXISTS : LAB_WIFI_TRUST_CHANGED;
        }
    }
    if (store->count >= LAB_WIFI_TRUST_LIMIT) return LAB_WIFI_TRUST_FULL;
    store->aps[store->count++] = *ap;
    return LAB_WIFI_TRUST_ADDED;
}

unsigned lab_wifi_suspect_flags(const lab_wifi_trust_store_t *store, const wifi_ap_record_t *ap)
{
    bool known_ssid = false;
    for (size_t index = 0; index < store->count; ++index) {
        const wifi_ap_record_t *known = &store->aps[index];
        if (memcmp(known->bssid, ap->bssid, 6) == 0) {
            return (same_ssid(known, ap) ? 0U : LAB_WIFI_SSID_CHANGED) |
                   (same_security(known, ap) ? 0U : LAB_WIFI_SECURITY_CHANGED);
        }
        known_ssid |= ap->ssid[0] != 0 && same_ssid(known, ap);
    }
    return known_ssid ? LAB_WIFI_UNKNOWN_BSSID : 0U;
}