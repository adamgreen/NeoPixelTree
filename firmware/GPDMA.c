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
#include <stdio.h>
#include <cmsis.h>
#include "GPDMA.h"

static uint32_t g_dmaChannelsInUse;

int allocateDmaChannel(DmaDesiredChannel desiredChannel)
{
    switch (desiredChannel)
    {
    case GPDMA_CHANNEL_HIGH:
        for (int i = GPDMA_CHANNEL_HIGHEST ; i <= GPDMA_CHANNEL_LOWEST ; i++)
        {
            uint32_t mask = (1 << i);
            if ((mask & g_dmaChannelsInUse) == 0)
            {
                g_dmaChannelsInUse |= mask;
                return i;
            }
        }
        return -1;
    case GPDMA_CHANNEL_LOW:
        // Reserve GPDMA_CHANNEL_LOWEST for memory to memory operations.
        for (int i = GPDMA_CHANNEL_LOWEST - 1; i >= GPDMA_CHANNEL_HIGHEST ; i--)
        {
            uint32_t mask = (1 << i);
            if ((mask & g_dmaChannelsInUse) == 0)
            {
                g_dmaChannelsInUse |= mask;
                return i;
            }
        }
        return -1;
    default:
        if (((1 << desiredChannel) & g_dmaChannelsInUse) == 0)
        {
            return desiredChannel;
        }
        return -1;
    }
    return -1;
}

void freeDmaChannel(int channel)
{
    if (channel >= GPDMA_CHANNEL_HIGHEST && channel <= GPDMA_CHANNEL_LOWEST)
    {
        g_dmaChannelsInUse &= ~(1 << channel);
    }
}

LPC_GPDMACH_TypeDef* dmaChannelFromIndex(int index)
{
    switch (index)
    {
    case GPDMA_CHANNEL0:
        return LPC_GPDMACH0;
    case GPDMA_CHANNEL1:
        return LPC_GPDMACH1;
    case GPDMA_CHANNEL2:
        return LPC_GPDMACH2;
    case GPDMA_CHANNEL3:
        return LPC_GPDMACH3;
    case GPDMA_CHANNEL4:
        return LPC_GPDMACH4;
    case GPDMA_CHANNEL5:
        return LPC_GPDMACH5;
    case GPDMA_CHANNEL6:
        return LPC_GPDMACH6;
    case GPDMA_CHANNEL7:
        return LPC_GPDMACH7;
    default:
        return NULL;
    }
}



static DmaInterruptHandler* g_pDmaHandlers = NULL;

void DMA_IRQHandler(void)
{
    uint32_t dmaInterruptStatus = LPC_GPDMA->DMACIntStat;
    DmaInterruptHandler* pCurr = g_pDmaHandlers;
    DmaInterruptHandler* pNext = NULL;

    while (pCurr)
    {
        pNext = pCurr->pNext;
        if (pCurr->handler(pCurr->pContext, dmaInterruptStatus))
        {
            // This handler handled the interrupt so we can return now.
            return;
        }
        pCurr = pNext;
    }
}

int addDmaInterruptHandler(DmaInterruptHandler* pHandler)
{
    int wasListEmpty = (g_pDmaHandlers == NULL);

    pHandler->pNext = g_pDmaHandlers;
    g_pDmaHandlers = pHandler;

    if (wasListEmpty)
    {
        // Enable GPDMA interrupt.
        NVIC_EnableIRQ(DMA_IRQn);
    }

    return wasListEmpty;
}

int removeDmaInterruptHandler(DmaInterruptHandler* pHandler)
{
    DmaInterruptHandler* pPrev = NULL;
    DmaInterruptHandler* pCurr = g_pDmaHandlers;

    while (pCurr && pCurr != pHandler)
    {
        pPrev = pCurr;
        pCurr = pCurr->pNext;
    }
    if (!pCurr)
    {
        // Handler was not found in list to remove.
        return 0;
    }
    if (!pPrev)
    {
        // List will now be empty.
        NVIC_DisableIRQ(DMA_IRQn);
        g_pDmaHandlers = NULL;
        return 1;
    }
    pPrev->pNext = pHandler->pNext;
    pHandler->pNext = NULL;
    return 0;
}



__attribute__((section("AHBSRAM0"),aligned)) static uint8_t g_dmaHeap0[16 * 1024];
static uint8_t*                                             g_pDmaHeap0 = g_dmaHeap0;
__attribute__((section("AHBSRAM1"),aligned)) static uint8_t g_dmaHeap1[16 * 1024];
static uint8_t*                                             g_pDmaHeap1 = g_dmaHeap1;

void* dmaHeap0Alloc(uint32_t size)
{
    uint8_t* p = g_pDmaHeap0;
    g_pDmaHeap0 += size;
    return p;
}

void* dmaHeap1Alloc(uint32_t size)
{
    uint8_t* p = g_pDmaHeap1;
    g_pDmaHeap1 += size;
    return p;
}
