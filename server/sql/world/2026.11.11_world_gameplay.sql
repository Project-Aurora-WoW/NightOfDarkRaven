-- 2026.11.11 — игровой слой мира: мобы, предметы, инвентарь
-- Применять после init_db.sql / initial_db.sql

USE ndv_world;

-- ------------------------------------------------------------
-- Шаблоны существ (NPC / мобы)
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `creature_template` (
    `entry` INT UNSIGNED NOT NULL,
    `name` VARCHAR(64) NOT NULL,
    `min_level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `max_level` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `health` INT UNSIGNED NOT NULL DEFAULT 100,
    `min_damage` INT UNSIGNED NOT NULL DEFAULT 1,
    `max_damage` INT UNSIGNED NOT NULL DEFAULT 2,
    `attack_speed_ms` INT UNSIGNED NOT NULL DEFAULT 2000,
    `attack_range` FLOAT NOT NULL DEFAULT 5.0,
    `xp_reward` INT UNSIGNED NOT NULL DEFAULT 10,
    `aggro_range` FLOAT NOT NULL DEFAULT 10.0,
    `faction` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `model_id` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ------------------------------------------------------------
-- Экземпляры существ на карте (спавны)
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `creature_spawn` (
    `guid` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `entry` INT UNSIGNED NOT NULL,
    `map_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `x` FLOAT NOT NULL DEFAULT 0.0,
    `y` FLOAT NOT NULL DEFAULT 0.0,
    `z` FLOAT NOT NULL DEFAULT 0.0,
    `o` FLOAT NOT NULL DEFAULT 0.0,
    `respawn_seconds` INT UNSIGNED NOT NULL DEFAULT 30,
    PRIMARY KEY (`guid`),
    KEY `idx_map` (`map_id`),
    CONSTRAINT `fk_spawn_template` FOREIGN KEY (`entry`)
        REFERENCES `creature_template`(`entry`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ------------------------------------------------------------
-- Шаблоны предметов
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `item_template` (
    `entry` INT UNSIGNED NOT NULL,
    `name` VARCHAR(64) NOT NULL,
    `item_class` TINYINT UNSIGNED NOT NULL DEFAULT 0, -- 0=разное,1=оружие,2=броня,3=расходник
    `quality` TINYINT UNSIGNED NOT NULL DEFAULT 1,    -- 0=плохой .. 5=легендарный
    `stackable` INT UNSIGNED NOT NULL DEFAULT 1,
    `armor` INT UNSIGNED NOT NULL DEFAULT 0,
    `min_damage` INT UNSIGNED NOT NULL DEFAULT 0,
    `max_damage` INT UNSIGNED NOT NULL DEFAULT 0,
    `heal_amount` INT UNSIGNED NOT NULL DEFAULT 0,
    `buy_price` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ------------------------------------------------------------
-- Инвентарь персонажа
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `character_inventory` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `character_guid` INT UNSIGNED NOT NULL,
    `slot` SMALLINT NOT NULL,           -- 0..23 = сумки, -1 = экипировка «в руках»
    `item_entry` INT UNSIGNED NOT NULL,
    `count` INT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_char_slot` (`character_guid`, `slot`),
    KEY `idx_char` (`character_guid`),
    CONSTRAINT `fk_inv_char` FOREIGN KEY (`character_guid`)
        REFERENCES `characters`(`id`) ON DELETE CASCADE,
    CONSTRAINT `fk_inv_item` FOREIGN KEY (`item_entry`)
        REFERENCES `item_template`(`entry`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ------------------------------------------------------------
-- Лут с существ
-- ------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `creature_loot` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `creature_entry` INT UNSIGNED NOT NULL,
    `item_entry` INT UNSIGNED NOT NULL,
    `chance_percent` TINYINT UNSIGNED NOT NULL DEFAULT 100,
    `min_count` INT UNSIGNED NOT NULL DEFAULT 1,
    `max_count` INT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (`id`),
    KEY `idx_loot_entry` (`creature_entry`),
    CONSTRAINT `fk_loot_creature` FOREIGN KEY (`creature_entry`)
        REFERENCES `creature_template`(`entry`) ON DELETE CASCADE,
    CONSTRAINT `fk_loot_item` FOREIGN KEY (`item_entry`)
        REFERENCES `item_template`(`entry`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ------------------------------------------------------------
-- Тестовый контент
-- ------------------------------------------------------------
INSERT IGNORE INTO `item_template`
    (`entry`, `name`, `item_class`, `quality`, `stackable`, `armor`, `min_damage`, `max_damage`, `heal_amount`, `buy_price`)
VALUES
    (1, 'Ржавый меч',        1, 1, 1, 0,  3,  5,  0, 10),
    (2, 'Кожаный доспех',    2, 1, 1, 5,  0,  0,  0, 15),
    (3, 'Малое зелье лечения', 3, 1, 20, 0, 0,  0, 30,  5),
    (4, 'Воронье перо',      0, 2, 20, 0,  0,  0,  0,  1);

INSERT IGNORE INTO `creature_template`
    (`entry`, `name`, `min_level`, `max_level`, `health`, `min_damage`, `max_damage`, `attack_speed_ms`, `attack_range`, `xp_reward`, `aggro_range`, `faction`)
VALUES
    (100, 'Лесной волк',  2, 3, 60,  2,  4, 2000, 5.0, 25, 8.0, 1),
    (101, 'Тёмный ворон', 1, 2, 40,  1,  3, 1800, 5.0, 15, 9.0, 1);

INSERT IGNORE INTO `creature_spawn` (`guid`, `entry`, `map_id`, `x`, `y`, `z`, `o`, `respawn_seconds`)
VALUES
    (1, 100, 0,  10.0,  10.0, 100.0, 0.0, 30),
    (2, 100, 0,  20.0, -10.0, 100.0, 0.0, 30),
    (3, 101, 0, -10.0,  15.0, 100.0, 0.0, 30);

INSERT IGNORE INTO `creature_loot` (`creature_entry`, `item_entry`, `chance_percent`, `min_count`, `max_count`)
VALUES
    (100, 4, 80, 1, 2),
    (100, 2, 20, 1, 1),
    (101, 4, 100, 1, 3),
    (101, 3, 30, 1, 1);
