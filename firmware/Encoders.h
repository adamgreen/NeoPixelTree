/* Copyright (C) 2018  Adam Green (https://github.com/adamgreen)

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
/* Class used to interface with two of the Pololu Magnetic Encoder Kits. */
#ifndef _ENCODERS_H_
#define _ENCODERS_H_

#include <assert.h>
#include <mbed.h>
#include <pinmap.h>
#include "Interlock.h"


// Number of microseconds used for debouncing encoder signals.
#define ENCODERS_DEBOUNCE_TIME 0UL


class EncoderBase
{
protected:
    virtual void handleEncoderTick(uint32_t* pRise0, uint32_t* pFall0) = 0;

    static void interruptHandler()
    {
        uint32_t rise0 = LPC_GPIOINT->IO0IntStatR;
        uint32_t fall0 = LPC_GPIOINT->IO0IntStatF;
        uint32_t rise2 = LPC_GPIOINT->IO2IntStatR;
        uint32_t fall2 = LPC_GPIOINT->IO2IntStatF;

        // Get in and quickly let the encoder code take first crack at processing the interrupts.
        m_pEncoders->handleEncoderTick(&rise0, &fall0);

        // If there are still interrupts to handle then chain in the original GPIO interrupt handler.
        if (rise0 || fall0 || rise2 || fall2)
            m_originalHandler();
    }

    static EncoderBase* m_pEncoders;
    static void         (*m_originalHandler)(void);
};
EncoderBase* EncoderBase::m_pEncoders = NULL;
void (*EncoderBase::m_originalHandler)(void) = NULL;


struct EncoderCounts
{
    volatile int32_t encoder1Count;
    volatile int32_t encoder2Count;
    volatile int32_t encoder3Count;
};


template<PinName encoder1Pin1, PinName encoder1Pin2, 
         PinName encoder2Pin1, PinName encoder2Pin2,
         PinName encoder3Pin1, PinName encoder3Pin2>
class Encoders : protected EncoderBase
{
public:
    Encoders()
    {
        // NOTE: Hard coded to only work with GPIO port 0 on the LPC1768.  The 2 pins for each encoder must
        //       be contiguous as well.
        assert( (encoder1Pin1 & ~0x1F) == LPC_GPIO0_BASE &&
                (encoder1Pin2 & ~0x1F) == LPC_GPIO0_BASE &&
                (encoder2Pin1 & ~0x1F) == LPC_GPIO0_BASE &&
                (encoder2Pin2 & ~0x1F) == LPC_GPIO0_BASE &&
                (encoder3Pin1 & ~0x1F) == LPC_GPIO0_BASE &&
                (encoder3Pin2 & ~0x1F) == LPC_GPIO0_BASE );
        assert( (encoder1Pin1 & 0x1F) - (encoder1Pin2 & 0x1F) == -1 &&
                (encoder2Pin1 & 0x1F) - (encoder2Pin2 & 0x1F) == -1 &&
                (encoder3Pin1 & 0x1F) - (encoder3Pin2 & 0x1F) == -1);

        // Set the pin mux to GPIO for these encoder pins.
        pin_function(encoder1Pin1, 0);
        pin_function(encoder1Pin2, 0);
        pin_function(encoder2Pin1, 0);
        pin_function(encoder2Pin2, 0);
        pin_function(encoder3Pin1, 0);
        pin_function(encoder3Pin2, 0);

        // Enable pull-ups on encoder inputs.
        pin_mode(encoder1Pin1, PullUp);
        pin_mode(encoder1Pin2, PullUp);
        pin_mode(encoder2Pin1, PullUp);
        pin_mode(encoder2Pin2, PullUp);
        pin_mode(encoder3Pin1, PullUp);
        pin_mode(encoder3Pin2, PullUp);

        // Calculate the mask for these encoder pins on port 0.
        uint32_t encoder1Mask = (1 << (encoder1Pin1 & 0x1F)) | (1 << (encoder1Pin2 & 0x1F));
        uint32_t encoder2Mask = (1 << (encoder2Pin1 & 0x1F)) | (1 << (encoder2Pin2 & 0x1F));
        uint32_t encoder3Mask = (1 << (encoder3Pin1 & 0x1F)) | (1 << (encoder3Pin2 & 0x1F));
        uint32_t encoderMask = encoder1Mask | encoder2Mask | encoder3Mask;

        // Configure the encoder pins as inputs.
        LPC_GPIO0->FIODIR &= ~encoderMask;

        // Record current encoder pin state.
        uint32_t port0 = LPC_GPIO0->FIOPIN;
        m_lastEncoder1 = (port0 >> (encoder1Pin1 & 0x1F)) & 0x3;
        m_lastEncoder2 = (port0 >> (encoder2Pin1 & 0x1F)) & 0x3;
        m_lastEncoder3 = (port0 >> (encoder3Pin1 & 0x1F)) & 0x3;

        // Clear the encoder counters.
        m_encoderCounts.encoder1Count = 0;
        m_encoderCounts.encoder2Count = 0;
        m_encoderCounts.encoder3Count = 0;

        // Haven't seen any encoder state transitions yet.
        m_stateTransitionsSeenEncoder1 = 0;
        m_stateTransitionsSeenEncoder2 = 0;
        m_stateTransitionsSeenEncoder3 = 0;

        populateTransitionsToIncrementTable();

        // Disable GPIO ints.
        NVIC_DisableIRQ(EINT3_IRQn);

        // Configure all encoder pins to interrupt on rising and falling edges.
        LPC_GPIOINT->IO0IntEnR |= encoderMask;
        LPC_GPIOINT->IO0IntEnF |= encoderMask;

        // Record previous interrupt handler for GPIO ints.
        m_originalHandler = (void (*)())NVIC_GetVector(EINT3_IRQn);

        // Only support having one of these classes instantiated.
        assert( m_pEncoders == NULL );
        m_pEncoders = this;

        m_debounceTimer.start();

        // Configure our interrupt handler for GPIO ints.
        NVIC_SetVector(EINT3_IRQn, (uint32_t)interruptHandler);
        NVIC_EnableIRQ(EINT3_IRQn);
    }

