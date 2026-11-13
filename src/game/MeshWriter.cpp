// SPDX-License-Identifier: MIT
#include "MeshWriter.hpp"

#include <cmath>
#include <numbers>

#include "zoo/base/Rng.hpp"

namespace zoogame {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

/// Two unit vectors perpendicular to `axis` and to each other, with
/// cross(side, up) == axis. For an axis that is not vertical `side` is horizontal.
void Basis(const Vector3& axis, Vector3& side, Vector3& up) {
    if (std::abs(axis.Y) > 0.99f) {
        side = Vector3(1.0f, 0.0f, 0.0f);
    } else {
        side = Vector3::Normalize(Vector3::Cross(Vector3(0.0f, 1.0f, 0.0f), axis));
    }
    up = Vector3::Cross(axis, side);
}

}  // namespace

void MeshWriter::SetTransform(const Matrix& transform) {
    placement_ = transform;
    placed_ = true;
    Combine();
}

void MeshWriter::ClearTransform() {
    placed_ = false;
    Combine();
}

void MeshWriter::SetLocal(const Matrix& local) {
    local_ = local;
    localized_ = true;
    Combine();
}

void MeshWriter::ClearLocal() {
    localized_ = false;
    Combine();
}

void MeshWriter::Combine() {
    transformed_ = placed_ || localized_;
    if (placed_ && localized_) {
        transform_ = local_ * placement_;
    } else if (placed_) {
        transform_ = placement_;
    } else if (localized_) {
        transform_ = local_;
    }
}

std::uint32_t MeshWriter::Vertex(const Vector3& position, const Vector3& normal, float u, float v) {
    const auto index = static_cast<std::uint32_t>(mesh.positions.size());
    if (transformed_) {
        mesh.positions.push_back(Vector3::Transform(position, transform_));
        mesh.normals.push_back(Vector3::Normalize(Vector3::TransformNormal(normal, transform_)));
    } else {
        mesh.positions.push_back(position);
        mesh.normals.push_back(normal);
    }
    mesh.texCoords0.emplace_back(u, v);
    return index;
}

void MeshWriter::Face(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    // The engine's front faces are clockwise.
    mesh.indices.push_back(a);
    mesh.indices.push_back(c);
    mesh.indices.push_back(b);
}

void MeshWriter::Tri(const Vector3& a, const Vector3& b, const Vector3& c) {
    const Vector3 ab = b - a;
    const Vector3 ac = c - a;
    const Vector3 cross = Vector3::Cross(ab, ac);
    const float area = cross.Length();
    if (area < 1e-9f) {
        return;
    }
    const Vector3 normal = cross / area;
    const float lengthU = ab.Length();
    const Vector3 unitU = ab / lengthU;
    const float cu = Vector3::Dot(ac, unitU);
    const float cv = (ac - unitU * cu).Length();
    const std::uint32_t ia = Vertex(a, normal, 0.0f, 0.0f);
    const std::uint32_t ib = Vertex(b, normal, lengthU, 0.0f);
    const std::uint32_t ic = Vertex(c, normal, cu, cv);
    Face(ia, ib, ic);
}

void MeshWriter::Quad(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d) {
    const Vector3 ab = b - a;
    const Vector3 ad = d - a;
    const Vector3 cross = Vector3::Cross(ab, ad);
    const float area = cross.Length();
    if (area < 1e-9f) {
        return;
    }
    const Vector3 normal = cross / area;
    const float lengthU = ab.Length();
    const float lengthV = ad.Length();
    const std::uint32_t ia = Vertex(a, normal, 0.0f, 0.0f);
    const std::uint32_t ib = Vertex(b, normal, lengthU, 0.0f);
    const std::uint32_t ic = Vertex(c, normal, lengthU, lengthV);
    const std::uint32_t id = Vertex(d, normal, 0.0f, lengthV);
    Face(ia, ib, ic);
    Face(ia, ic, id);
}

