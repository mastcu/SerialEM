;; AutoIT script to change aperture size, used when UI automation fails to do so
;; The first argument is the type of function to call: just Aperture for now
;; The second and third arguments are the aperture index and list element index, numbered from 0

;#include <MsgBoxConstants.au3>

If StringInStr($CmdLine[1], "aperture") Then
   Call("Aperture", $CmdLine[2], $CmdLine[3])
   Exit(0)
ElseIf StringInStr($CmdLine[1], "apBySize") Then
   Call("ApertureBySize", $CmdLine[2], $CmdLine[3])
   Exit(0)
Endif
Exit(1)

Func Aperture($AP, $SEL)
    Local $comboNames[5]
    $comboNames[0] = "[CLASS:ComboBox; INSTANCE:1]"
    $comboNames[1] = "[CLASS:ComboBox; INSTANCE:2]"
    $comboNames[2] = "[CLASS:ComboBox; INSTANCE:3]"
    $comboNames[3] = "[CLASS:ComboBox; INSTANCE:4]"
    $comboNames[4] = "[CLASS:ComboBox; INSTANCE:5]"
    WinActivate ("[TITLE:Apertures; CLASS:#32770; W:243; H:225]")
    ControlCommand ("Apertures", "", $comboNames[$AP], "ShowDropDown", "")
    $CB_SETCURSEL = 0x14E
    $hWnd=ControlGetHandle("Apertures", "", $comboNames[$AP]);
    DllCall("user32.dll", "int", "SendMessage", "hwnd", $hWnd, "int", $CB_SETCURSEL, "int", $SEL, "int", 0)
    ControlSend ("Apertures", "",$comboNames[$AP], "{ENTER}")
EndFunc

Func ApertureBySize($AP, $SEL)
    Local $comboNames[5]
    $comboNames[0] = "[CLASS:ComboBox; INSTANCE:1]"
    $comboNames[1] = "[CLASS:ComboBox; INSTANCE:2]"
    $comboNames[2] = "[CLASS:ComboBox; INSTANCE:3]"
    $comboNames[3] = "[CLASS:ComboBox; INSTANCE:4]"
    $comboNames[4] = "[CLASS:ComboBox; INSTANCE:5]"
    WinActivate ("[TITLE:Apertures; CLASS:#32770; W:243; H:225]")
    ControlSend ("Apertures", "",$comboNames[$AP], $SEL)
EndFunc
