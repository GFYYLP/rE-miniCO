#include <globals.h>
#include <rctypes.h>
#include <riplants/plants.h>

#include <vector>
#include <array>
#include <cmath>
#include <unordered_map>
#include <cstddef>  // for offsetof
#include <queue>
#include <stack>
#include <tuple>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct OrientVectors{
    glm::vec3 right;
    glm::vec3 up;
    glm::vec3 forward;
};
OrientVectors calculateOrient(float yaw, float pitch);


class BranchBuilder {
public:
    BranchBuilder(const PDNACPU& dna, uint32_t instanceIdx, glm::vec2 worldPos)
        : m_lsystem(dna.m_lsystem)
        , m_branchAngle(dna.m_density * 5.0f)
        , m_instanceIdx(instanceIdx)
    {
        float offsetX = glm::min(C::rng.getRandomValue(-50, 50) * 10.0f, float(I::terrainWidth  - 1 - worldPos.x));
        float offsetZ = glm::min(C::rng.getRandomValue(-50, 50) * 10.0f, float(I::terrainHeight - 1 - worldPos.y));
        int pixelIndex = (int(worldPos.y + offsetZ) * I::terrainWidth + int(worldPos.x + offsetX)) * 4;
        float offsetY = I::heightmap.pixels[pixelIndex] * 2000.0f;
        m_initialOffset = glm::vec3(worldPos.x + offsetX, offsetY, worldPos.y + offsetZ);

        m_turtle.position        = glm::vec3(0.0f);
        m_turtle.pitch           = 0.0f;
        m_turtle.yaw             = 0.0f;
        m_turtle.thickness       = dna.m_thickness;
        m_turtle.taperFactor     = 1.0f;
        m_turtle.generationLevel = 0;
        m_turtle.alternateSide   = 0;
    }

    std::tuple<int, glm::vec3, bool> evaluate() {
        walk();
        flushLeaves();
        bool ok = upload();
        fmt::println("Transform count of plant #{}: {}", m_instanceIdx, m_transformCount);
        return { m_transformCount, m_initialOffset, ok };
    }

private:
    struct TurtleState {
        glm::vec3 position;
        float pitch;
        float yaw;
        float thickness;
        float taperFactor;
        int generationLevel;
        int alternateSide;
    };

    const std::string& m_lsystem;
    float m_branchAngle;
    uint32_t m_instanceIdx;
    glm::vec3 m_initialOffset;

    TurtleState m_turtle = {};
    std::stack<TurtleState> m_stateStack;
    std::stack<int> m_parentStack;
    int m_transformCount = 0;
    int m_lCounter = 0;
    std::vector<std::pair<glm::mat3x4, glm::vec4>> m_leafList;
    glm::mat3x4 m_transformList[TRANSFORM_COUNT]         = {};
    glm::vec4   m_translationList[TRANSFORM_COUNT]       = {};
    glm::mat3x4 m_parentTransformList[TRANSFORM_COUNT]   = {};
    glm::vec4   m_parentTranslationList[TRANSFORM_COUNT] = {};

