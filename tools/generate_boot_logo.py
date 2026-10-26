"""Generate theme-specific RGB24 boot logos from the outlined identity and palettes."""
from pathlib import Path
import io
import json
import subprocess
from PIL import Image

assets = Path(__file__).resolve().parent.parent / 'assets'
master = (assets / 'Assets/Identity/rondo-wordmark-dark.svg').read_text()
themes = json.loads((assets / 'themes.json').read_text())['themes']
output = assets / 'Boot/Themes'
output.mkdir(parents=True, exist_ok=True)
for theme in themes:
    colors = theme['colors']
    svg = master.replace('#F4EBD8', colors['text']).replace('#CB5945', colors['accent'])
    derived = assets / 'Assets/Identity/Themes' / (theme['id'] + '.svg')
    derived.parent.mkdir(parents=True, exist_ok=True)
    derived.write_text(svg)
    png = subprocess.check_output(['rsvg-convert', '-w', '290', '-h', '96', str(derived)])
    logo = Image.open(io.BytesIO(png)).convert('RGBA')
    image = Image.new('RGB', (720, 480), colors['background'])
    image.paste(logo, ((720-logo.width)//2, (480-logo.height)//2), logo)
    target = output / (theme['id'] + '.bmp')
    image.save(target)
    data = target.read_bytes()
    assert data[:2] == b'BM' and int.from_bytes(data[28:30], 'little') == 24
    assert int.from_bytes(data[30:34], 'little') == 0
    if theme['id'] == 'rondo':
        (assets / 'Boot/bootlogo.bmp').write_bytes(data)
