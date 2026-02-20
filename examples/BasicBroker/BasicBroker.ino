#include <Arduino.h>
#include "PwmBroker.h"

void setup() {
    Serial.begin(115200);
    while (!Serial) {}

    Serial.println("\n===========================================");
    Serial.println("   PWM BROKER DEBUG BUILD ENTRYPOINT");
    Serial.println("===========================================");

    PwmBroker& broker = PwmBroker::getInstance();
    auto pwm = broker.requestResource(22, 1000);

    if (!pwm) {
        Serial.println("[ERR] Impossible d'allouer une ressource PWM.");
        return;
    }

    Serial.print("[OK] Channel: "); Serial.println(pwm->getChannel());
    Serial.print("[OK] Timer: "); Serial.println(pwm->getTimer());
    Serial.print("[OK] MaxDuty: "); Serial.println(pwm->getMaxDuty());

    pwm->setDuty(pwm->getMaxDuty() / 2);
}

void loop() {
    delay(1000);
}
