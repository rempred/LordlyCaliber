"""Generate numeric ROM-image alignment metadata from qualified local fits.

Run the parent tools/native_map_image_probe.cjs and native_map_alignment_probe.py
with the private ROM and existing manual map references first. Only coordinates
and source-image identities are emitted; no reference artwork is packaged.
"""
import argparse
import json
import math
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('report', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    report = json.loads(args.report.read_text(encoding='utf-8'))
    inputs = {row['key']: row for row in report['sources']['rows']}
    accepted, excluded = [], []
    for row in report['results']:
        key = row['key']
        reasons = []
        checks = row.get('heldOut', [])
        transform = row.get('worldToReference') or {}
        if row.get('referenceGrade') != 'site-fitted': reasons.append('unqualified manual reference')
        if row['score'] < .75: reasons.append('weak overall image match')
        if len(checks) != 4 or min(c['score'] for c in checks) < .60: reasons.append('weak excluded-region match')
        if len(checks) != 4 or max(c['maxParameterDrift'] for c in checks) > .015: reasons.append('unstable region exclusion')
        if transform.get('b', 0) or transform.get('d', 0) or transform.get('a', 0) <= 0 or transform.get('e', 0) <= 0:
            reasons.append('unsupported reference orientation')
        rect = row['normalizedRect']
        if len(rect) != 4 or not all(math.isfinite(v) for v in rect) or min(rect[2:]) <= 0:
            reasons.append('invalid image rectangle')
        if reasons:
            excluded.append({'scenario': key, 'reasons': reasons})
            continue
        rw, rh = row['referenceSize']
        x, y, w, h = rect
        a, c, e, f = (transform[v] for v in ('a', 'c', 'e', 'f'))
        bounds = [(x * rw - c) / a, (y * rh - f) / e,
                  ((x + w) * rw - c) / a, ((y + h) * rh - f) / e]
        if not all(math.isfinite(v) and abs(v) < 10000 for v in bounds):
            raise ValueError('Invalid world coordinates')
        source = inputs[key]
        accepted.append({'scenario': key, 'width': source['width'], 'height': source['height'],
                         'imageSha256': row['imageSha256'], 'worldBounds': bounds,
                         'fitScore': row['score'], 'excludedRegionScore': min(c['score'] for c in checks),
                         'maximumParameterDrift': max(c['maxParameterDrift'] for c in checks)})
    result = {'schema': 1, 'generator': 'native/tools/generate_map_image_alignment.py',
              'inputs': [report['generator'], 'wiki/scenario-map-calibration.json',
                         'US Rev 0 ROM terrain images identified by decoded SHA-256'],
              'meaning': 'Approximate image registration composed with existing manual world coordinates.',
              'maps': accepted, 'excluded': excluded}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'{len(accepted)} calibrated scenarios; {len(excluded)} held as separate image references')


if __name__ == '__main__': main()
