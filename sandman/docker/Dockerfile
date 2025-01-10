
FROM arm64v8/debian:12

ENV SANDMAN_ROOT=/sandman/

ADD sandman /usr/local/bin/sandman
RUN apt update && apt install libncurses-dev libmosquitto-dev libgpiod-dev -y

ENTRYPOINT ["/usr/local/bin/sandman", "--docker"]



