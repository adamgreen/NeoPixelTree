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
#include <assert.h>
#include <mbed.h>
#include "GPDMA.h"
#include "Interlocked.h"
#include "NeoPixel.h"


// This class utilizes DMA based SPI hardware to send data to NeoPixel LEDs.
// It was only coded to work on the LPC1768.
#ifndef TARGET_LPC176X
    #error("This NeoPixel class was only coded to work on the LPC1768.")
#endif



NeoPixel::NeoPixel(uint32_t ledCount, PinName outputPin) : SPI(outputPin, NC, NC)
{
    // Each NeoPixel data-bit should be 1.2 usec so use 12 SPI bits when running SPI at 10MHz.
    const uint32_t spiBitsPerNeoPixelBit = 12;
    const uint32_t bitsPerPixel = 24;
    // 500 * 1/10MHz = 50 usec.
    const uint32_t resetBits = 500;

    format(8, 3);
    frequency(10000000);

    m_frameCount = 0;
    m_isStarted = false;
    m_ledCount = ledCount;

    // Round up byte count.
    uint32_t ledBits = ledCount * bitsPerPixel * spiBitsPerNeoPixelBit;
    // The byte count dedicated to LED output data should be an even multiple of 3 bytes.
    assert ( (ledBits % 8) == 0 );
    m_ledBytes = ledBits / 8;
    m_packetSize = m_ledBytes + (resetBits + 7) / 8;

    // Place buffers used by DMA code in separate RAM bank to optimize performance.
    m_pFrontBuffers[0] = (uint8_t*)dmaHeap0Alloc(m_packetSize);
    m_pFrontBuffers[1] = (uint8_t*)dmaHeap1Alloc(m_packetSize);
    m_isBufferContentsNew[0] = false;
    m_isBufferContentsNew[1] = false;

    setConstantBitsInFrontBuffers();

    // Setup GPDMA module.
    enableGpdmaPower();
    enableGpdmaInLittleEndianMode();

    // Add this DMA handler to the linked list of DMA handlers.
    m_dmaHandler.handler = __dmaInterruptHandler;
    m_dmaHandler.pContext = (void*)this;
    addDmaInterruptHandler(&m_dmaHandler);
}

void NeoPixel::setConstantBitsInFrontBuffers()
{
    setConstantBitsInFrontBuffer(m_pFrontBuffers[0]);
    setConstantBitsInFrontBuffer(m_pFrontBuffers[1]);
}

void NeoPixel::setConstantBitsInFrontBuffer(uint8_t* pBuffer)
{
    // Each NeoPixel bit will be represented in 12 SPI bits.
    // The format of the 12 SPI bits will be:
    //    1111xxxx0000
    //    xxxx will be 0000 if NeoPixel bit is 0.
    //    xxxx will be 1111 if NeoPixel bit is 1.
    // Two NeoPixel bits will therefore be stored in 3 SPI bytes.
    //    1111xxxx 00001111 xxxx0000
    uint8_t* pEnd = pBuffer + m_ledBytes;
    while (pBuffer + 2 < pEnd)
    {
        // Fill in buffer 3 bytes at a time.
        *pBuffer++ = 0xF0;
        *pBuffer++ = 0x0F;
        *pBuffer++ = 0x00;
    }
    // The byte count dedicated to LED output data should be an even multiple of 3 bytes.
    assert ( pBuffer == pEnd );

    // Set frame reset bits to 0.
    memset(pBuffer, 0, m_packetSize - m_ledBytes);
}

NeoPixel::~NeoPixel()
{
    if (m_isStarted)
        freeDmaChannel(m_channelTx);
    removeDmaInterruptHandler(&m_dmaHandler);
}

uint32_t NeoPixel::getFrameCount()
{
    return m_frameCount;
}

void NeoPixel::start()
{
    // Allocate DMA channel for transmitting.
    m_channelTx = allocateDmaChannel(GPDMA_CHANNEL_LOW);
    m_pChannelTx = dmaChannelFromIndex(m_channelTx);
    m_sspTx = (_spi.spi == (LPC_SSP_TypeDef*)SPI_1) ? DMA_PERIPHERAL_SSP1_TX : DMA_PERIPHERAL_SSP0_TX;

    // Clear error and terminal complete interrupts for transmit channel.
    m_channelMask = 1 << m_channelTx;
    LPC_GPDMA->DMACIntTCClear = m_channelMask;
    LPC_GPDMA->DMACIntErrClr  = m_channelMask;

    // Prepare transmit channel DMA circular linked list to use 2 front buffers.
    m_dmaListItems[0].DMACCxSrcAddr  = (uint32_t)m_pFrontBuffers[0];
    m_dmaListItems[0].DMACCxDestAddr = (uint32_t)&_spi.spi->DR;
    m_dmaListItems[0].DMACCxLLI      = (uint32_t)&m_dmaListItems[1];
    m_dmaListItems[0].DMACCxControl  = DMACCxCONTROL_I | DMACCxCONTROL_SI |
                     (DMACCxCONTROL_BURSTSIZE_4 << DMACCxCONTROL_SBSIZE_SHIFT) |
                     (DMACCxCONTROL_BURSTSIZE_4 << DMACCxCONTROL_DBSIZE_SHIFT) |
                     (m_packetSize & DMACCxCONTROL_TRANSFER_SIZE_MASK);

    m_dmaListItems[1].DMACCxSrcAddr  = (uint32_t)m_pFrontBuffers[1];
    m_dmaListItems[1].DMACCxDestAddr = (uint32_t)&_spi.spi->DR;
    m_dmaListItems[1].DMACCxLLI      = (uint32_t)&m_dmaListItems[0];
    m_dmaListItems[1].DMACCxControl  = DMACCxCONTROL_I | DMACCxCONTROL_SI |
                     (DMACCxCONTROL_BURSTSIZE_4 << DMACCxCONTROL_SBSIZE_SHIFT) |
                     (DMACCxCONTROL_BURSTSIZE_4 << DMACCxCONTROL_DBSIZE_SHIFT) |
                     (m_packetSize & DMACCxCONTROL_TRANSFER_SIZE_MASK);

    m_pChannelTx->DMACCSrcAddr  = m_dmaListItems[0].DMACCxSrcAddr;
    m_pChannelTx->DMACCDestAddr = m_dmaListItems[0].DMACCxDestAddr;
    m_pChannelTx->DMACCLLI      = m_dmaListItems[0].DMACCxLLI;
    m_pChannelTx->DMACCControl  = m_dmaListItems[0].DMACCxControl;

    // Enable transmit channel.
    m_pChannelTx->DMACCConfig = DMACCxCONFIG_ENABLE |
                   (m_sspTx << DMACCxCONFIG_DEST_PERIPHERAL_SHIFT) |
                   DMACCxCONFIG_TRANSFER_TYPE_M2P |
                   DMACCxCONFIG_IE |
                   DMACCxCONFIG_ITC;

    // Turn on DMA transmit requests in SSP.
    _spi.spi->DMACR = (1 << 1);

    m_isStarted = true;
}

