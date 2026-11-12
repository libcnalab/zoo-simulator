// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <cstring>
#include <vector>

#include "AnimalViewer.hpp"
#include "ZooGame.hpp"

int main(int argc, char** argv) {
    // "--viewer" as the first argument starts the animal viewer instead.
    if (argc > 1 && std::strcmp(argv[1], "--viewer") == 0) {
        std::vector<char*> rest{argv[0]};
        rest.insert(rest.end(), argv + 2, argv + argc);
        return zoogame::RunAnimalViewer(static_cast<int>(rest.size()), rest.data());
    }
    // The game uses CNA's gamer services for its avatars only: their Guide
    // (sign-in, friends) has no place in a zoo and must not open on a key.
    setenv("CNA_GAMER_SERVICES_GUIDE_BUTTON", "0", 0);
    return CnaEngine::App::RunGame<zoogame::ZooGame>(argc, argv);
}
