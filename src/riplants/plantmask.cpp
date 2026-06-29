#include <globals.h>
#include <rctypes.h>
#include <riplants/plants.h>
#include <helpers/uploadqueue.h>

#include <vector>
#include <array>
#include <cmath>
#include <unordered_map>
#include <cstddef>
#include <queue>
#include <stack>
#include <algorithm>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


struct LeafShape {
    float baseRadius;
    float aspectRatio;
    int   lobeCount;
    float lobeDepth;
    float taperStrength;
    float asymmetry;
    float waviness;
    float tipSharpness;
};

struct FlowerShape {
    int   petalCount;
    float petalLength;
    float petalWidth;
    float petalCurl;
    float centerSize;
    float ruffleAmount;
};

struct Leaf {
    glm::vec2 center;
    float     rotation;
    float     scale;
    LeafShape shape;
    glm::vec3 normal;
    float     depth;
};


class PlantMask {
public:
    PlantMask(float density, float height, float thickness, unsigned int seed, int size)
        : m_density(density), m_height(height), m_thickness(thickness)
        , m_seed(seed), m_N(size) {}

    void generate(std::array<float, BITMASK_RES * 4>& mask) {
        generateFoliage(mask);
        generateFlower(mask);
    }

private:
    float        m_density, m_height, m_thickness;
    unsigned int m_seed;
    int          m_N;

    // --- leaf helpers ---

    static LeafShape makeLeafShape(float density, float height, float thickness, unsigned int seed) {
        auto seededRand = [seed](float min, float max, int offset) -> float {
            unsigned int s = seed + offset * 12345;
            s = (s ^ 61) ^ (s >> 16);
            s = s + (s << 3);
            s = s ^ (s >> 4);
            s = s * 0x27d4eb2d;
            s = s ^ (s >> 15);
            float normalized = (s & 0x7FFFFFFF) / float(0x7FFFFFFF);
            return min + normalized * (max - min);
        };

        LeafShape shape;
        shape.baseRadius    = 0.6f + thickness * 0.3f;
        shape.aspectRatio   = glm::mix(1.2f, 0.5f, density);
        shape.lobeCount     = int(glm::mix(0.0f, 8.0f, height));
        shape.lobeDepth     = glm::mix(0.1f, 0.5f, density * (1.0f - thickness * 0.5f));
        shape.taperStrength = glm::mix(0.2f, 0.6f, 1.0f - thickness);
        shape.asymmetry     = seededRand(0.0f, 0.15f, 0);
        shape.waviness      = density * seededRand(0.1f, 0.4f, 1);
        shape.tipSharpness  = glm::mix(0.3f, 0.9f, 1.0f - thickness);
        return shape;
    }

    static float leafSDF(glm::vec2 uv, const LeafShape& shape) {
        float angle = std::atan2(uv.y, uv.x);

        float sdf = glm::length(glm::vec2(uv.x * shape.aspectRatio, uv.y)) - shape.baseRadius;

        float taperFactor = glm::smoothstep(-shape.baseRadius, shape.baseRadius * 0.3f, uv.y);
        sdf += (1.0f - taperFactor) * shape.taperStrength;
        float tipFactor = glm::smoothstep(shape.baseRadius * 0.5f, shape.baseRadius, uv.y);
        sdf += tipFactor * shape.tipSharpness * 0.4f;

        if (shape.lobeCount > 0) {
            float lobeAngle  = angle + PI;
            float lobeMod    = std::sin(lobeAngle * shape.lobeCount * 0.5f);
            float lobeRegion = glm::smoothstep(-shape.baseRadius * 0.3f, shape.baseRadius, uv.y);
            sdf += lobeMod * shape.lobeDepth * lobeRegion;
        }
        if (shape.waviness > 0.0f) {
            float waveFreq = 8.0f + shape.lobeCount * 2.0f;
            float wave = std::sin(angle * waveFreq + uv.y * 3.0f) * shape.waviness * 0.1f;
            sdf += wave * glm::smoothstep(-shape.baseRadius, shape.baseRadius * 0.8f, uv.y);
        }
        if (shape.asymmetry > 0.0f) {
            float asymOffset = std::sin(angle) * shape.asymmetry;
            sdf += asymOffset * glm::smoothstep(-shape.baseRadius, shape.baseRadius, uv.y);
        }

        return sdf;
    }

