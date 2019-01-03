#include <mbed.h>
#include <BME280.h>

DigitalOut led(LED1);
BME280 sensor_bme(P0_27, P0_26);
EventQueue eventQueue(/* event count */ 50 * EVENTS_EVENT_SIZE);

void printTemperature(void)
{
    printf("%2.2f degC, %04.2f hPa, %2.2f %%\r\n", sensor_bme.getTemperature(), sensor_bme.getPressure(), sensor_bme.getHumidity());
    led = !led; // Toggle LED
}

int main()
{
    eventQueue.call_every(1000, printTemperature); // run every 1000 ms
    eventQueue.dispatch_forever();

    return 0;
}
