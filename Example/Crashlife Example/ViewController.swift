//
//  ViewController.swift
//  Crashlife Example
//
//    Copyright 2019 Buglife.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

import UIKit

class ViewController: UIViewController {

    @IBOutlet weak var testButton: UIButton?
    
    @IBAction func someAction() {
//        life_log_debug("I tapped a button!")
        
        //crashy crashy!
        let crasher = UnsafeMutableRawPointer(bitPattern: 1)
        crasher?.deallocate()
    }
    override func viewDidLoad() {
        super.viewDidLoad()
        self.title = "Crashlife Example"
        // Do any additional setup after loading the view, typically from a nib.
    }
    

    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        // Dispose of any resources that can be recreated.
    }


}

