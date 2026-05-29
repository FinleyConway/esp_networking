#include <catch2/catch_test_macros.hpp>

#include "common/core/packet_registry.hpp"
#include "common/core/serialise.hpp"
#include "common/api/types.hpp"

struct test_one_t {
    uint16_t id = 0;

    static void serialise(std::span<uint8_t>& payload, const test_one_t& data) {
        common::serialise_t::write(payload, data.id);
    }

    static test_one_t deserialise(std::span<uint8_t> payload) {
        test_one_t result;

        result.id = common::serialise_t::read<uint16_t>(payload); 

        return result;
    }

    static constexpr size_t payload_size() {
        return sizeof(
            uint16_t
        );
    }
};

struct test_two_t {
    uint16_t id = 0;

    static void serialise(std::span<uint8_t>& payload, const test_two_t& data) {
        common::serialise_t::write(payload, data.id);
    }

    static test_two_t deserialise(std::span<uint8_t> payload) {
        test_two_t result;

        result.id = common::serialise_t::read<uint16_t>(payload); 

        return result;
    }

    static constexpr size_t payload_size() {
        return sizeof(
            uint16_t
        );
    }
};

using reg_t = common::packet_registry_impl_t<
    test_one_t,
    test_two_t
>;

using payload_t = reg_t::payload_t;
using reg_id_t = reg_t::packet_id_t;

void test_one_callback(const test_one_t& t) {
    REQUIRE(t.id == 10);
}

TEST_CASE( "packet_registry class", "[packet_registry]" ) {
    reg_t registry;

    SECTION("Get packet size") {
        std::optional<size_t> bytes_1 = registry.expected_payload_size(0);
        size_t bytes_2 = registry.packet_size<test_one_t>();

        REQUIRE(bytes_1 == 0);
        REQUIRE(bytes_2 == 0);

        registry.register_callback<test_one_t, &test_one_callback>();

        std::optional<size_t> reg_bytes_1 = registry.expected_payload_size(0);
        size_t reg_bytes_2 = registry.packet_size<test_one_t>();

        REQUIRE(reg_bytes_1 == 2);
        REQUIRE(reg_bytes_2 == 2);
    }

    SECTION("Create payload") {
        test_one_t input{ .id = 10 };

        payload_t payload = registry.create(input);

        reg_id_t extracted_id = 0;
        std::memcpy(&extracted_id, payload.data(), sizeof(reg_id_t));

        REQUIRE(extracted_id == 0);

        std::span<uint8_t> payload_view(
            payload.data() + sizeof(reg_id_t), 
            payload.size() - sizeof(reg_id_t)
        );

        test_one_t expected = test_one_t::deserialise(payload_view);

        REQUIRE(input.id == expected.id);
    }

    SECTION("Dispatch regisered payload") {
        registry.register_callback<test_one_t, &test_one_callback>();
        
        test_one_t input{ .id = 10 };
        payload_t payload = registry.create(input);

        bool result = registry.dispatch(
            0,
            std::move(payload),
            4
        );

        REQUIRE(result == true);
    }

    SECTION("Dispatch invalid payload") {
        bool result = registry.dispatch(
            999,
            payload_t{},
            0
        );

        REQUIRE(result == false);
    }

    SECTION("Dispatch unregisered payload") {
        test_one_t input{ .id = 10 };
        payload_t payload = registry.create(input);

        bool result = registry.dispatch(
            0,
            std::move(payload),
            4
        );

        REQUIRE(result == false);
    }

    SECTION("Dispatch payload size mismatch") {
        registry.register_callback<test_one_t, &test_one_callback>();

        test_one_t input{ .id = 10 };
        payload_t payload = registry.create(input);

        bool result = registry.dispatch(
            0,
            std::move(payload),
            4 + 5
        );

        REQUIRE(result == false);
    }
}