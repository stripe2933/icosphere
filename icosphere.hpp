//
// Created by gomkyung2 on 2023/09/09.
//

#pragma once

#include <array>
#include <vector>
#include <unordered_map>

#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>

struct Triangle{
    glm::vec3 p1, p2, p3;

    [[nodiscard]] constexpr glm::vec3 normal() const noexcept{
        return glm::normalize(glm::cross(p2 - p1, p3 - p1));
    }
};

template <typename IndexType>
struct Mesh{
    using triangle_index_t = std::array<IndexType, 3>;
    using positions_t = std::vector<glm::vec3>;
    using triangle_indices_t = std::vector<triangle_index_t>;

    positions_t positions;
    triangle_indices_t triangle_indices;

    constexpr std::vector<Triangle> getTriangles() const noexcept{
        std::vector<Triangle> triangles;
        triangles.reserve(triangle_indices.size());

        std::ranges::transform(
            triangle_indices,
            std::back_inserter(triangles),
            [&](const triangle_index_t &indices) -> Triangle {
                const auto [i1, i2, i3] = indices;
                return { positions[i1], positions[i2], positions[i3] };
            }
        );

        return triangles;
    }
};

template <typename IndexType>
class Icosphere{
private:
    using mesh_t = Mesh<IndexType>;
    using triangle_index_t = typename mesh_t::triangle_index_t;

