#pragma once

#include "Level.h"

#include <cstddef>
#include <filesystem>
#include <vector>

class LevelManager {
public:
    explicit LevelManager(
        const std::filesystem::path& directory
    );

    [[nodiscard]] const Level& currentLevel() const;
    [[nodiscard]] bool hasNextLevel() const noexcept;

    bool nextLevel();

private:
    [[nodiscard]] static Level loadFromFile(
        const std::filesystem::path& path
    );

    std::vector<Level> levels_;
    std::size_t currentIndex_{ 0 };
};
