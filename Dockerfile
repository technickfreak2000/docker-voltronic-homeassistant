# Uncomment if building locally...
# FROM debian:stretch

FROM --platform=${TARGETPLATFORM:-linux/amd64} debian:stretch

ARG BUILD_DATE
ARG VERSION
ARG VCS_REF
ARG TARGETPLATFORM

LABEL org.label-schema.build-date=$BUILD_DATE \
  org.label-schema.version=$VERSION \
  org.label-schema.vcs-ref=$VCS_REF \

RUN apt update && apt install -y \
        curl \
        git \
        build-essential \
        cmake \
        jq \
        mosquitto-clients

ADD sources/ /opt/
ADD config/ /etc/inverter/

RUN cd /opt/inverter-cli && \
    mkdir bin && cmake . && make && mv inverter_poller bin/

WORKDIR /opt
ENTRYPOINT ["/bin/bash", "/opt/inverter-mqtt/entrypoint.sh"]