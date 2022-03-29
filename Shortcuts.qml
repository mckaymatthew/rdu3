import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt.example.qobjectSingleton 1.0
import FrontPanelButtons 1.0

Rectangle {
    id: page
    width: 320; height: 480
    color: "darkgray"
    Grid {
        columns: 3
        spacing: 2
        Text {
            id: screenOne
            text: MyApi.currentScreen
//            y: 30
//            anchors.horizontalCenter: page.horizontalCenter
            font.pointSize: 24; font.bold: true
        }
        Text {
            id: screenTwo
            text: MyApi.currentSecondScreen
//            y: 30
//            anchors.horizontalCenter: page.horizontalCenter
            font.pointSize: 24; font.bold: true
        }
        Button {
            text: "RF Power (100%)"
//            enabled: MyApi.currentScreen === "Multi RF Power"
            onClicked: setRfPower(100)
        }
        Button {
            text: "RF Power (50%)"
//            enabled: MyApi.currentScreen === "Multi RF Power"
            onClicked: setRfPower(50)
        }
    }
    function setRfPower2(power) {
        MyApi.press(FrontPanelButton.Exit)
        MyApi.press(FrontPanelButton.Multi)
        MyApi.touch(423,13)
        RDUController.spinMultiDial(-100) //Spin -100 to ensure we are at zero, no matter what the starting point
        RDUController.spinMultiDial(50) //Spin back up to setpoint
        MyApi.press(FrontPanelButton.Exit)

    }

    function setRfPower(power) {
        exitToHome(function(){
        clickMultiButton(function() {
        tapRfPowerOption(function() {
            console.log("Set RF power to "+power)
            RDUController.spinMultiDial(-100) //Spin -100 to ensure we are at zero, no matter what the starting point
            RDUController.spinMultiDial(50) //Spin back up to setpoint
            MyApi.press(FrontPanelButton.Exit)
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
