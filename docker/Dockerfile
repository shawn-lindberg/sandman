
FROM arm64v8/debian:12

EXPOSE 12183

RUN apt update; \
    apt install libncurses6 libxml2-dev libmosquitto-dev -y; \
    apt install sudo -y

ADD sandman /usr/local/bin/sandman

RUN echo '20240620-5' > /tmp/aag-version

ENTRYPOINT ["/usr/bin/sudo", "/usr/local/bin/sandman", "--daemon"]

