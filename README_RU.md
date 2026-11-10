# Rondo

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/Assets/Identity/rondo-wordmark-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="assets/Assets/Identity/rondo-wordmark-light.svg">
    <img src="assets/Assets/Identity/rondo-wordmark-light.svg" alt="Rondo" width="420">
  </picture>
</p>

<p align="center"><a href="README.md">English</a></p>

Rondo — оболочка для ретроигр на Anbernic RG34XXSP. Мне понравилась эта приставка, и я захотел сделать для неё оболочку, которой самому было бы удобно пользоваться каждый день.

## Скачать и установить

[Скачать Rondo для RG34XXSP](https://github.com/phoenixweiss/rondo/releases/latest) · [Установка, обновление и откат](docs/installation_ru.md)

Установите Rondo на карту, с которой уже загружается BaseOS на вашей RG34XXSP. Если вы только записали образ на карту, дайте BaseOS завершить первый запуск. В ZIP входят оболочка, ядра эмуляторов, исходники и лицензии. Образ BaseOS, коммерческие игры и проприетарные BIOS в архив не входят.

На BaseOS 1.3.0, 1.4.0 и 1.5.3 пакет проходит автоматические проверки. Bluetooth-геймпады и работу с двумя картами проверяли на RG34XXSP с BaseOS 1.5.3, предварительной версией самой BaseOS. Подробнее о том, что проверялось, — в [совместимости](docs/compatibility.md).

## Возможности

- Единая библиотека или группировка по системам, списки или плитки с обложками, поиск, избранное и недавние игры. [Навигация по библиотеке](docs/library.md)
- Игры на TF1, TF2 или на обеих картах. Для сохранений, скриншотов, BIOS, резервных копий, журналов и кэша обложек можно выбрать карту отдельно. [Подготовка TF2 и перенос данных](docs/storage-layout.md#format-tf2)
- Автоматическое продолжение игры, ручные слоты с превью и резервные копии прогресса. [Автосохранение](docs/autosave.md) · [Копии и восстановление](docs/backups.md)
- Bluetooth-геймпады и до четырёх игроков в играх, которые это поддерживают. Встроенное управление остаётся у первого игрока. Два 8BitDo Micro в режиме D проверены вместе в NES. [Bluetooth](docs/bluetooth.md) · [Управление и вибрация](docs/input-test.md)
- Скриншоты по L3 + R3 или из меню игры. [Сочетания и расположение файлов](docs/screenshots.md)
- Мастер настройки на русском и английском. В настройках доступны темы, параметры экрана, питание, Wi-Fi и время, которое можно задать вручную или синхронизировать по сети. [Мастер настройки](docs/setup.md) · [Настройки](docs/preferences.md) · [Экран](docs/display.md) · [Питание](docs/power.md) · [Wi-Fi](docs/wifi.md)

## Игровые системы

| Система | Ядро | Особенности |
| --- | --- | --- |
| NES | FCEUmm | [Управление, сохранения картриджей и слоты](docs/nes.md) |
| Game Boy / Game Boy Color | Gambatte | [Сохранения картриджей, часы и слоты](docs/game-boy.md) |
| Game Boy Advance | gpSP | [Ядро и совместимость](docs/game-boy-advance.md) |
| Super Nintendo | Supafaust | [Ядро и совместимость](docs/super-nintendo.md) |
| Mega Drive / Genesis | PicoDrive | [Шестикнопочная раскладка и возможности ядра](docs/mega-drive.md) |
| PlayStation | PCSX ReARMed | [CHD, многодисковые игры, BIOS и управление](docs/playstation.md) |

Для PlayStation по умолчанию используется встроенный OpenBIOS. Свой поддерживаемый BIOS или режим HLE можно выбрать в разделе **Настройки → Настройки ядер → Sony PlayStation**. Совместимость и скорость зависят от игры.

## Разработка

На macOS или Linux потребуются компилятор C++17, make, pkg-config, SDL2, SDL2_ttf и Python 3.11+. Команды установки зависимостей — в [руководстве разработчика](docs/development.md).

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements.txt
.venv/bin/python tools/build_core.py
.venv/bin/python tools/check.py
```

Команда `PYTHON=.venv/bin/python sh tools/demo.sh` откроет демо со звуком и техническим тестовым картриджем. Библиотека и данные демо по умолчанию хранятся в `build/demo`. См. [управление с клавиатуры](docs/development.md) и [сборку и проверку пакета H700](runtime/README.md).

[История изменений](CHANGELOG.md) · [Планы](docs/roadmap.md) · [CI](docs/ci.md) · [Выпуск версий](docs/release.md)

## Участие в проекте

Приветствуются сообщения об ошибках с шагами воспроизведения. Существенные пулл-реквесты лучше обсудить заранее, пока я разбираю накопившиеся задачи проекта. Подробнее в [CONTRIBUTING.md](CONTRIBUTING.md).

## Автор и лицензия

Проект создал и поддерживает [Павел Ткачёв (@phoenixweiss)](https://github.com/phoenixweiss).

Код и оригинальная графика Rondo распространяются по MIT. У сторонних компонентов [свои лицензии](THIRD_PARTY_NOTICES.md), включая GPLv2 для ряда ядер и некоммерческие условия PicoDrive.

[MIT](LICENSE) © 2026 PAVEL TKACHEV
