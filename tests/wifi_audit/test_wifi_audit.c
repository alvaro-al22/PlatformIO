/* Host-only tests; links the real wifi_audit.c, never any radio/serial code. */
#include "wifi_audit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(expr) do { \
    ++checks; \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: FAIL: %s\n", __FILE__, __LINE__, #expr); \
        exit(EXIT_FAILURE); \
    } \
} while (0)
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define TEXT(actual, expected) CHECK(strcmp((actual), (expected)) == 0)

static wifi_ap_record_t ap_record(uint8_t id, const char *ssid)
{
    wifi_ap_record_t ap = {0};
    ap.bssid[0] = 2;
    ap.bssid[5] = id;
    size_t len = strlen(ssid);
    CHECK(len <= 32);
    memcpy(ap.ssid, ssid, len);
    ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap.pairwise_cipher = ap.group_cipher = WIFI_CIPHER_TYPE_CCMP;
    ap.primary = 6;
    ap.rssi = -42;
    return ap;
}

static void reject_mac(const char *text)
{
    uint8_t address[6], before[6];
    memset(address, 0xa5, sizeof(address));
    memcpy(before, address, sizeof(before));
    CHECK(!lab_wifi_parse_bssid(text, address));
    CHECK(memcmp(address, before, sizeof(address)) == 0);
}

static void mac_strict(void)
{
    uint8_t address[6];
    const uint8_t expected[] = {0xa0, 0xb1, 0xc2, 0xd3, 0xe4, 0xf5};
    CHECK(lab_wifi_parse_bssid("a0:B1:c2:D3:e4:F5", address));
    CHECK(memcmp(address, expected, 6) == 0);
    CHECK(lab_wifi_parse_bssid("02:00:00:00:00:01", address));
    CHECK(address[0] == 2 && address[5] == 1); /* Local unicast. */
    CHECK(lab_wifi_parse_bssid("00:00:00:00:00:01", address));
    CHECK(lab_wifi_parse_bssid("FE:FF:FF:FF:FF:FF", address));
    CHECK(!lab_wifi_parse_bssid("02:00:00:00:00:01", NULL));
    const char *bad[] = {NULL, "", "00:00:00:00:00:00", "ff:ff:ff:ff:ff:ff",
        "01:00:5e:00:00:01", "03:00:00:00:00:01", "0:00:00:00:00:01",
        "002:00:00:00:00:01", "020000000001", "02-00-00-00-00-01",
        " 02:00:00:00:00:01", "02:00:00:00:00:01 ", "02:00:00:00:00:01\n",
        "02:00:00:00:00:01x", "02:00:00:00:00", "02:00:00:00:00:01:02"};
    for (size_t i = 0; i < COUNT(bad); ++i) reject_mac(bad[i]);
    for (size_t i = 0; i < 17; ++i) {
        char text[] = "02:ab:CD:ef:12:34";
        text[i] = i % 3 == 2 ? '-' : 'g';
        reject_mac(text);
    }
}

static void auth_modes(void)
{
    static const char *names[] = {"open", "wep", "wpa", "wpa2", "wpa/wpa2",
        "wpa2-ent", "wpa3", "wpa2/wpa3", "wapi", "owe", "wpa3-ent-192",
        "wpa3-ext", "wpa3-ext-mixed", "dpp", "wpa3-ent", "wpa2/wpa3-ent"};
    static const char *warnings[WIFI_AUTH_MAX] = {
        [WIFI_AUTH_OPEN] = "Open network: no Wi-Fi encryption or authentication",
        [WIFI_AUTH_WEP] = "Obsolete WEP: use WPA2-AES or WPA3",
        [WIFI_AUTH_WPA_PSK] = "Obsolete WPA: use WPA2-AES or WPA3",
        [WIFI_AUTH_WPA_WPA2_PSK] = "Mixed WPA/WPA2: disable legacy WPA compatibility"
    };
    CHECK(COUNT(names) == WIFI_AUTH_MAX);
    for (int i = 0; i < WIFI_AUTH_MAX; ++i) {
        TEXT(lab_wifi_auth_name((wifi_auth_mode_t)i), names[i]);
        const char *warning = lab_wifi_auth_warning((wifi_auth_mode_t)i);
        CHECK((warning == NULL) == (warnings[i] == NULL));
        if (warnings[i]) TEXT(warning, warnings[i]);
    }
    TEXT(lab_wifi_auth_name(WIFI_AUTH_ENTERPRISE), "wpa2-ent");
    const int invalid[] = {-1, WIFI_AUTH_MAX, 255};
    for (size_t i = 0; i < COUNT(invalid); ++i) {
        TEXT(lab_wifi_auth_name((wifi_auth_mode_t)invalid[i]), "unknown");
        CHECK(lab_wifi_auth_warning((wifi_auth_mode_t)invalid[i]) == NULL);
    }
}

