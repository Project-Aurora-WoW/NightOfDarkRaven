-- 2026.11.11 — сессионные токены авторизации (замена файловому authShared.txt)
-- Создаётся автоматически authserver при старте (TokenStore::EnsureTable),
-- скрипт нужен для ручного развёртывания и для ревью схемы.

USE ndv_auth;

CREATE TABLE IF NOT EXISTS `session_token` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `token` BINARY(8) NOT NULL,
    `account_id` INT UNSIGNED NOT NULL,
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `expires_at` BIGINT UNSIGNED NOT NULL,
    `consumed_at` TIMESTAMP NULL DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_token` (`token`),
    KEY `idx_account` (`account_id`),
    CONSTRAINT `fk_token_account` FOREIGN KEY (`account_id`)
        REFERENCES `account`(`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
