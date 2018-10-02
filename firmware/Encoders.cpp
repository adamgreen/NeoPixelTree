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
/* Class used to interface with Adafruit's Rotary Encoder (https://www.adafruit.com/product/377). */
#include "Encoders.h"


// The signal must be in the HIGH state for this many microseconds before it will be considered a HIGH pulse.
#define MINIMUM_HIGH_TIME 500

// The time in microseconds used to debounce the press of the encoder shaft.
#define DEBOUNCE_PRESS_TIME 1000

// Encoder transitions that have been detected between detent states.
// Have seen transition from detent state to next state for clockwise/counter clockwise rotation.
#define STATE_TRANSITION_CW_FIRST   (1 << 0)
#define STATE_TRANSITION_CCW_FIRST  (1 << 1)
// Have seen transition back to state just before final transition to detent for clockwise/counter clockwise rotation.
#define STATE_TRANSITION_CW_LAST    (1 << 2)
#define STATE_TRANSITION_CCW_LAST   (1 << 3)
// Have seen encoder state from middle of clockwise or counter clockwise rotation (both inputs LO).
#define STATE_TRANSITION_MIDDLE     (1 << 4)
// Have transitioned back to final detent state (both inputs HI).
#define STATE_TRANSITION_DETENT     (1 << 5)

// Bitmasks to check above state transition combinations for clockwise or counter clockwise rotation.
#define DETECTED_CW_TRANSITIONS     (STATE_TRANSITION_CW_FIRST | STATE_TRANSITION_CW_LAST | STATE_TRANSITION_MIDDLE)
#define DETECTED_CCW_TRANSITIONS    (STATE_TRANSITION_CCW_FIRST | STATE_TRANSITION_CCW_LAST | STATE_TRANSITION_MIDDLE)


static Timer g_timer;


Encoder::EncoderSignal::EncoderSignal(PinName pin) : m_pin(pin, PullUp)
{
    g_timer.start();
    uint32_t currTime = g_timer.read_us();

    m_lastHighPulse.startTime = currTime;
    m_lastHighPulse.endTime = currTime;
    m_lastHighPulse.value = 1;
    m_lastHighPulse.needsProcessing = false;

    m_currPulse.startTime = currTime;
    m_currPulse.endTime = currTime;
    m_currPulse.value = m_pin.read();
    m_currPulse.needsProcessing = true;
}

bool Encoder::EncoderSignal::sample(SignalStateQueue* pStateQueue)
{
    int      currValue = m_pin.read();
    uint32_t currTime = g_timer.read_us();
    uint32_t elapsedTime = currTime - m_currPulse.startTime;

    // Populate the state queue when we have detected the end of a clean HIGH pulse.
    // This will happen when we see the next trailing edge or the signal has been HIGH for a long time (as would
    // happen when just sitting at a detent during 0 rotation).
    if (m_currPulse.value && elapsedTime > MINIMUM_HIGH_TIME)
    {
        return populateStateQueue(currTime, currValue, pStateQueue);
    }
    else if (currValue != m_currPulse.value)
    {
        // Track the start of this new pulse state.
        m_currPulse.startTime = currTime;
        m_currPulse.endTime = currTime;
        m_currPulse.value = currValue;
        m_currPulse.needsProcessing = true;
    }

    // No new high pulse has been detected.
    return false;
}

bool Encoder::EncoderSignal::populateStateQueue(uint32_t currTime, int currValue, SignalStateQueue* pStateQueue)
{
    bool ret = false;

    // Can skip populating the queue if this is just an extension of the previous high pulse.
    m_currPulse.endTime = currTime;
    if (m_lastHighPulse.endTime != m_currPulse.startTime)
    {
        // Low pulse is inferred from timing of the preceding and succeeding high pulses.
        // The low pulse isn't measured directly since they tend to have excessive bounce but the high pulses don't.
        pStateQueue->states[0].startTime = m_lastHighPulse.endTime;
        pStateQueue->states[0].endTime = m_currPulse.startTime;
        pStateQueue->states[0].value = 0;
        pStateQueue->states[0].needsProcessing = true;

        pStateQueue->states[1] = m_currPulse;

        ret = true;
    }

    // Remember the current pulse as the last high pulse.
    m_lastHighPulse = m_currPulse;

    // Track the start of this new pulse state.
    m_currPulse.startTime = currTime;
    m_currPulse.endTime = currTime;
    m_currPulse.value = currValue;
    m_currPulse.needsProcessing = true;

    return ret;
}

