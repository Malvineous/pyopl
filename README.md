# PyOPL: OPL2/3 emulation for Python

- Copyright 2011-2012 Adam Nielsen <malvineous@shikadi.net>
- Copyright 2019 Adam Biser
- Copyright 2024 Laurence Dougal Myers <laurencedougalmyers@gmail.com>

http://www.github.com/Malvineous/pyopl

PyOPL is a simple wrapper around an OPL synthesiser so that it can be accessed
from within Python.  It uses the DOSBox synth, which has been released under
the GPL license.

PyOPL does not include any audio output mechanism, it simply converts register
and value pairs into PCM data.  The example programs use PyAudio for audio
output.  Note that PyGame is not suitable for this as it lacks a method for
streaming audio generated on-the-fly, and faking it by creating new Sound
objects is unreliable as they do not always queue correctly.

To install it:

```commandline
pip install PyOPL
```

The code is compiled like this:

```commandline
pip install build
python -m build
```

Install the build wheel:

```commandline
python -m pip install .
```

This library is released under the GPLv3 license.
