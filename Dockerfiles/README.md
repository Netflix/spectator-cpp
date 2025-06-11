# Docker Build :hammer_and_wrench:

The `spectator-cpp` project also supports a platform-agnostic build. The only prerequisite is the 
installation of `Docker`. Once `Docker` is installed, you can build the project by running the
following commands from the root directory of the project.

## Linux & Mac :penguin:

##### Warning:

- Do not prepend the command with `sudo` on Mac
- Start `Docker` before opening terminal on Mac 

```shell
sudo docker build -t spectator-cpp-image -f Dockerfiles/Ubuntu.Dockerfile .
sudo docker run -it spectator-cpp-image
./build.sh
```

## Windows

##### Warning:

- Start `Docker` before opening `Powershell`

```shell
docker build -t spectator-cpp-image -f Dockerfiles/Ubuntu.Dockerfile .
docker run -it spectator-cpp-image /bin/bash
apt-get install dos2unix
dos2unix build.sh
./build.sh
```