"""Explicit synthetic CSV input adapter; never infers units from numeric ranges."""
import csv
import hashlib
import io
import json
from pathlib import Path


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate JSON key: ' + key)
        result[key] = value
    return result


def load_config(path):
    path = Path(path).resolve()
    raw = path.read_bytes()
    config = json.loads(raw.decode('utf-8-sig'), object_pairs_hook=unique_object,
                        parse_constant=lambda x: (_ for _ in ()).throw(ValueError('nonfinite JSON: '+x)))
    sources = [{'path': str(path), 'sha256': hashlib.sha256(raw).hexdigest()}]
    sources.extend(resolve_curve_files(config, path))
    return config, sources


def resolve_curve_files(config, path):
    """Resolve a spectral-basis object using the same strict CSV contract."""
    path = Path(path).resolve()
    sources = []
    for role in ('illumination', 'response', 'optics'):
        curve = config.get(role)
        if curve is None or 'samples_file' not in curve:
            continue
        if 'samples' in curve:
            raise ValueError(role + ': choose samples or samples_file, not both')
        # This adapter intentionally accepts only declared artificial data.
        if curve.get('source_type') != 'synthetic' or not curve.get('source'):
            raise ValueError(role + ': synthetic provenance required')
        source = (path.parent / curve.pop('samples_file')).resolve()
        if source.suffix.lower() != '.csv':
            raise ValueError(role + ': explicit CSV required (no images/PFM)')
        data = source.read_bytes()
        rows = list(csv.reader(io.StringIO(data.decode('utf-8-sig'))))
        if not rows or rows[0] != ['wavelength', 'value']:
            raise ValueError(role + ': expected CSV header wavelength,value')
        if any(len(row) != 2 for row in rows[1:]):
            raise ValueError(role + ': each CSV row needs two columns')
        curve['samples'] = [[float(x), float(y)] for x, y in rows[1:]]
        sources.append({'role': role, 'path': str(source), 'sha256': hashlib.sha256(data).hexdigest(),
                        'wavelength_unit': curve.get('wavelength_unit'), 'value_unit': curve.get('value_unit')})
    return sources
