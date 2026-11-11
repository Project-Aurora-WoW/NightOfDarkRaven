-- characters_init.sql - Characters Database Initialization Script
-- ------------------------------------------------------------
-- ------------------------------------------------------------
-- 3. Default Test Data
-- ------------------------------------------------------------

-- Insert default test characters
INSERT IGNORE INTO `characters` (`account_id`, `name`, `race`, `class`, `level`, `map_id`, `x`, `y`, `z`, `o`)
VALUES (1, 'RavenTest', 1, 1, 1, 0, 0.0, 0.0, 100.0, 0.0);

-- ------------------------------------------------------------
-- 4. Procedure Documentation
-- ------------------------------------------------------------

-- sp_create_character parameters:
-- p_account_id: The account ID to associate the character with
-- p_name: The character name (max 32 characters)
-- p_race: Race ID (1-5 common races)
-- p_class: Class ID (1-10 classes)
-- p_level: Character level (1-80)
-- p_map_id: Starting map ID
-- p_pos_x, p_pos_y, p_pos_z, p_pos_o: Starting position coordinates
--
-- Returns: new_character_id - The auto-generated character ID
-- 2. Character Creation Procedure
-- ------------------------------------------------------------

-- Stored procedure to create a new character
DELIMITER //
CREATE PROCEDURE sp_create_character(
  IN p_account_id INT,
  IN p_name VARCHAR(32),
  IN p_race TINYINT,
  IN p_class TINYINT,
  IN p_level TINYINT,
  IN p_map_id INT,
  IN p_pos_x FLOAT,
  IN p_pos_y FLOAT,
  IN p_pos_z FLOAT,
  IN p_pos_o FLOAT
)
BEGIN
  -- Validate race and class values
  IF p_race NOT IN (1, 2, 3, 4, 5) THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Invalid race value';
  END IF;
  
  IF p_class NOT IN (1, 2, 3, 4, 5, 5, 6, 7, 8, 9, 10) THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Invalid class value';
  END IF;
  
  -- Validate level (1-80 typical range)
  IF p_level < 1 OR p_level > 80 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Level out of range (1-80)';
  END IF;
  
  -- Insert the character
  INSERT INTO `characters` (
    `account_id`, `name`, `race`, `class`, `level`, 
    `map_id`, `x`, `y`, `z`, `o`
  ) VALUES (
    p_account_id, p_name, p_race, p_class, p_level,
    p_map_id, p_pos_x, p_pos_y, p_pos_z, p_pos_o
  );
  
  -- Return the newly created character ID
  SELECT LAST_INSERT_ID() AS new_character_id;
END //
DELIMITER ;
-- Creates the characters table and default data if they don't exist

-- ------------------------------------------------------------
-- 1. Create Characters Table in ndv_world Database
-- ------------------------------------------------------------
USE ndv_world;

-- Drop table if exists (for fresh initialization)
DROP TABLE IF EXISTS `characters`;

-- Create characters table
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

-- Insert default test character
INSERT IGNORE INTO `characters` (`account_id`, `name`, `race`, `class`, `level`, `map_id`, `x`, `y`, `z`, `o`)
VALUES (1, 'RavenTest', 1, 1, 1, 0, 0.0, 0.0, 100.0, 0.0);
