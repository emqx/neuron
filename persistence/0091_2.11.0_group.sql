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

ALTER TABLE groups RENAME TO temp_groups;

CREATE TABLE IF NOT EXISTS
  groups (
    driver_name TEXT NOT NULL,
    name TEXT NULL check(length(name) <= 32),
    interval INTEGER NOT NULL check(interval >= 100),
	context TEXT NULL check(length(context) <= 512),
    UNIQUE (driver_name, name),
    FOREIGN KEY (driver_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE
  );
INSERT INTO groups SELECT driver_name, name, interval, NULL FROM temp_groups;

DROP TABLE temp_groups;
COMMIT;
