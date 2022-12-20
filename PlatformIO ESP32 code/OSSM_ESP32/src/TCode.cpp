// Adapted from
// OSR-Alpha4_ESP32
// by TempestMAx 5-3-22
// Please copy, share, learn, innovate, give attribution.
#pragma once
#include <Arduino.h>
#include <EEPROM.h>

#include <list>

#define MIN_SMOOTH_INTERVAL 3   // Minimum auto-smooth ramp interval for live commands (ms)
#define MAX_SMOOTH_INTERVAL 100 // Maximum auto-smooth ramp interval for live commands (ms)
#define CHANNELS 1

// -----------------------------
// Class to handle each axis
// -----------------------------
class Axis
{
   public:
    // Setup function
    Axis()
    {
        // Set default dynamic parameters
        rampStartTime = 0;
        rampStart = 5000;
        rampStopTime = rampStart;
        rampStop = rampStart;

        // Set Empty Name
        Name = "";
        lastT = 0;

        // Live command auto-smooth
        minInterval = MAX_SMOOTH_INTERVAL;
    }

    // Function to set the axis dynamic parameters
    void Set(int x, char ext, long y)
    {
        unsigned long t = millis(); // This is the time now
        x = constrain(x, 0, 9999);
        y = constrain(y, 0, 9999999);
        // Set ramp parameters, based on inputs
        // Live command
        if (y == 0 || (ext != 'S' && ext != 'I'))
        {
            // update auto-smooth regulator
            int lastInterval = t - rampStartTime;
            if (lastInterval > minInterval && minInterval < MAX_SMOOTH_INTERVAL)
            {
                minInterval += 1;
            }
            else if (lastInterval < minInterval && minInterval > MIN_SMOOTH_INTERVAL)
            {
                minInterval -= 1;
            }
            // Set ramp parameters
            rampStart = GetPosition();
            rampStopTime = t + minInterval;
        }
        // Speed command
        else if (ext == 'S')
        {
            rampStart = GetPosition(); // Start from current position
            int d = x - rampStart;     // Distance to move
            if (d < 0)
            {
                d = -d;
            }
            long dt = d; // Time interval (time = dist/speed)
            dt *= 100;
            dt /= y;
            rampStopTime = t + dt; // Time to arrive at new position
                                   // if (rampStopTime < t + minInterval) { rampStopTime = t + minInterval; }
        }
        // Interval command
        else if (ext == 'I')
        {
            rampStart = GetPosition(); // Start from current position
            rampStopTime = t + y;      // Time to arrive at new position
                                       // if (rampStopTime < t + minInterval) { rampStopTime = t + minInterval; }
        }
        rampStartTime = t;
        rampStop = x;
        lastT = t;
    }

    // Function to return the current position of this axis
    int GetPosition()
    {
        int x; // This is the current axis position, 0-9999
        unsigned long t = millis();
        if (t > rampStopTime)
        {
            x = rampStop;
        }
        else if (t > rampStartTime)
        {
            x = map(t, rampStartTime, rampStopTime, rampStart, rampStop);
        }
        else
        {
            x = rampStart;
        }
        x = constrain(x, 0, 9999);
        return x;
    }

    // Function to stop axis movement at current position
    void Stop()
    {
        unsigned long t = millis(); // This is the time now
        rampStart = GetPosition();
        rampStartTime = t;
        rampStop = rampStart;
        rampStopTime = t;
    }

    // Public variables
    String Name;         // Function name of this axis
    unsigned long lastT; //

   private:
    // Movement positions
    int rampStart;
    unsigned long rampStartTime;
    int rampStop;
    unsigned long rampStopTime;

    // Live command auto-smooth regulator
    int minInterval;
};

