// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <vector>

#include "MeshWriter.hpp"

namespace zoogame {

/// The printed things of the zoo. They share one sheet (one texture, one material, one draw
/// call for all of them): the name boards of the buildings, the plates on gates and
/// barriers, the panels of the information boards.
enum class Sign : std::uint8_t {
    Zoo,      ///< the board over the entrance (4:1)
    Menu,     ///< a stall's price board (4:3)
    Toilets,  ///< name boards (4:1) ...
    GiftShop,
    Drinks,
    IceCream,
    Burgers,
    Education,
    Research,
    Vet,
    Staff,  ///< plates (2:1) ...
    StaffOnly,
    Danger,
    DoNotFeed,
    Board0,  ///< panels of information boards (4:3): a heading, a picture, a map, lines of
    Board1,  ///< print too small to read from where one stands. They name no species: the
    Board2,  ///< board's own text is what the visitor reads on stepping up to it.
    Board3,
    Count
};

/// A square picture, sRGB bytes with alpha 255, row-major, the top row first.
struct SignSheet {
    int size = 0;
    std::vector<std::uint8_t> rgba;
};

/// Paints the sheet. The lettering is drawn here (a stroke alphabet of capitals), so the
/// signs need no font file and say the same on every machine.
SignSheet PaintSigns();

/// Width over height of a sign's face.
float SignAspect(Sign sign);

/// Writes a sign's face: a and b its lower corners, c and d the upper ones, seen from the side
/// they wind counter-clockwise from. A plate of its own behind it is the caller's affair.
void WriteSign(ModelBuilder& model, Sign sign, const Vector3& a, const Vector3& b, const Vector3& c,
               const Vector3& d);

}  // namespace zoogame
