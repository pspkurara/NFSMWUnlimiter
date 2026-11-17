"""Read-only geometry evidence; no guessed vertex layouts or game edits."""
from pathlib import Path
import json
import math
import struct

def decomp(src):
    n = struct.unpack_from('<I', src, 8)[0]
    total = struct.unpack_from('<I', src, 12)[0]
    out = bytearray()
    p = 18
    control = src[16] | 256
    types = src[17] | 256
    while p < total and len(out) < n:
        if control & 1:
            a, b = src[p:p + 2]
            p += 2
            if types & 1:
                count = (a >> 4 << 8 | b) + 3
                off = (a & 15) + 1
            else:
                count = (a & 31) + 3
                off = (a >> 5 << 8 | b) + 17
            for _ in range(count):
                out.append(out[-off])
            types >>= 1
        else:
            out.append(src[p])
            p += 1
        control >>= 1
        if control == 1 and p < total:
            control = src[p] | 256
            p += 1
        if types == 1 and p < total:
            types = src[p] | 256
            p += 1
    assert len(out) == n, (len(out), n)
    return bytes(out)

def chunks(b, a=0, z=None):
    if z is None:
        z = len(b)
    p = a
    while p + 8 <= z:
        t, n = struct.unpack_from('<II', b, p)
        if p + 8 + n > z:
            break
        yield (t, p + 8, n)
        if t & 2147483648:
            yield from chunks(b, p + 8, p + 8 + n)
        p += 8 + n

def triangle_evidence(solid, nodes, materials, positions):
    blocks = [(p, n) for tag, p, n in nodes if tag == 0x134B03]
    if len(blocks) != 1:
        return {'verified': False, 'reason': 'Expected one triangle-index block'}
    p, size = blocks[0]
    cursor = (p+15) & ~15
    expected = sum(m['triangles'] for m in materials)*6
    padding = (-expected) % 4
    if cursor+expected+padding != p+size or any(solid[cursor+expected:p+size]):
        return {'verified': False, 'reason': 'Triangle count does not match aligned index payload'}
    groups = []
    offsets = [0]*len(positions)
    for material in materials:
        vertices = positions[material['stream']]
        first = offsets[material['stream']]
        last = first+material['vertices']
        offsets[material['stream']] = last
        count = material['triangles']*3
        indices = struct.unpack_from(f'<{count}H', solid, cursor)
        cursor += count*2
        if vertices is None or (indices and max(indices) >= len(vertices)):
            return {'verified': False, 'reason': 'Unsupported vertex stream or out-of-range index'}
        if not indices:
            groups.append({'triangles': 0})
            continue
        bounds = [[min(vertices[v][axis] for v in indices) for axis in range(3)],
                  [max(vertices[v][axis] for v in indices) for axis in range(3)]]
        error = max(abs(bounds[end][axis]-material['bounds'][end][axis])
                    for end in range(2) for axis in range(3))
        contains = all(material['bounds'][0][axis] <= bounds[0][axis]+.00001 and
                       material['bounds'][1][axis] >= bounds[1][axis]-.00001 for axis in range(3))
        groups.append({'triangles': count//3, 'minimumIndex': min(indices), 'maximumIndex': max(indices),
                       'referencedBounds': bounds, 'materialBoundsError': error,
                       'withinMaterialVertexRange': min(indices) >= first and max(indices) < last,
                       'insideMaterialBounds': contains})
    # A geometric cross-check avoids choosing an index convention just because
    # it happens to stay in the full stream's numeric range.
    # Native material AABBs can include a deliberate margin (Punto uses 1 cm).
    # Require containment plus the exact material vertex partition, not equality.
    verified = all(g.get('withinMaterialVertexRange', True) and g.get('insideMaterialBounds', True) for g in groups)
    return {'verified': verified, 'indexConvention': 'vertex-stream relative', 'paddingBytes': padding, 'groups': groups,
            'triangles': sum(m['triangles'] for m in materials)}