void MeshWriter::QuadTwoSided(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d) {
    Quad(a, b, c, d);
    Quad(b, a, d, c);
}

void MeshWriter::Card(const Vector3& center, const Vector3& halfU, const Vector3& halfV, const Vector3& normal,
                      float u0, float v0, float u1, float v1) {
    const std::uint32_t a = Vertex(center - halfU + halfV, normal, u0, v0);
    const std::uint32_t b = Vertex(center + halfU + halfV, normal, u1, v0);
    const std::uint32_t c = Vertex(center + halfU - halfV, normal, u1, v1);
    const std::uint32_t d = Vertex(center - halfU - halfV, normal, u0, v1);
    Face(a, b, c);
    Face(a, c, d);
    Face(a, c, b);
    Face(a, d, c);
}

void MeshWriter::Box(const Vector3& min, const Vector3& max) {
    const float x0 = min.X, y0 = min.Y, z0 = min.Z;
    const float x1 = max.X, y1 = max.Y, z1 = max.Z;
    Quad({x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1});  // +x
    Quad({x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0});  // -x
    Quad({x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1});  // +z
    Quad({x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0});  // -z
    Quad({x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0});  // +y
    Quad({x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1});  // -y
}

void MeshWriter::BoxAt(float x, float z, float base, float sizeX, float sizeY, float sizeZ, float yaw) {
    const bool wasTransformed = transformed_;
    const Matrix previous = transform_;
    Matrix local = Matrix::CreateRotationY(yaw) * Matrix::CreateTranslation(x, base, z);
    if (wasTransformed) {
        local = local * previous;
    }
    transform_ = local;
    transformed_ = true;
    Box({-sizeX * 0.5f, 0.0f, -sizeZ * 0.5f}, {sizeX * 0.5f, sizeY, sizeZ * 0.5f});
    transform_ = previous;
    transformed_ = wasTransformed;
}

void MeshWriter::Beam(const Vector3& from, const Vector3& to, float width, float height) {
    const Vector3 along = to - from;
    const float length = along.Length();
    if (length < 1e-6f) {
        return;
    }
    Vector3 side;
    Vector3 up;
    Basis(along / length, side, up);
    const Vector3 s = side * (width * 0.5f);
    const Vector3 t = up * (height * 0.5f);
    Quad(from + s - t, from + s + t, to + s + t, to + s - t);
    Quad(from - s + t, from - s - t, to - s - t, to - s + t);
    Quad(from + s + t, from - s + t, to - s + t, to + s + t);
    Quad(from - s - t, from + s - t, to + s - t, to - s - t);
    Quad(to - s - t, to + s - t, to + s + t, to - s + t);
    Quad(from + s - t, from - s - t, from - s + t, from + s + t);
}

