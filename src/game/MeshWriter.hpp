// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <deque>
#include <vector>

#include "CnaEngine/Graphics/MeshData.hpp"
#include "Common.hpp"

namespace zoogame {

/// The procedural surface patterns. Each is a tileable greyscale texture that a
/// material tints with its colour (Surfaces).
enum class Pattern : std::uint8_t {
    Plain,
    Wood,    ///< sawn timber: grain along v
    Planks,  ///< boards with gaps, along v
    Bark,
    Leaves,
    Needles,
    Stone,
    Brick,
    Plaster,
    Thatch,
    RoofTiles,
    Metal,
    Concrete,
    WireMesh,  ///< see-through chain-link (drawn blended, both sides)
    Glass,     ///< clear glass (drawn blended, both sides; the colour is ignored)
    Pane,         ///< a glazed viewing panel: clear with streaks and a dusty foot (MeshWriter::Picture, 0..1)
    WeldMesh,     ///< see-through welded mesh of square cells (drawn blended, both sides)
    Signs,        ///< the sheet of printed signs (SignTextures; MeshWriter::Picture with a sign's corner)
    Sheeting,     ///< corrugated roofing sheet: ribs along v
    Rock,         ///< bedded rock: strata across v, lichen
    LeafSpray,    ///< a twig's worth of leaves, cut out (MeshWriter::Card; texture coordinates 0..1)
    NeedleSpray,  ///< a conifer bough, cut out (the same)
    Count
};

/// What a piece of geometry is made of.
struct Finish {
    zoo::Color3 color{0.5f, 0.5f, 0.5f};
    Pattern pattern = Pattern::Plain;

    bool operator==(const Finish&) const = default;
};

/// Writes primitives into a MeshData with positions, normals and texture
/// coordinates. Texture coordinates are in metres along the surface, so that
/// patterns keep their scale on every object (materials set the scale).
///
/// Faces are given counter-clockwise as seen from outside, y up. Two optional
/// transforms apply to everything written while they are set: the local one
/// first (a part within a model: a tilted desk, one wall of a house), then the
/// placement (the model within the world).
class MeshWriter {
public:
    CnaEngine::Graphics::MeshData mesh;

    void SetTransform(const Matrix& transform);
    void ClearTransform();
    void SetLocal(const Matrix& local);
    void ClearLocal();

    /// Flat triangle.
    void Tri(const Vector3& a, const Vector3& b, const Vector3& c);
    /// Flat quad; u runs from a to b, v from a to d.
    void Quad(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d);
    /// The same quad visible from both sides.
    void QuadTwoSided(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d);
    /// A flat card seen from both sides and shaded with one given normal on both (a spray of
    /// leaves takes the light of the crown it belongs to, not of its own plane). `halfU` and
    /// `halfV` run from the centre to the middle of two edges; the texture coordinates are
    /// given as they are, (u0, v0) at centre - halfU + halfV (the top left of the picture).
    void Card(const Vector3& center, const Vector3& halfU, const Vector3& halfV, const Vector3& normal,
              float u0, float v0, float u1, float v1);
    /// Axis-aligned box.
    void Box(const Vector3& min, const Vector3& max);
    /// A box standing on the ground plane y = `base`, centred on (x, z), turned by `yaw`.
    void BoxAt(float x, float z, float base, float sizeX, float sizeY, float sizeZ, float yaw = 0.0f);
    /// A rectangular beam from `from` to `to`: `width` across (horizontally for a
    /// beam that is not vertical), `height` the other way.
    void Beam(const Vector3& from, const Vector3& to, float width, float height);
    /// A round, possibly tapering tube with smooth sides.
    void Tube(const Vector3& from, const Vector3& to, float radiusFrom, float radiusTo, int sides = 8,
              bool capFrom = false, bool capTo = true);
    /// A horizontal disc facing up.
    void Disc(const Vector3& center, float radius, int sides = 12);
    /// A lumpy ellipsoid with smooth normals: tree crowns, bushes, boulders.
    /// `lumpiness` 0 gives a clean ellipsoid, 0.3 a rugged one. `flatBase` cuts
    /// the lower part off at that fraction of the vertical radius below the centre
    /// (1 = whole ellipsoid).
    void Blob(const Vector3& center, const Vector3& radii, std::uint32_t seed, float lumpiness = 0.18f,
              int segments = 9, int rings = 6, float flatBase = 1.0f);
    /// A gable roof over the rectangle [minX, maxX] x [minZ, maxZ] (already
    /// including any overhang): eaves at `eavesY`, ridge at `ridgeY` running along
    /// x when `ridgeAlongX`. Writes the two slopes only; see GableEnds.
    void GableRoof(float minX, float maxX, float minZ, float maxZ, float eavesY, float ridgeY,
                   bool ridgeAlongX, float thickness = 0.12f);
    /// The two triangular wall pieces under a gable roof.
    void GableEnds(float minX, float maxX, float minZ, float maxZ, float eavesY, float ridgeY,
                   bool ridgeAlongX);
    /// A four-sided pyramid (hip) roof.
    void HipRoof(float minX, float maxX, float minZ, float maxZ, float eavesY, float topY);
    /// A flat quad that shows a piece of a picture, seen from one side: a and b are the lower
    /// corners, c and d the upper ones (counter-clockwise from outside); (u0, v0) is the picture's
    /// corner at d and (u1, v1) the one at b, as they are (no scale in metres).
    void Picture(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, float u0, float v0,
                 float u1, float v1);

    bool Empty() const { return mesh.indices.empty(); }

private:
    std::uint32_t Vertex(const Vector3& position, const Vector3& normal, float u, float v);
    void Face(std::uint32_t a, std::uint32_t b, std::uint32_t c);

    void Combine();

    Matrix placement_;
    Matrix local_;
    Matrix transform_;
    bool placed_ = false;
    bool localized_ = false;
    bool transformed_ = false;
};

/// One material's share of a model.
struct ModelPart {
    Finish finish;
    MeshWriter writer;
};

/// A model under construction: geometry sorted by finish.
class ModelBuilder {
public:
    /// The writer for a finish (created on first use). The reference stays valid.
    MeshWriter& Use(const zoo::Color3& color, Pattern pattern);
    /// Sets (or clears) the transform of every part, present and future.
    void SetTransform(const Matrix& transform);
    void ClearTransform();
    /// The same for the local transform, which applies before the transform.
    void SetLocal(const Matrix& local);
    void ClearLocal();

    std::deque<ModelPart>& Parts() { return parts_; }
    const std::deque<ModelPart>& Parts() const { return parts_; }

private:
    std::deque<ModelPart> parts_;
    Matrix transform_;
    Matrix local_;
    bool transformed_ = false;
    bool localized_ = false;
};

}  // namespace zoogame
