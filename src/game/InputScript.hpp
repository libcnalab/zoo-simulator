// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "CnaEngine/Input/InputSnapshot.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"

namespace zoogame {

/// Scripted input for automated runs (--script FILE): what the mouse and the
/// keyboard do on which frame, and which frames are saved as screenshots. With
/// --lockstep a script plays back identically every time, which is how the
/// interface and the building tools are tested without a person at the mouse.
///
/// One command a line, `#` starts a comment; coordinates are window pixels:
///
///     FRAME move X Y
///     FRAME down [left|right|middle]      FRAME up [left|right|middle]
///     FRAME click X Y                     (press on FRAME, release on the next)
///     FRAME rclick X Y
///     FRAME drag X1 Y1 X2 Y2 [FRAMES]     (press, move over FRAMES frames, release)
///     FRAME key NAME                      (down on FRAME, up on the next)
///     FRAME hold NAME                     FRAME release NAME
///     FRAME wheel NOTCHES
///     FRAME look DX DY [FRAMES]           (mouse look of a captured mouse, in pixels)
///     FRAME shot PATH.png
///
/// Key names: A..Z, 0..9, Escape, Space, Enter, Shift, Control, Tab, Back,
/// Left, Right, Up, Down, F1..F10.
class InputScript {
public:
    /// nullopt (and `error`) when the file is missing or a line does not parse.
    static std::optional<InputScript> Load(const std::string& path, std::string& error);

    /// The input of the next frame (frames count from 1). Screenshots the script
    /// asks for on this frame are appended to `shots`.
    CnaEngine::Input::InputSnapshot Next(std::vector<std::string>& shots);
    int LastFrame() const { return lastFrame_; }
    /// How far the captured mouse moved on the frame Next() last returned (`look`).
    Microsoft::Xna::Framework::Vector2 Look() const { return look_; }

private:
    enum class What { Move, Down, Up, KeyDown, KeyUp, Wheel, Look, Shot };
    struct Step {
        int frame = 0;
        What what = What::Move;
        int x = 0;
        int y = 0;  ///< Move: position; Down/Up: x = button; Wheel: x = notches
        Microsoft::Xna::Framework::Input::Keys key{};
        std::string path;
    };
    void Add(Step step);

    std::vector<Step> steps_;  // ordered by frame
    std::size_t next_ = 0;
    int frame_ = 0;
    int lastFrame_ = 0;
    int x_ = 0;
    int y_ = 0;
    int wheel_ = 0;
    Microsoft::Xna::Framework::Vector2 look_{0.0f, 0.0f};
    bool buttons_[3] = {false, false, false};
    std::unordered_set<Microsoft::Xna::Framework::Input::Keys> keys_;
};

}  // namespace zoogame
