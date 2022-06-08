#!/bin/sh
#
# NEURON IIoT System for Industry 4.0
# Copyright (C) 2020-2022 EMQ Technologies Co., Ltd All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

set -e -u

case $1 in
    start)
        nohup /opt/neuron/ekuiper/bin/kuiperd >> /opt/neuron/ekuiper/log/nohup.out &
        ;;
    stop)
        pid=$(ps -ef |grep kuiperd |grep -v "grep" | awk '{print $2}')
        while $(kill "$pid" 2>/dev/null); do
            sleep 1
        done
        ;;
    ping)
        if [ "$(curl -sl -w %{http_code} 127.0.0.1:9081 -o /dev/null)" = "200" ]; then
            echo pong
        else
            echo "Ping kuiper failed"
            exit 1
        fi
        ;;
    *)
        echo "Usage: $SCRIPTNAME {start|stop|ping|restart|force-reload|status}" >&2
        exit 3
        ;;
esac
