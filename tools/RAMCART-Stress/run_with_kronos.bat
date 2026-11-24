@ECHO Off
SET EMULATOR_DIR=..\..\Emulators

if exist game.iso (
echo Press F1 inside Kronos to run the game
"%EMULATOR_DIR%\kronos\kronos.exe" -i game.iso
) else (
echo Please compile first !
)
