//
//  BluetoothDelegate.swift
//  EnvSensors
//
//  Created by Vladimir Lagunov on 27/04/2019.
//  Copyright © 2019 Vladimir Lagunov. All rights reserved.
//

import CoreBluetooth

class BluetoothDelegate : NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
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
            // TODO limit scan by services
            central.scanForPeripherals(withServices: nil, options: nil)
            // TODO suspend timer when app goes background?
            scanTimer = Timer.scheduledTimer(withTimeInterval: 20.0, repeats: true, block: { timer in
                if self.interestingPeripheral?.state != CBPeripheralState.connecting
                    && self.interestingPeripheral?.state != CBPeripheralState.connected
                    && !central.isScanning
                {
                    self.interestingPeripheral = nil
                    central.scanForPeripherals(withServices: nil, options: nil)
                }
            })
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        //print(peripheral.name, advertisementData[CBAdvertisementDataLocalNameKey] as? NSString)
        if (advertisementData[CBAdvertisementDataLocalNameKey] as? NSString == "shitmeter") {
            central.stopScan()
            interestingPeripheral = peripheral
            central.connect(interestingPeripheral!, options: nil)
        }
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        if (peripheral == interestingPeripheral) {
            peripheral.delegate = self
            //print("Connected")
            peripheral.discoverServices(nil)
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        for service in peripheral.services! {
            //print("Service", service)
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
            //print("Service", service, "Characteristics", c)
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        if (error != nil) {
            print(error)
        }
    }
    
//    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
//        print(characteristic, error)
//    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        //print(characteristic)
        // TODO fix endianness
        if (characteristic.uuid == temperatureUuid) {
            let value = characteristic.value?.withUnsafeBytes { (v: UnsafePointer<Int16>) in v.pointee }
            if (value != nil) {
                notificationCenter.post(name: Notification.Name.onTemperatureChange, object: Double.init(value!) / 100.0)
            }
        } else if (characteristic.uuid == pressureUuid) {
            let value = characteristic.value?.withUnsafeBytes { (v: UnsafePointer<UInt32>) in v.pointee }
            if (value != nil) {
                notificationCenter.post(name: Notification.Name.onPressureChange, object: Double.init(value!) / 10.0)
            }
        } else if (characteristic.uuid == humidityUuid) {
            let value = characteristic.value?.withUnsafeBytes { (v: UnsafePointer<UInt16>) in v.pointee }
            if (value != nil) {
                notificationCenter.post(name: Notification.Name.onHumidityChange, object: Double.init(value!) / 100.0)
            }
        } else if (characteristic.uuid == co2Uuid) {
            let value = characteristic.value?.withUnsafeBytes { (v: UnsafePointer<UInt16>) in v.pointee }
            if (value != nil) {
                notificationCenter.post(name: Notification.Name.onCO2Change, object: Double.init(value!))
            }
        }
    }
}
