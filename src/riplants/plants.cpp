#include <globals.h>
#include <rctypes.h>
#include <riplants/plants.h>
#include <helpers/uploadqueue.h>
#include <engine/vktypes.h>

#include <vector>
#include <array>
#include <cmath>
#include <unordered_map>
#include <cstddef>  // for offsetof
#include <queue>
#include <stack>
#include <tuple>


namespace PGlobals{
    std::vector<PDNACPU> dnaList;

    //parentID - dna/position
    uint32_t plantDNA[PLANT_COUNT];
    glm::vec2 plantPos[PLANT_COUNT];
    std::vector<PInstance> plantList;
    std::queue<uint32_t> deadQueue;

    bool firstInit = true;
}


std::tuple<int, glm::vec3, bool> generateBranches(uint32_t instanceIdx, const PDNACPU& currDNA, const glm::vec2& parentWorldPos);


bool generatePlant(uint32_t parentId){
    uint32_t newId;
    bool fromDeadQueue = !PGlobals::deadQueue.empty();
    if (fromDeadQueue) {
        newId = PGlobals::deadQueue.front();
    } else {
        if (E::activePInstanceCount >= E::instanceCount){
            fmt::println("Max plant instances reached!");
            return false;
        }
        newId = (uint32_t)E::activePInstanceCount;
    }

    //assesses mutation bias (TO BE IMPLEMENTED)
    PDNACPU& newDNA = PGlobals::dnaList[PGlobals::plantDNA[parentId]];
    glm::vec2 parentPos = PGlobals::plantPos[parentId];
    uint32_t dnaID = newDNA.m_id;

    auto lsystemVal = generateBranches(newId, newDNA, parentPos);
    if (!std::get<2>(lsystemVal)) return false;
    int transformCount = std::get<0>(lsystemVal);

    float newEnergy = 0.1f;
    float newAge = 0.03f + C::rng.getRandomValue(0.0f, 0.12f);
    glm::vec4 newPos = glm::vec4(std::get<1>(lsystemVal), 1.0f);
    uint32_t newSeed = (uint32_t)C::rng.getRandomValue();

    VkDrawIndexedIndirectCommand currIndirection = {
        E::meshes[2].indexCount,
        (uint32_t)transformCount,
        E::meshes[2].indexOffset,
        (int32_t)E::meshes[2].vertexOffset,
        0
    };

    if (!enqueueBatch({
        {7, &newId,          sizeof(uint32_t),                    offsetof(PInstance, id)       + newId * sizeof(uint32_t)},
        {7, &newEnergy,      sizeof(float),                       offsetof(PInstance, energy)   + newId * sizeof(float)},
        {7, &newAge,         sizeof(float),                       offsetof(PInstance, age)      + newId * sizeof(float)},
        {7, &newSeed,        sizeof(uint32_t),                    offsetof(PInstance, seed)     + newId * sizeof(uint32_t)},
        {7, &dnaID,          sizeof(uint32_t),                    offsetof(PInstance, dna)      + newId * sizeof(uint32_t)},
        {7, &newPos,         sizeof(glm::vec4),                   offsetof(PInstance, position) + newId * sizeof(glm::vec4)},
        {8, &currIndirection,sizeof(VkDrawIndexedIndirectCommand), sizeof(VkDrawIndexedIndirectCommand) * newId},
    })) return false;

    // Commit CPU state only after uploads are guaranteed to land.
    if (fromDeadQueue) PGlobals::deadQueue.pop();
    ++E::activePInstanceCount;
    E::plantTransformCount += transformCount;
    PGlobals::plantDNA[newId] = newDNA.m_id;
    PGlobals::plantPos[newId] = glm::vec2(std::get<1>(lsystemVal).x, std::get<1>(lsystemVal).z);

    if (parentId == newId) fmt::println("Plant #{} was generated", parentId);
    else fmt::println("Plant #{} gave birth to #{}", parentId, newId);
    return true;
}


bool removePlant(uint32_t deadId){
    static const float zero = 0.0f;
    if (!enqueueBatch({
        {7, &zero, sizeof(float), offsetof(PInstance, age) + deadId * sizeof(float)},
    })) return false;

    --E::activePInstanceCount;
    PGlobals::deadQueue.push(deadId);

    fmt::println("Bye-bye Plant #{}", deadId);
    return true;
}


