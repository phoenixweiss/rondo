"""Render the pinned Automatic masters proportionally; requires rsvg-convert."""
from pathlib import Path
import hashlib
import json
import subprocess
import xml.etree.ElementTree as ET

ASSETS = Path(__file__).resolve().parent.parent / 'assets'
manifest = ASSETS / 'Systems/sources.json'
metadata = json.loads(manifest.read_text())
for name, item in metadata['icons'].items():
    data = (ASSETS / 'Systems' / (name + '.svg')).read_bytes()
    if hashlib.sha256(data).hexdigest() != item['svg_sha256']:
        raise ValueError('Master hash mismatch: ' + name)
    svg = ET.fromstring(data.replace(b'currentColor', b'#FFFFFF'))
    x, y, width, height = map(float, svg.attrib['viewBox'].split())
    scale = min(320 / width, 200 / height)
    rendered_width, rendered_height = width * scale, height * scale
    svg.set('x', str((320 - rendered_width) / 2))
    svg.set('y', str((200 - rendered_height) / 2))
    svg.set('width', str(rendered_width))
    svg.set('height', str(rendered_height))
    canvas = ET.Element('{http://www.w3.org/2000/svg}svg', {
        'width': '320', 'height': '200', 'viewBox': '0 0 320 200'})
    canvas.append(svg)
    destination = ASSETS / 'IconsPNG' / ('system-' + name + '.png')
    subprocess.run(['rsvg-convert', '-w', '320', '-h', '200', '-o', str(destination)],
                   input=ET.tostring(canvas), check=True)
    item.update(png_sha256=hashlib.sha256(destination.read_bytes()).hexdigest(),
                source_viewbox=[x, y, width, height],
                rendered_viewbox=[rendered_width, rendered_height],
                scale_x=scale, scale_y=scale)
manifest.write_text(json.dumps(metadata, indent=2) + '\n')
