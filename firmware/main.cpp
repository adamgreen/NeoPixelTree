/* Copyright (C) 2016  Adam Green (https://github.com/adamgreen)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include <mbed.h>
#include "NeoPixel.h"


#define LED_COUNT 5


int main()
{
    static DigitalOut myled(LED1);
    static NeoPixel   ledControl(LED_COUNT, p11);
    RGBData           leds[LED_COUNT] = { {0xAA, 0x55, 0xAA}, {0x80, 0x80, 0x80}, {0x80, 0, 0}, {0, 0x80, 0}, {0, 0, 0x80} };
    static Timer      timer;
    uint32_t          lastFrameCount;

    ledControl.set(leds);
    ledControl.start();

    timer.start();
    lastFrameCount = ledControl.getFrameCount();

    // UNDONE:
    leds[3] = {0, 0, 0x80};
    ledControl.set(leds);
    while(1)
    {

        // Dump frame count every 5 seconds.
        if (timer.read_ms() > 5000)
        {
            uint32_t currFrameCount = ledControl.getFrameCount();
            timer.reset();
            printf("frame count: %.2f/second\n", (float)(currFrameCount - lastFrameCount) / 5.0f);
            lastFrameCount = currFrameCount;
        }

        myled = 1;
        wait(0.2);
        myled = 0;
        wait(0.2);
    }
}
