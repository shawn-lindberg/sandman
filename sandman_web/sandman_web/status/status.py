import psutil, docker, requests

from flask import (
    Blueprint, render_template
)
from werkzeug.exceptions import abort

#Check whether a Linux process is running based on process name
def is_process_running(process_name: str) -> bool:
    """Returns bool based on whether named process is running."""
    for process in psutil.process_iter(['pid', 'name']):
        if process.info['name'] == process_name:
            return True
    return False

#Check that Sandman is running
def check_sandman_health() -> int:
        """Returns a status code based on whether process is running."""
        process_name = "sandman"
        if is_process_running(process_name):
            return 0
        else:
            return 1

#Check that Rhasspy is running and responding
def check_rhasspy_health() -> int | str:
    """Returns a status code or string based on container status and http request response."""

    #Get the Rhasspy contatiner status
    client = docker.DockerClient(base_url='unix://var/run/docker.sock')
    try:
        rhasspy_container = client.containers.get("rhasspy")
        rhasspy_container_status = rhasspy_container.attrs["State"]
    except:
        rhasspy_container_status = False
        
    #Get the Rhasspy web response
    try:
        rhasspy_web_response = requests.get('http://localhost:12101')
        rhasspy_web_status = rhasspy_web_response.status_code
    except:
        rhasspy_web_status = 404

    #Check that the Rhasspy container is running and the web response is OK
    try:
        if rhasspy_container_status["Status"] == "running" and rhasspy_web_status == 200:
            return 0
        else:
            return 1
    except:
        #The container may not exist
        return "Failed check"

#Check that ha-bridge is running and responding
def check_ha_bridge_health() -> int | str:
    """Returns a status code or string based on container status and http request response."""

    #Get the ha-bridge contatiner status
    client = docker.DockerClient(base_url='unix://var/run/docker.sock')
    try:
        ha_bridge_container = client.containers.get("ha-bridge")
        ha_bridge_container_status = ha_bridge_container.attrs["State"]
    except:
        ha_bridge_container_status = False

    #Get the ha-bridge web response
    try:
        ha_bridge_web_response = requests.get('http://localhost:80')
        ha_bridge_web_status = ha_bridge_web_response.status_code
    except:
        ha_bridge_web_status = 404

    #Check that the ha-bridge container is running or the web response is OK
    try:
        if ha_bridge_container_status["Status"] == "running" or ha_bridge_web_status == 200:
            return 0
        else:
            return 1
    except:
        #ha-bridge may not be installed on host
        return "Failed check"

status_bp = Blueprint('status', __name__,template_folder='templates')

@status_bp.route('/status')
def status_home():

    #Perform the Sandman related health checks
    sandman_status_check = check_sandman_health()
    rhasspy_status_check = check_rhasspy_health()
    ha_bridge_status_check = check_ha_bridge_health()

    #Check that Sandman is in good health
    if sandman_status_check == 0:
        sandman_status = "Sandman process is running. ✔️"
    else:
        sandman_status = "Sandman process is not running. ❌"

    #Check that Rhasspy is in good health
    if rhasspy_status_check == 0:
        rhasspy_status = "Rhasspy is running. ✔️"
    elif rhasspy_status_check == "Failed check":
        rhasspy_status = "Rhasspy is not running. ❌"
        rhasspy_status += "The Rhasspy container may not exist."
    else:
        rhasspy_status = "Rhasspy is not running. ❌"

    #Check that ha-bridge is in good health
    if ha_bridge_status_check == 0:
        ha_bridge_status = "ha-bridge is running. ✔️"
    elif ha_bridge_status_check == "Failed check":
        ha_bridge_status = "ha-bridge is not running. ❌"
        ha_bridge_status += "ha-bridge may not be installed on the host as a container and or is otherwise unresponsive."
    else:
        ha_bridge_status = "ha-bridge is not running. ❌"
    
    return render_template("status.html",sandman_status = sandman_status, rhasspy_status = rhasspy_status, ha_bridge_status = ha_bridge_status)