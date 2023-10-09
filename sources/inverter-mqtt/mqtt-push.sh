#!/bin/bash
INFLUX_ENABLED=$(cat /etc/inverter/mqtt.json | jq '.influx.enabled' -r)

MQTT_SERVER=$(cat /etc/inverter/mqtt.json | jq '.server' -r)
MQTT_PORT=$(cat /etc/inverter/mqtt.json | jq '.port' -r)
MQTT_TOPIC=$(cat /etc/inverter/mqtt.json | jq '.topic' -r)
MQTT_DEVICENAME=$(cat /etc/inverter/mqtt.json | jq '.devicename' -r)
MQTT_SERIAL=$(cat /etc/inverter/mqtt.json | jq '.serial' -r)
MQTT_USERNAME=$(cat /etc/inverter/mqtt.json | jq '.username' -r)
MQTT_PASSWORD=$(cat /etc/inverter/mqtt.json | jq '.password' -r)

pushMQTTData () {
    if [ -n "$2" ]; then
        mosquitto_pub \
            -h $MQTT_SERVER \
            -p $MQTT_PORT \
            -u "$MQTT_USERNAME" \
            -P "$MQTT_PASSWORD" \
            -i ""$MQTT_DEVICENAME"_"$MQTT_SERIAL"" \
            -t "$MQTT_TOPIC/sensor/"$MQTT_DEVICENAME"_"$MQTT_SERIAL"/$1" \
            -m "$2"
    fi
}

###############################################################################
# Inverter modes: 1 = Power_On, 2 = Standby, 3 = Line, 4 = Battery, 5 = Fault, 6 = Power_Saving, 7 = Unknown

POLLER_JSON=$(timeout 10 /opt/inverter-cli/bin/inverter_poller -1)
BASH_HASH=$(echo $POLLER_JSON | jq -r '. | to_entries | .[] | @sh "[\(.key)]=\(.value)"')
eval "declare -A INVERTER_DATA=($BASH_HASH)"

for key in "${!INVERTER_DATA[@]}"; do
    value="${INVERTER_DATA[$key]}"
    
    # Check if the value is "true" and convert it to 1, or "false" and convert it to 0
    # Otherwise dont change the value  
    if [ "$value" == "true" ]; then
        value=1
    elif [ "$value" == "false" ]; then
        value=0
    fi
    
    pushMQTTData "$key" "$value"
done
