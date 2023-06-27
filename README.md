# c_banks_flightsim

Modernization of Carl Bank's 1998 flightsim

TL;DR

* `gcc -ansi banks.c -lm -lX11 -o banks`
* `cat horizon.scene pittsburgh.scene | ./banks`

Yes, it compiles and runs(!)

Ubuntu/Debian users:

`sudo apt install libx11-dev`

## Where this came from

Originally Banks wrote this in 1998

At some point, possibly in March 2017 (19 years later) a user on linuxgamecast.com attempted to deobfuscate the code, along with comments, along with a comment "Moving this to its own thread so a Google search can find it. Google never indexed my original post; maybe it will index this thread title.". Well, I found it via google search, so good job sir.

I've attempted to clean that user's significant work up further, and splat it out on github for the world to better discover. Both the original code (free of copyright, by the author's own proclamation in his blog post), and the De-obfuscation are interesting both as a historical note, and perhaps more importantly, as an excellent study of bare-bones 3D space simulators, effective methods of projecting points of 3D space against a flat screen, and a whole lot of other hard problems that you're not going to immediately understand, trying to decipher something like the Quake 1 source code.

I've cleaned up the code as-found, and it compiles and runs now the same as the obfuscated code.

## What does it look like?

Remarkably good given it was originally written in 58 LOC. Here are some screenshots. Two are of downtown Pittsburgh, one of the Egyptian Pyramids, and a fourth of a river.

![screenshot_1](https://github.com/hadlock/c_banks_flightsim/blob/master/static/banks.png) ![screenshot_1](https://github.com/hadlock/c_banks_flightsim/blob/master/static/banks2.png)
![screenshot_1](https://github.com/hadlock/c_banks_flightsim/blob/master/static/banks3.png) ![screenshot_1](https://github.com/hadlock/c_banks_flightsim/blob/master/static/banks4.png)

## What happened to the original code?

Well I cleaned that up too, within reason; it's in the `ioccc98` folder. Note you'll need

1. linux
2. x11 windowing system

Unfortunately, X11 isn't very actively developed anymore, and there's a non-zero chance you're running Wayland

The good news is 22 years later, if you have X11 installed, it ought to run just fine. There is plenty of cleaned-up documentation in that folder.

## 2017 notes

De-obfuscating the Banks flight simulator from IOCCC 1998. Not finished,
I haven’t tracked down all the math though author has mentioned he used
orthogonal matrices.

Stdin is lists of 3d point coordinates. Each list is an object
drawn connect-a-dot and terminated with 0 0 0. Z upward is negative.

### Compile

`gcc -ansi banks.c -lm -lX11 -o banks`

### Run

`cat horizon.scene pittsburgh.scene | ./banks`

Get the .scene data files from the IOCCC site.

<http://www0.us.ioccc.org/years.html#1998>

### Controls

Arrow keys are the flight stick.
Enter re-centers stick left-right, but not forward-back.
PageUp, PageDn = throttle

HUD on bottom-left:
speed, heading (0 = North), altitude

### Math note

Angles
There are 3 angles in the math. Your compass heading, your front-back tilt, and your sideways tilt = Tait-Bryan angles typical in aerospace math. Also called yaw, pitch, and roll. The Z axis is negative upward. That’s called left-handed coordinates. The rotation matrix assumes that.

The rotation matrix is not shown in final form in the wiki article I cite, so let’s derive it:

`cx = cos(x) and so on. x=sideTilt, y=forwardTilt, z=compass`

```shell
1 0 0 cy 0 -sy cz sz 0

0 cx sx * 0 1 0 * -sz cz 0

0 -sx cx sy 0 cy 0 0 1

cy 0 -sy cz sz 0

sx*sy cx sx*cy * -sz cz 0

cx*sy -sx cx*cy 0 0 1

cy*cz cy*sz -sy

sx*sy*cz-cx*sz sx*sy*sz+cx*cz sx*cy

cx*sy*cz+sx*sz cx*sy*sz-sx*cz cx*cy
```

It is shown in section 2.2.1 “Euler Angles” of banks1 references:

wiki1 = en.wikipedia.org/wiki/Perspective_transform#Perspective_projection
wiki2 = <https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix>
banks1 = “A DISCUSSION OF METHODS OF REAL-TIME AIRPLANE FLIGHT SIMULATION”
<http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.510.7499&rep=rep1&type=pdf>

### Obfuscation notes

The original program has some tricky syntax.

Forms like `-*(DN -N? N-DT ?N== RT?&u: & W:&h:&J )` are nested x ? y :z that return the address of a variable thatgets de-referenced by the * and finally decremented.

A comma like in `c += (I = M / l, l * H + I * M + a * X) * _;` makes a sequence of statements. The last one is the value of the `(…)` list.
