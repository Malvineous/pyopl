"""
demo.py - PyOPL usage example.

Copyright (C) 2011 Adam Nielsen <malvineous@shikadi.net>

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

# Playback frequency
freq = 48000

# An OPL helper class which handles the delay between notes and buffering
class OPLStream:
	def __init__(self, freq, ticksPerSecond):
		self.opl = pyopl.opl(freq, sampleSize=2)
		self.ticksPerSecond = ticksPerSecond
		self.buf = bytearray(512 * 2)

	def writeReg(self, reg, value):
		self.opl.writeReg(reg, value)

	def wait(self, ticks):
		# Figure out how many samples we need to get to obtain the delay
		fill = ticks * freq / self.ticksPerSecond
		tail = fill % 512
		if tail:
			buf_tail = bytearray(tail * 2)
		# Fill the buffer in 512-sample lots until full
		cur = self.buf
		while fill > 0:
			if fill < 512:
				# Resize the buffer for the last bit
				cur = buf_tail
			self.opl.getSamples(cur)
			pyaudio_buf = buffer(cur)
			stream.write(pyaudio_buf)
			fill -= 512

## Main code begins ##

# Require a filename to be given on the command line
if len(sys.argv) != 2:
	print "Please specify one IMF filename."
	sys.exit()

# Open the file given on the command line
i = open(sys.argv[1], 'rb')

# Check the first two bytes to see what the file version is
chunk = i.read(2)
lenData, = unpack('H', chunk)
if lenData == 0:
	print "Type-0 format detected"
	i.seek(0)
else:
	print "Type-1 format detected"

# Check the file extension to see how fast we should play it
dummy, ext = os.path.splitext(sys.argv[1])
if ext.lower() == '.wlf': ticksPerSecond = 700
else: ticksPerSecond = 560
print ticksPerSecond, "Hz file detected"

# Set up the audio stream
audio = pyaudio.PyAudio()
stream = audio.open(
	format = audio.get_format_from_width(2),
	channels = 1,
	rate = freq,
	output = True)

# Set up the OPL synth
oplStream = OPLStream(freq, ticksPerSecond)

# Switch to OPL3 mode.  Comment this out to hear the songs in OPL2 mode.
oplStream.writeReg(1, 32)

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

		# Show something that moves to be a bit more interesting
		if reg & 0xB0:
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
