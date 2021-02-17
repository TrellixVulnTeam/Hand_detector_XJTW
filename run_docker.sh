#!/bin/bash

echo "Pull The image"
sudo docker pull 247555/hand_tracker:latest
xhost +


echo -e " **** Make sure the device is video0 and video1 ! \nAnd Chaneg and MQTT IP ****"

sudo docker run --rm -d --network="host" --gpus all --device=/dev/video0:/dev/video0 --runtime nvidia -e MQTT="mqtt://localhost:1883" -e DISPLAY=$DISPLAY -v /tmp/.X11-unix/:/tmp/.X11-unix hand_tracker &
sudo docker run --rm -d --network="host" --gpus all --device=/dev/video1:/dev/video0 --runtime nvidia -e MQTT="mqtt://localhost:1883" -e DISPLAY=$DISPLAY -v /tmp/.X11-unix/:/tmp/.X11-unix hand_tracker
