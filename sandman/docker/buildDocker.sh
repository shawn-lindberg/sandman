
set -eux

cp /usr/local/bin/sandman .
docker build -t "sandman_docker:v1.0" .

