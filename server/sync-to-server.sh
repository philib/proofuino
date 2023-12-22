#/bin/sh

rsync -av --exclude=script.sh * pi@proofuino-logger.local:/home/pi/server
ssh pi@proofuino-logger.local 'sudo docker container restart grafana-container'
