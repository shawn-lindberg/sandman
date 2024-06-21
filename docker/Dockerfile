
FROM arm64v8/debian:12

# gpio crash, might need to run as root - https://forum.xojo.com/t/rpi4b-pigpio-gpio-gpioinitialise-fails-why/55960/3
USER root

EXPOSE 12183

# psmisc -> for killall command
# libunwind8 -> maybe for gpio crash - https://forum.xojo.com/t/rpi4b-pigpio-gpio-gpioinitialise-fails-why/55960/3

RUN apt update; \
    apt install libncurses6 libxml2-dev libmosquitto-dev -y; \
    apt install sudo psmisc libunwind8 -y 

ADD sandman /usr/local/bin/sandman
ADD startup.sh /usr/local/bin/startup.sh

RUN echo '20240620-14' > /tmp/aag-version

# skip entrypoint and test interactive for now
#ENTRYPOINT ["/usr/bin/sudo", "/usr/local/bin/sandman", "--daemon"]
ENTRYPOINT ["/usr/bin/bash", "/usr/local/bin/startup.sh"]



