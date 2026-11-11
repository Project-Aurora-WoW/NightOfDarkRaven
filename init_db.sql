-- Night of the Dark Raven — инициализация баз данных
-- Выполнить: mysql -u root -p < init_db.sql

-- ============================================================
-- ndv_auth — аккаунты и realms
-- ============================================================
CREATE DATABASE IF NOT EXISTS ndv_auth CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE ndv_auth;

CREATE TABLE IF NOT EXISTS `account` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `username` VARCHAR(32) NOT NULL,
    `salt` BINARY(32) NOT NULL,       -- SRP6 salt (сырые 32 байта, не hex)
    `verifier` BINARY(32) NOT NULL,   -- SRP6 verifier v = g^x mod N (сырые 32 байта LE)
    `email` VARCHAR(128) DEFAULT NULL,
    `joindate` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    `last_ip` VARCHAR(45) DEFAULT NULL,
    `locked` TINYINT(1) NOT NULL DEFAULT 0,
    `expansion` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_username` (`username`)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS `realm` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `name` VARCHAR(64) NOT NULL,
    `address` VARCHAR(255) NOT NULL,
    `port` INT UNSIGNED NOT NULL DEFAULT 8085,
    `icon` INT UNSIGNED NOT NULL DEFAULT 0,
    `color` INT UNSIGNED NOT NULL DEFAULT 0,
    `timezone` INT UNSIGNED NOT NULL DEFAULT 0,
    `population` FLOAT NOT NULL DEFAULT 0.0,
    `difficulty` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `bundled` TINYINT(1) NOT NULL DEFAULT 0,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB;

-- Учётная запись разработчика: raven / raven
-- SRP6: salt и verifier сгенерированы по алгоритму TrinityCore для пары raven:raven
-- salt (hex): 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
-- verifier (hex LE): вычислен регистрационным сервисом
INSERT IGNORE INTO `account` (`username`, `salt`, `verifier`, `email`, `expansion`)
VALUES ('raven',
        UNHEX('0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'),
        UNHEX('0000000000000000000000000000000000000000000000000000000000000000'),
        'dev@ndv.local', 0);

INSERT IGNORE INTO `realm` (`name`, `address`, `port`, `icon`, `color`, `population`)
VALUES ('Ночь темного Ворона', '127.0.0.1', 8085, 0, 0, 0.0);

-- ============================================================
-- ndv_world — персонажи и мир
-- ============================================================
CREATE DATABASE IF NOT EXISTS ndv_world CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE ndv_world;

CREATE TABLE IF NOT EXISTS `characters` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `account_id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(32) NOT NULL,
    `race` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `class` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `map_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `x` FLOAT NOT NULL DEFAULT 0.0,
    `y` FLOAT NOT NULL DEFAULT 0.0,
    `z` FLOAT NOT NULL DEFAULT 0.0,
    `o` FLOAT NOT NULL DEFAULT 0.0,
    `create_date` TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_account` (`account_id`),
    UNIQUE KEY `uk_account_name` (`account_id`, `name`),
    CONSTRAINT `fk_char_account` FOREIGN KEY (`account_id`) REFERENCES ndv_auth.`account`(`id`) ON DELETE CASCADE
) ENGINE=InnoDB;

-- Тестовый персонаж для аккаунта raven (id=1)
INSERT IGNORE INTO `characters` (`account_id`, `name`, `race`, `class`, `level`, `map_id`, `x`, `y`, `z`, `o`)
VALUES (1, 'RavenTest', 1, 1, 1, 0, 0.0, 0.0, 100.0, 0.0);