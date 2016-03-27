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
#ifndef ANIMATION_H_
#define ANIMATION_H_

#include <assert.h>
#include <mbed.h>
#include "NeoPixel.h"


struct AnimationKeyFrame
{
    RGBData* pPixels;
    int32_t  millisecondsBeforeNextFrame;
    bool     interpolateBetweenFrames;
};

class IPixelUpdate
{
public:
    virtual void updatePixels(NeoPixel& ledControl) = 0;
};

class Animation : public IPixelUpdate
{
public:
    Animation();

    void setKeyFrames(const AnimationKeyFrame* pFrames, size_t frameCount, size_t pixelCount);

    // IPixelUpdate methods.
    virtual void updatePixels(NeoPixel& ledControl);

    // Static methods used together to interpolate colour values.
    static void rgbToInterpolatableHsv(HSVData* pHsvDest, const RGBData* pRgbSrc);
    static void interpolateHsvToRgb(RGBData* pRgbDest, const HSVData* pHsvStart, const HSVData* pHsvStop,
                                    int32_t curr, int32_t total);
protected:
    void updatePixelsNonInterpolated(NeoPixel& ledControl);
    void updatePixelsInterpolated(NeoPixel& ledControl);
    void convertRgbPixelsToHsv(HSVData* pHsvDest, const RGBData* pRgbSrc, size_t pixelCount);
    void interpolateBetweenKeyFrames(int32_t currTime, int32_t totalTime);

    static uint8_t           s_powerTable[256];
    static uint8_t           s_logTable[256];
    const AnimationKeyFrame* m_pStart;
    const AnimationKeyFrame* m_pEnd;
    const AnimationKeyFrame* m_pCurr;
    const AnimationKeyFrame* m_pInterpolating;
    RGBData*                 m_pRgbPixels;
    HSVData*                 m_pHsvPrev;
    HSVData*                 m_pHsvNext;
    size_t                   m_pixelCount;
    Timer                    m_timer;
    int32_t                  m_lastRenderTime;
    bool                     m_dirty;
};

struct TwinkleProperties
{
    // The number of pixels in NeoPixel strip.
    size_t   pixelCount;
    // The twinkle should fade in and out in this number of milliseconds.
    uint32_t lifetimeMin;
    uint32_t lifetimeMax;
    // There should be a 1 in probability milliseconds chance of a twinkle starting.
    int      probability;
    // The colour of the twinkling LED should be constrained to these HSV limits.
    uint8_t  hueMin;
    uint8_t  hueMax;
    uint8_t  saturationMin;
    uint8_t  saturationMax;
    uint8_t  valueMin;
    uint8_t  valueMax;
};

class TwinkleAnimation : public IPixelUpdate
{
public:
    TwinkleAnimation();

    void setProperties(const TwinkleProperties* pProperties);

    // IPixelUpdate methods.
    virtual void updatePixels(NeoPixel& ledControl);

protected:
    struct PixelTwinkleInfo
    {
        uint32_t startTime;
        uint32_t lifetime;
        bool     isGettingBrighter;
    };
    void twinklePixel(RGBData* pRgbDest, const HSVData* pHsv, PixelTwinkleInfo* pInfo, uint32_t currTime);

    const TwinkleProperties* m_pProperties;
    RGBData*                 m_pRgbPixels;
    HSVData*                 m_pHsvPixels;
    PixelTwinkleInfo*        m_pTwinkleInfo;
    Timer                    m_timer;
    int32_t                  m_lastUpdate;
};


static inline void createRepeatingPixelPattern(RGBData* pDest, size_t destPixelCount,
                                               const RGBData* pPattern, size_t srcPixelCount)
{
    assert ( srcPixelCount > 0 );

    size_t         i = 0;
    const RGBData* pSrc = pPattern;
    while (destPixelCount--)
    {
        *pDest++ = *pSrc++;
        if (++i >= srcPixelCount)
        {
            i = 0;
            pSrc = pPattern;
        }
    }
}

static inline void createInterpolatedPixelPattern(RGBData* pDest, size_t destPixelCount,
                                                  const RGBData* pRgbStart, const RGBData* pRgbStop)
{
    int32_t totalPixels = (int32_t)destPixelCount;
    HSVData hsvStart;
    HSVData hsvStop;

    Animation::rgbToInterpolatableHsv(&hsvStart, pRgbStart);
    Animation::rgbToInterpolatableHsv(&hsvStop, pRgbStop);

    for (int32_t i = 0 ; i < totalPixels ; i++)
    {
        Animation::interpolateHsvToRgb(pDest++, &hsvStart, &hsvStop, i, totalPixels);
    }
}

#endif // ANIMATION_H_
