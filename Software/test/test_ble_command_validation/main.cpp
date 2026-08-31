#include <unity.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <locale>
#include <regex>
#include <string>
#include <vector>

#include "ble_command_validation.h"

namespace {

std::size_t comparisons = 0;

// These are the production expressions before the flash-size cleanup. Keep
// them in native tests only as an independent compatibility oracle.
const std::regex& previousCommandRegex() {
    static const std::regex expression(
        R"(go:(simplePenetration|strokeEngine|streaming|menu)|set:(speed|stroke|depth|sensation|buffer|pattern):\d+|set:wifi:[^|]+\|.+|stream:\d+:\d+)");
    return expression;
}

const std::regex& previousGpioRegex() {
    static const std::regex expression(R"(^\s*(\d+)\s*:(low|high|0|1)\s*$)",
                                       std::regex::icase);
    return expression;
}

std::string describeBytes(const std::string& value) {
    constexpr char hex[] = "0123456789abcdef";
    std::string output = "input hex:";
    for (unsigned char character : value) {
        output += ' ';
        output += hex[character >> 4];
        output += hex[character & 0x0f];
    }
    return output;
}

void checkAgainstPrevious(const std::string& value) {
    const bool expectedCommand = std::regex_match(value, previousCommandRegex());
    const bool actualCommand = ble_command_validation::isLegacyCommand(value);
    const bool expectedGpio = std::regex_match(value, previousGpioRegex());
    const bool actualGpio = ble_command_validation::isGpioCommand(value);
    comparisons += 2;

    if (expectedCommand != actualCommand || expectedGpio != actualGpio) {
        const std::string message = describeBytes(value);
        TEST_ASSERT_EQUAL_MESSAGE(expectedCommand, actualCommand,
                                  message.c_str());
        TEST_ASSERT_EQUAL_MESSAGE(expectedGpio, actualGpio, message.c_str());
    }
}

void test_known_legacy_commands_keep_their_syntax() {
    const std::vector<std::string> commands = {
        "go:simplePenetration", "go:strokeEngine", "go:streaming", "go:menu",
        "set:speed:0", "set:stroke:100", "set:depth:0001",
        "set:sensation:99", "set:buffer:1000", "set:pattern:0000",
        "stream:0:0", "stream:100:65535", "set:wifi:ssid|password",
        "set:wifi:s|p|more", "set:wifi:s\ns\r|p", "set:wifi:s|\t\f\v",
        std::string("set:wifi:s") + '\0' + "sid|password",
        std::string("set:wifi:ssid|p") + '\0' + "ass",
    };
    for (const auto& command : commands) {
        TEST_ASSERT_TRUE(ble_command_validation::isLegacyCommand(command));
        checkAgainstPrevious(command);
    }
}

void test_invalid_commands_are_not_newly_accepted() {
    const std::vector<std::string> commands = {
        "", "go:", "go:Menu", "Go:menu", " go:menu", "go:menu ",
        "go:menu\n", "go:unknown", "set:speed:", "set:speed:-1",
        "set:speed:+1", "set:speed:1.0", "set:speed:1:2", "set:Speed:1",
        "set:unknown:1", "set:wifi:ssid", "set:wifi:|password",
        "set:wifi:ssid|", "set:wifi:ssid|a\nb", "set:wifi:ssid|a\rb",
        "stream:1", "stream::1", "stream:1:", "stream:1:2:3",
        "stream:1:-2", "stream: 1:2", "1:high", "{\"op\":\"state.read\"}",
        std::string("set:speed:1") + '\0',
    };
    for (const auto& command : commands) {
        TEST_ASSERT_FALSE(ble_command_validation::isLegacyCommand(command));
        checkAgainstPrevious(command);
    }
}

void test_gpio_preserves_formats_and_leaves_range_checks_to_dispatch() {
    const std::vector<std::string> commands = {
        "1:low", "2:HIGH", "3:0", "4:1", "0001:HiGh",
        " \t1 \r\n:low\v\f", "0:high", "5:low", "9999999999999999999:1",
    };
    for (const auto& command : commands) {
        TEST_ASSERT_TRUE(ble_command_validation::isGpioCommand(command));
        checkAgainstPrevious(command);
    }

    const std::vector<std::string> invalid = {
        "", ":high", "-1:high", "+1:high", "1.0:high", "1: high",
        "1:\tlow", "1:hi gh", "1:2", "1:true", "1:high:low",
        "9:invalid", "9: high", "1:high\nx", "1 2:high",
        std::string("1:high") + '\0',
    };
    for (const auto& command : invalid) {
        TEST_ASSERT_FALSE(ble_command_validation::isGpioCommand(command));
        checkAgainstPrevious(command);
    }
}

void test_all_bytes_at_every_position_match_previous_grammars() {
    const std::array<std::string, 9> seeds = {
        "go:simplePenetration", "go:strokeEngine", "go:streaming", "go:menu",
        "set:speed:1", "set:wifi:s|p", "stream:1:2", "1:high", "2:low",
    };
    for (const auto& seed : seeds) {
        for (std::size_t position = 0; position <= seed.size(); ++position) {
            for (unsigned int byte = 0; byte <= 255; ++byte) {
                auto inserted = seed;
                inserted.insert(position, 1, static_cast<char>(byte));
                checkAgainstPrevious(inserted);

                if (position < seed.size()) {
                    auto replaced = seed;
                    replaced[position] = static_cast<char>(byte);
                    checkAgainstPrevious(replaced);
                }
            }
        }
    }
}

void test_gpio_whitespace_and_case_combinations_match() {
    constexpr std::array<const char*, 8> whitespace = {
        "", " ", "\t", "\n", "\r", "\f", "\v", " \t\r\n\f\v",
    };
    for (const std::string lower : {"low", "high"}) {
        for (unsigned int casing = 0; casing < (1U << lower.size()); ++casing) {
            auto level = lower;
            for (std::size_t index = 0; index < level.size(); ++index) {
                if (casing & (1U << index)) level[index] -= 'a' - 'A';
            }
            for (const char* leading : whitespace) {
                for (const char* beforeColon : whitespace) {
                    for (const char* trailing : whitespace) {
                        const std::string command = std::string(leading) +
                            "0004" + beforeColon + ":" + level + trailing;
                        TEST_ASSERT_TRUE(
                            ble_command_validation::isGpioCommand(command));
                        checkAgainstPrevious(command);
                    }
                }
            }
        }
    }
}

void test_decimal_syntax_does_not_add_conversion_or_length_limits() {
    const std::vector<std::string> numbers = {
        "0", "0001", "2147483647", "2147483648", "4294967295",
        "4294967296", "18446744073709551615", "18446744073709551616",
        std::string(512, '9'), std::string(512, '0') + '1',
    };
    for (const auto& number : numbers) {
        const std::string set = "set:speed:" + number;
        const std::string stream = "stream:" + number + ':' + number;
        const std::string gpio = number + ":high";
        TEST_ASSERT_TRUE(ble_command_validation::isLegacyCommand(set));
        TEST_ASSERT_TRUE(ble_command_validation::isLegacyCommand(stream));
        TEST_ASSERT_TRUE(ble_command_validation::isGpioCommand(gpio));
        checkAgainstPrevious(set);
        checkAgainstPrevious(stream);
        checkAgainstPrevious(gpio);
        checkAgainstPrevious(gpio + ":invalid");
    }
}

void test_seeded_mutations_match_previous_grammars() {
    const std::array<std::string, 10> seeds = {
        "go:menu", "go:strokeEngine", "set:speed:100", "set:buffer:0000",
        "set:wifi:ssid|password|extra", "stream:1:65535", "1:high",
        " \t0004\r:LoW\n", "9:invalid", "",
    };
    std::uint32_t randomState = 0x6f73736d;
    auto next = [&randomState]() {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        return randomState;
    };
    for (unsigned int sample = 0; sample < 10000; ++sample) {
        auto command = seeds[next() % seeds.size()];
        const unsigned int mutations = 1 + next() % 4;
        for (unsigned int mutation = 0; mutation < mutations; ++mutation) {
            const auto position = next() % (command.size() + 1);
            switch (next() % 3) {
                case 0:
                    command.insert(position, 1, static_cast<char>(next()));
                    break;
                case 1:
                    if (position < command.size()) command.erase(position, 1);
                    break;
                case 2:
                    if (position < command.size())
                        command[position] = static_cast<char>(next());
                    break;
            }
        }
        checkAgainstPrevious(command);
    }
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    std::locale::global(std::locale::classic());
    UNITY_BEGIN();
    RUN_TEST(test_known_legacy_commands_keep_their_syntax);
    RUN_TEST(test_invalid_commands_are_not_newly_accepted);
    RUN_TEST(test_gpio_preserves_formats_and_leaves_range_checks_to_dispatch);
    RUN_TEST(test_all_bytes_at_every_position_match_previous_grammars);
    RUN_TEST(test_gpio_whitespace_and_case_combinations_match);
    RUN_TEST(test_decimal_syntax_does_not_add_conversion_or_length_limits);
    RUN_TEST(test_seeded_mutations_match_previous_grammars);
    std::printf("Compared %zu parser results against the previous regexes.\n",
                comparisons);
    return UNITY_END();
}