static void cipher_modes(void)
{
    /* GMAC values exist in the SDK but currently use the name fallback. */
    static const char *names[] = {"none", "wep40", "wep104", "tkip", "ccmp",
        "tkip/ccmp", "aes-cmac128", "sms4", "gcmp", "gcmp256",
        "unknown", "unknown", "unknown"};
    static const bool legacy[] = {false, true, true, true, false, true,
        false, false, false, false, false, false, false};
    CHECK(COUNT(names) == WIFI_CIPHER_TYPE_UNKNOWN + 1);
    for (size_t i = 0; i < COUNT(names); ++i) {
        TEXT(lab_wifi_cipher_name((wifi_cipher_type_t)i), names[i]);
        CHECK(lab_wifi_legacy_cipher((wifi_cipher_type_t)i) == legacy[i]);
    }
    TEXT(lab_wifi_cipher_name((wifi_cipher_type_t)-1), "unknown");
    TEXT(lab_wifi_cipher_name((wifi_cipher_type_t)255), "unknown");
    CHECK(!lab_wifi_legacy_cipher((wifi_cipher_type_t)-1));
    CHECK(!lab_wifi_legacy_cipher((wifi_cipher_type_t)255));
}

static void display_check(const uint8_t ssid[33], const char *expected)
{
    struct { unsigned char before[8]; char text[LAB_WIFI_SSID_DISPLAY_SIZE];
             unsigned char after[8]; } output;
    uint8_t input[33];
    memcpy(input, ssid, sizeof(input));
    memset(&output, 0xa5, sizeof(output));
    lab_wifi_ssid_display(ssid, output.text);
    CHECK(memchr(output.text, 0, sizeof(output.text)) != NULL);
    TEXT(output.text, expected);
    for (size_t i = 0; i < 8; ++i) {
        CHECK(output.before[i] == 0xa5);
        CHECK(output.after[i] == 0xa5);
    }
    CHECK(memcmp(input, ssid, sizeof(input)) == 0);
}

static void ssid_escaping(void)
{
    const uint8_t mixed[33] = {'A', ' ', '"', '\\', '\n', 0x7f, 0x80, 0xff, '~'};
    display_check(mixed, "A \\x22\\x5c\\x0a\\x7f\\x80\\xff~");
    const uint8_t hidden[33] = {0, 'x'};
    display_check(hidden, "");
    const uint8_t terminated[33] = {'A', 0, 0xff};
    display_check(terminated, "A");
    for (unsigned ch = 1; ch <= 255; ++ch) {
        uint8_t ssid[33] = {(uint8_t)ch};
        char expected[5] = {0};
        if (ch >= 32 && ch < 127 && ch != '"' && ch != '\\') expected[0] = (char)ch;
        else snprintf(expected, sizeof(expected), "\\x%02x", ch);
        display_check(ssid, expected);
    }
    for (size_t len = 0; len <= 32; ++len) {
        uint8_t ssid[33];
        char expected[129] = {0};
        memset(ssid, 0xff, sizeof(ssid));
        if (len < 32) ssid[len] = 0; /* At 32, no NUL anywhere in input. */
        for (size_t i = 0; i < len; ++i) memcpy(expected + 4 * i, "\\xff", 4);
        display_check(ssid, expected);
    }
    uint8_t full[33];
    memset(full, 'a', sizeof(full));
    display_check(full, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
}

static void trust_basics(void)
{
    lab_wifi_trust_store_t store = {0}, before;
    wifi_ap_record_t hidden = ap_record(1, "");
    memcpy(&before, &store, sizeof(store));
    CHECK(lab_wifi_trust_add(&store, &hidden) == LAB_WIFI_TRUST_HIDDEN);
    CHECK(memcmp(&before, &store, sizeof(store)) == 0);
    wifi_ap_record_t ap = ap_record(1, "home");
    CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_ADDED);
    CHECK(store.count == 1);
    CHECK(memcmp(store.aps[0].ssid, ap.ssid, 33) == 0);
    memcpy(&before, &store, sizeof(store));
    CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_EXISTS);
    ap.rssi = -90;
    ap.primary = 11;
    CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_EXISTS);
    CHECK(lab_wifi_suspect_flags(&store, &ap) == 0);
    CHECK(memcmp(&before, &store, sizeof(store)) == 0);
    CHECK(lab_wifi_trust_add(&store, &hidden) == LAB_WIFI_TRUST_HIDDEN);
    CHECK(memcmp(&before, &store, sizeof(store)) == 0);
}

