# How to use this Dockerfile

To run a Licode Docker container you have two options: 

- You can build your own image using the Dockerfile we provide and then run the container from it or
- you can run the container directly from the image we provide in Docker Hub.

Both options require that you have [docker](https://docs.docker.com/installation/) installed on your machine.

## Build your own image and run the container from it

You have to download the [Licode's code](https://github.com/ging/licode) from GitHub and navigate to `docker` directory. There, to compile your own image just run:

	sudo docker build -t licode-image .


> **Note**
> If you do not want to have to use `sudo` in this or in the next section follow [these instructions](https://docs.docker.com/installation/ubuntulinux/#create-a-docker-group).

This builds a new Docker image following the steps in `Dockerfile` and saves it in your local Docker repository with the name `licode-image`. You can check the available images in your local repository using: 

	sudo docker images


> **Note**
> If you want to know more about images and the building process you can find it in [Docker's documentation](https://docs.docker.com/userguide/dockerimages/).

Now you can run a new container from the image you have just created with:

	sudo docker run -d --name licode -p 3001:3001 -p 8080:8080 -p 3000:3000 -p 30000-31000:30000-31000/udp -e "PUBLIC_IP=XXX.XXX.XXX.XXX" licode-image

Where the different params mean: 

* -d indicates that the container runs as a daemon
* --name is the name of the new container (you can use the name you want)
* -p stablishes a relation between local ports and a container's ports.
* PUBLIC_IP tells Licode the IP that is used to access the server from outside
* the last param is the name of the image

Once the container is running you can view the console logs using: 

	sudo docker logs -f licode


To stop the container:

	sudo docker stop licode



## Run the container from the last release in Docker Hub

You can also run the container from the [image we provide](https://hub.docker.com/r/lynckia/licode/) in Docker Hub. In this case you have only to execute the run command. But now the image name is lynckia/licode:*version* where `version` is the release you want to use:

	sudo docker run -d --name licode -p 3001:3001 -p 8080:8080 -p 3000:3000 -p 30000-31000:30000-31000/udp -e "PUBLIC_IP=XXX.XXX.XXX.XXX" lynckia/licode

> **Note**
> If you do not specify a version you are pulling from `latest` by default.
