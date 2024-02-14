from .dro_player import DROPlayer
from pathlib import Path
import unittest
import wave


class PyOPLTestCase(unittest.TestCase):
	def test_dro_rendering(self) -> None:
		test_dir = Path(__file__).parent

		dro_player = DROPlayer(str(test_dir / "correct_answer.dro"))
		rendered_data = dro_player.render_dro()

		# This is how I created the `correct_answer.wav`. Doing it this doesn't prove that the output is correct,
		#  but ensures that it's consistent across all builds.
		# with wave.open(str(Path(__file__) / ".." / "out.wav"). "wb") as out_wav_file:
		# 	out_wav_file.setnchannels(2)
		# 	out_wav_file.setsampwidth(2)
		# 	out_wav_file.setframerate(49716)
		# 	out_wav_file.writeframes(rendered_data)

		with wave.open(str(test_dir / "correct_answer.wav"), "rb") as wav_file:
			wav_data = wav_file.readframes(wav_file.getnframes())

		self.assertEqual(rendered_data, wav_data)


if __name__ == "__main__":
	unittest.main()