void MeshWriter::Tube(const Vector3& from, const Vector3& to, float radiusFrom, float radiusTo, int sides,
                      bool capFrom, bool capTo) {
    const Vector3 along = to - from;
    const float length = along.Length();
    if (length < 1e-6f || sides < 3) {
        return;
    }
    const Vector3 axis = along / length;
    Vector3 side;
    Vector3 up;
    Basis(axis, side, up);
    const float girth = 2.0f * kPi * std::max(radiusFrom, radiusTo);
    const float slope = (radiusFrom - radiusTo) / length;

    const auto first = static_cast<std::uint32_t>(mesh.positions.size());
    for (int i = 0; i <= sides; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(sides);
        const Vector3 radial = side * std::cos(angle) + up * std::sin(angle);
        const Vector3 normal = Vector3::Normalize(radial + axis * slope);
        const float u = girth * static_cast<float>(i) / static_cast<float>(sides);
        Vertex(from + radial * radiusFrom, normal, u, 0.0f);
        Vertex(to + radial * radiusTo, normal, u, length);
    }
    for (int i = 0; i < sides; ++i) {
        const std::uint32_t b0 = first + static_cast<std::uint32_t>(i) * 2;
        const std::uint32_t t0 = b0 + 1;
        const std::uint32_t b1 = b0 + 2;
        const std::uint32_t t1 = b0 + 3;
        if (radiusFrom > 1e-5f) {
            Face(b0, b1, t1);
        }
        if (radiusTo > 1e-5f) {
            Face(b0, t1, t0);
        }
    }
    const auto cap = [&](const Vector3& center, float radius, const Vector3& normal, bool towardsAxis) {
        const std::uint32_t c = Vertex(center, normal, 0.0f, 0.0f);
        const auto ring = static_cast<std::uint32_t>(mesh.positions.size());
        for (int i = 0; i <= sides; ++i) {
            const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(sides);
            const float cu = std::cos(angle) * radius;
            const float cv = std::sin(angle) * radius;
            Vertex(center + side * cu + up * cv, normal, cu, cv);
        }
        for (int i = 0; i < sides; ++i) {
            const std::uint32_t p0 = ring + static_cast<std::uint32_t>(i);
            if (towardsAxis) {
                Face(c, p0, p0 + 1);
            } else {
                Face(c, p0 + 1, p0);
            }
        }
    };
    if (capTo && radiusTo > 1e-5f) {
        cap(to, radiusTo, axis, true);
    }
    if (capFrom && radiusFrom > 1e-5f) {
        cap(from, radiusFrom, -axis, false);
    }
}

void MeshWriter::Disc(const Vector3& center, float radius, int sides) {
    const Vector3 normal(0.0f, 1.0f, 0.0f);
    const std::uint32_t c = Vertex(center, normal, 0.0f, 0.0f);
    const auto ring = static_cast<std::uint32_t>(mesh.positions.size());
    for (int i = 0; i <= sides; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(sides);
        const float dx = std::cos(angle) * radius;
        const float dz = std::sin(angle) * radius;
        Vertex(center + Vector3(dx, 0.0f, dz), normal, dx, dz);
    }
    for (int i = 0; i < sides; ++i) {
        const std::uint32_t p0 = ring + static_cast<std::uint32_t>(i);
        Face(c, p0 + 1, p0);
    }
}

