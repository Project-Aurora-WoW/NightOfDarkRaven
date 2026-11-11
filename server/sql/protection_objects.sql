-- protection_objects.sql - Views, Procedures, and Triggers for protection module

-- ------------------------------------------------------------
-- View: vw_locked_accounts
-- Shows all currently locked accounts for admin reference
-- ------------------------------------------------------------

CREATE OR REPLACE VIEW vw_locked_accounts AS
SELECT 
  ap.username,
  ap.failed_attempts,
  ap.total_failed_attempts,
  ap.last_attempt_ip,
  ap.last_attempt_time,
  ap.is_locked,
  ap.lock_until,
  TIMESTAMPDIFF(MINUTE, ap.lock_until, NOW()) AS minutes_until_unlock
FROM account_protection ap
WHERE ap.is_locked = 1
  AND (ap.lock_until IS NULL OR ap.lock_until > NOW());

-- ------------------------------------------------------------
-- View: vw_active_bans
-- Shows all currently active IP bans
-- ------------------------------------------------------------

CREATE OR REPLACE VIEW vw_active_bans AS
SELECT 
  ib.id,
  ib.ip_address,
  ib.ban_reason,
  ib.ban_date,
  TIMESTAMPDIFF(MINUTE, ib.ban_date, NOW()) AS minutes_since_ban,
  TIMESTAMPDIFF(MINUTE, ib.expires_at, NOW()) AS minutes_until_expire,
  ib.is_active
FROM ip_bans ib
WHERE ib.is_active = 1
  AND (ib.expires_at IS NULL OR ib.expires_at > NOW());

-- ------------------------------------------------------------
-- Stored Procedure: sp_record_failed_attempt
-- Records a failed login attempt and handles lockout logic
-- Parameters: p_username, p_ip_address, p_was_locked OUT, p_lock_minutes OUT
-- ------------------------------------------------------------

DELIMITER //
CREATE PROCEDURE sp_record_failed_attempt(
  IN p_username VARCHAR(32),
  IN p_ip_address VARCHAR(45),
  OUT p_was_locked TINYINT(1),
  OUT p_lock_minutes INT
)
BEGIN
  DECLARE v_account_id INT;
  DECLARE v_current_failed INT;
  DECLARE v_lock_duration INT;
  
  -- Get account ID from username
  SELECT id INTO v_account_id FROM account WHERE username = p_username;
  
  -- If account doesn't exist, create protection entry anyway
  IF v_account_id IS NULL THEN
    INSERT INTO account_protection (account_id, username, failed_attempts, is_locked)
    VALUES (0, p_username, 1, 0);
    SET v_account_id = 0;
  END IF;
  
  -- Increment failed attempts
  UPDATE account_protection 
  SET 
    failed_attempts = failed_attempts + 1,
    total_failed_attempts = total_failed_attempts + 1,
    last_attempt_ip = p_ip_address,
    last_attempt_time = NOW(),
    updated_at = NOW()
  WHERE account_id = v_account_id;
  
  -- Get updated failed count
  SELECT failed_attempts INTO v_current_failed FROM account_protection WHERE account_id = v_account_id;
  
  -- Get lock duration from settings
  SELECT setting_value INTO v_lock_duration FROM protection_settings 
    WHERE setting_key = 'lockout_duration_minutes';
  
  -- Check if account should be locked
  IF v_current_failed >= (SELECT setting_value FROM protection_settings WHERE setting_key = 'max_failed_attempts') 
     AND is_locked = 0 THEN
    
    -- Lock the account
    UPDATE account_protection 
    SET 
      is_locked = 1,
      lock_until = DATE_ADD(NOW(), INTERVAL v_lock_duration MINUTE),
      updated_at = NOW()
    WHERE account_id = v_account_id;
    
    SET p_was_locked = 1;
    SET p_lock_minutes = v_lock_duration;
    
  ELSE
    SET p_was_locked = 0;
    SET p_lock_minutes = 0;
  END IF;
END //
DELIMITER ;

-- ------------------------------------------------------------
-- Stored Procedure: sp_reset_login_attempts
-- Resets failed attempts after successful login
-- ------------------------------------------------------------

DELIMITER //
CREATE PROCEDURE sp_reset_login_attempts(
  IN p_username VARCHAR(32)
)
BEGIN
  DECLARE v_account_id INT;
  
  -- Get account ID
  SELECT id INTO v_account_id FROM account WHERE username = p_username;
  
  -- If account has protection entry, reset it
  IF v_account_id IS NOT NULL THEN
    UPDATE account_protection 
    SET 
      failed_attempts = 0,
      is_locked = 0,
      lock_until = NULL,
      updated_at = NOW()
    WHERE account_id = v_account_id;
  END IF;
END //
DELIMITER ;

-- ------------------------------------------------------------
-- Stored Procedure: sp_check_account_locked
-- Checks if an account is currently locked
-- ------------------------------------------------------------

DELIMITER //
CREATE PROCEDURE sp_check_account_locked(
  IN p_username VARCHAR(32),
  OUT p_is_locked TINYINT(1),
  OUT p_lock_until DATETIME,
  OUT p_minutes_remaining INT
)
BEGIN
  SELECT 
    is_locked,
    lock_until,
    TIMESTAMPDIFF(MINUTE, lock_until, NOW()) INTO p_is_locked, p_lock_until, p_minutes_remaining
  FROM account_protection 
  WHERE username = p_username;
  
  -- If lock has expired, auto-unlock
  IF p_is_locked = 1 AND p_lock_until < NOW() THEN
    UPDATE account_protection SET is_locked = 0, lock_until = NULL, updated_at = NOW() WHERE username = p_username;
    SET p_is_locked = 0;
    SET p_minutes_remaining = 0;
  END IF;
END //
DELIMITER ;

-- ------------------------------------------------------------
-- Stored Procedure: sp_maintain_protection_indices
-- Maintenance procedure for query optimization
-- ------------------------------------------------------------

DELIMITER //
CREATE PROCEDURE sp_maintain_protection_indices()
BEGIN
  -- Analyze tables for query optimization
  ANALYZE TABLE account_protection;
  ANALYZE TABLE ip_bans;
  ANALYZE TABLE protection_settings;
  
  -- Optimize tables if needed
  OPTIMIZE TABLE account_protection, ip_bans, protection_settings;
END //
DELIMITER ;