    static glm::vec3 leafNormal(glm::vec2 leafUV, const LeafShape& shape, const glm::vec3& baseNormal) {
        float r           = glm::length(leafUV);
        float normalizedR = glm::clamp(r / shape.baseRadius, 0.0f, 1.0f);
        float curvatureStrength = normalizedR * (1.0f - shape.tipSharpness * 0.3f);

        glm::vec3 curvature = glm::vec3(
            leafUV.x * curvatureStrength * 0.6f,
            leafUV.y * curvatureStrength * 0.4f,
            0.0f
        );
        if (shape.lobeCount > 2) {
            float angle       = std::atan2(leafUV.y, leafUV.x);
            float veinPattern = std::sin(angle * shape.lobeCount * 0.5f) * 0.15f;
            curvature.z += veinPattern * normalizedR;
        }
        return glm::normalize(baseNormal + curvature);
    }

    Leaf makeLeaf(int hierarchyLevel, const std::vector<LeafShape>& variations) {
        enum PlacementMode { Scatter, Fan, Radial };
        PlacementMode mode = (m_height < 0.2f) ? Fan
                           : (m_thickness > 0.5f && m_height < 0.5f) ? Radial
                           : Scatter;

        const int   numLeaves    = glm::clamp(int(m_density * m_height * 50.0f), 5, 60);
        const float leafScaleBase = m_thickness * 1.0f;

        Leaf leaf;
        if (mode == Fan) {
            leaf.center = glm::vec2(
                C::rng.getRandomValue(-0.3f, 0.3f),
                C::rng.getRandomValue(-0.2f, 0.8f)
            );
        } else if (mode == Radial) {
            float angle = (float(hierarchyLevel) / float(numLeaves)) * 2.0f * PI
                        + C::rng.getRandomValue(-0.2f, 0.2f);
            float r = C::rng.getRandomValue(0.2f, 0.7f);
            leaf.center   = glm::vec2(std::cos(angle), std::sin(angle)) * r * 0.6f;
            leaf.rotation = angle;
        } else {
            float spreadFactor = glm::mix(0.8f, 0.5f, m_density);
            leaf.center = glm::vec2(
                C::rng.getRandomValue(-40.0f * spreadFactor, 40.0f * spreadFactor) / 60.0f,
                C::rng.getRandomValue(-40.0f * spreadFactor, 40.0f * spreadFactor) / 60.0f
            );
        }

        float scaleVariation = C::rng.getRandomValue(0.8f, 1.2f);
        leaf.scale = (leafScaleBase * scaleVariation) / 100.0f;
        leaf.depth = C::rng.getRandomValue(0.0f, 1.0f);

        int variationIdx = C::rng.getRandomValue(0, (int)variations.size() - 1);
        leaf.shape = variations[variationIdx];

        float theta = C::rng.getRandomValue(0.0f, 2.0f * PI);
        float phi   = C::rng.getRandomValue(0.0f, PI / 2.0f);
        leaf.normal = glm::vec3(
            std::sin(phi) * std::cos(theta),
            std::sin(phi) * std::sin(theta),
            std::cos(phi)
        );

        return leaf;
    }

    std::vector<LeafShape> buildVariations() {
        int numVariations = glm::clamp(int(m_height * 5.0f), 2, 8);
        std::vector<LeafShape> variations;
        variations.reserve(numVariations);
        for (int i = 0; i < numVariations; ++i) {
            variations.push_back(makeLeafShape(m_density * 0.002f, m_height * 0.01f, m_thickness * 0.002f, m_seed + i));
        }
        return variations;
    }

    std::vector<Leaf> placeLeaves(const std::vector<LeafShape>& variations) {
        int numLeaves = int(m_density * m_height * 3.0f);
        std::vector<Leaf> leaves;
        leaves.reserve(numLeaves);
        for (int i = 0; i < numLeaves; ++i) {
            leaves.push_back(makeLeaf(i, variations));
        }
        std::sort(leaves.begin(), leaves.end(),
            [](const Leaf& a, const Leaf& b) { return a.depth < b.depth; });
        return leaves;
    }

