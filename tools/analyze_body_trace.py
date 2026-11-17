"""Read-only checks of recorded stance matrices; does not infer visible geometry."""
import argparse
import json
import math
from pathlib import Path
import re


def analyze(text):
    # IDs restart on each game launch, so group by header rather than ID alone.
    records = []
    current = None
    for line in text.splitlines():
        header = re.search(r"BodyTrace (\d+) car=.* type=(\S+) view=(\S+) reflection=(\d+).* height=\[([^]]+)\]", line)
        if header:
            current = dict(id=int(header[1]), car=header[2], view=header[3],
                           reflection=int(header[4]), height=list(map(float, header[5].split())),
                           matrices={}, pairs=[])
            records.append(current)
            continue
        match = re.search(r"BodyTrace (\d+) (\S+) part=(\S+) determinant=(\S+) matrix=\[([^]]+)\]", line)
        if not match or current is None or int(match[1]) != current['id']:
            continue
        matrix = list(map(float, match[5].replace(';', ' ').split()))
        if len(matrix) != 16 or not all(map(math.isfinite, matrix)):
            raise ValueError('Invalid matrix in trace')
        stage, part = match[2], match[3]
        current['matrices'][(stage, part)] = (float(match[4]), matrix)
        if stage == 'part-output':
            current['pairs'].append(part)

    checked = 0
    failures = []
    for record in records:
        matrices = record['matrices']
        world = next((value[1] for (stage, _), value in matrices.items()
                      if stage == 'height-world'), None)
        for part in record['pairs']:
            before = matrices.get(('part-input', part))
            after = matrices[('part-output', part)]
            if before is None or world is None:
                failures.append(dict(id=record['id'], part=part, reason='Missing transform evidence'))
                continue
            expected = [sum(before[1][row * 4 + k] * world[k * 4 + col]
                            for k in range(4)) for row in range(4) for col in range(4)]
            error = max(abs(a - b) for a, b in zip(expected, after[1]))
            checked += 1
            if error > 2e-5 or before[0] * after[0] <= 0:
                failures.append(dict(id=record['id'], part=part, error=error,
                                     reason='Transform mismatch or orientation changed'))
    return dict(snapshots=len(records), checkedPartPairs=checked,
                passes=[dict(id=r['id'], car=r['car'], view=r['view'], reflection=r['reflection'],
                             height=r['height'], sampledParts=len(r['pairs'])) for r in records],
                failures=failures,
                limitation='Only logged submissions are covered; visibility, mesh contents and later renderer changes are unproven.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = analyze(args.log.read_text(encoding='utf-8', errors='replace'))
    report = json.dumps(result, indent=2)
    if args.output:
        args.output.write_text(report + '\n', encoding='utf-8')
    else:
        print(report)
    raise SystemExit(1 if result['failures'] or not result['checkedPartPairs'] else 0)