void generateDNA(const PDNACPU* parentDNA = nullptr){
    //assesses parent mutation bias (TO BE IMPLEMENTED)

    //initializes PDNACPU
    if (parentDNA != nullptr){
        // PDNACPU finalDNA = *parentDNA;
        // finalDNA.id = (int)PGlobals::dnaList.size();

        // //applies mutations (TO BE IMPLEMENTED)

        // PGlobals::dnaList.push_back(finalDNA);
    }
    else {
        // PDNACPU firstDNA(0, 0);
        // firstDNA.m_id = (int)PGlobals::dnaList.size();
        // enqueueTextureUpload(2, &firstDNA.m_mask, sizeof(float)*BITMASK_RES*4,
        //                 BITMASK_WIDTH, BITMASK_HEIGHT, VK_FORMAT_R32G32B32A32_SFLOAT , firstDNA.m_id);
        // PGlobals::dnaList.push_back(firstDNA);

        // PDNACPU secondDNA(1, 2, "F^", "F", 200.0f, 3.6f, 0.2f);
        // secondDNA.m_id = (int)PGlobals::dnaList.size();
        // enqueueTextureUpload(2, &secondDNA.m_mask, sizeof(float)*BITMASK_RES*4,
        //                 BITMASK_WIDTH, BITMASK_HEIGHT, VK_FORMAT_R32G32B32A32_SFLOAT , secondDNA.m_id);
        // PGlobals::dnaList.push_back(secondDNA);

        for (int i=0; i < 10; i++){
            float height, thickness, density;
            float weight = 0.0f; 
            do {
                height = C::rng.getRandomValue(5, 50) / 10.0f;
                thickness = C::rng.getRandomValue(5,  100) / 10.0f;
                density = C::rng.getRandomValue(10, 100) / 10.0f;

                // large plants should be rare
                float heightBias = glm::mix(1.0f, 0.05f, height );

                //penalize high height + low thickness
                float softBodyPenalty = (height > 0.7f && thickness < 0.3f) 
                                    ? glm::mix(1.0f, 0.02f, (height - 0.7f) / 0.3f) // linearly decrease from 1 to 0.02 as height goes from 0.4 to 1.0
                                    : 1.0f;

                // penalize structurally implausible h/t ratio
                //float structuralPenalty = glm::clamp(t / (h + 0.1f), 0.05f, 1.0f);

                weight = softBodyPenalty;// * heightBias * structuralPenalty;
                
            } while (C::rng.getRandomValue(0.0f, 1.0f) > weight);  // rejection sampling to achieve desired distribution

            PDNACPU currDNA(i, 0, "F^", "F", height, thickness, density);
            // PDNACPU currDNA(i, 0, "F^", "F", C::rng.getRandomValue(5, 50)/10.0f, 
            //                         C::rng.getRandomValue(5, 100)/10.0f,
            //                         C::rng.getRandomValue(5, 100)/10.0f);
            currDNA.m_id = (int)PGlobals::dnaList.size();
            enqueueTextureUpload(2, &currDNA.m_mask, sizeof(float)*BITMASK_RES*4,
                            BITMASK_WIDTH, BITMASK_HEIGHT, VK_FORMAT_R32G32B32A32_SFLOAT , currDNA.m_id);
            enqueueBufferUpdate(6,&currDNA.m_hue,sizeof(float),
                            offsetof(PDNA, hue) + i * sizeof(float));
            enqueueBufferUpdate(6,&currDNA.m_sustainCost,sizeof(float),
                            offsetof(PDNA, sustainCost) + i * sizeof(float));
            PGlobals::dnaList.push_back(currDNA);
        }
    }
    //PDNACPU finalDNA(PGlobals::dnaList.size());
}


void initPlants(){
    if (true){
        generateDNA();  
    }

    static std::vector<VkDrawIndexedIndirectCommand> emptyIndirection(E::instanceCount);

    if (PGlobals::firstInit) {
        fmt::println("Initial DNA generation complete. Total DNA count: {}", PGlobals::dnaList.size());
        memset(emptyIndirection.data(), 0, emptyIndirection.size() * sizeof(VkDrawIndexedIndirectCommand));


        PGlobals::firstInit = false;
    } else {
        // Reset CPU-side instance tracking so new plants start from slot 0.
        E::activePInstanceCount = 0;
        E::plantTransformCount  = 0;
        while (!PGlobals::deadQueue.empty()) PGlobals::deadQueue.pop();

        // Clear indirection buffer so old draw calls don't persist.
        enqueueBufferUpdate(8, emptyIndirection.data(), emptyIndirection.size() * sizeof(VkDrawIndexedIndirectCommand), 0);
    }


    constexpr int environmentBias = 1;  //TO BE IMPLEMENTED
    // const int spawnCount = C::rng.getRandomValue(0, 
    //         (int)((I::terrainWidth * I::terrainHeight / 100000) * environmentBias));
     const int spawnCount = 20;


    for (int i = 0; i < spawnCount; ++i){
        //concentrates towards the center for now
        glm::vec2 pos = glm::vec2(C::rng.getRandomValue((int)(I::terrainWidth * 0.2), (int)(I::terrainWidth * 0.8)),
                C::rng.getRandomValue((int)(I::terrainHeight * 0.2), (int)(I::terrainHeight * 0.8)));

        //discard if outside world bound/land (TO BE IMPLEMENTED with heightmap download)
        if (false){

        }

        PGlobals::plantDNA[i] = C::rng.getRandomValue(0, (int)PGlobals::dnaList.size() - 1);
        PGlobals::plantPos[i] = pos;

        generatePlant(i);
    }

    //std::this_thread::sleep_for(std::chrono::milliseconds(100));

    fmt::println("Plant transform count: {}", E::plantTransformCount);
}