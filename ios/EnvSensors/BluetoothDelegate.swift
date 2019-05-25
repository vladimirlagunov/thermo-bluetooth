//
//  BluetoothDelegate.swift
//  EnvSensors
//
//  Created by Vladimir Lagunov on 27/04/2019.
//  Copyright © 2019 Vladimir Lagunov. All rights reserved.
//

import CoreBluetooth

class BluetoothDelegate: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    private let environmentalUuid = CBUUID(string: "0x181A")
    private let humidityUuid = CBUUID(string: "0x2A6F")
    private let pressureUuid = CBUUID(string: "0x2A6D")
    private let temperatureUuid = CBUUID(string: "0x2A6E")
    private let co2Uuid = CBUUID(string: "0x2A70")  // non-standard extension

    private var notificationCenter: NotificationCenter
    private var interestingPeripheral: CBPeripheral?
    private var scanTimer: Timer?

    init(notificationCenter: NotificationCenter) {
        self.notificationCenter = notificationCenter
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if (central.state == CBManagerState.poweredOn) {
            central.scanForPeripherals(withServices: [environmentalUuid], options: nil)
            // TODO suspend timer when app goes background?
            scanTimer = Timer.scheduledTimer(withTimeInterval: 20.0, repeats: true, block: { timer in
                if self.interestingPeripheral?.state != CBPeripheralState.connecting
                           && self.interestingPeripheral?.state != CBPeripheralState.connected
                           && !central.isScanning {
                    self.interestingPeripheral = nil
                    central.scanForPeripherals(withServices: nil, options: nil)
                }
            })
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        if (advertisementData[CBAdvertisementDataLocalNameKey] as? NSString == "shitmeter") {
            central.stopScan()
            interestingPeripheral = peripheral
            central.connect(interestingPeripheral!, options: nil)
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        if (peripheral == interestingPeripheral) {
            peripheral.delegate = self
            peripheral.discoverServices(nil)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        for service in peripheral.services! {
            if service.uuid == environmentalUuid {
                peripheral.discoverCharacteristics(nil, for: service)
                break
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        for c in service.characteristics! {
            if c.uuid == temperatureUuid || c.uuid == pressureUuid || c.uuid == humidityUuid || c.uuid == co2Uuid {
                peripheral.setNotifyValue(true, for: c)
                peripheral.readValue(for: c)
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        let _ = setValue(characteristic, temperatureUuid, Notification.Name.onTemperatureChange, 100.0, Int16.max)
                || setValue(characteristic, pressureUuid, Notification.Name.onPressureChange, 10.0, UInt32.max)
                || setValue(characteristic, humidityUuid, Notification.Name.onHumidityChange, 100.0, UInt16.max)
                || setValue(characteristic, co2Uuid, Notification.Name.onCO2Change, 1.0, UInt16.max)
    }

    @inlinable func setValue<T: BinaryInteger>(
            _ characteristic: CBCharacteristic,
            _ uuid: CBUUID,
            _ name: Notification.Name,
            _ divisor: Double,
            _ ignore: T) -> Bool {
        if (characteristic.uuid == uuid) {
            // TODO what about endianness?
            let value = characteristic.value?.withUnsafeBytes { (p: UnsafeRawBufferPointer) in
                p.load(as: T.self)
            }
            if (value != nil && value != ignore) {
                notificationCenter.post(name: name, object: Double.init(value!) / divisor)
            }
            return true
        }
        return false
    }
}
