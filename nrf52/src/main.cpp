#include <iostream>
#include <memory>

#include <ble/BLE.h>
#include <ble/Gap.h>
#include <ble/GattCharacteristic.h>
#include <mbed.h>
#include <BME280.h>

#include <nrf_soc.h>
#include <events/EventQueue.h>

/**
* @class EnvironmentalService
* @brief BLE Environmental Service. This service provides temperature, humidity and pressure measurement.
* Service:  https://developer.bluetooth.org/gatt/services/Pages/ServiceViewer.aspx?u=org.bluetooth.service.environmental_sensing.xml
* Temperature: https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.temperature.xml
* Humidity: https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.humidity.xml
* Pressure: https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.pressure.xml
*/
class EnvironmentalService {
public:
    typedef int16_t TemperatureType_t;
    typedef uint16_t HumidityType_t;
    typedef uint32_t PressureType_t;
    typedef uint16_t CO2Type_t;

    /**
     * @brief   EnvironmentalService constructor.
     * @param   _ble Reference to BLE device.
     */
    EnvironmentalService(BLE &_ble) :
            ble(_ble),
            temperatureCharacteristic(GattCharacteristic::UUID_TEMPERATURE_CHAR, &temperature,
                                      GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY),
            humidityCharacteristic(GattCharacteristic::UUID_HUMIDITY_CHAR, &humidity,
                                   GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY),
            pressureCharacteristic(GattCharacteristic::UUID_PRESSURE_CHAR, &pressure,
                                   GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY),
            co2Characteristic(0x2A70 /* non-standard extension */, &co2,
                              GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY) {
        static bool serviceAdded = false; /* We should only ever need to add the information service once. */
        if (serviceAdded) {
            return;
        }

        GattCharacteristic *charTable[] = {&humidityCharacteristic,
                                           &pressureCharacteristic,
                                           &temperatureCharacteristic,
                                           &co2Characteristic};

        GattService environmentalService(GattService::UUID_ENVIRONMENTAL_SERVICE, charTable,
                                         sizeof(charTable) / sizeof(GattCharacteristic *));

        ble.gattServer().addService(environmentalService);
        serviceAdded = true;
    }

    /**
     * @brief   Update humidity characteristic.
     * @param   newHumidityVal New humidity measurement.
     */
    void updateHumidity(HumidityType_t newHumidityVal) {
        humidity = (HumidityType_t) (newHumidityVal * 100);
        ble.gattServer().write(humidityCharacteristic.getValueHandle(), (uint8_t *) &humidity, sizeof(HumidityType_t));
    }

    /**
     * @brief   Update pressure characteristic.
     * @param   newPressureVal New pressure measurement.
     */
    void updatePressure(PressureType_t newPressureVal) {
        pressure = (PressureType_t) (newPressureVal * 10);
        ble.gattServer().write(pressureCharacteristic.getValueHandle(), (uint8_t *) &pressure, sizeof(PressureType_t));
    }

    /**
     * @brief   Update temperature characteristic.
     * @param   newTemperatureVal New temperature measurement.
     */
    void updateTemperature(float newTemperatureVal) {
        temperature = (TemperatureType_t) (newTemperatureVal * 100);
        ble.gattServer().write(temperatureCharacteristic.getValueHandle(), (uint8_t *) &temperature,
                               sizeof(TemperatureType_t));
    }

    void updateCO2(CO2Type_t newCO2Val) {
        co2 = newCO2Val;
        ble.gattServer().write(co2Characteristic.getValueHandle(), (uint8_t *) &co2, sizeof(CO2Type_t));
    }

private:
    BLE &ble;

    TemperatureType_t temperature = 0;
    HumidityType_t humidity = 0;
    PressureType_t pressure = 0;
    CO2Type_t co2 = 0;

    ReadOnlyGattCharacteristic<TemperatureType_t> temperatureCharacteristic;
    ReadOnlyGattCharacteristic<HumidityType_t> humidityCharacteristic;
    ReadOnlyGattCharacteristic<PressureType_t> pressureCharacteristic;
    ReadOnlyGattCharacteristic<CO2Type_t> co2Characteristic;
};

