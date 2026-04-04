CREATE DATABASE IF NOT EXISTS chat_server
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE chat_server;

CREATE TABLE IF NOT EXISTS users (
  id BIGINT NOT NULL AUTO_INCREMENT,
  username VARCHAR(255) NOT NULL,
  password VARCHAR(255) NOT NULL,
  created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (id),
  UNIQUE KEY uk_users_username (username)
);

CREATE TABLE IF NOT EXISTS chat_message (
  message_id BIGINT NOT NULL AUTO_INCREMENT,
  id BIGINT NOT NULL,
  username VARCHAR(255) NOT NULL,
  session_id VARCHAR(128) NOT NULL,
  is_user TINYINT(1) NOT NULL,
  content LONGTEXT NOT NULL,
  ts BIGINT NOT NULL,
  PRIMARY KEY (message_id),
  KEY idx_chat_message_user_session_ts (id, session_id, ts)
);
