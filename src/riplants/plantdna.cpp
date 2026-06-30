#include <globals.h>
#include <rctypes.h>
#include <riplants/plants.h>
#include <helpers/uploadqueue.h>

#include <vector>
#include <string>
#include <array>
#include <cmath>
#include <cstddef>  // for offsetof
#include <algorithm>
#include <iostream>

#define MAX_DEPTH 2


std::string genRule(float height, float thickness, float density, float apical, float& ruleVerticality) {
    std::string rule = "";//(height < 0.5f) ? "^" : "F";
    char terminal = (height < 0.3f) ? '^' : 'f';

    float branchiness = glm::clamp(thickness * 0.5f + density * 0.5f, 0.0f, 1.0f);
    int droopPerSeg = int(glm::mix(0.0f, 2.0f, 1.0f - thickness));  // continuous droop: thin flexible stems get D interleaved per segment

    //trunk
    int trunkHeight = glm::clamp(int(height * 4.0f), 1, 5);
    for (int i = 0; i < trunkHeight; ++i) {
        rule += "F";
        ruleVerticality *= 0.7f;
        if (i == 0 && height > 0.5f) rule += "V";  //taller plants get a verticality boost at the base to prevent early branching from getting too horizontal
        if (density > 0.6f && i > 0 && i % 2 == 0) rule += "R";
    }

    //branching structure
    int whorlCount       = glm::clamp(int(height * 2.0f + thickness * 1.0f), 1, 4);
    int branchesPerWhorl = glm::clamp(int(density * 2.5f + thickness * 1.5f + 1.0f), 1, 3);
    for (int whorl = 0; whorl < whorlCount; ++whorl) {
        float whorlPosition = float(whorl) / float(glm::max(whorlCount - 1, 1));

        //tall sparse plants get long bare trunk sections
        if (whorl > 0) {
            int internodeLen = glm::clamp(int(height * 2.0f * (1.0f - density * 0.5f)), 0, 3);
            for (int i = 0; i < internodeLen; ++i) {
                rule += "F";
                ruleVerticality *= 0.7f;
            }
        }

        //whorl of branches
        for (int b = 0; b < branchesPerWhorl; ++b) {
            rule += "[";
            rule += "L";

            //branch angling
            int divergences = glm::clamp(int(1.0f + density * 3.0f), 1, 4);
            for (int r = 0; r < divergences; ++r) rule += "R";

            //taller plants get longer branches, 
            //tempered by thickness which favors shorter, bushier branching instead of long sweeping branches
            float heightReduction = glm::mix(0.7f, 1.0f, 1.0f - whorlPosition);
            int branchLength = glm::clamp(int(height * 2.0f * heightReduction + thickness * 1.0f), 1, 4);
            for (int seg = 0; seg < branchLength; ++seg) {
                for (int d = 0; d < droopPerSeg; ++d) rule += "D";  //interleaved droop for thinner plants
                rule += "F";
                if (seg > 0 && seg % 2 == 0) rule += "R";
                if (branchiness > 0.65f && seg == branchLength / 2) {
                    rule += "[RRF";
                    rule += terminal + "]";
                }
            }

            rule += terminal;
            rule += "]";
        }
    }

    //apical crowning is more likely with taller plants
    //but can be overridden by high density/thickness which favors bushier branching instead
    if (apical > 0.65f && height > 0.7f) {

        //extra vertical growth before terminal branches to create a more pronounced crown
        int crownRise = glm::clamp(int(height * 2.0f), 1, 3);
        for (int i = 0; i < crownRise; ++i) {
            rule += "F";
            ruleVerticality *= 0.9f;
        }

        //terminal branches 
        int terminalBranches = glm::clamp(int(density * 3.0f + 2.0f), 2, 4);
        for (int i = 0; i < terminalBranches; ++i) {
            rule += "[L";
            for (int r = 0; r < 3; ++r) rule += "R";
            int terminalLength = glm::clamp(int(thickness * 2.0f + 1.0f), 1, 3);
            for (int seg = 0; seg < terminalLength; ++seg) {
                for (int d = 0; d < droopPerSeg; ++d) rule += "D";
                rule += "F";
                if (seg > 0) rule += "R";
            }
            rule += terminal + "]";
        }
    }

    return rule;
}

