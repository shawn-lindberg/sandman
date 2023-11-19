import os

from flask import Flask

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

    # A simple page that says hello.
    @app.route('/hello')
    def hello():
        return 'Hello, World!'

    from . import reports
    app.register_blueprint(reports.blueprint)

    return app