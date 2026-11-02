#pragma once

#include "Level.h"

#include <cstddef>
#include <filesystem>
#include <vector>

class LevelManager {
public:
    explicit LevelManager(const std::filesystem::path& directory);

    [[nodiscard]] const Level& currentLevel() const;

    [[nodiscard]] const Level& levelAt(std::size_t index) const;

    [[nodiscard]] std::size_t levelCount() const noexcept;

    [[nodiscard]] std::size_t currentIndex() const noexcept;

    [[nodiscard]] bool hasNextLevel() const noexcept;

    bool nextLevel();

    bool goToLevel(std::size_t index);

private:
    [[nodiscard]] static Level loadFromFile(const std::filesystem::path& path);

    std::vector<Level> levels_;
    std::size_t currentIndex_{ 0 };
};
