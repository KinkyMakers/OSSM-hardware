
#include "OssmUi/OssmUi.h"

#include "constants/Images.h"

void OssmUi::Setup()
{
    display.begin();

    display.clearBuffer();
    display.drawXBMP(40, 14, 50, 50, Images::KMLogo);
    display.sendBuffer();
}

void OssmUi::UpdateMessage(String message_in)
{
    // compute the string width to center it later.
    display.setFont(u8g2_font_helvR08_tf);

    int x = (display.getDisplayWidth() - display.getUTF8Width(message_in.c_str())) / 2;

    // draw inverted rectangle to clear the screen
    display.setColorIndex(0);
    display.drawBox(0, 0, 128, 12);
    display.setColorIndex(1);
    display.drawUTF8(x, 10, message_in.c_str());

    display.sendBuffer();
}
void OssmUi::UpdateState(String mode_label, const int speed_percentage, const int encoder_position)
{
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

    // draw the encoder position rect
    int height = (64 - 12) * encoder_position / 100;
    display.drawBox(128 - 24, 64 - height, 24, height);


    display.setFont(u8g2_font_helvR08_tf);
    display.drawUTF8(0, 10, "SPEED");

    int width = display.getUTF8Width(mode_label.c_str());
    display.drawUTF8(128 - width, 10, mode_label.c_str());

    display.sendBuffer();
}
