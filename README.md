# Petri-net-parser
Parses a Petri net and runs it

```
mkdir build
cd && build && cmake .. && make install
cd ../install
./Symmetri_flight ../nets/PT1.pnml ../nets/PT2.pnml ../nets/PT3.pnml
# or
./Symmetri_hello_world ../nets/n1.pnml
```
outputs somehting like

```
11:18 $ ./Symmetri_hello_world ../nets/n1.pnml
[2022-02-22 11:23:36.835] [info] PNML file-path: ../nets/n1.pnml
[2022-02-22 11:23:36.836] [info] initial marking for total net: 1 1 1 1 0 0 0 0 0

[2022-02-22 11:23:36.838] [info] interface online at http://localhost:2222/
[2022-02-22 11:23:36.838573] [info] [thread 27227] [NOCASE] Transition t0 started.
[2022-02-22 11:23:37.838858] [info] [thread 27227] [NOCASE] Transition t0 ended.
[2022-02-22 11:23:37.839369] [info] [thread 27229] [NOCASE] Transition t4 started.
[2022-02-22 11:23:37.839395] [info] [thread 27228] [NOCASE] Transition t1 started.

```

and look at `http://localhost:2222/` for a live view of the activity.


# WIP / TODO

- Put in different eventloop?
- reinvestigate return types/codes/errors for transitions
- investige state = marking + active transitions?
- Write some tests
- Create piano
- research transition guards/coloured nets
- State hash?
# Cloc

```
cloc --exclude-list-file=.clocignore .
```