    void walk() {
        for (int i = 0; i < (int)m_lsystem.size(); ++i) {
            char c = m_lsystem[i];
            if (c == 'F') {   //trunk/branch
                if (m_transformCount >= E::transformCount) {
                    fmt::println("Transform limit reached!");
                    break;
                }

                auto orients = calculateOrient(m_turtle.yaw, m_turtle.pitch);

                m_translationList[m_transformCount] = glm::vec4(m_turtle.position, m_turtle.thickness);
                m_transformList[m_transformCount] = glm::mat3x4(
                    glm::vec4(orients.right,   m_turtle.generationLevel),
                    glm::vec4(orients.up,      m_turtle.thickness * 30.0f),
                    glm::vec4(orients.forward, 0.0f)
                );

                if (!m_parentStack.empty()) {
                    int parentIdx = m_parentStack.top();
                    m_parentTransformList[m_transformCount]   = m_transformList[parentIdx];
                    m_parentTranslationList[m_transformCount] = m_translationList[parentIdx];
                } else {
                    //root has no parent
                    m_parentTransformList[m_transformCount] = glm::mat3x4(
                        glm::vec4(1, 0, 0, 1.0f),
                        glm::vec4(0, 1, 0, 0.0f),
                        glm::vec4(0, 0, 1, 0.0f)
                    );
                    m_parentTranslationList[m_transformCount] = glm::vec4(0.0f);
                }

                ++m_transformCount;
                m_turtle.position += orients.forward * m_turtle.thickness * 10.0f;  //moves forward
                m_turtle.taperFactor -= (1.0f - 0.8f);
            }
            else if (c == 'f') {  //terminal branch
                auto orients = calculateOrient(m_turtle.yaw, 0.0f);
                m_leafList.push_back({
                    glm::mat3x4(
                        glm::vec4(orients.right,   m_turtle.position.x + m_initialOffset.x),
                        glm::vec4(orients.up,      m_turtle.position.z + m_initialOffset.z),
                        glm::vec4(orients.forward, 1.0f)),
                    glm::vec4(m_turtle.position.x, m_turtle.position.y - 20.0f, m_turtle.position.z, 30.0f)
                });
            }
            else if (c == 'R') {
                m_turtle.pitch += glm::radians((float)C::rng.getRandomValue(-m_branchAngle, m_branchAngle));
                m_turtle.yaw   += glm::radians((float)C::rng.getRandomValue(-m_branchAngle, m_branchAngle));
            }
            else if (c == '^') {  //foliage
                float density = m_turtle.thickness;
                int instanceCount = glm::clamp(int(density * 40.0f + 10.0f), 5, 80);
                float spread = glm::mix(15.0f, 3.0f, density);  //tighter cluster at higher density

                for (int j = 0; j < instanceCount; ++j) {
                    float quadYaw   = m_turtle.yaw   + glm::radians(C::rng.getRandomValue(-30.0f, 30.0f));
                    float quadPitch = m_turtle.pitch + glm::radians(C::rng.getRandomValue(-15.0f, 15.0f));
                    glm::vec3 offset = glm::vec3(
                        C::rng.getRandomValue(-spread, spread),
                        0.0f,
                        C::rng.getRandomValue(-spread, spread)
                    );
                    int px   = glm::clamp(int(m_turtle.position.x + m_initialOffset.x + offset.x), 0, I::terrainWidth  - 1);
                    int pz   = glm::clamp(int(m_turtle.position.z + m_initialOffset.z + offset.z), 0, I::terrainHeight - 1);
                    int pidx = (pz * I::terrainWidth + px) * 4;
                    offset.y = I::heightmap.pixels[pidx] * 2000.0f;

                    auto orients = calculateOrient(quadYaw, quadPitch);
                    m_leafList.push_back({
                        glm::mat3x4(
                            glm::vec4(orients.right,   m_turtle.position.x + m_initialOffset.x),
                            glm::vec4(orients.up,      m_turtle.position.z + m_initialOffset.z),
                            glm::vec4(orients.forward, 3.0f)
                        ),
                        glm::vec4(
                            m_turtle.position.x + offset.x,
                            offset.y - 30.0f,  //lower foliage (center-heavy mask) to minimize detachment from parent
                            m_turtle.position.z + offset.z,
                            7.0f
                        )
                    });
                }
            }
            else if (c == '[') { //push
                if (m_transformCount > 0) m_parentStack.push(m_transformCount - 1);  //current segment becomes parent
                m_stateStack.push(m_turtle);
                ++m_turtle.generationLevel;
                m_turtle.thickness *= 0.65f;
            }
            else if (c == ']') {  //pop
                if (!m_parentStack.empty()) m_parentStack.pop();
                if (!m_stateStack.empty()) {
                    m_turtle = m_stateStack.top();
                    m_stateStack.pop();
                }
            }
            else if (c == 'L') { //golden spiral divergence
                constexpr float GOLDEN_ANGLE = 137.508f;
                m_turtle.yaw   += glm::radians(GOLDEN_ANGLE * m_lCounter++);
                m_turtle.pitch += glm::radians(m_branchAngle * 0.9f);
            }
            else if (c == 'A') {  //alternating
                m_turtle.alternateSide = 1 - m_turtle.alternateSide;
                m_turtle.yaw   += glm::radians((m_turtle.alternateSide ? 1.0f : -1.0f) * 90.0f);
                m_turtle.pitch += glm::radians(m_branchAngle);
            }
            else if (c == 'V') {  //vertical bias
                m_turtle.pitch *= 0.7f * (C::rng.getRandomValue(70, 150) / 100.0f);
            }
            else if (c == 'D') {  //drooping
                m_turtle.pitch += glm::radians(m_branchAngle * 1.5f);
            }
        }
    }

    void flushLeaves() {
        //builds coherent transform buffer
        for (const auto& leaf : m_leafList) {
            if (m_transformCount >= E::transformCount) {
                fmt::println("Transform limit reached!");
                break;
            }
            m_transformList[m_transformCount]   = leaf.first;
            m_translationList[m_transformCount] = leaf.second;
            ++m_transformCount;
        }
    }

    bool upload() {
        return enqueueBatch({
            {9, &m_transformList,         sizeof(glm::mat3x4) * TRANSFORM_COUNT,
                offsetof(PMatrix, transform)         + m_instanceIdx * sizeof(glm::mat3x4) * TRANSFORM_COUNT},
            {9, &m_translationList,       sizeof(glm::vec4)   * TRANSFORM_COUNT,
                offsetof(PMatrix, translation)       + m_instanceIdx * sizeof(glm::vec4)   * TRANSFORM_COUNT},
            {9, &m_parentTransformList,   sizeof(glm::mat3x4) * TRANSFORM_COUNT,
                offsetof(PMatrix, parentTransform)   + m_instanceIdx * sizeof(glm::mat3x4) * TRANSFORM_COUNT},
            {9, &m_parentTranslationList, sizeof(glm::vec4)   * TRANSFORM_COUNT,
                offsetof(PMatrix, parentTranslation) + m_instanceIdx * sizeof(glm::vec4)   * TRANSFORM_COUNT},
            {9, &m_initialOffset,         sizeof(glm::vec4),
                offsetof(PMatrix, worldOffset)       + m_instanceIdx * sizeof(glm::vec4)},
        });
    }
};


std::tuple<int, glm::vec3, bool> generateBranches(uint32_t instanceIdx, const PDNACPU& currDNA, const glm::vec2& parentWorldPos) {
    return BranchBuilder(currDNA, instanceIdx, parentWorldPos).evaluate();
}