    struct pair_hash{
        constexpr std::size_t operator()(std::pair<IndexType, IndexType> pair) const noexcept{
            // Boost hash_combine.
            // https://stackoverflow.com/questions/35985960/c-why-is-boosthash-combine-the-best-way-to-combine-hash-values

            std::size_t seed = std::hash<IndexType>{}(pair.first);
            seed ^= std::hash<IndexType>{}(pair.second) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    static constexpr std::array<glm::vec3, 12> subdivision_0_positions {
        glm::vec3 {  0.0000000e+00,  0.0000000e+00,  1.0000000e+00 },
        glm::vec3 {  8.9442718e-01,  0.0000000e+00,  4.4721359e-01 },
        glm::vec3 {  2.7639320e-01,  8.5065079e-01,  4.4721359e-01 },
        glm::vec3 { -7.2360682e-01,  5.2573109e-01,  4.4721359e-01 },
        glm::vec3 { -7.2360682e-01, -5.2573109e-01,  4.4721359e-01 },
        glm::vec3 {  2.7639320e-01, -8.5065079e-01,  4.4721359e-01 },
        glm::vec3 {  7.2360682e-01,  5.2573109e-01, -4.4721359e-01 },
        glm::vec3 { -2.7639320e-01,  8.5065079e-01, -4.4721359e-01 },
        glm::vec3 { -8.9442718e-01,  1.0953574e-16, -4.4721359e-01 },
        glm::vec3 { -2.7639320e-01, -8.5065079e-01, -4.4721359e-01 },
        glm::vec3 {  7.2360682e-01, -5.2573109e-01, -4.4721359e-01 },
        glm::vec3 {  0.0000000e+00,  0.0000000e+00, -1.0000000e+00 }
    };

    static constexpr std::array<triangle_index_t, 20> subdivision_0_indices {
        triangle_index_t {  0,  1,  2 },
        triangle_index_t {  0,  2,  3 },
        triangle_index_t {  0,  3,  4 },
        triangle_index_t {  0,  4,  5 },
        triangle_index_t {  0,  5,  1 },
        triangle_index_t {  1,  6,  2 },
        triangle_index_t {  2,  7,  3 },
        triangle_index_t {  3,  8,  4 },
        triangle_index_t {  4,  9,  5 },
        triangle_index_t {  5, 10,  1 },
        triangle_index_t {  2,  6,  7 },
        triangle_index_t {  3,  7,  8 },
        triangle_index_t {  4,  8,  9 },
        triangle_index_t {  5,  9, 10 },
        triangle_index_t {  1, 10,  6 },
        triangle_index_t {  6, 11,  7 },
        triangle_index_t {  7, 11,  8 },
        triangle_index_t {  8, 11,  9 },
        triangle_index_t {  9, 11, 10 },
        triangle_index_t { 10, 11,  6 }
    };

    static constexpr std::pair<IndexType, IndexType> make_ascending_pair(IndexType idx1, IndexType idx2) noexcept{
        return idx1 < idx2 ? std::make_pair(idx1, idx2) : std::make_pair(idx2, idx1);
    }

public:
    static mesh_t generate(std::uint8_t level) noexcept{
        if (level == 0){
            return { .positions = { subdivision_0_positions.cbegin(), subdivision_0_positions.cend() },
                     .triangle_indices = { subdivision_0_indices.cbegin(), subdivision_0_indices.cend() } };
        }

        const auto [previous_positions, previous_triangle_indices] = generate(level - 1);

        /*
         * The newly generated positions are the midpoints of the three sides of the existing triangle, and therefore,
         * three new coordinates are created for each triangle. However, since the midpoint of a side shared by two
         * neighboring triangles is calculated twice, the final number of generated coordinates is (the number of
         * existing triangles) * 3 / 2. The total number of coordinates is then
         *     (the number of existing triangles) * 3 / 2 + (the number of existing coordinates).
         */
        typename mesh_t::positions_t new_positions { std::move(previous_positions) };
        new_positions.reserve(new_positions.size() + previous_triangle_indices.size() * 3 / 2);

        /*
         * Since each existing triangle is divided into 4 new triangles, the final number of generated triangles is
         * indeed (the number of existing triangles) * 4.
         */
        typename mesh_t::triangle_indices_t new_triangle_indices;
        new_triangle_indices.reserve(previous_triangle_indices.size() * 4);

        /*
         * The three vertices of the existing triangle and the midpoints of each side are inserted into the new_positions
         * list. Since midpoints will be used again in neighboring triangles (at most once), they are inserted into the
         * edge_midpoints with keys representing pairs of sorted indices for the endpoints of the side that contains the
         * midpoint, and the values being the index of the midpoint in new_positions.
         * When needed later, you can retrieve the midpoint from edge_midpoints and use it. Afterward, you can remove
         * the midpoint from edge_midpoints since midpoints are only used in two triangles.
         */
        std::unordered_map<std::pair<IndexType, IndexType>, IndexType, pair_hash> edge_midpoints;
        const auto process_midpoint = [&](IndexType idx1, IndexType idx2) -> IndexType /* midpoint index */ {
            if (auto it = edge_midpoints.find(make_ascending_pair(idx1, idx2)); it != edge_midpoints.end()){
                const IndexType midpoint_index = it->second;
                edge_midpoints.erase(it);
                return midpoint_index;
            }
            else{
                glm::vec3 &midpoint = new_positions.emplace_back(glm::normalize((new_positions[idx1] + new_positions[idx2]) / 2.f));
                const IndexType midpoint_index = std::distance(new_positions.begin().base(), &midpoint);
                edge_midpoints.emplace(make_ascending_pair(idx1, idx2), midpoint_index);
                return midpoint_index;
            }
        };
        for (const auto [i1, i2, i3] : previous_triangle_indices){
            const IndexType m12 = process_midpoint(i1, i2);
            const IndexType m23 = process_midpoint(i2, i3);
            const IndexType m31 = process_midpoint(i3, i1);

            new_triangle_indices.emplace_back(triangle_index_t { i1, m12, m31 });
            new_triangle_indices.emplace_back(triangle_index_t { m12, i2, m23 });
            new_triangle_indices.emplace_back(triangle_index_t { m31, m23, i3 });
            new_triangle_indices.emplace_back(triangle_index_t { m12, m23, m31 });
        }

        // Assertions.
        assert(edge_midpoints.empty());
        assert(new_triangle_indices.size() == previous_triangle_indices.size() * 4);

        return { .positions = std::move(new_positions), .triangle_indices = std::move(new_triangle_indices) };
    }
};