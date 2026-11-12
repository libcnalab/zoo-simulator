// SPDX-License-Identifier: MIT
#include "Saves.hpp"

#include <algorithm>
#include <cstdlib>
#include <system_error>

namespace zoogame {

namespace {

std::filesystem::path& Override() {
    static std::filesystem::path directory;
    return directory;
}

}  // namespace

std::filesystem::path SaveDirectory() {
    if (!Override().empty())
        return Override();
    if (const char* data = std::getenv("XDG_DATA_HOME"); data != nullptr && *data != '\0')
        return std::filesystem::path(data) / "zoo-simulator" / "saves";
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0')
        return std::filesystem::path(home) / ".local" / "share" / "zoo-simulator" / "saves";
    return std::filesystem::path("saves");
}

void SetSaveDirectory(std::filesystem::path directory) {
    Override() = std::move(directory);
}

std::string SafeTitle(std::string_view title) {
    std::string safe;
    for (const char c : title) {
        const auto u = static_cast<unsigned char>(c);
        // Letters of any script (UTF-8 bytes) pass; what a file system may choke on does not.
        const bool bad = u < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
                         c == '<' || c == '>' || c == '|';
        safe.push_back(bad ? '_' : c);
    }
    while (!safe.empty() && (safe.back() == ' ' || safe.back() == '.'))
        safe.pop_back();
    while (!safe.empty() && (safe.front() == ' ' || safe.front() == '.'))
        safe.erase(safe.begin());
    return safe.empty() ? std::string("zoo") : safe;
}

std::filesystem::path SavePath(std::string_view title) {
    return SaveDirectory() / (SafeTitle(title) + std::string(kSaveSuffix));
}

std::vector<SaveEntry> ListSaves() {
    std::vector<SaveEntry> saves;
    std::error_code failed;
    for (std::filesystem::directory_iterator it(SaveDirectory(), failed), end; !failed && it != end;
         it.increment(failed)) {
        const std::string name = it->path().filename().string();
        if (!it->is_regular_file(failed) || name.size() <= kSaveSuffix.size() || !name.ends_with(kSaveSuffix))
            continue;
        SaveEntry entry;
        entry.path = it->path();
        entry.title = name.substr(0, name.size() - kSaveSuffix.size());
        entry.written = it->last_write_time(failed);
        entry.readable = zoo::ReadSaveSummary(entry.path.string(), entry.summary, &entry.problem);
        saves.push_back(std::move(entry));
    }
    std::sort(saves.begin(), saves.end(),
              [](const SaveEntry& a, const SaveEntry& b) { return a.written > b.written; });
    return saves;
}

}  // namespace zoogame
