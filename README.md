# Night of the Dark Raven - MMORPG Project

## Обзор
Night of the Dark Raven - это MMORPG в фэнтези-стиле на базе Unreal Engine 5.8.4. Проект включает в себя сервер авторизации, игровой сервер и клиент.

## Структура проекта
```
NightOfDarkRaven/
├── authserver/              # Сервер авторизации (C++ проект)
├── client/                  # UE5.8 клиент
├── server/                  # Серверные скрипты и конфигурация
│   ├── sql/                # SQL скрипты для баз данных
│   └── NightOfDarkRaven.Build.cs
├── authserver.conf          # Конфигурационный файл сервера
└── .gitignore              # Игнорируемые файлы
└── README.md               # Описание проекта
```

## Требования
- Unreal Engine 5.8
- Visual Studio 2022
- MySQL 8.0
- Boost 1.92.0 (C:\local\boost_1_92_0)

## Настройка и запуск
### 1. Настройка баз данных
Выполните скрипты из папки `server/sql/initial/`:
- `initial_db.sql` - Создает базы `ndv_auth` и `ndv_world` с таблицами
- `characters_init.sql` - Создает таблицу персонажей и процедуру `sp_create_character`

3. Настройка базы данных
- MySQL сервер должен быть запущен
- Использовать учетные данные: `root` / `root`
- Базы данных: `ndv_auth`, `ndv_world`, `ndv_characters`

4. Запуск серверов
- **Auth server**: `authserver.exe` (порт 3724)
- **Игровой сервер**: `worldserver.exe` (порт 8085)
- **Клиент**: Unreal Engine 5.8 проект

5. Вход в систему
- Дефолтный аккаунт: `raven` / `raven`

## Структура баз данных
Проект использует три базы данных:
- `ndv_auth` - Аутентификация аккаунтов (таблицы: account, realm, account_protection, ip_bans, protection_settings, user_roles, role_permissions, account_roles)
- `ndv_world` - Мировые данные (таблица: characters)
- `ndv_characters` - Отдельная база персонажей (через separate_chars_db.sql)

## Запуск
1. Настройте `authserver.conf` с вашими данными MySQL
2. Выполните скрипты из папки `server/sql/initial/`
3. Запустите `authserver.exe`
4. Запустите `worldserver.exe`
5. Запустите клиент Unreal Engine 5.8 и войдите под аккаунтом `raven` / `raven`

## Лицензия
Проект создан в образовательных целях.

## Авторы
Проект entwickelt als Teil einer Bildungsinitiative von MMORPG development.

## Связь
Для вопросов и связи свяжитесь через GitHub репозиторий.