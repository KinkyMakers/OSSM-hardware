#ifndef PATTERN_REGISTRY_H
#define PATTERN_REGISTRY_H

#include <vector>

struct PatternEntry {
    const char* id;
    const char* name;
    const char* description;
    bool isHardcoded;
};

extern std::vector<PatternEntry> patternCatalog;
extern size_t totalPatternCount;

void buildPatternCatalog();

#endif  // PATTERN_REGISTRY_H
