#!/bin/bash
# test d'automatisation pour la recuperation de la temperature humidite
cd /tmp;


rtl_sdr -f 868428000 -s 250000 data.raw -g 49.6 -S 2>/dev/null &
sleep 40;
killall rtl_sdr  1>/dev/null &
sleep 1;
echo "Analyse du fichier"
/home/bengo/Outils/SDR/rtl_sdr-ws1501/decode_data/bin/Release/decode_data
