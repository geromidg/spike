#!/bin/bash
iw dev wlp8s0 scan | grep SSID | while read line; do echo ${line/SSID: /}; done