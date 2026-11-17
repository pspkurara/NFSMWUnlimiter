"""Geometry cross-checks: stream-relative indices and native aligned payloads."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from inspect_stance_geometry import triangle_evidence, material_identities, projected_radius_range


class TriangleEvidenceTests(unittest.TestCase):
    def test_cap_is_not_an_outer_ring(self):
        cap = [(-1, .2, -1), (1, .2, -1), (1, .2, 1), (-1, .2, 1)]
        lo, hi = projected_radius_range(cap, [0, 1, 2, 0, 2, 3])
        self.assertEqual(lo, 0)
        self.assertAlmostEqual(hi, 2**.5)

    def test_sidewall_ring_stays_away_from_axle(self):
        lo, hi = projected_radius_range([(1, 0, 0), (2, 0, 0), (1, .2, 1)], [0, 1, 2])
        self.assertEqual(lo, 1)
        self.assertEqual(hi, 2)

    vertices = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (10, 0, 0), (11, 0, 0), (10, 1, 0)]

    def check_indices(self, indices, materials, padding=None):
        payload = struct.pack(f'<{len(indices)}H', *indices)
        if padding is None:
            padding = bytes((-len(payload)) % 4)
        data = bytes(16) + payload + padding
        return triangle_evidence(data, [(0x134B03, 8, len(data)-8)], materials, [self.vertices])

    @staticmethod
    def material(bounds):
        return {'stream': 0, 'vertices': 3, 'triangles': 1, 'bounds': bounds}

    def test_global_stream_indices_partition_two_materials(self):
        groups = [self.material([[0, 0, 0], [1, 1, 0]]),
                  self.material([[10, 0, 0], [11, 1, 0]])]
        result = self.check_indices([0, 1, 2, 3, 4, 5], groups)
        self.assertTrue(result['verified'])
        self.assertEqual(result['triangles'], 2)

    def test_material_identities_preserve_texture_stride_and_names(self):
        data = struct.pack('<4I', 0x12345678, 0, 0xAABBCCDD, 0) + b'TYRE\0'
        textures, names = material_identities(data, [(0x134012, 0, 16), (0x134C02, 16, 5)])
        self.assertEqual(textures, [0x12345678, 0xAABBCCDD])
        self.assertEqual(names, ['TYRE'])
        with self.assertRaises(ValueError):
            material_identities(data, [(0x134012, 0, 15)])
        with self.assertRaises(ValueError):
            material_identities(data, [(0x134012, 0, 8), (0x134012, 8, 8)])

    def test_local_indices_cannot_borrow_another_material(self):
        wide = [[-1, -1, -1], [12, 2, 1]]
        groups = [self.material(wide), self.material(wide)]
        self.assertFalse(self.check_indices([0, 1, 2, 0, 1, 2], groups)['verified'])

    def test_native_padding_and_expanded_aabb(self):
        group = self.material([[-.01, -.01, -.01], [1.01, 1.01, .01]])
        result = self.check_indices([0, 1, 2], [group])
        self.assertTrue(result['verified'])
        self.assertEqual(result['paddingBytes'], 2)
        self.assertFalse(self.check_indices([0, 1, 2], [group], b'\1\0')['verified'])

    def test_indices_outside_stream_or_bounds_are_rejected(self):
        group = self.material([[0, 0, 0], [1, 1, 0]])
        self.assertFalse(self.check_indices([0, 1, 6], [group])['verified'])
        group['bounds'][1][0] = .5
        self.assertFalse(self.check_indices([0, 1, 2], [group])['verified'])


if __name__ == '__main__':
    unittest.main()
