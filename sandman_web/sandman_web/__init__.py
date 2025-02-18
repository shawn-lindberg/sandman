"""The web interface for interacting with Sandman."""

import logging
import logging.handlers
import os

from flask import Flask, redirect, request

# Create a formatter.
log_formatter = logging.Formatter(
    "[%(asctime)s] %(levelname)s in %(module)s: %(message)s"
)

# Add file handler to the root logger.
file_handler = logging.handlers.RotatingFileHandler(
    "flask_app.log", backupCount=10, maxBytes=1000000
)
file_handler.setLevel(logging.WARNING)
file_handler.setFormatter(log_formatter)

logger = logging.getLogger()
logger.addHandler(file_handler)


def create_app(test_config: dict[any] = None) -> Flask:
    """Create and configure the app.

    test_config - Which testing configuration to use, if any.
    """
    app = Flask(__name__, instance_relative_config=True)
    app.config.from_mapping(
        SECRET_KEY="dev",
    )

    if test_config is None:
        # Load the instance config when not testing, if it exists.
        app.config.from_pyfile("config.py", silent=True)
    else:
        # Otherwise use the test config.
        app.config.from_mapping(test_config)

    # Make sure the instance folder exists.
    try:
        os.makedirs(app.instance_path)
    except OSError:
        pass

    # Sandman Reports Home Page
    @app.route("/")
    def home() -> str:
        return reports.index()

    # A path to redirect to the rhasspy admin home page.
    @app.route("/rhasspy")
    def rhasspy() -> str:
        server_ip = request.host.split(":")[0]
        return redirect("http://" + server_ip + ":12101")

    # Register blueprints
    from .reports import reports

    app.register_blueprint(reports.blueprint)

    from .status import status

    app.register_blueprint(status.status_bp)

    # Create global status variable.
    @app.context_processor
    def status_processor() -> dict[any]:
        if status.is_healthy() == True:
            health_issue = False
        else:
            health_issue = True
        return dict(health_issue=health_issue)

    return app
