#!/usr/bin/env python3
"""Fetch and build a pinned Horizon .NET runtime profile."""
import argparse
import json
import os
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent / 'scripts'))
from source_support import git_source, read_mirrors, run

ROOT = Path(__file__).resolve().parent
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--profile', choices=['net10-coreclr', 'net10-nativeaot', 'net9-nativeaot', 'net9-mono-llvm'], default='net10-coreclr')
p.add_argument('--jobs', type=int, default=min(os.cpu_count() or 2, 8))
p.add_argument('--example', choices=['basic', 'bcl'], help='Also link a CoreCLR example NRO and matching managed payload')
p.add_argument('--fetch-only', action='store_true')
p.add_argument('--source-mirrors', type=Path)
a = p.parse_args()
if a.jobs < 1: p.error('--jobs must be positive')
if a.example and (a.profile != 'net10-coreclr' or a.fetch_only):
    p.error('--example requires a full net10-coreclr build')
spec = json.loads((ROOT / 'runtime.lock.json').read_text())['profiles'][a.profile]
runtime = git_source(spec, ROOT / 'artifacts/sources' / a.profile, read_mirrors(a.source_mirrors))
if not a.fetch_only:
    command = [sys.executable, runtime / 'eng/libnx/build.py', '--flavor', spec['flavor'], '--jobs', a.jobs]
    if spec.get('llvm'): command.append('--llvm')
    if a.source_mirrors: command += ['--source-mirrors', a.source_mirrors.resolve()]
    run(command)
    result = {'profile': a.profile, 'source': spec, 'runtime': str(runtime),
              'environment': json.loads((runtime / 'artifacts/horizon/environment.json').read_text())}
    output = ROOT / 'artifacts' / a.profile
    output.mkdir(parents=True, exist_ok=True)
    if a.example:
        env = os.environ | result['environment']
        run([sys.executable, runtime / 'src/coreclr/pal/tests/libnx/host/build.py',
             '--probe', a.example, '--output', output / 'example'], env=env)
        result['example'] = str(output / 'example')
    (output / 'build.json').write_text(json.dumps(result, indent=2) + '\n')
print(runtime)
