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

UPDATE nodes
SET plugin_name = 'MQTT'
WHERE plugin_name = 'mqtt';

UPDATE nodes
SET plugin_name = 'eKuiper'
WHERE plugin_name = 'ekuiper';

UPDATE nodes
SET plugin_name = 'SparkPlugB'
WHERE plugin_name = 'sparkplugb';

UPDATE nodes
SET plugin_name = 'Beckhoff ADS'
WHERE plugin_name = 'ads';

UPDATE nodes
SET plugin_name = 'Modbus TCP'
WHERE plugin_name = 'modbus-tcp';

UPDATE nodes
SET plugin_name = 'Modbus RTU'
WHERE plugin_name = 'modbus-rtu';

UPDATE nodes
SET plugin_name = 'Modbus TCP'
WHERE plugin_name = 'modbus-plus-tcp';

UPDATE nodes
SET plugin_name = 'Modbus TCP QH'
WHERE plugin_name = 'modbus-qh-tcp';

UPDATE nodes
SET plugin_name = 'Siemens S7 ISOTCP'
WHERE plugin_name = 's7comm';

UPDATE nodes
SET plugin_name = 'Siemens S7 ISOTCP for 300/400'
WHERE plugin_name = 's7comm_for_300';

UPDATE nodes
SET plugin_name = 'OPC UA'
WHERE plugin_name = 'opcua';

UPDATE nodes
SET plugin_name = 'Omron FINS TCP'
WHERE plugin_name = 'fins';

UPDATE nodes
SET plugin_name = 'Mitsubishi 3E'
WHERE plugin_name = 'qna3e';

UPDATE nodes
SET plugin_name = 'Mitsubishi 1E'
WHERE plugin_name = 'a1e';

UPDATE nodes
SET plugin_name = 'IEC60870-5-104'
WHERE plugin_name = 'iec104';

UPDATE nodes
SET plugin_name = 'BACnet/IP'
WHERE plugin_name = 'bacnet';

UPDATE nodes
SET plugin_name = 'KNXnet/IP'
WHERE plugin_name = 'knx';

UPDATE nodes
SET plugin_name = 'DLT645-2007'
WHERE plugin_name = 'dlt645';

UPDATE nodes
SET plugin_name = 'NON A11'
WHERE plugin_name = 'nona11';

UPDATE nodes
SET plugin_name = 'EtherNet/IP(CIP)'
WHERE plugin_name = 'EtherNet-IP';

UPDATE nodes
SET plugin_name = 'Fanuc Focas Ethernet'
WHERE plugin_name = 'focas';

UPDATE nodes
SET plugin_name = 'File'
WHERE plugin_name = 'file';

COMMIT;