#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "core/module.h"
#include "core/command_args.h"
#include "nvs.h"
#include "wifi_audit.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_DISCONNECTED_BIT BIT1
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_MAX_SCAN_RESULTS 20
#define WIFI_PMF_TIMEOUT_MS 5000
#define WIFI_PMF_FRAME_CAPACITY 1536
#define WIFI_TRUST_SCAN_MAX_AGE_US (120LL * 1000000LL)

static const char *TAG = "wifi";
static EventGroupHandle_t events;
static esp_netif_t *netif;
static bool started;
static wifi_ap_record_t scan_records[WIFI_MAX_SCAN_RESULTS];
static uint16_t scan_count;
static bool scan_valid;
static int64_t scan_time_us;
static lab_wifi_trust_store_t trusted;

static struct {
    uint8_t bssid[6];
    uint8_t channel;
    size_t length;
    bool oversized;
    uint8_t frame[WIFI_PMF_FRAME_CAPACITY];
} pmf_sample;
static bool pmf_collecting;
static bool pmf_radio_dirty;
static portMUX_TYPE pmf_lock = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR on_pmf_frame(void *buffer, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT || buffer == NULL) return;
    const wifi_promiscuous_pkt_t *packet = buffer;
    if (packet->rx_ctrl.rx_state != 0 || packet->rx_ctrl.sig_len < 40) return;
    size_t length = packet->rx_ctrl.sig_len - 4;
    portENTER_CRITICAL(&pmf_lock);
    if (pmf_collecting && pmf_sample.length == 0 &&
        packet->rx_ctrl.channel == pmf_sample.channel &&
        lab_wifi_pmf_frame_matches(packet->payload, length, pmf_sample.bssid)) {
        if (length <= sizeof(pmf_sample.frame)) {
            memcpy(pmf_sample.frame, packet->payload, length);
            pmf_sample.length = length;
        } else {
            pmf_sample.oversized = true;
        }
    }
    portEXIT_CRITICAL(&pmf_lock);
}

static esp_err_t pmf_capture_start(const wifi_ap_record_t *ap)
{
    portENTER_CRITICAL(&pmf_lock);
    memcpy(pmf_sample.bssid, ap->bssid, sizeof(pmf_sample.bssid));
    pmf_sample.channel = ap->primary;
    pmf_sample.length = 0;
    pmf_sample.oversized = false;
    pmf_collecting = false;
    portEXIT_CRITICAL(&pmf_lock);
    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    esp_err_t error = esp_wifi_set_promiscuous_filter(&filter);
    if (error == ESP_OK) error = esp_wifi_set_promiscuous_rx_cb(on_pmf_frame);
    if (error == ESP_OK) {
        portENTER_CRITICAL(&pmf_lock);
        pmf_collecting = true;
        portEXIT_CRITICAL(&pmf_lock);
        error = esp_wifi_set_promiscuous(true);
    }
    return error;
}

static esp_err_t pmf_capture_stop(void)
{
    portENTER_CRITICAL(&pmf_lock);
    pmf_collecting = false;
    portEXIT_CRITICAL(&pmf_lock);
    esp_err_t error = esp_wifi_set_promiscuous(false);
    esp_err_t callback_error = esp_wifi_set_promiscuous_rx_cb(NULL);
    return error != ESP_OK ? error : callback_error;
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event = data;
        ESP_LOGW(TAG, "Disconnected, reason %d", event->reason);
        xEventGroupClearBits(events, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(events, WIFI_DISCONNECTED_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(events, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_start(void)
{
    if (started) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(lab_nvs_init(), TAG, "NVS init failed");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    esp_err_t error = esp_event_loop_create_default();
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
        return error;
    }
    if (events == NULL) {
        events = xEventGroupCreate();
        ESP_RETURN_ON_FALSE(events != NULL, ESP_ERR_NO_MEM, TAG, "no memory");
    }
    if (netif == NULL) {
        netif = esp_netif_create_default_wifi_sta();
        ESP_RETURN_ON_FALSE(netif != NULL, ESP_FAIL, TAG, "netif create failed");
    }
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&config), TAG, "wifi init failed");
    // Credentials live only in RAM; they are lost on reset.
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "set storage failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                                   on_event, NULL), TAG, "handler failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL),
                        TAG, "handler failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start failed");
    started = true;
    return ESP_OK;
}

