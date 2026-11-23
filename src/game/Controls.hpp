// SPDX-License-Identifier: MIT
#pragma once

#include "CnaEngine/Input/InputSnapshot.hpp"
#include "Microsoft/Xna/Framework/Input/ButtonState.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"

#include "Common.hpp"

namespace zoogame {

using Microsoft::Xna::Framework::Input::Keys;

enum class MouseKey { Left, Right, Middle };

/// The input of one frame with what changed since the frame before.
class Controls {
public:
    void Begin(const CnaEngine::Input::InputSnapshot& snapshot) {
        before_ = now_;
        now_ = snapshot;
        if (!primed_) {
            before_ = now_;
            primed_ = true;
        }
        consumed_ = false;
        typing_ = false;
    }

    bool Down(Keys key) const { return now_.keyboard.IsKeyDown(key); }
    bool Pressed(Keys key) const { return now_.keyboard.IsKeyDown(key) && !before_.keyboard.IsKeyDown(key); }
    bool Released(Keys key) const { return !now_.keyboard.IsKeyDown(key) && before_.keyboard.IsKeyDown(key); }
    bool Shift() const { return Down(Keys::LeftShift) || Down(Keys::RightShift); }
    bool Control() const { return Down(Keys::LeftControl) || Down(Keys::RightControl); }

    bool Down(MouseKey key) const { return IsDown(now_, key); }
    bool Pressed(MouseKey key) const { return IsDown(now_, key) && !IsDown(before_, key); }
    bool Released(MouseKey key) const { return !IsDown(now_, key) && IsDown(before_, key); }

    Vector2 Mouse() const {
        return Vector2(static_cast<float>(now_.mouse.getXProperty()),
                       static_cast<float>(now_.mouse.getYProperty()));
    }
    Vector2 MouseMove() const {
        return Vector2(static_cast<float>(now_.mouse.getXProperty() - before_.mouse.getXProperty()),
                       static_cast<float>(now_.mouse.getYProperty() - before_.mouse.getYProperty()));
    }
    /// The pointer's movement while a button is held (nothing on the frame it goes down).
    Vector2 Drag(MouseKey key) const {
        return IsDown(now_, key) && IsDown(before_, key) ? MouseMove() : Vector2(0.0f, 0.0f);
    }
    /// Wheel movement in notches, positive away from the user.
    float Wheel() const {
        return static_cast<float>(now_.mouse.getScrollWheelValueProperty() -
                                  before_.mouse.getScrollWheelValueProperty()) /
               120.0f;
    }

    /// The interface marks the pointer as its own for this frame: the world
    /// below must not react to clicks or the wheel.
    void ConsumePointer() { consumed_ = true; }
    bool PointerFree() const { return !consumed_; }
    /// A text field has the keyboard: letter keys are not commands this frame.
    void ConsumeKeyboard() { typing_ = true; }
    bool KeyboardFree() const { return !typing_; }

private:
    static bool IsDown(const CnaEngine::Input::InputSnapshot& s, MouseKey key) {
        using Microsoft::Xna::Framework::Input::ButtonState;
        switch (key) {
            case MouseKey::Left:
                return s.mouse.getLeftButtonProperty() == ButtonState::Pressed;
            case MouseKey::Right:
                return s.mouse.getRightButtonProperty() == ButtonState::Pressed;
            case MouseKey::Middle:
                return s.mouse.getMiddleButtonProperty() == ButtonState::Pressed;
        }
        return false;
    }

    CnaEngine::Input::InputSnapshot now_;
    CnaEngine::Input::InputSnapshot before_;
    bool primed_ = false;
    bool consumed_ = false;
    bool typing_ = false;
};

}  // namespace zoogame
