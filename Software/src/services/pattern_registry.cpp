#include "pattern_registry.h"

#include <cstring>

#include "Strings.h"
#include "esp_log.h"
#include "memory.h"

std::vector<PatternEntry> patternCatalog;
size_t totalPatternCount = 0;

static constexpr size_t hardcodedNameCount =
    sizeof(ui::strings::strokeEngineNames) /
    sizeof(ui::strings::strokeEngineNames[0]);

static constexpr size_t hardcodedDescCount =
    sizeof(ui::strings::strokeEngineDescriptions) /
    sizeof(ui::strings::strokeEngineDescriptions[0]);

void buildPatternCatalog() {
    patternCatalog.clear();

    for (size_t i = 0; i < hardcodedNameCount; i++) {
        const char* desc = (i < hardcodedDescCount)
                               ? ui::strings::strokeEngineDescriptions[i]
                               : ui::strings::noDescription;
        patternCatalog.push_back(
            {nullptr, ui::strings::strokeEngineNames[i], desc, true});
    }

    for (JsonPair kv : registry.as<JsonObject>()) {
        JsonObject entry = kv.value().as<JsonObject>();
        const char* name = entry["name"] | "Unknown";
        const char* desc = entry["description"] | "";

        patternCatalog.push_back(
            {strdup(kv.key().c_str()), strdup(name), strdup(desc), false});
    }

    totalPatternCount = patternCatalog.size();

    ESP_LOGI("REGISTRY", "Pattern catalog built: %d hardcoded + %d registry = %d total",
             hardcodedNameCount,
             totalPatternCount - hardcodedNameCount,
             totalPatternCount);
}
