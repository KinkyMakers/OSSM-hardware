#pragma once

#include "esp_log.h"
#include "esp32s3/rom/rtc.h"

void print_reset_reason(int reason)
{
  switch ( reason)
  {
    case 1 : ESP_LOGI("main", "POWERON_RESET");break;          /**<1,  Vbat power on reset*/
    case 3 : ESP_LOGI("main", "SW_RESET");break;               /**<3,  Software reset digital core*/
    case 4 : ESP_LOGI("main", "OWDT_RESET");break;             /**<4,  Legacy watch dog reset digital core*/
    case 5 : ESP_LOGI("main", "DEEPSLEEP_RESET");break;        /**<5,  Deep Sleep reset digital core*/
    case 6 : ESP_LOGI("main", "SDIO_RESET");break;             /**<6,  Reset by SLC module, reset digital core*/
    case 7 : ESP_LOGI("main", "TG0WDT_SYS_RESET");break;       /**<7,  Timer Group0 Watch dog reset digital core*/
    case 8 : ESP_LOGI("main", "TG1WDT_SYS_RESET");break;       /**<8,  Timer Group1 Watch dog reset digital core*/
    case 9 : ESP_LOGI("main", "RTCWDT_SYS_RESET");break;       /**<9,  RTC Watch dog Reset digital core*/
    case 10 : ESP_LOGI("main", "INTRUSION_RESET");break;       /**<10, Instrusion tested to reset CPU*/
    case 11 : ESP_LOGI("main", "TGWDT_CPU_RESET");break;       /**<11, Time Group reset CPU*/
    case 12 : ESP_LOGI("main", "SW_CPU_RESET");break;          /**<12, Software reset CPU*/
    case 13 : ESP_LOGI("main", "RTCWDT_CPU_RESET");break;      /**<13, RTC Watch dog Reset CPU*/
    case 14 : ESP_LOGI("main", "EXT_CPU_RESET");break;         /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : ESP_LOGI("main", "RTCWDT_BROWN_OUT_RESET");break;/**<15, Reset when the vdd voltage is not stable*/
    case 16 : ESP_LOGI("main", "RTCWDT_RTC_RESET");break;      /**<16, RTC Watch dog reset digital core and rtc module*/
    default : ESP_LOGI("main", "NO_MEAN");
  }
}

void print_reset_reason() {
  ESP_LOGI("main", "CPU0 reset reason:");
  print_reset_reason(rtc_get_reset_reason(0));

  ESP_LOGI("main", "CPU1 reset reason:");
  print_reset_reason(rtc_get_reset_reason(1));
}