icat - image cat
================
Outputs an image on a 256-color or 24-bit color enabled terminal with UTF-8 locale,
such as gnome-terminal, konsole or rxvt-unicode (urxvt).

Building
--------

Build requirements: icat depends on ffmpeg (libavutil, etc...).

To compile:

	make

Running
-------

Run icat with a local file:

	icat foo.mkv

or with multiple files:

	icat img1.jpg img2.jpg > newfile.txt

or, if `-` is used as as file name, it reads from standard input:

	curl -sL https://raw.github.com/atextor/icat/master/sample.png | icat -

The above commands results the following output in 256-color terminal, see [`sample.256-color.txt`](sample.256-color.txt) for the actual output:

![Output of sample.png](sample.256-color.png)

For terminals that support 24-bit color (such as Konsole and Yakuake, see [this document](https://gist.github.com/XVilka/8346728) for more information about terminals and their color support), this can be enabled using:

	icat -m 24bit sample.png


Author
------

icat was written by Andreas Textor <textor.andreas@googlemail.com>.
The sample icon is from the Nuvola icon theme by David Vignoni.


