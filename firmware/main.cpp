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
#include "Animation.h"
#include "NeoPixel.h"


#define LED_COUNT 5
#define ARRAY_SIZE(X) (sizeof(X)/sizeof(X[0]))

// The animations that I currently have defined for this sample.
enum Animations
{
    Solid_White,
    Solid_Red,
    Solid_Green,
    Solid_Red_Green,
    Solid_Red_Green_White,
    Solid_Red_Orange_Yellow_Green_Blue,
    Chase_Red_Green,
    Chase_Red_Green_White,
    Chase_Red_Orange_Yellow_Green_Blue,
    Throbbing_Red,
    Throbbing_Green,
    Throbbing_White,
    Fade_Red_Green,
    Fade_Red_Green_White,
    Fade_Red_Orange_Yellow_Green_Blue,
    Fade_Rainbow,
    Max_Animation
};

static Animations g_currAnimation = Throbbing_Red;
static Animation  g_animation;


// Function Prototypes.
static void updateAnimation();
static void advanceToNextAnimation();


int main()
{
    static DigitalOut myled(LED1);
    static NeoPixel   ledControl(LED_COUNT, p11);
    static Timer      timer;
    static Timer      ledTimer;

    updateAnimation();
    ledControl.start();

    timer.start();
    ledTimer.start();

    while(1)
    {

        if (timer.read_ms() > 2000)
        {
            timer.reset();
            advanceToNextAnimation();
        }
        g_animation.updatePixels(ledControl);

        if (ledTimer.read_ms() >= 250)
        {
            myled = !myled;
            ledTimer.reset();
        }
    }
}

