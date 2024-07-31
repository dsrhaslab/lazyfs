# Download base image ubuntu 
FROM ubuntu:20.04

# Give default values for interactive prompts when installing packages
ENV DEBIAN_FRONTEND=noninteractive 

# Test variables
ARG application
ARG test_id
ARG app_version

ENV test_id=${test_id}
ENV script_arguments=""
ENV script=/test/${test_id}.sh

# Paths
ARG config_path=config.toml
ARG aux_path=aux.sh
ARG install_script=${application}/${application}-install.sh

WORKDIR /test

# Update and install dependendencies
RUN apt-get -y update &&\
    apt-get install -y git &&\
    apt-get install -y g++ cmake &&\
    apt-get install -y libfuse3-dev libfuse3-3 fuse3 &&\
    apt-get install -y wget

# Uncomment line user_allow_other on file /etc/fuse.conf
RUN sed -i 's/#user_allow_other/user_allow_other/' /etc/fuse.conf

#Copy the necessary files
COPY ${application}/${test_id} .
COPY ${config_path} .
COPY ${aux_path} .
COPY ${install_script} .

# Install and build LazyFS
RUN git clone https://github.com/kianda17/LazyFS-Branch lazyfs
RUN cd lazyfs && \
    cd libs/libpcache && \
    chmod +x build.sh && \
    ./build.sh && \
    cd ../../lazyfs && \
    chmod +x build.sh && \
    ./build.sh && \
    cd scripts && \
    chmod +x *.sh

# Make the script executable
RUN chmod +x *.sh

# Install
RUN ./${application}-install.sh ${app_version}

# Run the script when the container starts
ENTRYPOINT /bin/bash -c  "${script} ${script_arguments}"