static void print_ap(const wifi_ap_record_t *ap, const char *pmf)
{
    char ssid[LAB_WIFI_SSID_DISPLAY_SIZE];
    lab_wifi_ssid_display(ap->ssid, ssid);
    printf(MACSTR " ssid=\"%s\" rssi=%d dBm ch=%u auth=%s pairwise=%s group=%s%s%s%s\n",
           MAC2STR(ap->bssid), ssid, ap->rssi, ap->primary,
           lab_wifi_auth_name(ap->authmode), lab_wifi_cipher_name(ap->pairwise_cipher),
           lab_wifi_cipher_name(ap->group_cipher), pmf ? " pmf_advertised=" : "",
           pmf ? pmf : "", ap->ssid[0] == 0 ? " (hidden/empty SSID)" : "");
}

static esp_err_t refresh_scan(bool passive, uint8_t channel)
{
    scan_valid = false;
    scan_count = 0;
    wifi_scan_config_t config = {.show_hidden = true, .channel = channel,
        .scan_type = passive ? WIFI_SCAN_TYPE_PASSIVE : WIFI_SCAN_TYPE_ACTIVE};
    if (passive) config.scan_time.passive = 120;
    esp_err_t error = esp_wifi_scan_start(&config, true);
    if (error != ESP_OK) {
        printf("ERR wifi scan: %s\n", esp_err_to_name(error));
        return error;
    }
    uint16_t total = 0;
    error = esp_wifi_scan_get_ap_num(&total);
    if (error == ESP_OK && total != 0) {
        scan_count = total < WIFI_MAX_SCAN_RESULTS ? total : WIFI_MAX_SCAN_RESULTS;
        error = esp_wifi_scan_get_ap_records(&scan_count, scan_records);
    }
    if (error != ESP_OK) {
        esp_wifi_clear_ap_list();
        scan_count = 0;
        printf("ERR wifi scan results: %s\n", esp_err_to_name(error));
        return error;
    }
    if (total == 0) {
        error = esp_wifi_clear_ap_list();
        if (error != ESP_OK) {
            printf("ERR wifi scan cleanup: %s\n", esp_err_to_name(error));
            return error;
        }
    }
    scan_time_us = esp_timer_get_time();
    scan_valid = true;
    printf("OK scan: %u APs found, %u retained; 2.4 GHz only\n", total, scan_count);
    printf("NOTE scan mode=%s channel=%u (0=all allowed channels)\n", passive ? "passive" : "active", channel);
    if (total > scan_count) {
        puts("WARN truncated scan: omitted APs are NOT assessed; absence is not proof of safety");
    }
    return ESP_OK;
}

