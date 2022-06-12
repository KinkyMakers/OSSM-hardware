#include "Utilities.h"

void OSSM::setup()
{
    LogDebug("Software version");
    LogDebug(SW_VERSION);
    g_ui.Setup();
    g_ui.UpdateOnly();
    delay(50);
    g_ui.UpdateMessage("Booting up!");
#ifdef INITIAL_SETUP
    FastLED.setBrightness(150);
    fill_rainbow(leds, NUM_LEDS, 34, 1);
    FastLED.show();
    writeEepromSettings();
    WiFi.begin("IoT_PHB", "penthouseb"); // donthackmyguestnetworkplz
    wifiAutoConnect();
    updateFirmware();
#endif
    readEepromSettings();
    initializeStepperParameters();
    initializeInputs();
    strcpy(Id, ossmId);
    wifiAutoConnect();
    delay(500);
    if (checkForUpdate() == true)
    {
        updatePrompt();
    };
}

void OSSM::wifiAutoConnect()
{
    wm.setConfigPortalTimeout(1);
    if (!wm.autoConnect("OSSM Setup"))
    {
        LogDebug("failed to connect and hit timeout");
    }
    LogDebug("exiting autoconnect");
}

void OSSM::wifiConnectOrHotspotBlocking()
{
    // This should always be run in a thread!!!
    wm.setConfigPortalTimeout(120);
    wm.setConfigPortalBlocking(false);
    // here we try to connect to WiFi or launch settings hotspot for you to enter WiFi credentials
    if (!wm.autoConnect("OSSM setup"))
    {
        // TODO: Set Status LED to indicate failure
        LogDebug("failed to connect and hit timeout");
    }
    else
    {
        // TODO: Set Status LED to indicate everything is ok!
        LogDebug("Connected!");
    }
    for (;;)
    {
        wm.process();
        vTaskDelay(1);
        // delete this task once connected!
        if (WiFi.status() == WL_CONNECTED)
        {
            vTaskDelete(NULL);
        }
    }
}

void OSSM::updatePrompt()
{
    Serial.println("about to start httpOtaUpdate");
    if (WiFi.status() != WL_CONNECTED)
    {
        // return if no WiFi
        return;
    }
    if (checkForUpdate() == false)
    {
        return;
    }
    //   Tell user we are updating!

    g_ui.UpdateMessage("Press to update SW");

    if (waitForAnyButtonPress(5000) == false)
    {
        // user did not accept update
        return;
    }

    updateFirmware();
}
void OSSM::updateFirmware()
{
    FastLED.setBrightness(150);
    fill_rainbow(leds, NUM_LEDS, 192, 1);
    FastLED.show();
    g_ui.UpdateMessage("Updating - 1 minute...");

    WiFiClient client;
    t_httpUpdate_return ret = httpUpdate.update(client, "http://d2sy3zdr3r1gt5.cloudfront.net/ossmfirmware2.bin");
    // Or:
    // t_httpUpdate_return ret = httpUpdate.update(client, "server", 80, "file.bin");

    switch (ret)
    {
        case HTTP_UPDATE_FAILED:
            Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(),
                          httpUpdate.getLastErrorString().c_str());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("HTTP_UPDATE_OK");
            break;
    }
}

bool OSSM::checkForUpdate()
{
    String serverNameBubble = "http://d2g4f7zewm360.cloudfront.net/check-for-ossm-update"; // live url
#ifdef VERSIONTEST
    serverNameBubble = "http://d2oq8yqnezqh3r.cloudfront.net/check-for-ossm-update"; // version-test
#endif
    LogDebug("about to hit http for update");
    HTTPClient http;
    http.begin(serverNameBubble);
    http.addHeader("Content-Type", "application/json");
    StaticJsonDocument<200> doc;
    // Add values in the document
    doc["ossmSwVersion"] = SW_VERSION;

    String requestBody;
    serializeJson(doc, requestBody);
    LogDebug("about to POST");
    int httpResponseCode = http.POST(requestBody);
    LogDebug("POSTed");
    String payload = "{}";
    // int httpResponseCode = http.POST("{\"trainerSwVersion\":\"96\"}");
    payload = http.getString();
    LogDebug("HTTP Response code: ");
    LogDebug(httpResponseCode);
    LogDebug(payload);
    StaticJsonDocument<200> bubbleResponse;

    deserializeJson(bubbleResponse, payload);

    const char *status = bubbleResponse["status"]; // "success"
    bool response_needUpdate = bubbleResponse["response"]["needUpdate"];
    LogDebug(payload);
    const char *updateNeeded = bubbleResponse["response"]["needUpdate"];

    if (httpResponseCode <= 0)
    {
        LogDebug("Failed to reach update server");
    }
    http.end();
    return response_needUpdate;
}

bool OSSM::checkConnection()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }
    else
    {
        return true;
    }
}

