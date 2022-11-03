# Petri-net-parser

Parses a Petri net and runs it

## Build

Clone the repository and make sure you also initialize the submodules.

```bash
mkdir build
cd build 
cmake .. -DBUILD_TESTING=0 -DBUILD_EXAMPLES=0 -DCMAKE_INSTALL_PREFIX:PATH=../install
cmake .. -DBUILD_TESTING=1 -DBUILD_EXAMPLES=0 -DCMAKE_INSTALL_PREFIX:PATH=../install
cmake .. -DBUILD_TESTING=1 -DBUILD_EXAMPLES=1 -DCMAKE_INSTALL_PREFIX:PATH=../install
make install
```

## Run

```
cd ../install
./Symmetri_flight ../nets/PT1.pnml ../nets/PT2.pnml ../nets/PT3.pnml
# or
./Symmetri_hello_world ../nets/passive_n1.pnml ../nets/T50startP0.pnml
```

and look at `http://localhost:2222/` for a live view of the activity.


# WIP / TODO

- research transition guards/coloured nets
- single webiface server
- replace maps with vectors (?)

https://www.youtube.com/watch?v=2KGkcGtGVM4
https://stlab.cc/tip/2017/12/23/small-object-optimizations.html

# Cloc

https://www.youtube.com/watch?v=2KGkcGtGVM4
https://stlab.cc/tip/2017/12/23/small-object-optimizations.html
```
cloc --exclude-list-file=.clocignore .
```
