/**
 * @file main.cpp (MonkeyTest - Industrial Edition)
 * @brief Stress test avec pression sur le recyclage des premiers baux (leases).
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
const uint32_t FREQS[]     = {1000, 5000, 10000, 15000, 20000}; 

struct ActiveResource {
    std::unique_ptr<PwmControl> control;
    uint8_t pin;
    uint32_t freq;
};

std::vector<ActiveResource> resources;
char lastActionBuffer[128] = "Initializing fuzzer...";

void printPadded(String val, int width) {
    Serial.print(val);
    for (int i = 0; i < width - (int)val.length(); i++) Serial.print(" ");
}

void displayDashboard() {
    Serial.print("\033[H"); 
    Serial.println(BOLD "=== PWM BROKER INDUSTRIAL STRESS TEST ===" RESET);
    Serial.println("ID | PIN | FREQ    | CH | TMR | MAX_DUTY | STATUS");
    Serial.println("--------------------------------------------------");

    for (int i = 0; i < POOL_SIZE; i++) {
        Serial.print(i < 10 ? "0" : ""); Serial.print(i); Serial.print(" | ");

        if (i < (int)resources.size()) {
            auto& r = resources[i];
            printPadded(String(r.control->getPwmPin()), 4); Serial.print("| ");
            printPadded(String(r.control->getFrequency()), 8); Serial.print("| ");
            printPadded(String(r.control->getChannel()), 3); Serial.print("| ");
            printPadded(String(r.control->getTimer()), 4); Serial.print("| ");
            printPadded(String(r.control->getMaxDuty()), 9); Serial.print("| ");
            Serial.println(GREEN "ACTIVE" RESET);
        } else {
            String status = (i < 16) ? "IDLE" : RED "OVER" RESET;
            Serial.print("-   | -       | -  | -   | -        | ");
            Serial.println(status);
        }
    }

    Serial.println("--------------------------------------------------");
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
        uint8_t targetPin = VALID_PINS[random(0, sizeof(VALID_PINS))];
        uint32_t targetFreq = FREQS[random(0, 5)];

        auto res = PwmBroker::getInstance().requestResource(targetPin, targetFreq);
        
        if (res) {
            resources.push_back({std::move(res), targetPin, targetFreq});
            snprintf(lastActionBuffer, sizeof(lastActionBuffer), GREEN "LEASE Pin %d (%u Hz)" RESET, targetPin, targetFreq);
        } else {
            snprintf(lastActionBuffer, sizeof(lastActionBuffer), YELLOW "DENY %u Hz (Broker Full)" RESET, targetFreq);
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
}

void loop() {
    runBruteForceStep();
    delay(250); 
}
