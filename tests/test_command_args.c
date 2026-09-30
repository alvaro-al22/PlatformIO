#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../src/core/command_args.h"

static unsigned checks;
#define CHECK(condition) do { ++checks; if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)

int main(void)
{
    uint32_t number = 99;
    CHECK(!lab_arg_u32(NULL, 100, &number));
    CHECK(!lab_arg_u32("1", 100, NULL));
    const char *invalid[] = {"", "-1", "+1", " 1", "1 ", "1x", "1.0", "0x10", "4294967296", "99999999999999999999999999"};
    for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
        number = 99;
        CHECK(!lab_arg_u32(invalid[index], UINT32_MAX, &number));
        CHECK(number == 99);
    }
    CHECK(lab_arg_u32("4294967295", UINT32_MAX, &number) && number == UINT32_MAX);
    CHECK(lab_arg_u32("0", 0, &number) && number == 0);
    CHECK(!lab_arg_u32("1", 0, &number));
    CHECK(lab_arg_u32("00005", 5, &number) && number == 5);
    for (unsigned value = 0; value < 1000; ++value) {
        char text[16];
        snprintf(text, sizeof(text), "%u", value);
        CHECK(lab_arg_u32(text, value, &number) && number == value);
        if (value) CHECK(!lab_arg_u32(text, value - 1, &number));
    }
    struct { uint8_t bytes[4]; uint8_t guard; } output = {{0}, 0xa5};
    size_t length = 99;
    CHECK(lab_arg_hex("00aB12FF", output.bytes, 4, &length));
    CHECK(length == 4 && output.bytes[0] == 0 && output.bytes[1] == 0xab && output.bytes[3] == 0xff);
    const char *bad_hex[] = {"", "0", "123", "0x", "gg", "00 ff", "0011223344", "-1"};
    for (size_t index = 0; index < sizeof(bad_hex) / sizeof(bad_hex[0]); ++index) {
        CHECK(!lab_arg_hex(bad_hex[index], output.bytes, 4, &length));
        CHECK(output.guard == 0xa5);
    }
    CHECK(!lab_arg_hex(NULL, output.bytes, 4, &length));
    CHECK(!lab_arg_hex("00", NULL, 4, &length));
    CHECK(!lab_arg_hex("00", output.bytes, 4, NULL));
    CHECK(!lab_arg_hex("00", output.bytes, 0, &length));
    for (unsigned value = 0; value < 256; ++value) {
        char text[3];
        snprintf(text, sizeof(text), "%02x", value);
        CHECK(lab_arg_hex(text, output.bytes, 4, &length) && length == 1 && output.bytes[0] == value);
    }
    CHECK(lab_arg_name("remote_1-Test"));
    CHECK(lab_arg_name("123456789012345"));
    const char *bad_names[] = {"", ".", "..", "../test", "a/b", "a\\b", "x:y", "a b", "bad\n", "1234567890123456", "\x80"};
    for (size_t index = 0; index < sizeof(bad_names) / sizeof(bad_names[0]); ++index) CHECK(!lab_arg_name(bad_names[index]));
    CHECK(!lab_arg_name(NULL));
    printf("PASS command arguments: %u checks\n", checks);
    return 0;
}
