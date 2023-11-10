# Flight

This is a little application that parses three Petri nets and uses multiple ways of composing to demonstrate the capabilities. It also demonstrates how to implement custom behavior for *fire*, *pause* and *cancel* using a user supplied class `Foo` in `transitions.hpp`.

## installing

You can build and install to a local `install`-directory:

```bash
mkdir build
cd build
cmake -DBUILD_EXAMPLES=ON -DBUILD_TESTING=OFF ..
make
```

You can run it like this:

```bash
./build/examples/flight/symmetri_flight nets/PT1.pnml nets/PT2.pnml nets/PT3.pnml
```


You can interact with the application through simple keys followed by an [enter]

```bash
input options:
 [p] - pause
 [r] - resume
 [x] - exit
 ```
