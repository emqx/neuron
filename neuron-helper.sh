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


SCRIPTNAME=$(basename "$0")

usage() {
        echo "Usage: $SCRIPTNAME {start|stop|ping}" >&2
        exit 3
}

if [ $# -eq 0 ]
then
  usage
fi

case $1 in
    start)
        # start neuron first
        ./neuron --daemon
        # then start ekuiper
        cd ./ekuiper && nohup ./bin/kuiperd >> ./log/nohup.out &
        # setup neuron stream
        sleep 1
        curl -X POST --location http://127.0.0.1:9081/streams \
             -H 'Content-Type: application/json' \
             -d '{"sql":"CREATE STREAM neuronStream() WITH (TYPE=\"neuron\",FORMAT=\"json\",SHARED=\"true\");"}'
        ;;

    stop)
        # kill ekuiper first
        kill `pidof kuiperd` 2>/dev/null
        # then kill neuron
        kill `pidof neuron` 2>/dev/null
        ;;

    ping)
        if [ "$(curl -sl -w %{http_code} -X POST 127.0.0.1:7001/api/v2/ping -o /dev/null)" = "200" ]; then
            echo neuron pong
        else
            echo "ping neuron failed"
            exit 1
        fi
        if [ "$(curl -sl -w %{http_code} 127.0.0.1:9081 -o /dev/null)" = "200" ]; then
            echo ekuiper pong
        else
            echo "ping ekuiper failed"
            exit 1
        fi
        ;;

    *)
        usage
        ;;
esac
