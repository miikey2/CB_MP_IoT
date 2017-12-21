#!/bin/bash

lastTemp=25
lastHumidity=70
lastLight=30
lastMoisture=70

while true; do
	for ((i = 1; i < 6; i+=1))
	do
	Rando=$((RANDOM%4))
	addOrMinus=$((RANDOM%2))
		if(( $i == 1))
		then
			Rando=$((RANDOM%2))
			mosquitto_pub -h 'brain.engineering' -u 'cbraines' -P "Tp:5tF'<5dc_k@;<" -t "/f/pir-level" -m $Rando
		elif(($i == 2))
		then
			if(($addOrMinus == 1))
			then
			lastTemp=$((lastTemp+Rando))
			else
			lastTemp=$((lastTemp-Rando))
			fi
			if(($lastTemp < 1))
			then
			lastTemp=0
			elif(($lastTemp > 99))
			then
			lastTemp=1
			fi
		mosquitto_pub -h 'brain.engineering' -u 'cbraines' -P "Tp:5tF'<5dc_k@;<" -t "/f/temp-level" -m $lastTemp
		elif(($i == 3))
		then
			if(($addOrMinus == 1))
			then
			lastHumidity=$((lastHumidity+Rando))
			else
			lastHumidity=$((lastHumidity-Rando))
			fi
			if(($lastHumidity < 1))
			then
			lastHumidity=0
			elif(($lastHumidity > 99))
			then
			lastHumidity=1
			fi
		mosquitto_pub -h 'brain.engineering' -u 'cbraines' -P "Tp:5tF'<5dc_k@;<" -t "/f/humidity-level" -m $lastHumidity
		elif(($i == 4))
		then
			if(($addOrMinus == 1))
			then
			lastMoisture=$((lastMoisture+Rando))
			else
			lastMoisture=$((lastMoisture-Rando))
			fi
			if(($lastMoisture < 1))
			then
			lastMoisture=0
			elif(($lastMoisture > 99))
			then
			lastMoisture=1
			fi
		mosquitto_pub -h 'brain.engineering' -u 'cbraines' -P "Tp:5tF'<5dc_k@;<" -t "/f/moisture-level" -m $lastMoisture
		elif(($i == 5))
		then
			if(($addOrMinus == 1))
			then
			lastLight=$((lastLight+Rando))
			else
			lastLight=$((lastLight-Rando))
			fi
			if(($lastLight < 1))
			then
			lastLight=0
			elif(($lastLight > 99))
			then
			lastLight=1
			fi
		mosquitto_pub -h 'brain.engineering' -u 'cbraines' -P "Tp:5tF'<5dc_k@;<" -t "/f/light-level" -m $lastLight
		fi
	sleep 0.2	
	done
done
