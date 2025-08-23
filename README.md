# JoyMerge (PoC)
Объединяет два Joy-Con в один виртуальный XInput-контроллер на Android 15 (LineageOS 22.2). Требуется root (Magisk).

## Сборка на Linux (L4T или обычный ПК)
1) Установите инструменты:
```bash
sudo apt update
sudo apt install openjdk-17-jdk git unzip wget curl cmake ninja-build build-essential gradle
```
2) Поставьте Android SDK (cli-tools) и нужные компоненты:
```bash
mkdir -p $HOME/Android/cmdline-tools
cd $HOME/Android
# скачайте архив commandlinetools и распакуйте в $HOME/Android/cmdline-tools/latest
# затем:
export ANDROID_HOME=$HOME/Android
export ANDROID_SDK_ROOT=$HOME/Android
yes | $HOME/Android/cmdline-tools/latest/bin/sdkmanager --licenses
$HOME/Android/cmdline-tools/latest/bin/sdkmanager "platform-tools" "platforms;android-35" "build-tools;35.0.0" "ndk;26.3.11579264" "cmake;3.22.1"
```
3) Сборка APK:
```bash
cd JoyMerge
gradle :app:assembleDebug
```
Готовый файл: `app/build/outputs/apk/debug/app-debug.apk`

## Установка на LineageOS (Switch)
1) Скопируйте APK в раздел Android (карта памяти):
```bash
cp app/build/outputs/apk/debug/app-debug.apk /run/media/mmcblk0pX/Download/JoyMerge.apk
```
2) Перезагрузитесь в LineageOS и установите APK из папки *Download*.

## Magisk/Root и SELinux
- Дайте приложению root (запрос появится при первом запуске сервиса).
- Для теста можно временно:
```bash
su -c setenforce 0
```
- Политики (временно в live-режиме):
```bash
su -c magiskpolicy --live "allow untrusted_app input_device chr_file { read open getattr ioctl }"
su -c magiskpolicy --live "allow untrusted_app uinput_device chr_file { write open ioctl getattr }"
```
Рекомендуется оформить это в отдельный Magisk-модуль на проде.

## Использование
1) Сопрягите оба Joy-Con по Bluetooth в Android.
2) Откройте JoyMerge → **Start**. Появится виртуальный геймпад `JoyMerge Virtual XInput`.
3) **Stop** — виртуальный контроллер удалится, Joy-Con вернутся в обычный режим.

## Примечания
PoC без тонкой калибровки и авто‑reconnect. Подправьте маппинг под свои коды, если нужно.
