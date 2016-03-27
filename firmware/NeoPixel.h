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
#ifndef NEO_PIXEL_H_
#define NEO_PIXEL_H_

#include <mbed.h>
#include "GPDMA.h"


// The RGB data for each LED is packed into this structure.
struct RGBData
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};


class NeoPixel : public SPI
{
public:
    NeoPixel(uint32_t ledCount, PinName outputPin);
    ~NeoPixel();

    uint32_t getFrameCount();
    void     start();
    void     set(const RGBData* pRGB);

protected:
    void setConstantBitsInFrontBuffers();
    void setConstantBitsInFrontBuffer(uint8_t* pBuffer);
    void waitForNextFlipToComplete();
    void emitByte(uint8_t byte);

    static int __dmaInterruptHandler(void* pContext, uint32_t dmaInterruptStatus);
    int        dmaInterruptHandler(uint32_t dmaInterruptStatus);

    uint8_t*                m_pFrontBuffers[2];
    uint8_t*                m_pEmitBuffer;
    LPC_GPDMACH_TypeDef*    m_pChannelTx;
    DmaInterruptHandler     m_dmaHandler;
    DmaLinkedListItem       m_dmaListItems[2];
    volatile uint32_t       m_frameCount;
    uint32_t                m_channelTx;
    uint32_t                m_channelMask;
    uint32_t                m_sspTx;
    uint32_t                m_ledCount;
    uint32_t                m_ledBytes;
    uint32_t                m_packetSize;
    bool                    m_isStarted;
    bool                    m_isBufferContentsNew[2];
    uint8_t                 m_dummyRead;
};

#endif // NEO_PIXEL_H_
