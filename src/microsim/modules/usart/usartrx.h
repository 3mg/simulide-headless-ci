/***************************************************************************
 *   Copyright (C) 2021 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#pragma once

#include <queue>

#include "usartmodule.h"

class UartRx : public UartTR
{
    public:
        UartRx( UsartModule* usart, eMcu* mcu, QString name );
        ~UartRx();

        virtual void enable( uint8_t en ) override;
        virtual void voltChanged() override;
        virtual void runEvent() override;
        virtual uint8_t getData() override;

        void ignoreData( bool i ) { m_ignoreData = i; }
        void setFifoSize( uint8_t s ) { m_fifoSize = s; }

        // Deliver a byte directly into the receive FIFO, bypassing pin-level
        // bit sampling. Used by the headless socket API (send_serial) to
        // simulate incoming serial data without driving RX pin voltages.
        void injectByte( uint8_t data );

    protected:
        void setRxFlags();
        void readBit();
        void rxEnd();
        void byteReceived( uint16_t frame );

        bool m_startHigh;
        bool m_ignoreData;

        uint16_t m_fifo[2];
        int  m_fifoP;
        int  m_fifoSize;
};
