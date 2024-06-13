
FROM arm64v8/debian:12

EXPOSE 12183

RUN apt update; \
    apt install libncurses6 libxml2-dev libmosquitto-dev -y; \
    apt install supervisor -y

ADD sandman /usr/local/bin/sandman

RUN echo '2024010-5' > /tmp/aag-version

ENTRYPOINT ["/usr/local/bin/sandman"]

