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
    using triangle_index_t = mesh_t::triangle_index_t;

    struct pair_hash{
        constexpr std::size_t operator()(std::pair<IndexType, IndexType> pair) const noexcept{
            // Boost hash_combine.
            // https://stackoverflow.com/questions/35985960/c-why-is-boosthash-combine-the-best-way-to-combine-hash-values

            std::size_t seed = std::hash<IndexType>{}(pair.first);
            seed ^= std::hash<IndexType>{}(pair.second) + 0x9e3779b9 + (seed<<6) + (seed>>2);
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
         * 새로 생성되는 위치는 기존 삼각형의 세 변의 중점이며, 따라서 삼각형 당 3개의 새 좌표가 생성된다. 그러나, 이웃한 두 삼각형 면이 공유하는
         * 변의 중점이 두 번 계산되므로 최종적으로 생성되는 좌표의 개수는 (기존 삼각형 개수) * 3 / 2이다. 총 좌표의 개수는
         *     (기존 삼각형 개수) * 3 / 2 + (기존 좌표 개수)
         * 이다.
         */
        typename mesh_t::positions_t new_positions { std::move(previous_positions) };
        new_positions.reserve(new_positions.size() + previous_triangle_indices.size() * 3 / 2);

        /*
         * 기존 삼각형이 4개의 새로운 삼각형으로 나누어지므로 최종적으로 생성되는 삼각형의 개수는
         *     (기존 삼각형 개수) * 4
         * 이다.
         */
        typename mesh_t::triangle_indices_t new_triangle_indices;
        new_triangle_indices.reserve(previous_triangle_indices.size() * 4);

        /*
         * 기존 삼각형의 세 꼭짓점과, 각 변의 중점을 new_positions에 삽입한다. 중점은 차후 인접한 삼각형에서 다시 사용되므로 (최대 1번)
         * edge_midpoints에 해당 중점이 포함된 변의 정렬된 양 끝점 인덱스 쌍을 키, new_positions 내 중점의 인덱스를 값으로 하여 삽입한다. 이후
         * 다음 사용 시 중점을 edge_midpoints에서 불러와 사용하고, 이후 해당 중점은 다시 탐색되지 않으므로 (중점은 오직 두 개의 삼각형에서만
         * 사용되므로) edge_midpoints에서 삭제한다.
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