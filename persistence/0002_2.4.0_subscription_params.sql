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

DROP TABLE
  IF EXISTS old_subscriptions;

ALTER TABLE
  subscriptions RENAME TO old_subscriptions;

-- add column params for subscription parameters
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

INSERT INTO
  subscriptions
SELECT
  app_name,
  driver_name,
  group_name,
  NULL
FROM
  old_subscriptions;

DROP TABLE
  old_subscriptions;

COMMIT;
