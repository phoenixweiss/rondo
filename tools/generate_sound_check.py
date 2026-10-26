"""Create Rondo's original short sound-check phrase (MIT), without audio playback."""
from pathlib import Path
import math
import struct
import wave

RATE = 44100
DURATION = 1.8
notes = [(0.05, 261.6256, -0.2), (0.32, 329.6276, 0.2),
         (0.59, 391.9954, -0.1), (0.92, 523.2511, 0.1)]
frames = []
for i in range(round(RATE * DURATION)):
    t = i / RATE
    left = right = 0.0
    for start, frequency, pan in notes:
        age = t - start
        if 0 <= age < 0.65:
            envelope = min(1.0, age / 0.012) * math.exp(-age * 5.5)
            envelope *= min(1.0, (0.65-age) / 0.07)
            voice = sum(math.sin(2*math.pi*frequency*h*age)/(h*h)
                        for h in (1, 2, 3, 4)) * envelope * 0.24
            left += voice*(1-pan); right += voice*(1+pan)
    for start in (0.05, 0.59, 1.13):
        age = t - start
        if 0 <= age < 0.4:
            envelope = min(1.0, age/0.008) * math.exp(-age*12)
            bass = math.sin(2*math.pi*130.8128*age) * envelope * 0.16
            # A soft, deterministic percussive tap without a noisy attack.
            tap = math.sin(2*math.pi*(900*age + 90*math.sin(age*8))) * math.exp(-age*90) * min(1.0, age/0.004) * 0.035
            left += bass + tap; right += bass + tap
    fade = min(1.0, (DURATION-t)/0.12)
    frames.extend((round(left*fade*32767), round(right*fade*32767)))
assert max(abs(v) for v in frames) < 25000
output = Path(__file__).resolve().parent.parent/'assets/Audio/sound-check.wav'
output.parent.mkdir(exist_ok=True)
with wave.open(str(output), 'wb') as wav:
    wav.setnchannels(2); wav.setsampwidth(2); wav.setframerate(RATE)
    wav.writeframes(struct.pack('<'+'h'*len(frames), *frames))
