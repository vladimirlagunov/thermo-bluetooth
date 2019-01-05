#include <iostream>
#include <memory>

#include <ble/BLE.h>
#include <ble/Gap.h>
#include <ble/services/EnvironmentalService.h>
#include <mbed.h>
#include <BME280.h>


#include <nrf_soc.h>


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

    if (bluetooth.gap().getState().connected) {
        environmentalService->updateTemperature(temperature);
        environmentalService->updatePressure((uint32_t) pressure);
        environmentalService->updateHumidity((uint16_t) humidity);
        std::cerr << "Gap is connected" << std::endl;
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
