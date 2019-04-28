//
//  ViewController.swift
//  EnvSensors
//
//  Created by Vladimir Lagunov on 24/03/2019.
//  Copyright © 2019 Vladimir Lagunov. All rights reserved.
//

import UIKit

class ViewController: UIViewController {
    private var appNotifications: NotificationCenter?

    override func viewDidLoad() {
        super.viewDidLoad()
        // Do any additional setup after loading the view, typically from a nib.
        if let n = (UIApplication.shared.delegate as? AppDelegate)?.appNotifications {
            n.addObserver(forName: Notification.Name.onTemperatureChange, object: nil, queue: nil,
                          using: { (v) in self.setTemperature(value: v.object) })
            n.addObserver(forName: Notification.Name.onPressureChange, object: nil, queue: nil,
                          using: { (v) in self.setPressure(value: v.object) })
            n.addObserver(forName: Notification.Name.onHumidityChange, object: nil, queue: nil,
                          using: { (v) in self.setHumidity(value: v.object) })
            n.addObserver(forName: Notification.Name.onCO2Change, object: nil, queue: nil,
                          using: { (v) in self.setCO2(value: v.object) })
        }
        temperatureLabel.text = absentValue
        pressureLabel.text = absentValue
        humidityLabel.text = absentValue
        co2Label.text = absentValue
    }

    @IBOutlet weak var temperatureLabel: UILabel!
    @IBOutlet weak var pressureLabel: UILabel!
    @IBOutlet weak var humidityLabel: UILabel!
    @IBOutlet weak var co2Label: UILabel!
    
    private let absentValue = "..."
    
    func setTemperature(value: Any?) {
        if let v = value as? Double {
            temperatureLabel.text = String(format: "%.1fºC", v)
        } else {
            temperatureLabel.text = absentValue
        }
    }
    
    func setPressure(value: Any?) {
        if let hPa = value as? Double {
            let mmHg = hPa * 0.75006375541921
            pressureLabel.text = "\(Int(mmHg)) mmHg"
        } else {
            pressureLabel.text = absentValue
        }
    }
    
    func setHumidity(value: Any?) {
        if let v = value as? Double {
            humidityLabel.text = String(format: "%.1f%%", v)
        } else {
            humidityLabel.text = absentValue
        }
    }
    
    func setCO2(value: Any?) {
        if let v = value as? Double {
            co2Label.text = "\(Int(v)) PPM"
        } else {
            co2Label.text = absentValue
        }
    }
}
