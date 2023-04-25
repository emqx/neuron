/**
 * NEURON IIoT System for Industry 4.0
 * Copyright (C) 2020-2023 EMQ Technologies Co., Ltd All rights reserved.
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

CREATE TABLE
  IF NOT EXISTS templates (
    name TEXT PRIMARY KEY check(length(name) <= 128),
    plugin_name TEXT NOT NULL check(length(plugin_name) <= 32)
  );

CREATE TABLE
  IF NOT EXISTS template_groups (
    tmpl_name TEXT NOT NULL,
    name TEXT NULL check(length(name) <= 128),
    interval INTEGER NOT NULL check(interval >= 100),
    UNIQUE (tmpl_name, name),
    FOREIGN KEY (tmpl_name) REFERENCES templates (name) ON UPDATE CASCADE ON DELETE CASCADE
  );

CREATE TABLE
  IF NOT EXISTS template_tags (
    tmpl_name TEXT NOT NULL,
    group_name TEXT NOT NULL,
    name TEXT NULL check(length(name) <= 64),
    address TEXT NULL check(length(address) <= 128),
    attribute INTEGER NOT NULL check(attribute BETWEEN 0 AND 15),
    PRECISION INTEGER NOT NULL check(PRECISION BETWEEN 0 AND 17),
    decimal REAL NOT NULL,
    TYPE INTEGER NOT NULL check(TYPE BETWEEN 0 AND 19),
    description TEXT NULL check(length(description) <= 128),
    value TEXT DEFAULT NULL,
    UNIQUE (tmpl_name, group_name, name),
    FOREIGN KEY (tmpl_name, group_name) REFERENCES template_groups (tmpl_name, name) ON UPDATE CASCADE ON DELETE CASCADE
  );

COMMIT;
