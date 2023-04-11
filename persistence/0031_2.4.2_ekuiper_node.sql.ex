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

-- Update default data-stream-processing node setting when
-- the data-stream-processing node exists and is at init state.
-- The init state implies the user has not provide any setting yet.
INSERT INTO
  settings (node_name, setting)
SELECT
  name,
  '{"params":{"host":"0.0.0.0","port":7081}}'
FROM
  nodes
WHERE
  name = 'data-stream-processing'
  and state = 1;

-- Update default data-stream-processing node to running state when
-- the data-stream-processing node exists and is at init state.
UPDATE
  nodes
SET
  state = 3
where
  name = 'data-stream-processing'
  and state = 1;

COMMIT;
