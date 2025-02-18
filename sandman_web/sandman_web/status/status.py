"""Implements the status webpage."""

import enum

import docker
import psutil
import requests
from flask import Blueprint, render_template


class _HealthType(enum.Enum):
    RUNNING = 1
    NOT_RUNNING = 2
    NOT_FOUND = 3


def _is_process_running(process_name: str) -> bool:
    """Check whether a Linux process is running based on process name."""
    for process in psutil.process_iter(["pid", "name"]):
        if process.info["name"] == process_name:
            return True

    return False


def _check_sandman_health() -> _HealthType:
    """Check that Sandman is running.

    Returns a status code based on whether process is running.
    """
    process_name = "sandman"
    if _is_process_running(process_name):
        return _HealthType.RUNNING
    else:
        return _HealthType.NOT_RUNNING


def _check_rhasspy_health() -> _HealthType:
    """Check that Rhasspy is running and responding.

    Returns a status code based on container status and http request response.
    """
    # Get the Rhasspy contatiner status
    client = docker.DockerClient(base_url="unix://var/run/docker.sock")
    try:
        container = client.containers.get("rhasspy")
    except Exception:
        return _HealthType.NOT_FOUND
    else:
        container_status = container.attrs["State"]["Status"]

    # Get the Rhasspy web response
    try:
        web_response = requests.get("http://localhost:12101")
    except Exception:
        web_status = 404
    else:
        web_status = web_response.status_code

    # Check that the Rhasspy container is running and the web response is OK
    if container_status == "running" and web_status == 200:
        return _HealthType.RUNNING
    else:
        return _HealthType.NOT_RUNNING


def _check_ha_bridge_health() -> _HealthType:
    """Check that ha-bridge is running and responding.

    Returns a status code based on container status and http request response.
    """
    # Get the ha-bridge contatiner status
    client = docker.DockerClient(base_url="unix://var/run/docker.sock")
    try:
        container = client.containers.get("ha-bridge")
    except Exception:
        # ha-bridge may not be installed on host
        return _HealthType.NOT_FOUND
    else:
        container_status = container.attrs["State"]["Status"]

    # Get the ha-bridge web response
    try:
        web_response = requests.get("http://localhost:80")
    except Exception:
        web_status = 404
    else:
        web_status = web_response.status_code

    # Check that the ha-bridge container is running or the web response is OK
    if container_status == "running" or web_status == 200:
        return _HealthType.RUNNING
    else:
        return _HealthType.NOT_RUNNING


def is_healthy() -> bool:
    """Return whether the status is healthy overall."""
    sandman_health = _check_sandman_health()
    rhasspy_health = _check_rhasspy_health()

    if (
        sandman_health == _HealthType.RUNNING
        and rhasspy_health == _HealthType.RUNNING
    ):
        return True

    return False


status_bp = Blueprint("status", __name__, template_folder="templates")


@status_bp.route("/status")
def status_home() -> str:
    """Implement the route for the status page."""
    # Perform the Sandman related health checks.
    sandman_health = _check_sandman_health()
    rhasspy_health = _check_rhasspy_health()
    ha_bridge_health = _check_ha_bridge_health()

    # Check that Sandman is in good health.
    if sandman_health == _HealthType.RUNNING:
        sandman_status = "Sandman process is running. ✔️"
    else:
        sandman_status = "Sandman process is not running. ❌"

    # Check that Rhasspy is in good health.
    if rhasspy_health == _HealthType.RUNNING:
        rhasspy_status = "Rhasspy is running. ✔️"
    else:
        rhasspy_status = "Rhasspy is not running. ❌"
        if rhasspy_health == _HealthType.NOT_FOUND:
            rhasspy_status += "The Rhasspy container may not exist."

    # Check that ha-bridge is in good health
    if ha_bridge_health == _HealthType.RUNNING:
        ha_bridge_status = "ha-bridge is running. ✔️"
    else:
        ha_bridge_status = "ha-bridge is not running. ❌"
        if ha_bridge_health == _HealthType.NOT_FOUND:
            ha_bridge_status += (
                "ha-bridge may not be installed on the host as a container "
                "or is otherwise unresponsive."
            )

    return render_template(
        "status.html",
        sandman_status=sandman_status,
        rhasspy_status=rhasspy_status,
        ha_bridge_status=ha_bridge_status,
    )
