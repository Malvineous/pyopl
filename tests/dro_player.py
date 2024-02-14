"""
DRO player code is adapted from DRO Trimmer by Laurence Dougal Myers.
The actual conversion to PCM samples might be inaccurate, but it's good enough to
perform a regression test.
"""
import array
from enum import Enum
import struct
import typing
import pyopl


def read_dro(file_name: str) -> "DROData":
	"""Adapted from DRO Trimmer by Laurence Dougal Myers"""
	with open(file_name, "rb") as drof:
		drof.read(8)  # header name
		drof.read(4)  # header version
		(
			length_pairs,  # unused
			length_ms,  # unused
			hardware_type,  # unused
			dro_format,  # unused
			compression,  # unused
			short_delay_code,
			long_delay_code,
			codemap_length,
		) = struct.unpack("<2L6B", drof.read(14))
		codemap = struct.unpack(str(codemap_length) + "B", drof.read(codemap_length))
		raw_data = array.array("B")
		raw_data.fromfile(drof, length_pairs * 2)
		return DROData(codemap, raw_data, short_delay_code, long_delay_code)


class DROInstructionType(Enum):
	REGISTER = 0
	DELAY_MS = 1


class DROData:
	def __init__(
			self,
			codemap: typing.Tuple[int, ...],
			raw_data: array.array,
			short_delay_code: int,
			long_delay_code: int
	) -> None:
		self._codemap = codemap
		self._raw_data = raw_data
		self._short_delay_code = short_delay_code
		self._long_delay_code = long_delay_code

	def __iter__(self) -> typing.Iterator[
		typing.Union[
			typing.Tuple["DROInstructionType.DELAY_MS", int],
			typing.Tuple["DROInstructionType.REGISTER", int, int]
		]
	]:
		iterator = iter(self._raw_data)
		try:
			while True:
				reg = next(iterator)
				val = next(iterator)
				if reg == self._short_delay_code:
					val += 1
					yield [DROInstructionType.DELAY_MS, val]
				elif reg == self._long_delay_code:
					val = (val + 1) << 8
					yield [DROInstructionType.DELAY_MS, val]
				else:
					bank = (reg & 0x80) >> 7
					reg = self._codemap[reg & 0x7F]
					yield [DROInstructionType.REGISTER, bank, reg, val]
		except StopIteration:
			pass


class DROPlayer:
	def __init__(self, file_name: str) -> None:
		self._bit_depth = 16
		self._buffer_size = 512
		self._channels = 2
		self._buffer = self._create_bytearray(self._buffer_size)
		self._dro = read_dro(file_name)
		self._frequency = 49716
		self._opl: pyopl.opl = pyopl.opl(
			self._frequency,
			sampleSize=(self._bit_depth // 8),
			channels=self._channels,
		)
		self._sample_overflow = 0
		self._output = bytes()

	def _create_bytearray(self, size: int) -> bytearray:
		return bytearray(size * (self._bit_depth // 8) * self._channels)

	def _render_ms(self, length_ms: int) -> None:
		samples_to_render = length_ms * self._frequency / 1000.0
		self._render_samples(samples_to_render)

	def _render_samples(self, samples_to_render: float) -> None:
		samples_to_render += self._sample_overflow
		self.sample_overflow = samples_to_render % 1
		if samples_to_render < 2:
			# Limitation: needs a minimum of two samples.
			return
		samples_to_render = int(samples_to_render // 1)
		while samples_to_render > 1:
			if samples_to_render < self._buffer_size:
				tmp_buffer = self._create_bytearray(
					(samples_to_render % self._buffer_size)
				)
				samples_to_render = 0
			else:
				tmp_buffer = self._buffer
				samples_to_render -= self._buffer_size
			self._opl.getSamples(tmp_buffer)
			self._output += tmp_buffer

	def render_dro(self):
		# Reset (may not be required)
		for bank in range(2):
			for reg in range(0x100):
				self._write(bank, reg, 0x00)

		# Render all instructions
		for entry in self._dro:
			if entry[0] == DROInstructionType.DELAY_MS:
				_, delay_ms = entry
				self._render_ms(delay_ms)
			else:
				_, bank, reg, val = entry
				self._write(bank, reg, val)

		return self._output

	def _write(self, bank: int, register: int, value: int) -> None:
		self._opl.writeReg(register | (bank << 2), value)
