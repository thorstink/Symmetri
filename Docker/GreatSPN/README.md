Use:

```
docker run --net=host --env="DISPLAY" --volume="$HOME/.Xauthority:/root/.Xauthority:rw" -v ~/Projects/petri-net-program/Systemetri/:/Systemetri greatspn:latest
```

if X11 issues, run:
```
xhost +
```

build:

```
docker build . -t greatspn
```