void MeshWriter::Blob(const Vector3& center, const Vector3& radii, std::uint32_t seed, float lumpiness,
                      int segments, int rings, float flatBase) {
    if (segments < 3 || rings < 2) {
        return;
    }
    // Three random wave directions and phases make the lumps.
    zoo::Rng rng(seed, 77);
    Vector3 waves[3];
    float phases[3];
    for (int k = 0; k < 3; ++k) {
        waves[k] = Vector3::Normalize(
            Vector3(rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f), rng.Range(-1.0f, 1.0f)) +
            Vector3(0.01f, 0.0f, 0.0f));
        phases[k] = rng.Range(0.0f, 2.0f * kPi);
    }
    const auto lump = [&](const Vector3& dir) {
        return 0.5f * std::sin(Vector3::Dot(dir, waves[0]) * 3.1f + phases[0]) +
               0.3f * std::sin(Vector3::Dot(dir, waves[1]) * 5.3f + phases[1]) +
               0.2f * std::sin(Vector3::Dot(dir, waves[2]) * 7.9f + phases[2]);
    };

    const float phiMax = std::acos(std::clamp(-flatBase, -1.0f, 1.0f));
    const int columns = segments + 1;
    const int rows = rings + 1;
    std::vector<Vector3> points(static_cast<std::size_t>(columns * rows));
    std::vector<Vector3> normals(points.size(), Vector3(0.0f, 0.0f, 0.0f));
    const auto at = [columns](int row, int column) {
        return static_cast<std::size_t>(row * columns + column);
    };
    for (int j = 0; j < rows; ++j) {
        const float phi = phiMax * static_cast<float>(j) / static_cast<float>(rings);
        for (int i = 0; i < columns; ++i) {
            const float theta = 2.0f * kPi * static_cast<float>(i % segments) / static_cast<float>(segments);
            const Vector3 dir(std::sin(phi) * std::cos(theta), std::cos(phi),
                              std::sin(phi) * std::sin(theta));
            const float factor = 1.0f + lumpiness * lump(dir);
            points[at(j, i)] = center + radii * dir * factor;
        }
    }
    for (int j = 0; j < rings; ++j) {
        for (int i = 0; i < segments; ++i) {
            const Vector3& a = points[at(j, i)];
            const Vector3& b = points[at(j, i + 1)];
            const Vector3& c = points[at(j + 1, i + 1)];
            const Vector3& d = points[at(j + 1, i)];
            const Vector3 n1 = Vector3::Cross(b - a, c - a);
            const Vector3 n2 = Vector3::Cross(c - a, d - a);
            normals[at(j, i)] += n1 + n2;
            normals[at(j, i + 1)] += n1;
            normals[at(j + 1, i + 1)] += n1 + n2;
            normals[at(j + 1, i)] += n2;
        }
    }
    for (int j = 0; j < rows; ++j) {
        // The seam column shares its normal with column 0.
        const Vector3 seam = normals[at(j, 0)] + normals[at(j, segments)];
        normals[at(j, 0)] = seam;
        normals[at(j, segments)] = seam;
    }
    // Poles: every vertex of the ring is the same point.
    Vector3 top(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < columns; ++i) {
        top += normals[at(0, i)];
    }
    for (int i = 0; i < columns; ++i) {
        normals[at(0, i)] = top;
    }
    if (flatBase >= 1.0f) {
        Vector3 bottom(0.0f, 0.0f, 0.0f);
        for (int i = 0; i < columns; ++i) {
            bottom += normals[at(rings, i)];
        }
        for (int i = 0; i < columns; ++i) {
            normals[at(rings, i)] = bottom;
        }
    }

    const float radiusXZ = std::max(radii.X, radii.Z);
    const auto first = static_cast<std::uint32_t>(mesh.positions.size());
    for (int j = 0; j < rows; ++j) {
        for (int i = 0; i < columns; ++i) {
            Vector3 normal = normals[at(j, i)];
            const float length = normal.Length();
            normal = length > 1e-9f ? normal / length : Vector3(0.0f, 1.0f, 0.0f);
            const float u = 2.0f * kPi * radiusXZ * static_cast<float>(i) / static_cast<float>(segments);
            const float v = phiMax * radii.Y * static_cast<float>(j) / static_cast<float>(rings);
            Vertex(points[at(j, i)], normal, u, v);
        }
    }
    for (int j = 0; j < rings; ++j) {
        for (int i = 0; i < segments; ++i) {
            const std::uint32_t a = first + static_cast<std::uint32_t>(at(j, i));
            const std::uint32_t b = first + static_cast<std::uint32_t>(at(j, i + 1));
            const std::uint32_t c = first + static_cast<std::uint32_t>(at(j + 1, i + 1));
            const std::uint32_t d = first + static_cast<std::uint32_t>(at(j + 1, i));
            if (j > 0) {
                Face(a, b, c);
            }
            if (j + 1 < rings || flatBase < 1.0f) {
                Face(a, c, d);
            }
        }
    }
}

