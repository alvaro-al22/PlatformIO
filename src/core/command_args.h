#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline bool lab_arg_u32(const char *text, uint32_t maximum, uint32_t *value)
{
    if (!text || !*text || !value) return false;
    uint32_t parsed = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        uint32_t digit = (uint32_t)(*text - '0');
        if (digit > maximum || parsed > (maximum - digit) / 10) return false;
        parsed = parsed * 10 + digit;
    }
    *value = parsed;
    return true;
}

static inline int lab_hex_digit(char character)
{
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

static inline bool lab_arg_hex(const char *text, uint8_t *bytes, size_t capacity, size_t *length)
{
    if (!text || !*text || !bytes || !length) return false;
    size_t count = 0;
    while (*text) {
        if (!text[1] || count == capacity) return false;
        int high = lab_hex_digit(text[0]), low = lab_hex_digit(text[1]);
        if (high < 0 || low < 0) return false;
        bytes[count++] = (uint8_t)((high << 4) | low);
        text += 2;
    }
    *length = count;
    return true;
}

static inline bool lab_arg_name(const char *text)
{
    if (!text || !*text) return false;
    size_t length = 0;
    for (; *text; ++text) {
        if (++length > 15 || !((*text >= 'a' && *text <= 'z') ||
            (*text >= 'A' && *text <= 'Z') || (*text >= '0' && *text <= '9') ||
            *text == '_' || *text == '-')) return false;
    }
    return true;
}
