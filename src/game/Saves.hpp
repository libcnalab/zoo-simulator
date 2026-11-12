// SPDX-License-Identifier: MIT
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "zoo/sim/ZooSave.hpp"

namespace zoogame {

/// A saved game as the load and save windows list it.
struct SaveEntry {
    std::filesystem::path path;
    std::string title;  ///< the file name without ".zoo.json"
    zoo::SaveSummary summary;
    bool readable = false;
    std::string problem;  ///< why not, when it is not
    std::filesystem::file_time_type written;
};

/// Where saved games live: $XDG_DATA_HOME/zoo-simulator/saves, or
/// ~/.local/share/zoo-simulator/saves; another place after SetSaveDirectory().
std::filesystem::path SaveDirectory();
void SetSaveDirectory(std::filesystem::path directory);

/// The saves in the directory, the newest first.
std::vector<SaveEntry> ListSaves();
/// The file a save called `title` goes to (the title made safe as a file name).
std::filesystem::path SavePath(std::string_view title);
/// `title` with what a file name cannot hold replaced; never empty.
std::string SafeTitle(std::string_view title);

inline constexpr std::string_view kSaveSuffix = ".zoo.json";
inline constexpr std::string_view kAutosaveTitle = "autosave";

}  // namespace zoogame
