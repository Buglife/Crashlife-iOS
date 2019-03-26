<p align="center">
	<img src="https://ds9bjnn93rsnp.cloudfront.net/assets/temp/crashlife_logo_github-45fd44376f131c331d787105fbe6814d5c3e9149372d0a26b891924cefa08032.png" width=300 />
</p>

![Platform](https://img.shields.io/cocoapods/p/Buglife.svg)
[![CocoaPods Compatible](https://img.shields.io/cocoapods/v/Crashlife.svg)](https://cocoapods.org/pods/Crashlife)
[![Carthage compatible](https://img.shields.io/badge/Carthage-compatible-4BC51D.svg?style=flat)](https://github.com/Carthage/Carthage)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Twitter](https://img.shields.io/badge/twitter-@BuglifeApp-blue.svg)](https://twitter.com/buglifeapp)

Crashlife is an awesome crash reporting SDK & web platform for iOS apps. Here's how it works:

1. Your app crashes and relaunches
2. Crashlife sends the crash report to the Crashlife web dashboard
3. There is no step 3.

You can also find Crashlife for Android [here](https://github.com/buglife/crashlife-android).


---

|   | Main Features |
|---|---------------|
| 📖 | Open source |
| 🏃🏽‍♀️ | Fast & lightweight |
| 📜 | Custom attributes  |
| ℹ️ | Captured footprints and logs, with info / warning / error levels |
| 👩🏽‍💻 | Written in Objective-C, with full Swift support |


## Installation

### CocoaPods

To integrate Buglife into your Xcode project using [CocoaPods](https://cocoapods.org), specify it in your `Podfile`:

```ruby
pod 'Crashlife'
```

Then, run the following command:

```bash
$ pod install
```

### Carthage

Place the following line in your Cartfile:

``` Swift
github "Buglife/Crashlife-iOS"
```

Now run `carthage update`. Then drag & drop the Crashlife.framework in the Carthage/build folder to your project. Refer to the [Carthage README](https://github.com/Carthage/Carthage#adding-frameworks-to-an-application) for detailed / updated instructions.

## Code

1. Import the Crashlife framework header into your app delegate.

    ```swift
    // Swift
    import Crashlife
    ```
    
    ```objective-c
    // Objective-C
    #import <Crashlife/Crashlife.h>
    ```

2. Add the following to your app delegate's `application:didFinishLaunchingWithOptions:` method.
	
	```swift
	// Swift
	Crashlife.shared.start(withAPIKey: "YOUR_API_KEY_HERE")
	```
	```objective-c
	// Objective-C
	[[Crashlife sharedCrashlife] startWithAPIKey:@"YOUR_API_KEY_HERE"];
	```
	Be sure to replace `YOUR_API_KEY_HERE` with your own API key.
	
3. Build & run your app, then crash it (hopefully deliberately)! Note that the crash reporter will not activate for most crashes if the app was started attached to a debugger.

4. On relaunch, your crash will be submitted; then go to the Crashlife web dashboard.

### Symbolication

#### With Bitcode

Check back here soon!

#### Without Bitcode

Add a build phase to your Xcode project to upload dSYMs with each build:

1. Select your project in the Xcode project navigator
2. Select the “Build Phases” tab
3. Click the “+” button in the top left corner, and select “New Run Script Phase”
4. In the newly created run script, change the “Shell” field to `/usr/bin/ruby`
5. Paste the following snippet into the build phase:

	```ruby
	fork do	
        Process.setsid
        STDIN.reopen("/dev/null")
        STDOUT.reopen("/dev/null", "a")
        STDERR.reopen("/dev/null", "a")
        
        require 'shellwords'
        
        API_KEY='YOUR_API_KEY_HERE'
        
        bundle_identifier = ENV['PRODUCT_BUNDLE_IDENTIFIER']
        infoplist_path = "#{ENV['BUILT_PRODUCTS_DIR']}/#{ENV['INFOPLIST_PATH']}"
        bundle_short_version = `/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" #{infoplist_path}`.strip
        bundle_version = `/usr/libexec/PlistBuddy -c "Print CFBundleVersion" #{infoplist_path}`.strip
        
        Dir["#{ENV["DWARF_DSYM_FOLDER_PATH"]}/*/Contents/Resources/DWARF/*"].each do |dsym|
            system("curl -F apiKey=#{API_KEY} -F bundle_identifier=#{Shellwords.escape(bundle_identifier)} -F bundle_version=#{Shellwords.escape(bundle_version)} -F bundle_short_version=#{Shellwords.escape(bundle_short_version)} -F dsym=@#{Shellwords.escape(dsym)} -F projectRoot=#{Shellwords.escape(ENV["PROJECT_DIR"])} https://www.buglife.com/upload_dsym")
        end
    end
    ```
		
## Usage

### Crash Reporting

Once initialized, Crashlife will watch for uncaught exceptions (C++, ObjC), as well as Mach and POSIX signals. Crash reports will be saved to disk and sent on next launch. If the crash report can't be submitted for any reason, it will persist on disk until it can be sent at launch.

Additionally, Crashlife supports logging caught exceptions and error, warning, and info messages to the web dashboard as individual events. 

### Caught exception reporting

Crashlife can log caught exceptions will a full stack trace. From your catch block, call

```swift
// Swift
Crashlife.shared.logException(anNSException)
```
```objective-c
// Objective-C
[[Crashlife sharedCrashlife] logException:anNSException];
```

You can also pass a new (not yet-thrown) exception, however for performance reasons, it will not contain a stack trace. Crashlife will attempt to send the exception event immediately, and will cache it in case it is unable to do so. 

### Error/Warning/Info events

Crashlife supports logging messages of the severity of your choice: Crash, Error, Warning, and Info.

```swift
// Log an NSError
Crashlife.shared.logErrorObject(anNSError)

// Log an error
Crashlife.shared.logError("An error occurred: ...")

// Log a warning
Crashlife.shared.logWarning("Warning: doing something dangerous...")

// Log an informational message
Crashlife.shared.logInfo("Note: something weird is going on...")
```
```objective-c
// Objective-C
[[Crashlife sharedCrashlife] logErrorObject:error];

[[Crashlife sharedCrashlife] logError:@"An error occurred..."];

[[Crashlife sharedCrashlife] logWarning:@"Warning: doing something dangerous..."];

[[Crashlife sharedCrashlife] logInfo:@"Note: something weird is going on..."];
```

### Footprints

In order to aid in reproducing crashes, you can include footprints indicating what code paths were followed in order to reach the crash or error. These footprints can include their own attributes to avoid cluttering up the custom attributes. These footprints will not be sent to the Crashlife web dashboard unless a report is made. 

```swift
// Leave a footprint with no metadata
Crashlife.shared.leaveFootprint("User navigated to screen 2")

// Leave a footprint with metadata
var attributes = [:]
attributes["Developer"] = "You"
attributes["App"] = "Awesome"
Crashlife.shared.leaveFootprint("User did something else", withMetadata:attributes)
```
```objective-C
// Objective-C
[[Crashlife sharedCrashlife] leaveFootprint:@"User navigated to screen 2"];

NSDictionary<NSString *, NSString *> *attributes = @{@"Developer" : @"You", @"App" : @"Awesome"};
[[Crashlife sharedCrashlife] leaveFootprint:@"User did something else" withMetadata:attributes];
```

### Custom Attributes

#### Adding custom attributes

You can include custom attributes (i.e. key-value pairs) to your crash reports and logged events, as such:

```swift
// Swift
Crashlife.shared.setStringValue("2Pac", forAttribute:"Artist")
Crashlife.shared.setStringValue("California Love", forAttribute:"Song")
```
```objective-c
// Objective-C
[[Crashlife sharedCrashlife] setStringValue:@"2Pac" forAttribute:@"Artist"];
[[Crashlife sharedCrashlife] setStringValue:@"California Love" forAttribute:@"Song"];


#### Removing attributes

To clear an attribute, set its value to nil.

```swift
Crashlife.shared.setStringValue(nil, forAttribute:"Artist")
```
```objective-c
[[Crashlife sharedCrashlife] setStringValue:nil forAttribute:@"Artist"];
```


### User Identification

You may set a string representing the user’s name, database ID or other identifier:

```swift
let username = ... // the current username
Crashlife.shared.setUserIdentifier(username)
```
```objective-c
NSString *username = ...;
[[Crashlife sharedCrashlife] setUserIdentifier:username];
```

## Requirements

* Xcode 8 or later
* iOS 9 or later

## Contributing

We don't have any contributing guidelines at the moment, but feel free to submit pull requests & file issues within GitHub!

## License

```
Copyright (C) 2019 Buglife, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0
    
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```
