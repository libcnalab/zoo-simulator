// SPDX-License-Identifier: MIT
#include "InputScript.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace zoogame {

namespace {

using Microsoft::Xna::Framework::Vector2;
using Microsoft::Xna::Framework::Input::ButtonState;
using Microsoft::Xna::Framework::Input::KeyboardState;
using Microsoft::Xna::Framework::Input::Keys;
using Microsoft::Xna::Framework::Input::MouseState;

bool ParseKey(const std::string& name, Keys& key) {
    if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z') {
        key = static_cast<Keys>(static_cast<int>(Keys::A) + (name[0] - 'A'));
        return true;
    }
    if (name.size() == 1 && name[0] >= '0' && name[0] <= '9') {
        key = static_cast<Keys>(static_cast<int>(Keys::D0) + (name[0] - '0'));
        return true;
    }
    static const struct {
        const char* name;
        Keys key;
    } kNamed[] = {
        {"Escape", Keys::Escape},   {"Space", Keys::Space}, {"Enter", Keys::Enter},
        {"Shift", Keys::LeftShift}, {"Tab", Keys::Tab},     {"Control", Keys::LeftControl},
        {"Back", Keys::Back},       {"Left", Keys::Left},   {"Right", Keys::Right},
        {"Up", Keys::Up},           {"Down", Keys::Down},   {"F1", Keys::F1},
        {"F2", Keys::F2},           {"F3", Keys::F3},       {"F4", Keys::F4},
        {"F5", Keys::F5},           {"F6", Keys::F6},       {"F7", Keys::F7},
        {"F8", Keys::F8},           {"F9", Keys::F9},       {"F10", Keys::F10},
    };
    for (const auto& named : kNamed) {
        if (name == named.name) {
            key = named.key;
            return true;
        }
    }
    return false;
}

int ParseButton(const std::string& name) {
    if (name.empty() || name == "left")
        return 0;
    if (name == "right")
        return 1;
    if (name == "middle")
        return 2;
    return -1;
}

}  // namespace

void InputScript::Add(Step step) {
    lastFrame_ = std::max(lastFrame_, step.frame);
    steps_.push_back(std::move(step));
}

std::optional<InputScript> InputScript::Load(const std::string& path, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "cannot read " + path;
        return std::nullopt;
    }
    InputScript script;
    std::string line;
    int number = 0;
    while (std::getline(file, line)) {
        ++number;
        if (const auto hash = line.find('#'); hash != std::string::npos)
            line.erase(hash);
        std::istringstream in(line);
        int frame = 0;
        std::string command;
        if (!(in >> frame))
            continue;  // blank
        const auto fail = [&](const char* why) {
            error = path + ":" + std::to_string(number) + ": " + why;
            return std::nullopt;
        };
        if (!(in >> command) || frame < 1)
            return fail("expected FRAME COMMAND");
        if (command == "move" || command == "click" || command == "rclick") {
            int x = 0;
            int y = 0;
            if (!(in >> x >> y))
                return fail("expected X Y");
            script.Add({frame, What::Move, x, y, {}, {}});
            if (command != "move") {
                const int button = command == "click" ? 0 : 1;
                script.Add({frame, What::Down, button, 0, {}, {}});
                script.Add({frame + 1, What::Up, button, 0, {}, {}});
            }
        } else if (command == "drag") {
            int x1 = 0, y1 = 0, x2 = 0, y2 = 0, frames = 4;
            if (!(in >> x1 >> y1 >> x2 >> y2))
                return fail("expected X1 Y1 X2 Y2");
            in >> frames;
            frames = std::max(frames, 1);
            script.Add({frame, What::Move, x1, y1, {}, {}});
            script.Add({frame, What::Down, 0, 0, {}, {}});
            for (int i = 1; i <= frames; ++i)
                script.Add({frame + i,
                            What::Move,
                            x1 + (x2 - x1) * i / frames,
                            y1 + (y2 - y1) * i / frames,
                            {},
                            {}});
            script.Add({frame + frames + 1, What::Up, 0, 0, {}, {}});
        } else if (command == "down" || command == "up") {
            std::string name;
            in >> name;
            const int button = ParseButton(name);
            if (button < 0)
                return fail("unknown mouse button");
            script.Add({frame, command == "down" ? What::Down : What::Up, button, 0, {}, {}});
        } else if (command == "key" || command == "hold" || command == "release") {
            std::string name;
            Keys key{};
            if (!(in >> name) || !ParseKey(name, key))
                return fail("unknown key");
            if (command != "release")
                script.Add({frame, What::KeyDown, 0, 0, key, {}});
            if (command == "key")
                script.Add({frame + 1, What::KeyUp, 0, 0, key, {}});
            if (command == "release")
                script.Add({frame, What::KeyUp, 0, 0, key, {}});
        } else if (command == "wheel") {
            int notches = 0;
            if (!(in >> notches))
                return fail("expected NOTCHES");
            script.Add({frame, What::Wheel, notches, 0, {}, {}});
        } else if (command == "look") {
            // Mouse look: DX DY pixels in all, spread over FRAMES frames (1 when not given).
            int dx = 0, dy = 0, frames = 1;
            if (!(in >> dx >> dy))
                return fail("expected DX DY");
            in >> frames;
            frames = std::max(frames, 1);
            for (int i = 0; i < frames; ++i)
                script.Add({frame + i,
                            What::Look,
                            dx * (i + 1) / frames - dx * i / frames,
                            dy * (i + 1) / frames - dy * i / frames,
                            {},
                            {}});
        } else if (command == "shot") {
            std::string file_;
            if (!(in >> file_))
                return fail("expected PATH");
            script.Add({frame, What::Shot, 0, 0, {}, file_});
        } else {
            return fail("unknown command");
        }
    }
    std::stable_sort(script.steps_.begin(), script.steps_.end(),
                     [](const Step& a, const Step& b) { return a.frame < b.frame; });
    return script;
}

CnaEngine::Input::InputSnapshot InputScript::Next(std::vector<std::string>& shots) {
    ++frame_;
    look_ = Vector2(0.0f, 0.0f);
    for (; next_ < steps_.size() && steps_[next_].frame <= frame_; ++next_) {
        const Step& step = steps_[next_];
        switch (step.what) {
            case What::Move:
                x_ = step.x;
                y_ = step.y;
                break;
            case What::Down:
                buttons_[step.x] = true;
                break;
            case What::Up:
                buttons_[step.x] = false;
                break;
            case What::KeyDown:
                keys_.insert(step.key);
                break;
            case What::KeyUp:
                keys_.erase(step.key);
                break;
            case What::Wheel:
                wheel_ += step.x * 120;
                break;
            case What::Look:
                look_ = look_ + Vector2(static_cast<float>(step.x), static_cast<float>(step.y));
                break;
            case What::Shot:
                shots.push_back(step.path);
                break;
        }
    }
    const auto state = [](bool down) { return down ? ButtonState::Pressed : ButtonState::Released; };
    CnaEngine::Input::InputSnapshot snapshot;
    snapshot.keyboard = KeyboardState(keys_);
    snapshot.mouse = MouseState(x_, y_, wheel_, state(buttons_[0]), state(buttons_[2]), state(buttons_[1]),
                                ButtonState::Released, ButtonState::Released);
    return snapshot;
}

}  // namespace zoogame
