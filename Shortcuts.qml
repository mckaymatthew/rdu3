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
                    text: "WCCO\nNews"
                    onClicked: directDialAM("83000")
                }
                Button {
                    text: "KUOM\nCollege"
                    onClicked: directDialAM("77000")
                }
                Button {
                    text: "KTNF\nLefty"
                    onClicked: directDialAM("95000")
                }
                Button {
                    text: "KTIS\nJesus 1"
                    onClicked: directDialAM("90000")
                }
                Button {
                    text: "KMNV\nSpanish"
                    onClicked: directDialAM("140000")
                }
                Button {
                    text: "KQSP\nTropical"
                    onClicked: directDialAM("153000")
                }
            }
        }
    }
    //Set the RF power in the Multi Menu to the setpoint
    function setRfPower(power) {
        var rfPowerOptionLocation = Qt.point(423,13)
        exitToHome(function(){
        console.log("Opening multi-menu")
            RDU.press(FrontPanelButton.Multi)
            RDU.schedule(5,function() {
                RDU.touch(rfPowerOptionLocation)
                RDU.schedule(5,function() {
                    console.log("Set RF power to "+power)
                    //Use a tool like GIMP to find top,left and width,height
                    var currentSetting = RDU.readText(382,35,76,22,true,false)
                    var lastCharacter = currentSetting.slice(-1)
                    if(lastCharacter === "%") {
                        var setpoint = currentSetting.slice(0,-1);
                        var ticks = power - setpoint;
                        console.log("Current RF Power Setting "+ currentSetting + " (" + setpoint +"). Need to move " + ticks)
                        //RDUController.spinMultiDial(ticks);
                        //Observation: If you spin the multi-dial "too soon" after the menu appears, it does not register the click
                        //100ms seems sufficent
                        RDU.schedule(5, function() { RDUController.spinMultiDial(ticks); })
                    } else {
                        console.log("Failed to OCR RF Power setting.");
                    }
                    //With exit as well, if you do this too quickly after spinning the dial the radio does not recognize.
                    RDU.schedule(15, function() { RDU.press(FrontPanelButton.Exit); })
                  })
            })
        })
    }
    //Dial in a frequency. Must be passed as a string
    function directDialAM(newSetting) {
        exitToHome(function(){
            //Define buttons and symbols we'll use
            var SmallVFOPt1 = Qt.point(200,67) //Left "." in VFO , if waterfall shown
            var SmallVFOPt2 = Qt.point(273,67) //Right "." in VFO, if waterfall shown
            var SmallVFOClick = Qt.point(185,56) //The Mhz digit to click to get to dialer
            var BigVFOClick = Qt.point(110,70) //Same, but if no waterfall
            var FINPClick = Qt.point(377,94) //FINP button on dialer
            var ent = Qt.point(375,200) //Enter button on dialer
            var mode = Qt.point(100,15) //Mode button top left
            var modeAm = Qt.point(240,70) //AM button in mode menu

            //Digits on the direct dialer, starting with 0
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
                    Qt.point(300,200)]

            //Read both pixel values.
            var smallVFO1 = RDU.pixel(SmallVFOPt1)
            var smallVFO2 = RDU.pixel(SmallVFOPt2)
            //If they match and are white, the waterfall is up
            var smallVFO = Qt.colorEqual(smallVFO1, smallVFO2) && Qt.colorEqual(smallVFO1, "white")
            //console.log("VFO: " + (smallVFO ? "Small" : "Large"))
            //Starting 4 * 33ms from now start our actions
            var delayIdx = 4;
            //Between each action, wait 4 * 33ms
            var delayAmount = 4;

            RDU.schedule(delayIdx, function() { RDU.touch(mode) })
            delayIdx = delayIdx + delayAmount;
            RDU.schedule(delayIdx, function() { RDU.touch(modeAm) })
            delayIdx = delayIdx + delayAmount;
            RDU.schedule(delayIdx, function() { RDU.touch(smallVFO ? SmallVFOClick : BigVFOClick) })
            delayIdx = delayIdx + delayAmount;
            RDU.schedule(delayIdx, function() { RDU.touch(FINPClick) })
            delayIdx = delayIdx + delayAmount;
            //Take parameter and map it to digits and a lambda
            var toTouch = newSetting.split('').map(x => function() { RDU.touch(digits[x]) })
            //Queue up the digits
            for(var i = 0; i < toTouch.length; i++) {
                RDU.schedule(delayIdx, toTouch[i])
                delayIdx = delayIdx + delayAmount;
            }
            RDU.schedule(delayIdx++, function() { RDU.touch(ent) })
            delayIdx = delayIdx + delayAmount;
            //No need to press exit as ENT brings us home.
        })
    }
    /*
      Press the "EXIT" button, up to 4 times to get back to the home Screen

      THe situation where you need to press exit 4 times is:
        In the Menu
            In the Settings submenu
                Adjusting a setting with the multi dial
                    The screen is asleep
      */
    function exitToHome(atHomeCallback, recurisonLimit = 4) {
        console.log("Exit to home, level " + recurisonLimit)
        if(atHomeScreen()) {
            console.log("Exit to home, at home screen")
            atHomeCallback()
        } else {
            if(recurisonLimit !== 0) {
                RDU.schedule(5,function() {
                    RDU.press(FrontPanelButton.Exit)
                    exitToHome(atHomeCallback, recurisonLimit-1)
                })
            } else {
                console.log("Failed to get to home screen")
            }
        }
    }
    function atHomeScreen() {
        var timeDot1Pt = Qt.point(452,6)
        var timeDot2Pt = Qt.point(452,14)

        //Read both pixel values.
        var timeDot1 = RDU.pixel(timeDot1Pt)
        var timeDot2 = RDU.pixel(timeDot2Pt)
        //Assumes that if you can see the clock in the top right the home screen is active
        //Doesnt work when you have the mode select menu up... donno what to do there.
        var dotsPresent = Qt.colorEqual(timeDot1, timeDot2) && Qt.colorEqual(timeDot1, "white")
        return dotsPresent
    }
}
