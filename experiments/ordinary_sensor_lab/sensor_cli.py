"""Run an isolated synthetic sensor reference experiment from inline JSON or CSV curves."""
import argparse
import hashlib
import json
from pathlib import Path
from curve_io import load_config
from sensor_lab import run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('config', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=100000)
    parser.add_argument('--seed', type=int, default=20260913)
    args = parser.parse_args()
    try:
        config, sources = load_config(args.config)
        result = run(config, args.samples, args.seed)
        result['input_files'] = sources
        result['implementation'] = {name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                                    for name in ('sensor_cli.py', 'curve_io.py', 'sensor_lab.py')}
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2, allow_nan=False)+'\n', encoding='utf8')
    except (ValueError, KeyError, TypeError, OSError) as exc:
        parser.exit(2, 'Invalid synthetic experiment: '+str(exc)+'\n')
    print(str(args.output.resolve()))


if __name__ == '__main__':
    main()
