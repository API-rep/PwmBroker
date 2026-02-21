/**
 * @file main.cpp (MonkeyTest - Industrial Edition)
 * @brief Stress test + edge campaign (modes, bornes, pression, release).
 */

#include <Arduino.h>
#include <vector>
#include "PwmBroker.h"

// --- ANSI Terminal Colors ---
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"
#define CLEAR   "\033[H\033[J"

// --- Configuration ---
const uint8_t MAX_CHANNELS = 16;
const uint8_t POOL_SIZE    = 18; 
const uint8_t VALID_PINS[] = {12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
const uint32_t FREQS[]     = {
    1,
    10,
    50,
    100,
    1000,
    5000,
    10000,
    20000,
    50000,
    100000,
    500000,
    1000000
};

struct ActiveResource {
    std::unique_ptr<PwmControl> control;
    uint8_t pin;
    uint32_t freq;
    PwmModeRequest requestedMode;
};

struct EdgeStats {
    uint32_t totalRequests = 0;
    uint32_t successfulRequests = 0;
    uint32_t deniedRequests = 0;
    uint32_t setDutyCalls = 0;
    uint32_t setDutyFailures = 0;
    uint32_t integrityWarnings = 0;
};

std::vector<ActiveResource> resources;
EdgeStats edgeStats;
char lastActionBuffer[128] = "Initializing fuzzer...";

const char* modeLabel(PwmModeRequest mode) {
    switch (mode) {
        case PwmModeRequest::LowSpeed: return "LOW";
        case PwmModeRequest::HighSpeed: return "HIGH";
        case PwmModeRequest::Auto:
        default: return "AUTO";
    }
}

void clearAllResources() {
    resources.clear();
}

bool exerciseDutyExtremes(PwmControl& ctrl) {
    bool ok = true;
    uint32_t maxDuty = ctrl.getMaxDuty();
    uint32_t samples[4] = {0, maxDuty / 2, maxDuty, maxDuty + 1};

    for (uint8_t i = 0; i < 4; i++) {
        edgeStats.setDutyCalls++;
        bool status = ctrl.setDuty(samples[i]);
        if (!status) {
            edgeStats.setDutyFailures++;
            ok = false;
        }
    }

    return ok;
}

void runEdgeCampaign() {
    Serial.println(BOLD "\n[EDGE] Starting deterministic edge campaign" RESET);

    PwmModeRequest modes[] = {
        PwmModeRequest::Auto,
        PwmModeRequest::LowSpeed,
        PwmModeRequest::HighSpeed
    };

    uint8_t pinCursor = 0;
    for (uint8_t modeIndex = 0; modeIndex < 3; modeIndex++) {
        for (uint8_t freqIndex = 0; freqIndex < (sizeof(FREQS) / sizeof(FREQS[0])); freqIndex++) {
            edgeStats.totalRequests++;

            uint8_t pin = VALID_PINS[pinCursor % (sizeof(VALID_PINS) / sizeof(VALID_PINS[0]))];
            pinCursor++;
            uint32_t freq = FREQS[freqIndex];
            PwmModeRequest requestMode = modes[modeIndex];

            auto res = PwmBroker::getInstance().requestResource(pin, freq, requestMode);

            if (!res) {
                edgeStats.deniedRequests++;
                continue;
            }

            edgeStats.successfulRequests++;

            if (res->getFrequency() != freq) {
                edgeStats.integrityWarnings++;
            }

            if (!exerciseDutyExtremes(*res)) {
                edgeStats.integrityWarnings++;
            }

            resources.push_back({std::move(res), pin, freq, requestMode});

            if (resources.size() >= MAX_CHANNELS) {
                clearAllResources();
            }
        }
    }

    clearAllResources();

    Serial.print(BOLD "[EDGE] Requests: " RESET); Serial.println(edgeStats.totalRequests);
    Serial.print(BOLD "[EDGE] Success : " RESET); Serial.println(edgeStats.successfulRequests);
    Serial.print(BOLD "[EDGE] Denied  : " RESET); Serial.println(edgeStats.deniedRequests);
    Serial.print(BOLD "[EDGE] setDuty : " RESET); Serial.println(edgeStats.setDutyCalls);
    Serial.print(BOLD "[EDGE] Duty KO : " RESET); Serial.println(edgeStats.setDutyFailures);
    Serial.print(BOLD "[EDGE] Warn    : " RESET); Serial.println(edgeStats.integrityWarnings);
    Serial.println(BOLD "[EDGE] Campaign done\n" RESET);
}

void printPadded(String val, int width) {
    Serial.print(val);
    for (int i = 0; i < width - (int)val.length(); i++) Serial.print(" ");
}

void displayDashboard() {
    Serial.print(CLEAR);
    Serial.println(BOLD "=== PWM BROKER INDUSTRIAL STRESS TEST ===" RESET);
    Serial.println("ID | PIN | FREQ    | MODE | CH | TMR | MAX_DUTY | STATUS");
    Serial.println("----------------------------------------------------------------");

    for (int i = 0; i < POOL_SIZE; i++) {
        Serial.print(i < 10 ? "0" : ""); Serial.print(i); Serial.print(" | ");

        if (i < (int)resources.size()) {
            auto& r = resources[i];
            printPadded(String(r.control->getPwmPin()), 4); Serial.print("| ");
            printPadded(String(r.control->getFrequency()), 8); Serial.print("| ");
            printPadded(String(modeLabel(r.requestedMode)), 5); Serial.print("| ");
            printPadded(String(r.control->getChannel()), 3); Serial.print("| ");
            printPadded(String(r.control->getTimer()), 4); Serial.print("| ");
            printPadded(String(r.control->getMaxDuty()), 9); Serial.print("| ");
            Serial.println(GREEN "ACTIVE" RESET);
        } else {
            String status = (i < 16) ? "IDLE" : RED "OVER" RESET;
            Serial.print("-   | -       | -    | -  | -   | -        | ");
            Serial.println(status);
        }
    }

    Serial.println("----------------------------------------------------------------");
    Serial.print(BOLD "Last Action: " RESET); 
    Serial.print(lastActionBuffer); 
    Serial.println("                    "); 

    bool timersUsed[4] = {false, false, false, false};
    int timerCount = 0;
    for(auto& r : resources) {
        int t = r.control->getTimer();
        if(t >= 0 && t < 4 && !timersUsed[t]) {
            timersUsed[t] = true;
            timerCount++;
        }
    }

    Serial.print(BOLD "Stats: " RESET); 
    Serial.print(resources.size()); Serial.print("/16 Channels | ");
    Serial.print(timerCount); Serial.println("/4 Timers in use" RESET);
}

void runBruteForceStep() {
    int action = random(0, 100);
    
    // Stratégie de remplissage agressif (90% création tant qu'on a de la place)
    int probabilityToLease = (resources.size() < 16) ? 90 : 15;

    if (action < probabilityToLease && resources.size() < POOL_SIZE) {
        uint8_t targetPin = VALID_PINS[random(0, sizeof(VALID_PINS) / sizeof(VALID_PINS[0]))];
        uint32_t targetFreq = FREQS[random(0, sizeof(FREQS) / sizeof(FREQS[0]))];
        PwmModeRequest requestedMode = (PwmModeRequest)random(0, 3);

        auto res = PwmBroker::getInstance().requestResource(targetPin, targetFreq, requestedMode);
        
        if (res) {
            (void)exerciseDutyExtremes(*res);
            resources.push_back({std::move(res), targetPin, targetFreq, requestedMode});
            snprintf(lastActionBuffer, sizeof(lastActionBuffer), GREEN "LEASE %s Pin %d (%u Hz)" RESET, modeLabel(requestedMode), targetPin, targetFreq);
        } else {
            snprintf(lastActionBuffer, sizeof(lastActionBuffer), YELLOW "DENY %s %u Hz" RESET, modeLabel(requestedMode), targetFreq);
        }
    } 
    else if (!resources.empty()) {
        // --- LOGIQUE DE DÉZALOUAGE ÉVOLUÉE ---
        int index;
        int subAction = random(0, 100);
        
        if (subAction < 20) {
            // 20% de chance de supprimer le TOUT PREMIER lease (ID 00)
            index = 0;
            snprintf(lastActionBuffer, sizeof(lastActionBuffer), CYAN "REL OLD (ID 00) Pin %d" RESET, resources[index].pin);
        } else {
            // 80% de chance de supprimer au hasard
            index = random(0, resources.size());
            snprintf(lastActionBuffer, sizeof(lastActionBuffer), CYAN "REL RND (ID %02d) Pin %d" RESET, index, resources[index].pin);
        }
        
        resources.erase(resources.begin() + index);
    }

    displayDashboard();
}

void setup() {
    Serial.begin(115200);
    randomSeed(analogRead(0));
    Serial.print(CLEAR); 
    delay(1000);
    runEdgeCampaign();
}

void loop() {
    runBruteForceStep();
    delay(250); 
}
