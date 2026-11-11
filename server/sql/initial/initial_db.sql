-- initial_db.sql - Initial database creation script
-- Creates default databases and tables if they don't exist

-- ------------------------------------------------------------
-- 1. Create Authentication Database (ndv_auth)
-- ------------------------------------------------------------
CREATE DATABASE IF NOT EXISTS ndv_auth CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
USE ndv_auth;

-- Table: account
CREATE TABLE IF NOT EXISTS `account` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `username` varchar(32) NOT NULL,
  `salt` binary(32) NOT NULL,
  `verifier` binary(32) NOT NULL,
  `email` varchar(128) DEFAULT NULL,
  `joindate` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `last_ip` varchar(45) DEFAULT NULL,
  `locked` tinyint(1) NOT NULL DEFAULT 0,
  `expansion` tinyint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- Table: realm
CREATE TABLE IF NOT EXISTS `realm` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `name` varchar(64) NOT NULL,
  `address` varchar(255) NOT NULL,
  `port` int unsigned NOT NULL DEFAULT 8085,
  `icon` int unsigned NOT NULL DEFAULT 0,
  `color` int unsigned NOT NULL DEFAULT 0,
  `timezone` int unsigned NOT NULL DEFAULT 0,
  `population` float NOT NULL DEFAULT 0.0,
  `difficulty` tinyint unsigned NOT NULL DEFAULT 0,
  `bundled` tinyint(1) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

-- Insert default data
INSERT IGNORE INTO `account` (`username`, `salt`, `verifier`, `email`, `expansion`)
VALUES ('raven',
        UNHEX('0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef'),
        UNHEX('000000000000000000000000000000000000000000000000000000000000000'),
        'dev@ndv.local', 0);

INSERT IGNORE INTO `realm` (`name`, `address`, `port`, `icon`, `color`, `population`)
VALUES ('Ночь темного Ворона', '127.0.0.1', 8085, 0, 0, 0.0);