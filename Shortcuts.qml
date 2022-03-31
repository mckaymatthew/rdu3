import QtQuick 2.15
import QtQuick.Controls 2.15
 import QtQuick.Layouts 1.15
import Qt.example.qobjectSingleton 1.0
import FrontPanelButtons 1.0

Rectangle {
    id: page
    width: 300; height: 200
    color: "#616161"
    ColumnLayout {
        GroupBox {
            title: "RF Power"
            RowLayout {
                anchors.fill: parent
                spacing: 6
                Button {
                    text: "5W"
                    onClicked: setRfPower(5)
                }
                Button {
                    text: "50W"
                    onClicked: setRfPower(50)
                }
                Button {
                    text: "75W"
                    onClicked: setRfPower(75)
                }
                Button {
                    text: "100W"
                    onClicked: setRfPower(100)
                }
            }
        }
        GroupBox {
            title: "AM Radio"
            GridLayout {
                columns: 2
                anchors.fill: parent
                Button {
                    text: "WCCO/News"
                    onClicked: tuneTo("83000")
                }
                Button {
                    text: "KUOM/University"
                    onClicked: tuneTo("77000")
                }
                Button {
                    text: "KTNF/Lefty"
                    onClicked: tuneTo("95000")
                }
                Button {
                    text: "WCCO Badly"
                    onClicked: tuneTo("83001")
                }
            }
        }
    }

    function setRfPower(power) {
        exitToHome(function(){
        clickMultiButton(function() {
        tapRfPowerOption(function() {
            console.log("Set RF power to "+power)
            //Use a tool like GIMP to find top,left and width,height
            var currentSetting = MyApi.readText(382,35,76,22,true,false)
            var lastCharacter = currentSetting.slice(-1)
            if(lastCharacter === "%") {
                var setpoint = currentSetting.slice(0,-1);
                var ticks = power - setpoint;
                console.log("Current RF Power Setting "+ currentSetting + " (" + setpoint +"). Need to move " + ticks)
                //RDUController.spinMultiDial(ticks);
                //Observation: If you spin the multi-dial "too soon" after the menu appears, it does not register the click
                //100ms seems sufficent
                MyApi.openLoopDelay(100, function() { RDUController.spinMultiDial(ticks); })
            } else {
                console.log("Failed to OCR RF Power setting.");
            }
            //With exit as well, if you do this too quickly after spinning the dial the radio does not recognize.
            MyApi.openLoopDelay(300, function() { MyApi.press(FrontPanelButton.Exit); })
        })})})
    }
    function tuneTo(newSetting) {
        exitToHome(function(){
            var LsbIndicatorNoWaterfall = Qt.point(247,37)
            var LsbIndicatorWaterfall = Qt.point(260,37)
            var LsbSetterWaterfall = Qt.point(252,56)
            var LsbSetterNoWaterfall = LsbSetterWaterfall //Same point works
            var acceptableRegex = '[0-9]{1,2}\.[0-9]{3}\.[0-9]{2}'

            var currentSetting = MyApi.readText(162,37,173,35,true,true).replace(/[^\.0-9]/,'') //coords when waterfall up
            console.log(currentSetting);
            var foundTunerValue = currentSetting.search(acceptableRegex) !== -1
            var isWaterfall = true
            if(!foundTunerValue) {
                isWaterfall = false
                currentSetting = MyApi.readText(65,38,304,61,true,true).replace(/[^\.0-9]/,'') //coords when waterfall NOT up
                console.log(currentSetting);
                foundTunerValue = currentSetting.search(acceptableRegex) !== -1
            }

            if(foundTunerValue) {
                console.log("Found tuner setting " + currentSetting + " with waterfall " + (isWaterfall ? "showing" : "hidden"))
                currentSetting = currentSetting.split('.').join('') //Strip '.'
                var HzLsbRequired = newSetting.slice(-2) !== "00" //Determine what LSB we need
                var HzLsbSet = !Qt.colorEqual("white", MyApi.pixel(isWaterfall ? LsbIndicatorWaterfall : LsbIndicatorNoWaterfall))
                var tunerDelta = newSetting - currentSetting
                var tickDelta = tunerDelta/(HzLsbRequired?1:100)

                console.log("Current tuner setting: " + currentSetting + ", LSB: " + (HzLsbSet ? "Hz" : "Khz"))
                console.log("Desired tuner setting: " + newSetting + ", LSB: " + (HzLsbRequired ? "Hz" : "Khz"));
                console.log("Tuner Delta: " + tunerDelta + ", ticks: " + tickDelta)

                var delay = 0;
                //Change LSB if required
                if(HzLsbRequired != HzLsbSet) {
                    MyApi.openLoopDelay(delay, function() { MyApi.touch(isWaterfall ? LsbSetterWaterfall : LsbSetterNoWaterfall) })
                }
                delay = delay + 10 //330ms
                var tickSize = 10
                var intertick = 3 //33ms
                var operations = Math.floor(Math.abs(tickDelta) / tickSize)
                var perTick = Math.sign(tunerDelta) * tickSize
                var remainder =
                for(var i = 0; i < operations; i = i + 1) {
                    MyApi.openLoopDelay(delay, function() { RDUController.spinMainDial(perTick) })
                    delay = delay + intertick
                }
                MyApi.openLoopDelay(delay, function() { RDUController.spinMainDial(Math.sign(tunerDelta) * remainder )})

                delay = delay + 10 //330ms

                //Change LSB back
                if(HzLsbRequired != HzLsbSet) {
                    MyApi.openLoopDelay(delay, function() { RDUController.spinMainDial(isWaterfall ? LsbSetterWaterfall : LsbSetterNoWaterfall) })
                }


            } else {
                console.log("Failed to find sane-looking tuner settings");
            }
        });

    }

    /*
      Press the "EXIT" button, up to 3 times to get back to the home Screen

      THe situation where you need to press exit 3 times is:
        In the Menu
            In the Settings submenu
                The screen is asleep
      */
    function exitToHome(atHomeCallback, recurisonLimit = 3) {
        console.log("Exit to home, level " + recurisonLimit)
        if(MyApi.currentScreen === "Home") {
            console.log("Exit to home, at home screen")
            atHomeCallback()
        } else {
            var timeoutCall = function() {
                console.log("Exit to home, timeout callback")
                exitToHome(atHomeCallback, recurisonLimit-1)
            }
            if(recurisonLimit !== 0) {
                MyApi.onScreen("Home",atHomeCallback, 250, timeoutCall)
                MyApi.press(FrontPanelButton.Exit)
            } else {
                console.log("Failed to get to home screen")
            }
        }
    }
    function clickMultiButton(atMultiCallback) {
        console.log("Opening multi-menu")
        MyApi.onScreen("Multi",atMultiCallback)
        MyApi.press(FrontPanelButton.Multi)
    }
    function tapRfPowerOption(atRfPowerCallback) {
        console.log("Clicking RF Power option")
        MyApi.onScreen("Multi","RF Power",atRfPowerCallback)
        MyApi.touch(Qt.point(423,13))
    }
}