    void rasterizeFoliage(const std::vector<Leaf>& leaves, std::array<float, BITMASK_RES * 4>& mask) {
        const float feather = 0.05f;
        for (const auto& leaf : leaves) {
            float cosR = std::cos(leaf.rotation);
            float sinR = std::sin(leaf.rotation);

            for (int y = 0; y < m_N; ++y) {
                for (int x = 0; x < m_N; ++x) {
                    glm::vec2 uv     = (glm::vec2(x + 0.5f, y + 0.5f) / float(m_N)) * 2.0f - 1.0f;
                    glm::vec2 border = glm::max(glm::abs(uv) - 0.7f, glm::vec2(0.0f));
                    float vignette   = 1.0f - glm::smoothstep(0.0f, 0.5f, glm::length(border));

                    glm::vec2 offset  = uv - leaf.center;
                    glm::vec2 rotated = glm::vec2(
                        offset.x * cosR + offset.y * sinR,
                       -offset.x * sinR + offset.y * cosR
                    );
                    glm::vec2 leafUV = rotated / (leaf.scale * 3.0f);

                    float d     = leafSDF(leafUV, leaf.shape);
                    float alpha = glm::smoothstep(feather, -feather, d) * vignette;

                    if (alpha > 0.01f) {
                        int idx = (y * m_N + x) * 4;

                        glm::vec3 n    = leafNormal(leafUV, leaf.shape, leaf.normal);
                        float srcA     = alpha;
                        float dstA     = mask[idx];
                        float outA     = srcA + dstA * (1.0f - srcA);

                        if (outA > 0.0f) {
                            glm::vec3 blendedNormal = glm::normalize(
                                (n * srcA + glm::vec3(mask[idx+1], mask[idx+2], mask[idx+3]) * dstA * (1.0f - srcA)) / outA
                            );
                            mask[idx]   = glm::clamp(outA, 0.0f, 1.0f);
                            mask[idx+1] = blendedNormal.x;
                            mask[idx+2] = blendedNormal.y;
                            mask[idx+3] = blendedNormal.z;
                        }
                    }
                }
            }
        }
    }

    void generateFoliage(std::array<float, BITMASK_RES * 4>& mask) {
        auto variations = buildVariations();
        auto leaves     = placeLeaves(variations);
        rasterizeFoliage(leaves, mask);
    }

    // --- flower helpers ---

    static FlowerShape makeFlowerShape(float density, float height, float thickness) {
        FlowerShape shape;
        shape.petalCount   = int(glm::mix(3.0f, 12.0f, height));
        shape.petalLength  = glm::mix(0.5f, 1.0f, thickness);
        shape.petalWidth   = glm::mix(0.3f, 0.7f, thickness);
        shape.petalCurl    = glm::mix(0.2f, 0.8f, density);
        shape.centerSize   = glm::mix(0.3f, 0.1f, thickness);
        shape.ruffleAmount = (density + height) * 0.3f;
        return shape;
    }

    static float petalSDF(glm::vec2 uv, const FlowerShape& shape) {
        glm::vec2 p = uv;
        p.y += shape.centerSize;
        p.x *= (1.0f / shape.petalWidth);

        float tip         = glm::smoothstep(shape.petalLength * 0.9f, shape.centerSize * 0.2f, p.y);
        float ruffleFreq  = 5.0f + shape.ruffleAmount * 3.0f;
        float ruffle      = std::sin(p.y * ruffleFreq) * shape.ruffleAmount * 0.05f;

        return glm::length(p) - glm::mix(shape.petalLength * 0.7f, shape.petalLength * 0.3f, tip) + ruffle;
    }

    static float flowerSDF(glm::vec2 uv, const FlowerShape& shape) {
        float a = glm::atan(uv.y, uv.x);
        float r = glm::length(uv);

        if (r < shape.centerSize) return r - shape.centerSize;

        float sector  = 2.0f * PI / shape.petalCount;
        float localA  = glm::mod(a + sector * 0.5f, sector) - sector * 0.5f;
        glm::vec2 petalUV = glm::vec2(std::cos(localA) * r, std::sin(localA) * r);

        return petalSDF(petalUV, shape);
    }

    void generateFlower(std::array<float, BITMASK_RES * 4>& mask) {
        FlowerShape shape = makeFlowerShape(m_density, m_height, m_thickness);

        for (int y = 0; y < m_N; ++y) {
            for (int x = 0; x < m_N; ++x) {
                glm::vec2 uv = (glm::vec2(x + 0.5f, y + 0.5f) / float(m_N)) * 2.0f - 1.0f;

                float d     = flowerSDF(uv, shape);
                float alpha = glm::smoothstep(0.02f, -0.02f, d);

                if (alpha > 0.01f) {
                    int idx = ((y + m_N) * m_N + x) * 4;

                    float r           = glm::length(uv);
                    float curlFactor  = shape.petalCurl * r;
                    glm::vec3 n       = glm::normalize(glm::vec3(
                        glm::normalize(uv) * -curlFactor,
                        1.0f - curlFactor * 0.5f
                    ));

                    mask[idx]   = alpha;
                    mask[idx+1] = curlFactor;
                    mask[idx+2] = n.x * 0.5f + 0.5f;
                    mask[idx+3] = n.y * 0.5f + 0.5f;
                }
            }
        }
    }
};


void PDNACPU::generateMask() {
    unsigned int seed = (unsigned int)(m_density * 1000.0f + m_height * 100.0f + m_thickness * 10.0f);
    PlantMask(m_density, m_height, m_thickness, seed, m_bitmaskSize).generate(m_mask);
}