// -----------------------------
// Class to manage Toy Comms
// -----------------------------
class TCode
{
   public:
    // Setup function
    TCode(String firmware, String tcode)
    {
        firmwareID = firmware;
        tcodeID = tcode;

        // Vibe channels start at 0
        for (int i = 0; i < CHANNELS; i++)
        {
            Vibration[i].Set(0, ' ', 0);
        }
    }

    // Function to name and activate axis
    void RegisterAxis(String ID, String axisName)
    {
        char type = ID.charAt(0);
        int channel = ID.charAt(1) - '0';
        if ((0 <= channel && channel < CHANNELS))
        {
            switch (type)
            {
                // Axis commands
                case 'L':
                    Linear[channel].Name = axisName;
                    break;
                case 'R':
                    Rotation[channel].Name = axisName;
                    break;
                case 'V':
                    Vibration[channel].Name = axisName;
                    break;
                case 'A':
                    Auxiliary[channel].Name = axisName;
                    break;
            }
        }
    }

    // Function to read off individual bytes as input
    void ByteInput(byte inByte)
    {
        bufferString += (char)inByte; // Add new character to string

        if (inByte == '\n')
        {                                // Execute string on newline
            bufferString.trim();         // Remove spaces, etc, from buffer
            executeString(bufferString); // Execute string
            bufferString = "";           // Clear input string
        }
    }

    // Function to read off whole strings as input
    void StringInput(String inString)
    {
        bufferString = inString;     // Replace existing buffer with input string
        bufferString.trim();         // Remove spaces, etc, from buffer
        executeString(bufferString); // Execute string
        bufferString = "";           // Clear input string
    }

    // Function to set an axis
    void AxisInput(String ID, int magnitude, char extension, long extMagnitude)
    {
        char type = ID.charAt(0);
        int channel = ID.charAt(1) - '0';
        if ((0 <= channel && channel < CHANNELS))
        {
            switch (type)
            {
                // Axis commands
                case 'L':
                    Linear[channel].Set(magnitude, extension, extMagnitude);
                    break;
                case 'R':
                    Rotation[channel].Set(magnitude, extension, extMagnitude);
                    break;
                case 'V':
                    Vibration[channel].Set(magnitude, extension, extMagnitude);
                    break;
                case 'A':
                    Auxiliary[channel].Set(magnitude, extension, extMagnitude);
                    break;
            }
        }
    }

    // Function to read the current position of an axis
    int AxisRead(String ID)
    {
        int x = 5000; // This is the return variable
        char type = ID.charAt(0);
        int channel = ID.charAt(1) - '0';
        if ((0 <= channel && channel < CHANNELS))
        {
            switch (type)
            {
                // Axis commands
                case 'L':
                    x = Linear[channel].GetPosition();
                    break;
                case 'R':
                    x = Rotation[channel].GetPosition();
                    break;
                case 'V':
                    x = Vibration[channel].GetPosition();
                    break;
                case 'A':
                    x = Auxiliary[channel].GetPosition();
                    break;
            }
        }
        return x;
    }

    // Function to query when an axis was last commanded
    unsigned long AxisLast(String ID)
    {
        unsigned long t = 0; // Return time
        char type = ID.charAt(0);
        int channel = ID.charAt(1) - '0';
        if ((0 <= channel && channel < CHANNELS))
        {
            switch (type)
            {
                // Axis commands
                case 'L':
                    t = Linear[channel].lastT;
                    break;
                case 'R':
                    t = Rotation[channel].lastT;
                    break;
                case 'V':
                    t = Vibration[channel].lastT;
                    break;
                case 'A':
                    t = Auxiliary[channel].lastT;
                    break;
            }
        }
        return t;
    }

   private:
    // Strings
    String firmwareID;
    String tcodeID;
    String bufferString; // String to hold incomming commands

    // Declare axes
    Axis Linear[CHANNELS];
    Axis Rotation[CHANNELS];
    Axis Vibration[CHANNELS];
    Axis Auxiliary[CHANNELS];

