import datetime
import json
import os

from operator import itemgetter

from flask import (
    Blueprint, flash, g, redirect, render_template, request, session, url_for
)
from werkzeug.exceptions import abort

# We need to know where to find the reports.
reports_path = '/usr/local/var/sandman/reports'
report_prefix = 'sandman'
report_extension = '.rpt'

# The date and time format for report events.
report_date_time_format = '%Y/%m/%d %H:%M:%S %Z'

blueprint = Blueprint('reports', __name__, url_prefix = '/reports')

@blueprint.route('/')
def index():

    # Build a list of all the reports.
    reports = []

    for path in os.listdir(reports_path):

        base_name, extension = os.path.splitext(path)
        
        if extension != report_extension:
            continue

        # We expect all of the reports to start with the same prefix, so ignore any that don't have 
        # it.
        if base_name.startswith(report_prefix) == False:
            continue

        date_string = base_name[len(report_prefix):]

        # Try to convert the name to a date.
        date_format = '%Y-%m-%d'

        try:
            date = datetime.datetime.strptime(date_string, date_format)

        except ValueError:
            continue

        # Add a dictionary containing the date.
        reports.append({'year' : date.year, 'month' : date.month, 'day' : date.day})
    
    # Sort them in descending order.
    sorted_reports = sorted(reports, key = itemgetter('year', 'month', 'day'), reverse = True)

    return render_template('reports/reports.html', reports = sorted_reports)

@blueprint.route('/<int:year>/<int:month>/<int:day>/report')
def report(year, month, day):

    report_name = '{year:04d}-{month:02d}-{day:02d}'.format(**vars())
    report_filename = reports_path + '/' + report_prefix + report_name + report_extension

    # Try to generate a report start time to fall back on if we don't get one from the file.
    date_format = '%Y-%m-%d'

    try:
        report_end_date = datetime.datetime.strptime(report_name, date_format)

    except ValueError:
        abort(404, 'Oops!')

    # The start date is one day before.
    report_start_date_time = report_end_date + datetime.timedelta(days = -1, hours = 17)
 
    # Attempt to open the file so that we can get the data out of it.
    report_version = None
    report_infos = []

    try:
        report_file = open(report_filename, encoding="utf-8")

        # Process every line of the file.
        for line_index, line in enumerate(report_file):

            # Try to convert the line to JSON.
            try:
                line_json = json.loads(line)

            except JSONDecodeError:
                continue

            # The first line should be the header information.
            if line_index == 0:

                report_version = line_json.get('version')

                if report_version is None:
                    break

            else:

                # Get the date and time and convert it to an object.
                line_date_time = line_json.get('dateTime')

                if line_date_time is None:
                    continue

                try:
                    info_date_time = datetime.datetime.strptime(line_date_time, 
                        report_date_time_format)

                except ValueError:
                    continue

                line_event = line_json.get('event')

                if line_event is None:
                    line_event = 'None'

                if report_version == 2:
                    # Convert the event into the most likely sort of control event.
                    control_action_parts = line_event.split(': ')
                    control_name = control_action_parts[0]

                    control_action = 'stop'
                    if control_action_parts[1] == 'moving up':
                        control_action = 'move up'
                    elif control_action_parts[1] == 'moving down':
                        control_action = 'move down'

                    line_event = {'type' : 'control',
                                  'control' : control_name,
                                  'action' : control_action, 
                                  'source' : 'command'
                                  }

                report_infos.append((info_date_time, line_event))

        report_file.close()
    
    except OSError:
        abort(404, 'Oops!')

    # Now that we have pulled data out of the file, do some processing to convert it to what we 
    # need for display. Part of that will be converting to dictionaries but also calculating the 
    # offset from the start time.
    event_infos = []

    for date_time, event in report_infos:

        # Figure out how many seconds from the start this event is.
        seconds_from_start = int((date_time - report_start_date_time).total_seconds())
        
        event_info = {'year' : date_time.year, 
                      'month' : date_time.month,
                      'day' : date_time.day, 
                      'hour' : date_time.hour,
                      'minute' : date_time.minute,
                      'second' : date_time.second,
                      'secondsFromStart' : seconds_from_start,
                      'event' : event
                     }

        event_infos.append(event_info)

    report_start_date_string = report_start_date_time.strftime('%Y-%m-%d')

    # Either generate these from the start time or based on the actual data set in the future.
    start_hour = 17
    hour_range = 24

    return render_template('reports/report.html', start_date_string = report_start_date_string, 
        end_date_string = report_name, start_hour = start_hour, event_infos = event_infos)