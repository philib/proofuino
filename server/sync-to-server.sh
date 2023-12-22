#/bin/sh

rsync -av --exclude=script.sh * pi@proofuino-logger.local:/home/pi/server
