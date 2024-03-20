import psutil, docker, requests

from flask import (
    Blueprint, render_template
)
from werkzeug.exceptions import abort

#Check whether a Linux process is running based on process name
def is_process_running(process_name: str):
    for process in psutil.process_iter(['pid', 'name']):
        if process.info['name'] == process_name:
            return True
    return False

#Check that Sandman is running
def check_sandman_health():
        process_name = "sandman"
        if is_process_running(process_name):
            return True
        else:
            return False

#Check that Rhasspy is running and responding
#Get the Rhasspy contatiner status
def check_rhasspy_health():
    client = docker.DockerClient(base_url='unix://var/run/docker.sock')
    rhasspy_container = client.containers.get("rhasspy")
    rhasspy_container_status = rhasspy_container.attrs["State"]

    #Get the Rhasspy web response
    try:
        rhasspy_web_response = requests.get('http://localhost:12101')
        rhasspy_web_status = rhasspy_web_response.status_code
    except:
        rhasspy_web_status = False

    #Check that the Rhasspy container is running and the web response is OK
    try:
        if rhasspy_container_status["Status"] == "running" and rhasspy_web_status == 200:
            return True
        else:
            return False
    except:
        #The container may not exist
        return "Failed check"

#Check that ha-bridge is running and responding
#Get the ha-bridge contatiner status
def check_ha_bridge_health():
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
        ha_bridge_web_status = False

    #Check that the ha-bridge container is running and the web response is OK
    try:
        if ha_bridge_container_status["Status"] == "running" and ha_bridge_web_status == 200:
            return True
        else:
            return False
    except:
        #The container may not exist
        return "Failed check"

status_bp = Blueprint('status', __name__,template_folder='templates')

@status_bp.route('/status')
def status_home():

    #Perform the Sandman related health checks
    sandman_status_check = check_sandman_health()
    rhasspy_status_check = check_rhasspy_health()
    ha_bridge_status_check = check_ha_bridge_health()

    #Check that Sandman is in good health
    if sandman_status_check:
        sandman_status = "Sandman process is running. ✔️"
    else:
        sandman_status = "Sandman process is not running. ❌"

    #Check that Rhasspy is in good health
    if rhasspy_status_check:
        rhasspy_status = "Rhasspy is running. ✔️"
    elif rhasspy_status_check == "Failed check":
        rhasspy_status = "Rhasspy is not running. ❌"
        rhasspy_status += "The Rhasspy container may not exist."
    else:
        rhasspy_status = "Rhasspy is not running. ❌"

    #Check that ha-bridge is in good health
    if ha_bridge_status_check == True:
        ha_bridge_status = "ha-bridge is running. ✔️"
    elif ha_bridge_status_check == "Failed check":
        ha_bridge_status = "ha-bridge is not running. ❌"
        ha_bridge_status += "The ha-bridge container may not exist."
    else:
        ha_bridge_status = "ha-bridge is not running. ❌"
    
    return render_template("status.html",sandman_status = sandman_status, rhasspy_status = rhasspy_status, ha_bridge_status = ha_bridge_status)