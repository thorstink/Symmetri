# Symmetri

A C++ library that parses petri nets and *executes* them by tying *functions* to *transitions*.

```cpp
using namespace symmetri;
StateNet net = {{"t0", {{"Pa", "Pb"}, {"Pc"}}},
                {"t1", {{"Pc", "Pc"}, {"Pb", "Pb", "Pd"}}}};
Store store = {{"t0", &foo}, {"t1", &bar}};
std::vector<std::pair<symmetri::Transition, int8_t>> priority = {};
NetMarking m0 = {{"Pa", 4}, {"Pb", 2}, {"Pc", 0}, {"Pd", 0}};
StoppablePool stp(1);
symmetri::Application app(net, m0, {}, store, priority,
                          "test_net_without_end", stp);
auto [ev, res] = app(); // run untill done.
stp.stop();
```

## Build

Clone the repository and make sure you also initialize the submodules.

```bash
mkdir build
cd build
cmake .. -DBUILD_TESTING=0 -DBUILD_EXAMPLES=0
cmake .. -DBUILD_TESTING=1 -DBUILD_EXAMPLES=0
cmake .. -DBUILD_TESTING=1 -DBUILD_EXAMPLES=1
```

## Run

```
cd ../build
./examples/flight/symmetri_flight ../nets/PT1.pnml ../nets/PT2.pnml ../nets/PT3.pnml
# or
./Symmetri_hello_world ../nets/passive_n1.pnml ../nets/T50startP0.pnml
```

and look at `http://localhost:2222/` for a live view of the activity.


# WIP / TODO

- research transition guards/coloured nets

https://www.youtube.com/watch?v=2KGkcGtGVM4
https://stlab.cc/tip/2017/12/23/small-object-optimizations.html

# Cloc

https://www.youtube.com/watch?v=2KGkcGtGVM4
https://stlab.cc/tip/2017/12/23/small-object-optimizations.html
```
cloc --exclude-list-file=.clocignore .
```