static void trust_capacity(void)
{
    struct { lab_wifi_trust_store_t store; unsigned char guard[16]; } data = {0};
    memset(data.guard, 0xa5, sizeof(data.guard));
    CHECK(LAB_WIFI_TRUST_LIMIT == 16);
    for (size_t i = 0; i < LAB_WIFI_TRUST_LIMIT; ++i) {
        wifi_ap_record_t ap = ap_record((uint8_t)i, "mesh");
        CHECK(lab_wifi_trust_add(&data.store, &ap) == LAB_WIFI_TRUST_ADDED);
        CHECK(data.store.count == i + 1);
    }
    lab_wifi_trust_store_t before;
    memcpy(&before, &data.store, sizeof(before));
    wifi_ap_record_t ap = ap_record(200, "new");
    CHECK(lab_wifi_trust_add(&data.store, &ap) == LAB_WIFI_TRUST_FULL);
    ap = ap_record(15, "mesh");
    CHECK(lab_wifi_trust_add(&data.store, &ap) == LAB_WIFI_TRUST_EXISTS);
    ap.authmode = WIFI_AUTH_OPEN;
    CHECK(lab_wifi_trust_add(&data.store, &ap) == LAB_WIFI_TRUST_CHANGED);
    ap = ap_record(200, "");
    CHECK(lab_wifi_trust_add(&data.store, &ap) == LAB_WIFI_TRUST_HIDDEN);
    CHECK(memcmp(&before, &data.store, sizeof(before)) == 0);
    for (size_t i = 0; i < sizeof(data.guard); ++i) CHECK(data.guard[i] == 0xa5);
}

static void known_changes_refuse_overwrite(void)
{
    lab_wifi_trust_store_t store = {0}, before;
    wifi_ap_record_t original = ap_record(1, "home");
    CHECK(lab_wifi_trust_add(&store, &original) == LAB_WIFI_TRUST_ADDED);
    memcpy(&before, &store, sizeof(before));
    /* All combinations of SSID/auth/pairwise/group changes independently. */
    for (unsigned mask = 0; mask < 16; ++mask) {
        wifi_ap_record_t ap = original;
        if (mask & 1) ap.ssid[0] = 'H';
        if (mask & 2) ap.authmode = WIFI_AUTH_OPEN;
        if (mask & 4) ap.pairwise_cipher = WIFI_CIPHER_TYPE_TKIP;
        if (mask & 8) ap.group_cipher = WIFI_CIPHER_TYPE_TKIP;
        unsigned expected = (mask & 1 ? LAB_WIFI_SSID_CHANGED : 0U) |
                            (mask & 14 ? LAB_WIFI_SECURITY_CHANGED : 0U);
        CHECK(lab_wifi_suspect_flags(&store, &ap) == expected);
        CHECK(lab_wifi_trust_add(&store, &ap) ==
              (mask ? LAB_WIFI_TRUST_CHANGED : LAB_WIFI_TRUST_EXISTS));
        CHECK(memcmp(&before, &store, sizeof(store)) == 0);
        CHECK(lab_wifi_suspect_flags(&store, &original) == 0);
    }
    wifi_ap_record_t hidden = original;
    hidden.ssid[0] = 0;
    CHECK(lab_wifi_suspect_flags(&store, &hidden) == LAB_WIFI_SSID_CHANGED);
    hidden.authmode = WIFI_AUTH_OPEN;
    CHECK(lab_wifi_suspect_flags(&store, &hidden) ==
          (LAB_WIFI_SSID_CHANGED | LAB_WIFI_SECURITY_CHANGED));
    CHECK(lab_wifi_trust_add(&store, &hidden) == LAB_WIFI_TRUST_HIDDEN);
    CHECK(memcmp(&before, &store, sizeof(store)) == 0);
}

