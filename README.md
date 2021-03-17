# Petri-net-parser
Parses a Petri net and runs it

```
mkdir build
cd && build && cmake .. && make install
cd ../install
./mpalg_app ../nets/big7.json
```
outputs somehting like

```
18:13 $ ./mpalg_app ../nets/big7.json 
initial maring: 1 1 1 1 0 0 0 0 0
interface online at http://localhost:2222/
```

and kick off by going to `http://localhost:2222/` and push the button.
