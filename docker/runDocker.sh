
# adding another bind mount for sandman log
# network host allows use of all ports - https://docs.docker.com/network/drivers/host/
docker run --privileged -it \
   --network host \
   --expose 12183 \
   --mount type=bind,src=/lib/libpigpio.so.1,target=/lib/libpigpio.so.1 \
   --mount type=bind,src=/usr/local/var/sandman,target=/usr/local/var/sandman \
   --mount type=bind,src=/usr/local/etc/sandman,target=/usr/local/etc/sandman \
   sandman_docker:v2 

# this isn't working, docker ps -a shows container exited
#docker run --privileged -d \
#   --network host \
#   --expose 12183 \
#   --mount type=bind,src=/lib/libpigpio.so.1,target=/lib/libpigpio.so.1 \
#   --mount type=bind,src=/usr/local/var/sandman,target=/usr/local/var/sandman \
#   --mount type=bind,src=/usr/local/etc/sandman,target=/usr/local/etc/sandman \
#   sandman_docker:v2 


# validate sandman log updated:

#2024/06/11 01:39:39 UTC] Dialogue session ended with ID: 3d719c37-cefc-4013-8785-e1d112d0ba80 and reason: nominal
#-rw-r--r-- 1 root root 2533 Jun 10 20:39 sandman.log
#/usr/local/var/sandman

