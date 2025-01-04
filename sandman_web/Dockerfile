FROM python:3.11.11-slim-bookworm

WORKDIR /usr/src/app

# These environment variables prevent outputting .pyc files and buffering input and output 
# respectively.
ENV PYTHONDONTWRITEBYTECODE 1
ENV PYTHONUNBUFFERED 1

# We need to install all the required Python modules.
RUN pip install --upgrade pip
COPY ./requirements.txt /usr/src/app/requirements.txt
RUN pip install -r requirements.txt

COPY . /usr/src/app
