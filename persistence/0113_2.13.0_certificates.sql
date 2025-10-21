/**
* NEURON IIoT System for Industry 4.0
* Copyright (C) 2020-2024 EMQ Technologies Co., Ltd All rights reserved.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 3 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

BEGIN TRANSACTION;

-- Create server certificates table for managing SSL/TLS certificates
CREATE TABLE IF NOT EXISTS server_certificates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    app_name TEXT NOT NULL UNIQUE,
    common_name TEXT NOT NULL,
    subject TEXT,
    issuer TEXT,
    valid_from DATETIME,
    valid_to DATETIME,
    serial_number TEXT,
    fingerprint TEXT,
    certificate_data BLOB NOT NULL,
    private_key_data BLOB,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,

    FOREIGN KEY (app_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE
);

-- Create indexes for server certificates table to improve query performance
CREATE INDEX IF NOT EXISTS idx_server_certificates_app_name ON server_certificates(app_name);
CREATE INDEX IF NOT EXISTS idx_server_certificates_common_name ON server_certificates(common_name);

-- Create trigger to automatically update the updated_at timestamp for server certificates
CREATE TRIGGER IF NOT EXISTS trg_server_certificates_updated_at 
    AFTER UPDATE ON server_certificates 
    FOR EACH ROW 
BEGIN 
    UPDATE server_certificates 
    SET updated_at = CURRENT_TIMESTAMP 
    WHERE id = NEW.id; 
END;

-- Create client certificates table for managing client SSL/TLS certificates
CREATE TABLE IF NOT EXISTS client_certificates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    app_name TEXT NOT NULL,
    common_name TEXT NOT NULL,
    subject TEXT,
    issuer TEXT,
    valid_from DATETIME,
    valid_to DATETIME,
    serial_number TEXT,
    fingerprint TEXT,
    certificate_data BLOB NOT NULL,
    is_ca INTEGER DEFAULT 0,
    trust_status INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (app_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE
);

-- Create indexes for client certificates table to improve query performance
CREATE INDEX IF NOT EXISTS idx_client_certificates_app_name ON client_certificates(app_name);
CREATE INDEX IF NOT EXISTS idx_client_certificates_common_name ON client_certificates(common_name);
CREATE INDEX IF NOT EXISTS idx_client_certificates_fingerprint ON client_certificates(fingerprint);

-- Create security policies table for northbound app security management
CREATE TABLE IF NOT EXISTS security_policies (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    app_name TEXT NOT NULL UNIQUE,
    policy_name TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (app_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE
);

-- Create indexes for security policies table to improve query performance
CREATE INDEX IF NOT EXISTS idx_security_policies_app_name ON security_policies(app_name);
CREATE INDEX IF NOT EXISTS idx_security_policies_policy_name ON security_policies(policy_name);

-- Create trigger to automatically update the updated_at timestamp for security policies
CREATE TRIGGER IF NOT EXISTS trg_security_policies_updated_at 
    AFTER UPDATE ON security_policies 
    FOR EACH ROW 
BEGIN 
    UPDATE security_policies 
    SET updated_at = CURRENT_TIMESTAMP 
    WHERE id = NEW.id; 
END;

-- Create auth_settings table for managing authentication enable status for northbound applications
CREATE TABLE IF NOT EXISTS auth_settings (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    app_name TEXT NOT NULL UNIQUE,
    enabled BOOLEAN NOT NULL DEFAULT FALSE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (app_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE
);

-- Create index for auth_settings table to improve query performance
CREATE INDEX IF NOT EXISTS idx_auth_settings_app_name ON auth_settings(app_name);

-- Create trigger to automatically update the updated_at timestamp for auth_settings
CREATE TRIGGER IF NOT EXISTS trg_auth_settings_updated_at 
    AFTER UPDATE ON auth_settings 
    FOR EACH ROW 
BEGIN 
    UPDATE auth_settings 
    SET updated_at = CURRENT_TIMESTAMP 
    WHERE id = NEW.id; 
END;

-- Create auth_users table for managing user credentials for northbound applications
-- Each northbound app can have multiple username/password pairs
CREATE TABLE IF NOT EXISTS auth_users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    app_name TEXT NOT NULL,
    username TEXT NOT NULL,
    password_hash TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    FOREIGN KEY (app_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE,
    UNIQUE(app_name, username)
);

-- Create indexes for auth_users table to improve query performance
CREATE INDEX IF NOT EXISTS idx_auth_users_app_name ON auth_users(app_name);
CREATE INDEX IF NOT EXISTS idx_auth_users_username ON auth_users(username);

-- Create trigger to automatically update the updated_at timestamp for auth_users
CREATE TRIGGER IF NOT EXISTS trg_auth_users_updated_at 
    AFTER UPDATE ON auth_users 
    FOR EACH ROW 
BEGIN 
    UPDATE auth_users 
    SET updated_at = CURRENT_TIMESTAMP 
    WHERE id = NEW.id; 
END;

COMMIT;