void OSSM::initializeStepperParameters()
{
    stepper.connectToPins(MOTOR_STEP_PIN, MOTOR_DIRECTION_PIN);
    float stepsPerMm = motorStepPerRevolution / (pulleyToothCount * beltPitchMm);
    stepper.setStepsPerMillimeter(stepsPerMm);
    stepper.setLimitSwitchActive(LIMIT_SWITCH_PIN);
    stepper.startAsService(); // Kinky Makers - we have modified this function
    // from default library to run on core 1 and suggest you don't run anything else on that core.
}

void OSSM::initializeInputs()
{
    pinMode(MOTOR_ENABLE_PIN, OUTPUT);
    pinMode(WIFI_RESET_PIN, INPUT_PULLDOWN);
    pinMode(WIFI_CONTROL_TOGGLE_PIN, LOCAL_CONTROLLER); // choose between WIFI_CONTROLLER and LOCAL_CONTROLLER
    // Set analog pots (control knobs)
    pinMode(SPEED_POT_PIN, INPUT);
    adcAttachPin(SPEED_POT_PIN);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db); // allows us to read almost full 3.3V range
}

bool OSSM::findHome()
{
    if (hardwareVersion >= 20)
    {
        maxStrokeLengthMm = sensorlessHoming();
        if (maxStrokeLengthMm > 20)
        {
            return true;
        }
        return false;
    }
    else
    {
        sensorHoming();
        return true;
    }
    LogDebug("Homing returning");
}

float OSSM::sensorlessHoming()
{
    // find retracted position, mark as zero, find extended position, calc total length, subtract 2x offsets and record
    // length.
    //  move to offset and call it zero. homing complete.
    float currentLimit = 1.5;
    currentSensorOffset = (getAnalogAverage(36, 1000));
    float current = getAnalogAverage(36, 200) - currentSensorOffset;
    float measuredStrokeMm = 0;
    stepper.setAccelerationInMillimetersPerSecondPerSecond(1000);
    stepper.setDecelerationInMillimetersPerSecondPerSecond(10000);

    g_ui.UpdateMessage("Finding Home Sensorless");

    // disable motor briefly in case we are against a hard stop.
    digitalWrite(MOTOR_ENABLE_PIN, HIGH);
    delay(600);
    digitalWrite(MOTOR_ENABLE_PIN, LOW);
    delay(100);

    Serial.print(getAnalogAverage(36, 500) - currentSensorOffset);
    Serial.print(",");
    Serial.println(stepper.getCurrentPositionInMillimeters());

    // find reverse limit

    stepper.setSpeedInMillimetersPerSecond(25);
    stepper.setTargetPositionInMillimeters(-400);
    current = getAnalogAverage(36, 200) - currentSensorOffset;
    while (current < currentLimit)
    {
        current = getAnalogAverage(36, 25) - currentSensorOffset;
        Serial.print(current);
        Serial.print(",");
        Serial.println(stepper.getCurrentPositionInMillimeters());
    }
    // stepper.emergencyStop();

    stepper.setTargetPositionToStop();
    stepper.moveRelativeInMillimeters(strokeZeroOffsetmm); //"move to" is blocking
    stepper.setCurrentPositionAsHomeAndStop();
    g_ui.UpdateMessage("Checking Stroke");
    // int loop = 0;
    // while (loop < 100)
    // {
    //     current = getAnalogAverage(36, 25) - currentSensorOffset;
    //     Serial.print(current);
    //     Serial.print(",");
    //     Serial.println(stepper.getCurrentPositionInMillimeters());
    //     loop++;
    // }
    // stepper.setTargetPositionInMillimeters(6);
    // stepper.setCurrentPositionAsHomeAndStop();
    delay(100);

    // find forward limit

    stepper.setSpeedInMillimetersPerSecond(25);
    stepper.setTargetPositionInMillimeters(400);
    delay(300);
    current = getAnalogAverage(36, 200) - currentSensorOffset;
    while (current < currentLimit)
    {
        current = getAnalogAverage(36, 25) - currentSensorOffset;
        if (stepper.getCurrentPositionInMillimeters() > 90)
        {
            Serial.print(current);
            Serial.print(",");
            Serial.println(stepper.getCurrentPositionInMillimeters());
        }
    }

    stepper.setTargetPositionToStop();
    stepper.moveRelativeInMillimeters(-strokeZeroOffsetmm);
    measuredStrokeMm = -stepper.getCurrentPositionInMillimeters();
    stepper.setCurrentPositionAsHomeAndStop();
    Serial.print("Sensorless Homing complete!  ");
    Serial.print(measuredStrokeMm);
    Serial.println(" mm");
    g_ui.UpdateMessage("Homing Complete");
    // digitalWrite(MOTOR_ENABLE_PIN, HIGH);
    // delay(500);
    // digitalWrite(MOTOR_ENABLE_PIN, LOW);

    return measuredStrokeMm;
}
void OSSM::sensorHoming()
{
    // find limit switch and then move to end of stroke and call it zero
    stepper.setAccelerationInMillimetersPerSecondPerSecond(300);
    stepper.setDecelerationInMillimetersPerSecondPerSecond(10000);

    LogDebug("OSSM will now home");
    g_ui.UpdateMessage("Finding Home Switch");
    stepper.setSpeedInMillimetersPerSecond(15);
    stepper.moveToHomeInMillimeters(1, 25, 300, LIMIT_SWITCH_PIN);
    LogDebug("OSSM has homed, will now move out to max length");
    g_ui.UpdateMessage("Moving to Max");
    stepper.setSpeedInMillimetersPerSecond(10);
    stepper.moveToPositionInMillimeters((-1 * maxStrokeLengthMm) - strokeZeroOffsetmm);
    LogDebug("OSSM has moved out, will now set new home?");
    stepper.setCurrentPositionAsHomeAndStop();
    LogDebug("OSSM should now be home and happy");
}

