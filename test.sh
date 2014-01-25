#!/bin/bash
# test d'automatisation pour la recuperation de la temperature humidite
cd /tmp;


rtl_sdr -f 868428000 -s 250000 data.raw -g 49.6 -S 2>/tmp/capture.log &
sleep 30;
killall rtl_sdr  1>/tmp/capture.log &
sleep 1;
echo "Analyse du fichier"
time /home/bengo/Outils/SDR/rtl_sdr-ws1501/decode_data/bin/Release/decode_data
