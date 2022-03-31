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
                    onClicked: directDial("83000")
                }
                Button {
                    text: "KUOM/University"
                    onClicked: directDial("77000")
                }
                Button {
                    text: "KTNF/Lefty"
                    onClicked: directDial("95000")
                }
                Button {
                    text: "WCCO Badly"
                    onClicked: directDial("83001")
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
                MyApi.openLoopDelay(5, function() { RDUController.spinMultiDial(ticks); })
            } else {
                console.log("Failed to OCR RF Power setting.");
            }
            //With exit as well, if you do this too quickly after spinning the dial the radio does not recognize.
            MyApi.openLoopDelay(15, function() { MyApi.press(FrontPanelButton.Exit); })
        })})})
    }
    function directDial(newSetting) {
        exitToHome(function(){
            var SmallVFOPt1 = Qt.point(200,67)
            var SmallVFOPt2 = Qt.point(273,67)
            var SmallVFOClick = Qt.point(185,56)
            var BigVFOClick = Qt.point(110,70)
            var FINPClick = Qt.point(377,94)

            var digits= [
                    Qt.point(200,250),
                    Qt.point(100,100),
                    Qt.point(200,100),
                    Qt.point(300,100),
                    Qt.point(100,150),
                    Qt.point(200,150),
                    Qt.point(300,150),
                    Qt.point(100,200),
                    Qt.point(200,200),
                    Qt.point(300,200),

                    ]
            var ent = Qt.point(375,200)

            var smallVFO1 = MyApi.pixel(SmallVFOPt1)
            var smallVFO2 = MyApi.pixel(SmallVFOPt2)

            var smallVFO = Qt.colorEqual(smallVFO1, smallVFO2) && Qt.colorEqual(smallVFO1, "white")
            console.log("VFO: " + (smallVFO ? "Small" : "Large"))
            var delayIdx = 5;
            var delayAmount = 5;

            MyApi.openLoopDelay(delayIdx, function() { MyApi.touch(smallVFO ? SmallVFOClick : BigVFOClick) })
            delayIdx = delayIdx + delayAmount;
            MyApi.openLoopDelay(delayIdx, function() { MyApi.touch(FINPClick) })
            delayIdx = delayIdx + delayAmount;
            var toTouch = newSetting.split('').map(x => function() { MyApi.touch(digits[x]) })
            for(var i = 0; i < toTouch.length; i++) {
                MyApi.openLoopDelay(delayIdx, toTouch[i])
                delayIdx = delayIdx + delayAmount;
            }
            MyApi.openLoopDelay(delayIdx++, function() { MyApi.touch(ent) })
            delayIdx = delayIdx + delayAmount;


        })

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
