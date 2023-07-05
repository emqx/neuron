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

-- For backward compatibility before version 2.5,
-- add parameters to modbus plugin.

UPDATE settings
SET setting = JSON_SET(json(setting), '$.params.transport_mode', 0)
FROM nodes
WHERE 
    settings.node_name = nodes.name
    AND JSON_EXTRACT(json(setting), '$.params.transport_mode') IS NULL 
    AND (nodes.plugin_name='modbus-qh-tcp' OR nodes.plugin_name='modbus-plus-tcp' OR nodes.plugin_name='modbus-tcp' OR nodes.plugin_name='Modbus TCP' OR nodes.plugin_name='Modbus TCP QH');

UPDATE settings
SET setting = JSON_SET(json(setting), '$.params.connection_mode', 0)
FROM nodes
WHERE 
    settings.node_name = nodes.name
    AND JSON_EXTRACT(json(setting), '$.params.connection_mode') IS NULL 
    AND (nodes.plugin_name='modbus-qh-tcp' OR nodes.plugin_name='modbus-plus-tcp' OR nodes.plugin_name='modbus-rtu' OR nodes.plugin_name='modbus-tcp' OR nodes.plugin_name='Modbus TCP' OR nodes.plugin_name='Modbus RTU' OR nodes.plugin_name='Modbus TCP QH');

UPDATE settings
SET setting = JSON_SET(json(setting), '$.params.group_interval', 1000)
FROM nodes
WHERE 
    settings.node_name = nodes.name
    AND JSON_EXTRACT(json(setting), '$.params.group_interval') IS NULL 
    AND (nodes.plugin_name='modbus-qh-tcp' OR nodes.plugin_name='modbus-plus-tcp' OR nodes.plugin_name='modbus-rtu' OR nodes.plugin_name='modbus-tcp' OR nodes.plugin_name='Modbus TCP' OR nodes.plugin_name='Modbus RTU' OR nodes.plugin_name='Modbus TCP QH');

UPDATE settings
SET setting = JSON_SET(json(setting), '$.params.interval', 20)
FROM nodes
WHERE 
    settings.node_name = nodes.name
    AND JSON_EXTRACT(json(setting), '$.params.interval') IS NULL 
    AND (nodes.plugin_name='modbus-qh-tcp' OR nodes.plugin_name='modbus-plus-tcp' OR nodes.plugin_name='modbus-rtu' OR nodes.plugin_name='modbus-tcp' OR nodes.plugin_name='Modbus TCP' OR nodes.plugin_name='Modbus RTU' OR nodes.plugin_name='Modbus TCP QH');

COMMIT;