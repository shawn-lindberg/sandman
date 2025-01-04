from flask.cli import FlaskGroup

import sandman_web

app = sandman_web.create_app()
cli = FlaskGroup(app)

if __name__ == '__main__':
   cli()