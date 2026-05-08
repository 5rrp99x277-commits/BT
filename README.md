# BT Dummy for Canon TM-350

Эмулятор Bluetooth устройства для Canon imagePROGRAF TM-350.

## Описание

Когда на плоттере включают WiFi, он ожидает что BT (второй USB device на хабе) 
исчезнет с шины на ~6 секунд. Этот проект реализует именно это поведение.

## Аппаратное подключение

```
WiFi RP2040 GPIO3 (OUT)  ---->  GPIO3 (IN) BT RP2040
GND WiFi  --------------------->  GND BT
```

## Автоматическая сборка (GitHub Actions)

1. Создай форк этого репозитория
2. GitHub Actions автоматически соберёт .uf2 при каждом push
3. Скачай артефакт `bt_dummy_firmware` из последнего Actions run

## Ручная сборка

```bash
# Установи зависимости
sudo apt-get install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi

# Клонируй pico-sdk
git clone --recursive https://github.com/raspberrypi/pico-sdk.git
export PICO_SDK_PATH=$(pwd)/pico-sdk

# Собери
mkdir build && cd build
cmake ..
make -j$(nproc)

# Прошивка:
# build/bt_dummy.uf2 -> RP2040-Zero в bootloader mode
```

## Arduino IDE (альтернатива)

См. `arduino/` папку для версии под Arduino IDE.
