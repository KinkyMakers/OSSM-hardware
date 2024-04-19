
#include "OssmUi/OssmUi.h"

#include "constants/Images.h"

void OssmUi::Setup() {
    display.begin();

    display.clearBuffer();
    display.drawXBMP(40, 14, 50, 50, Images::KMLogo);
    display.sendBuffer();
}

void OssmUi::UpdateMessage(const String& message_in) {
    // compute the string width to center it later.
    display.setFont(u8g2_font_helvR08_tf);

    int x =
        (display.getDisplayWidth() - display.getUTF8Width(message_in.c_str())) /
        2;

    // draw inverted rectangle to clear the screen
    display.setColorIndex(0);
    display.drawBox(0, 0, 128, 12);
    display.setColorIndex(1);
    display.drawUTF8(x, 10, message_in.c_str());

    display.sendBuffer();
}

static String last_mode_label = "";
static int last_speed_percentage = 0;
static int last_encoder_position = 0;

void OssmUi::UpdateState(const String& mode_label, const int speed_percentage,
                         const int encoder_position) {
    if (last_mode_label == mode_label &&
        last_speed_percentage == speed_percentage &&
        last_encoder_position == encoder_position) {
        return;
    }
    last_encoder_position = encoder_position;
    last_speed_percentage = speed_percentage;
    last_mode_label = mode_label;

    // clear the left area of the screen, below 12 px
    display.setColorIndex(0);
    display.drawBox(0, 12, 24, 64);

    // clear the right area of the screen below 12 px
    display.drawBox(104, 12, 24, 64);

    // Clear the label area
    display.drawBox(0, 0, 128, 12);

    // draw the speed percentage rect
    display.setColorIndex(1);
    int speed_height = (64 - 12) * speed_percentage / 100;
    display.drawBox(0, 64 - speed_height, 24, speed_height);

    // Speed gradation
    display.setColorIndex(2);         // XOR mode
    display.drawLine(0, 12, 23, 12);  // 100%
    display.drawLine(0, 25, 6, 25);   // 75%
    display.drawLine(0, 38, 12, 38);  // 50%
    display.drawLine(0, 51, 6, 51);   // 25%
    display.drawLine(0, 63, 23, 63);  // 0%

    // draw the encoder position rect
    display.setColorIndex(1);
    int height = constrain((64 - 12) * encoder_position / 100, 0, 64 - 12);
    display.drawBox(128 - 24, 64 - height, 24, height);

    // Encoder position gradation (this should be obvious, but I want it
    // symetrical!)
    display.setColorIndex(2);                 // XOR mode
    display.drawLine(128, 12, 128 - 23, 12);  // 100%
    display.drawLine(128, 25, 128 - 6, 25);   // 75%
    display.drawLine(128, 38, 128 - 12, 38);  // 50%
    display.drawLine(128, 51, 128 - 6, 51);   // 25%
    display.drawLine(128, 63, 128 - 23, 63);  // 0%

    display.setColorIndex(1);
    display.setFont(u8g2_font_helvR08_tf);
    display.drawUTF8(0, 10, speed_percentage == 0 ? "STOPPED" : "SPEED");

    int width = display.getUTF8Width(mode_label.c_str());
    display.drawUTF8(128 - width, 10, mode_label.c_str());

    display.sendBuffer();
}
