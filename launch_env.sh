#!/bin/bash
# docker run -it -v `pwd`:/opt/telloapp -w /opt/telloapp --net=host --rm python:latest bash
xhost +
docker run -it --rm -v /tmp/.X11-unix:/tmp/.X11-unix -e PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION=python -e DISPLAY=$DISPLAY -v `pwd`:/opt/telloapp -w /opt/telloapp --device=/dev/video0 --net=host --rm drunkcoders/tello:latest bash
