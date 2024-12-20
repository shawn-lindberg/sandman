
set -eux

export SANDMAN_ROOT=/sandman/

docker run -d \
   --network host \
   --mount type=bind,src=/usr/local/var/sandman,target=/usr/local/var/sandman \
   --mount type=bind,src=/usr/local/etc/sandman,target=/usr/local/etc/sandman \
   --mount type=bind,src=/usr/lib/,target=/usr/lib/ \
   --mount type=bind,src=${HOME},target=${SANDMAN_ROOT} \
   --mount type=bind,src=/dev/gpiochip0,target=/dev/gpiochip0 \
   --device=/dev/gpiomem:/dev/gpiomem \
   --device=/dev/gpiochip0:/dev/gpiochip0 \
   sandman_docker:v2

