"""
demo.py - PyOPL usage example.

Copyright (C) 2011-2012 Adam Nielsen <malvineous@shikadi.net>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""
from struct import *
import pyopl
import pyaudio
import sys
import os.path

# Playback frequency.  Try a different value if the output sounds stuttery.
freq = 44100

# How many bytes per sample (2 == 16-bit samples).  This is the only value
# currently implemented.
sample_size = 2

# How many channels to output (2 == stereo).  The OPL2 is mono, so in stereo
# mode the mono samples are copied to both channels.  Enabling OPL3 mode will
# switch the synth to true stereo (and in this case setting num_channels=1
# will just drop the right channel.)  It is done this way so that you can set
# num_channels=2 and output stereo data, and it doesn't matter whether the
# synth is in OPL2 or OPL3 mode - it will always work.
num_channels = 2

# How many samples to synthesise at a time.  Higher values will reduce CPU
# usage but increase lag.
synth_size = 512

# An OPL helper class which handles the delay between notes and buffering
class OPLStream:
	def __init__(self, freq, ticksPerSecond):
		self.opl = pyopl.opl(freq, sampleSize=sample_size, channels=num_channels)
		self.ticksPerSecond = ticksPerSecond
		self.buf = bytearray(synth_size * sample_size * num_channels)
		# Python 3 doesn't have Python 2's buffer() builtin and _portaudio doesn't work with memoryview,
		# convert to bytes() as needed, which isn't as efficient.
		OPLStream.pyaudio_buf = property(lambda self: bytes(self.buf))
		self.delay = 0

	def writeReg(self, reg, value):
		self.opl.writeReg(reg, value)

	def wait(self, ticks):
		# Rather than calculating the exact number of samples we need to generate,
		# we just keep generating 512 samples at a time until we've waited as close
		# as possible to the requested delay.
		# This does mean we might wait for up to 511/freq samples too little (at
		# 48kHz that's a worst-case of 10.6ms too short) but nobody should notice
		# and it saves enough CPU time and complexity to be worthwhile.
		self.delay += ticks * freq / self.ticksPerSecond
		while self.delay > synth_size:
			self.opl.getSamples(self.buf)
			# We put the samples into self.buf which also updates self.pyaudio_buf
			stream.write(self.pyaudio_buf)
			self.delay -= synth_size

	# This is an alternate way of calculating the delay.  It has slightly higher
	# CPU usage but provides more accurate delays (+/- 0.04ms).
	# To use it, rename the function to "wait" and rename the other "wait"
	# function to something else.
	def wait2(self, ticks):
		# Figure out how many samples we need to get to obtain the delay
		fill = ticks * freq // self.ticksPerSecond
		tail = fill % synth_size
		if tail:
			buf_tail = bytearray(tail * sample_size * num_channels)
		# Fill the buffer in 512-sample lots until full
		cur = self.buf
		while fill > 1: # DOSBox synth can't generate < 2 samples
			if fill < synth_size:
				# Resize the buffer for the last bit
				cur = buf_tail
			self.opl.getSamples(cur)
			# Python 3 doesn't have Python 2's buffer() builtin and _portaudio doesn't work with memoryview,
			# convert to bytes() as needed, which isn't as efficient.
			pyaudio_buf = bytes(cur)
			stream.write(pyaudio_buf)
			fill -= synth_size


## Main code begins ##

# Require a filename to be given on the command line
if len(sys.argv) != 2:
	print("Please specify one IMF filename.")
	sys.exit()

# Open the file given on the command line
i = open(sys.argv[1], 'rb')

# Check the first two bytes to see what the file version is
chunk = i.read(2)
lenData, = unpack('H', chunk)
if lenData == 0:
	print("Type-0 format detected")
	i.seek(0)
else:
	print("Type-1 format detected")

# Check the file extension to see how fast we should play it
dummy, ext = os.path.splitext(sys.argv[1])
if ext.lower() == '.wlf': ticksPerSecond = 700
else: ticksPerSecond = 560
print("{} Hz file detected".format(ticksPerSecond))

# Set up the audio stream
audio = pyaudio.PyAudio()
stream = audio.open(
	format = audio.get_format_from_width(sample_size),
	channels = num_channels,
	rate = freq,
	output = True)

# At this point we have to hope PyAudio has got us the audio format we
# requested.  It doesn't always, but it lacks functions for us to check.
# This means we could end up outputting data in the wrong format...

# Set up the OPL synth
oplStream = OPLStream(freq, ticksPerSecond)

# Enable Wavesel on OPL2
oplStream.writeReg(1, 32)

# Enable OPL3 mode (this will likely mute the output, see below)
#oplStream.writeReg(0x105, 1)

lenRead = 0
channelActive = [0] * 9
core = []
perc = []
try:
	while lenRead < lenData or lenData == 0:

		# Read the next OPL bytes from the file
		chunk = i.read(4)
		lenRead += 4
		if not chunk: break
		reg, val, delay = unpack('BBH', chunk)

		# Send them to the synth
		oplStream.writeReg(reg, val)

		## EXAMPLE: Write to upper half of OPL3
		## First, override the panning register to make the sound come out of both
		## channels.  This is required if playing OPL2 data, as these registers are
		## unused on the OPL2 so are usually set to zero, silencing the channel.
		## You won't need this if you're playing native OPL3 data.
		#if reg & 0xC0 == 0xC0:
		#	val = val | 0x30
		## Write the same value to the same register on the upper register set
		#oplStream.writeReg(0x100 + reg, val)

		# Show something that moves to be a bit more interesting
		if reg & 0xB0 == 0xB0:
			c = reg & 0x0F
			if c < 9:
				channelActive[c] = val & 0x20
				core = ["\r"]
				for x in channelActive:
					if x: core.append('-')
					else: core.append('_')
			if reg == 0xBD and val & 0x20:
				perc = [' ']
				for x in range(0, 5):
					if (val >> x) & 1: perc.append('=')
					else: perc.append('.')
			sys.stdout.write(''.join(core))
			sys.stdout.write(''.join(perc))
			sys.stdout.flush()

		# Wait for the given number of ticks
		if delay: oplStream.wait(delay)

except EOFError:
	pass
except KeyboardInterrupt:
	pass

print

stream.close()
audio.terminate()
