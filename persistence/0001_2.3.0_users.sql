/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
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
--- Add users table ---
CREATE TABLE
  IF NOT EXISTS users (
    name TEXT PRIMARY KEY check(length(name) <= 32),
    password TEXT NOT NULL
  );

--- Default admin user ---
INSERT INTO
  users (name, password)
VALUES
  (
    "admin",
    "$5$PwFeXpBBIBZuZdZl$fP8fFPWCLoaWcnVXVSR.3Xi8TEqCvX92gjhowNNn6S4"
  );

--- Add tags table ---
ALTER TABLE tags RENAME TO old_tags;
CREATE TABLE IF NOT EXISTS
  tags (
    driver_name TEXT NOT NULL,
    group_name TEXT NOT NULL,
    name TEXT NULL check(length(name) <= 32),
    address TEXT NULL check(length(address) <= 64),
    attribute INTEGER NOT NULL check(attribute BETWEEN 0 AND 7),
    precision INTEGER NOT NULL check(precision BETWEEN 0 AND 17),
    decimal REAL NOT NULL,
    type INTEGER NOT NULL check(type BETWEEN 0 AND 19),
    description TEXT NULL check(length(description) <= 128),
    UNIQUE (driver_name, group_name, name),
    FOREIGN KEY (driver_name, group_name) REFERENCES groups (driver_name, name) ON UPDATE CASCADE ON DELETE CASCADE
  );
INSERT INTO tags SELECT * FROM old_tags;
DROP TABLE old_tags;
COMMIT;