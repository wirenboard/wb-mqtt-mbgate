#!/bin/bash

set -x

if [[ $# < 1 ]]; then
    echo "Usage: $0 wirenboard_address"
    exit 1
fi

HOST=$1

# get flooding script from repository if it's not here
if [[ ! -s flood.py ]]; then
    echo "Getting flood script from Github..."
    wget "https://raw.githubusercontent.com/contactless/mqtt-tools/master/flood.py"
    chmod +x ./flood.py
fi

# create our tmp dir
TMPDIR=`mktemp -d`

# get current benchmark configuration
. ./benchmark.conf

# generate benchmark configuration for wb-mqtt-mbgate
./generate_test_config.py -c $TMPDIR/gw-bench.conf -n $NUM_CONTROLS -p 502


# copy it on remote device, stop remote daemon and start our instance
scp $TMPDIR/gw-bench.conf root@$HOST\:/tmp/gw-bench.conf

ssh root@$HOST "rm -rf /tmp/gw-profile.out && service wb-mqtt-mbgate stop && /usr/bin/wb-mqtt-mbgate -c /tmp/gw-bench.conf" &
SSH_MBGATE_PID=$!

# start flooder on given frequency
./flood.py -h $HOST -p 1883 -f $MQTT_UPDATE_FQ -n $NUM_CONTROLS &
FLOODER_PID=$!


# run benchmark itself
echo "Waiting for gateway to get up..."

while ! nc -z $HOST 502; do
    sleep 1
done

./modbus_poll.py -c ./modbus_poll.conf -s $HOST -p 502 -t $NUM_CLIENTS -n $NUM_CONTROLS -f $MODBUS_POLL_FQ &
POLLER_PID=$!

trap "echo 'Signal received, stopping...' && kill $POLLER_PID" SIGINT SIGTERM

wait $POLLER_PID

# clean up
echo "Cleaning up environment..."

kill $SSH_MBGATE_PID
wait $SSH_MBGATE_PID

# kill $FLOODER_PID
wait $FLOODER_PID

ssh root@$HOST $'PID=`ps aux | grep "[w]b-mqtt-mbgate" | grep -v "bash" | head -1 | awk \'{print $2}\'` && kill $PID && while kill -0 $PID 2>/dev/null; do sleep 1; done  && rm -rf /tmp/gw-bench.conf'

scp root@$HOST:./gmon.out ./gmon.out

rm -rf TMPDIR
