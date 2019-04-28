#include <iostream>
#include <memory>

#include <ble/BLE.h>
#include <ble/Gap.h>
//#include <ble/services/EnvironmentalService.h>
#include <ble/GattCharacteristic.h>
#include <mbed.h>
#include <BME280.h>

#include <nrf_soc.h>

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
                                   GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY) {
        static bool serviceAdded = false; /* We should only ever need to add the information service once. */
        if (serviceAdded) {
            return;
        }

        GattCharacteristic *charTable[] = {&humidityCharacteristic,
                                           &pressureCharacteristic,
                                           &temperatureCharacteristic};

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

private:
    BLE &ble;

    TemperatureType_t temperature;
    HumidityType_t humidity;
    PressureType_t pressure;

    ReadOnlyGattCharacteristic<TemperatureType_t> temperatureCharacteristic;
    ReadOnlyGattCharacteristic<HumidityType_t> humidityCharacteristic;
    ReadOnlyGattCharacteristic<PressureType_t> pressureCharacteristic;
};

class App {
    EventQueue eventQueue{50 * EVENTS_EVENT_SIZE};
    BLE &bluetooth = BLE::Instance();
    const char deviceName[11] = "shitmeter";
    const uint16_t bleUuidList[1]{GattService::UUID_ENVIRONMENTAL_SERVICE};
    std::unique_ptr<EnvironmentalService> environmentalService;
    BME280 bme280{P0_27, P0_26};

    void scheduleBleEventProcessing(BLE::OnEventsToProcessCallbackContext *context) {
        eventQueue.call(&context->ble, &BLE::processEvents);
    }

    void bleInitComplete(BLE::InitializationCompleteCallbackContext *context);

    void measureBme280();

    void bleOnDisconnect(const Gap::DisconnectionCallbackParams_t *params) {
        std::cerr << "Someone disconnected" << std::endl;
        bluetooth.gap().startAdvertising();
    }

    void bleOnConnect(const Gap::ConnectionCallbackParams_t *params) {
        std::cerr << "Someone connected" << std::endl;
    }

    void onHVX(const GattHVXCallbackParams *params) {
        std::cerr << "onHVX" << std::endl;
    }

public:
    int run();
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
    ble.gattClient().onHVX({this, &App::onHVX});

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

void App::measureBme280() {
    static int counter = 0;
    float temperature = bme280.getTemperature();
    float pressure = bme280.getPressure();
    float humidity = bme280.getHumidity();

    std::cerr
            << std::endl
            << "============ " << counter++ << std::endl
            << "Temperature: " << temperature << " C" << std::endl
            << "Pressure:    " << pressure << " hPa" << std::endl
            << "Humidity:    " << humidity << "%" << std::endl;

    Gap &gap = bluetooth.gap();
    if (gap.getState().connected) {
        std::cerr << "Gap is connected" << std::endl;
        environmentalService->updateTemperature(temperature);
        environmentalService->updatePressure((uint32_t) pressure);
        environmentalService->updateHumidity((uint16_t) humidity);
    } else {
        std::cerr << "Gap is not connected" << std::endl;
    }
}

int App::run() {
    eventQueue.call([&]() {
        ble_error_t error = bluetooth.init(this, &App::bleInitComplete);
        if (error != BLE_ERROR_NONE) {
            std::cerr << "bluetooth init error " << error << std::endl;
        }
        eventQueue.call(this, &App::measureBme280);
        eventQueue.call_every(5000, this, &App::measureBme280);
    });
    eventQueue.dispatch_forever();
    return 0;
}

int main() {
    return App().run();
}