    // Function to divide up and execute input string
    void executeString(String bufferString)
    {
        //    //debug
        //    Serial.print("executestring: ");
        //    Serial.println(bufferString.c_str());
        int index = bufferString.indexOf(' '); // Look for spaces in string
        while (index > 0)
        {
            readCmd(bufferString.substring(0, index));        // Read off first command
            bufferString = bufferString.substring(index + 1); // Remove first command from string
            bufferString.trim();
            index = bufferString.indexOf(' '); // Look for next space
        }
        readCmd(bufferString); // Read off last command
    }

    // Function to process the individual commands
    void readCmd(String command)
    {
        command.toUpperCase();

        // Switch between command types
        switch (command.charAt(0))
        {
            // Axis commands
            case 'L':
            case 'R':
            case 'V':
            case 'A':
                axisCmd(command);
                break;

            // Device commands
            case 'D':
                deviceCmd(command);
                break;

            // Setup commands
            case '$':
                setupCmd(command);
                break;
        }
    }

    // Function to read and interpret axis commands
    void axisCmd(String command)
    {
        char type = command.charAt(0); // Type of command - LRVA
        boolean valid = true;          // Command validity flag, valid by default

        // Check for channel number
        int channel = command.charAt(1) - '0';
        if (channel < 0 || channel >= CHANNELS)
        {
            valid = false;
        }
        channel = constrain(channel, 0, CHANNELS);

        // Check for an extension
        char extension = ' ';
        int index = command.indexOf('S', 2);
        if (index > 0)
        {
            extension = 'S';
        }
        else
        {
            index = command.indexOf('I', 2);
            if (index > 0)
            {
                extension = 'I';
            }
        }
        if (index < 0)
        {
            index = command.length();
        }

        // Get command magnitude
        String magString = command.substring(2, index);
        magString = magString.substring(0, 4);
        while (magString.length() < 4)
        {
            magString += '0';
        }
        int magnitude = magString.toInt();
        if (magnitude == 0 && magString.charAt(0) != '0')
        {
            valid = false;
        } // Invalidate if zero returned, but not a number

        // Get extension magnitude
        long extMagnitude = 0;
        if (extension != ' ')
        {
            magString = command.substring(index + 1);
            magString = magString.substring(0, 8);
            extMagnitude = magString.toInt();
        }
        if (extMagnitude == 0)
        {
            extension = ' ';
        }

        // Switch between command types
        if (valid)
        {
            switch (type)
            {
                // Axis commands
                case 'L':
                    Linear[channel].Set(magnitude, extension, extMagnitude);
                    break;
                case 'R':
                    Rotation[channel].Set(magnitude, extension, extMagnitude);
                    break;
                case 'V':
                    Vibration[channel].Set(magnitude, extension, extMagnitude);
                    break;
                case 'A':
                    Auxiliary[channel].Set(magnitude, extension, extMagnitude);
                    break;
            }
        }
    }

    // Function to identify and execute device commands
    void deviceCmd(String command)
    {
        int i;
        // Remove "D"
        command = command.substring(1);

        // Look for device stop command
        if (command.substring(0, 4) == "STOP")
        {
            for (i = 0; i < 10; i++)
            {
                Linear[i].Stop();
            }
            for (i = 0; i < 10; i++)
            {
                Rotation[i].Stop();
            }
            for (i = 0; i < 10; i++)
            {
                Vibration[i].Set(0, ' ', 0);
            }
            for (i = 0; i < 10; i++)
            {
                Auxiliary[i].Stop();
            }
        }
        else
        {
            // Look for numbered device commands
            int commandNumber = command.toInt();
            if (commandNumber == 0 && command.charAt(0) != '0')
            {
                command = -1;
            }
            switch (commandNumber)
            {
                case 0:
                    Serial.println(firmwareID);
                    break;

                case 1:
                    Serial.println(tcodeID);
                    break;

                case 2:
                    for (i = 0; i < 10; i++)
                    {
                        axisRow("L" + String(i), 8 * i, Linear[i].Name);
                    }
                    for (i = 0; i < 10; i++)
                    {
                        axisRow("R" + String(i), 8 * i + 80, Rotation[i].Name);
                    }
                    for (i = 0; i < 10; i++)
                    {
                        axisRow("V" + String(i), 8 * i + 160, Vibration[i].Name);
                    }
                    for (i = 0; i < 10; i++)
                    {
                        axisRow("A" + String(i), 8 * i + 240, Auxiliary[i].Name);
                    }
                    break;
            }
        }
    }