Encoder::Encoder(PinName pinA, PinName pinB, PinName pinPress)
    : m_signalA(pinA), m_signalB(pinB), m_pin(pinPress, PullUp)
{
    g_timer.start();

    populateTransitionsToIncrementTable();

    m_lastEncoderValue = (m_signalB.read() << 1) | m_signalA.read();
    m_isPressed = !m_pin.read();
    m_pressStartTime = g_timer.read_us();

    memset(&m_queueA, 0, sizeof(m_queueA));
    memset(&m_queueB, 0, sizeof(m_queueB));
}

bool Encoder::sample(EncoderState* pState)
{
    // Table used to look up clockwise/counter-clockwise state transition based on current and last state.
    static const uint32_t stateTable[4][4] =
    {
        {0,                       STATE_TRANSITION_CW_LAST,   STATE_TRANSITION_CCW_LAST, STATE_TRANSITION_DETENT},
        {STATE_TRANSITION_MIDDLE, 0,                          0,                         STATE_TRANSITION_DETENT},
        {STATE_TRANSITION_MIDDLE, 0,                          0,                         STATE_TRANSITION_DETENT},
        {0,                       STATE_TRANSITION_CCW_FIRST, STATE_TRANSITION_CW_FIRST, STATE_TRANSITION_DETENT}
    };

    // Assume that there is no new encoder state this time. Will change these variables later if we determine that
    // the encoder state has changed since the last call to sample().
    pState->count = 0;
    bool ret = samplePress();
    pState->isPressed = m_isPressed;

    bool qUpdatedA = m_signalA.sample(&m_queueA);
    bool qUpdatedB = m_signalB.sample(&m_queueB);
    if (qUpdatedA || qUpdatedB)
    {
        // Atleast one of the encoder output signals (A or B) has just changed state so iterate through the
        // 2 most recently seen high/low pulses from each encoder signal pin.
        int      a = 0;
        int      b = 0;
        bool     reprocessing = true;
        uint32_t stateTransitionsSeen = 0;
        while (a < 2 || b < 2)
        {
            EncoderSignal::SignalState* pCurrState;
            int                         shift;

            // Iterate over both signal queues at once. On each iteration pick the outstanding pulse from either
            // queue with the oldest start time.
            if (b >= 2 || (a < 2 && m_queueA.states[a].startTime < m_queueB.states[b].startTime))
            {
                pCurrState = &m_queueA.states[a];
                shift = 0;
                a++;
            }
            else
            {
                pCurrState = &m_queueB.states[b];
                shift = 1;
                b++;
            }

            // Handle the fact that we may need to catch up on previously processed state transitions before
            // starting to process newer ones.
            if (reprocessing && !pCurrState->needsProcessing)
            {
                uint32_t currEncoderValue = pCurrState->encoderValue;
                uint32_t encoderTransition = stateTable[m_lastEncoderValue][currEncoderValue];
                if (encoderTransition != STATE_TRANSITION_DETENT)
                {
                    stateTransitionsSeen |= encoderTransition;
                }
                m_lastEncoderValue = currEncoderValue;
            }

            // Only need to process this pulse if we haven't already processed it on an earlier call to sample().
            if (!reprocessing || pCurrState->needsProcessing)
            {
                uint32_t currEncoderValue = (pCurrState->value << shift) | (m_lastEncoderValue & ~(1 << shift));

                // Look up state transition based on current and last state.
                uint32_t encoderTransition = stateTable[m_lastEncoderValue][currEncoderValue];

                if (encoderTransition == STATE_TRANSITION_DETENT)
                {
                    int32_t encoderDelta = m_transitionsToIncrementTable[stateTransitionsSeen];
                    if (encoderDelta != 0)
                    {
                        pState->count += encoderDelta;
                        stateTransitionsSeen = 0;
                        ret = true;
                    }
                }
                else
                {
                    stateTransitionsSeen |= encoderTransition;
                }
                m_lastEncoderValue = currEncoderValue;
                pCurrState->encoderValue = currEncoderValue;
                pCurrState->needsProcessing = false;
                reprocessing = false;
            }
        }
    }

    return ret;
}

bool Encoder::samplePress()
{
    uint32_t currTime = g_timer.read_us();
    if (m_isPressed)
    {
        uint32_t elapsedTime = currTime - m_pressStartTime;
        if (elapsedTime < DEBOUNCE_PRESS_TIME)
        {
            return false;
        }
    }

    bool isPressed = !m_pin.read();
    if (isPressed && !m_isPressed)
    {
        m_isPressed = true;
        m_pressStartTime = currTime;
        return true;
    }
    if (!isPressed && m_isPressed)
    {
        m_isPressed = false;
        return true;
    }
    return false;
}

void Encoder::populateTransitionsToIncrementTable()
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
