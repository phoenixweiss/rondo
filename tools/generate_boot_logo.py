"""Generate the RG34XXSP RGB24 BMP from the approved, outlined identity."""
from pathlib import Path
from PIL import Image

assets = Path(__file__).resolve().parent.parent / 'assets'
logo = Image.open(assets / 'Assets/Identity/rondo-wordmark-dark.png').convert('RGBA')
logo.thumbnail((440, 200), Image.Resampling.LANCZOS)
image = Image.new('RGB', (720, 480), '#1C1B18')
image.paste(logo, ((720-logo.width)//2, (480-logo.height)//2), logo)
output = assets / 'Boot/bootlogo.bmp'
output.parent.mkdir(exist_ok=True)
image.save(output)
data = output.read_bytes()
assert data[:2] == b'BM'
assert int.from_bytes(data[28:30], 'little') == 24
assert int.from_bytes(data[30:34], 'little') == 0
