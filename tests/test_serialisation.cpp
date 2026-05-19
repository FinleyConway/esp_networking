#include <catch2/catch_test_macros.hpp>

#include <bit>

#include "common/core/serialisation.hpp"

struct test_t {
    int64_t id = 0;
    uint32_t sweets = 0;
    float lollies = 0.0f;
    double pop = 0.0;

    bool operator==(const test_t& other) const {
        return (
            id == other.id &&
            sweets == other.sweets &&
            lollies == other.lollies &&
            pop == other.pop
        );
    }
};

struct test_with_array_t {
    std::array<int32_t, 10> arr;

    bool operator==(const test_with_array_t& other) const {
        return arr == other.arr;
    }
};

TEST_CASE( "Integer serialisation", "[serialisation]" ) {
    std::array<uint8_t, 10> payload;

    SECTION("Write to payload") {
        std::span<uint8_t> payload_sp(payload);
        uint16_t value = 10;

        common::write(payload_sp, value);

        uint16_t test_value = 0;
        std::memcpy(&test_value, payload.data(), sizeof(uint16_t));

        if constexpr (std::endian::native == std::endian::big) {
            REQUIRE(test_value == 0x0A00);
        }
        else {
            REQUIRE(test_value == 0x000A);
        }
    }

    SECTION("Read from payload") {
        payload[0] = 0x0A;
        payload[1] = 0x00;

        std::span<uint8_t> payload_sp(payload);
        uint16_t value = common::read<uint16_t>(payload_sp);

        REQUIRE(value == 10);
    }

    SECTION("Read and writing from payload") {
        test_t write_t = {
            .id = 100,
            .sweets = 99,
            .lollies = 50.0f,
            .pop = 2.0
        };

        std::array<uint8_t, 24> alt_payload;
        std::span<uint8_t> write_payload_sp(alt_payload);

        common::write(write_payload_sp, write_t.id);
        common::write(write_payload_sp, write_t.sweets);
        common::write(write_payload_sp, write_t.lollies);
        common::write(write_payload_sp, write_t.pop);

        test_t send_t;
        std::span<uint8_t> read_payload_sp(alt_payload);

        send_t.id = common::read<int64_t>(read_payload_sp);
        send_t.sweets = common::read<uint32_t>(read_payload_sp);
        send_t.lollies = common::read<float>(read_payload_sp);
        send_t.pop = common::read<double>(read_payload_sp);

        REQUIRE(write_t == send_t);
    }

    SECTION("Read and writing an array") {
        test_with_array_t array_write {
            .arr = {
                0, 1, 2, 3, 4, 5, 6, 7, 8, 9
            }
        };

        std::array<uint8_t, 4*10> alt_payload;
        std::span<uint8_t> write_payload_sp(alt_payload);

        common::write(write_payload_sp, array_write.arr);

        test_with_array_t array_read;
        std::span<uint8_t> read_payload_sp(alt_payload);

        array_read.arr = common::read<int32_t, 10>(read_payload_sp);

        REQUIRE(array_write == array_read);
    }
}