void PDNACPU::generateBranch() {
    float apical = glm::clamp(m_height * 0.1f * 0.65f + m_thickness * 0.1f * 0.35f, 0.0f, 1.0f);
    std::string rule = genRule(m_height * 0.15f, m_thickness* 0.1f, m_density* 0.1f, apical, m_ruleVerticality);
    fmt::println("PDNA #{} l-rule: {}", m_id, rule);

    m_axiom   = "F";
    m_lsystem = m_axiom;

    int maxDepth     = glm::clamp((int)(m_height * 0.5f + 2.0f), 3, 6);  
    
    //strategically trims rule iterating to optimize transform budget
    float branchFactor  = glm::clamp(m_density * m_height, 0.0f, 1.0f);
    int transformBudget = (int)glm::mix(80.0f, float(TRANSFORM_COUNT - 50), branchFactor);
    int maxIterations = glm::clamp(int(m_height * 3.0f + 1.0f), 1, 3);
    for (int iteration = 0; iteration < maxIterations; ++iteration) {
        std::string next;
        next.reserve(m_lsystem.size() * 2);
        int depth = 0;

        for (int i = 0; i < (int)m_lsystem.size(); ++i) {
            char c = m_lsystem[i];

            if      (c == '[') { depth++; next += '['; }
            else if (c == ']') { depth--; next += ']'; }
            else if (c == 'F') {
                bool atBranchEnd = (i + 1 < (int)m_lsystem.size() && m_lsystem[i+1] == ']');
                bool tooDeep     = (depth >= maxDepth);

                if (tooDeep || atBranchEnd) {
                    next += 'f';
                } else {
                    next += rule;
                }
            }
            else {
                next += c;
            }
        }

        m_lsystem = next;

        //safety net
        int fCount = std::count(m_lsystem.begin(), m_lsystem.end(), 'F');
        if (fCount > transformBudget) {
            fmt::println("PDNA #{} - safety net hit ({} F) at iter {}", m_id, fCount, iteration + 1);
            break;
        }
        if ((int)m_lsystem.size() > 50000) {
            fmt::println("PDNA #{} - string too large at iter {}", m_id, iteration + 1);
            break;
        }
    }

    int finalFCount = std::count(m_lsystem.begin(), m_lsystem.end(), 'F');
    fmt::println("PDNA #{} - final: {} chars, ~{} transforms", m_id, m_lsystem.size(), finalFCount);
}

float angleDist(float a, float b) {
    float d = abs(a - b);
    return glm::min(d, 2*PI - d);
}


float pigmentCostCurve(float hueDeg){
    float h = glm::radians(hueDeg);
    float cost = 1.0;

    //slightly reduced multipliers for subtler effect
    cost -= 0.35 * exp(-glm::pow(angleDist(h, glm::radians(100.0f)),2) / glm::pow(glm::radians(40.0f),2));
    cost -= 0.25 * exp(-glm::pow(angleDist(h, glm::radians(10.0f)),2) / glm::pow(glm::radians(35.0f),2));
    cost -= 0.20 * exp(-glm::pow(angleDist(h, glm::radians(320.0f)),2) / glm::pow(glm::radians(30.0f),2));

    //rarer zones
    cost += 0.5 * exp(-glm::pow(angleDist(h, glm::radians(240.0f)),2) / glm::pow(glm::radians(20.0f),2));
    cost += 0.35 * exp(-glm::pow(angleDist(h, glm::radians(190.0f)),2) / glm::pow(glm::radians(18.0f),2));

    return glm::max(cost, 0.1f);
}


void PDNACPU::generateColor(){
    float h;
    do {
        h = C::rng.getRandomValue(0, 360);
    } while (C::rng.getRandomValue(0,1) > 1.0 / pigmentCostCurve(h));
    m_hue = h; 

    m_sustainCost = m_height + m_thickness + m_density + pigmentCostCurve(m_hue);

    fmt::println("PDNA #{} sustainCost: {}", m_id, m_sustainCost);
}