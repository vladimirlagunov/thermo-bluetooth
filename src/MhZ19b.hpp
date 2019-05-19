#pragma once

#include <memory>
#include <iostream>
#include <RawSerial.h>
#include <events/EventQueue.h>


namespace mhz19b {
    /**
     * NB:
     * MHZ19B requires 5V Vin, it responds with 3.3V incorrect values.
     */
    class Supervisor {
        events::EventQueue &eventQueue;
        mbed::RawSerial mhz19bSerial;
        uint8_t receiveBuffer[9];
        uint16_t co2ppm;
        static constexpr uint8_t requestBuffer[9] = {
                0xFF,  // 0 constant
                0x01,  // 1 sensor number, probably constant
                0x86,  // 2 read command
                0x00,  // 3
                0x00,  // 4
                0x00,  // 5
                0x00,  // 6
                0x00,  // 7
                0x79,  // 8 checksum
        };

        template<class T>
        static uint8_t checksum(const T buffer, unsigned int offset) {
            uint8_t result = 0;
            for (unsigned int i = offset; i < offset + 7; ++i) {
                result += buffer[i];
            }
            return 0xFF - result + 1;
        }

        void onDataReceived(int events) {
            if (!(events & SERIAL_EVENT_RX_COMPLETE)) {
                eventQueue.call([events]() {
                    std::cerr << "Got events 0x" << std::hex << events << std::dec << std::endl;
                });
                return;
            }
            eventQueue.call([this]() {
                if (receiveBuffer[0] == 0xFF && receiveBuffer[1] == 0x86) {
                    if (checksum(receiveBuffer, 1) != receiveBuffer[8]) {
                        std::cerr
                                << "Checksum does not match. Expected "
                                << checksum(receiveBuffer, 1)
                                << ", got"
                                << receiveBuffer[8]
                                << std::endl;
                    } else {
                        co2ppm = (static_cast<uint16_t>(receiveBuffer[2]) << 8u) + receiveBuffer[3];
                    }
                } else {
                    std::cerr << "Can't fetch co2 ppm. Buffer is" << std::hex;
                    for (uint8_t i : receiveBuffer) {
                        std::cerr << ' ' << (int) i;
                    }
                    std::cerr << std::dec << std::endl;
                }
            });
        }

        void sendRequest() {
            if (mhz19bSerial.writeable()) {
                mhz19bSerial.abort_read();
                mhz19bSerial.abort_write();
                mhz19bSerial.write(
                        requestBuffer, sizeof(requestBuffer),
                        [this](int) {
                            eventQueue.call([this]() {
                                mhz19bSerial.read(
                                        receiveBuffer, 9,
                                        {this, &Supervisor::onDataReceived},
                                        SERIAL_EVENT_RX_ALL);
                            });
                        },
                        SERIAL_EVENT_TX_ALL);

            } else {
                std::cerr << "Serial is not writeable" << std::endl;
            }
        }

    public:
        Supervisor(events::EventQueue &eventQueue, PinName receivePin, PinName transmitPin)
                : eventQueue(eventQueue),
                  mhz19bSerial(transmitPin, receivePin, 9600),
                  receiveBuffer{0},
                  co2ppm{0} {
            eventQueue.call(this, &Supervisor::sendRequest);
            eventQueue.call_every(5000, this, &Supervisor::sendRequest);
        }

        uint16_t getCo2Ppm() {
            return co2ppm;
        }
    };
}
