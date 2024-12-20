
set -eux

export SANDMAN_ROOT="/sandman/"

cp /usr/local/bin/sandman .
docker build -t "sandman_docker:v2" .