def inspect(root, car):
    if Path(car).name != car or car in ('.', '..'):
        raise ValueError('Expected a single car directory name')
    data = (root / 'CARS' / car / 'GEOMETRY.BIN').read_bytes()
    directory = next((data[p:p + n] for tag, p, n in chunks(data) if tag == 1261572), None)
    if directory is None or len(directory) % 24:
        raise ValueError('Missing or unsupported solid directory')
    result = []
    for entry in range(0, len(directory), 24):
        key, offset, size, unpacked, _, _ = struct.unpack_from('<6I', directory, entry)
        if offset >= len(data) or size > len(data) - offset:
            raise ValueError('Solid range exceeds geometry file')
        solid = decomp(data[offset:]) if data[offset:offset + 4] == b'JDLZ' else data[offset:offset + size]
        nodes = list(chunks(solid))
        header = next((solid[p:p + n] for tag, p, n in nodes if tag == 1261585), None)
        if not header or len(header) < 161:
            continue
        name = header[160:].split(b'\x00')[0].decode('ascii', errors='replace')
        if not any((term in name for term in ('BODY_A', 'BASE_A', 'WHEEL_A', 'BRAKE_A'))):
            continue
        record = {'name': name, 'hash': f'{key:08X}', 'materials': [], 'buffers': [], 'pivot': list(struct.unpack_from('<16f', header, 64))}
        m = record['pivot']
        record['pivotDeterminant'] = m[0] * (m[5] * m[10] - m[6] * m[9]) - m[1] * (m[4] * m[10] - m[6] * m[8]) + m[2] * (m[4] * m[9] - m[5] * m[8])
        materials = []
        for tag, p, n in nodes:
            if tag != 1264386:
                continue
            begin = p + 15 & ~15
            if (p + n - begin) % 104:
                record['error'] = 'Unsupported shading-group size'
                continue
            for pos in range(begin, p + n, 104):
                effect, _, flags, vertices, triangles = struct.unpack_from('<5I', solid, pos + 48)
                bounds = [list(struct.unpack_from('<3f', solid, pos)),
                          list(struct.unpack_from('<3f', solid, pos+12))]
                materials.append({'effect': effect, 'flags': flags, 'vertices': vertices,
                                  'triangles': triangles, 'bounds': bounds})
        record['materials'] = materials
        streams = []
        for material in materials:
            if not streams or streams[-1]['effect'] != material['effect']:
                streams.append({'effect': material['effect'], 'vertices': 0})
            material['stream'] = len(streams)-1
            streams[-1]['vertices'] += material['vertices']
        buffers = [(p, n) for tag, p, n in nodes if tag == 1264385]
        if len(buffers) != len(streams):
            record['error'] = 'Stream/effect count mismatch'
        else:
            positions = []
            for stream, (p, n) in zip(streams, buffers):
                begin = p + 127 & ~127
                size = p + n - begin
                count = stream['vertices']
                stride = size // count if count and size % count == 0 else 0
                stream['stride'] = stride
                if stream['effect'] not in (0, 4, 5) or stride != 36:
                    stream['supported'] = False
                    positions.append(None)
                else:
                    vertices = [struct.unpack_from('<3f', solid, begin + i * stride) for i in range(count)]
                    positions.append(vertices)
                    if not all((math.isfinite(x) for vertex in vertices for x in vertex)):
                        raise ValueError('Nonfinite positions: ' + name)
                    stream['supported'] = True
                    stream['bounds'] = [[min((v[i] for v in vertices)) for i in range(3)], [max((v[i] for v in vertices)) for i in range(3)]]
                    stream['normalLengthRange'] = [min((sum((x * x for x in struct.unpack_from('<3f', solid, begin + i * stride + 12))) for i in range(count))), max((sum((x * x for x in struct.unpack_from('<3f', solid, begin + i * stride + 12))) for i in range(count)))]
                record['buffers'].append(stream)
            record['topology'] = triangle_evidence(solid, nodes, materials, positions)
        result.append(record)
    return result
if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--cars', nargs='+', required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    output = {}
    for car in args.cars:
        try:
            output[car] = inspect(args.game, car)
        except (ValueError, OSError, struct.error, IndexError, AssertionError) as error:
            output[car] = {'error': str(error)}
    text = json.dumps(output, indent=2) + '\n'
    if args.output:
        args.output.write_text(text, encoding='utf-8')
        print('Read-only report:', args.output)
    else:
        print(text)