/**
 * NB:
 * MHZ19B requires 5V Vin, it responds with 3.3V incorrect values.
 */
class MHZ19B {
    events::EventQueue &eventQueue;
    mbed::RawSerial mhz19bSerial;
    Callback<void(uint16_t)> co2handler;
    uint8_t receiveBuffer[9]{0};
    const time_t startTime{time(nullptr)};
    bool propagateData{false};
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
                    uint16_t co2ppm = (static_cast<uint16_t>(receiveBuffer[2]) << 8u) + receiveBuffer[3];
                    // At start sensor returns outputs 429, then 410 and only after near a two minutes
                    // sensor starts working correctly.
                    // But it is not known if sensor was powered before program start (reboot) or both
                    // CPU and sensor was powered off.
                    if (!propagateData && ((co2ppm != 429 && co2ppm != 410) || startTime + 120 <= time(nullptr))) {
                        propagateData = true;
                    }
                    if (propagateData) {
                        co2handler(co2ppm);
                    }
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

public:
    MHZ19B(events::EventQueue &eventQueue, PinName receivePin, PinName transmitPin, Callback<void(uint16_t)> co2handler)
            : eventQueue(eventQueue),
              mhz19bSerial(transmitPin, receivePin, 9600),
              co2handler{co2handler} {}

    void sendRequest() {
        if (mhz19bSerial.writeable()) {
            mhz19bSerial.abort_read();
            mhz19bSerial.abort_write();
            mhz19bSerial.write(
                    requestBuffer, sizeof(requestBuffer),
                    [this](int) {
                        eventQueue.call([this]() {
                            mhz19bSerial.read(
                                    receiveBuffer, sizeof(receiveBuffer),
                                    {this, &MHZ19B::onDataReceived},
                                    SERIAL_EVENT_RX_ALL);
                        });
                    },
                    SERIAL_EVENT_TX_ALL);

        } else {
            std::cerr << "Serial is not writeable" << std::endl;
        }
    }
};

class App {
    events::EventQueue eventQueue{50 * EVENTS_EVENT_SIZE};
    BLE &bluetooth = BLE::Instance();
    const char deviceName[11] = "shitmeter";
    const uint16_t bleUuidList[1]{GattService::UUID_ENVIRONMENTAL_SERVICE};
    std::unique_ptr<EnvironmentalService> environmentalService;
    MHZ19B mhz19b{eventQueue, P0_12, P0_11, {this, &App::onCO2Change}};
    BME280 bme280{P0_27, P0_26};
    float temperature = 0;
    float pressure = 0;
    float humidity = 0;
    uint16_t co2ppm;

    void scheduleBleEventProcessing(BLE::OnEventsToProcessCallbackContext *context) {
        eventQueue.call(&context->ble, &BLE::processEvents);
    }

    void bleInitComplete(BLE::InitializationCompleteCallbackContext *context);

    void bleOnDisconnect(const Gap::DisconnectionCallbackParams_t *params) {
        std::cerr << "Someone disconnected" << std::endl;
        bluetooth.gap().startAdvertising();
    }

    void bleOnConnect(const Gap::ConnectionCallbackParams_t *params) {
        std::cerr << "Someone connected" << std::endl;
    }

    void measureTemperature();

    void measurePressure();

    void measureHumidity();

    void measureCO2();

    void printInfo();

    void onCO2Change(uint16_t value);

public:
    int run();

    bool isGapConnected() const;
};