    EncoderCounts getAndClearEncoderCounts()
    {
        EncoderCounts encoderCounts = m_encoderCounts;
        m_encoderCounts.encoder1Count = 0;
        m_encoderCounts.encoder2Count = 0;
        m_encoderCounts.encoder3Count = 0;
        return encoderCounts;
    }

protected:
    void populateTransitionsToIncrementTable()
    {
        // This populates the lookup table which maps the encoder state transitions seen to +1 or -1.
        for (uint32_t i = 0 ; i < sizeof(m_transitionsToIncrementTable) / sizeof(m_transitionsToIncrementTable[0]) ; i++)
        {
            if ((i & DETECTED_CCW_TRANSITIONS) == DETECTED_CCW_TRANSITIONS)
            {
                // Counter Clockwise rotation between 2 detents has been detected.
                m_transitionsToIncrementTable[i] = -1;
            }
            else if ((i & DETECTED_CW_TRANSITIONS) == DETECTED_CW_TRANSITIONS)
            {
                // Clockwise rotation between 2 detents has been detected.
                m_transitionsToIncrementTable[i] = 1;
            }
            else
            {
                // Transition history would indicate that bounce/noise got us back to detent state so no rotation.
                m_transitionsToIncrementTable[i] = 0;
            }
        }
    }
    