int OSSM::readEepromSettings()
{
    LogDebug("read eeprom");
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(0, hardwareVersion);
    EEPROM.get(4, numberStrokes);
    EEPROM.get(12, travelledDistanceMeters);
    EEPROM.get(20, lifeSecondsPowered);

    return hardwareVersion;
}

void OSSM::writeEepromSettings()
{
    // Be very careful with this so you don't break your configuration!
    LogDebug("write eeprom");
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(0, HW_VERSION);
    EEPROM.put(4, 0);
    EEPROM.put(12, 0);
    EEPROM.put(20, 0);
    EEPROM.commit();
    LogDebug("eeprom written");
}
void OSSM::writeEepromLifeStats()
{
    // Be very careful with this so you don't break your configuration!
    LogDebug("writing eeprom life stats");
    Serial.printf("\nwriting eeprom life stats...\n");
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(4, numberStrokes);
    EEPROM.put(12, travelledDistanceMeters);
    EEPROM.put(20, lifeSecondsPoweredAtStartup);
    EEPROM.commit();
    LogDebug("eeprom written");
}

void OSSM::updateLifeStats()
{

    float minutes = 0;
    float hours = 0;
    float days = 0;
    float travelledDistanceKilometers = 0;

    travelledDistanceKilometers = (0.001 * travelledDistanceMeters);
    lifeSecondsPowered = (0.001 * millis()) + lifeSecondsPoweredAtStartup;
    minutes = lifeSecondsPowered / 60;
    hours = minutes / 60;
    days = hours / 24;
    if ((millis() - lastLifeUpdateMillis) > 5000)
    {
        Serial.printf("\n%dd %dh %dm %ds \n", ((int(days))), (int(hours) % 24), (int(minutes) % 60),
                      (int(lifeSecondsPowered) % 60));
        Serial.printf("%.0f strokes \n", numberStrokes);
        Serial.printf("%.2f kilometers \n", travelledDistanceKilometers);
        Serial.printf("%.2fA avg current \n", averageCurrent);
        lastLifeUpdateMillis = millis();
    }
    if ((millis() - lastLifeWriteMillis) > 180000)
    {
        // write eeprom every 3 minutes
        writeEepromLifeStats();
        lastLifeWriteMillis = millis();
    }

    return;
}

void OSSM::getAnalogInputs()
{
    speedPercentage = getAnalogAverage(SPEED_POT_PIN, 50);
    strokePercentage = getEncoderPercentage();
    immediateCurrent = getCurrentReadingAmps(20);
    averageCurrent = immediateCurrent * 0.02 + averageCurrent * 0.98;
}

float OSSM::getCurrentReadingAmps(int samples)
{
    float currentAnalogPercent = getAnalogAverage(36, samples) - currentSensorOffset;
    float current = currentAnalogPercent * 0.13886;
    // 0.13886 is a scaling factor determined by real life testing. Convert percent full scale to amps.
    return current;
}
float OSSM::getVoltageReading(int samples) {}

float OSSM::getEncoderPercentage()
{
    const int encoderFullScale = 100;
    int position = g_encoder.read();
    float outputPositionPercentage;
    if (position < 0)
    {
        g_encoder.write(0);
        position = 0;
    }
    else if (position > encoderFullScale)
    {
        g_encoder.write(encoderFullScale);
        position = encoderFullScale;
    }

    outputPositionPercentage = 100.0 * position / encoderFullScale;

    return outputPositionPercentage;
}

float OSSM::getAnalogAverage(int pinNumber, int samples)
{
    float sum = 0;
    float average = 0;
    float percentage = 0;
    for (int i = 0; i < samples; i++)
    {
        // TODO: Possibly use fancier filters?
        sum += analogRead(pinNumber);
    }
    average = sum / samples;
    // TODO: Might want to add a deadband
    percentage = 100.0 * average / 4096.0; // 12 bit resolution
    return percentage;
}

bool OSSM::waitForAnyButtonPress(float waitMilliseconds)
{
    float timeStartMillis = millis();
    bool initialEncoderFlag = encoderButtonToggle;
    LogDebug("Waiting for button press");
    while ((digitalRead(WIFI_RESET_PIN) == LOW) && (initialEncoderFlag == encoderButtonToggle))
    {
        if ((millis() - timeStartMillis) > waitMilliseconds)
        {
            LogDebug("button not pressed");

            return false;
        }
        delay(10);
    }
    LogDebug("button pressed");
    return true;
}
