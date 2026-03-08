REM Bring some order to Arduon CLI
REM ------------------------------

REM This avoids “stuff scattered under AppData”.

REM 1. Create folders
mkdir C:\dev\arduino-workspace\_arduino
mkdir C:\dev\arduino-workspace\_arduino\data
mkdir C:\dev\arduino-workspace\_arduino\downloads
mkdir C:\dev\arduino-workspace\_arduino\user
mkdir C:\dev\arduino-workspace\projects

REM 2. Configure Arduino CLI to use those folders
REM    for configuration
arduino-cli config init
arduino-cli config set directories.data "C:\dev\arduino-workspace\_arduino\data"
arduino-cli config set directories.downloads "C:\dev\arduino-workspace\_arduino\downloads"
arduino-cli config set directories.user "C:\dev\arduino-workspace\_arduino\user"
arduino-cli config dump

REM 3. Add ESP32 support 
arduino-cli config init
arduino-cli core update-index
arduino-cli config set board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index

REM 4. Install ESP32 core:
arduino-cli core install esp32:esp32

