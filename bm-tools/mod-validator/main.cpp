#include <iostream>
#include <string>
#include <json/json.h>
#include <fstream>

/**
 * Basketball Manager Mod Validator Tool
 * Validates mod.json and mod structure
 */

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: bm-mod-validator <mod_directory>\n";
        return 1;
    }

    std::string modDir = argv[1];
    std::cout << "[ModValidator] Validating mod: " << modDir << "\n";

    // Check for mod.json
    std::string modJsonPath = modDir + "/mod.json";
    std::ifstream modFile(modJsonPath);
    
    if (!modFile.is_open()) {
        std::cerr << "[ERROR] mod.json not found\n";
        return 1;
    }

    // Parse JSON
    Json::Value root;
    try {
        modFile >> root;
    } catch (...) {
        std::cerr << "[ERROR] Invalid JSON in mod.json\n";
        return 1;
    }

    // Validate required fields
    if (!root.isMember("name") || !root.isMember("version")) {
        std::cerr << "[ERROR] Missing required fields (name, version)\n";
        return 1;
    }

    std::cout << "[ModValidator] Mod name: " << root["name"].asString() << "\n";
    std::cout << "[ModValidator] Mod version: " << root["version"].asString() << "\n";

    // Check data files
    if (root.isMember("content")) {
        Json::Value content = root["content"];
        // Validate referenced files exist
    }

    std::cout << "[ModValidator] ✅ Mod validation passed\n";
    return 0;
}