    // Function to modify axis preference values
    void setupCmd(String command)
    {
        int minVal, maxVal;
        String minValString, maxValString;
        boolean valid;
        // Axis type
        char type = command.charAt(1);
        switch (type)
        {
            case 'L':
            case 'R':
            case 'V':
            case 'A':
                valid = true;
                break;

            default:
                type = ' ';
                valid = false;
                break;
        }
        // Axis channel number
        int channel = (command.substring(2, 3)).toInt();
        if (channel == 0 && command.charAt(2) != '0')
        {
            valid = false;
        }
        // Input numbers
        int index1 = command.indexOf('-');
        if (index1 != 3)
        {
            valid = false;
        }
        int index2 = command.indexOf('-', index1 + 1); // Look for spaces in string
        if (index2 <= 3)
        {
            valid = false;
        }
        if (valid)
        {
            // Min value
            minValString = command.substring(4, index2);
            minValString = minValString.substring(0, 4);
            while (minValString.length() < 4)
            {
                minValString += '0';
            }
            minVal = minValString.toInt();
            if (minVal == 0 && minValString.charAt(0) != '0')
            {
                valid = false;
            }
            // Max value
            maxValString = command.substring(index2 + 1);
            maxValString = maxValString.substring(0, 4);
            while (maxValString.length() < 4)
            {
                maxValString += '0';
            }
            maxVal = maxValString.toInt();
            if (maxVal == 0 && maxValString.charAt(0) != '0')
            {
                valid = false;
            }
        }
        // If a valid command, save axis preferences to EEPROM
        if (valid)
        {
            // offset for ossm settings
            int memIndex = 28;
            switch (type)
            {
                case 'L':
                    memIndex = memIndex + 0;
                    break;
                case 'R':
                    memIndex = memIndex + 80;
                    break;
                case 'V':
                    memIndex = memIndex + 160;
                    break;
                case 'A':
                    memIndex = memIndex + 240;
                    break;
            }
            memIndex += 8 * channel;
            minVal = constrain(minVal, 0, 9999);
            EEPROM.put(memIndex, minVal - 1);
            minVal = constrain(maxVal, 0, 9999);
            EEPROM.put(memIndex + 4, maxVal - 10000);
            // Output that axis changed successfully
            switch (type)
            {
                case 'L':
                    axisRow("L" + String(channel), memIndex, Linear[channel].Name);
                    break;
                case 'R':
                    axisRow("R" + String(channel), memIndex, Rotation[channel].Name);
                    break;
                case 'V':
                    axisRow("V" + String(channel), memIndex, Vibration[channel].Name);
                    break;
                case 'A':
                    axisRow("A" + String(channel), memIndex, Auxiliary[channel].Name);
                    break;
            }
        }
    }

    // Function to print the details of an axis
    void axisRow(String axisID, int memIndex, String axisName)
    {
        int low, high;
        if (axisName != "")
        {
            EEPROM.get(memIndex, low);
            low = constrain(low, -1, 9998);
            EEPROM.get(memIndex + 4, high);
            high = constrain(high, -10000, -1);
            Serial.print(axisID);
            Serial.print(" ");
            Serial.print(low + 1);
            Serial.print(" ");
            Serial.print(high + 10000);
            Serial.print(" ");
            Serial.println(axisName);
        }
    }
};
