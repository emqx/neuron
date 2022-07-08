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
--- Add plugins table ---
CREATE TABLE
  plugins (library TEXT PRIMARY KEY check(length(library) <= 32));

--- Add adapters table ---
CREATE TABLE
  nodes (
    name TEXT PRIMARY KEY check(length(name) <= 32),
    type integer(1) NOT NULL check(TYPE IN (1, 2)),
    state integer(1) NOT NULL check(state BETWEEN 0 AND 4),
    plugin_name TEXT NOT NULL check(length(plugin_name) <= 32)
  );

--- Add settings table ---
CREATE TABLE
  settings (
    node_name TEXT NOT NULL,
    setting TEXT NOT NULL,
    FOREIGN KEY (node_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE
  );

--- Add groups table ---
CREATE TABLE
  groups (
    driver_name TEXT NOT NULL,
    name NOT NULL check(length(name) <= 32),
    interval INTEGER NOT NULL check(interval >= 100),
    UNIQUE (driver_name, name),
    FOREIGN KEY (driver_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE
  );

--- Add tags table ---
CREATE TABLE
  tags (
    driver_name TEXT NOT NULL,
    group_name TEXT NOT NULL,
    name NOT NULL UNIQUE check(length(name) <= 32),
    address NOT NULL check(length(address) <= 64),
    attribute INTEGER NOT NULL check(attribute BETWEEN 0 AND 7),
    type INTEGER NOT NULL check(type BETWEEN 0 AND 15),
    description NOT NULL check(length(description) <= 128),
    UNIQUE (driver_name, group_name, name),
    FOREIGN KEY (driver_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE,
    FOREIGN KEY (group_name) REFERENCES groups (name) ON UPDATE CASCADE ON DELETE CASCADE
  );

--- Add subscriptions table ---
CREATE TABLE
  subscriptions (
    app_name TEXT NOT NULL,
    driver_name TEXT NOT NULL,
    group_name INTEGER NOT NULL,
    CHECK (app_name != driver_name),
    UNIQUE (app_name, driver_name, group_name),
    FOREIGN KEY (app_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE,
    FOREIGN KEY (driver_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE,
    FOREIGN KEY (group_name) REFERENCES groups (name) ON UPDATE CASCADE ON DELETE CASCADE
  );