static int wifi_pmf(const char *argument)
{
    uint8_t bssid[6];
    if (!lab_wifi_parse_bssid(argument, bssid)) {
        puts("ERR USAGE: wifi pmf <BSSID xx:xx:xx:xx:xx:xx> (unicast, nonzero)");
        return ESP_ERR_INVALID_ARG;
    }
    if (!scan_valid || esp_timer_get_time() - scan_time_us > WIFI_TRUST_SCAN_MAX_AGE_US) {
        puts("ERR run wifi scan first; PMF requires a successful scan less than 120 seconds old");
        return ESP_ERR_INVALID_STATE;
    }
    const wifi_ap_record_t *target = NULL;
    for (uint16_t index = 0; index < scan_count; ++index) {
        if (memcmp(scan_records[index].bssid, bssid, sizeof(bssid)) == 0) {
            target = &scan_records[index];
            break;
        }
    }
    if (target == NULL) {
        puts("ERR BSSID not in retained scan results; run wifi scan and check the address");
        return ESP_ERR_NOT_FOUND;
    }
    if (target->primary < 1 || target->primary > 14) {
        puts("ERR PMF requires a valid 2.4 GHz channel; run wifi scan again");
        return ESP_ERR_INVALID_STATE;
    }
    wifi_ap_record_t connected_ap;
    esp_err_t error = esp_wifi_sta_get_ap_info(&connected_ap);
    if (error == ESP_OK) {
        puts("ERR PMF requires a disconnected station; use wifi disconnect first");
        return ESP_ERR_INVALID_STATE;
    }
    if (error != ESP_ERR_WIFI_NOT_CONNECT) return error;
    bool promiscuous = false;
    ESP_RETURN_ON_ERROR(esp_wifi_get_promiscuous(&promiscuous), TAG, "read capture state failed");
    ESP_RETURN_ON_FALSE(!promiscuous, ESP_ERR_INVALID_STATE, TAG, "capture already active");
    uint8_t previous_channel;
    wifi_second_chan_t previous_secondary;
    wifi_promiscuous_filter_t previous_filter;
    ESP_RETURN_ON_ERROR(esp_wifi_get_channel(&previous_channel, &previous_secondary),
                        TAG, "read channel failed");
    ESP_RETURN_ON_ERROR(esp_wifi_get_promiscuous_filter(&previous_filter),
                        TAG, "read capture filter failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_channel(target->primary, WIFI_SECOND_CHAN_NONE),
                        TAG, "set PMF channel failed (station must not be connecting)");
    printf("WARN experimental PMF: " MACSTR " ch=%u timeout=%u ms; prior capture caused resets\n",
           MAC2STR(target->bssid), target->primary, WIFI_PMF_TIMEOUT_MS);
    fflush(stdout);
    error = pmf_capture_start(target);
    if (error == ESP_OK) {
        int64_t deadline = esp_timer_get_time() + WIFI_PMF_TIMEOUT_MS * 1000LL;
        while (esp_timer_get_time() < deadline) {
            portENTER_CRITICAL(&pmf_lock);
            bool received = pmf_sample.length != 0;
            portEXIT_CRITICAL(&pmf_lock);
            if (received) break;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    esp_err_t stop_error = pmf_capture_stop();
    esp_err_t filter_error = esp_wifi_set_promiscuous_filter(&previous_filter);
    esp_err_t channel_error = esp_wifi_set_channel(previous_channel, previous_secondary);
    if (stop_error != ESP_OK || filter_error != ESP_OK || channel_error != ESP_OK) {
        pmf_radio_dirty = true;
        printf("ERR PMF cleanup: stop=%s filter=%s channel=%s; reset device before using Wi-Fi\n",
               esp_err_to_name(stop_error), esp_err_to_name(filter_error), esp_err_to_name(channel_error));
        return stop_error != ESP_OK ? stop_error : filter_error != ESP_OK ? filter_error : channel_error;
    }
    if (error != ESP_OK) {
        printf("ERR PMF capture: %s\n", esp_err_to_name(error));
        return error;
    }
    lab_wifi_pmf_t pmf = lab_wifi_pmf_from_frame(pmf_sample.frame, pmf_sample.length);
    printf(MACSTR " ch=%u pmf_advertised=%s\n", MAC2STR(target->bssid), target->primary,
           lab_wifi_pmf_name(pmf));
    if (pmf_sample.length == 0) puts("NOTE no matching announcement captured before timeout");
    if (pmf_sample.oversized) puts("NOTE announcements exceeding 1536 bytes were skipped");
    puts("NOTE single announcement, not negotiated PMF or verified identity; unknown is not unsupported");
    return ESP_OK;
}

static int wifi_scan(bool passive, uint8_t channel)
{
    esp_err_t error = refresh_scan(passive, channel);
    if (error != ESP_OK) return error;
    for (uint16_t index = 0; index < scan_count; ++index) {
        print_ap(&scan_records[index], NULL);
    }
    return 0;
}

static int wifi_audit(void)
{
    esp_err_t error = refresh_scan(false, 0);
    if (error != ESP_OK) return error;
    unsigned flagged = 0;
    for (uint16_t index = 0; index < scan_count; ++index) {
        const wifi_ap_record_t *ap = &scan_records[index];
        print_ap(ap, NULL);
        const char *warning = lab_wifi_auth_warning(ap->authmode);
        bool legacy = lab_wifi_legacy_cipher(ap->pairwise_cipher) ||
                      lab_wifi_legacy_cipher(ap->group_cipher);
        if (warning != NULL) printf("  WARN %s\n", warning);
        if (legacy) puts("  WARN WEP/TKIP cipher advertised; use AES-CCMP or WPA3");
        flagged += warning != NULL || legacy;
        if (ap->authmode == WIFI_AUTH_OWE) {
            puts("  NOTE OWE encrypts the link but does not authenticate the AP");
        }
        if (strcmp(lab_wifi_auth_name(ap->authmode), "unknown") == 0 ||
            strcmp(lab_wifi_cipher_name(ap->pairwise_cipher), "unknown") == 0 ||
            strcmp(lab_wifi_cipher_name(ap->group_cipher), "unknown") == 0) {
            puts("  SKIP unknown security mode/cipher: manual review required");
        }
    }
    printf("OK audit complete: %u/%u retained APs have legacy/open warnings\n", flagged, scan_count);
    puts("NOTE advertised settings only: no password, negotiated PMF, client or router identity verification");
    return 0;
}

static int wifi_trust_persist(const char *action)
{
    bool load = strcmp(action, "load") == 0;
    bool erase = strcmp(action, "erase") == 0;
    esp_err_t error = lab_nvs_init();
    if (error != ESP_OK) return error;
    nvs_handle_t handle;
    error = nvs_open("wifi_trust", load ? NVS_READONLY : NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    if (erase) {
        error = nvs_erase_key(handle, "baseline");
        if (error == ESP_OK) error = nvs_commit(handle);
    } else {
        struct baseline {
            uint32_t version, record_size;
            lab_wifi_trust_store_t store;
        };
        struct baseline *saved = calloc(1, sizeof(*saved));
        if (!saved) { nvs_close(handle); return ESP_ERR_NO_MEM; }
        if (load) {
            size_t length = sizeof(*saved);
            error = nvs_get_blob(handle, "baseline", saved, &length);
            if (error == ESP_OK && (length != sizeof(*saved) || saved->version != 1 ||
                saved->record_size != sizeof(wifi_ap_record_t) || saved->store.count > LAB_WIFI_TRUST_LIMIT)) error = ESP_ERR_INVALID_SIZE;
            if (error == ESP_OK) trusted = saved->store;
        } else {
            saved->version = 1;
            saved->record_size = sizeof(wifi_ap_record_t);
            saved->store = trusted;
            error = nvs_set_blob(handle, "baseline", saved, sizeof(*saved));
            if (error == ESP_OK) error = nvs_commit(handle);
        }
        free(saved);
    }
    nvs_close(handle);
    printf("%s trust %s: %s; credentials are not stored\n", error == ESP_OK ? "OK" : "ERR", action, esp_err_to_name(error));
    return error;
}

static int wifi_trust(const char *argument)
{
    if (strcmp(argument, "save") == 0 || strcmp(argument, "load") == 0 || strcmp(argument, "erase") == 0)
        return wifi_trust_persist(argument);
    if (strcmp(argument, "list") == 0) {
        printf("OK %u/%u trusted APs in RAM; explicit trust save/load for persistence\n",
               (unsigned)trusted.count, LAB_WIFI_TRUST_LIMIT);
        for (size_t index = 0; index < trusted.count; ++index) print_ap(&trusted.aps[index], NULL);
        puts("NOTE stored baseline, not a live scan or verified identity; PMF is not stored/compared");
        return 0;
    }
    if (strcmp(argument, "clear") == 0) {
        memset(&trusted, 0, sizeof(trusted));
        puts("OK trust list cleared");
        return 0;
    }
    uint8_t bssid[6];
    if (!lab_wifi_parse_bssid(argument, bssid)) {
        puts("ERR USAGE: wifi trust <BSSID>|list|clear|save|load|erase (unicast, nonzero)");
        return ESP_ERR_INVALID_ARG;
    }
    if (!scan_valid || esp_timer_get_time() - scan_time_us > WIFI_TRUST_SCAN_MAX_AGE_US) {
        puts("ERR run wifi scan first; trust requires a successful scan less than 120 seconds old");
        return ESP_ERR_INVALID_STATE;
    }
    for (uint16_t index = 0; index < scan_count; ++index) {
        if (memcmp(scan_records[index].bssid, bssid, sizeof(bssid)) != 0) continue;
        lab_wifi_trust_result_t result = lab_wifi_trust_add(&trusted, &scan_records[index]);
        switch (result) {
        case LAB_WIFI_TRUST_ADDED:
            print_ap(&scan_records[index], NULL);
            puts("OK baseline saved in RAM; verify this BSSID in your router settings");
            return 0;
        case LAB_WIFI_TRUST_EXISTS:
            puts("OK already trusted; baseline unchanged");
            return 0;
        case LAB_WIFI_TRUST_CHANGED:
            puts("ERR SSID/security differs from baseline; review before wifi trust clear and re-enrolment");
            return ESP_ERR_INVALID_STATE;
        case LAB_WIFI_TRUST_FULL:
            puts("ERR trust list full (16 APs); use wifi trust clear to reset it");
            return ESP_ERR_NO_MEM;
        case LAB_WIFI_TRUST_HIDDEN:
            puts("ERR cannot enrol hidden/empty SSID; no name available for comparison");
            return ESP_ERR_INVALID_ARG;
        }
    }
    puts("ERR BSSID not in retained scan results; run wifi scan and check the address");
    return ESP_ERR_NOT_FOUND;
}

static int wifi_suspects(void)
{
    esp_err_t error = refresh_scan(false, 0);
    if (error != ESP_OK) return error;
    unsigned flagged = 0;
    for (uint16_t index = 0; index < scan_count; ++index) {
        const wifi_ap_record_t *ap = &scan_records[index];
        unsigned flags = lab_wifi_suspect_flags(&trusted, ap);
        if (flags == 0) continue;
        ++flagged;
        print_ap(ap, NULL);
        if (flags & LAB_WIFI_UNKNOWN_BSSID) puts("  REVIEW unknown BSSID using a trusted SSID");
        if (flags & LAB_WIFI_SSID_CHANGED) puts("  REVIEW known BSSID advertising a different/hidden SSID");
        if (flags & LAB_WIFI_SECURITY_CHANGED) puts("  REVIEW known BSSID advertising changed security");
    }
    printf("OK review complete: %u/%u retained APs flagged\n", flagged, scan_count);
    puts("NOTE mesh/repeaters/router changes can explain alerts; BSSIDs can be spoofed");
    puts("NOTE no alert does not prove safety; hidden SSIDs and omitted APs limit coverage");
    return 0;
}

static int wifi_connect(const char *ssid, const char *password)
{
    size_t ssid_length = strlen(ssid);
    size_t password_length = password ? strlen(password) : 0;
    if (ssid_length == 0 || ssid_length > 32 || (password && (password_length < 8 || password_length > 63))) {
        puts("ERR USAGE: wifi connect <ssid> [password 8..63 chars]");
        return ESP_ERR_INVALID_ARG;
    }
    if (xEventGroupGetBits(events) & WIFI_CONNECTED_BIT) {
        xEventGroupClearBits(events, WIFI_DISCONNECTED_BIT);
        esp_wifi_disconnect();
        xEventGroupWaitBits(events, WIFI_DISCONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(2000));
    }
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, ssid, ssid_length);
    if (password) {
        memcpy(config.sta.password, password, password_length);
    }
    config.sta.threshold.authmode = password ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    esp_err_t error = esp_wifi_set_config(WIFI_IF_STA, &config);
    memset(&config, 0, sizeof(config));
    if (error == ESP_OK) {
        xEventGroupClearBits(events, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT);
        error = esp_wifi_connect();
    }
    if (error != ESP_OK) {
        printf("ERR wifi connect: %s\n", esp_err_to_name(error));
        return error;
    }
    EventBits_t bits = xEventGroupWaitBits(events, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    if (!(bits & WIFI_CONNECTED_BIT)) {
        esp_wifi_disconnect();
        puts("ERR wifi connect: failed or timed out; check SSID, password and signal");
        return ESP_FAIL;
    }
    puts("OK connected");
    return 0;
}

static int wifi_status(void)
{
    wifi_ap_record_t ap;
    if (!(xEventGroupGetBits(events) & WIFI_CONNECTED_BIT) || esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
        puts("OK disconnected");
        return 0;
    }
    char ssid[sizeof(ap.ssid) + 1] = {0};
    memcpy(ssid, ap.ssid, sizeof(ap.ssid));
    lab_sanitize(ssid);
    esp_netif_ip_info_t ip = {0};
    esp_netif_get_ip_info(netif, &ip);
    printf("OK connected ssid=%s rssi=%d ch=%u\nIP: " IPSTR "\nGateway: " IPSTR "\n", ssid, ap.rssi,
           ap.primary, IP2STR(&ip.ip), IP2STR(&ip.gw));
    return 0;
}

static int wifi_command(int argc, char **argv)
{
    static const char *usage = "ERR USAGE: wifi scan [active|passive] [channel 0..13]|audit|pmf <BSSID>|suspects|trust <BSSID|list|clear|save|load|erase>|status|disconnect|connect <ssid> [password]";
    if (argc < 2) {
        puts(usage);
        return ESP_ERR_INVALID_ARG;
    }
    if (argc == 3 && strcmp(argv[1], "trust") == 0) return wifi_trust(argv[2]);
    if (pmf_radio_dirty) {
        puts("ERR Wi-Fi state uncertain after PMF cleanup failure; reset device");
        return ESP_ERR_INVALID_STATE;
    }
    if (argc == 3 && strcmp(argv[1], "pmf") == 0) return wifi_pmf(argv[2]);
    bool scan = argc >= 2 && argc <= 4 && strcmp(argv[1], "scan") == 0;
    bool passive = scan && argc >= 3 && strcmp(argv[2], "passive") == 0;
    uint32_t channel = 0;
    if (scan && ((argc >= 3 && !passive && strcmp(argv[2], "active") != 0) ||
        (argc == 4 && !lab_arg_u32(argv[3], 13, &channel)))) {
        puts(usage);
        return ESP_ERR_INVALID_ARG;
    }
    bool audit = argc == 2 && strcmp(argv[1], "audit") == 0;
    bool suspects = argc == 2 && strcmp(argv[1], "suspects") == 0;
    bool status = argc == 2 && strcmp(argv[1], "status") == 0;
    bool disconnect = argc == 2 && strcmp(argv[1], "disconnect") == 0;
    bool connect = (argc == 3 || argc == 4) && strcmp(argv[1], "connect") == 0;
    if (!scan && !audit && !suspects && !status && !disconnect && !connect) {
        puts(usage);
        return ESP_ERR_INVALID_ARG;
    }
    if (suspects && trusted.count == 0) {
        puts("ERR no trusted APs; run wifi scan then wifi trust <BSSID> for each known AP");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t error = wifi_start();
    if (error != ESP_OK) {
        printf("ERR wifi start: %s\n", esp_err_to_name(error));
        return error;
    }
    if (audit) return wifi_audit();
    if (suspects) return wifi_suspects();
    if (scan) {
        return wifi_scan(passive, (uint8_t)channel);
    }
    if (status) {
        return wifi_status();
    }
    if (disconnect) {
        esp_wifi_disconnect();
        puts("OK disconnected");
        return 0;
    }
    if (connect) {
        return wifi_connect(argv[2], argc == 4 ? argv[3] : NULL);
    }
    puts(usage);
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t diagnose(void)
{
        printf("SKIP wifi: RF not tested by diagnostics; driver=%s trusted=%u (RAM)\n",
            started ? "started" : "not started", (unsigned)trusted.count);
    return ESP_OK;
}

const lab_module_t lab_wifi_module = {
    .name = "wifi",
    .description = "Wi-Fi: scan [active|passive] [channel]|audit|pmf|trust <BSSID|list|clear|save|load|erase>|suspects|status|connect|disconnect",
    .command = wifi_command,
    .diagnose = diagnose,
    .implemented = true,
};
