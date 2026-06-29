#pragma once

#include <rctypes.h>

#include <vector>
#include <array>
#include <cmath>
#include <unordered_map>
#include <cstddef>  // for offsetof
#include <queue>

#include <glm/glm.hpp>

#define BITMASK_WIDTH 128
#define BITMASK_HEIGHT (BITMASK_WIDTH * 2)
#define BITMASK_RES (BITMASK_HEIGHT * BITMASK_WIDTH)

#define TRANSFORM_COUNT 300
#define PLANT_COUNT 300
#define DNA_COUNT 30


struct MaskPixels {
    uint32_t alpha;              // offset 0
    uint32_t _pad0;              // offset 4 
    uint32_t _pad1;              // offset 8
    uint32_t _pad2;              // offset 12  (these 3 pad fields will later be translated to flower data)
    glm::vec4 normalHint;        // offset 16
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

//TODO: pad floats to 16-bit, do some timing to see if the added memory footprint is worth the performance gain
struct PInstance{
    uint32_t id[PLANT_COUNT];
    float energy[PLANT_COUNT];
    float age[PLANT_COUNT];
    uint32_t seed[PLANT_COUNT];
    uint32_t dna[PLANT_COUNT];
    glm::vec4 position[PLANT_COUNT];   //world-space transformation 
};


enum PMutables{
    wood, lobed,                     //light pressure (excess fertility)
    carnivore, parasitic,            //fertility pressure (excess light)
    explosive, flowering, biolumi,   //reproduction (excess energy)
    mimicry, thorny, toxic,          //self-defense     //purple/yellow tinges for toxicity 

    max_mutables,
};


struct PDNACPU{
    int m_id = 0;
    int m_type;
    std::string m_lsystem; 
    std::string m_axiom;

    std::string m_rule;
    float m_hue;
    float m_sustainCost;
    const int m_bitmaskSize = BITMASK_WIDTH;
    //MaskPixels m_leafMasks[BITMASK_RES] = {};
    //PDNA m_gpuDNA = {};
    std::array<float, BITMASK_RES*4> m_mask = {};
    float m_height;      // target adult height in arbitrary units
    float m_thickness;    // trunk diameter (lower for herbs, higher for trees)
    float m_density;      // branch/leaves frequency
    int m_lifespan;        // lifespan in cycles/frames
    float m_ruleVerticality=1.0f;  //ensure height don't grow disproportionately for plants that generate a lot of trunk Fs
    std::array<int, PMutables::max_mutables> m_mutateBias = {};
    //Sound sound;

    PDNACPU(const int& id = 0, const int& m_type = 0,
                    const std::string& rule = "FR[RRFR[RF]]", const std::string& axiom = "F",
                    const float& height = 1.0f, const float& thickness = 2.0f, const float& density = 7.0f,
                    const std::array<int, PMutables::max_mutables>& mutateBias = {})
        : m_id(id), m_type(m_type),
        m_rule(rule), m_axiom(axiom),
        m_height(height), m_thickness(thickness), m_density(density),
        m_mutateBias(mutateBias)
    {
        std::string baseRule = rule;
        std::string baseAxiom = axiom;
        switch(m_type){
            case 0:{ //tree
                break;
            }
            case 1:{ //low grass
                baseRule = "";
                break;
            }
            case 2:{ //herb
                // baseRule = "^";
                // baseAxiom = "^";
                break;
            }
            default:{
                break;
            }
        }

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




