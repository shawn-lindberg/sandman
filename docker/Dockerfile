
FROM arm64v8/debian:12

# gpio crash, might need to run as root - https://forum.xojo.com/t/rpi4b-pigpio-gpio-gpioinitialise-fails-why/55960/3
USER root

EXPOSE 12183
EXPOSE 8888

# psmisc -> for killall command
# libunwind8 -> maybe for gpio crash - https://forum.xojo.com/t/rpi4b-pigpio-gpio-gpioinitialise-fails-why/55960/3

RUN apt update; \
    apt install libncurses6 libxml2-dev libmosquitto-dev -y; \
    apt install sudo psmisc libunwind8 -y 

ADD sandman /usr/local/bin/sandman
ADD startup.sh /usr/local/bin/startup.sh

RUN echo '20240808-1' > /tmp/aag-version

#ENTRYPOINT ["/usr/local/bin/sandman"]
#ENTRYPOINT ["/usr/bin/bash", "/usr/local/bin/startup.sh"]

ENTRYPOINT ["/usr/bin/sudo", "/usr/local/bin/sandman", "--daemon"]



