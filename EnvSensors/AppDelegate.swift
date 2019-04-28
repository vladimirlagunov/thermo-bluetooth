//
//  AppDelegate.swift
//  EnvSensors
//
//  Created by Vladimir Lagunov on 24/03/2019.
//  Copyright © 2019 Vladimir Lagunov. All rights reserved.
//

import CoreBluetooth
import UIKit

@UIApplicationMain
class AppDelegate: UIResponder, UIApplicationDelegate {

    var window: UIWindow?
    
    private var randomValueTimer: Timer?
    private var myCentralManager: CBCentralManager?
    private var bluetoothDelegate: BluetoothDelegate?
    var appNotifications: NotificationCenter?

    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        // Override point for customization after application launch.
        appNotifications = NotificationCenter.init()
        bluetoothDelegate = BluetoothDelegate.init(notificationCenter: appNotifications!)
        myCentralManager = CBCentralManager.init(delegate: bluetoothDelegate!, queue: nil)
        return true
    }

    func applicationWillResignActive(_ application: UIApplication) {
        // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
        // Use this method to pause ongoing tasks, disable timers, and invalidate graphics rendering callbacks. Games should use this method to pause the game.
    }

    func applicationDidEnterBackground(_ application: UIApplication) {
        // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
        // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
    }

    func applicationWillEnterForeground(_ application: UIApplication) {
        // Called as part of the transition from the background to the active state; here you can undo many of the changes made on entering the background.
        randomValueTimer?.invalidate()
        randomValueTimer = nil
    }

    func applicationDidBecomeActive(_ application: UIApplication) {
        // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
//        randomValueTimer = Timer.scheduledTimer(withTimeInterval: 5.0,
//                                                repeats: true,
//                                                block: { timer in self.makeRandomValues() })
    }

    func applicationWillTerminate(_ application: UIApplication) {
        // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
    }


    private func makeRandomValues() {
        if let n = appNotifications {
            n.post(name: Notification.Name.onTemperatureChange, object: Double.random(in: -99.9...99.9))
            n.post(name: Notification.Name.onPressureChange, object: Double.random(in: 700.0...800.0))
            n.post(name: Notification.Name.onHumidityChange, object: Double.random(in: 0.0...100.0))
            n.post(name: Notification.Name.onCO2Change, object: Double.random(in: 200.0...5000.0))
        }
    }
}
