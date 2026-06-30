#pragma once

#include <rctypes.h>

#include <vector>
#include <array>
#include <cmath>
#include <unordered_map>
#include <cstddef>
#include <queue>

#include <glm/glm.hpp>

#define BITMASK_WIDTH 128
#define BITMASK_HEIGHT (BITMASK_WIDTH * 2)
#define BITMASK_RES (BITMASK_HEIGHT * BITMASK_WIDTH)

#define TRANSFORM_COUNT 300
#define PLANT_COUNT 300
#define DNA_COUNT 30


struct MaskPixels {
    uint32_t alpha;              
    uint32_t _pad0;            
    uint32_t _pad1;              
    uint32_t _pad2; //these 3 pad fields will later be translated to flower data)
    glm::vec4 normalHint;       
};
struct PDNA{
    float hue[DNA_COUNT];
    float sustainCost[DNA_COUNT];
};


struct PMatrix{
    glm::mat3x4 transform[PLANT_COUNT][TRANSFORM_COUNT]; // orientation + scale
    glm::vec4 translation[PLANT_COUNT][TRANSFORM_COUNT];    // translation
    glm::mat3x4 parentTransform[PLANT_COUNT][TRANSFORM_COUNT];
    glm::vec4 parentTranslation[PLANT_COUNT][TRANSFORM_COUNT];
    glm::vec4 worldOffset[PLANT_COUNT];
};

struct PInstance{
    uint32_t id[PLANT_COUNT];
    float energy[PLANT_COUNT];
    float age[PLANT_COUNT];
    uint32_t seed[PLANT_COUNT];
    uint32_t dna[PLANT_COUNT];
    glm::vec4 position[PLANT_COUNT];   //world-space transformation 
};

struct PDNACPU{
    int m_id = 0;
    std::string m_lsystem; 
    std::string m_axiom;

    std::string m_rule;
    float m_hue;
    float m_sustainCost;
    const int m_bitmaskSize = BITMASK_WIDTH;
    std::array<float, BITMASK_RES*4> m_mask = {};
    float m_height;      // target adult height in arbitrary units
    float m_thickness;    // trunk diameter (lower for herbs, higher for trees)
    float m_density;      // branch/leaves frequency
    int m_lifespan;        // lifespan in cycles/frames
    float m_ruleVerticality=1.0f;  //ensure height doesn't grow disproportionately for plants that generate a lot of trunk Fs
    //std::array<int, PMutables::max_mutables> m_mutateBias = {};
    //Sound sound;

    PDNACPU(const int& id = 0, 
                    const std::string& rule = "FR[RRFR[RF]]", const std::string& axiom = "F",
                    const float& height = 1.0f, const float& thickness = 2.0f, const float& density = 7.0f)
        : m_id(id), m_rule(rule), m_axiom(axiom),
        m_height(height), m_thickness(thickness), m_density(density)
    {
        std::string baseRule = rule;
        std::string baseAxiom = axiom;

        //assesses mutation biases (TO BE IMPLEMENTED)


        //applies final stats
        m_rule = baseRule;
        m_axiom = baseAxiom;
        generateMask();
        generateBranch();
        generateColor();
    }

    void generateBranch();
    void generateColor();
    void generateMask();
};

namespace PGlobals{
    extern std::vector<PDNACPU> dnaList;

    //parentID - dna/position
    extern uint32_t plantDNA[PLANT_COUNT];
    extern glm::vec2 plantPos[PLANT_COUNT];
    extern std::vector<PInstance> plantList;
    extern std::queue<uint32_t> deadQueue;
}




