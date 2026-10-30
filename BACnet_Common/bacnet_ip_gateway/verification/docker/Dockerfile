FROM python:3.10-slim

# Install system dependencies, Java (JRE), Node.js, and Allure CLI
RUN apt-get update && apt-get install -y --no-install-recommends \
    procps \
    iproute2 \
    net-tools \
    build-essential \
    libjsoncpp-dev \
    default-jre-headless \
    nodejs \
    npm \
    openssh-client \
    sshpass \
    && npm install -g allure-commandline \
    && rm -rf /var/lib/apt/lists/*

# Install python requirements
COPY verification/requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir -r /tmp/requirements.txt

WORKDIR /workspace/bacnet_ip_gateway

ENTRYPOINT ["python3", "verification/scripts/run_gateway_tcs.py"]

