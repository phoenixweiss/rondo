# Rondo

[English](README.md)

Личный проект фронтенда для Anbernic RG34XXSP. Приставка мне очень понравилась, а подходящего для себя фронтенда я не нашёл — поэтому начал делать Rondo.

**Ранняя разработка.** Интерфейс и формат настроек могут меняться.

## Компоненты

- Каталог библиотеки и сохранение настроек с восстановлением после ошибки записи.
- Английская и русская локализация; английский используется при отсутствии перевода.
- Общие SDL-компоненты отрисовки, типографика, иконки и подсказки клавиатуры/контроллера.
- Компактный индикатор громкости: подтверждённый уровень, отключённый звук, ожидание и ошибка.
- Запуск NES через отдельный процесс с закреплённой версией ядра FCEUmm.
- Сохранение SRAM, ручные слоты с кадром игры и продолжение с сохранённого состояния.
- Библиотека, избранное, настройки и меню игры.
- Модельные тесты, проверки отрисовки и сценарии запуска/сохранения/продолжения без окна.

## Разработка

Нужны компилятор C++17, make, pkg-config, SDL2, SDL2_ttf и Python 3.11+.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements.txt
make -j4 all build/model-tests build/core-tests build/device-tests build/volume-ui-tests
.venv/bin/python tools/build_core.py
make test
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/volume-ui-tests assets build/volume-renders
.venv/bin/python tools/check_source.py
rondo_core=build/cores/fceumm_libretro.dylib
[ "$(uname -s)" != Linux ] || rondo_core=build/cores/fceumm_libretro.so
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy .venv/bin/python tests/run_checks.py --core "$rondo_core" --work build/native-check
```

Обычная сборка создаёт build/rondo. Ядро собирается отдельно из исходников: загрузка проверяется по SHA-256, версия закреплена в docs/dependencies/core-source.json. Проверки используют dummy-драйверы видео и звука и собственный тестовый картридж. Каталог build/native-check должен быть новым. Тесты не обращаются к SD-карте и не подтверждают работу физических кнопок, яркости или сна устройства.

## Запуск на компьютере

```sh
PYTHON=.venv/bin/python sh tools/demo.sh
```

Команда открывает окно и воспроизводит звук тестового картриджа. Библиотека и сохранения демо находятся в build/demo; путь можно задать через RONDO_DEMO_DIR.

В меню: стрелки, Enter/Z — выбор, Esc/X — назад, C — избранное/информация, V — настройки, Q/E — раздел, Tab — меню игры, =/- — громкость. В NES: Z/X — A/B, Enter — Start, Right Shift — Select.

Коммерческие ROM, BIOS и пользовательские сохранения в репозиторий не входят.

## Лицензия и участие

Код Rondo и оригинальная графика распространяются по [MIT](LICENSE). У шрифтов, иконок и остальных зависимостей сохраняются [свои условия](THIRD_PARTY_NOTICES.md).

Сообщения об ошибках с шагами воспроизведения приветствуются. Существенные PR лучше предварительно обсуждать: сейчас разработка идёт по бэклогу автора. Подробнее — в [CONTRIBUTING.md](CONTRIBUTING.md).