static void unknown_bssid(void)
{
    lab_wifi_trust_store_t store = {0}, before;
    wifi_ap_record_t ap = ap_record(1, "home");
    CHECK(lab_wifi_suspect_flags(&store, &ap) == 0);
    CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_ADDED);
    memcpy(&before, &store, sizeof(before));
    ap.bssid[5] = 2;
    CHECK(lab_wifi_suspect_flags(&store, &ap) == LAB_WIFI_UNKNOWN_BSSID);
    ap.authmode = WIFI_AUTH_OPEN;
    CHECK(lab_wifi_suspect_flags(&store, &ap) == LAB_WIFI_UNKNOWN_BSSID);
    ap = ap_record(3, "unrelated");
    CHECK(lab_wifi_suspect_flags(&store, &ap) == 0);
    ap = ap_record(3, "Home");
    CHECK(lab_wifi_suspect_flags(&store, &ap) == 0);
    ap.ssid[0] = 0;
    CHECK(lab_wifi_suspect_flags(&store, &ap) == 0);
    CHECK(memcmp(&before, &store, sizeof(store)) == 0);
}

static void trusted_mesh(void)
{
    lab_wifi_trust_store_t store = {0};
    for (unsigned i = 0; i < 4; ++i) {
        wifi_ap_record_t ap = ap_record((uint8_t)i, "mesh");
        if (i & 1) ap.authmode = WIFI_AUTH_WPA3_PSK;
        CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_ADDED);
    }
    CHECK(store.count == 4);
    for (size_t i = 0; i < store.count; ++i) {
        wifi_ap_record_t ap = store.aps[i];
        ap.rssi = -85;
        ap.primary = 1;
        CHECK(lab_wifi_suspect_flags(&store, &ap) == 0);
        CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_EXISTS);
    }
    wifi_ap_record_t unknown = ap_record(99, "mesh");
    CHECK(lab_wifi_suspect_flags(&store, &unknown) == LAB_WIFI_UNKNOWN_BSSID);
}

static void raw_ssids(void)
{
    lab_wifi_trust_store_t store = {0};
    wifi_ap_record_t ap = ap_record(1, "raw");
    ap.ssid[3] = 0x80;
    CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_ADDED);
    ap.bssid[5] = 2;
    CHECK(lab_wifi_suspect_flags(&store, &ap) == LAB_WIFI_UNKNOWN_BSSID);
    ap.ssid[3] = 0x81; /* Both would collapse to the same printable placeholder. */
    CHECK(lab_wifi_suspect_flags(&store, &ap) == 0);
    CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_ADDED);
    CHECK(store.count == 2);
    CHECK(lab_wifi_suspect_flags(&store, &ap) == 0);
    ap.bssid[5] = 1;
    CHECK(lab_wifi_suspect_flags(&store, &ap) == LAB_WIFI_SSID_CHANGED);
    CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_CHANGED);
    ap = ap_record(3, "raw\\x80"); /* Literal escaped text is not raw bytes. */
    CHECK(lab_wifi_suspect_flags(&store, &ap) == 0);
}

static void ssids_32_bytes(void)
{
    lab_wifi_trust_store_t store = {0};
    wifi_ap_record_t ap = ap_record(1, "012345678901234567890123456789AB");
    CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_ADDED);
    ap.ssid[32] = 0xff; /* Byte 33 is never part of the SSID. */
    CHECK(lab_wifi_suspect_flags(&store, &ap) == 0);
    CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_EXISTS);
    display_check(ap.ssid, "012345678901234567890123456789AB");
    ap.bssid[5] = 2;
    CHECK(lab_wifi_suspect_flags(&store, &ap) == LAB_WIFI_UNKNOWN_BSSID);
    ap.ssid[31] = 'C';
    CHECK(lab_wifi_suspect_flags(&store, &ap) == 0);
    CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_ADDED);
    ap.bssid[5] = 1;
    CHECK(lab_wifi_suspect_flags(&store, &ap) == LAB_WIFI_SSID_CHANGED);
    CHECK(lab_wifi_trust_add(&store, &ap) == LAB_WIFI_TRUST_CHANGED);
    ap = store.aps[0];
    ap.bssid[5] = 3;
    ap.ssid[31] = 0; /* A 31-byte prefix must not match the 32-byte SSID. */
    CHECK(lab_wifi_suspect_flags(&store, &ap) == 0);
}

