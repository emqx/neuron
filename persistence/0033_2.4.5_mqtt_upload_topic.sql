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

-- for backward compatibility with version 2.3,
-- migrate MQTT plugin upload-topic to subscription parameters.
WITH
  topics AS (
    SELECT
      name AS app,
      json_extract(json(setting), '$.params.upload-topic') AS topic
    FROM
      nodes
      JOIN settings
    WHERE
      name = node_name
      AND plugin_name = 'MQTT'
  )
UPDATE
  subscriptions
SET
  params = (
    SELECT
      json_object('params', json_object('topic', topic)) AS params
    FROM
      topics
    WHERE
      topics.app = subscriptions.app_name
      AND topics.topic NOT NULL
  )
WHERE
  EXISTS (
    SELECT
      topic
    FROM
      topics
    WHERE
      topics.app = subscriptions.app_name
      AND topics.topic NOT NULL
      AND subscriptions.params IS NULL
  );

COMMIT;
