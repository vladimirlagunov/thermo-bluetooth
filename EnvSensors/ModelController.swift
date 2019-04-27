//
//  ModelController.swift
//  EnvSensors
//
//  Created by Vladimir Lagunov on 25/03/2019.
//  Copyright © 2019 Vladimir Lagunov. All rights reserved.
//

import Foundation

extension Notification.Name {
    static let onTemperatureChange = Notification.Name("onTemperatureChange")
    static let onPressureChange = Notification.Name("onPressureChange")
    static let onHumidityChange = Notification.Name("onHumidityChange")
    static let onCO2Change = Notification.Name("onCO2Change")
}
