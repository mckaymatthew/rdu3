import QtQuick 2.15
import QtQuick.Controls 2.15
 import QtQuick.Layouts 1.15
import Qt.example.qobjectSingleton 1.0
import FrontPanelButtons 1.0

Rectangle {
    id: page
    width: 300; height: 200
    color: "#616161"
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
        MyApi.touch(423,13)
    }
}
