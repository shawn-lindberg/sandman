"""Implements the reports list and individual reports webpages."""

import datetime
import json
import os
import pathlib
from operator import itemgetter

from flask import (
    Blueprint,
    render_template,
)
from werkzeug.exceptions import abort

_report_prefix = "sandman"
_report_extension = ".rpt"

# The date and time format for report events.
_report_date_time_format = "%Y/%m/%d %H:%M:%S %Z"


def _get_reports_path() -> str:
    return str(pathlib.Path.home()) + "/.sandman/reports"


blueprint = Blueprint(
    "reports",
    __name__,
    url_prefix="/reports",
    template_folder="templates",
    static_folder="static",
)


@blueprint.route("/")
def index() -> str:
    """Implement the page listing all of the reports."""
    reports_path = _get_reports_path()

    # Build a list of all the reports.
    reports = []

    for path in os.listdir(reports_path):
        base_name, extension = os.path.splitext(path)

        if extension != _report_extension:
            continue

        # We expect all of the reports to start with the same prefix, so
        # ignore any that don't have it.
        if base_name.startswith(_report_prefix) == False:
            continue

        date_string = base_name[len(_report_prefix) :]

        # Try to convert the name to a date.
        date_format = "%Y-%m-%d"

        try:
            date = datetime.datetime.strptime(date_string, date_format)

        except ValueError:
            continue

        # Add a dictionary containing the date.
        reports.append(
            {"year": date.year, "month": date.month, "day": date.day}
        )

    # Sort them in descending order.
    sorted_reports = sorted(
        reports, key=itemgetter("year", "month", "day"), reverse=True
    )

    return render_template("reports.html", reports=sorted_reports)


def _update_report_event_from_version_3(event: dict[any]) -> None:
    # Handle the schedule -> routine rename on on old data.
    if event["type"] == "schedule":
        event["type"] = "routine"

    if event["type"] == "control" and event["source"] == "schedule":
        event["source"] = "routine"


def _make_report_event_from_version_2(event: str) -> dict[any]:
    # Convert the event into the most likely sort of control event.
    control_action_parts = event.split(": ")
    control_name = control_action_parts[0]

    control_action = "stop"
    if control_action_parts[1] == "moving up":
        control_action = "move up"
    elif control_action_parts[1] == "moving down":
        control_action = "move down"

    converted_event = {
        "type": "control",
        "control": control_name,
        "action": control_action,
        "source": "command",
    }

    return converted_event


def _parse_report_file(filename: str) -> tuple[int, list[any]]:
    """Parse a report file.

    Returns a tuple containing the version as the first element (or None if
    there was an error). The second element of the tuple is a list of tuples.
    These list elements correspond to events from the report, where the first
    element is the date and time, and the second is the event information.
    """
    version = None
    infos = []

    try:
        with open(filename, encoding="utf-8") as report_file:
            # Process every line of the file.
            for line_index, line in enumerate(report_file):
                # Try to convert the line to JSON.
                try:
                    line_json = json.loads(line)

                except json.JSONDecodeError:
                    continue

                # The first line should be the header information.
                if line_index == 0:
                    version = line_json.get("version")

                    if version is None:
                        break

                else:
                    # Get the date and time and convert it to an object.
                    line_date_time = line_json.get("dateTime")

                    if line_date_time is None:
                        continue

                    try:
                        info_date_time = datetime.datetime.strptime(
                            line_date_time, _report_date_time_format
                        )

                    except ValueError:
                        continue

                    line_event = line_json.get("event")

                    if line_event is None:
                        line_event = "None"

                    if version == 3:
                        _update_report_event_from_version_3(line_event)

                    elif version == 2:
                        line_event = _make_report_event_from_version_2(
                            line_event
                        )

                    infos.append((info_date_time, line_event))

    except OSError:
        return (version, infos)

    return (version, infos)


@blueprint.route("/<int:year>/<int:month>/<int:day>/report")
def report(year: int, month: int, day: int) -> str:
    """Implement the page for a specific report."""
    reports_path = _get_reports_path()

    report_name = f"{year:04d}-{month:02d}-{day:02d}"
    report_filename = (
        reports_path + "/" + _report_prefix + report_name + _report_extension
    )

    # Try to generate a report start time to fall back on if we don't get one
    # from the file.
    date_format = "%Y-%m-%d"

    try:
        report_end_date = datetime.datetime.strptime(report_name, date_format)

    except ValueError:
        abort(404, "Oops!")

    # The start date is one day before.
    report_start_date_time = report_end_date + datetime.timedelta(
        days=-1, hours=17
    )

    # Attempt to parse the data in the file.
    report_version, report_infos = _parse_report_file(report_filename)

    if report_version is None:
        abort(404, "Oops!")

    # Now that we have pulled data out of the file, do some processing to
    # convert it to what we need for display. Part of that will be converting
    # to dictionaries but also calculating the offset from the start time.
    event_infos = []

    for date_time, event in report_infos:
        # Figure out how many seconds from the start this event is.
        seconds_from_start = int(
            (date_time - report_start_date_time).total_seconds()
        )

        event_info = {
            "year": date_time.year,
            "month": date_time.month,
            "day": date_time.day,
            "hour": date_time.hour,
            "minute": date_time.minute,
            "second": date_time.second,
            "secondsFromStart": seconds_from_start,
            "event": event,
        }

        event_infos.append(event_info)

    report_start_date_string = report_start_date_time.strftime("%Y-%m-%d")

    # Either generate these from the start time or based on the actual data
    # set in the future.
    start_hour = 17

    return render_template(
        "report.html",
        start_date_string=report_start_date_string,
        end_date_string=report_name,
        start_hour=start_hour,
        event_infos=event_infos,
    )
