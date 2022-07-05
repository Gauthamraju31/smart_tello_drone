FROM python:3.8
RUN apt-get update && apt-get install libgl1-mesa-glx -y
COPY app /opt/tello
WORKDIR /opt/tello
COPY requirements.txt requirements.txt
RUN pip install -r requirements.txt
ENV PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION=python
