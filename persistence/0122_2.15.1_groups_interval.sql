/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2026 EMQ Technologies Co., Ltd All rights reserved.
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
PRAGMA foreign_keys = OFF;

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS
  groups_new (
    driver_name TEXT NOT NULL,
    name TEXT NULL check(length(name) <= 128),
    interval INTEGER NOT NULL check(interval >= 10),
    context TEXT NULL check(length(context) <= 512),
    UNIQUE (driver_name, name),
    FOREIGN KEY (driver_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE
  );

INSERT INTO groups_new (driver_name, name, interval, context)
SELECT driver_name, name, interval, context FROM groups;

DROP TABLE groups;
ALTER TABLE groups_new RENAME TO groups;

COMMIT;

PRAGMA foreign_keys = ON;
