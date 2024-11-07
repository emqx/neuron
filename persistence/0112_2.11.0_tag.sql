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

ALTER TABLE tags RENAME TO temp_tags;

CREATE TABLE
  IF NOT EXISTS tags (
    driver_name TEXT NOT NULL,
    group_name TEXT NOT NULL,
    name TEXT NULL check (length (name) <= 128),
    address TEXT NULL check (length (address) <= 128),
    attribute INTEGER NOT NULL check (attribute BETWEEN 0 AND 15),
    precision INTEGER NOT NULL check (precision BETWEEN 0 AND 17),
    decimal REAL NOT NULL,
    bias REAL NOT NULL check (bias BETWEEN -1000 AND 1000),
    type INTEGER NOT NULL check (type BETWEEN 0 AND 40),
    description TEXT NULL check (length (description) <= 512),
    value TEXT,
    UNIQUE (driver_name, group_name, name),
    FOREIGN KEY (driver_name, group_name) REFERENCES groups (driver_name, name) ON UPDATE CASCADE ON DELETE CASCADE
  );

INSERT INTO tags SELECT * FROM temp_tags;

DROP TABLE temp_tags;

ALTER TABLE subscriptions RENAME TO temp_subscriptions;
CREATE TABLE
  IF NOT EXISTS subscriptions (
    app_name TEXT NOT NULL,
    driver_name TEXT NOT NULL,
    group_name TEXT NOT NULL,
    params TEXT DEFAULT NULL,
    CHECK (app_name != driver_name),
    UNIQUE (app_name, driver_name, group_name),
    FOREIGN KEY (app_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE,
    FOREIGN KEY (driver_name, group_name) REFERENCES groups (driver_name, name) ON UPDATE CASCADE ON DELETE CASCADE
  );
INSERT INTO subscriptions SELECT * FROM temp_subscriptions;

DROP TABLE temp_subscriptions;

COMMIT;