-- Characters table for ndv_world database
-- Links characters to accounts via account_id
-- Stores character position and state for login/character selection

CREATE TABLE IF NOT EXISTS `characters` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `account_id` int unsigned NOT NULL,
  `name` varchar(32) NOT NULL,
  `race` tinyint unsigned NOT NULL DEFAULT 0,
  `class` tinyint unsigned NOT NULL DEFAULT 1,
  `level` tinyint unsigned NOT NULL DEFAULT 1,
  `map_id` int unsigned NOT NULL DEFAULT 0,
  `x` float NOT NULL DEFAULT 0.0,
  `y` float NOT NULL DEFAULT 0.0,
  `z` float NOT NULL DEFAULT 0.0,
  `o` float NOT NULL DEFAULT 0.0,
  `create_date` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `last_save` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `idx_account` (`account_id`),
  KEY `uk_account_name` (`account_id`, `name`),
  CONSTRAINT `fk_char_account` FOREIGN KEY (`account_id`) REFERENCES ndv_auth.`account`(`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Player characters linked to auth accounts';

-- Sample character for testing
INSERT IGNORE INTO `characters` (`account_id`, `name`, `race`, `class`, `level`, `map_id`, `x`, `y`, `z`, `o`)
VALUES (1, 'RavenTest', 1, 1, 1, 0, 0.0, 0.0, 100.0, 0.0);