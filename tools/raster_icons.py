"""Regenerate SDL UI icon derivatives from licensed SVG sources; requires cairosvg."""
from pathlib import Path
import cairosvg
import subprocess
ROOT=Path(__file__).resolve().parent.parent/'assets'
(ROOT/'IconsPNG').mkdir(exist_ok=True)
for source in (ROOT/'Assets/Icons').glob('*.svg'):
    data=source.read_text().replace('currentColor','#F4EBD8')
    cairosvg.svg2png(bytestring=data.encode(),write_to=str(ROOT/'IconsPNG'/(source.stem+'.png')),output_width=48,output_height=48)
# The wordmark uses luminance masks; librsvg preserves them, unlike CairoSVG.
subprocess.run(['rsvg-convert', '-w', '880', '-o',
                str(ROOT/'Assets/Identity/rondo-wordmark-dark.png'),
                str(ROOT/'Assets/Identity/rondo-wordmark-dark.svg')], check=True)
