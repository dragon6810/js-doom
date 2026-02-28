#!/usr/bin/env bash

sudo dnctl pipe 1 config delay $1
sudo dnctl pipe 2 config delay $1
printf "dummynet out quick on lo0 proto udp pipe 1\ndummynet in quick on lo0 proto udp pipe 2\n" | sudo pfctl -f -
sudo pfctl -e
