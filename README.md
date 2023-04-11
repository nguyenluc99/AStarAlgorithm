# AStarAlgorithm
A visualization version of A* algorithm finding shortest path between two nodes.

## Prerequisite: SDL2 (http://www.libsdl.org):
#### Installation:
- Linux:

```
    apt-get install libsdl2-dev
```
- Mac OS X:
```
    brew install sdl2
```
## How to run:

Go to project location:
```
cd ImGuiProject/src/AStarAlgorithm
```

Build using `make`
```
make clean && make
````

Run the program:
```
./AStarAlgorithm
```

## Usage note:
<li> Please use "Random Grid" button to initialize a grid with height and width (number of row and number of column on each row)
<li> If you need an empty grid with just SOURCE and TARGET, just leave the "Blocked ratio" be zero.
<li> Speed of the algorithm can be changed in real-time during execution using the slider "Steps/sec".
<li> If you have any question, please open an issue or email me :) .
