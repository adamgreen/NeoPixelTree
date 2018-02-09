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
#ifndef _ENCODERS_H_
#define _ENCODERS_H_

#include <mbed.h>


struct EncoderState
{
    int32_t count;
    bool    isPressed;
};


class Encoder
{
public:
    Encoder(PinName pinA, PinName pinB, PinName pinPress);

    bool sample(EncoderState* pState);

protected:
    bool samplePress();
    void populateTransitionsToIncrementTable();
    
    class EncoderSignal
    {
    public:
        struct SignalState
        {
            uint32_t    startTime;
            uint32_t    endTime;
            uint32_t    encoderValue;
            int         value;
            bool        needsProcessing;
        };
        
        struct SignalStateQueue
        {
            SignalState states[2];
        };
    
    
        EncoderSignal(PinName pin);

        int read()
        {
            return m_pin.read();
        }
        
        bool sample(SignalStateQueue* pStateQueue);
    
    protected:
        bool populateStateQueue(uint32_t currTime, int currValue, SignalStateQueue* pStateQueue);
        
        SignalState m_lastHighPulse;
        SignalState m_currPulse;
        DigitalIn   m_pin;
    };


    int32_t                         m_transitionsToIncrementTable[32];
    uint32_t                        m_lastEncoderValue;
    uint32_t                        m_pressStartTime;
    EncoderSignal::SignalStateQueue m_queueA;
    EncoderSignal::SignalStateQueue m_queueB;
    EncoderSignal                   m_signalA;
    EncoderSignal                   m_signalB;
    bool                            m_isPressed;
    DigitalIn                       m_pin;
};

#endif // _ENCODERS_H_
