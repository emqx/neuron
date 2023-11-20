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

--- fix primary key ---
BEGIN;

DROP TABLE IF EXISTS old_node_cache;
ALTER TABLE node_cache RENAME TO old_node_cache;

CREATE TABLE IF NOT EXISTS node_cache (
  node_name TEXT PRIMARY KEY,
  CACHE TEXT NOT NULL,
  FOREIGN KEY (node_name) REFERENCES nodes (name) ON UPDATE CASCADE ON DELETE CASCADE
);

INSERT OR REPLACE INTO node_cache SELECT * FROM old_node_cache;

DROP TABLE old_node_cache;

COMMIT;
