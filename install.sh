#!/bin/bash 

# Simple script to build/install / update 
# For ubuntu based systems only!

# Enable debug output
DEBUG=false

# Path to old installation
PATH_TO_OLD_INSTALL=/opt/ha-inverter-mqtt-agent

# path to new installation
PATH_TO_NEW_INSTALL=/opt/ha-inverter-mqtt-agent

clear
echo "Lets install a better client for your Inverter"
echo "No more piece of sh!!# WatchPower!"
echo ""
echo "Before running this script please make sure you have a compatiple inverter. Every inverter using WatchPower should in theorie work."
echo "Furthermore, make sure your Homeassistant and MQTT server is up, accassible and running."
echo ""
echo "Please keep in mind that this project could brick or even damage your inverter and any connected device!!!"
echo "USE AT YOUR OWN RISK!!!"
echo ""

#### Check if user does know what he is doing
while true; do

read -p "Do you know what you are doing? (Y/n) " -n 1 -r yn
if [ -z "$yn" ]; then
        yn="y"  # by default if Enter is pressed 
    else 
        echo ""
fi

case $yn in 
	[yY] ) echo "Ok, I'll proceed";
        sudo rm -rf /tmp/docker-voltronic-hass
		break;;
	[nN] ) echo "I see, exiting quickly...";
		exit;;
	* ) echo "invalid response";;
esac

done
echo ""

#### Check if docker / nano should be installed
while true; do

read -p "Do you NOT have docker or nano installed and want to install it? (y/N) " -n 1 -r yn
if [ -z "$yn" ]; then
        yn="n"  # by default if Enter is pressed
    else 
        echo ""
fi

case $yn in 
	[yY] ) echo "Ok, installing docker, docker compose and nano";
        sudo apt update 
        sudo apt install ca-certificates curl nano -y
	sudo install -m 0755 -d /etc/apt/keyrings
 	sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
	sudo chmod a+r /etc/apt/keyrings/docker.asc
	
	# Add the repository to Apt sources:
	echo \
	  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
	  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
	  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
	sudo apt-get update
 	sudo apt-get install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker compose-plugin -y
        echo ""
        if ! [ "$DEBUG" = true ] ; then
            for i in {3..1};do echo -n "$i." && sleep 1; done
            clear
        fi
        
		break;;
	[nN] ) echo "I see, not doing anything...";
        echo ""
		break;;
	* ) echo "invalid response";;
esac

done

#### Check if old installation is there
while true; do

read -p "Do have an old installation? (y/N) " -n 1 -r yn
if [ -z "$yn" ]; then
        yn="n"  # by default if Enter is pressed
    else 
        echo ""
fi

case $yn in 
	[yY] ) 
        read -p "Directory to old installation: " -i "$PATH_TO_OLD_INSTALL" -e PATH_TO_OLD_INSTALL

        cd $PATH_TO_OLD_INSTALL

        echo ""
        echo "Trying to stop docker..."
        docker compose stop

        echo ""
        echo "The use of an old configuration could potentially brick your install."
        echo "It can be a time saver tho. If in doubt, skip it. Otherwise just check the config."
        echo ""

        #### Check if user wants to save the old config
        while true; do

        read -p "Do you wanna save the config of your old installation? (Y/n) " -n 1 -r yn
        if [ -z "$yn" ]; then
                yn="y"  # by default if Enter is pressed
            else 
                echo ""
        fi

        case $yn in 
            [yY] ) echo "Saving config to /tmp/docker-voltronic-hass...";
                sudo mkdir /tmp/docker-voltronic-hass
                sudo cp -rf "$PATH_TO_OLD_INSTALL/config/" /tmp/docker-voltronic-hass
                break;;
            [nN] ) echo "Installing fresh...";
                echo ""
                break;;
            * ) echo "invalid response";;
        esac

        done

        cd
        sudo rm -rf "$PATH_TO_OLD_INSTALL"

		break;;
	[nN] ) echo "Installing fresh...";
        echo ""
		break;;
	* ) echo "invalid response";;
esac

done

clear

#### Where to store new installation?
read -p "Directory to new installation: " -i "$PATH_TO_NEW_INSTALL" -e PATH_TO_NEW_INSTALL

echo ""

sudo git clone --recurse-submodules https://github.com/technickfreak2000/docker-voltronic-homeassistant.git "$PATH_TO_NEW_INSTALL"

cd "$PATH_TO_NEW_INSTALL"

sudo mv -f /tmp/docker-voltronic-hass/config/* "$PATH_TO_NEW_INSTALL/config/"

echo ""

#### Edit config?
while true; do

read -p "Would you like to edit the configs? (y/N) " -n 1 -r yn
if [ -z "$yn" ]; then
        yn="n"  # by default if Enter is pressed
    else 
        echo ""
fi

case $yn in 
	[yY] )
        sudo nano "$PATH_TO_NEW_INSTALL/config/inverter.conf" 
        sudo nano "$PATH_TO_NEW_INSTALL/config/mqtt.json" 
        clear
		break;;
	[nN] ) echo "Moving on...";
        echo ""
		break;;
	* ) echo "invalid response";;
esac

done

echo "Building Container..."

sudo docker compose build

echo ""

if ! [ "$DEBUG" = true ] ; then
    for i in {3..1};do echo -n "$i." && sleep 1; done
    clear
fi

#### Start container?
while true; do

read -p "Should I start the container? (Y/n) " -n 1 -r yn
if [ -z "$yn" ]; then
        yn="y"  # by default if Enter is pressed
    else 
        echo ""
fi

case $yn in 
	[yY] ) echo "Starting container..."
        sudo docker compose up -d
		break;;
	[nN] ) echo "Moving on...";
        echo ""
		break;;
	* ) echo "invalid response";;
esac

done

echo "Your instance should be up and running now!"

echo ""
echo "In case you missed it, you can talk to the inverter directly with: "
echo "sudo docker exec -it voltronic-mqtt bash -c '/opt/inverter-cli/bin/inverter_poller -h'"
echo ""

case $yn in 
	[yY] ) 
        if ! [ "$DEBUG" = true ] ; then
            for i in {3..1};do echo -n "$i." && sleep 1; done
            echo ""
        fi

        echo "Lets do a quick test: "
        echo "Running: sudo docker exec -it voltronic-mqtt bash -c '/opt/inverter-cli/bin/inverter_poller -d -1'"
        sudo docker exec -it voltronic-mqtt bash -c '/opt/inverter-cli/bin/inverter_poller -d -1'
esac
