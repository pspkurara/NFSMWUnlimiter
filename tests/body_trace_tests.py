import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('body_trace', Path(__file__).resolve().parents[1] / 'tools/analyze_body_trace.py')
trace = importlib.util.module_from_spec(spec)
spec.loader.exec_module(trace)


def sample(reflection=0, output_height=-0.25):
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1 if reflection else 1, 0, 0, 0, 0, 1]
    world = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -0.25, 1]
    output = identity.copy()
    output[14] = output_height
    lines = [f'BodyTrace 1 car=1234 type=FIATCOUPE view=5678 reflection={reflection} damage=0 height=[-0.25 -0.25]']
    for stage, matrix in [('height-world', world), ('part-input', identity), ('part-output', output)]:
        determinant = -1 if reflection and stage != 'height-world' else 1
        lines.append(f'BodyTrace 1 {stage} part=1234 determinant={determinant} matrix=[{" ".join(map(str, matrix))}]')
    return '\n'.join(lines)


class BodyTraceTests(unittest.TestCase):
    def test_reflection_preserved_is_not_a_new_inversion(self):
        result = trace.analyze(sample(reflection=1))
        self.assertEqual(result['checkedPartPairs'], 1)
        self.assertEqual(result['failures'], [])

    def test_wrong_translation_is_detected(self):
        self.assertEqual(len(trace.analyze(sample(output_height=0.25))['failures']), 1)

    def test_restarted_ids_remain_separate(self):
        result = trace.analyze(sample() + '\n' + sample(reflection=1))
        self.assertEqual(result['snapshots'], 2)
        self.assertEqual(result['checkedPartPairs'], 2)

    def test_missing_world_is_incomplete_evidence(self):
        text = '\n'.join(line for line in sample().splitlines() if 'height-world' not in line)
        result = trace.analyze(text)
        self.assertEqual(result['checkedPartPairs'], 0)
        self.assertEqual(len(result['failures']), 1)


if __name__ == '__main__':
    unittest.main()