    virtual void handleEncoderTick(uint32_t* pRise0, uint32_t* pFall0)
    {
        uint32_t rise0 = *pRise0;
        uint32_t fall0 = *pFall0;

        // Calculate the mask for these encoder pins on port 0.
        static const uint32_t encoder1Mask = (1 << (encoder1Pin1 & 0x1F)) | (1 << (encoder1Pin2 & 0x1F));
        static const uint32_t encoder2Mask = (1 << (encoder2Pin1 & 0x1F)) | (1 << (encoder2Pin2 & 0x1F));
        static const uint32_t encoder3Mask = (1 << (encoder3Pin1 & 0x1F)) | (1 << (encoder3Pin2 & 0x1F));
        static const uint32_t encoderMask = encoder1Mask | encoder2Mask | encoder3Mask;

        // Check to see if any of the encoder signals have changed.
        if ((uint32_t)m_debounceTimer.read_us() > ENCODERS_DEBOUNCE_TIME &&
            ((rise0 & encoderMask) || (fall0 & encoderMask)))
        {
            // Fetch the current state of the encoder pins.
            uint32_t port0 = LPC_GPIO0->FIOPIN;
            uint32_t encoder1 = (port0 >> (encoder1Pin1 & 0x1F)) & 0x3;
            uint32_t encoder2 = (port0 >> (encoder2Pin1 & 0x1F)) & 0x3;
            uint32_t encoder3 = (port0 >> (encoder3Pin1 & 0x1F)) & 0x3;

            // Look up state transition based on current and last state.
            static const uint32_t stateTable[4][4] = 
            {
                {0,                       STATE_TRANSITION_CW_LAST,   STATE_TRANSITION_CCW_LAST, STATE_TRANSITION_DETENT},
                {STATE_TRANSITION_MIDDLE, 0,                          0,                         STATE_TRANSITION_DETENT},
                {STATE_TRANSITION_MIDDLE, 0,                          0,                         STATE_TRANSITION_DETENT},
                {0,                       STATE_TRANSITION_CCW_FIRST, STATE_TRANSITION_CW_FIRST, STATE_TRANSITION_DETENT} 
            };
            uint32_t encoder1Transition = stateTable[m_lastEncoder1][encoder1];
            uint32_t encoder2Transition = stateTable[m_lastEncoder2][encoder2];
            uint32_t encoder3Transition = stateTable[m_lastEncoder3][encoder3];

            if (encoder1Transition == STATE_TRANSITION_DETENT)
            {
                int32_t encoder1Delta = m_transitionsToIncrementTable[m_stateTransitionsSeenEncoder1];
                if (encoder1Delta != 0)
                {
                    interlockedAdd(&m_encoderCounts.encoder1Count, encoder1Delta);
                    m_stateTransitionsSeenEncoder1 = 0;
                }
            }
            else
            {
                m_stateTransitionsSeenEncoder1 |= encoder1Transition;
            }
            
            if (encoder2Transition == STATE_TRANSITION_DETENT)
            {
                int32_t encoder2Delta = m_transitionsToIncrementTable[m_stateTransitionsSeenEncoder2];
                if (encoder2Delta != 0)
                {
                    interlockedAdd(&m_encoderCounts.encoder2Count, encoder2Delta);
                    m_stateTransitionsSeenEncoder2 = 0;
                }
            }
            else
            {
                m_stateTransitionsSeenEncoder2 |= encoder2Transition;
            }

            if (encoder3Transition == STATE_TRANSITION_DETENT)
            {
                int32_t encoder3Delta = m_transitionsToIncrementTable[m_stateTransitionsSeenEncoder3];
                if (encoder3Delta != 0)
                {
                    interlockedAdd(&m_encoderCounts.encoder3Count, encoder3Delta);
                    m_stateTransitionsSeenEncoder3 = 0;
                }
            }
            else
            {
                m_stateTransitionsSeenEncoder3 |= encoder3Transition;
            }

            // Record current encoder state.
            m_lastEncoder1 = encoder1;
            m_lastEncoder2 = encoder2;
            m_lastEncoder3 = encoder3;

            // Reset debounce timer to ignore encoder bounces for a short period of time.
            m_debounceTimer.reset();
        }

        // Clear out these GPIO interrupts.
        LPC_GPIOINT->IO0IntClr = encoderMask;
        rise0 &= ~encoderMask;
        fall0 &= ~encoderMask;
        *pRise0 = rise0;
        *pFall0 = fall0;
    }

    enum ClassConstants
    {
        // Encoder transitions that have been detected between detent states.
        // Have seen transition from detent state to next state for clockwise/counter clockwise rotation.
        STATE_TRANSITION_CW_FIRST   = (1 << 0),
        STATE_TRANSITION_CCW_FIRST  = (1 << 1),
        // Have seen transition back to detent state for clockwise/counter clockwise rotation.
        STATE_TRANSITION_CW_LAST    = (1 << 2),
        STATE_TRANSITION_CCW_LAST   = (1 << 3),
        // Have seen encoder state from middle of clockwise or counter clockwise rotation (both inputs LO).
        STATE_TRANSITION_MIDDLE     = (1 << 4),
        // Have transitioned back to final detent state (both inputs HI).
        STATE_TRANSITION_DETENT     = (1 << 5),
        
        // Bitmasks to check above state transition combinations for clockwise or counter clockwise rotation.
        DETECTED_CW_TRANSITIONS  = (STATE_TRANSITION_CW_FIRST | STATE_TRANSITION_CW_LAST | STATE_TRANSITION_MIDDLE),
        DETECTED_CCW_TRANSITIONS = (STATE_TRANSITION_CCW_FIRST | STATE_TRANSITION_CCW_LAST | STATE_TRANSITION_MIDDLE)
    };
    
    uint32_t      m_lastEncoder1;
    uint32_t      m_lastEncoder2;
    uint32_t      m_lastEncoder3;
    uint32_t      m_stateTransitionsSeenEncoder1;
    uint32_t      m_stateTransitionsSeenEncoder2;
    uint32_t      m_stateTransitionsSeenEncoder3;
    int32_t       m_transitionsToIncrementTable[32];
    EncoderCounts m_encoderCounts;
    Timer         m_debounceTimer;
};

#endif // _ENCODERS_H_