void NeoPixel::set(const RGBData* pRGB)
{
    waitForNextFlipToComplete();

    uint32_t bufferToUpdate = !(m_frameCount & 1);
    // Emit bits into the front buffer not currently being sent over SPI.
    m_pEmitBuffer = m_pFrontBuffers[bufferToUpdate];
    for (uint32_t i = 0 ; i < m_ledCount ; i++)
    {
        RGBData led = *pRGB++;

        emitByte(led.red);
        emitByte(led.green);
        emitByte(led.blue);
    }
    m_isBufferContentsNew[bufferToUpdate] = true;
}

void NeoPixel::waitForNextFlipToComplete()
{
    uint32_t lastFrame = m_frameCount;

    // Would hang forever if DMA operations haven't been started yet so just return.
    if (!m_isStarted)
        return;

    while (m_frameCount == lastFrame)
    {
    }
}

void NeoPixel::emitByte(uint8_t byte)
{
    // Each NeoPixel bit will be represented in 12 SPI bits.
    // The format of the 12 SPI bits will be:
    //    1111xxxx0000
    //    xxxx will be 0000 if NeoPixel bit is 0.
    //    xxxx will be 1111 if NeoPixel bit is 1.
    // Two NeoPixel bits will therefore be stored in 3 SPI bytes.
    //    1111xxxx 00001111 xxxx0000

    // Bit 7
    *m_pEmitBuffer &= 0xF0;
    if (byte & 0x80)
        *m_pEmitBuffer |= 0x0E;
    m_pEmitBuffer += 2;
    // Bit 6
    *m_pEmitBuffer &= 0x0F;
    if (byte & 0x40)
        *m_pEmitBuffer |= 0xE0;
    m_pEmitBuffer++;
    // Bit 5
    *m_pEmitBuffer &= 0xF0;
    if (byte & 0x20)
        *m_pEmitBuffer |= 0x0E;
    m_pEmitBuffer += 2;
    // Bit 4
    *m_pEmitBuffer &= 0x0F;
    if (byte & 0x10)
        *m_pEmitBuffer |= 0xE0;
    m_pEmitBuffer++;
    // Bit 3
    *m_pEmitBuffer &= 0xF0;
    if (byte & 0x08)
        *m_pEmitBuffer |= 0x0E;
    m_pEmitBuffer += 2;
    // Bit 2
    *m_pEmitBuffer &= 0x0F;
    if (byte & 0x04)
        *m_pEmitBuffer |= 0xE0;
    m_pEmitBuffer++;
    // Bit 1
    *m_pEmitBuffer &= 0xF0;
    if (byte & 0x02)
        *m_pEmitBuffer |= 0x0E;
    m_pEmitBuffer += 2;
    // Bit 0
    *m_pEmitBuffer &= 0x0F;
    if (byte & 0x01)
        *m_pEmitBuffer |= 0xE0;
    m_pEmitBuffer++;
}

int NeoPixel::__dmaInterruptHandler(void* pContext, uint32_t dmaInterruptStatus)
{
    NeoPixel* pThis = (NeoPixel*)pContext;

    return pThis->dmaInterruptHandler(dmaInterruptStatus);
}

int NeoPixel::dmaInterruptHandler(uint32_t dmaInterruptStatus)
{
    if (dmaInterruptStatus & m_channelMask)
    {
        // If front buffer that was just sent is older than one to be sent next, copy in the newer contents so that
        // we don't flicker between older and newer value if client app doesn't set on every frame flip.
        uint32_t bufferJustSent = m_frameCount & 1;
        uint32_t bufferToSendNext = !bufferJustSent;
        if (m_isBufferContentsNew[bufferJustSent] == false && m_isBufferContentsNew[bufferToSendNext] == true)
        {
            memcpy(m_pFrontBuffers[bufferJustSent], m_pFrontBuffers[bufferToSendNext], m_packetSize);
            m_isBufferContentsNew[bufferToSendNext] = false;
        }

        // The SPI transmit channel triggered an interupt. This will release client coding waiting in set() method.
        interlockedIncrement(&m_frameCount);

        // Clear the terminal count interrupt for this channel.
        LPC_GPDMA->DMACIntTCClear = m_channelMask;

        // Interrupt was handled.
        return 1;
    }

    // Didn't handle this DMA interrupt.
    return 0;
}