static void pmf_announcements(void)
{
    uint8_t frame[128] = {0x80};
    const uint8_t elements[] = {
        0, 3, 'l', 'a', 'b',
        48, 20, 1, 0, 0, 15, 172, 4,
        1, 0, 0, 15, 172, 4, 1, 0, 0, 15, 172, 2, 0x80, 0
    };
    memcpy(frame + 36, elements, sizeof(elements));
    size_t length = 36 + sizeof(elements);
    CHECK(lab_wifi_pmf_from_frame(NULL, length) == LAB_WIFI_PMF_UNKNOWN);
    CHECK(lab_wifi_pmf_from_frame(frame, length) == LAB_WIFI_PMF_OPTIONAL);
    frame[length - 2] = 0xc0;
    CHECK(lab_wifi_pmf_from_frame(frame, length) == LAB_WIFI_PMF_REQUIRED);
    frame[0] = 0x50;
    CHECK(lab_wifi_pmf_from_frame(frame, length) == LAB_WIFI_PMF_REQUIRED);
    frame[length - 2] = 0;
    CHECK(lab_wifi_pmf_from_frame(frame, length) == LAB_WIFI_PMF_UNSUPPORTED);
    frame[length - 2] = 0x40;
    CHECK(lab_wifi_pmf_from_frame(frame, length) == LAB_WIFI_PMF_UNKNOWN);
    frame[length - 2] = 0x80;
    for (size_t size = 0; size < length; ++size) {
        CHECK(lab_wifi_pmf_from_frame(frame, size) ==
              (size == 41 ? LAB_WIFI_PMF_UNSUPPORTED : LAB_WIFI_PMF_UNKNOWN));
    }
    for (unsigned flags = 0; flags <= 255; ++flags) {
        frame[length - 2] = (uint8_t)flags;
        lab_wifi_pmf_t expected = flags & 0x80 ?
            (flags & 0x40 ? LAB_WIFI_PMF_REQUIRED : LAB_WIFI_PMF_OPTIONAL) :
            (flags & 0x40 ? LAB_WIFI_PMF_UNKNOWN : LAB_WIFI_PMF_UNSUPPORTED);
        CHECK(lab_wifi_pmf_from_frame(frame, length) == expected);
    }
    frame[length - 2] = 0x80;
    frame[42] = 18;
    CHECK(lab_wifi_pmf_from_frame(frame, length - 2) == LAB_WIFI_PMF_UNSUPPORTED);
    frame[42] = 19;
    CHECK(lab_wifi_pmf_from_frame(frame, length - 1) == LAB_WIFI_PMF_UNKNOWN);
    frame[42] = 20;
    const size_t corrupt_offsets[] = {0, 1, 22, 37, 42, 43, 49, 50, 55, 56};
    for (size_t index = 0; index < COUNT(corrupt_offsets); ++index) {
        size_t offset = corrupt_offsets[index];
        uint8_t saved = frame[offset];
        frame[offset] = 0xff;
        CHECK(lab_wifi_pmf_from_frame(frame, length) == LAB_WIFI_PMF_UNKNOWN);
        frame[offset] = saved;
    }
    frame[length] = 48;
    frame[length + 1] = 0;
    CHECK(lab_wifi_pmf_from_frame(frame, length + 2) == LAB_WIFI_PMF_UNKNOWN);
    frame[length] = 0;
    CHECK(lab_wifi_pmf_from_frame(frame, length + 2) == LAB_WIFI_PMF_UNKNOWN);
    frame[length] = 221;
    CHECK(lab_wifi_pmf_from_frame(frame, length + 2) == LAB_WIFI_PMF_OPTIONAL);
    frame[length] = 0;
    frame[42] = 22;
    CHECK(lab_wifi_pmf_from_frame(frame, length + 2) == LAB_WIFI_PMF_OPTIONAL);
    frame[42] = 26;
    CHECK(lab_wifi_pmf_from_frame(frame, length + 6) == LAB_WIFI_PMF_OPTIONAL);
    frame[length] = 1;
    CHECK(lab_wifi_pmf_from_frame(frame, length + 6) == LAB_WIFI_PMF_UNKNOWN);
    frame[42] = 42;
    CHECK(lab_wifi_pmf_from_frame(frame, length + 22) == LAB_WIFI_PMF_OPTIONAL);
    frame[1] = 4;
    CHECK(lab_wifi_pmf_from_frame(frame, length + 22) == LAB_WIFI_PMF_UNKNOWN);
    frame[1] = 0;
    frame[22] = 1;
    CHECK(lab_wifi_pmf_from_frame(frame, length + 22) == LAB_WIFI_PMF_UNKNOWN);
    frame[22] = 0;
    const uint8_t multiple_suites[] = {
        0, 0, 48, 28, 1, 0, 0, 15, 172, 4,
        2, 0, 0, 15, 172, 4, 0, 15, 172, 8,
        2, 0, 0, 15, 172, 2, 0, 15, 172, 8, 0xc0, 0
    };
    memcpy(frame + 36, multiple_suites, sizeof(multiple_suites));
    CHECK(lab_wifi_pmf_from_frame(frame, 36 + sizeof(multiple_suites)) == LAB_WIFI_PMF_REQUIRED);
    CHECK(lab_wifi_pmf_from_frame(frame, 38) == LAB_WIFI_PMF_UNSUPPORTED);
    frame[36] = 221;
    CHECK(lab_wifi_pmf_from_frame(frame, 36 + sizeof(multiple_suites)) == LAB_WIFI_PMF_UNKNOWN);
    TEXT(lab_wifi_pmf_name(LAB_WIFI_PMF_UNKNOWN), "unknown");
    TEXT(lab_wifi_pmf_name(LAB_WIFI_PMF_UNSUPPORTED), "unsupported");
    TEXT(lab_wifi_pmf_name(LAB_WIFI_PMF_OPTIONAL), "optional");
    TEXT(lab_wifi_pmf_name(LAB_WIFI_PMF_REQUIRED), "required");
    TEXT(lab_wifi_pmf_name((lab_wifi_pmf_t)255), "unknown");
}

