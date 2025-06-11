# Use the official Ubuntu base image from Docker Hub
FROM ubuntu:latest

# Add a few required packages for building and developer tools
RUN apt-get update && apt-get install -y \
    vim \
    git \
    python3 \
    python3-venv \
    gcc-13\
    g++-13 \
    cmake \
    build-essential

# Create a default working directory
WORKDIR /home/ubuntu/spectator-cpp

# Copy all files & folders in the projects root directory 
# Exclude files listed in the dockerignore file
COPY ../ /home/ubuntu/spectator-cpp

# Setup Python virtual environment using the existing script
RUN chmod +x setup-venv.sh && ./setup-venv.sh

# When container starts, activate the virtual environment
ENTRYPOINT ["/bin/bash", "-c", "source venv/bin/activate && exec /bin/bash"]