#include "LevelManager.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

    // Remove leading and trailing whitespace.
    std::string trim(const std::string& text) {
        const auto first = text.find_first_not_of(" \t\r\n");

        if (first == std::string::npos) {
            return {};
        }

        const auto last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    GateType parseGateType(const std::string& name) {
        if (name == "AND") {
            return GateType::And;
        }

        if (name == "OR") {
            return GateType::Or;
        }

        if (name == "NOT") {
            return GateType::Not;
        }

        throw std::runtime_error(
            "Unknown gate type: " + name
        );
    }

}

// Load all .lvl files when manager is created.
LevelManager::LevelManager(
    const std::filesystem::path& directory
) {
    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error(
            "Level directory not found: " + directory.string()
        );
    }

    std::vector<std::filesystem::path> files;

    for (const auto& entry :
        std::filesystem::directory_iterator(directory)) {

        if (entry.is_regular_file() &&
            entry.path().extension() == ".lvl") {
            files.push_back(entry.path());
        }
    }

    // Sort by filename. level01, level02...
    std::sort(files.begin(), files.end());

    if (files.empty()) {
        throw std::runtime_error(
            "No .lvl files found in: " + directory.string()
        );
    }

    for (const auto& file : files) {
        levels_.push_back(loadFromFile(file));
    }
}

Level LevelManager::loadFromFile(
    const std::filesystem::path& path
) {
    std::ifstream file{ path };

    if (!file.is_open()) {
        throw std::runtime_error(
            "Unable to open level: " + path.string()
        );
    }

    Level level;
    std::unordered_set<std::string> seenKeys;

    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;

        line = trim(line);

        // Ignore empty lines and comments.
        if (line.empty() || line.starts_with('#')) {
            continue;
        }

        const auto separator = line.find('=');

        if (separator == std::string::npos) {
            throw std::runtime_error(
                path.string() + " line " +
                std::to_string(lineNumber) +
                ": Expected key=value."
            );
        }

        const std::string key =
            trim(line.substr(0, separator));

        const std::string value =
            trim(line.substr(separator + 1));

        if (key.empty() || value.empty()) {
            throw std::runtime_error(
                path.string() + " line " +
                std::to_string(lineNumber) +
                ": Empty key or value."
            );
        }

        if (!seenKeys.insert(key).second) {
            throw std::runtime_error(
                path.string() + ": Duplicate key " + key
            );
        }

        if (key == "id") {
            level.id = value;
        }
        else if (key == "title") {
            level.title = value;
        }
        else if (key == "objective") {
            level.objective = value;
        }
        else if (key == "hint") {
            level.hint = value;
        }
        else if (key == "gates") {
            std::stringstream stream{ value };
            std::string gateName;

            while (std::getline(stream, gateName, ',')) {
                const auto gate = parseGateType(trim(gateName));

                if (level.allowsGate(gate)) {
                    throw std::runtime_error(
                        path.string() +
                        ": Duplicate gate in inventory."
                    );
                }

                level.availableGates.push_back(gate);
            }
        }
        else if (key == "truth_table") {
            if (value.size() != level.expectedOutputs.size()) {
                throw std::runtime_error(
                    path.string() +
                    ": Truth table must contain 4 outputs."
                );
            }

            for (std::size_t i = 0; i < value.size(); ++i) {
                if (value[i] != '0' && value[i] != '1') {
                    throw std::runtime_error(
                        path.string() +
                        ": Truth table must contain only 0 or 1."
                    );
                }

                level.expectedOutputs[i] = value[i] == '1';
            }
        }
        else {
            throw std::runtime_error(
                path.string() + ": Unknown property " + key
            );
        }
    }

    if (file.bad()) {
        throw std::runtime_error(
            "Error reading level: " + path.string()
        );
    }

    // Make sure file contains every required property.
    for (const auto* required : {
        "id", "title", "objective", "hint",
        "gates", "truth_table"
        }) {
        if (!seenKeys.contains(required)) {
            throw std::runtime_error(
                path.string() +
                ": Missing required property " + required
            );
        }
    }

    if (level.availableGates.empty()) {
        throw std::runtime_error(
            path.string() + ": No gates available."
        );
    }

    return level;
}

const Level& LevelManager::currentLevel() const {
    return levels_.at(currentIndex_);
}

bool LevelManager::hasNextLevel() const noexcept {
    return currentIndex_ + 1 < levels_.size();
}

bool LevelManager::nextLevel() {
    if (!hasNextLevel()) {
        return false;
    }

    ++currentIndex_;
    return true;
}