static void pmf_target_filter(void)
{
    uint8_t frame[64] = {0x80};
    const uint8_t bssid[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    memcpy(frame + 10, bssid, sizeof(bssid));
    memcpy(frame + 16, bssid, sizeof(bssid));
    CHECK(!lab_wifi_pmf_frame_matches(NULL, sizeof(frame), bssid));
    CHECK(!lab_wifi_pmf_frame_matches(frame, sizeof(frame), NULL));
    for (size_t length = 0; length < 36; ++length) {
        CHECK(!lab_wifi_pmf_frame_matches(frame, length, bssid));
    }
    CHECK(lab_wifi_pmf_frame_matches(frame, 36, bssid));
    CHECK(lab_wifi_pmf_frame_matches(frame, sizeof(frame), bssid));
    frame[0] = 0x50;
    CHECK(lab_wifi_pmf_frame_matches(frame, sizeof(frame), bssid));
    const size_t mismatches[] = {0, 1, 10, 15, 16, 21, 22};
    for (size_t index = 0; index < COUNT(mismatches); ++index) {
        size_t offset = mismatches[index];
        uint8_t saved = frame[offset];
        frame[offset] ^= 1U;
        CHECK(!lab_wifi_pmf_frame_matches(frame, sizeof(frame), bssid));
        frame[offset] = saved;
    }
    uint8_t multicast[6] = {0x03, 0x11, 0x22, 0x33, 0x44, 0x55};
    memcpy(frame + 10, multicast, sizeof(multicast));
    memcpy(frame + 16, multicast, sizeof(multicast));
    CHECK(!lab_wifi_pmf_frame_matches(frame, sizeof(frame), multicast));
}

int main(void)
{
    const struct { const char *name; void (*run)(void); } tests[] = {
        {"mac_strict", mac_strict}, {"auth_modes", auth_modes},
        {"cipher_modes", cipher_modes}, {"ssid_escaping", ssid_escaping},
        {"trust_basics", trust_basics}, {"trust_capacity", trust_capacity},
        {"known_changes_refuse_overwrite", known_changes_refuse_overwrite},
        {"unknown_bssid", unknown_bssid}, {"trusted_mesh", trusted_mesh},
        {"raw_ssids", raw_ssids}, {"ssids_32_bytes", ssids_32_bytes},
        {"pmf_announcements", pmf_announcements}, {"pmf_target_filter", pmf_target_filter}
    };
    for (size_t i = 0; i < COUNT(tests); ++i) {
        unsigned start = checks;
        tests[i].run();
        printf("PASS %-32s %u checks\n", tests[i].name, checks - start);
    }
    printf("PASS: %zu test groups, %u checks\n", COUNT(tests), checks);
    return EXIT_SUCCESS;
}
