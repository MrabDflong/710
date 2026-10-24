
#include "LevelManager.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <system_error>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

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
        if (name == "WIRE") {
            return GateType::Wire;
        }
        if (name == "NAND") {
            return GateType::Nand;
        }
        if (name == "XOR") {
            return GateType::Xor;
        }

        throw std::runtime_error("Unknown gate type: " + name);
    }

    float parseCoordinate(const std::string& value) {
        try {
            std::size_t processed = 0;
            const float result = std::stof(value, &processed);

            if (processed != value.size() ||
                !std::isfinite(result)) {
                throw std::invalid_argument("Invalid coordinate.");
            }

            return result;
        }
        catch (const std::exception&) {
            throw std::runtime_error("Invalid socket coordinate: " + value);
        }
    }

    SocketDefinition parseSocket(const std::string& value) {
        std::stringstream stream{ value };
        std::vector<std::string> fields;
        std::string part;

        while (std::getline(stream, part, ',')) {
            fields.push_back(trim(part));
        }

        if (value.back() == ',' ||
            (fields.size() != 4 && fields.size() != 5)) {
            throw std::runtime_error("Socket requires ID,X,Y and 1 or 2 sources.");
        }

        for (const auto& field : fields) {
            if (field.empty()) {
                throw std::runtime_error("Socket contains an empty field.");
            }
        }

        SocketDefinition socket;
        socket.id = fields[0];
        socket.x = parseCoordinate(fields[1]);
        socket.y = parseCoordinate(fields[2]);

        socket.sources.assign(
            fields.begin() + 3,
            fields.end()
        );

        return socket;
    }

}


// LevelManager
// ------------------------------
LevelManager::LevelManager(
    const std::filesystem::path& directory
) {
    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error("Level directory not found: " + directory.string());
    }

    std::vector<std::filesystem::path> files;

    for (const auto& entry :
        std::filesystem::directory_iterator(directory)) {

        if (entry.is_regular_file() && entry.path().extension() == ".lvl") {
            files.push_back(entry.path());
        }
    }

    // Filename order determines work-order order.
    std::sort(files.begin(), files.end());

    if (files.empty()) {
        throw std::runtime_error("No .lvl files found in: " + directory.string());
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
        throw std::runtime_error("Unable to open level: " + path.string());
    }

    Level level;
    std::unordered_set<std::string> seenKeys;

    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        line = trim(line);

        if (line.empty() || line.starts_with('#')) {
            continue;
        }

        const auto separator = line.find('=');

        if (separator == std::string::npos) {
            throw std::runtime_error(
                path.string() + " line " + std::to_string(lineNumber) + ": Expected key=value."
            );
        }

        const std::string key = trim(line.substr(0, separator));

        const std::string value = trim(line.substr(separator + 1));

        if (key.empty() || value.empty()) {
            throw std::runtime_error(
                path.string() + " line " + std::to_string(lineNumber) + ": Empty key or value."
            );
        }

        // Multiple sockets are allowed.
        // Multiples of other properties are not.
        if (key != "socket" &&
            !seenKeys.insert(key).second) {
            throw std::runtime_error(path.string() + ": Duplicate key " + key);
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
            std::string name;

            while (std::getline(stream, name, ',')) {
                const GateType gate = parseGateType(trim(name));

                if (level.allowsGate(gate)) {
                    throw std::runtime_error(
                        path.string() + ": Duplicate gate in inventory."
                    );
                }

                level.availableGates.push_back(gate);
            }

            if (value.back() == ',') {
                throw std::runtime_error(path.string() + ": Empty gate in inventory.");
            }
        }
        else if (key == "truth_table") {
            if (value.size() != level.expectedOutputs.size()) {
                throw std::runtime_error(
                    path.string() + ": Truth table must contain 4 outputs."
                );
            }

            for (std::size_t i = 0; i < value.size(); ++i) {
                if (value[i] != '0' && value[i] != '1') {
                    throw std::runtime_error(
                        path.string() + ": Truth table must contain only 0 or 1."
                    );
                }

                level.expectedOutputs[i] = value[i] == '1';
            }
        }
        else if (key == "socket") {
            level.sockets.push_back(parseSocket(value));
        }
        else if (key == "output") {
            level.outputSocket = value;
        }
        else if (key == "optimal_gates") {
            std::size_t count{};

            const auto [end, error] = std::from_chars(
                value.data(),
                value.data() + value.size(),
                count
            );

            if (error != std::errc{} ||
                end != value.data() + value.size()) {
                throw std::runtime_error(path.string() + ": Invalid optimal_gates value.");
            }

            level.optimalGateCount = count;
        }
        else if (key == "certifies") {
            level.certifies = parseGateType(value);
        }
        else if (key == "arrival_message") {
            level.arrivalMessage = value;
        }
        else {
            throw std::runtime_error(path.string() + ": Unknown property " + key);
        }
    }

    if (file.bad()) {
        throw std::runtime_error("Error reading level: " + path.string());
    }

    // Validate required properties.
    for (const auto* required : {
        "id", 
        "title", 
        "objective", 
        "hint",
        "gates", 
        "truth_table", 
        "output"
        }) {
        if (!seenKeys.contains(required)) {
            throw std::runtime_error(
                path.string() + ": Missing required property " + required
            );
        }
    }

    if (level.availableGates.empty() || level.sockets.empty()) {
        throw std::runtime_error(
            path.string() + ": Level needs gates and at least one socket."
        );
    }

    // Validate socket IDs and connection order.
    std::unordered_set<std::string> definedSockets;

    for (const auto& socket : level.sockets) {
        if (socket.id == "A" || socket.id == "B" ||
            !definedSockets.insert(socket.id).second) {
            throw std::runtime_error(
                path.string() + ": Duplicate or reserved socket ID: " + socket.id
            );
        }

        // Each source must be an external input or a socket defined before this one.
        for (const auto& source : socket.sources) {
            if (source != "A" && source != "B" &&
                !definedSockets.contains(source)) {
                throw std::runtime_error(
                    path.string() + ": Invalid or forward connection: " + source
                );
            }

            if (source == socket.id) {
                throw std::runtime_error(
                    path.string() + ": Socket cannot connect to itself."
                );
            }
        }

        // At least one available gate must fit this socket.
        const bool compatible = std::any_of(
            level.availableGates.begin(),
            level.availableGates.end(),
            [&](GateType type) {
                return LogicGate{ type }.inputCount() == socket.sources.size();
            }
        );

        if (!compatible) {
            throw std::runtime_error(
                path.string() + ": No compatible gate for socket " + socket.id
            );
        }
    }

    if (!definedSockets.contains(level.outputSocket)) {
        throw std::runtime_error(
            path.string() + ": Output references an unknown socket."
        );
    }

    if (level.optimalGateCount.has_value() &&
        *level.optimalGateCount > level.sockets.size()) {
        throw std::runtime_error(
            path.string() + ": Optimal gate count exceeds socket count."
        );
    }

    if (level.certifies.has_value()) {
        if (*level.certifies == GateType::Wire) {
            throw std::runtime_error(
                path.string() + ": WIRE cannot be a certified logic gate."
            );
        }

        if (level.allowsGate(*level.certifies)) {
            throw std::runtime_error(
                path.string() + ": Certified gate is already in the inventory."
            );
        }
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