void MeshWriter::GableRoof(float minX, float maxX, float minZ, float maxZ, float eavesY, float ridgeY,
                           bool ridgeAlongX, float thickness) {
    const float e = eavesY;
    const float r = ridgeY;
    if (ridgeAlongX) {
        const float mid = (minZ + maxZ) * 0.5f;
        Quad({minX, e, maxZ}, {maxX, e, maxZ}, {maxX, r, mid}, {minX, r, mid});
        Quad({maxX, e, minZ}, {minX, e, minZ}, {minX, r, mid}, {maxX, r, mid});
    } else {
        const float mid = (minX + maxX) * 0.5f;
        Quad({maxX, e, maxZ}, {maxX, e, minZ}, {mid, r, minZ}, {mid, r, maxZ});
        Quad({minX, e, minZ}, {minX, e, maxZ}, {mid, r, maxZ}, {mid, r, minZ});
    }
    // The underside, so that the roof is closed seen from below.
    const float under = e - thickness;
    Quad({minX, under, minZ}, {maxX, under, minZ}, {maxX, under, maxZ}, {minX, under, maxZ});
    // The fascia around the eaves.
    Quad({maxX, under, maxZ}, {maxX, under, minZ}, {maxX, e, minZ}, {maxX, e, maxZ});
    Quad({minX, under, minZ}, {minX, under, maxZ}, {minX, e, maxZ}, {minX, e, minZ});
    Quad({minX, under, maxZ}, {maxX, under, maxZ}, {maxX, e, maxZ}, {minX, e, maxZ});
    Quad({maxX, under, minZ}, {minX, under, minZ}, {minX, e, minZ}, {maxX, e, minZ});
}

void MeshWriter::GableEnds(float minX, float maxX, float minZ, float maxZ, float eavesY, float ridgeY,
                           bool ridgeAlongX) {
    const float e = eavesY;
    const float r = ridgeY;
    if (ridgeAlongX) {
        const float mid = (minZ + maxZ) * 0.5f;
        Tri({maxX, e, maxZ}, {maxX, e, minZ}, {maxX, r, mid});
        Tri({minX, e, minZ}, {minX, e, maxZ}, {minX, r, mid});
    } else {
        const float mid = (minX + maxX) * 0.5f;
        Tri({minX, e, maxZ}, {maxX, e, maxZ}, {mid, r, maxZ});
        Tri({maxX, e, minZ}, {minX, e, minZ}, {mid, r, minZ});
    }
}

void MeshWriter::HipRoof(float minX, float maxX, float minZ, float maxZ, float eavesY, float topY) {
    const float e = eavesY;
    const Vector3 top((minX + maxX) * 0.5f, topY, (minZ + maxZ) * 0.5f);
    Tri({minX, e, maxZ}, {maxX, e, maxZ}, top);
    Tri({maxX, e, maxZ}, {maxX, e, minZ}, top);
    Tri({maxX, e, minZ}, {minX, e, minZ}, top);
    Tri({minX, e, minZ}, {minX, e, maxZ}, top);
    Quad({minX, e, minZ}, {maxX, e, minZ}, {maxX, e, maxZ}, {minX, e, maxZ});
}

MeshWriter& ModelBuilder::Use(const zoo::Color3& color, Pattern pattern) {
    const Finish finish{color, pattern};
    for (ModelPart& part : parts_) {
        if (part.finish == finish) {
            return part.writer;
        }
    }
    ModelPart& part = parts_.emplace_back();
    part.finish = finish;
    if (transformed_) {
        part.writer.SetTransform(transform_);
    }
    if (localized_) {
        part.writer.SetLocal(local_);
    }
    return part.writer;
}

void ModelBuilder::SetTransform(const Matrix& transform) {
    transform_ = transform;
    transformed_ = true;
    for (ModelPart& part : parts_) {
        part.writer.SetTransform(transform);
    }
}

void ModelBuilder::ClearTransform() {
    transformed_ = false;
    for (ModelPart& part : parts_) {
        part.writer.ClearTransform();
    }
}

void ModelBuilder::SetLocal(const Matrix& local) {
    local_ = local;
    localized_ = true;
    for (ModelPart& part : parts_) {
        part.writer.SetLocal(local);
    }
}

void ModelBuilder::ClearLocal() {
    localized_ = false;
    for (ModelPart& part : parts_) {
        part.writer.ClearLocal();
    }
}

}  // namespace zoogame
