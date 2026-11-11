-- protection_tables.sql - Table definitions for protection module
CREATE TABLE IF NOT EXISTS account_protection (
  id int unsigned NOT NULL AUTO_INCREMENT,
  account_id int unsigned NOT NULL,
  username varchar(32) NOT NULL,
  failed_attempts int unsigned NOT NULL DEFAULT 0,
  total_failed_attempts int unsigned NOT NULL DEFAULT 0,
  last_attempt_ip varchar(45) DEFAULT NULL,
  last_attempt_time datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  is_locked tinyint(1) NOT NULL DEFAULT 0,
  lock_until datetime DEFAULT NULL,
  created_at datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  updated_at datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uq_account_id (account_id),
  KEY idx_username (username),
  KEY idx_is_locked (is_locked),
  KEY idx_last_attempt_time (last_attempt_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Brute-force protection state per account';

CREATE TABLE IF NOT EXISTS ip_bans (
  id int unsigned NOT NULL AUTO_INCREMENT,
  ip_address varchar(45) NOT NULL,
  banned_by varchar(64) DEFAULT NULL,
  ban_reason varchar(128) DEFAULT 'Brute-force protection',
  ban_date datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  expires_at datetime DEFAULT NULL,
  is_active tinyint(1) NOT NULL DEFAULT 1,
  PRIMARY KEY (id),
  UNIQUE KEY uq_ip_address (ip_address),
  KEY idx_expires_at (expires_at),
  KEY idx_is_active (is_active)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='IP ban list for brute-force and flood protection';

CREATE TABLE IF NOT EXISTS protection_settings (
  id int unsigned NOT NULL AUTO_INCREMENT,
  setting_key varchar(64) NOT NULL,
  setting_value varchar(128) NOT NULL,
  description varchar(255) DEFAULT NULL,
  updated_at datetime NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uq_setting_key (setting_key)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='Protection system configuration settings';

INSERT INTO protection_settings (setting_key, setting_value, description) VALUES
('max_failed_attempts', '5', 'Max failed logins before account lockout'),
('lockout_duration_minutes', '30', 'How long account stays locked (minutes)'),
('rate_limit_per_second', '5', 'Max login attempts per second per IP'),
('ip_banning_enabled', '1', 'Enable IP-based blocking'),
('auto_unlock_enabled', '1', 'Automatically unlock accounts after expiry');

-- ------------------------------------------------------------
-- Initial data: Default roles and permissions
-- ------------------------------------------------------------

-- Default roles: user, game_master, administrator
INSERT INTO user_roles (role_name, role_display_name, is_administrator) VALUES
('user', 'Игрок', 0),
('game_master', 'Game Master', 0),
('administrator', 'Администратор', 1);

-- ------------------------------------------------------------
-- Permissions for each role
-- ------------------------------------------------------------

-- User permissions (basic login and gameplay)
INSERT INTO role_permissions (role_id, permission_key, permission_description) VALUES
(1, 'auth_login', 'Basic login capability'),
(1, 'realm_list', 'View realm list'),
(1, 'character_select', 'Select character');

-- Game Master permissions (moderation commands)
INSERT INTO role_permissions (role_id, permission_key, permission_description) VALUES
(2, 'auth_login', 'Basic login capability'),
(2, 'realm_list', 'View realm list'),
(2, 'character_select', 'Select character'),
(2, 'gm_commands', 'Access to Game Master commands'),
(2, 'kick_player', 'Kick players from world'),
(2, 'ban_player', 'Ban players from world'),
(2, 'mute_player', 'Mute players in chat');

-- Administrator permissions (full control)
INSERT INTO role_permissions (role_id, permission_key, permission_description) VALUES
(3, 'auth_login', 'Basic login capability'),
(3, 'realm_list', 'View realm list'),
(3, 'character_select', 'Select character'),
(3, 'gm_commands', 'Access to Game Master commands'),
(3, 'kick_player', 'Kick players from world'),
(3, 'ban_player', 'Ban players from world'),
(3, 'mute_player', 'Mute players in chat'),
(3, 'shutdown_server', 'Graceful server shutdown'),
(3, 'restart_server', 'Restart server'),
(3, 'change_config', 'Modify server configuration'),
(3, 'manage_accounts', 'View/edit user accounts'),
(3, 'view_logs', 'View server logs'),
(3, 'change_language', 'Change console language'),
(3, 'add_account', 'Add new accounts'),
(3, 'delete_account', 'Delete accounts');

-- ------------------------------------------------------------
-- Assign default roles to existing accounts
-- ------------------------------------------------------------

-- Assign 'user' role to all existing accounts
INSERT INTO account_roles (account_id, role_id)
SELECT a.id, 1 FROM account a
ON DUPLICATE KEY UPDATE role_id = 1;