# Symmetri

A C++-17 library that takes a Petri net and turns it into a program. This is done by mapping *[transitions](https://en.wikipedia.org/wiki/Petri_net#Petri_net_basics)* to *functions* and calling the functions for which their transition counterpart is *[fireable](https://en.wikipedia.org/wiki/Petri_net#Execution_semantics)*. Petri nets are a graphical language that naturally can model concurrent and distributed systems [wikipedia](https://en.wikipedia.org/wiki/Petri_net#Petri_net_basics).

```mermaid
    graph LR
    classDef inactive fill:#ffc0;
    classDef active fill:#0050cb;
    classDef fake_tok_place color:#ffc0;
    classDef place color:#00c0cb;

    B-->D[foo]:::inactive;
    Z(("#9679;")):::place-->A
    A:::inactive-->B(("#9679;" )):::fake_tok_place;
    A[bar]:::active-->C(("#9679;")):::fake_tok_place;
    C-->D;
    D-->Z
    D-->B;
```

```cpp
using namespace symmetri;
StateNet net = {{"foo", {{"B", "C"}, {"Z"}}},
                {"bar", {{"Z"}, {"B", "C"}}}};
Store store = {{"foo", &foo}, {"bar", &bar}};
std::vector<std::pair<symmetri::Transition, int8_t>> priority = {};
NetMarking m0 = {{"Z", 1}, {"B", 0}, {"C", 0}};
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
