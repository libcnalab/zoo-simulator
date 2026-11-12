// SPDX-License-Identifier: MIT
#pragma once

namespace zoogame {

/// Runs the animal viewer instead of the game. Arguments: --animal <name>,
/// --clip <name>, and the engine's own (--frames, --screenshot, ...).
int RunAnimalViewer(int argc, char** argv);

}  // namespace zoogame