static void updateAnimation()
{
    static RGBData           pixels1[LED_COUNT];
    static RGBData           pixels2[LED_COUNT];
    static RGBData           pixels3[LED_COUNT];
    static RGBData           pixels4[LED_COUNT];
    static RGBData           pixels5[LED_COUNT];
    static AnimationKeyFrame keyFrames[5];

    switch (g_currAnimation)
    {
    case Solid_White:
        {
            RGBData pattern[] = { WHITE };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern, ARRAY_SIZE(pattern));
            keyFrames[0] = {pixels1, 0x7FFFFFFF, false};
            g_animation.setKeyFrames(keyFrames, 1, LED_COUNT);
            break;
        }
    case Solid_Red:
        {
            RGBData pattern[] = { RED };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern, ARRAY_SIZE(pattern));
            keyFrames[0] = {pixels1, 0x7FFFFFFF, false};
            g_animation.setKeyFrames(keyFrames, 1, LED_COUNT);
            break;
        }
    case Solid_Green:
        {
            RGBData pattern[] = { GREEN };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern, ARRAY_SIZE(pattern));
            keyFrames[0] = {pixels1, 0x7FFFFFFF, false};
            g_animation.setKeyFrames(keyFrames, 1, LED_COUNT);
            break;
        }
    case Solid_Red_Green:
        {
            RGBData pattern[] = { RED, GREEN };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern, ARRAY_SIZE(pattern));
            keyFrames[0] = {pixels1, 0x7FFFFFFF, false};
            g_animation.setKeyFrames(keyFrames, 1, LED_COUNT);
            break;
        }
    case Solid_Red_Green_White:
        {
            RGBData pattern[] = { RED, GREEN, WHITE };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern, ARRAY_SIZE(pattern));
            keyFrames[0] = {pixels1, 0x7FFFFFFF, false};
            g_animation.setKeyFrames(keyFrames, 1, LED_COUNT);
            break;
        }
    case Solid_Red_Orange_Yellow_Green_Blue:
        {
            RGBData pattern[] = { RED, DARK_ORANGE, YELLOW, GREEN, BLUE };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern, ARRAY_SIZE(pattern));
            keyFrames[0] = {pixels1, 0x7FFFFFFF, false};
            g_animation.setKeyFrames(keyFrames, 1, LED_COUNT);
            break;
        }
    case Chase_Red_Green:
        {
            RGBData pattern1[] = { RED, GREEN };
            RGBData pattern2[] = { GREEN, RED };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern1, ARRAY_SIZE(pattern1));
            createRepeatingPixelPattern(pixels2, ARRAY_SIZE(pixels2), pattern2, ARRAY_SIZE(pattern2));
            keyFrames[0] = {pixels1, 250, false};
            keyFrames[1] = {pixels2, 250, false};
            g_animation.setKeyFrames(keyFrames, 2, LED_COUNT);
            break;
        }
    case Chase_Red_Green_White:
        {
            RGBData pattern1[] = { RED, GREEN, WHITE };
            RGBData pattern2[] = { WHITE, RED, GREEN };
            RGBData pattern3[] = { GREEN, WHITE, RED };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern1, ARRAY_SIZE(pattern1));
            createRepeatingPixelPattern(pixels2, ARRAY_SIZE(pixels2), pattern2, ARRAY_SIZE(pattern2));
            createRepeatingPixelPattern(pixels3, ARRAY_SIZE(pixels3), pattern3, ARRAY_SIZE(pattern3));
            keyFrames[0] = {pixels1, 250, false};
            keyFrames[1] = {pixels2, 250, false};
            keyFrames[2] = {pixels3, 250, false};
            g_animation.setKeyFrames(keyFrames, 3, LED_COUNT);
            break;
        }
    case Chase_Red_Orange_Yellow_Green_Blue:
        {
            RGBData pattern1[] = { RED, DARK_ORANGE, YELLOW, GREEN, BLUE };
            RGBData pattern2[] = { BLUE, RED, DARK_ORANGE, YELLOW, GREEN };
            RGBData pattern3[] = { GREEN, BLUE, RED, DARK_ORANGE, YELLOW };
            RGBData pattern4[] = { YELLOW, GREEN, BLUE, RED, DARK_ORANGE };
            RGBData pattern5[] = { DARK_ORANGE, YELLOW, GREEN, BLUE, RED };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern1, ARRAY_SIZE(pattern1));
            createRepeatingPixelPattern(pixels2, ARRAY_SIZE(pixels2), pattern2, ARRAY_SIZE(pattern2));
            createRepeatingPixelPattern(pixels3, ARRAY_SIZE(pixels3), pattern3, ARRAY_SIZE(pattern3));
            createRepeatingPixelPattern(pixels4, ARRAY_SIZE(pixels4), pattern4, ARRAY_SIZE(pattern4));
            createRepeatingPixelPattern(pixels5, ARRAY_SIZE(pixels5), pattern5, ARRAY_SIZE(pattern5));
            keyFrames[0] = {pixels1, 250, false};
            keyFrames[1] = {pixels2, 250, false};
            keyFrames[2] = {pixels3, 250, false};
            keyFrames[3] = {pixels4, 250, false};
            keyFrames[4] = {pixels5, 250, false};
            g_animation.setKeyFrames(keyFrames, 5, LED_COUNT);
            break;
        }
    case Throbbing_Red:
        {
            RGBData pattern1[] = { RGBData(8, 0, 0) };
            RGBData pattern2[] = { RGBData(255, 0, 0) };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern1, ARRAY_SIZE(pattern1));
            createRepeatingPixelPattern(pixels2, ARRAY_SIZE(pixels2), pattern2, ARRAY_SIZE(pattern2));
            keyFrames[0] = {pixels1, 1000, true};
            keyFrames[1] = {pixels2, 1000, true};
            g_animation.setKeyFrames(keyFrames, 2, LED_COUNT);
            break;
        }
    case Throbbing_Green:
        {
            RGBData pattern1[] = { RGBData(0, 8, 0) };
            RGBData pattern2[] = { RGBData(0, 255, 0) };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern1, ARRAY_SIZE(pattern1));
            createRepeatingPixelPattern(pixels2, ARRAY_SIZE(pixels2), pattern2, ARRAY_SIZE(pattern2));
            keyFrames[0] = {pixels1, 1000, true};
            keyFrames[1] = {pixels2, 1000, true};
            g_animation.setKeyFrames(keyFrames, 2, LED_COUNT);
            break;
        }
    case Throbbing_White:
        {
            RGBData pattern1[] = { RGBData(8, 8, 8) };
            RGBData pattern2[] = { RGBData(255, 255, 255) };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern1, ARRAY_SIZE(pattern1));
            createRepeatingPixelPattern(pixels2, ARRAY_SIZE(pixels2), pattern2, ARRAY_SIZE(pattern2));
            keyFrames[0] = {pixels1, 1000, true};
            keyFrames[1] = {pixels2, 1000, true};
            g_animation.setKeyFrames(keyFrames, 2, LED_COUNT);
            break;
        }
    case Fade_Red_Green:
        {
            RGBData pattern1[] = { RED, GREEN };
            RGBData pattern2[] = { GREEN, RED };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern1, ARRAY_SIZE(pattern1));
            createRepeatingPixelPattern(pixels2, ARRAY_SIZE(pixels2), pattern2, ARRAY_SIZE(pattern2));
            keyFrames[0] = {pixels1, 250, true};
            keyFrames[1] = {pixels2, 250, true};
            g_animation.setKeyFrames(keyFrames, 2, LED_COUNT);
            break;
        }
    case Fade_Red_Green_White:
        {
            RGBData pattern1[] = { RED, GREEN, WHITE };
            RGBData pattern2[] = { WHITE, RED, GREEN };
            RGBData pattern3[] = { GREEN, WHITE, RED };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern1, ARRAY_SIZE(pattern1));
            createRepeatingPixelPattern(pixels2, ARRAY_SIZE(pixels2), pattern2, ARRAY_SIZE(pattern2));
            createRepeatingPixelPattern(pixels3, ARRAY_SIZE(pixels3), pattern3, ARRAY_SIZE(pattern3));
            keyFrames[0] = {pixels1, 250, true};
            keyFrames[1] = {pixels2, 250, true};
            keyFrames[2] = {pixels3, 250, true};
            g_animation.setKeyFrames(keyFrames, 3, LED_COUNT);
            break;
        }
    case Fade_Red_Orange_Yellow_Green_Blue:
        {
            RGBData pattern1[] = { RED, DARK_ORANGE, YELLOW, GREEN, BLUE };
            RGBData pattern2[] = { BLUE, RED, DARK_ORANGE, YELLOW, GREEN };
            RGBData pattern3[] = { GREEN, BLUE, RED, DARK_ORANGE, YELLOW };
            RGBData pattern4[] = { YELLOW, GREEN, BLUE, RED, DARK_ORANGE };
            RGBData pattern5[] = { DARK_ORANGE, YELLOW, GREEN, BLUE, RED };
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern1, ARRAY_SIZE(pattern1));
            createRepeatingPixelPattern(pixels2, ARRAY_SIZE(pixels2), pattern2, ARRAY_SIZE(pattern2));
            createRepeatingPixelPattern(pixels3, ARRAY_SIZE(pixels3), pattern3, ARRAY_SIZE(pattern3));
            createRepeatingPixelPattern(pixels4, ARRAY_SIZE(pixels4), pattern4, ARRAY_SIZE(pattern4));
            createRepeatingPixelPattern(pixels5, ARRAY_SIZE(pixels5), pattern5, ARRAY_SIZE(pattern5));
            keyFrames[0] = {pixels1, 250, true};
            keyFrames[1] = {pixels2, 250, true};
            keyFrames[2] = {pixels3, 250, true};
            keyFrames[3] = {pixels4, 250, true};
            keyFrames[4] = {pixels5, 250, true};
            g_animation.setKeyFrames(keyFrames, 5, LED_COUNT);
            break;
        }
    case Fade_Rainbow:
        {
            RGBData pattern1[] = { RED };
            RGBData pattern2[] = { VIOLET };
            createInterpolatedPixelPattern(pixels1, ARRAY_SIZE(pixels1), pattern1, pattern2);
            createInterpolatedPixelPattern(pixels2, ARRAY_SIZE(pixels2), pattern2, pattern1);
            keyFrames[0] = {pixels1, 1000, true};
            keyFrames[1] = {pixels2, 1000, true};
            g_animation.setKeyFrames(keyFrames, 2, LED_COUNT);
            break;
        }
    default:
        {
            RGBData pattern = BLACK;
            createRepeatingPixelPattern(pixels1, ARRAY_SIZE(pixels1), &pattern, 1);
            keyFrames[0] = {pixels1, 0x7FFFFFFF, false};
            g_animation.setKeyFrames(keyFrames, 1, LED_COUNT);
            break;
        }
    }
}

static void advanceToNextAnimation()
{
    g_currAnimation = (Animations)(g_currAnimation + 1);
    if (g_currAnimation >= Max_Animation)
    {
        g_currAnimation = Solid_White;
    }
    updateAnimation();
}
