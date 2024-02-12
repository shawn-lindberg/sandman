import os

from flask import Flask,request,redirect

def create_app(test_config = None):
    """Creates and configures the app.

    test_config - Which testing configuration to use, if any.
    """

    app = Flask(__name__, instance_relative_config = True)
    app.config.from_mapping(
        SECRET_KEY='dev',
    )

    if test_config is None:
        # Load the instance configwhen not testing, if it exists.
        app.config.from_pyfile('config.py', silent = True)
    else:
        # Otherwise use the test config.
        app.config.from_mapping(test_config)

    # Make sure the instance folder exists.
    try:
        os.makedirs(app.instance_path)
    except OSError:
        pass

    # Sandman Reports Home Page
    @app.route('/')
    def home():
        return reports.index()

    # A path to redirect to the rhasspy admin home page.
    @app.route('/rhasspy')
    def rhasspy():
        server_ip = request.host.split(':')[0]
        return redirect("http://" + server_ip + ':12101')

    from . import reports
    app.register_blueprint(reports.blueprint)

    return app