void App::bleInitComplete(BLE::InitializationCompleteCallbackContext *context) {
    BLE &ble = context->ble;

#define CHECK_ERROR(expr, msg) \
        if (ble_error_t bleError = (expr); bleError != BLE_ERROR_NONE) { \
            std::cerr << '[' << msg << "] " << ble.errorToString(bleError) << std::endl; \
            return; \
        }

    CHECK_ERROR(context->error, "BLE init");
    if (ble.getInstanceID() != BLE::DEFAULT_INSTANCE) {
        std::cerr << "BLE instance id " << ble.getInstanceID() << " is not default instance id "
                  << BLE::DEFAULT_INSTANCE << std::endl;
        return;
    }

    ble.onEventsToProcess({this, &App::scheduleBleEventProcessing});

    environmentalService = std::make_unique<EnvironmentalService>(ble);

    Gap &gap = ble.gap();
    gap.onConnection(this, &App::bleOnConnect);
    gap.onDisconnection(this, &App::bleOnDisconnect);

    CHECK_ERROR(
            gap.accumulateAdvertisingPayload(
                    GapAdvertisingData::LE_GENERAL_DISCOVERABLE
                    | GapAdvertisingData::BREDR_NOT_SUPPORTED),
            "BLE gap accumulateAdvertisingPayload(Flags_t discoverability)");

    CHECK_ERROR(
            gap.accumulateAdvertisingPayload(
                    GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS,
                    (uint8_t *) bleUuidList, sizeof(bleUuidList)),
            "BLE gap accumulateAdvertisingPayload(DataType_t serviceIds)");

    CHECK_ERROR(
            gap.accumulateAdvertisingPayload(GapAdvertisingData::GENERIC_THERMOMETER),
            "BLE gap accumulateAdvertisingPayload(Appearance_t)");

    CHECK_ERROR(
            gap.accumulateAdvertisingPayload(
                    GapAdvertisingData::COMPLETE_LOCAL_NAME,
                    (uint8_t *) deviceName, sizeof(deviceName)),
            "BLE gap accumulateAdvertisingPayload(DataType_t localName)");

    gap.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    gap.setAdvertisingInterval(1000);

    CHECK_ERROR(gap.startAdvertising(), "BLE error startAdvertising()");

#undef CHECK_ERROR
    std::cerr << "BLE initialized successfully. Device name: " << deviceName << std::endl;
}

void App::printInfo() {
    static int counter = 0;

    std::cerr
            << std::endl
            << "============ " << counter++ << std::endl
            << "Temperature: " << temperature << " C" << std::endl
            << "Pressure:    " << pressure << " hPa" << std::endl
            << "Humidity:    " << humidity << "%" << std::endl
            << "CO2:         " << co2ppm << " PPM" << std::endl;

    if (isGapConnected()) {
        std::cerr << "Gap is connected" << std::endl;
    } else {
        std::cerr << "Gap is not connected" << std::endl;
    }
}

bool App::isGapConnected() const {
    return bluetooth.gap().getState().connected;
}

void App::measureTemperature() {
    temperature = bme280.getTemperature();
    if (isGapConnected()) {
        environmentalService->updateTemperature(temperature);
    }
}

void App::measurePressure() {
    pressure = bme280.getPressure();
    if (isGapConnected()) {
        environmentalService->updatePressure((uint32_t) pressure);
    }
}

void App::measureHumidity() {
    humidity = bme280.getHumidity();
    if (isGapConnected()) {
        environmentalService->updateHumidity((uint16_t) humidity);
    }
}

void App::measureCO2() {
    mhz19b.sendRequest();
}

void App::onCO2Change(uint16_t value) {
    co2ppm = value;
    if (isGapConnected()) {
        environmentalService->updateCO2(co2ppm);
    }
}

int App::run() {
    eventQueue.call([&]() {
        ble_error_t error = bluetooth.init(this, &App::bleInitComplete);
        if (error != BLE_ERROR_NONE) {
            std::cerr << "bluetooth init error " << error << std::endl;
        }
        eventQueue.call(this, &App::printInfo);
        eventQueue.call_every(3000, this, &App::measureTemperature);
        eventQueue.call_every(3000, this, &App::measurePressure);
        eventQueue.call_every(3000, this, &App::measureHumidity);
        eventQueue.call_every(3000, this, &App::measureCO2);
        eventQueue.call_every(5000, this, &App::printInfo);
    });
    eventQueue.dispatch_forever();
    return 0;
}

int main() {
    return App().run();
}
